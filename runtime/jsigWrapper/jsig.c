/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#if defined(WIN32)
#include <windows.h>
#else /* defined(WIN32) */
#include <dlfcn.h>
#endif /* defined(WIN32) */
#include <errno.h>
#include <stddef.h>

#if defined(J9ZOS390)
#include <dll.h>
#include "atoe.h"
#define dlsym dllqueryfn
#define dlopen(a,b) dllload(a)
#define dlclose dllfree
#endif /* defined(J9ZOS390) */

#include "jsig.h"
#include "omrutilbase.h"

static void lockTable(void);
static void unlockTable(void);
static void *getFunction(void **tableAddress, char *name);
static SigHandlerTable handlerTable = {0};

static void
lockTable(void)
{
	uintptr_t oldLocked = 0;
	do {
		oldLocked = handlerTable.locked;
	} while (0 != compareAndSwapUDATA((uintptr_t*)&handlerTable.locked, oldLocked, 1));
	issueReadWriteBarrier();
}

static void
unlockTable(void)
{
	issueReadWriteBarrier();
	handlerTable.locked = 0;
}

static void *
getFunction(void **tableAddress, char *name)
{
	void *function = NULL;

	lockTable();
	if (NULL == *tableAddress) {
#if defined(WIN32)
		*tableAddress = GetProcAddress(GetModuleHandle(TEXT("omrsig.dll")), name);
#elif defined(J9ZTPF) /* defined(WIN32) */
		void *handle = dlopen("libomrsig.so", RTLD_LAZY);
		*tableAddress = dlsym(handle, name);
#else /* defined(WIN32) */
		/* Note: this needs to be ifdef'd this way for zOS to compile as it redefines dlopen */
		void *handle = 
#if defined(OSX)
			dlopen("libomrsig.dylib", RTLD_GLOBAL | RTLD_LAZY);
#else /* OSX */
			dlopen("libomrsig.so", RTLD_GLOBAL | RTLD_LAZY);
#endif /*OSX */
		*tableAddress = dlsym(handle, name);
#endif /* defined(WIN32) */
	}
	function = *tableAddress;
	unlockTable();

	return function;
}

#if defined(WIN32)
#pragma warning(push)
#pragma warning(disable : 4273)
void (__cdecl * __cdecl
signal(_In_ int signum, _In_opt_ void (__cdecl *handler)(int)))(int)
#else /* defined(WIN32) */
sighandler_t
signal(int signum, sighandler_t handler)
#endif /* defined(WIN32) */
{
	sighandler_t rc = SIG_ERR;
	Signal func = (Signal)getFunction(&handlerTable.signal, "signal");

	if (NULL != func) {
		rc = func(signum, handler);
	}
	return rc;
}
#if defined(WIN32)
#pragma warning(pop)
#endif /* defined(WIN32) */

#if !defined(WIN32)

int
sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	int rc = -1;
	SigAction func = (SigAction)getFunction(&handlerTable.sigaction, "sigaction");

	if (NULL != func) {
		rc = func(signum, act, oldact);
	}
	return rc;
}

sighandler_t
sigset(int sig, sighandler_t disp)
{
	sighandler_t rc = SIG_ERR;
	SigSet func = (SigSet)getFunction(&handlerTable.sigset, "sigset");

	if (NULL != func) {
		rc = func(sig, disp);
	}
	return rc;
}

int
sigignore(int sig)
{
	int rc = -1;
	SigIgnore func = (SigIgnore)getFunction(&handlerTable.sigignore, "sigignore");

	if (NULL != func) {
		rc = func(sig);
	}
	return rc;
}

sighandler_t
bsd_signal(int signum, sighandler_t handler)
{
	sighandler_t rc = SIG_ERR;
	Bsd_Signal func = (Bsd_Signal)getFunction(&handlerTable.bsd_signal, "bsd_signal");

	if (NULL != func) {
		rc = func(signum, handler);
	}
	return rc;
}

#if !defined(J9ZOS390)

sighandler_t
sysv_signal(int signum, sighandler_t handler)
{
	sighandler_t rc = SIG_ERR;
	Sysv_Signal func = (Sysv_Signal)getFunction(&handlerTable.sysv_signal, "sysv_signal");

	if (NULL != func) {
		rc = func(signum, handler);
	}
	return rc;
}

#endif /* !defined(J9ZOS390) */

#if defined(LINUX)

__sighandler_t
__sysv_signal(int sig, __sighandler_t handler)
{
	sighandler_t rc = SIG_ERR;
	__Sysv_Signal func = (__Sysv_Signal)getFunction(&handlerTable.__sysv_signal, "__sysv_signal");

	if (NULL != func) {
		rc = func(sig, handler);
	}
	return rc;
}

sighandler_t
ssignal(int sig, sighandler_t handler)
{
	sighandler_t rc = SIG_ERR;
	Ssignal func = (Ssignal)getFunction(&handlerTable.ssignal, "ssignal");

	if (NULL != func) {
		rc = func(sig, handler);
	}
	return rc;
}

#endif /* defined(LINUX) */

int
jsig_primary_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	int rc = -1;
	Sig_Primary_Sigaction func = (Sig_Primary_Sigaction)getFunction(&handlerTable.sig_primary_sigaction, "omrsig_primary_sigaction");

	if (NULL != func) {
		rc = func(signum, act, oldact);
	}
	return rc;
}

#endif /* !defined(WIN32) */


int
jsig_handler(int sig, void *siginfo, void *uc)
{
	int rc = OMRSIG_RC_DEFAULT_ACTION_REQUIRED;
	Sig_Handler func = (Sig_Handler)getFunction(&handlerTable.sig_handler, "omrsig_handler");

	if (NULL != func) {
		rc = func(sig, siginfo, uc);
	}
	return rc;
}

sighandler_t
jsig_primary_signal(int signum, sighandler_t handler)
{
	sighandler_t rc = SIG_ERR;
	Sig_Primary_Signal func = (Sig_Primary_Signal)getFunction(&handlerTable.sig_primary_signal, "omrsig_primary_signal");

	if (NULL != func) {
		rc = func(signum, handler);
	}
	return rc;
}
