/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#ifndef NDN_NETFACE_H_
#define NDN_NETFACE_H_

#include "ndn-lite/forwarder/face.h"
#include "ndn-lite/encode/fragmentation-support.h"
#include <kernel_types.h>
#include <thread.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Link layer face.
 */
typedef struct ndn_netface {
  /*
   * The inherited interface.
   */
  ndn_face_intf_t intf;
  /*
   * Link layer MTU.
   */
  uint16_t mtu;
  /*
   * Re-assembly buffer.
   */
  uint8_t frag_buffer[500];
  /*
   * Assembler help the re-assembly.
   */
  ndn_frag_assembler_t assembler;
  /*
   * Corresponding link layer PID.
   */
  kernel_pid_t pid;
} ndn_netface_t;

/*
 * Initializes the netif table and try to add existing
 * network devices into the netif and face tables.
 * @return the initialized netfaces.
 */
uint32_t ndn_netface_auto_construct(void);

/*
 * Helper function to traverse the netface table, printing usable link layer faces.
 */
void ndn_netface_traverse_print(void);

ndn_netface_t*
ndn_netface_get_list(void);

#ifdef __cplusplus
}
#endif

#endif /* NDN_NETFACE_H_ */
 /** @} */
