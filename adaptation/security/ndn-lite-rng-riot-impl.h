/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#ifndef RNG_RIOT_IMPL_H
#define RNG_RIOT_IMPL_H

#include <stdint.h>

/**
 * return 1 if runs successfully
 */
int
ndn_lite_riot_rng(uint8_t *dest, unsigned size);

void
ndn_lite_riot_rng_load_backend(void);

#endif // RNG_RIOT_IMPL_H