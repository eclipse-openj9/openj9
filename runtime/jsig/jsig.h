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


#ifndef jsig_h
#define jsig_h

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>

#ifdef WIN32

#ifndef JSIGDLLSPEC
#define JSIGDLLSPEC __declspec(dllimport)
#endif

typedef void (__cdecl *sig_handler_t)(int);

JSIGDLLSPEC int omrsig_handler(int sig, void *siginfo, void *uc);
JSIGDLLSPEC sig_handler_t omrsig_primary_signal(int sig, sig_handler_t disp);

#else /* UNIX */

typedef void (*sig_handler_t)(int);

int omrsig_handler(int sig, void *siginfo, void *uc);
int  omrsig_primary_sigaction(int sig, const struct sigaction *act, struct sigaction *oact);
sig_handler_t omrsig_primary_signal(int sig, sig_handler_t disp);

#endif /* UNIX */

#ifdef __cplusplus
}
#endif

#define JSIG_RC_DEFAULT_ACTION_REQUIRED 0
#define JSIG_RC_SIGNAL_HANDLED 1

#endif     /* jsig_h */
 

