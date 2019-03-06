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
#include "nfd-lite.h"

#include "ndn-lite/forwarder/forwarder.h"
#include "ndn-lite/encode/fragmentation-support.h"

#include <net/gnrc/netapi.h>
#include <net/gnrc/netif.h>
#include <net/gnrc/netreg.h>
#include <thread.h>
#include <timex.h>
#include <xtimer.h>

#define GNRC_NFD_LITE_STACK_SIZE        (THREAD_STACKSIZE_DEFAULT)
#define GNRC_NFD_LITE_PRIO              (THREAD_PRIORITY_MAIN - 3)
#define GNRC_NFD_LITE_MSG_QUEUE_SIZE    (8U)

static ndn_forwarder_t* forwarder = NULL;
static char _stack[GNRC_NFD_LITE_STACK_SIZE];
kernel_pid_t nfd_lite_pid = KERNEL_PID_UNDEF;

static void _process_packet(kernel_pid_t face_id, gnrc_pktsnip_t *pkt)
{
  ndn_netface_t* netface = ndn_netface_find((uint16_t)face_id);
  if (netface == NULL)
  {
    printf("nfd-lite: unregistered netface id: %" PRIkernel_pid ")\n", face_id);
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

/* Main event loop for NFD Lite */
static void *_event_loop(void *args)
{
  (void)args;
  msg_t msg, reply, msg_q[GNRC_NFD_LITE_MSG_QUEUE_SIZE];
  gnrc_netreg_entry_t me_reg;

  msg_init_queue(msg_q, GNRC_NFD_LITE_MSG_QUEUE_SIZE);

  me_reg.demux_ctx = GNRC_NETREG_DEMUX_CTX_ALL;
  me_reg.target.pid = thread_getpid();

  /* register interest in all NDN packets */
  gnrc_netreg_register(GNRC_NETTYPE_NDN, &me_reg);

  /* preinitialize ACK to GET/SET commands*/
  reply.type = GNRC_NETAPI_MSG_TYPE_ACK;

  /* start event loop */
  while (1)
  {
    msg_receive(&msg);
    switch (msg.type)
    {
      case GNRC_NETAPI_MSG_TYPE_RCV:
           printf("nfd-lite: RCV message received from pid %"
                  PRIkernel_pid "\n", msg.sender_pid);
          _process_packet(msg.sender_pid, (gnrc_pktsnip_t *)msg.content.ptr);
          break;

      case GNRC_NETAPI_MSG_TYPE_GET:
      case GNRC_NETAPI_MSG_TYPE_SET:
           reply.content.value = -ENOTSUP;
           msg_reply(&msg, &reply);
           break;

      case GNRC_NETAPI_MSG_TYPE_SND:
           printf("nfd-lite: SND message received from pid %"
                  PRIkernel_pid "\n", msg.sender_pid);
      default:
          break;
    }
  }
  return NULL;
}


kernel_pid_t nfd_lite_init(void)
{
  if (forwarder == NULL)
    forwarder = ndn_forwarder_init();

  ndn_netface_auto_construct();

  /* check if thread is already running */
  if (nfd_lite_pid == KERNEL_PID_UNDEF)
  {
    /* start UDP thread */
    nfd_lite_pid = thread_create(_stack, sizeof(_stack), GNRC_NFD_LITE_PRIO,
                                 THREAD_CREATE_STACKTEST, _event_loop, NULL, "nfd-lite");
  }
  return nfd_lite_pid;
}
