
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
#if defined(J9VM_GC_IDLE_HEAP_MANAGER)
#include "j9protos.h"
#include "j9consts.h"
#include "vmhook_internal.h"

#include "IdleGCManager.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "OMRVMInterface.hpp"
#include "Heap.hpp"

MM_IdleGCManager *
MM_IdleGCManager::newInstance(MM_EnvironmentBase* env)
{
	MM_IdleGCManager* idleGCMgr = (MM_IdleGCManager*)env->getForge()->allocate(sizeof(MM_IdleGCManager), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (idleGCMgr) {
		new(idleGCMgr) MM_IdleGCManager(env);
		if (!idleGCMgr->initialize(env)) {
			idleGCMgr->kill(env);
			idleGCMgr = NULL;
		}
	}
	return idleGCMgr;
}

void
MM_IdleGCManager::kill(MM_EnvironmentBase* env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_IdleGCManager::tearDown(MM_EnvironmentBase* env)
{
	J9HookInterface** hookInterface = _javaVM->internalVMFunctions->getVMHookInterface(_javaVM);
	if (NULL != hookInterface) {
		(*hookInterface)->J9HookUnregister(hookInterface, J9HOOK_VM_RUNTIME_STATE_CHANGED, idleGCManagerVMStateHook, this);
	}
}

bool
MM_IdleGCManager::initialize(MM_EnvironmentBase* env)
{
	J9HookInterface** hookInterface = _javaVM->internalVMFunctions->getVMHookInterface(_javaVM);
	if (NULL != hookInterface && (*hookInterface)->J9HookRegister(hookInterface, J9HOOK_VM_RUNTIME_STATE_CHANGED, idleGCManagerVMStateHook, this)) {
		return false;
	}
	return true;
}

void
MM_IdleGCManager::manageFreeHeap(J9VMThread* currentThread)
{
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(currentThread->omrVMThread);
	MM_GCExtensions* _extensions = MM_GCExtensions::getExtensions(env);

	_javaVM->internalVMFunctions->internalAcquireVMAccess(currentThread);
	_extensions->heap->systemGarbageCollect(env, J9MMCONSTANT_EXPLICIT_GC_IDLE_GC);
	_javaVM->internalVMFunctions->internalReleaseVMAccess(currentThread);
}

extern "C" {
void idleGCManagerVMStateHook(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	J9VMRuntimeStateChanged* j9VMState = (J9VMRuntimeStateChanged*)eventData;
	MM_IdleGCManager* idleMgr = (MM_IdleGCManager*)userData;

	/* idle gc manager currently handles only ACTIVE -> IDLE transition*/
	if (j9VMState->state == J9VM_RUNTIME_STATE_IDLE) {
		idleMgr->manageFreeHeap(j9VMState->vmThread);
	}
}
} /*end extern "C"  */
#endif /* defined(J9VM_GC_IDLE_HEAP_MANAGER) */
