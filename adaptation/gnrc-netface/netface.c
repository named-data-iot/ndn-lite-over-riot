/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#include "netface.h"

#include "ndn-lite/encode/fragmentation-support.h"
#include "ndn-lite/forwarder/forwarder.h"
#include "ndn-lite/ndn-constants.h"

#include <net/netopt.h>
#include <net/gnrc/netapi.h>
#include <net/gnrc/netif.h>
#include <net/gnrc/netif/hdr.h>
#include <net/gnrc/netreg.h>
#include <kernel_types.h>
#include <thread.h>
#include <timex.h>

#include "stdio.h"
#include "inttypes.h"

#define MAX_NET_QUEUE_SIZE 16

static msg_t msg_q[MAX_NET_QUEUE_SIZE];
static gnrc_netreg_entry_t me_reg;
static ndn_netface_t _netface_table[GNRC_NETIF_NUMOF];

static int _ndn_netface_send_packet(kernel_pid_t pid, gnrc_pktsnip_t* pkt)
{
  /* allocate interface header */
  gnrc_pktsnip_t *netface_hdr = gnrc_netif_hdr_build(NULL, 0, NULL, 0);

  if (netface_hdr == NULL)
  {
    printf("ndn: error on interface header allocation, dropping packet\n");
    gnrc_pktbuf_release(pkt);
    return -1;
  }

  /* add interface header to packet */
  LL_PREPEND(pkt, netface_hdr);

  /* mark as broadcast */
  ((gnrc_netif_hdr_t *)pkt->data)->flags |= GNRC_NETIF_HDR_FLAGS_BROADCAST;
  ((gnrc_netif_hdr_t *)pkt->data)->if_pid = pid;

  /* send to interface */
  if (gnrc_netapi_send(pid, pkt) < 1)
  {
    printf("ndn: failed to send packet (netface=%" PRIkernel_pid ")\n", pid);
    gnrc_pktbuf_release(pkt);
    return -1;
  }

  printf("ndn: successfully sent one gnrc packet (netface=%" PRIkernel_pid ")\n", pid);
  return 0;
}

static int _ndn_netface_send_fragments(kernel_pid_t pid, const uint8_t* data,
                                       uint32_t data_size, uint16_t mtu)
{
  if (mtu <= NDN_FRAG_HDR_LEN)
  {
    printf("ndn: mtu smaller than L2 fragmentation header size (iface=%"
           PRIkernel_pid ")\n", pid);
    return -1;
  }

  int total_frags = data_size / (mtu - NDN_FRAG_HDR_LEN) + 1;
  if (total_frags > 32) {
    printf("ndn: too many fragments to send (iface=%"
           PRIkernel_pid ")\n", pid);
    return -1;
  }

  ndn_fragmenter_t fragmenter;
  uint8_t fragmented[mtu];

  uint16_t identifier = 0;
  ndn_rng((uint8_t*)&identifier, sizeof(identifier));
  ndn_fragmenter_init(&fragmenter, data, data_size, mtu, &identifier);

  while(fragmenter.counter < fragmenter.total_frag_num)
  {
    uint32_t size = (fragmenter.counter == fragmenter.total_frag_num - 1)? 
                    (fragmenter.original_size - fragmenter.offset + 3) : mtu;
    ndn_fragmenter_fragment(&fragmenter, fragmented);
    gnrc_pktsnip_t *pkt = gnrc_pktbuf_add(NULL, (void*)fragmented, size,
                                          GNRC_NETTYPE_NDN);
    if (pkt == NULL) {
      printf("ndn: cannot create packet during sending fragments (iface=%"
             PRIkernel_pid ")\n", pid);
      return -1;
    }

    if (_ndn_netface_send_packet(pid, pkt) < 0)
      return -1;
    printf("ndn: sent fragment (SEQ=%d, ID=%02X, "
           "size=%d, netface=%" PRIkernel_pid ")\n",
           (int)fragmenter.counter, fragmenter.frag_identifier, (int)size, pid);

  }

  return 0;
}


/* Network Face Interfaces */
ndn_netface_t*
ndn_netface_find(uint16_t index)
{
  if (index > GNRC_NETIF_NUMOF)
   return NULL;

  return &_netface_table[index];
}

int
ndn_netface_up(struct ndn_face_intf* self)
{
  self->state = NDN_FACE_STATE_UP;
  return 0;
}

void
ndn_netface_destroy(struct ndn_face_intf* self)
{
  self->state = NDN_FACE_STATE_DESTROYED;
  return;
}

int
ndn_netface_down(struct ndn_face_intf* self)
{
  self->state = NDN_FACE_STATE_DOWN;
  return 0;
}

int
ndn_netface_send(struct ndn_face_intf* self, const uint8_t* packet, uint32_t size)
{
  ndn_netface_t* phyface = (ndn_netface_t*)self;
  kernel_pid_t pid = phyface->pid;
  if (phyface == NULL)
    printf("ndn: no such physical net face, face_id  = %d\n", self->face_id);

  /* check mtu */
  if (size > phyface->mtu)
  {
    return _ndn_netface_send_fragments(pid, packet, size, phyface->mtu);
  }

  gnrc_pktsnip_t* pkt = gnrc_pktbuf_add(NULL, packet, size,
                                        GNRC_NETTYPE_NDN);
  if (pkt == NULL)
  {
    printf("ndn: cannot create packet during sending (netface=%"
           PRIkernel_pid ")\n", pid);
    return -1;
  }
  return _ndn_netface_send_packet(pid, pkt);
}



