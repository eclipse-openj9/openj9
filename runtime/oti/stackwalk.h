/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#ifndef STACKWALK_H
#define STACKWALK_H

/*
 * @ddr_namespace: map_to_type=J9StackWalkConstants
 * @ddr_options: valuesandbuildflags
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "j9comp.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "bcnames.h"

#define UNTAG2(source, type) ((type) (((UDATA) (source)) & ~3))

#define UNTAGGED_METHOD_CP(method) J9_CP_FROM_METHOD(method)

#define UNTAGGED_A0(fp) UNTAG2((fp)->savedA0, UDATA *)

#ifdef J9VM_INTERP_GROWABLE_STACKS
#define CONVERT_TO_RELATIVE_STACK_OFFSET(vmThread, ptr) ((UDATA *) (((U_8 *) (vmThread)->stackObject->end) - (U_8 *) (ptr)))
#else
#define CONVERT_TO_RELATIVE_STACK_OFFSET(vmThread, ptr) ((UDATA *) (ptr))
#endif
#define CONVERT_FROM_RELATIVE_STACK_OFFSET(vmThread, ptr) CONVERT_TO_RELATIVE_STACK_OFFSET(vmThread, ptr)

#ifdef J9VM_INTERP_STACKWALK_TRACING
#define WALK_NAMED_INDIRECT_O_SLOT(slot, ind, tag) swWalkObjectSlot(walkState, (slot), (ind), (tag))
#define WALK_NAMED_INDIRECT_I_SLOT(slot, ind, tag) swWalkIntSlot(walkState, (slot), (ind), (tag))
#else
#define WALK_NAMED_INDIRECT_O_SLOT(slot, ind, tag) walkState->objectSlotWalkFunction(walkState->currentThread, walkState, (slot), REMOTE_ADDR(slot))
#define WALK_NAMED_INDIRECT_I_SLOT(slot, ind, tag)
#endif
#define WALK_INDIRECT_O_SLOT(slot, ind) WALK_NAMED_INDIRECT_O_SLOT((slot), (ind), NULL)
#define WALK_INDIRECT_I_SLOT(slot, ind) WALK_NAMED_INDIRECT_I_SLOT((slot), (ind), NULL)
#define WALK_NAMED_O_SLOT(slot, tag) WALK_NAMED_INDIRECT_O_SLOT((slot), NULL, (tag))
#define WALK_NAMED_I_SLOT(slot, tag) WALK_NAMED_INDIRECT_I_SLOT((slot), NULL, (tag))
#define WALK_O_SLOT(slot) WALK_INDIRECT_O_SLOT((slot), NULL)
#define WALK_I_SLOT(slot) WALK_INDIRECT_I_SLOT((slot), NULL)

#ifdef J9VM_OUT_OF_PROCESS
#define DONT_REDIRECT_SRP
#include "j9dbgext.h"
#define LOCAL_ADDR(remoteAddr) dbgTargetToLocal(remoteAddr)
#define REMOTE_ADDR(localAddr) dbgLocalToTarget(localAddr)
#define READ_BYTE(addr) dbgReadByte(addr)
#define READ_METHOD(remoteMethod) dbgReadMethod(remoteMethod)
#define READ_CP(remoteCP) dbgReadCP(remoteCP)
#define READ_CLASS(remoteClass) dbgReadClass(remoteClass)
#define READ_OBJECT(remoteObject) dbgReadObject(remoteObject)
#define READ_UDATA(addr) dbgReadUDATA(addr)
#else
#define LOCAL_ADDR(remoteAddr) ((void *) (remoteAddr))
#define REMOTE_ADDR(localAddr) ((void *) (localAddr))
#define READ_BYTE(addr) (*(addr))
#define READ_METHOD(remoteMethod) (remoteMethod)
#define READ_CP(remoteCP) (remoteCP)
#define READ_CLASS(remoteClass) (remoteClass)
#define READ_OBJECT(remoteObject) (remoteObject)
#define READ_UDATA(addr) (*(addr))
#endif

#if defined(J9VM_INTERP_STACKWALK_TRACING) && !defined(J9VM_OUT_OF_PROCESS)
#define MARK_SLOT_AS_OBJECT(walkState, slot) swMarkSlotAsObject((walkState), (slot))
#else
#define MARK_SLOT_AS_OBJECT(walkState, slot)
#endif

#define IS_SPECIAL_FRAME_PC(pc) ( (UDATA)(pc) <= J9SF_MAX_SPECIAL_FRAME_TYPE )

#define IS_JNI_CALLIN_FRAME_PC(pc) ( *(U_8*)(pc) == JBimpdep2 )

#ifdef J9VM_INTERP_STACKWALK_TRACING
#define SWALK_PRINT_CLASS_OF_RUNNING_METHOD(walkState) swPrintf((walkState), 4, "\tClass of running method\n")
#else
#define SWALK_PRINT_CLASS_OF_RUNNING_METHOD(walkState)
#endif

#if defined(J9VM_ARCH_S390) && !defined(J9VM_ENV_DATA64)
#define MASK_PC(pc) ((U_8 *) (((UDATA) (pc)) & 0x7FFFFFFF))
#else
#define MASK_PC(pc) ((U_8 *) (pc))
#endif

#define WALK_METHOD_CLASS(walkState) \
	do { \
		if ((walkState)->flags & J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS) { \
			j9object_t classObject; \
			SWALK_PRINT_CLASS_OF_RUNNING_METHOD(walkState); \
			(walkState)->slotType = J9_STACKWALK_SLOT_TYPE_INTERNAL; \
			(walkState)->slotIndex = -1; \
			classObject = J9VM_J9CLASS_TO_HEAPCLASS((walkState)->constantPool->ramClass); \
			WALK_O_SLOT( &classObject ); \
		} \
	} while(0)

#define J9SW_JIT_RETURN_TABLE_SIZE 0x9

#define J9_STACK_FLAGS_JIT_TRANSITION_TO_INTERPRETER_MASK 0x70080000
#define J9_STACK_FLAGS_METHOD_ENTRY 0x08000000
#define J9_STACK_FLAGS_RETURNS_OBJECT 0x00010000
#define J9_STACK_FLAGS_J2_IFRAME 0x00000001
#define J9_STACK_FLAGS_JIT_CALL_IN_TYPE_METHODTYPE 0x02000000
#define J9_STACK_FLAGS_JIT_CALL_IN_FRAME 0x10000000
#define J9_STACK_FLAGS_EXIT_TRACEPOINT_LEVEL3 0x00040000
#define J9_STACK_FLAGS_USE_SPECIFIED_CLASS_LOADER 0x00040000
#define J9_STACK_FLAGS_JIT_NATIVE_TRANSITION 0x40000000
#define J9_STACK_INVISIBLE_FRAME 0x00000002
#define J9_STACK_FLAGS_UNUSED_0x80000000 0x80000000
#define J9_STACK_FLAGS_JIT_CALL_IN_TYPE_J2_I 0x00000000
#define J9_STACK_FLAGS_JNI_REFS_REDIRECTED 0x00010000
#define J9_STACK_FLAGS_ARGS_ALIGNED 0x00000002
#define J9_STACK_FLAGS_JIT_JNI_CALL_OUT_FRAME 0x20000000
#define J9_STACK_FLAGS_RELEASE_VMACCESS 0x00020000
#define J9_STACK_REPORT_FRAME_POP 0x00000001
#define J9_STACK_FLAGS_JIT_ARGS_ALIGNED 0x04000000
#define J9_JNI_PUSHED_REFERENCE_COUNT_MASK 0x000000FF
#define J9_STACK_FLAGS_CALL_OUT_FRAME_ALLOCATED 0x00020000

#define J9_STACK_FLAGS_JIT_RESOLVE_FRAME 0x00080000
#define J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK 0x01F00000
#define J9_STACK_FLAGS_JIT_GENERIC_RESOLVE 0x00000000
#define J9_STACK_FLAGS_JIT_STATIC_METHOD_RESOLVE 0x00100000
#define J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE 0x00200000
#define J9_STACK_FLAGS_JIT_DATA_RESOLVE 0x00300000
#define J9_STACK_FLAGS_JIT_LOOKUP_RESOLVE 0x00400000
#define J9_STACK_FLAGS_JIT_VIRTUAL_METHOD_RESOLVE 0x00500000
#define J9_STACK_FLAGS_JIT_INTERFACE_METHOD_RESOLVE 0x00600000
#define J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME 0x00700000
#define J9_STACK_FLAGS_JIT_RUNTIME_HELPER_RESOLVE 0x00800000
#define J9_STACK_FLAGS_JIT_RECOMPILATION_RESOLVE 0x00900000
#define J9_STACK_FLAGS_JIT_MONITOR_ENTER_RESOLVE 0x00A00000
#define J9_STACK_FLAGS_JIT_ALLOCATION_RESOLVE 0x00B00000
#define J9_STACK_FLAGS_JIT_BEFORE_ANEWARRAY_RESOLVE 0x00C00000
#define J9_STACK_FLAGS_JIT_BEFORE_MULTIANEWARRAY_RESOLVE 0x00D00000
#define J9_STACK_FLAGS_JIT_INDUCE_OSR_RESOLVE 0x00E00000
#define J9_STACK_FLAGS_JIT_METHOD_MONITOR_ENTER_RESOLVE 0x00F00000
#define J9_STACK_FLAGS_JIT_FAILED_METHOD_MONITOR_ENTER_RESOLVE 0x01000000
#define J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE 0x01100000

/* Platform-specific defines */

