/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

/**
 * @file
 * @ingroup VMChk
 */

#include "j9.h"
#include "j9port.h"
#include "vmaccess.h"
#include "vmcheck.h"

/*
 *	J9VMThread sanity:
 *		Valid VM check:
 *			J9VMThread->javaVM->javaVM == J9VMThread->javaVM.
 *
 *		Exclusive access check:
 *			If any J9VMThread has exclusive access, ensure no other thread has exclusive or vm access.
 */

static void verifyJ9VMThread(J9JavaVM *vm, J9VMThread *thread);


/**
 * Check VM thread sanity.
 *
 * Note: Caller must either have exclusive VM access or must
 *       be executing on behalf of a thread that does.
 *
 * @param[in] J9JavaVM The Java VM to check. *
 */
void
checkJ9VMThreadSanity(J9JavaVM *vm)
{
	UDATA count = 0;
	J9VMThread *mainThread;
	J9VMThread *thread;
	UDATA numberOfThreadsWithVMAccess = 0;
	BOOLEAN exclusiveVMAccess = (J9_XACCESS_EXCLUSIVE == DBG_ARROW(vm, exclusiveAccessState));
	VMCHECK_PORT_ACCESS_FROM_JAVAVM(vm);

	vmchkPrintf(vm, "  %s Checking threads>\n", VMCHECK_PREFIX);

	mainThread = (J9VMThread *)DBG_ARROW(vm, mainThread);
	thread = mainThread;
	do {
		verifyJ9VMThread(vm, thread);

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		if (!DBG_ARROW(thread, inNative))
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
		{
			if (J9_PUBLIC_FLAGS_VM_ACCESS == (DBG_ARROW(thread, publicFlags) & J9_PUBLIC_FLAGS_VM_ACCESS)) {
				numberOfThreadsWithVMAccess++;
			}
		}

		count++;
		thread = (J9VMThread *)DBG_ARROW(thread, linkNext);
	} while (thread != mainThread);

	if (exclusiveVMAccess && (numberOfThreadsWithVMAccess > 1)) {
		vmchkPrintf(vm, "%s - Error numberOfThreadsWithVMAccess (%d) > 1 with vm->exclusiveAccessState == J9_XACCESS_EXCLUSIVE>\n",
			VMCHECK_FAILED, numberOfThreadsWithVMAccess);
	}

	vmchkPrintf(vm, "  %s Checking %d threads done>\n", VMCHECK_PREFIX, count);
}

static void
verifyJ9VMThread(J9JavaVM *vm, J9VMThread *thread)
{
	J9JavaVM *threadJavaVM = (J9JavaVM *)DBG_ARROW(thread, javaVM);

	if (vm != threadJavaVM) {
		vmchkPrintf(vm, "%s - Error vm (0x%p) != thread->javaVM (0x%p) for thread=0x%p>\n",
			VMCHECK_FAILED, vm, threadJavaVM, thread);
	} else if (threadJavaVM != (J9JavaVM *)DBG_ARROW(threadJavaVM, javaVM)) {
		vmchkPrintf(vm, "%s - Error thread->javaVM (0x%p) != thread->javaVM->javaVM (0x%p) for thread=0x%p>\n",
			VMCHECK_FAILED, threadJavaVM, DBG_ARROW(threadJavaVM, javaVM), thread);
	}
}
