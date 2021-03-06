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
#include "../../../adaptation/ndn-lite.h"
#include "../../../adaptation/gnrc-netface/netface.h"
#include "../../../adaptation/gnrc-udp/udp.h"
#include "ndn-lite/encode/name.h"
#include "ndn-lite/encode/data.h"
#include "ndn-lite/encode/interest.h"
#include "ndn-lite/security/ndn-lite-rng.h"
#include <kernel_types.h>
#include <thread.h>
#include "xtimer.h"
#include "shell.h"

static ndn_interest_t interest;
static uint8_t buffer[250];
bool running = false;

void
on_data(const uint8_t* rawdata, uint32_t data_size, void* userdata)
{
  ndn_data_t data;
  printf("On data\n");
  if(ndn_data_tlv_decode_digest_verify(&data, rawdata, data_size)){
    printf("Decoding failed.\n");
  }

  for (int i = 0; i < 100; i++)
    printf("It says: %d\n", *(data.content_value + i));
  running = false;
}

void
on_timeout(void* userdata)
{
  printf("On timeout\n");
  running = false;
}


int main(void)
{
  printf("/**** Application Is Running: PID = %" PRIkernel_pid " ****/\n",
        thread_getpid());

  ndn_lite_startup();
  ipv6_addr_t remote_addr;
  if (ipv6_addr_from_str(&remote_addr, "ff02::1") == NULL) {
    puts("Error: unable to parse destination address");
    return;
  }

  ndn_udp_face_t* face = ndn_udp_face_construct(6363, remote_addr, 6363);
  ndn_forwarder_add_route_by_str(&face->intf, "/ndn", strlen("/ndn"));
  ndn_name_from_string(&interest.name, "/ndn/test", strlen("/ndn/test"));
  ndn_name_print(&interest.name);
  ndn_interest_set_MustBeFresh(&interest, true);
  ndn_interest_set_CanBePrefix(&interest, true);
  ndn_rng(&interest.nonce, sizeof(interest.nonce));
  interest.lifetime = 5000;
  printf("expressing interest\n");

  ndn_encoder_t encoder;
  encoder_init(&encoder, buffer, sizeof(buffer));
  int ret = ndn_interest_tlv_encode(&encoder, &interest);
  if (ret == 0) {
    printf("interest encoding success\n");
  }

  ndn_forwarder_express_interest(encoder.output_value, encoder.offset, on_data, on_timeout, NULL);
  running = true;
  while(running) {
    ndn_forwarder_process();
    xtimer_sleep(1);
  }
  printf("current time (after quitting) is %ld ms\n", ndn_time_now_ms());

  char line_buf[SHELL_DEFAULT_BUFSIZE];
  shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
}