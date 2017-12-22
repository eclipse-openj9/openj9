
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

/**
 * @file
 * @ingroup GC_Base
 */
#if !defined(IDLERESOURCEMANAGERHPP_)
#define IDLERESOURCEMANAGERHPP_
#include "j9.h"
#include "j9cfg.h"
#if defined(J9VM_GC_IDLE_HEAP_MANAGER)
#include "BaseNonVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"

extern "C" {
/**
 * Hook "J9HOOK_VM_RUNTIME_STATE_CHANGED" callback function
 * Manages Heap Free Pages If Current Runtime State is IDLE
 */
void idleGCManagerVMStateHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
}

/**
 * Manages free java heap memory whenever JVM becomes idle. Registers for VM Runtime State Notification Hook
 */
class MM_IdleGCManager : public MM_BaseNonVirtual
{
private:
	/*
	 * reference to the language runtime
	 */
	J9JavaVM* _javaVM;

protected:
public:

private:
protected:
	/**
	 * Initialize the object of this class and registers for Runtime State hook
	 */
	bool initialize(MM_EnvironmentBase* env);
	/**
	 * cleanup the object & unregisters registered hook
	 */
	void tearDown(MM_EnvironmentBase* env);
public:
	/**
	 * creates the object
	 */
	static MM_IdleGCManager* newInstance(MM_EnvironmentBase* env);
	/**
	 * deallocates the object
	 */
	void kill(MM_EnvironmentBase* env);
	/**
	  * Whenever JVM becomes idle, uses the opportunity to free up pages of free java heap
	  */
	void manageFreeHeap(J9VMThread* currentThread);

	/**
	 * construct the object
	 */
	MM_IdleGCManager(MM_EnvironmentBase* env)
		: MM_BaseNonVirtual()
		, _javaVM((J9JavaVM*)env->getOmrVM()->_language_vm)
	{
		_typeId = __FUNCTION__;
	}
};
#endif /* defined(J9VM_GC_IDLE_HEAP_MANAGER) */
#endif /* IDLERESOURCEMANAGERHPP_ */
