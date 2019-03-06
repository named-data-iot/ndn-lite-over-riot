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
#include "../../adaptation/nfd-lite.h"
#include "shell.h"

int main(void)
{
  kernel_pid_t pid = nfd_lite_init();
  printf("nfd-lite is running: pid = %" PRIkernel_pid "\n", pid);

  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
}