#if defined(J9VM_ARCH_X86)

/* X86 common */

/* @ddr_namespace: map_to_type=J9StackWalkFlags */

#undef  J9SW_JIT_FLOATS_PASSED_AS_DOUBLES
#undef  J9SW_JIT_HELPERS_PASS_PARAMETERS_ON_STACK
#undef  J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
#define J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH

/* @ddr_namespace: map_to_type=J9StackWalkConstants */

#define J9SW_JIT_STACK_SLOTS_USED_BY_CALL 0x1

#if defined(J9VM_ENV_DATA64)

/* X86-64 */

/* @ddr_namespace: map_to_type=J9StackWalkFlags */

#define J9SW_NEEDS_JIT_2_INTERP_THUNKS
#define J9SW_PARAMETERS_IN_REGISTERS

/* @ddr_namespace: map_to_type=J9StackWalkConstants */

#define J9SW_ARGUMENT_REGISTER_COUNT 0x4
#define J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT 0x8
#define JIT_RESOLVE_PARM(parmNumber) (walkState->walkedEntryLocalStorage->jitGlobalStorageBase[jitArgumentRegisterNumbers[(parmNumber) - 1]])
#define J9SW_POTENTIAL_SAVED_REGISTERS 0x10
#define J9SW_REGISTER_MAP_MASK 0xFFFF
#define J9SW_JIT_FIRST_RESOLVE_PARM_REGISTER 0x3
#define J9SW_JIT_CALLEE_PRESERVED_SIZE 8
#undef  J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#undef  J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#define J9SW_LOWEST_MEMORY_PRESERVED_REGISTER jit_rbx

