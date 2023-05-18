/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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

#define J9_SIGUSR2_CYCLE_MSEC  200

static UDATA sigUsr2Handler(struct J9PortLibrary *portLibrary, void *userData);
static UDATA sigUsr2Wrapper(struct J9PortLibrary *portLibrary, U_32 gpType, void *gpInfo, void *userData);

jint
J9SigUsr2Startup(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JCL_J9SigUsr2Startup_Entry();

#if defined(SIGUSR2)
	if (OMR_ARE_ANY_BITS_SET(vm->sigFlags, J9_SIG_NO_SIG_USR2)) {
		/* Set if either -Xrs or -XX:-HandleSIGUSR2 command-line options are specified. */
		Trc_JCL_J9SigUsr2Startup_Disabled();
		return 0;
	}

	if (0 != j9sig_set_async_signal_handler(sigUsr2Wrapper, vm, OMRPORT_SIG_FLAG_SIGUSR2)) {
		Trc_JCL_J9SigUsr2Startup_Failure();
		return JNI_ERR;
	}

	vm->J9SigUsr2Shutdown = J9SigUsr2Shutdown;

	Trc_JCL_J9SigUsr2Startup_Exit();
#else /* defined(SIGUSR2) */
	Trc_JCL_J9SigUsr2Startup_Unavailable();
#endif /* defined(SIGUSR2) */

	return 0;
}

void
J9SigUsr2Shutdown(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JCL_J9SigUsr2Shutdown_Entry();
	j9sig_set_async_signal_handler(sigUsr2Wrapper, vm, 0);
	Trc_JCL_J9SigUsr2Shutdown_Exit();
}

static UDATA
sigUsr2Handler(struct J9PortLibrary *portLibrary, void *userData)
{
	J9JavaVM *vm = (J9JavaVM *)userData;
	omrthread_t currentThread = NULL;
	static U_64 lastDumpTime = 0;
	UDATA currentPriority = 0;

	PORT_ACCESS_FROM_JAVAVM(vm);

	if (J9THREAD_SUCCESS != omrthread_attach_ex(&currentThread, J9THREAD_ATTR_DEFAULT)) {
		return 0;
	}

	/* Minimise overlapping dump requests (Note that this probably won't work very well
	 * with multiple VM instances, since lastDumpTime is a static). The proper solution is
	 * to make this handler multi-VM aware, rather than registering one handler per VM.
	 */
	if (j9time_hires_delta(lastDumpTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MILLISECONDS) < J9_SIGUSR2_CYCLE_MSEC) {
		return 0;
	}

	currentPriority = omrthread_get_priority(currentThread);
	omrthread_set_priority(currentThread, J9THREAD_PRIORITY_MAX);

	/**** WARNING: do not try and attach a VM thread here, as we may be debugging a deadlock. ****/

	J9DMP_TRIGGER(vm, NULL, J9RAS_DUMP_ON_USER2_SIGNAL);

	/* Update cycle time only after finished dumps. */
	lastDumpTime = j9time_hires_clock();

	omrthread_set_priority(currentThread, currentPriority);
	omrthread_detach(currentThread);

	return 0;
}

static UDATA
sigUsr2Wrapper(struct J9PortLibrary *portLibrary, U_32 gpType, void *gpInfo, void *userData)
{
	J9JavaVM *vm = (J9JavaVM *)userData;
	UDATA result = 0;

	PORT_ACCESS_FROM_JAVAVM(vm);

	j9sig_protect(sigUsr2Handler, vm,
		vm->internalVMFunctions->structuredSignalHandlerVM, vm,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
		&result);

	return result;
}
