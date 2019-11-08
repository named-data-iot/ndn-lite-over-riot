/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#include <stdio.h>
#include "xtimer.h"
#include "timex.h"

#include "ndn-lite/util/uniform-time.h"

ndn_time_ms_t ndn_time_now_ms(void){
   return xtimer_usec_from_ticks64(xtimer_now64()) / 1000;
}

ndn_time_us_t ndn_time_now_us(void){
return xtimer_usec_from_ticks64(xtimer_now64());
}

void ndn_time_delay(ndn_time_ms_t delay){
xtimer_usleep(delay * 1000);
}