#else /* J9VM_ENV_DATA64 */

/* X86-32 */

/* @ddr_namespace: map_to_type=J9StackWalkFlags */

#undef  J9SW_JIT_FLOATS_PASSED_AS_DOUBLES
#define J9SW_JIT_HELPERS_PASS_PARAMETERS_ON_STACK
#define J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
#undef  J9SW_NEEDS_JIT_2_INTERP_THUNKS
#undef  J9SW_PARAMETERS_IN_REGISTERS

/* @ddr_namespace: map_to_type=J9StackWalkConstants */

#undef  J9SW_ARGUMENT_REGISTER_COUNT
#undef  J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT
#define JIT_RESOLVE_PARM(parmNumber) (walkState->bp[parmNumber])
#define J9SW_POTENTIAL_SAVED_REGISTERS 0x7
#define J9SW_REGISTER_MAP_MASK 0x7F
#undef  J9SW_JIT_FIRST_RESOLVE_PARM_REGISTER
#define J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER 0x0
#define J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER 0x0
#define J9SW_JIT_CALLEE_PRESERVED_SIZE 3
#define J9SW_LOWEST_MEMORY_PRESERVED_REGISTER jit_esi

#endif /* J9VM_ENV_DATA64 */

#elif defined(J9VM_ARCH_POWER)

