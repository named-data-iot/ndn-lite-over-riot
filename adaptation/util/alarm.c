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

#include "../../ndn-lite/util/alarm.h"

/* Use it carefully, This has to be called once at system boot.
 * If auto_init is enabled, it will call this for you
 */
void ndn_alarm_init(void)
{
  xtimer_init();
}

uint64_t ndn_alarm_millis_get_now(void)
{
  return xtimer_usec_from_ticks64(xtimer_now64()) / 1000;
}

uint64_t ndn_alarm_micros_get_now(void)
{
  return xtimer_usec_from_ticks64(xtimer_now64());
}

void ndn_alarm_delay(uint32_t delay)
{
  xtimer_usleep(delay * 1000);
}
