/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#include <net/gnrc/netapi.h>
#include <net/gnrc/netif.h>
#include <net/gnrc/netreg.h>

#include "ndn-lite/forwarder/forwarder.h"
#include <ndn-lite/security/ndn-lite-sec-config.h>

#define GNRC_NDN_LITE_MSG_QUEUE_SIZE    (8U)

// typedef struct ndn_lite_instance {
//   // reference to forwarder
//   ndn_forwarder_t* forwarder;

//   // RIOT message queue
//   msg_t msg;
//   msg_t reply;
//   msg_t msg_q[GNRC_NDN_LITE_MSG_QUEUE_SIZE];

//   // RIOT network stack registeration entry
//   gnrc_netreg_entry_t me_reg;
// } ndn_lite_instance_t;

void
ndn_lite_startup();