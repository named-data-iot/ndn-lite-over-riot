/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v3.0. See the file LICENSE in the top level
 * directory for more details.
 */

#ifndef NDN_UDP_FACE_H_
#define NDN_UDP_FACE_H_

#include "ndn-lite/forwarder/forwarder.h"
#include "ndn-lite/util/msg-queue.h"


#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "timex.h"
#include "utlist.h"
#include "xtimer.h"


#ifdef __cplusplus
extern "C" {
#endif


/*
 * UDP face adaptation
 */
typedef struct ndn_udp_face {
  /*
   * The inherited interface.
   */
  ndn_face_intf_t intf;
  /*
   * Local port used for UDP 
   */
  uint16_t local_port;
  /*
   * Remote port used for UDP 
   */
  uint16_t remote_port;
  /*
   * Remote address used for UDP.
   * Should be IPv6 link-local multicast address
   */
  ipv6_addr_t remote_addr;
} ndn_udp_face_t;

ndn_udp_face_t*
ndn_udp_face_construct(uint16_t local_port, ipv6_addr_t remote_addr, uint16_t remote_port);


#ifdef __cplusplus
}
#endif

#endif