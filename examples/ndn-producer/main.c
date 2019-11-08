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
#include "../../adaptation/ndn-lite.h"
#include "../../adaptation/gnrc-netface/netface.h"
#include "ndn-lite/encode/name.h"
#include "ndn-lite/encode/data.h"
#include "ndn-lite/encode/interest.h"
#include <kernel_types.h>
#include <thread.h>
#include "xtimer.h"
#include "shell.h"

static uint8_t buffer[200] = {0};

static ndn_name_t name;
bool running = false;

void
on_interest(const uint8_t *interest, uint32_t interest_size, void *userdata)
{
  printf("interest arrival\n");
}

int main(void)
{

  printf("/**** Application Is Running: PID = %" PRIkernel_pid " ****/\n",
        thread_getpid());

 ndn_lite_startup();

 ndn_name_from_string(&name, "/ndn/test", strlen("/ndn/test"));
 ndn_name_print(&name);

   ndn_forwarder_register_name_prefix(&name, on_interest, NULL);
  running = true;
  while(running) {
    ndn_forwarder_process();
    xtimer_usleep(10);
  }

  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
}