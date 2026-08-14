/*******************************************************************************
 * Copyright IBM Corp. and others 2026
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

#include "GCJFR.hpp"

#if defined(J9VM_OPT_JFR)

#include "j9.h"
#include "j9protos.h"
#include "mmprivatehook.h"
#include "mmomrhook.h"
#include "modronapi.hpp"

#define BEFORE_GC 0
#define AFTER_GC 1

/**
 * Register GC-related JFR hooks.
 *
 * This function registers all garbage collection related hooks for JFR event recording:
 * - jfrGCCycleStartHook (corresponding to public OMR GC cycle start trigger)
 * - jfrPublicGCEndHook (corresponding to public OMR GC cycle end trigger)
 * - jfrPrivateGCEndHook (corresponding to private OMR GC cycle end trigger)
 *
 * @param vm[in] The Java VM
 * @return 0 on success, non-zero on failure
 */
jint
jfrRegisterGCHooks(J9JavaVM *vm)
{
	J9HookInterface** gcOmrHooks = vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM);
	J9HookInterface** gcPrivateHooks = vm->memoryManagerFunctions->j9gc_get_private_hook_interface(vm);

	if ((*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, jfrGCCycleStartHook, OMR_GET_CALLSITE(), NULL)) {
		return -1;
	}
	if ((*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_END, jfrPublicGCEndHook, OMR_GET_CALLSITE(), NULL)) {
		return -1;
	}
	/*
	 * Effectively allowing a non-GC callback to register to a private GC hook, with a contract that is very lightweight.
	 * We want JFR time reporting to be as realistic as possible (including times of all potentially expensive hooks
	 * registered to the public variant of cycle end hook), similar to Verbose GC.
	 */
	if ((*gcPrivateHooks)->J9HookRegisterWithCallSite(gcPrivateHooks, J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END, jfrPrivateGCEndHook, OMR_GET_CALLSITE(), NULL)) {
		return -1;
	}

	return 0;
}

/**
 * JFR GC Hook corresponding to public OMR cycle start trigger.
 *
 * This function emits all garbage collection related JFR events on the public OMR cycle start trigger.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
void
jfrGCCycleStartHook(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_GCCycleStartEvent *event = (MM_GCCycleStartEvent *)eventData;
	OMR_VMThread *omrVMThread = (OMR_VMThread *)event->omrVMThread;
	J9VMThread *currentThread = (J9VMThread *)omrVMThread->_language_vmthread;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = javaVM->internalVMFunctions;

	/* Emit heap summary with gcWhenID = BeforeGC */
	vmFuncs->jfrGCHeapSummary(omrVMThread, BEFORE_GC);
}

/**
 * JFR GC Hook corresponding to public OMR cycle end trigger.
 *
 * This function emits all garbage collection related JFR events on the public OMR cycle end trigger.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
void
jfrPublicGCEndHook(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_GCCycleEndEvent *event = (MM_GCCycleEndEvent *)eventData;
	OMR_VMThread *omrVMThread = (OMR_VMThread *)event->omrVMThread;
	J9VMThread *currentThread = (J9VMThread *)omrVMThread->_language_vmthread;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = javaVM->internalVMFunctions;
	/* Check cycle type and send to the correct one */
	if ((OMR_GC_CYCLE_TYPE_SCAVENGE != event->cycleType) && (OMR_GC_CYCLE_TYPE_VLHGC_PARTIAL_GARBAGE_COLLECT != event->cycleType)) {
		vmFuncs->jfrOldGarbageCollection(omrVMThread);
	} else {
		vmFuncs->jfrYoungGarbageCollection(omrVMThread);
	}
}

/**
 * JFR GC Hook corresponding to private OMR cycle end trigger.
 *
 * This function emits all garbage collection related JFR events on the private OMR cycle end trigger.
 *
 * @param hook[in] the VM hook interface
 * @param eventNum[in] the event number
 * @param eventData[in] the event data
 * @param userData[in] the registered user data
 */
void
jfrPrivateGCEndHook(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
{
	MM_GCPostCycleEndEvent *event = (MM_GCPostCycleEndEvent *)eventData;
	OMR_VMThread *omrVMThread = (OMR_VMThread *)event->currentThread;
	J9VMThread *currentThread = (J9VMThread *)omrVMThread->_language_vmthread;
	J9JavaVM *javaVM = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = javaVM->internalVMFunctions;

	vmFuncs->jfrGarbageCollection(omrVMThread);
	/* Emit heap summary with gcWhenID = AfterGC */
	vmFuncs->jfrGCHeapSummary(omrVMThread, AFTER_GC);

}

/**
 * Deregister GC-related JFR hooks.
 *
 * This function unregisters all garbage collection related hooks that were
 * previously registered for JFR event recording.
 *
 * @param vm[in] The Java VM
 */
void
jfrDeregisterGCHooks(J9JavaVM *vm)
{
	J9HookInterface** gcOmrHooks = vm->memoryManagerFunctions->j9gc_get_omr_hook_interface(vm->omrVM);
	J9HookInterface** gcPrivateHooks = vm->memoryManagerFunctions->j9gc_get_private_hook_interface(vm);

	(*gcOmrHooks)->J9HookUnregister(gcOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_START, jfrGCCycleStartHook, NULL);
	(*gcOmrHooks)->J9HookUnregister(gcOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_END, jfrPublicGCEndHook, NULL);
	(*gcPrivateHooks)->J9HookUnregister(gcPrivateHooks, J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END, jfrPrivateGCEndHook, NULL);
}

#endif /* J9VM_OPT_JFR */
