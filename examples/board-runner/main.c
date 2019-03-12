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
#include "ndn-lite/encode/name.h"
#include "ndn-lite/forwarder/forwarder.h"
#include "ndn-lite/util/alarm.h"
#include "ndn-lite/face/direct-face.h"
#include <kernel_types.h>
#include <thread.h>
#include "shell.h"
#include <stdbool.h>

static bool fired = false;

int
on_interest_timeout_callback(const uint8_t* interest, uint32_t interest_size)
{
  (void)interest;
  (void)interest_size;
  printf("PIT timeout, sys time = %ld ms\n", (uint32_t)ndn_alarm_millis_get_now());
  fired = true;
  return 0;
}


int main(void)
{

  printf("/**** Application Is Running: PID = %" PRIkernel_pid " ****/\n",
        thread_getpid());

  ndn_lite_instance_t* instance = ndn_lite_init();
  if (!instance)
    printf("ndn-lite: instance initialization fail\n");

  char* buffer = "/ndn/name/tests";
  int hold_size = ndn_name_uri_tlv_probe_size(buffer, strlen(buffer));

  uint8_t hold[hold_size];
  ndn_encoder_t encoder;
  encoder_init(&encoder, hold, sizeof(hold));

  ndn_name_uri_tlv_encode(&encoder, buffer, strlen(buffer));

  ndn_decoder_t decoder;
  decoder_init(&decoder, hold, encoder.offset);
  ndn_name_print(&decoder);
  putchar('\n');


  uint8_t hold_more[100];
  encoder_init(&encoder, hold_more, sizeof(hold_more));
  ndn_interest_uri_tlv_encode(&encoder, buffer, strlen(buffer),
                              200, 1234);

  ndn_direct_face_construct(10);


  ndn_direct_face_express_interest(NULL, hold_more, encoder.offset, NULL,
                                   on_interest_timeout_callback);

  while(1) {
    if (!fired)
      printf("sys time = %ld ms\n", (uint32_t)ndn_alarm_millis_get_now());
    ndn_lite_process(instance);
  }

  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
}
