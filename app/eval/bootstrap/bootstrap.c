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

static uint32_t m_measure_tp0 = 0;
static uint32_t m_measure_tp1 = 0;
static uint32_t m_measure_tp2 = 0;
static ndn_ecc_prv_t* ecc_secp256r1_prv_key;
static ndn_ecc_pub_t* ecc_secp256r1_pub_key;
static ndn_ecc_pub_t controller_dh_pub;
static ndn_hmac_key_t* hmac_key;
static ndn_interest_t static_interest;
static ndn_data_t static_data;
static ndn_name_t static_name;
ndn_netface_t* face;
bool running = false;

static uint8_t sec_boot_buf[1024];
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

void
on_data_2(const uint8_t* rawdata, uint32_t data_size, void* userdata)
{
  // parse received data
  ndn_hmac_key_t* hmac = ndn_key_storage_get_hmac_key(2);
  if (ndn_data_tlv_decode_hmac_verify(&static_data, rawdata, data_size, hmac) != NDN_SUCCESS) {
    NDN_LOG_ERROR("[BOOTSTRAPPING]: Decoding failed.\n");
    return;
  }
  NDN_LOG_DEBUG("BOOTSTRAPPING-DATA2-PKT-SIZE: %u Bytes\n", data_size);
  NDN_LOG_INFO("[BOOTSTRAPPING]: Receive Sign On Certificate Data packet with name");
  NDN_LOG_INFO_NAME(&static_data.name);
  // parse content
  // format: self certificate, encrypted key
  ndn_decoder_t decoder;
  decoder_init(&decoder, static_data.content_value, static_data.content_size);
  // self cert certificate
  uint32_t probe = 0;
  decoder_get_type(&decoder, &probe);
  if (probe != TLV_Data) return;
  decoder_get_length(&decoder, &probe);
  decoder.offset += probe;
  ndn_data_t self_cert;
  if (ndn_data_tlv_decode_no_verify(&self_cert, static_data.content_value, encoder_probe_block_size(TLV_Data, probe), NULL, NULL) != NDN_SUCCESS) {
    return;
  }

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp1 = ndn_time_now_us();
#endif

  uint8_t key[] = {0xc2, 0x86, 0x69, 0x6d, 0x88, 0x7c, 0x9a, 0xa0, 0x61, 0x1b, 0xbb, 0x3e, 0x20, 0x25, 0xa4, 0x5a};
  uint8_t iv[] = {0x56, 0x2e, 0x17, 0x99, 0x6d, 0x09, 0x3d, 0x28, 0xdd, 0xb3, 0xba, 0x69, 0x5a, 0x2e, 0x6f, 0x58};
  uint8_t plain_text[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                          0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
  uint8_t cipher_text[] = {0xd2, 0x96, 0xcd, 0x94, 0xc2, 0xcc, 0xcf, 0x8a, 0x3a, 0x86, 0x30, 0x28, 0xb5, 0xe1, 0xdc, 0x0a,
                          0x75, 0x86, 0x60, 0x2d, 0x25, 0x3c, 0xff, 0xf9, 0x1b, 0x82, 0x66, 0xbe, 0xa6, 0xd6, 0x1a, 0xb1};

  ndn_aes_key_t* aes_key = ndn_key_storage_get_empty_aes_key();
  ndn_aes_key_init(aes_key, key, sizeof(key), 124);
  uint32_t used_size = 0;
  uint8_t plaintext[40] = {0};
  int ret = ndn_aes_cbc_decrypt(cipher_text, sizeof(cipher_text),
                                plaintext, &used_size, iv, aes_key);

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp2 = ndn_time_now_us();
  NDN_LOG_DEBUG("BOOTSTRAPPING-DATA2-PKT-AES-DEC: %" PRIu32 " us\n", m_measure_tp2 - m_measure_tp1);
#endif

  if (ret != NDN_SUCCESS) {
    NDN_LOG_ERROR("Cannot decrypt sealed private key, Error code: %d\n", ret);
    return;
  }

#if ENABLE_NDN_LOG_DEBUG
  NDN_LOG_DEBUG("BOOTSTRAPPING-END-TIME: %" PRIu32 " us\n", ndn_time_now_us());
#endif

}


void
on_data_1(const uint8_t* rawdata, uint32_t data_size, void* userdata)
{
  // parse received data
  if (ndn_data_tlv_decode_hmac_verify(&static_data, rawdata, data_size, hmac_key) != NDN_SUCCESS) {
    NDN_LOG_ERROR("[BOOTSTRAPPING]: Decoding failed.");
    return;
  }
  NDN_LOG_DEBUG("BOOTSTRAPPING-DATA1-PKT-SIZE: %u Bytes\n", data_size);
  NDN_LOG_INFO("[BOOTSTRAPPING]: Receive Sign On Data packet with name");
  NDN_LOG_INFO_NAME(&static_data.name);
  uint32_t probe = 0;

  // parse content
  // format: a data packet, ecdh pub, salt
  ndn_decoder_t decoder;
  decoder_init(&decoder, static_data.content_value, static_data.content_size);
  // trust anchor certificate
  decoder_get_type(&decoder, &probe);
  if (probe != TLV_Data) return;
  decoder_get_length(&decoder, &probe);
  // calculate the sha256 digest of the trust anchor
  ndn_sha256(decoder.input_value, encoder_probe_block_size(TLV_Data, probe),
             trust_anchor_sha);
  ndn_data_t trust_anchor_cert;
  if (ndn_data_tlv_decode_no_verify(&trust_anchor_cert, static_data.content_value, encoder_probe_block_size(TLV_Data, probe), NULL, NULL) != NDN_SUCCESS) {
    return;
  }

  // parse ecdh pub key, N2
  ndn_ecc_pub_init(&controller_dh_pub, ecc_secp256r1_pub_key, probe, NDN_ECDSA_CURVE_SECP256R1, 1);
  ndn_ecc_prv_t* self_prv_key = ndn_key_storage_get_ecc_prv_key(SEC_BOOT_DH_KEY_ID_TEST);

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp1 = ndn_time_now_us();
#endif

  // get shared secret using DH process
  uint8_t shared[32];
  ndn_ecc_dh_shared_secret(&controller_dh_pub, self_prv_key, shared, sizeof(shared));

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp2 = ndn_time_now_us();
  NDN_LOG_DEBUG("BOOTSTRAPPING-DATA1-ECDH: %" PRIu32 " us\n", m_measure_tp2 - m_measure_tp1);
#endif

  // decode salt from the replied data
  uint8_t salt[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp1 = ndn_time_now_us();
#endif

  // generate AES key using HKDF
  ndn_aes_key_t* sym_aes_key = ndn_key_storage_get_empty_aes_key();
  uint8_t symmetric_key[NDN_APPSUPPORT_AC_EDK_SIZE];
  ndn_hkdf(shared, sizeof(shared), symmetric_key, sizeof(symmetric_key),
           salt, sizeof(salt), NULL, 0);
  ndn_aes_key_init(sym_aes_key, symmetric_key, sizeof(symmetric_key), SEC_BOOT_AES_KEY_ID_TEST);

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp2 = ndn_time_now_us();
  NDN_LOG_DEBUG("BOOTSTRAPPING-DATA1-HKDF: %" PRIu32 " us\n", m_measure_tp2 - m_measure_tp1);
#endif

  // prepare for the next interest: register the prefix
  ndn_name_init(&static_name);
  ndn_name_from_string(&static_name, "/ndn-iot", strlen("/ndn-iot"));
  ndn_encoder_t encoder;
  encoder_init(&encoder, sec_boot_buf, sizeof(sec_boot_buf));
  ndn_name_tlv_encode(&encoder, &static_name);
  // send cert interest
 // ndn_time_delay(60);
  
  
  // interest-2
  // generate the cert interest (2nd interest)
  int ret = 0;
  ndn_interest_init(&static_interest);
  ndn_key_storage_t* key_storage = ndn_key_storage_get_instance();

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp1 = ndn_time_now_us();
#endif

  ndn_name_append_string_component(&static_interest.name, "ndn-iot", strlen("ndn-iot"));
  ndn_name_append_string_component(&static_interest.name, "cert", strlen("cert"));
  // set params
  // format: name component, N2, sha2 of trust anchor, N1
  encoder_init(&encoder, sec_boot_buf, sizeof(sec_boot_buf));
  // identifier name component
  name_component_t device_identifier_comp;
  name_component_from_string(&device_identifier_comp, "identifier-20", strlen("identifier-20"));
  name_component_tlv_encode(&encoder, &device_identifier_comp);
  // append the ecdh pub key, N2
  encoder_append_type(&encoder, TLV_SEC_BOOT_N2_ECDH_PUB);
  encoder_append_length(&encoder, ndn_ecc_get_pub_key_size(&controller_dh_pub));
  encoder_append_raw_buffer_value(&encoder, ndn_ecc_get_pub_key_value(&controller_dh_pub), 
                                  ndn_ecc_get_pub_key_size(&controller_dh_pub));
  // append sha256 of the trust anchor
  encoder_append_type(&encoder, TLV_SEC_BOOT_ANCHOR_DIGEST);
  encoder_append_length(&encoder, NDN_SEC_SHA256_HASH_SIZE);
  encoder_append_raw_buffer_value(&encoder, trust_anchor_sha, NDN_SEC_SHA256_HASH_SIZE);
  // append the ecdh pub key, N1
  ndn_ecc_pub_t* self_dh_pub = ndn_key_storage_get_ecc_pub_key(SEC_BOOT_DH_KEY_ID_TEST);
  encoder_append_type(&encoder, TLV_SEC_BOOT_N1_ECDH_PUB);
  encoder_append_length(&encoder, ndn_ecc_get_pub_key_size(self_dh_pub));
  encoder_append_raw_buffer_value(&encoder, ndn_ecc_get_pub_key_value(self_dh_pub),
                                  ndn_ecc_get_pub_key_size(self_dh_pub));
  // set parameter
  ndn_interest_set_Parameters(&static_interest, encoder.output_value, encoder.offset);
  // set must be fresh
  ndn_interest_set_MustBeFresh(&static_interest,true);
  static_interest.lifetime = 5000;

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp2 = ndn_time_now_us();
  NDN_LOG_DEBUG("BOOTSTRAPPING-INT2-PKT-ENCODING: %" PRIu32 " us\n", m_measure_tp2 - m_measure_tp1);
#endif

  // sign the interest
  ndn_name_t key_locator;
  ndn_name_from_string(&key_locator, "/ndn/key/name", strlen("/ndn/key/name"));
  ndn_ecc_prv_t* prv = ndn_key_storage_get_ecc_prv_key(1);
  ndn_signed_interest_ecdsa_sign(&static_interest, &key_locator, prv);

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp1 = ndn_time_now_us();
  NDN_LOG_DEBUG("BOOTSTRAPPING-INT2-ECDSA-SIGN: %" PRIu32 " us\n", m_measure_tp1 - m_measure_tp2);
#endif

  // send it out
  encoder_init(&encoder, sec_boot_buf, sizeof(sec_boot_buf));
  ndn_interest_tlv_encode(&encoder, &static_interest);
  ret = ndn_forwarder_express_interest(encoder.output_value, encoder.offset,
                                       on_data_2, NULL, NULL);
  if (ret != 0) {
    NDN_LOG_ERROR("[BOOTSTRAPPING]: Fail to send out adv Interest. Error Code: %d", ret);
    return;
  }
  NDN_LOG_DEBUG("BOOTSTRAPPING-INT2-PKT-SIZE: %u Bytes\n", encoder.offset);
  NDN_LOG_INFO("[BOOTSTRAPPING]: Send SEC BOOT cert Interest packet with name");
  NDN_LOG_INFO_NAME(&static_interest.name);
}

int
main()
{
  int ret;

  ndn_lite_startup();
  ndn_netface_t* netface = ndn_netface_get_list();
  face = &netface[0];
  name_component_t comp;
  name_component_from_string(&comp, "identifier-20", strlen("identifier-20"));  
  simulate_bootstrap(&face->intf, 3, 0, &comp, 1, 0);
  ndn_forwarder_add_route_by_str(&face->intf, "/ndn", strlen("/ndn"));
  ndn_forwarder_add_route_by_str(&face->intf, "/ndn-iot", strlen("/ndn-iot"));



  /*
    Interest1: 274
    Ecc make key
    ecdsa interest signing
    Expressing

    Data1: 504
    hmac verify
    sha256
    Ecdh

    Interest2: 372
    Ecdh interest signing
    expressing

    Data2: 446
    hmac verify
    Aes decrypt(256) 
  */

  // preparation
  ndn_key_storage_get_empty_ecc_key(&ecc_secp256r1_pub_key, &ecc_secp256r1_prv_key);
  ndn_ecc_prv_init(ecc_secp256r1_prv_key, secp256r1_prv_key_bytes, sizeof(secp256r1_prv_key_bytes),
                   NDN_ECDSA_CURVE_SECP256R1, 1);
  ndn_ecc_pub_init(ecc_secp256r1_pub_key, secp256r1_pub_key_bytes, sizeof(secp256r1_pub_key_bytes),
                   NDN_ECDSA_CURVE_SECP256R1, 1);
  hmac_key = ndn_key_storage_get_empty_hmac_key();
  ndn_hmac_key_init(hmac_key, hmac_key_bytes, sizeof(hmac_key_bytes), 2);


  ndn_ecc_pub_t* dh_pub = NULL;
  ndn_ecc_prv_t* dh_prv = NULL;
  ndn_key_storage_get_empty_ecc_key(&dh_pub, &dh_prv);

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp0 = ndn_time_now_us();
  NDN_LOG_DEBUG("BOOTSTRAPPING-START-TIME: %" PRIu32 " us\n", ndn_time_now_us());
#endif


#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp1 = ndn_time_now_us();
#endif

  if (ndn_ecc_make_key(dh_pub, dh_prv, NDN_ECDSA_CURVE_SECP256R1, SEC_BOOT_DH_KEY_ID_TEST) != NDN_SUCCESS) {
    return NDN_SEC_CRYPTO_ALGO_FAILURE;
  }

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp2 = ndn_time_now_us();
  NDN_LOG_DEBUG("BOOTSTRAPPING-INT1-ECDH-KEYGEN: %" PRIu32 " us\n", m_measure_tp2 - m_measure_tp1);
#endif

  // register route
  ret = ndn_forwarder_add_route_by_str(face, "/ndn/sign-on", strlen("/ndn/sign-on"));
  if (ret != NDN_SUCCESS) return ret;
  NDN_LOG_INFO("[BOOTSTRAPPING]: Successfully add route");




  // the interest-1
#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp1 = ndn_time_now_us();
#endif

  // generate the sign on interest  (1st interest)
  // make the Interest packet
  ndn_interest_init(&static_interest);
  ndn_name_append_string_component(&static_interest.name, "ndn", strlen("ndn"));
  ndn_name_append_string_component(&static_interest.name, "sign-on", strlen("sign-on"));
  // make Interest parameter
  // format: a name component, a list of services provided, the EC_pub_key
  ndn_encoder_t encoder;
  encoder_init(&encoder, sec_boot_buf, sizeof(sec_boot_buf));
  // append the identifier name component
  name_component_t device_identifier_comp;
  name_component_from_string(&device_identifier_comp, "identifier-20", strlen("identifier-20"));
  name_component_tlv_encode(&encoder, &device_identifier_comp);
  // append the capabilities
  encoder_append_type(&encoder, TLV_SEC_BOOT_CAPABILITIES);
  encoder_append_length(&encoder, 1);
  uint8_t service = NDN_SD_TEMP;
  encoder_append_raw_buffer_value(&encoder, &service, 1);
  // append the ecdh pub key, N1
  dh_pub = ndn_key_storage_get_ecc_pub_key(SEC_BOOT_DH_KEY_ID_TEST);
  encoder_append_type(&encoder, TLV_SEC_BOOT_N1_ECDH_PUB);
  encoder_append_length(&encoder, ndn_ecc_get_pub_key_size(dh_pub));
  encoder_append_raw_buffer_value(&encoder, ndn_ecc_get_pub_key_value(dh_pub), ndn_ecc_get_pub_key_size(dh_pub));
  // set parameter
  ndn_interest_set_Parameters(&static_interest, encoder.output_value, encoder.offset);
  // set must be fresh
  ndn_interest_set_MustBeFresh(&static_interest,true);
  static_interest.lifetime = 5000;

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp2 = ndn_time_now_us();
  NDN_LOG_DEBUG("BOOTSTRAPPING-INT1-PKT-ENCODING: %" PRIu32 " us\n", m_measure_tp2 - m_measure_tp1);
#endif

  // sign the interest
  ndn_name_t key_locator;
  ndn_name_init(&key_locator);
  ndn_name_append_component(&key_locator, &device_identifier_comp);
  ndn_signed_interest_ecdsa_sign(&static_interest, &key_locator, ecc_secp256r1_prv_key);

#if ENABLE_NDN_LOG_DEBUG
  m_measure_tp1 = ndn_time_now_us();
  NDN_LOG_DEBUG("BOOTSTRAPPING-INT1-PKT-ECDSA-SIGN: %" PRIu32 " us\n", m_measure_tp1 - m_measure_tp2);
#endif

  // send it out
  encoder_init(&encoder, sec_boot_buf, sizeof(sec_boot_buf));
  ndn_interest_tlv_encode(&encoder, &static_interest);
  ret = ndn_forwarder_express_interest(encoder.output_value, encoder.offset, on_data_1, NULL, NULL);
  if (ret != 0) {
    NDN_LOG_ERROR("[BOOTSTRAPPING]: Fail to send out adv Interest. Error Code: %d", ret);
    return ret;
  }
  NDN_LOG_DEBUG("BOOTSTRAPPING-INT1-PKT-SIZE: %u Bytes\n", encoder.offset);
  NDN_LOG_INFO("[BOOTSTRAPPING]: Send SEC BOOT sign on Interest packet with name");
  NDN_LOG_INFO_NAME(&static_interest.name);

  running = true;
  while(running) {
    ndn_forwarder_process();
    //usleep(10000);
  }

  ndn_face_destroy(&face->intf);
  return 0;
}
