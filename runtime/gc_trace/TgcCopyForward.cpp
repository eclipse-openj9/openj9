
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

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "modronopt.h"
#include "mmhook.h"

#if defined(J9VM_GC_VLHGC)
#include "CycleState.hpp"
#include "EnvironmentVLHGC.hpp"
#include "GCExtensions.hpp"
#include "TgcExtensions.hpp"
#include "VMThreadListIterator.hpp"

/****************************************
 * Hook callback
 ****************************************
 */

static void
tgcHookCopyForwardEnd(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThread *vmThread = static_cast<J9VMThread*>(((MM_CopyForwardEndEvent *)eventData)->currentThread->_language_vmthread);
	MM_TgcExtensions *tgcExtensions = MM_TgcExtensions::getExtensions(vmThread);
	
	tgcExtensions->printf("CFDF:     cards   packets  overflow      next     depth      root\n");

	J9VMThread *walkThread = NULL;
	GC_VMThreadListIterator threadIterator(vmThread);
	while ((walkThread = threadIterator.nextVMThread()) != NULL) {
		MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(walkThread);
		if ((walkThread == vmThread) || (env->getThreadType() == GC_SLAVE_THREAD)) {
			tgcExtensions->printf("%4zu:   %7zu   %7zu   %7zu   %7zu   %7zu   %7zu\n",
				env->getSlaveID(),
				env->_copyForwardStats._objectsCardClean,
				env->_copyForwardStats._objectsScannedFromWorkPackets,
				env->_copyForwardStats._objectsScannedFromOverflowedRegion,
				env->_copyForwardStats._objectsScannedFromNextInChain,
				env->_copyForwardStats._objectsScannedFromDepthStack,
				env->_copyForwardStats._objectsScannedFromRoot
			);
		}
	}
}

/****************************************
 * Initialization
 ****************************************
 */

bool
tgcCopyForwardInitialize(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	bool result = true;

	J9HookInterface** privateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	(*privateHooks)->J9HookRegisterWithCallSite(privateHooks, J9HOOK_MM_PRIVATE_COPY_FORWARD_END, tgcHookCopyForwardEnd, OMR_GET_CALLSITE(), javaVM);

	return result;
}

#endif /* J9VM_GC_VLHGC */