/* PPC common */

/* @ddr_namespace: map_to_type=J9StackWalkFlags */

#define J9SW_JIT_FLOATS_PASSED_AS_DOUBLES
#undef  J9SW_JIT_HELPERS_PASS_PARAMETERS_ON_STACK
#undef  J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
#define J9SW_NEEDS_JIT_2_INTERP_THUNKS
#define J9SW_PARAMETERS_IN_REGISTERS
#undef  J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH

/* @ddr_namespace: map_to_type=J9StackWalkConstants */

#define J9SW_ARGUMENT_REGISTER_COUNT 0x8
#define J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT 0x8
#define JIT_RESOLVE_PARM(parmNumber) (walkState->walkedEntryLocalStorage->jitGlobalStorageBase[jitArgumentRegisterNumbers[(parmNumber) - 1]])
#define J9SW_JIT_STACK_SLOTS_USED_BY_CALL 0x0
#define J9SW_POTENTIAL_SAVED_REGISTERS 0x20
#define J9SW_REGISTER_MAP_MASK 0xFFFFFFFF
#define J9SW_JIT_FIRST_RESOLVE_PARM_REGISTER 0x3
#undef  J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#undef  J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#define J9SW_LOWEST_MEMORY_PRESERVED_REGISTER jit_r31

#if defined(J9VM_ENV_DATA64)

/* PPC-64 */

#define J9SW_JIT_CALLEE_PRESERVED_SIZE 16

#else /* J9VM_ENV_DATA64 */

/* PPC-32 */

#define J9SW_JIT_CALLEE_PRESERVED_SIZE 17

#endif /* J9VM_ENV_DATA64 */

#elif defined(J9VM_ARCH_S390)

/* 390 common */

/* @ddr_namespace: map_to_type=J9StackWalkFlags */

#undef  J9SW_JIT_FLOATS_PASSED_AS_DOUBLES
#undef  J9SW_JIT_HELPERS_PASS_PARAMETERS_ON_STACK
#undef  J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
#define J9SW_NEEDS_JIT_2_INTERP_THUNKS
#define J9SW_PARAMETERS_IN_REGISTERS
#define J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH

/* @ddr_namespace: map_to_type=J9StackWalkConstants */

#define J9SW_ARGUMENT_REGISTER_COUNT 0x3
#define J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT 0x4
#define JIT_RESOLVE_PARM(parmNumber) (walkState->walkedEntryLocalStorage->jitGlobalStorageBase[jitArgumentRegisterNumbers[(parmNumber) - 1]])
#define J9SW_JIT_STACK_SLOTS_USED_BY_CALL 0x0
#undef  J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#undef  J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#define J9SW_JIT_FIRST_RESOLVE_PARM_REGISTER 0x1

#if defined(J9VM_ENV_DATA64)

/* 390-64 */

#define J9SW_POTENTIAL_SAVED_REGISTERS 0x10
#define J9SW_REGISTER_MAP_MASK 0xFFFF
#define J9SW_JIT_CALLEE_PRESERVED_SIZE 7
#define J9SW_LOWEST_MEMORY_PRESERVED_REGISTER jit_r12