void 
_process_packet(ndn_face_intf_t* self,  gnrc_pktsnip_t *pkt)
{

  size_t len = pkt->size;
  uint8_t* buf = (uint8_t*)pkt->data;

  // not considering fragmentation first
  if  (pkt == NULL || pkt->type != GNRC_NETTYPE_NDN) {
    printf("ndn: pkt is null or pkt type is not NDN\n");
    return -1;
  }
  ndn_netface_t* face = (ndn_netface_t*)self;

  /* check if the packet starts with l2 fragmentation header */
  if (buf[0] & NDN_FRAG_HB_MASK) {
    uint16_t frag_id = (buf[1] << 8) + buf[2];
    printf("ndn: l2 fragment received (MF=%x, SEQ=%u, ID=%02x, "
            "packet size = %d, iface=%" PRIkernel_pid ")\n",
            (buf[0] & NDN_FRAG_MF_MASK) >> 5,
            buf[0] & NDN_FRAG_SEQ_MASK,
            frag_id, len, face->pid);
    int ret = ndn_frag_assembler_assemble_frag(&face->assembler, buf, len);
    
    // release the gnrc packet
    gnrc_pktbuf_release(pkt);

    if (face->assembler.is_finished) {
      ndn_forwarder_receive(self, face->frag_buffer, face->assembler.offset);
      ndn_frag_assembler_init(&face->assembler, face->frag_buffer, sizeof(face->frag_buffer));
      return;
    }     
  }

  else {
    ndn_forwarder_receive(self, buf, len);
  }
}

void
ndn_netface_receive(ndn_face_intf_t* self, void* param, uint32_t param_size)
{
  msg_t msg, reply;

  /* preinitialize ACK to GET/SET commands*/
  reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
  
  int ret = msg_try_receive(&msg);
  if (ret == -1) {
    //printf("no messgae receive yet\n");
    
     // post another message receiving
     ndn_msgqueue_post(self, ndn_netface_receive, param, param_size);
     return;
  }

  switch (msg.type)
  {
    case GNRC_NETAPI_MSG_TYPE_RCV:
        printf("ndn: RCV message received from pid %"
              PRIkernel_pid "\n", msg.sender_pid);
        _process_packet(self, (gnrc_pktsnip_t *)msg.content.ptr);
        break;

    case GNRC_NETAPI_MSG_TYPE_GET:
    case GNRC_NETAPI_MSG_TYPE_SET:
        reply.content.value = -ENOTSUP;
        msg_reply(&msg, &reply);
        break;
    case GNRC_NETAPI_MSG_TYPE_SND:
    default:
        break;
  }

  ndn_msgqueue_post(self, ndn_netface_receive, param, param_size);
}


uint32_t
ndn_netface_auto_construct(void)
{
  // set up riot messge queue to receive this
  msg_init_queue(msg_q, MAX_NET_QUEUE_SIZE);

  me_reg.demux_ctx = GNRC_NETREG_DEMUX_CTX_ALL;
  me_reg.target.pid = thread_getpid();

  /* register interest in all NDN packets */
  gnrc_netreg_register(GNRC_NETTYPE_NDN, &me_reg);


  /* initialize the netif table entry */
  for (int i = 0; i < GNRC_NETIF_NUMOF; ++i)
    _netface_table[i].intf.face_id = NDN_INVALID_ID;

  /* get list of interfaces */
  uint32_t netface_num = gnrc_netif_numof();

  if (netface_num == 0)
  {
    printf("ndn: no interfaces registered, cannot add netface\n");
    return -1;
  }

  int i = -1;
  gnrc_netif_t* netface = NULL;

  while ((netface = gnrc_netif_iter(netface)))
  {
    i++;
    kernel_pid_t pid = netface->pid;
    gnrc_nettype_t proto;
    // get device mtu
    if (gnrc_netapi_get(pid, NETOPT_MAX_PACKET_SIZE, 0,
                        &_netface_table[i].mtu,
                        sizeof(uint16_t)) < 0)
    {
      printf("ndn: cannot get netface mtu (pid=%"
             PRIkernel_pid ")\n", pid);
      continue;
    }

    // set device net proto to NDN
    proto = GNRC_NETTYPE_NDN;
    printf("setting ndn as network layer protocol\n");
    int ret = gnrc_netapi_set(pid, NETOPT_PROTO, 0,  &proto, sizeof(proto));
    if (ret < 0) printf("setting to ndn failed\n");

    _netface_table[i].intf.state = NDN_FACE_STATE_DOWN;
    _netface_table[i].intf.face_id = NDN_INVALID_ID;
    _netface_table[i].intf.type = NDN_FACE_TYPE_NET;
    _netface_table[i].intf.up = ndn_netface_up;
    _netface_table[i].intf.send = ndn_netface_send;
    _netface_table[i].intf.down = ndn_netface_down;
    _netface_table[i].intf.destroy = ndn_netface_destroy;
    _netface_table[i].pid = pid;

     ndn_forwarder_register_face(&_netface_table[i].intf);
     ndn_face_up(&_netface_table[i].intf);
     ndn_frag_assembler_init(&_netface_table[i].assembler, _netface_table[i].frag_buffer, 
                              sizeof(_netface_table[i].frag_buffer));
     
     // post face receive event
     ndn_msgqueue_post(&_netface_table[i].intf, ndn_netface_receive, NULL, 0);

  }
  return netface_num;
}

void ndn_netface_traverse_print(void)
{
  printf("-----------------------------------------\n");
  for (uint8_t i = 0; i < GNRC_NETIF_NUMOF; i++)
  {
    if ( _netface_table[i].intf.face_id != NDN_INVALID_ID);
      printf("network face: %s  face id: %d  mtu: %d\n", thread_getname(_netface_table[i].pid),  
                   _netface_table[i].intf.face_id, _netface_table[i].mtu);
  }
  printf("-----------------------------------------\n");
}
 /** @} */
