/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#ifndef NDN_NETIF_H_
#define NDN_NETIF_H_

#include "ndn-lite/forwarder/face.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Network face entry.
 */
typedef struct ndn_netface {
  ndn_face_intf_t intf; /** << base class of ndn-lite face */
  uint16_t mtu;        /**< mtu of the interface */
  uint8_t frag_buffer[NDN_FRAG_BUFFER_MAX]; /**< reassembly buffer */
} ndn_netface_t;

/**
 * Initializes the netif table and try to add existing
 * network devices into the netif and face tables.
 */
uint32_t ndn_netface_auto_construct(void);

ndn_netface_t* ndn_netface_find(uint16_t face_id);

#ifdef __cplusplus
}
#endif

#endif /* NDN_NETIF_H_ */
 /** @} */
