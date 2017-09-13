/*******************************************************************************
 * Copyright (c) 2003, 2014 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
#ifndef jsigunix_h
#define jsigunix_h

#ifdef __cplusplus
extern "C" {
#endif

#include "jsig.h"

/* Unix specific types */
typedef int (*sigactionfunc_t)(int, const struct sigaction *, struct sigaction *);

/* Shared Unix functions */
sig_handler_t bsd_signal(int, sig_handler_t);
sig_handler_t sysv_signal(int, sig_handler_t);

/* Platform dependent Unix functions */
int real_sigaction(int sig, const struct sigaction *act, struct sigaction *oact);
int real_sigprocmask(int option, const sigset_t *new_set, sigset_t *old_set);

#ifdef __cplusplus
}
#endif

#endif     /* jsigunix_h */

