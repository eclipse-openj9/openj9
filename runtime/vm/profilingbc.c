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
#include "j9protos.h"
#include "j9consts.h"
#include "vmhook_internal.h"
#include "bcnames.h"
#include "ut_j9vm.h"
#include "mmhook.h"
#include "vm_internal.h"

#if defined(J9VM_INTERP_PROFILING_BYTECODES) /* File level ifdef */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
static void flushForClassesUnload (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

static void cleanupBytecodeProfilingData (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);

/**
 * Flush any pending data in the current thread's profiling buffer by reporting
 * the J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL event to any
 * listeners and resetting the buffer.
 *
 * If the thread doesn't have a buffer, one will be created for it.
 * 
 */
void
flushBytecodeProfilingData(J9VMThread* vmThread)
{
	UDATA profilingBufferSize = vmThread->javaVM->jitConfig->iprofilerBufferSize;

	Trc_VM_flushBytecodeProfilingData_Entry(vmThread, vmThread->profilingBufferCursor, vmThread->profilingBufferEnd);

	if (vmThread->profilingBufferEnd) {
		U_8* bufferStart = vmThread->profilingBufferEnd - profilingBufferSize ;

		ALWAYS_TRIGGER_J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL(
			vmThread->javaVM->hookInterface,
			vmThread,
			bufferStart,
			vmThread->profilingBufferCursor - bufferStart);
	} else {
		PORT_ACCESS_FROM_VMC(vmThread);
		U_8* newBuffer = j9mem_allocate_memory(profilingBufferSize , OMRMEM_CATEGORY_VM);

		Trc_VM_flushBytecodeProfilingData_newBuffer(vmThread, newBuffer);

		if (newBuffer) {
			vmThread->profilingBufferCursor = newBuffer;
			vmThread->profilingBufferEnd = newBuffer + profilingBufferSize ;
		}
	}

	Trc_VM_flushBytecodeProfilingData_Exit(vmThread);
}

/**
 * Free any profiling buffer associated with this thread. If the buffer contains
 * unflushed data, report the J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL 
 * event to any listeners first.
 */
static void
cleanupBytecodeProfilingData(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMThreadDestroyEvent* event = eventData;
	J9VMThread* vmThread = event->vmThread;

	Trc_VM_cleanupBytecodeProfilingData_Entry();

	if (vmThread->profilingBufferEnd) {
		PORT_ACCESS_FROM_VMC(vmThread);
		UDATA profilingBufferSize = (UDATA)userData;
		U_8* bufferStart = vmThread->profilingBufferEnd - profilingBufferSize;

		vmThread->profilingBufferEnd = vmThread->profilingBufferCursor = NULL;

		j9mem_free_memory(bufferStart);
	}

	Trc_VM_cleanupBytecodeProfilingData_Exit();
}

/**
 * This function is called by hookRegistrationEvent() whenever the state of
 * the J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL hook has
 * changed.
 *
 */
void
profilingBytecodeBufferFullHookRegistered(J9JavaVM* vm)
{
	J9HookInterface** vmHooks = J9_HOOK_INTERFACE(vm->hookInterface);
	void *iprofilerBufferSize = (void *)vm->jitConfig->iprofilerBufferSize;
	Trc_VM_profilingBytecodeBufferFullHookRegistered_Entry();

	/* make sure that we clean up any profiling data on thread shutdown */
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_DESTROY, cleanupBytecodeProfilingData, OMR_GET_CALLSITE(), iprofilerBufferSize)) {
		Trc_VM_profilingBytecodeBufferFullHookRegistered_ThreadHookFailed();
		Assert_VM_unreachable();
	}

#ifdef J9VM_GC_DYNAMIC_CLASS_UNLOADING
	/* make sure that we flush any profiling data when a class is unloaded */
	if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, flushForClassesUnload, OMR_GET_CALLSITE(), iprofilerBufferSize)
	|| (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_ANON_CLASSES_UNLOAD, flushForClassesUnload, OMR_GET_CALLSITE(), iprofilerBufferSize)
	) {
		Trc_VM_profilingBytecodeBufferFullHookRegistered_ClassUnloadHookFailed();
		Assert_VM_unreachable();
	}
#endif

	Trc_VM_profilingBytecodeBufferFullHookRegistered_Exit();
}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
/*
 * Classes are being unloaded.
 * As a precaution, flush any pending profile buffers in ALL threads, since they may contain references to 
 * classes being unloaded. This could cause a crash if a listener attempted to process that data.
 * Caller must have exclusive VM access.
 */
static void
flushForClassesUnload(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMClassesUnloadEvent* event = eventData;
	J9VMThread *vmThread = event->currentThread;
	J9VMThread* cursor = vmThread;
	UDATA profilingBufferSize = (UDATA)userData;

	Trc_VM_flushForClassesUnload_Entry(vmThread);

	do {
		if (cursor->profilingBufferEnd) {
			U_8* bufferStart = cursor->profilingBufferEnd - profilingBufferSize ;
			cursor->profilingBufferCursor = bufferStart;
		}
		cursor = cursor->linkNext;
	} while (cursor != vmThread);

	Trc_VM_flushForClassesUnload_Exit(vmThread);
}
#endif

#endif /* J9VM_INTERP_PROFILING_BYTECODES */  /* File level ifdef */
