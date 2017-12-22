/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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
 
#if !defined(JSIG_H_)
#define JSIG_H_

#include <stdint.h>
#include "omrsig.h"

#define JSIG_RC_SIGNAL_HANDLED OMRSIG_RC_SIGNAL_HANDLED
#define JSIG_RC_DEFAULT_ACTION_REQUIRED OMRSIG_RC_DEFAULT_ACTION_REQUIRED

typedef int (*Sig_Handler)(int sig, void *siginfo, void *uc);
int jsig_handler(int sig, void *siginfo, void *uc);
typedef sighandler_t (*Sig_Primary_Signal)(int signum, sighandler_t handler);
sighandler_t jsig_primary_signal(int signum, sighandler_t handler);

#if defined(WIN32)

typedef void (__cdecl * (__cdecl *Signal)(_In_ int _SigNum, _In_opt_ void (__cdecl * _Func)(int)))(int);
_CRTIMP void (__cdecl * __cdecl signal(_In_ int _SigNum, _In_opt_ void (__cdecl * _Func)(int)))(int);

#else /* defined(WIN32) */

typedef int (*Sig_Primary_Sigaction)(int signum, const struct sigaction *act, struct sigaction *oldact);
int jsig_primary_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

typedef int (*SigAction)(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
typedef sighandler_t (*Signal)(int signum, sighandler_t handler);
sighandler_t signal(int signum, sighandler_t handler);

typedef sighandler_t (*SigSet)(int sig, sighandler_t disp);
sighandler_t sigset(int sig, sighandler_t disp);
typedef int (*SigIgnore)(int sig);
int sigignore(int sig);
typedef sighandler_t (*Bsd_Signal)(int signum, sighandler_t handler);
sighandler_t bsd_signal(int signum, sighandler_t handler);

#if !defined(J9ZOS390)
typedef sighandler_t (*Sysv_Signal)(int signum, sighandler_t handler);
sighandler_t sysv_signal(int signum, sighandler_t handler);
#endif /* !defined(J9ZOS390) */

#if defined(LINUX)
typedef __sighandler_t (*__Sysv_Signal)(int sig, __sighandler_t handler);
__sighandler_t __sysv_signal(int sig, __sighandler_t handler);
typedef sighandler_t (*Ssignal)(int sig, sighandler_t handler);
sighandler_t ssignal(int sig, sighandler_t handler);
#endif /* defined(LINUX) */

#if defined(J9ZOS390)
typedef int (*__SigActionSet)(size_t newct, const __sigactionset_t newsets[], size_t *oldct, __sigactionset_t oldsets[], int options);
int __sigactionset(size_t newct, const __sigactionset_t newsets[], size_t *oldct, __sigactionset_t oldsets[], int options);
#endif /* defined(J9ZOS390) */

#endif /* defined(WIN32) */

typedef struct SigHandlerTable {
	volatile uintptr_t locked;
	void *signal;
	void *sig_primary_signal;
	void *sig_handler;
#if !defined(WIN32)
	void *sig_primary_sigaction;
	void *sigaction;
	void *sigset;
	void *sigignore;
	void *bsd_signal;
#if !defined(J9ZOS390)
	void *sysv_signal;
#endif /* !defined(J9ZOS390) */
#if defined(LINUX)
	void *__sysv_signal;
	void *ssignal;
#endif /* defined(LINUX) */
#endif /* !defined(WIN32) */
} SigHandlerTable;

#endif /* !defined(JSIG_H_) */
