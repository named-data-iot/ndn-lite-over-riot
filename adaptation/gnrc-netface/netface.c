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
#include "l2.h"

#include "ndn-lite/encode/fragmentation-support.h"
#include "ndn-lite/forwarder/forwarder.h"

#define ENABLE_NDN_LOG_ERROR 1
#define ENABLE_NDN_LOG_DEBUG 1
#define ENABLE_NDN_LOG_INFO 1

#include "ndn-lite/util/logger.h"
#include "ndn-lite/ndn-constants.h"

#include <net/netopt.h>
#include <net/gnrc/netapi.h>
#include <net/gnrc/netif.h>
#include <net/gnrc/netif/hdr.h>
#include <net/gnrc/netreg.h>
#include <kernel_types.h>
#include <thread.h>
#include <timex.h>

#include "inttypes.h"

#define MAX_NET_QUEUE_SIZE 8

static msg_t msg_q[MAX_NET_QUEUE_SIZE];
static gnrc_netreg_entry_t me_reg;
static ndn_netface_t _netface_table[GNRC_NETIF_NUMOF];

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
  if (phyface == NULL) {
    NDN_LOG_ERROR("no such physical netface, forwarder face_id  = %d\n", self->face_id);
  }

  /* check mtu */
  if (size > phyface->mtu){
    return ndn_l2_send_fragments(pid, packet, size, phyface->mtu);
  }

  gnrc_pktsnip_t* pkt = gnrc_pktbuf_add(NULL, packet, size, GNRC_NETTYPE_NDN);
  if (pkt == NULL) {
    NDN_LOG_ERROR("cannot create packet during sending (netface=%"
                   PRIkernel_pid ")\n", pid);
    return -1;
  }
  return ndn_l2_send_packet(pid, pkt);
}


void
ndn_netface_receive(ndn_face_intf_t* self, void* param, uint32_t param_size)
{
  msg_t msg, reply;

  /* preinitialize ACK to GET/SET commands*/
  reply.type = GNRC_NETAPI_MSG_TYPE_ACK;
  int ret = msg_try_receive(&msg);
  if (ret == -1) {
    ndn_msgqueue_post(self, ndn_netface_receive, param, param_size);
    return;
  }

  switch (msg.type)
  {
    case GNRC_NETAPI_MSG_TYPE_RCV:
      NDN_LOG_DEBUG("RCV message received from pid %"
                     PRIkernel_pid "\n", msg.sender_pid);
      ndn_l2_process_packet(self, (gnrc_pktsnip_t *)msg.content.ptr);
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
  /* set RIOT msg queue to receive link layer messages */
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
  if (netface_num == 0) {
    NDN_LOG_ERROR("no interfaces registered, cannot add netface\n");
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
                        sizeof(uint16_t)) < 0) {
      NDN_LOG_ERROR("cannot get netface mtu (pid=%"
                     PRIkernel_pid ")\n", pid);
      continue;
    }

    // set device net proto to NDN
    if (gnrc_netapi_get(pid, NETOPT_PROTO, 0,
                        &proto, sizeof(proto)) == sizeof(proto)) {
        // this device supports PROTO option
        NDN_LOG_DEBUG("set device net proto to NDN\n");
        if (proto != GNRC_NETTYPE_NDN) {
            proto = GNRC_NETTYPE_NDN;
            gnrc_netapi_set(pid, NETOPT_PROTO, 0,
                            &proto, sizeof(proto));
        }
    }

    /* setting up forwarder face*/
    _netface_table[i].intf.state = NDN_FACE_STATE_DOWN;
    _netface_table[i].intf.face_id = NDN_INVALID_ID;
    _netface_table[i].intf.type = NDN_FACE_TYPE_NET;
    _netface_table[i].intf.up = ndn_netface_up;
    _netface_table[i].intf.send = ndn_netface_send;
    _netface_table[i].intf.down = ndn_netface_down;
    _netface_table[i].intf.destroy = ndn_netface_destroy;
    _netface_table[i].pid = pid;

    /* registering netface to forwarder */
    ndn_forwarder_register_face(&_netface_table[i].intf);
    ndn_face_up(&_netface_table[i].intf);
    ndn_frag_assembler_init(&_netface_table[i].assembler, _netface_table[i].frag_buffer, 
                              sizeof(_netface_table[i].frag_buffer));
     
    /* posting a msgqueue netface receiving event */
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

ndn_netface_t*
ndn_netface_get_list(void)
{
  return &_netface_table;
}
 /** @} */
