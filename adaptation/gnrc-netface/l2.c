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
#define ENABLE_NDN_LOG_ERROR 1
#define ENABLE_NDN_LOG_DEBUG 1
#define ENABLE_NDN_LOG_INFO 1
#include "ndn-lite/util/logger.h"
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

int 
ndn_l2_send_packet(kernel_pid_t pid, gnrc_pktsnip_t* pkt)
{
  /* allocate interface header */
  gnrc_pktsnip_t *netface_hdr = gnrc_netif_hdr_build(NULL, 0, NULL, 0);

  if (netface_hdr == NULL){
    NDN_LOG_ERROR("interface header allocation failed, dropping packet\n");
    gnrc_pktbuf_release(pkt);
    return -1;
  }

  /* add interface header to packet */
  LL_PREPEND(pkt, netface_hdr);
  /* mark as broadcast */
  ((gnrc_netif_hdr_t *)pkt->data)->flags |= GNRC_NETIF_HDR_FLAGS_BROADCAST;
  ((gnrc_netif_hdr_t *)pkt->data)->if_pid = pid;
  /* send to interface */
  if (gnrc_netapi_send(pid, pkt) < 1) {
    NDN_LOG_ERROR("failed to send packet (netface=%" PRIkernel_pid ")\n", pid);
    gnrc_pktbuf_release(pkt);
    return -1;
  }

  NDN_LOG_DEBUG("successfully sent one gnrc packet (netface=%" PRIkernel_pid ")\n", pid);
  NDN_LOG_DEBUG("forwarder sending: %" PRIu32 " ms\n", ndn_time_now_ms());
  return 0;
}

int 
ndn_l2_send_fragments(kernel_pid_t pid, const uint8_t* data,
                          uint32_t data_size, uint16_t mtu)
{
  if (mtu <= NDN_FRAG_HDR_LEN) {
    NDN_LOG_ERROR("MTU smaller than L2 fragmentation header size (iface=%"
                   PRIkernel_pid ")\n", pid);
    return -1;
  }

  int total_frags = data_size / (mtu - NDN_FRAG_HDR_LEN) + 1;
  if (total_frags > 32) {
    NDN_LOG_ERROR("ndn: too many fragments to send (iface=%"
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
      NDN_LOG_ERROR("cannot create packet during sending fragments (iface=%"
              PRIkernel_pid ")\n", pid);
      return -1;
    }

    if (ndn_l2_send_packet(pid, pkt) < 0) {
      return -1;
    }

    NDN_LOG_DEBUG("sent fragment (SEQ=%d, ID=%02X, "
                  "size=%d, netface=%" PRIkernel_pid ")\n",
                  (int)fragmenter.counter, fragmenter.frag_identifier, (int)size, pid);
  }
  NDN_LOG_DEBUG("forwarder sending: %" PRIu32 " ms\n", ndn_time_now_ms());
  return 0;
}

int
ndn_l2_process_packet(ndn_face_intf_t* self, gnrc_pktsnip_t *pkt)
{
  size_t len = pkt->size;
  uint8_t* buf = (uint8_t*)pkt->data;
  ndn_netface_t* face = (ndn_netface_t*)self;

  /* assuming this is an NDN packet, not NDN-over-UDP */
  if (pkt == NULL || pkt->type != GNRC_NETTYPE_NDN) {
    NDN_LOG_DEBUG("running on NDN-over-UDP overlay, discard\n");
    return -1;
  }

  /* check if the packet starts with l2 fragmentation header */
  if (buf[0] & NDN_FRAG_HB_MASK) {
    uint16_t frag_id = (buf[1] << 8) + buf[2];
    NDN_LOG_DEBUG("l2 fragment received (MF=%x, SEQ=%u, ID=%02x, "
                  "packet size = %d, iface=%" PRIkernel_pid ")\n",
                  (buf[0] & NDN_FRAG_MF_MASK) >> 5,
                   buf[0] & NDN_FRAG_SEQ_MASK,
                   frag_id, len, face->pid);
    ndn_frag_assembler_assemble_frag(&face->assembler, buf, len);
    
    /* release the gnrc packet */
    gnrc_pktbuf_release(pkt);
    if (face->assembler.is_finished) {
      NDN_LOG_DEBUG("forwarder receiving: %" PRIu32 " ms\n", ndn_time_now_ms());
      ndn_forwarder_receive(self, face->frag_buffer, face->assembler.offset);
      ndn_frag_assembler_init(&face->assembler, face->frag_buffer, sizeof(face->frag_buffer));
      return;
    }     
  }

  else {
    NDN_LOG_DEBUG("forwarder receiving: %" PRIu32 " ms\n", ndn_time_now_ms());
    ndn_forwarder_receive(self, buf, len);
  }
}
 /** @} */
