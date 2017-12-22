/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#include "j9port.h"
#include "j9.h"
#include "omr.h"
#include "j9protos.h"
#include "j9comp.h"
#include "j9consts.h"
#include "gc_hooktests.h"
#include "mmhook.h"

static void allocationThresholdHandler(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void vmShutdownHandler(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void commonAllocHandler(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static UDATA numAllocs;

jint JNICALL
JVM_OnLoad( JavaVM *jvm, char* options, void *reserved )
{
	J9JavaVM *javaVM = (J9JavaVM*)jvm;
	J9HookInterface **vmHooks = javaVM->internalVMFunctions->getVMHookInterface(javaVM);
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	if (0 != (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_OBJECT_ALLOCATE_WITHIN_THRESHOLD, allocationThresholdHandler, OMR_GET_CALLSITE(),NULL)) {
		j9tty_printf(PORTLIB,"Unable to register for hook\n");
		return JNI_ERR;
	} else {
		j9tty_printf(PORTLIB,"Registered for J9HOOK_MM_ALLOCATION_THRESHOLD hook\n");
	}

	if (0 != (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_OBJECT_ALLOCATE, commonAllocHandler, OMR_GET_CALLSITE(), NULL)) {
		j9tty_printf(PORTLIB,"Unable to register for hook\n");
		return JNI_ERR;
	}

	/*if (0 != (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_OBJECT_ALLOCATE_INSTRUMENTABLE, commonAllocHandler, OMR_GET_CALLSITE(), NULL)) {
		j9tty_printf(PORTLIB,"Unable to register for hook\n");
		return JNI_ERR;
	}*/

	if (0 != (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SHUTTING_DOWN, vmShutdownHandler, OMR_GET_CALLSITE(), NULL)) {
		j9tty_printf(PORTLIB,"Unable to register shutdown listener.\n");
		return JNI_ERR;
	}

	numAllocs = 0;

	javaVM->memoryManagerFunctions->j9gc_set_allocation_threshold(javaVM->mainThread, 1024*64, 128*1024);

	return JNI_OK;
}

static void
commonAllocHandler(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMObjectAllocateEvent * acEvent = (J9VMObjectAllocateEvent*)eventData;
	J9VMThread * vmThread = acEvent->currentThread;
	J9JavaVM * vm = vmThread->javaVM;
	static UDATA state = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);
	numAllocs++;

	if ( numAllocs >= 100 && state == 0) {
		state = 1;
	}
	if ( numAllocs >= 200 && state == 2) {
		state = 3;
	}
	if ( numAllocs >= 300 && state == 4) {
		state = 5;
	}
	if ( numAllocs >= 400 && state == 6) {
		state = 7;
	}

	switch(state) {
	case 1:
		j9tty_printf(PORTLIB,"100 allocations, setting threshold lower\n");
		vm->memoryManagerFunctions->j9gc_set_allocation_threshold(vmThread, 1024*32, 64*1024);
		state = 2;
		break;
	case 3:
		j9tty_printf(PORTLIB,"200 allocations, setting threshold lower\n");
		vm->memoryManagerFunctions->j9gc_set_allocation_threshold(vmThread, 32, 64);
		state = 4;
		break;
	case 5:
		j9tty_printf(PORTLIB,"500 allocations increasing threshold\n");
		vm->memoryManagerFunctions->j9gc_set_allocation_threshold(vmThread, 1024*32, 64*1024);
		state = 6;
		break;
	case 7:
		j9tty_printf(PORTLIB,"Disabling allocation threshold\n");
		vm->memoryManagerFunctions->j9gc_set_allocation_threshold(vmThread, UDATA_MAX, UDATA_MAX);
		state = 8;
		break;
	}
}

static void
allocationThresholdHandler(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
#if 0
	J9VMAllocationThresholdEvent* atEvent = (J9VMAllocationThresholdEvent*)eventData;
	J9VMThread *vmThread = atEvent->currentThread;
	J9JavaVM *vm = vmThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/*j9tty_printf(PORTLIB,"Object allocated of size %lu (low:%lu high:%lu)\n",atEvent->size,atEvent->lowThreshold,atEvent->highThreshold);*/

	/*j9tty_printf(PORTLIB,"Number of allocates: %lu\n",numAllocs);*/
#endif
}

static void
vmShutdownHandler(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMShutdownEvent* event = eventData;
	J9VMThread *vmThread = event->vmThread;
	J9HookInterface **vmHooks = vmThread->javaVM->internalVMFunctions->getVMHookInterface(vmThread->javaVM);
	PORT_ACCESS_FROM_VMC(vmThread);

	(*vmHooks)->J9HookUnregister(vmHooks,J9HOOK_VM_OBJECT_ALLOCATE_WITHIN_THRESHOLD,allocationThresholdHandler,NULL);
	(*vmHooks)->J9HookUnregister(vmHooks,J9HOOK_VM_OBJECT_ALLOCATE,commonAllocHandler,NULL);
	/*(*vmHooks)->J9HookUnregister(vmHooks,J9HOOK_VM_OBJECT_ALLOCATE_INSTRUMENTABLE,commonAllocHandler,NULL);*/
	j9tty_printf(PORTLIB,"Unregistering hooks\n");
	j9tty_printf(PORTLIB,"Total number of allocates: %lu\n",numAllocs);
}
