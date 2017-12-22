/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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

#include "j9user.h"
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "omrthread.h"
#include "j9dump.h"
#include "jni.h"
#include "vmaccess.h"
#include "jvminit.h"

#include "jcl_internal.h"
#include "j9port.h"
#include "ut_j9jcl.h"



#define J9_SIQUIT_CYCLE_MSEC  200

#if defined(J9VM_INTERP_SIG_QUIT_THREAD)   /* priv. proto (autogen) */
static UDATA sigQuitHandler(struct J9PortLibrary* portLibrary, void* userData);
#endif /* J9VM_INTERP_SIG_QUIT_THREAD && PORT_SIGNAL_SUPPORT (autogen) */

#if defined(J9VM_INTERP_SIG_QUIT_THREAD)   /* priv. proto (autogen) */
static UDATA sigQuitWrapper(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData);
#endif /* J9VM_INTERP_SIG_QUIT_THREAD && PORT_SIGNAL_SUPPORT (autogen) */

#if (defined(J9VM_INTERP_SIG_QUIT_THREAD)) /* priv. proto (autogen) */

jint
J9SigQuitStartup(J9JavaVM * vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JCL_J9SigQuitStartup_Entry();

	if (vm->sigFlags & J9_SIG_NO_SIG_QUIT) {
		/* Set if -Xrs command-line option */
		Trc_JCL_J9SigQuitStartup_Disabled();
		return 0;
	}

	if (j9sig_set_async_signal_handler(sigQuitWrapper, vm, J9PORT_SIG_FLAG_SIGQUIT)) {
		Trc_JCL_J9SigQuitStartup_Failure();
		return JNI_ERR;
	}

	vm->J9SigQuitShutdown = J9SigQuitShutdown;

	Trc_JCL_J9SigQuitStartup_Exit();

	return 0;
}

#endif /* J9VM_INTERP_SIG_QUIT_THREAD (autogen) */


#if (defined(J9VM_INTERP_SIG_QUIT_THREAD)) /* priv. proto (autogen) */

void
J9SigQuitShutdown(J9JavaVM * vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JCL_J9SigQuitShutdown_Entry();
	j9sig_set_async_signal_handler(sigQuitWrapper, vm, 0);
	Trc_JCL_J9SigQuitShutdown_Exit();
}

#endif /* J9VM_INTERP_SIG_QUIT_THREAD (autogen) */


#if defined(J9VM_INTERP_SIG_QUIT_THREAD)   /* priv. proto (autogen) */

static UDATA 
sigQuitHandler(struct J9PortLibrary* portLibrary, void* userData)
{
	J9JavaVM* vm = userData;
	omrthread_t currentThread;
	static U_64 lastDumpTime = 0;
	UDATA currentPriority;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (omrthread_attach_ex(&currentThread, J9THREAD_ATTR_DEFAULT)) {
		return 0;
	}

	/* Minimise overlapping dump requests (Note that this probably won't work very well 
	 * with multiple VM instances, since lastDumpTime is a static). The proper solution is
	 * to make this handler multi-VM aware, rather than registering one handler per VM
	 */
	if ( j9time_hires_delta(lastDumpTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MILLISECONDS) < J9_SIQUIT_CYCLE_MSEC ) {
		return 0;
	}

	currentPriority = omrthread_get_priority(currentThread);
	omrthread_set_priority(currentThread, J9THREAD_PRIORITY_MAX);

	/**** WARNING: do not try and attach a VM thread here, as we may be debugging a deadlock ****/

#ifdef J9VM_RAS_DUMP_AGENTS
	J9DMP_TRIGGER( vm, NULL, J9RAS_DUMP_ON_USER_SIGNAL );
#else
	vm->internalVMFunctions->printThreadInfo(vm, NULL, NULL, TRUE);
#endif

	/* Listeners to this event may deadlock, so run dumps first */
	TRIGGER_J9HOOK_VM_USER_INTERRUPT(vm->hookInterface, vm);

	/* Update cycle time only after finished dumps */
	lastDumpTime = j9time_hires_clock();

	omrthread_set_priority(currentThread, currentPriority);
	omrthread_detach(currentThread);

	return 0;
}
#endif /* J9VM_INTERP_SIG_QUIT_THREAD  (autogen) */


#if defined(J9VM_INTERP_SIG_QUIT_THREAD)   /* priv. proto (autogen) */

static UDATA 
sigQuitWrapper(struct J9PortLibrary* portLibrary, U_32 gpType, void* gpInfo, void* userData)
{
	J9JavaVM* vm = userData;
	UDATA result;

	PORT_ACCESS_FROM_JAVAVM(vm);

	j9sig_protect(sigQuitHandler, vm,
		vm->internalVMFunctions->structuredSignalHandlerVM, vm,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
		&result);

	return result;
}
#endif /* J9VM_INTERP_SIG_QUIT_THREAD  (autogen) */



