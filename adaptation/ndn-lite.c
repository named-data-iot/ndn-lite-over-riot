/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#include "ndn-lite.h"
#include "gnrc-netface/netface.h"
#include "gnrc-netface/netface.h"
#include "ndn-lite/forwarder/forwarder.h"

void
ndn_lite_startup()
{
  //register_platform_security_init(ndn_lite_posix_rng_load_backend);
  ndn_security_init();
  ndn_forwarder_init();
  ndn_netface_auto_construct();
  ndn_netface_traverse_print();
}