/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

#include "gnrc-netface/netface.h"
#include "ndn-lite.h"

#include "ndn-lite/forwarder/forwarder.h"
#include "ndn-lite/encode/fragmentation-support.h"

#include <thread.h>
#include <timex.h>
#include <xtimer.h>

static ndn_lite_instance_t instance;

static void _process_packet(kernel_pid_t face_id, gnrc_pktsnip_t *pkt)
{
  ndn_netface_t* netface = ndn_netface_find((uint16_t)face_id);
  if (netface == NULL)
  {
    printf("ndn-lite: unregistered netface id: %" PRIkernel_pid ")\n", face_id);
    return;
  }

  // reassembly
  static ndn_frag_assembler_t assembler;
  ndn_frag_assembler_init(&assembler, netface->frag_buffer, sizeof(netface->frag_buffer));
  ndn_frag_assembler_assemble_frag(&assembler, (uint8_t*)pkt->data, pkt->size);

  // if finished, deliver to the forwarder
  if (assembler.is_finished)
  {
    ndn_face_receive(&netface->intf, assembler.original, assembler.offset);
    memset(netface->frag_buffer, 0, sizeof(netface->frag_buffer));
  }
}

ndn_lite_instance_t* ndn_lite_init(void)
{
  // initialize forwarder
  instance.forwarder = ndn_forwarder_init();
  if (!instance.forwarder) return NULL;

  // auto construct netfaces and print info
  ndn_netface_auto_construct();
  ndn_netface_traverse_print();

  // prepare the RIOT message queue
  msg_init_queue(instance.msg_q, GNRC_NDN_LITE_MSG_QUEUE_SIZE);
  instance.me_reg.demux_ctx = GNRC_NETREG_DEMUX_CTX_ALL;
  instance.me_reg.target.pid = thread_getpid();
  /* register interest in all NDN packets */
  gnrc_netreg_register(GNRC_NETTYPE_NDN, &instance.me_reg);
  /* preinitialize ACK to GET/SET commands*/
  instance.reply.type = GNRC_NETAPI_MSG_TYPE_ACK;

  return &instance;
}

int ndn_lite_process(ndn_lite_instance_t* instance)
{
  // process forwarder first to check PIT
  ndn_forwarder_process(instance->forwarder);

  // process RIOT message queue
  msg_receive(&instance->msg);
  switch (instance->msg.type)
  {
    case GNRC_NETAPI_MSG_TYPE_RCV:
         printf("ndn-lite: RCV message received from pid %"
                PRIkernel_pid "\n", instance->msg.sender_pid);
        _process_packet(instance->msg.sender_pid,
                       (gnrc_pktsnip_t *)instance->msg.content.ptr);
        break;

    case GNRC_NETAPI_MSG_TYPE_GET:
    case GNRC_NETAPI_MSG_TYPE_SET:
         instance->reply.content.value = -ENOTSUP;
         msg_reply(&instance->msg, &instance->reply);
         break;

    case GNRC_NETAPI_MSG_TYPE_SND:
         printf("ndn-lite: SND message received from pid %"
                PRIkernel_pid "\n", instance->msg.sender_pid);
    default:
        break;
  }

  return NDN_SUCCESS;
}
