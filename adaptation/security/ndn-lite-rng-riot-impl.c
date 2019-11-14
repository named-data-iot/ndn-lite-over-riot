
/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#include "ndn-lite-rng-riot-impl.h"
#include <ndn-lite/security/ndn-lite-rng.h>

int
ndn_lite_riot_rng(uint8_t *dest, unsigned size)
{
  random_bytes(dest, size);
  return 1;
}

void
ndn_lite_riot_rng_load_backend(void)
{
  ndn_rng_backend_t* backend = ndn_rng_get_backend();
  backend->rng = ndn_lite_riot_rng;
}