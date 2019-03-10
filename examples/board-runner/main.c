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
#include "ndn-lite/forwarder/forwarder.h"
#include "ndn-lite/util/alarm.h"
#include "../../adaptation/gnrc-netface/netface.h"
#include <kernel_types.h>
#include <thread.h>
#include "shell.h"

int main(void)
{

  printf("/**** Application Is Running: PID = %" PRIkernel_pid " ****/\n",
        thread_getpid());

  ndn_forwarder_t* forwarder = ndn_forwarder_init();
  if (forwarder)
    printf("/**** Forwarder Initialized ****/\n");

  printf("/**** Network Faces Auto Construct ****/\n");
  uint64_t start = ndn_alarm_millis_get_now();
  ndn_netface_auto_construct();
  uint64_t delta = ndn_alarm_millis_get_now() - start;
  printf("Auto Construction Complete In %d ms\n", (int)delta);

  ndn_netface_traverse_print();

  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
}
