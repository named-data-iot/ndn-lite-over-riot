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
#include <kernel_types.h>
#include <thread.h>
#include "shell.h"

int main(void)
{

  printf("/**** Application Is Running: PID = %" PRIkernel_pid " ****/\n",
        thread_getpid());

  ndn_lite_instance_t* instance = ndn_lite_init();
  if (!instance)
    printf("ndn-lite instance initialization fail\n");

  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
}
