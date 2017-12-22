/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef J9THREAD_H
#define J9THREAD_H

/* @ddr_namespace: default */
#include "omrthread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define j9thread_t omrthread_t
#define j9thread_tls_key_t omrthread_tls_key_t
#define j9thread_entrypoint_t omrthread_entrypoint_t
#define j9thread_process_time_t omrthread_process_time_t

#define j9thread_sleep omrthread_sleep
#define j9thread_sleep_interruptable omrthread_sleep_interruptable
#define j9thread_yield omrthread_yield
#define j9thread_exit omrthread_exit
#define j9thread_interrupt omrthread_interrupt
#define j9thread_set_name omrthread_set_name
#define j9thread_get_priority omrthread_get_priority
#define j9thread_set_priority omrthread_set_priority
#define j9thread_get_handle omrthread_get_handle

#define j9thread_monitor_init_with_name omrthread_monitor_init_with_name
#define j9thread_monitor_destroy omrthread_monitor_destroy
#define j9thread_monitor_enter omrthread_monitor_enter
#define j9thread_monitor_try_enter omrthread_monitor_try_enter
#define j9thread_monitor_exit omrthread_monitor_exit
#define j9thread_monitor_wait omrthread_monitor_wait
#define j9thread_monitor_wait_timed omrthread_monitor_wait_timed
#define j9thread_monitor_notify omrthread_monitor_notify
#define j9thread_monitor_notify_all omrthread_monitor_notify_all
#define j9thread_monitor_owned_by_self omrthread_monitor_owned_by_self
#define j9thread_monitor_num_waiting omrthread_monitor_num_waiting

#define j9thread_rwmutex_init omrthread_rwmutex_init
#define j9thread_rwmutex_destroy omrthread_rwmutex_destroy
#define j9thread_rwmutex_enter_write omrthread_rwmutex_enter_write
#define j9thread_rwmutex_exit_write omrthread_rwmutex_exit_write
#define j9thread_rwmutex_enter_read omrthread_rwmutex_enter_read
#define j9thread_rwmutex_exit_read omrthread_rwmutex_exit_read

#define j9thread_get_process_times omrthread_get_process_times
#define j9thread_get_cpu_time omrthread_get_cpu_time
#define j9thread_get_self_cpu_time omrthread_get_self_cpu_time

omrthread_t j9thread_self(void);
void *j9thread_tls_get(omrthread_t thread, omrthread_tls_key_t key);

#ifdef __cplusplus
}
#endif

#endif /* J9THREAD_H */
