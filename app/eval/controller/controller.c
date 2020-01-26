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
#include <stdlib.h>
#include "../../../adaptation/ndn-lite.h"
#include "../../../adaptation/gnrc-netface/netface.h"
#include "ndn-lite/encode/name.h"
#include "ndn-lite/encode/data.h"
#include "ndn-lite/encode/interest.h"
#include "ndn-lite/ndn-services.h"

#define ENABLE_NDN_LOG_DEBUG 1
#define ENABLE_NDN_LOG_INFO 1
#define ENABLE_NDN_LOG_ERROR 1
#include "ndn-lite/util/logger.h"

#define SEC_BOOT_DH_KEY_ID_TEST  10011U
#define SEC_BOOT_AES_KEY_ID_TEST 10012U

#include "../../helper/bootstrap-helper.h"
#include "../../helper/services.h"

uint64_t m_measure_tp1 = 0;
uint64_t m_measure_tp2 = 0;
ndn_ecc_prv_t* ecc_secp256r1_prv_key;
ndn_ecc_pub_t* ecc_secp256r1_pub_key;
ndn_ecc_pub_t controller_dh_pub;
ndn_hmac_key_t* hmac_key;
static ndn_interest_t static_interest;
static ndn_data_t static_data;
ndn_netface_t* face;
bool running = false;

static uint8_t sec_boot_buf[512];
uint8_t trust_anchor_sha[NDN_SEC_SHA256_HASH_SIZE];
uint8_t secp256r1_prv_key_bytes[32] = {
  0xA7, 0x58, 0x4C, 0xAB, 0xD3, 0x82, 0x82, 0x5B, 0x38, 0x9F, 0xA5, 0x45, 0x73, 0x00, 0x0A, 0x32,
  0x42, 0x7C, 0x12, 0x2F, 0x42, 0x4D, 0xB2, 0xAD, 0x49, 0x8C, 0x8D, 0xBF, 0x80, 0xC9, 0x36, 0xB5
};
uint8_t secp256r1_pub_key_bytes[64] = {
  0x99, 0x26, 0xD6, 0xCE, 0xF8, 0x39, 0x0A, 0x05, 0xD1, 0x8C, 0x10, 0xAE, 0xEF, 0x3C, 0x2A, 0x3C,
  0x56, 0x06, 0xC4, 0x46, 0x0C, 0xE9, 0xE5, 0xE7, 0xE6, 0x04, 0x26, 0x43, 0x13, 0x8A, 0x3E, 0xD4,
  0x6E, 0xBE, 0x0F, 0xD2, 0xA2, 0x05, 0x0F, 0x00, 0xAC, 0x6F, 0x5D, 0x4B, 0x29, 0x77, 0x2D, 0x54,
  0x32, 0x27, 0xDC, 0x05, 0x77, 0xA7, 0xDC, 0xE0, 0xA2, 0x69, 0xC8, 0x8B, 0x4C, 0xBF, 0x25, 0xF2
};

uint8_t hmac_key_bytes[16] = {
  0xB1, 0x2A, 0x28, 0x42, 0xC7, 0xC8, 0x62, 0xC9, 0x12, 0x9B, 0x19, 0x2D, 0xC1, 0x8C, 0xB0, 0xC9
};

int
on_interest_1(const uint8_t* interest, uint32_t interest_size, void* userdata)
{
  ndn_interest_from_block(&static_interest, interest, interest_size);
  NDN_LOG_DEBUG("on /ndn/sign-on\n");

  ndn_data_t anchor;
  ndn_data_init(&anchor);
  uint8_t content[100] = {0};
  ndn_name_from_string(&anchor.name, "/iot/home/", strlen("/iot/home"));
  ndn_data_set_content(&anchor, content, sizeof(content));
  
  ndn_encoder_t encoder;
  uint8_t buffer[512];
  encoder_init(&encoder, buffer, sizeof(buffer));

  ndn_key_storage_t* storage = ndn_key_storage_get_instance();
  ndn_data_tlv_encode(&encoder, &storage->trust_anchor);


  ndn_name_t producer_identity;
  ndn_name_from_string(&producer_identity, "/ndn/ndn", strlen("/ndn/ndn"));

  ndn_data_init(&static_data);
  static_data.name = static_interest.name;
  //ndn_name_append_string_component(&data_1.name, "v01", strlen("0v2"));
  ndn_data_set_content(&static_data, encoder.output_value, encoder.offset);
  encoder_init(&encoder, buffer, sizeof(buffer));
  ndn_data_tlv_encode_hmac_sign(&encoder, &static_data, &producer_identity, hmac_key);
  ndn_forwarder_put_data(encoder.output_value, encoder.offset);
    
}


int
on_interest_2(const uint8_t* interest, uint32_t interest_size, void* userdata)
{
  ndn_interest_from_block(&static_interest, interest, interest_size);
  NDN_LOG_DEBUG("on /ndn-iot\n");

  ndn_key_storage_t* storage = ndn_key_storage_get_instance();
  ndn_encoder_t encoder;
  uint8_t buffer[512];
  encoder_init(&encoder, buffer, sizeof(buffer));
  ndn_data_tlv_encode(&encoder, &storage->self_cert);
  uint8_t pseudo[32] = {0};  
  encoder_append_raw_buffer_value(&encoder, pseudo, sizeof(pseudo)); 
  
  ndn_data_init(&static_data);
  static_data.name = static_interest.name;

  ndn_data_set_content(&static_data, encoder.output_value, encoder.offset);
  
  NDN_LOG_DEBUG("setting content\n");

  ndn_name_t producer_identity;
  ndn_name_from_string(&producer_identity, "/ndn/ndn", strlen("/ndn/ndn"));
  encoder_init(&encoder, buffer, sizeof(buffer));
  ndn_data_tlv_encode_hmac_sign(&encoder, &static_data, &producer_identity, hmac_key);
  ndn_forwarder_put_data(encoder.output_value, encoder.offset);
    
}

int
main()
{
  int ret;

  ndn_lite_startup();
  ndn_netface_t* netface = ndn_netface_get_list();
  face = &netface[0];
  ndn_name_t prefix;
  ndn_name_from_string(&prefix, "/ndn", strlen("/ndn"));
  ndn_forwarder_register_name_prefix(&prefix, on_interest_1, NULL);
  ndn_name_from_string(&prefix, "/ndn-iot", strlen("/ndn-iot"));
  ndn_forwarder_register_name_prefix(&prefix, on_interest_2, NULL);

  name_component_t comp;
  name_component_from_string(&comp, "identifier-20", strlen("identifier-20"));  
  simulate_bootstrap(&face->intf, 3, 0, &comp, 1, 0);

  hmac_key = ndn_key_storage_get_empty_hmac_key();
  ndn_hmac_key_init(hmac_key, hmac_key_bytes, sizeof(hmac_key_bytes), 2);



  running = true;
  while(running) {
    ndn_forwarder_process();
    //ndn_time_delay(1);
  }

  ndn_face_destroy(&face->intf);
  return 0;
}
