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
#include "ndn-lite/encode/light/name-light.h"
#include "ndn-lite/face/direct-face.h"
#include <kernel_types.h>
#include <thread.h>
#include "shell.h"

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

  uint8_t hold_more[500];
  encoder_init(&encoder, hold_more, sizeof(hold_more));
  ndn_interest_uri_tlv_encode(&encoder, buffer, strlen(buffer),
                              3000, 1234);

  ndn_direct_face_t* direct = ndn_direct_face_construct(10);
  (void)direct;

  ndn_interest_t interest;
  int ret_val = ndn_interest_from_block(&interest, hold_more, encoder.offset);
  if (ret_val != NDN_SUCCESS)
    printf("error code: %d\n", ret_val);


  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
}