#else /* J9VM_ENV_DATA64 */

/* 390-32 */

#define J9SW_POTENTIAL_SAVED_REGISTERS 0x20
#define J9SW_REGISTER_MAP_MASK 0xFFFFFFFF
#define J9SW_JIT_CALLEE_PRESERVED_SIZE 14
#define J9SW_LOWEST_MEMORY_PRESERVED_REGISTER jit_r28

#endif /* J9VM_ENV_DATA64 */

#elif defined(J9VM_ARCH_ARM)

/* ARM common */

/* @ddr_namespace: map_to_type=J9StackWalkFlags */

#undef  J9SW_JIT_FLOATS_PASSED_AS_DOUBLES
#undef  J9SW_JIT_HELPERS_PASS_PARAMETERS_ON_STACK
#undef  J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
#define J9SW_NEEDS_JIT_2_INTERP_THUNKS
#define J9SW_PARAMETERS_IN_REGISTERS
#define J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH

/* @ddr_namespace: map_to_type=J9StackWalkConstants */

#define J9SW_ARGUMENT_REGISTER_COUNT 0x4
#undef  J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT
#define JIT_RESOLVE_PARM(parmNumber) (walkState->walkedEntryLocalStorage->jitGlobalStorageBase[jitArgumentRegisterNumbers[(parmNumber) - 1]])
#define J9SW_JIT_STACK_SLOTS_USED_BY_CALL 0x0
#define J9SW_POTENTIAL_SAVED_REGISTERS 0xC
#define J9SW_REGISTER_MAP_MASK 0xFFFFFFFF
#define J9SW_JIT_FIRST_RESOLVE_PARM_REGISTER 0x0
#undef  J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#undef  J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#define J9SW_LOWEST_MEMORY_PRESERVED_REGISTER jit_r10
#define J9SW_JIT_CALLEE_PRESERVED_SIZE 3

#elif defined(J9VM_ARCH_AARCH64)

/* AArch64 */

/* @ddr_namespace: map_to_type=J9StackWalkFlags */

#define J9SW_JIT_FLOATS_PASSED_AS_DOUBLES
#undef  J9SW_JIT_HELPERS_PASS_PARAMETERS_ON_STACK
#undef  J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
#define J9SW_NEEDS_JIT_2_INTERP_THUNKS
#define J9SW_PARAMETERS_IN_REGISTERS
#define J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH

/* @ddr_namespace: map_to_type=J9StackWalkConstants */

#define J9SW_ARGUMENT_REGISTER_COUNT 0x8
#define J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT 0x8
#define JIT_RESOLVE_PARM(parmNumber) (walkState->walkedEntryLocalStorage->jitGlobalStorageBase[jitArgumentRegisterNumbers[(parmNumber) - 1]])
#define J9SW_JIT_STACK_SLOTS_USED_BY_CALL 0x0
#define J9SW_POTENTIAL_SAVED_REGISTERS 0x20
#define J9SW_REGISTER_MAP_MASK 0xFFFFFFFF
#define J9SW_JIT_FIRST_RESOLVE_PARM_REGISTER 0x0
#undef  J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#undef  J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER
#define J9SW_LOWEST_MEMORY_PRESERVED_REGISTER jit_r18
#define J9SW_JIT_CALLEE_PRESERVED_SIZE 11

#else
#error Unsupported platform
#endif

#if defined(J9SW_ARGUMENT_REGISTER_COUNT)
extern U_8 jitArgumentRegisterNumbers[J9SW_ARGUMENT_REGISTER_COUNT];
#endif /* J9SW_ARGUMENT_REGISTER_COUNT */
#if defined(J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT)
extern U_8 jitFloatArgumentRegisterNumbers[J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT];
#endif /* J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT */

#ifdef __cplusplus
}
#endif

#endif /* STACKWALK_H */
