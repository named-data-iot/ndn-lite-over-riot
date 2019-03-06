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

static ndn_netface_t _netface_table[GNRC_NETIF_NUMOF];

/* helper function */
static ndn_netface_t* _ndn_netface_find(kernel_pid_t pid)
{
  if (pid ==  KERNEL_PID_UNDEF)
    return NULL;
  for (int i = 0; i < GNRC_NETIF_NUMOF; ++i)
  {
    if (_netface_table[i].intf.face_id == pid)
      return &_netface_table[i];
  }
  return NULL;
}

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

  printf("ndn: successfully sent packet (netface=%" PRIkernel_pid ")\n", pid);
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

  // TODO: frag_identifier is hard code as 99 now, need to be random
  ndn_fragmenter_init(&fragmenter, data, data_size, mtu, 99);

  while(fragmenter.counter < fragmenter.total_frag_num)
  {
    ndn_fragmenter_fragment(&fragmenter, fragmented);
    uint32_t size = (fragmenter.counter = fragmenter.total_frag_num - 1)?
                     mtu : (fragmenter.original_size - fragmenter.offset);
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
    // yield after sending a fragment
    thread_yield();
  }
  return 0;
}


/* Network Face Interfaces */
ndn_netface_t*
ndn_netface_find(uint16_t face_id)
{
  return _ndn_netface_find((kernel_pid_t)face_id);
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
ndn_netface_send(struct ndn_face_intf* self, const ndn_name_t* name,
                const uint8_t* packet, uint32_t size)
{
  (void)name;
  kernel_pid_t pid = (kernel_pid_t)self->face_id;

  ndn_netface_t* netface = _ndn_netface_find(pid);
  if (netface == NULL)
  {
    printf("ndn: no such network face (netface=%" PRIkernel_pid ")", pid);
    return -1;
  }

  /* check mtu */
  if (size > netface->mtu)
  {
    printf("ndn: packet size (%lu) exceeds network face mtu (%u); "
           "send with fragmentation (netface=%" PRIkernel_pid ")\n",
           size, netface->mtu, pid);
    return _ndn_netface_send_fragments(pid, packet, size, netface->mtu);
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

uint32_t
ndn_netface_auto_construct(void)
{
  /* initialize the netif table entry */
  for (int i = 0; i < GNRC_NETIF_NUMOF; ++i)
    _netface_table[i].intf.face_id = (uint16_t)KERNEL_PID_UNDEF;

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
    if (gnrc_netapi_get(pid, NETOPT_PROTO, 0,
                         &proto, sizeof(proto)) == sizeof(proto))
    {
      // this device supports PROTO option
      if (proto != GNRC_NETTYPE_NDN)
      {
        proto = GNRC_NETTYPE_NDN;
        gnrc_netapi_set(pid, NETOPT_PROTO, 0,
                        &proto, sizeof(proto));
      }
    }

    _netface_table[i].intf.state = NDN_FACE_STATE_DOWN;
    _netface_table[i].intf.face_id = (uint16_t)pid;
    _netface_table[i].intf.type = NDN_FACE_TYPE_NET;

    _netface_table[i].intf.up = ndn_netface_up;
    _netface_table[i].intf.send = ndn_netface_send;
    _netface_table[i].intf.down = ndn_netface_down;
    _netface_table[i].intf.destroy = ndn_netface_destroy;

    // add default route "/ndn" for this face
    char comp[] = "ndn";
    name_component_t component;
    ndn_name_t default_prefix;
    name_component_from_string(&component, comp, sizeof(comp));
    ndn_name_init(&default_prefix, &component, 1);
    ndn_forwarder_fib_insert(&default_prefix, &_netface_table[i].intf, 0);
  }
  return netface_num;
}

 /** @} */
