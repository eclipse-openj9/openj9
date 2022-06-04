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

#include <string.h>

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "vmhook_internal.h"
#include "ut_j9vm.h"
#include "vm_internal.h"


static void hookRegistrationEvent (J9HookInterface** hook, UDATA eventNum, void* voidEventData, void* userData);
static void hookAboutToBootstrapEvent (J9HookInterface** hook, UDATA eventNum, void* voidEventData, void* userData);


/*
 * Returns the VM's hook interface.
 * Callers should include "vmhook.h" for event and struct definitions.
 * This function is available through the J9VMInternalVMFunctions table
 */
J9HookInterface**
getVMHookInterface(J9JavaVM* vm)
{
	return J9_HOOK_INTERFACE(vm->hookInterface);
}


/*
 * Initialize the VM's hook interface.
 * Return 0 on success, non-zero on failure.
 */
IDATA
initializeVMHookInterface(J9JavaVM* vm)
{
	J9HookInterface** hookInterface = J9_HOOK_INTERFACE(vm->hookInterface);
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (J9HookInitializeInterface(hookInterface, OMRPORT_FROM_J9PORT(PORTLIB), sizeof(vm->hookInterface))) {
		return -1;
	}


	if ((*hookInterface)->J9HookRegisterWithCallSite(hookInterface, J9HOOK_REGISTRATION_EVENT, hookRegistrationEvent, OMR_GET_CALLSITE(), vm)) {
		return -1;
	}

	/* the VM wants to be the last one to respond to the about to bootstrap event so that other threads can set things up first */
	if ((*hookInterface)->J9HookRegisterWithCallSite(hookInterface, J9HOOK_VM_ABOUT_TO_BOOTSTRAP | J9HOOK_TAG_AGENT_ID, hookAboutToBootstrapEvent, OMR_GET_CALLSITE(), NULL, J9HOOK_AGENTID_LAST)) {
		return -1;
	}

	return 0;
}


/*
 * Shutdown the VM's hook interface.
 */
void
shutdownVMHookInterface(J9JavaVM* vm)
{
	J9HookInterface** hookInterface = J9_HOOK_INTERFACE(vm->hookInterface);

	if (*hookInterface) {
		(*hookInterface)->J9HookShutdownInterface(hookInterface);
	}
}


static void
hookRegistrationEvent(J9HookInterface** hook, UDATA eventNum, void* voidEventData, void* userData)
{
	J9HookRegistrationEvent* eventData = voidEventData;
	J9JavaVM* vm = userData;

	Trc_VM_hookRegistration(NULL, eventData->isRegistration, eventData->eventNum, (void*)eventData->function, eventData->userData);

	switch (eventData->eventNum) {
#ifdef J9VM_INTERP_PROFILING_BYTECODES
		case J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL:
			profilingBytecodeBufferFullHookRegistered(vm);
			break;
#endif
	}
}


/*
 * The VM is about to start loading classes.
 * Disable hooks which must have been hooked by now
 */
static void
hookAboutToBootstrapEvent(J9HookInterface** hook, UDATA eventNum, void* voidEventData, void* userData)
{
	J9VMAboutToBootstrapEvent* eventData = voidEventData;
	J9VMThread* vmThread = eventData->currentThread;
	J9JavaVM* vm = vmThread->javaVM;
	J9HookInterface** hookInterface = J9_HOOK_INTERFACE(vm->hookInterface);

	/* these hooks must be reserved by now. Attempt to disable them so that they're in a well-known state after this */
	(*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_MONITOR_CONTENDED_EXIT);

	if ((*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_METHOD_ENTER)
		|| (*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_METHOD_RETURN)
		|| (*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_FRAME_POP)
		|| (*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_POP_FRAMES_INTERRUPT)
		|| (*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_SINGLE_STEP)
		|| (*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_BREAKPOINT)
		|| (*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_GET_FIELD)
		|| (*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_PUT_FIELD)
		|| (*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_GET_STATIC_FIELD)
		|| (*hookInterface)->J9HookDisable(hookInterface, J9HOOK_VM_PUT_STATIC_FIELD)
		|| (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_METHOD_TRACE_ENABLED)
		|| (vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_CAN_ACCESS_LOCALS))
	{
		omrthread_monitor_enter(vm->runtimeFlagsMutex);
		vm->extendedRuntimeFlags |= J9_EXTENDED_RUNTIME_DEBUG_MODE;
		omrthread_monitor_exit(vm->runtimeFlagsMutex);
	}
}



#if defined(J9VM_INTERP_NATIVE_SUPPORT)
/*
 * Returns the JIT's hook interface.
 * Callers should include "jithook.h" for event and struct definitions.
 * This function will return the hook interface for the JIT, if it is enabled.
 * It will return NULL if neither is installed.
 * This function is available through the J9VMInternalVMFunctions table.
 */
J9HookInterface**
getJITHookInterface(J9JavaVM* vm)
{
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (vm->jitConfig) {
		return J9_HOOK_INTERFACE(vm->jitConfig->hookInterface);
	}
#endif

	return NULL;
}
#endif /* INTERP_NATIVE_SUPPORT */
