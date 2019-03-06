/*
 * Copyright (C) 2019 Tianyuan Yu
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 *
 * See AUTHORS.md for complete list of NDN IOT PKG authors and contributors.
 */

 #include <thread.h>
 #include <timex.h>
 
 /**
  * @brief   The PID to the NDN Lite thread.
  *
  * @note    Use @ref ndn_init() to initialize. **Do not set by hand**.
  */
 extern kernel_pid_t nfd_lite_pid;

 /*
  * @brief   Initialization of the NDN thread.
  *
  * @return  The PID to the NDN thread, on success.
  * @return  a negative errno on error.
  * @return  -EOVERFLOW, if there are too many threads running already
  * @return  -EEXIST, if NDN was already initialized.
  */
 kernel_pid_t nfd_lite_init(void);
