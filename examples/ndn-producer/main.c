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


#include "msg.h"
#include "net/gnrc/netreg.h"
#include "net/gnrc/pktdump.h"


static uint8_t buffer[250] = {0};
static uint8_t content[150];
static ndn_name_t name;
static ndn_data_t data;
bool running = false;

void
on_interest(const uint8_t *interest, uint32_t interest_size, void *userdata)
{
  ndn_encoder_t encoder;
  
  for (int i = 0; i < sizeof(content); i++)
    content[i] = i;

  printf("On interest\n");
  ndn_name_from_string(&data.name, "/ndn/test/01", strlen("/ndn/test/01"));
  ndn_data_set_content(&data, content, sizeof(content));
  ndn_metainfo_init(&data.metainfo);
  ndn_metainfo_set_content_type(&data.metainfo, NDN_CONTENT_TYPE_BLOB);
  encoder_init(&encoder, buffer, sizeof(buffer));
  ndn_data_tlv_encode_digest_sign(&encoder, &data);
  ndn_forwarder_put_data(encoder.output_value, encoder.offset);
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