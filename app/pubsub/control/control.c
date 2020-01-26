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

#include "ndn-lite/encode/name.h"
#include "ndn-lite/encode/data.h"
#include "ndn-lite/encode/interest.h"
#include "ndn-lite/app-support/access-control.h"
#include "ndn-lite/app-support/pub-sub.h"
#include "ndn-lite/security/ndn-lite-rng.h"

#define ENABLE_NDN_LOG_INFO 1
#define ENABLE_NDN_LOG_DEBUG 1
#define ENABLE_NDN_LOG_ERROR 1
#include "ndn-lite/util/logger.h"
#include "../../helper/services.h"
#include "../../helper/bootstrap-helper.h"

bool running;
static uint8_t buffer[1024];

typedef struct {
  uint64_t last_battery;
  uint32_t last_motion;
  uint32_t timestamp; /* to avoid overflow on mac */
} state_t;

state_t state = {100, 0, 0};

/* Motion Sensor
 *
 * Certificate: /ndn-iot/motion_sensor9/KEY
 * Locator: /ndn-iot/living/motion_sensor9
 * Services: 
 *   Battery:
 *      uint32_t [1, 100]
 *   Motion: 
 *      uint32_t [1, 100] 
 * 
 * Signing Key:
 *   Battery: /ndn-iot/motion_sensor9/KEY/<keyID>/battery
 *   Motion: /ndn-iot/motion_sensor9/KEY/<keyID>/motion
 * 
 * Implementation: 
 *   Signing Key:
 *     Different KeyName but same key bits
 *   Pub Interval:
 *     Battery: publish only if drop one percent
 *     Motion: publish only if Value - lastValue > 10 
 */


/* subscribe to the command namespace
 * /ndn-iot/Motion/CMD/<...>/OFF
 * /ndn-iot/Motion/CMD/<...>/ON
 *
 *
 */
void initialize()
{
  ndn_ac_register_encryption_key_request(NDN_SD_MOTION);

  /* inject the access keys */
  ndn_access_control_t* ac_state = ndn_ac_get_state();
  uint8_t value[30] = {8};
  ndn_aes_key_t* key = NULL;
  ndn_time_ms_t now = ndn_time_now_ms();
  uint32_t keyid;
  for (int i = 0; i < 10; i++) {
    if (ac_state->self_services[i] == NDN_SD_MOTION ||
        ac_state->self_services[i] == NDN_SD_BATTERY) {
      ndn_rng((uint8_t*)&keyid, 4);
      ac_state->ekeys[i].key_id = keyid;
      ac_state->ekeys[i].expires_at = 40000 + now;
      key = ndn_key_storage_get_empty_aes_key();
      ndn_aes_key_init(key, value, NDN_AES_BLOCK_SIZE, keyid);
    }
  }

}



int main(int argc, char *argv[])
{
  int ret = 0;
  ndn_lite_startup();

  /* Simulate Bootstrapping
   *
   * 1. Setting TrustAnchor /ndn-iot/controller/KEY/123/self/456
   * 2. Install identity certificate named /ndn-iot/living/motion_sensor9/KEY/234
   * 3. Add route /ndn-iot to the given UDP face
   * 4. Install KeyID-10002 Key as default AES-128 Key.
   */
  ndn_netface_t* netface = ndn_netface_get_list();
  ndn_face_intf_t* face_ptr = &netface[0].intf;

  if (!face_ptr) {
    NDN_LOG_ERROR("Face construction failed\n");
  }

  name_component_t id[2];
  name_component_from_string(&id[0], "living", strlen("living"));
  name_component_from_string(&id[1], "motion_sensor9", strlen("motion_sensor9"));
  simulate_bootstrap(face_ptr, NDN_SD_MOTION, 1, &id, 2, 1);

  running = true;
  bool cmd_generation = true;

  initialize();
  ndn_key_storage_t* storage = ndn_key_storage_get_instance();
  NDN_LOG_DEBUG("self_identity is ");
  NDN_LOG_DEBUG_NAME(&storage->self_identity);

  state.timestamp = ndn_time_now_us();
  //service_data_publishing();

  while(running) {
    ndn_forwarder_process();

    if (ndn_time_now_us() - state.timestamp > (uint32_t)1000000
        && cmd_generation) {
      uint8_t command_id = 112;
      ps_publish_command(NDN_SD_MOTION, &command_id, sizeof(command_id),
                         &id, 2, &command_id, sizeof(command_id));
      cmd_generation = false;
    }
  }
  ndn_face_destroy(face_ptr);
  return 0;
}