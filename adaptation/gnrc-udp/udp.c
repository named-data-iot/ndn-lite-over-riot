/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v3.0. See the file LICENSE in the top level
 * directory for more details.
 */


#include "udp.h"
#include "ndn-lite/ndn-error-code.h"
#include "ndn-lite/ndn-constants.h"
#include "ndn-lite/util/logger.h"
#include <net/netopt.h>
#include <net/gnrc/netapi.h>
#include <net/gnrc/netif.h>
#include <net/gnrc/netif/hdr.h>
#include <net/gnrc/netreg.h>
#include "net/gnrc/udp.h"

#define MAX_NET_QUEUE_SIZE 8
static msg_t msg_q[MAX_NET_QUEUE_SIZE];
static gnrc_netreg_entry_t me_reg;
static ndn_udp_face_t udp_face;

void
ndn_udp_face_recv(void *self, size_t param_len, void *param)
{
  msg_t msg, reply;

  /* preinitialize ACK to GET/SET commands*/
  reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
  msg_try_receive(&msg);
  switch (msg.type)
  {
    case GNRC_NETAPI_MSG_TYPE_RCV:
      NDN_LOG_DEBUG("RCV message received from pid %"
                     PRIkernel_pid "\n", msg.sender_pid);      
      /* catch the received packet */
      gnrc_pktsnip_t* pkt = (gnrc_pktsnip_t *)msg.content.ptr;
      size_t len = pkt->size;
      uint8_t* buf = pkt->data;
      /* assume a NDN-over-UDP packet, if not, discard */
      if (pkt->type == GNRC_NETTYPE_NDN){
        NDN_LOG_DEBUG("run over pure NDN, discard from UDP face\n");
      }
      /* deliver the packet to the forwarder */
      ndn_forwarder_receive(self, buf, len);
      /* release the gnrc packet */
      gnrc_pktbuf_release(pkt);
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
 
  /* schedule a new msgqueue event */
  ndn_msgqueue_post(self, ndn_udp_face_recv, param_len, param);
}

int
ndn_udp_face_up(struct ndn_face_intf* self){
  ndn_msgqueue_post(self, ndn_udp_face_recv, 0, NULL);
  self->state = NDN_FACE_STATE_UP;
  return NDN_SUCCESS;
}

int
ndn_udp_face_down(struct ndn_face_intf* self){
  self->state = NDN_FACE_STATE_DOWN;
  return NDN_SUCCESS;
}

void
ndn_udp_face_destroy(ndn_face_intf_t* self){
  /* unregister the UDP event */
  gnrc_netreg_unregister(GNRC_NETTYPE_UDP, &me_reg);
  me_reg.target.pid = KERNEL_PID_UNDEF;
  ndn_face_down(self);
  ndn_forwarder_unregister_face(self);
  NDN_LOG_DEBUG("UDP event unregisteration");
}

int
ndn_udp_face_send(ndn_face_intf_t* self, const uint8_t* packet, uint32_t size)
{  
  ndn_udp_face_t* face = (ndn_udp_face_t*)self;

  /* 
   * This operation is questionable. It only select the first available GNRC netif.
   * Considering most device only have one interface available, we can accept this.
   */
  gnrc_netif_t* netif = gnrc_netif_iter(NULL);

  gnrc_pktsnip_t *payload, *udp, *ip;
  unsigned payload_size;
  /* allocate payload */
  payload = gnrc_pktbuf_add(NULL, packet, size, GNRC_NETTYPE_UNDEF);
  if (payload == NULL) {
    NDN_LOG_ERROR("unable to copy data to packet buffer");
    return;
  }
  /* store size for output */
  payload_size = (unsigned)payload->size;
  /* allocate UDP header */
  udp = gnrc_udp_hdr_build(payload, face->local_port, face->remote_port);
  if (udp == NULL) {
    NDN_LOG_ERROR("unable to allocate UDP header");
    gnrc_pktbuf_release(payload);
    return;
  }
  /* allocate IPv6 header */
  ip = gnrc_ipv6_hdr_build(udp, NULL, &face->remote_addr);
  if (ip == NULL) {
    NDN_LOG_ERROR("unable to allocate IPv6 header");
    gnrc_pktbuf_release(udp);
    return;
  }
  /* add netif header, if interface was given */
  if (netif != NULL) {
    gnrc_pktsnip_t *netif_hdr = gnrc_netif_hdr_build(NULL, 0, NULL, 0);
    ((gnrc_netif_hdr_t *)netif_hdr->data)->if_pid = netif->pid;
    LL_PREPEND(ip, netif_hdr);
  }
  /* send packet */
  if (!gnrc_netapi_dispatch_send(GNRC_NETTYPE_UDP, GNRC_NETREG_DEMUX_CTX_ALL, ip)) {
    NDN_LOG_ERROR("unable to locate UDP thread");
    gnrc_pktbuf_release(ip);
    return;
  }
}

ndn_udp_face_t*
ndn_udp_face_construct(uint16_t local_port, ipv6_addr_t remote_addr, uint16_t remote_port)
{
  udp_face.local_port = local_port;
  udp_face.remote_addr = remote_addr;
  udp_face.remote_port = remote_port;

  /* set RIOT msg queue to receive UDP message */
  msg_init_queue(msg_q, MAX_NET_QUEUE_SIZE);
  me_reg.demux_ctx = local_port;
  me_reg.target.pid = thread_getpid();

  /* register interest in all UDP packets */
  gnrc_netreg_register(GNRC_NETTYPE_UDP, &me_reg);
  udp_face.intf.face_id = NDN_INVALID_ID;
  int iret = ndn_forwarder_register_face(&udp_face.intf);
  if(iret != NDN_SUCCESS){
    NDN_LOG_ERROR("UDP face registeration failed\n");
    return NULL;
  }

  udp_face.intf.type = NDN_FACE_TYPE_NET;
  udp_face.intf.state = NDN_FACE_STATE_DOWN;
  udp_face.intf.up = ndn_udp_face_up;
  udp_face.intf.down = ndn_udp_face_down;
  udp_face.intf.send = ndn_udp_face_send;
  udp_face.intf.destroy = ndn_udp_face_destroy;

  ndn_face_up(&udp_face.intf);
  return &udp_face;
}
