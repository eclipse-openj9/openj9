
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#include "j9.h"
#include "j9cfg.h"
#include "omrhookable.h"
#include "ModronAssertions.h"

#include "VMInterface.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "MemorySpace.hpp"
#include "OMRVMInterface.hpp"

extern "C" {

void 
j9gc_all_object_and_vm_slots_do(J9JavaVM *javaVM, void *function, void *userData, UDATA walkFlags)
{
	/* Deprecated Function - Remove */
	Assert_MM_unreachable();
}

void 
j9gc_flush_caches_for_walk(J9JavaVM *javaVM)
{
	GC_OMRVMInterface::flushCachesForWalk(javaVM->omrVM);
}

void
j9gc_flush_nonAllocationCaches_for_walk(J9JavaVM *javaVM)
{
	J9VMThread* currentThread = javaVM->internalVMFunctions->currentVMThread(javaVM);
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(currentThread->omrVMThread);
	GC_OMRVMInterface::flushNonAllocationCaches(env);
}

J9HookInterface**
j9gc_get_hook_interface(J9JavaVM *javaVM)
{
	return GC_VMInterface::getHookInterface(MM_GCExtensions::getExtensions(javaVM));

}

J9HookInterface**
j9gc_get_omr_hook_interface(OMR_VM *omrVM)
{
	return GC_OMRVMInterface::getOmrHookInterface(MM_GCExtensionsBase::getExtensions(omrVM));

}

/**
 * Returns the arraylet leaf size in bytes
 * @parm jajaVM - the Java VM
 * @return arraylet leaf size.
 */ 
UDATA 
j9gc_arraylet_getLeafSize(J9JavaVM* javaVM)
{
	return javaVM->arrayletLeafSize;
}

/**
 * Returns the shift count of the leaf size,
 * where leaf size = 1 << leaf lig size
 * @parm jajaVM - the Java VM
 * @return arraylet leaf log size.
 */ 
UDATA 
j9gc_arraylet_getLeafLogSize(J9JavaVM* javaVM)
{
	return javaVM->arrayletLeafLogSize;
}

/**
 * This API ensures integrity of the Ownable List, it is to be used prior to building threadinfo externally.
 * This should be called as early as possible, before gahtering any references, as this method may move the references.
 */
void
j9gc_ensureLockedSynchronizersIntegrity(J9VMThread *vmThread)
{
	Assert_MM_true(vmThread->omrVMThread->exclusiveCount > 0);

	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread->omrVMThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();

	/* GC is only concerned when we're in the middle of a CS cycle, we must complete the
	 * cycle to ensure integrity of the Ownable Sync list*/
	if (extensions->isConcurrentScavengerInProgress()) {
		((MM_MemorySpace *)vmThread->omrVMThread->memorySpace)->localGarbageCollect(env, J9MMCONSTANT_IMPLICIT_GC_COMPLETE_CONCURRENT);
	}
}

} /* Extern C */
