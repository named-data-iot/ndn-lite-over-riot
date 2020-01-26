/*
 * Copyright (C) 2020 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v3.0. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#include <stdio.h>
#include "../../../adaptation/ndn-lite.h"
#include "../../../adaptation/gnrc-netface/netface.h"
#include "ndn-lite/encode/name.h"
#include "ndn-lite/encode/data.h"
#include "ndn-lite/ndn-services.h"

#define ENABLE_NDN_LOG_INFO 1
#define ENABLE_NDN_LOG_DEBUG 1
#define ENABLE_NDN_LOG_ERROR 1
#include "ndn-lite/util/logger.h"

#include "ndn-lite/encode/interest.h"
#include "ndn-lite/app-support/pub-sub.h"
#include "ndn-lite/app-support/access-control.h"
#include "../../helper/bootstrap-helper.h"
#include "../../helper/services.h"



bool running;
static uint8_t buffer[1024];

typedef struct {
  uint8_t lastStatus;
  uint64_t motionStopTime;
} state_t;

state_t state = {1, 0};

void on_motion_publish(uint8_t service, bool is_cmd, const name_component_t* identifiers, uint32_t identifiers_size,
               const uint8_t* suffix, uint32_t suffix_len, const uint8_t* content, uint32_t content_len,
               void* userdata)
{
  if (is_cmd) {
    NDN_LOG_DEBUG("motion CMD received\n");
    return;
  }

  NDN_LOG_DEBUG("motion DATA received\n");
  /* if above the threshold */
  uint32_t threshold = 50;
  uint32_t value = *(uint32_t*)content;
  //ps_subscribe_to_command(NDN_SD_MOTION, NULL, 0, on_motion_publish, NULL);
  if (value > threshold) {
    // NDN_LOG_DEBUG("turnnig on lights due to motion\n");
    // uint8_t command_id = 111;
    // /* turnning on all lights if possible, let user policy decide which acutally to be turned on */
    // ps_publish_command(NDN_SD_LED, &command_id, sizeof(command_id), NULL, 0, &value, sizeof(value));
    // state.lastStatus = 1; /* on */
    // state.motionStopTime = 0; /* should work like null */ 
  }
  else {

  }

  ndn_time_delay(100);
}

void initialize()
{
  ndn_ac_register_access_request(NDN_SD_MOTION);
  ndn_ac_register_access_request(NDN_SD_ILLUMINANCE);
  ndn_ac_register_encryption_key_request(NDN_SD_LED);
  // ndn_ac_after_bootstrapping();
  
  /* inject the access keys */
  ndn_access_control_t* ac_state = ndn_ac_get_state();
  uint8_t value[16] = {8};
  ndn_aes_key_t* key = NULL;
  ndn_time_ms_t now = ndn_time_now_ms();
  uint32_t keyid;
  for (int i = 0; i < 10; i++) {
    if (ac_state->access_services[i] == NDN_SD_MOTION ||
        ac_state->access_services[i] == NDN_SD_ILLUMINANCE) {
      ndn_rng((uint8_t*)&keyid, 4);
      ac_state->access_keys[i].key_id = keyid;
      ac_state->access_keys[i].expires_at = 40000 + now;
      key = ndn_key_storage_get_empty_aes_key();
      ndn_aes_key_init(key, value, 16, keyid);
    }
  }

  for (int i = 0; i < 10; i++) {
    if (ac_state->self_services[i] == NDN_SD_LED) {
      ndn_rng((uint8_t*)&keyid, 4);
      ac_state->ekeys[i].key_id = keyid;
      ac_state->ekeys[i].expires_at = 40000 + now;
      key = ndn_key_storage_get_empty_aes_key();
      ndn_aes_key_init(key, value, 16, keyid);
    }
  }

  name_component_t id[2];
  name_component_from_string(&id[0], "living", strlen("living"));
  name_component_from_string(&id[1], "motion_sensor9", strlen("motion_sensor9"));
  //ps_subscribe_to_content(NDN_SD_MOTION, &id, 2, 500, on_motion_publish, NULL);
  ps_subscribe_to_command(NDN_SD_MOTION, &id, 2, on_motion_publish, NULL);
  ps_after_bootstrapping();
}


int main(int argc, char *argv[])
{
  int ret = 0;
  ndn_lite_startup();

  /* Simulate Bootstrapping
   *
   * 1. Setting TrustAnchor /ndn-iot/controller/KEY/123/self/456
   * 2. Install identity certificate named /ndn-iot/Hub/KEY/234/home/567
   * 3. Add route /ndn-iot to the given UDP face
   * 4. Install KeyID-10002 Key as default AES-128 Key.
   */
  ndn_netface_t* netface = ndn_netface_get_list();
  ndn_face_intf_t* face_ptr = &netface[0].intf;

  if (!face_ptr) {
    NDN_LOG_ERROR("Face construction failed\n");
  }
  name_component_t id;
  name_component_from_string(&id, "Hub", strlen("Hub"));
  simulate_bootstrap(face_ptr, 3, 0, &id, 1, 1);


  // initialize
  initialize();

  running = true;
  while(running) {
    ndn_forwarder_process();
  }
  ndn_face_destroy(face_ptr);
  return 0;
}
