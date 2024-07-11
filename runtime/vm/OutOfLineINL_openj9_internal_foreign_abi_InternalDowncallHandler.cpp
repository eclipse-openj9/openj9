/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include "VMHelpers.hpp"
#include "BytecodeAction.hpp"
#include "UnsafeAPI.hpp"
#include "j9vmnls.h"
#include "OutOfLineINL.hpp"
#include "LayoutFFITypeHelpers.hpp"
#include "AtomicSupport.hpp"

extern "C" {

#if JAVA_SPEC_VERSION >= 16
/* openj9.internal.foreign.abi.InternalDowncallHandler: private static synchronized native void resolveRequiredFields(); */
VM_BytecodeAction
OutOfLineINL_openj9_internal_foreign_abi_InternalDowncallHandler_resolveRequiredFields(J9VMThread *currentThread, J9Method *method)
{
	VM_BytecodeAction rc = EXECUTE_BYTECODE;
	J9JavaVM *vm = currentThread->javaVM;
	J9ConstantPool *jclConstantPool = (J9ConstantPool *)vm->jclConstantPool;
	static const U_16 cpIndex[] = {
			J9VMCONSTANTPOOL_OPENJ9INTERNALFOREIGNABIINTERNALDOWNCALLHANDLER_ARGTYPESADDR
#if JAVA_SPEC_VERSION >= 22
			, J9VMCONSTANTPOOL_OPENJ9INTERNALFOREIGNABIINTERNALDOWNCALLHANDLER_BOUNDMH
#endif /* JAVA_SPEC_VERSION >= 22 */
			, J9VMCONSTANTPOOL_OPENJ9INTERNALFOREIGNABIINTERNALDOWNCALLHANDLER_CIFNATIVETHUNKADDR
#if JAVA_SPEC_VERSION >= 22
			, J9VMCONSTANTPOOL_OPENJ9INTERNALFOREIGNABIINTERNALDOWNCALLHANDLER_INVOKECACHE
			, J9VMCONSTANTPOOL_JAVALANGINVOKENATIVEMETHODHANDLE_INVOKECACHE
			, J9VMCONSTANTPOOL_JAVALANGINVOKENATIVEMETHODHANDLE_NEP
#endif /* JAVA_SPEC_VERSION >= 22 */
	};
	const size_t cpEntryNum = sizeof(cpIndex) / sizeof(cpIndex[0]);

	VM_OutOfLineINL_Helpers::buildInternalNativeStackFrame(currentThread, method);
	for (size_t i = 0; i < cpEntryNum; i++) {
		J9RAMFieldRef *cpFieldRef = ((J9RAMFieldRef*)jclConstantPool) + cpIndex[i];
		UDATA const flags = cpFieldRef->flags;
		UDATA const valueOffset = cpFieldRef->valueOffset;

		if (!VM_VMHelpers::instanceFieldRefIsResolved(flags, valueOffset)) {
			resolveInstanceFieldRef(currentThread, NULL, jclConstantPool, cpIndex[i], J9_RESOLVE_FLAG_NO_THROW_ON_FAIL | J9_RESOLVE_FLAG_JCL_CONSTANT_POOL, NULL);
			if (VM_VMHelpers::exceptionPending(currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
		}
	}
	VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);

done:
	VM_OutOfLineINL_Helpers::returnVoid(currentThread, 0);
	return rc;
}

/**
 * openj9.internal.foreign.abi.InternalDowncallHandler: private native void initCifNativeThunkData(String[] argLayouts, String retLayout, boolean newArgTypes, int varArgIndex);
 *
 * @brief Prepare the prep_cif for the native function specified by the arguments/return layouts
 * @param argLayouts[in] A c string array describing the argument layouts
 * @param retLayout[in] A c string describing the return layouts
 * @param newArgTypes[in] a flag determining whether to create a new ffi_type array for arguments
 * @param varArgIndex[in] the variadic argument index if exists in the argument list
 * @return void
 */
VM_BytecodeAction
OutOfLineINL_openj9_internal_foreign_abi_InternalDowncallHandler_initCifNativeThunkData(J9VMThread *currentThread, J9Method *method)
{
	VM_BytecodeAction rc = EXECUTE_BYTECODE;
	J9JavaVM *vm = currentThread->javaVM;
	LayoutFFITypeHelpers ffiTypeHelpers(currentThread);
	ffi_status status = FFI_OK;
	ffi_cif *cif = NULL;
	ffi_type *returnType = NULL;
	ffi_type **argTypes = NULL;
	J9CifArgumentTypes *cifArgTypesNode = NULL;

	I_32 varArgIndex = *(I_32*)currentThread->sp;
	bool newArgTypes = (bool)*(U_32*)(currentThread->sp + 1);
	j9object_t retLayoutStrObject = J9_JNI_UNWRAP_REFERENCE(currentThread->sp + 2);
	j9object_t argLayoutStrsObject = J9_JNI_UNWRAP_REFERENCE(currentThread->sp + 3);
	j9object_t nativeInvoker = J9_JNI_UNWRAP_REFERENCE(currentThread->sp + 4);
	U_32 argTypesCount = J9INDEXABLEOBJECT_SIZE(currentThread, argLayoutStrsObject);
	UDATA returnLayoutSize = 0;

	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Set up the ffi_type of the return layout in the case of primitive or struct */
	returnLayoutSize = ffiTypeHelpers.getLayoutFFIType(&returnType, retLayoutStrObject);
	if (returnLayoutSize == UDATA_MAX) {
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		goto done;
	/* Only intended for struct as the primitive's ffi_type is non-null. */
	} else if ((NULL == returnType)
				|| (((FFI_TYPE_STRUCT == returnType->type)
#if defined(J9ZOS390) && defined(J9VM_ENV_DATA64)
					|| (FFI_TYPE_STRUCT_FF == returnType->type)
					|| (FFI_TYPE_STRUCT_DD == returnType->type)
#endif /* defined(J9ZOS390) && defined(J9VM_ENV_DATA64) */
				) && (NULL == returnType->elements)))
	{
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		setNativeOutOfMemoryError(currentThread, 0, 0);
		goto done;
	}

	if (!newArgTypes) {
		argTypes = (ffi_type **)(UDATA)J9VMOPENJ9INTERNALFOREIGNABIINTERNALDOWNCALLHANDLER_ARGTYPESADDR(currentThread, nativeInvoker);
	} else {
		argTypes = (ffi_type **)j9mem_allocate_memory(sizeof(ffi_type *) * (argTypesCount + 1), J9MEM_CATEGORY_VM_FFI);
		if (NULL == argTypes) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto freeAllMemoryThenExit;
		}
		argTypes[argTypesCount] = NULL;

		for (U_32 argIndex = 0; argIndex < argTypesCount; argIndex++) {
			j9object_t argLayoutStrObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, argLayoutStrsObject, argIndex);
			/* Set up the ffi_type of the argument layout in the case of primitive or struct */
			UDATA argLayoutSize = ffiTypeHelpers.getLayoutFFIType(&argTypes[argIndex], argLayoutStrObject);
			if (argLayoutSize == UDATA_MAX) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
				goto freeAllMemoryThenExit;
			/* Only intended for struct as the primitive's ffi_type is non-null */
			} else if ((NULL == argTypes[argIndex])
						|| (((FFI_TYPE_STRUCT == argTypes[argIndex]->type)
#if defined(J9ZOS390) && defined(J9VM_ENV_DATA64)
							|| (FFI_TYPE_STRUCT_FF == argTypes[argIndex]->type)
							|| (FFI_TYPE_STRUCT_DD == argTypes[argIndex]->type)
#endif /* defined(J9ZOS390) && defined(J9VM_ENV_DATA64) */
						) && (NULL == argTypes[argIndex]->elements))
			) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeAllMemoryThenExit;
			}
		}
	}

	if (NULL == vm->cifNativeCalloutDataCache) {
		vm->cifNativeCalloutDataCache = pool_new(sizeof(ffi_cif), 0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_VM_FFI, POOL_FOR_PORT(PORTLIB));
		if (NULL == vm->cifNativeCalloutDataCache) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto freeAllMemoryThenExit;
		}
	}

	omrthread_monitor_enter(vm->cifNativeCalloutDataCacheMutex);
	cif = (ffi_cif *)pool_newElement(vm->cifNativeCalloutDataCache);
	omrthread_monitor_exit(vm->cifNativeCalloutDataCacheMutex);
	if (NULL == cif) {
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		setNativeOutOfMemoryError(currentThread, 0, 0);
		goto freeAllMemoryThenExit;
	}

	/* The variadic argument index is -1 by default if it doesn't exist in the argument list.
	 * Note: it is literally equal to the count of the fixed arguments before variadic arguments.
	 */
	if (varArgIndex < 0) {
		status = ffi_prep_cif(cif, FFI_DEFAULT_ABI, argTypesCount, returnType, &(argTypes[0]));
	} else {
		status = ffi_prep_cif_var(cif, FFI_DEFAULT_ABI, varArgIndex, argTypesCount, returnType, &(argTypes[0]));
	}
	if (FFI_OK != status) {
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		goto freeAllMemoryThenExit;
	}

	if (newArgTypes) {
		if (NULL == vm->cifArgumentTypesCache) {
			vm->cifArgumentTypesCache = pool_new(sizeof(J9CifArgumentTypes), 0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_VM_FFI, POOL_FOR_PORT(PORTLIB));
			if (NULL == vm->cifArgumentTypesCache) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeAllMemoryThenExit;
			}
		}

		omrthread_monitor_enter(vm->cifArgumentTypesCacheMutex);
		cifArgTypesNode = (J9CifArgumentTypes *)pool_newElement(vm->cifArgumentTypesCache);
		omrthread_monitor_exit(vm->cifArgumentTypesCacheMutex);
		if (NULL == cifArgTypesNode) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto freeAllMemoryThenExit;
		}
		cifArgTypesNode->argumentTypes = (void **)argTypes;

		VM_AtomicSupport::writeBarrier();
		J9VMOPENJ9INTERNALFOREIGNABIINTERNALDOWNCALLHANDLER_SET_ARGTYPESADDR(currentThread, nativeInvoker, (intptr_t)argTypes);
	}

	VM_AtomicSupport::writeBarrier();
	J9VMOPENJ9INTERNALFOREIGNABIINTERNALDOWNCALLHANDLER_SET_CIFNATIVETHUNKADDR(currentThread, nativeInvoker, (intptr_t)cif);

done:
	VM_OutOfLineINL_Helpers::returnVoid(currentThread, 5);
	return rc;

freeAllMemoryThenExit:
	if (newArgTypes && (NULL != argTypes)) {
		for (U_32 argIndex = 0; argTypes[argIndex] != NULL; argIndex++) {
			ffiTypeHelpers.freeStructFFIType(argTypes[argIndex]);
		}
		j9mem_free_memory(argTypes);
		argTypes = NULL;
	}
	ffiTypeHelpers.freeStructFFIType(returnType);
	goto done;
}

#if JAVA_SPEC_VERSION >= 22
/* openj9.internal.foreign.abi.InternalDowncallHandler: private static native boolean isFfiProtoEnabled(); */
VM_BytecodeAction
OutOfLineINL_openj9_internal_foreign_abi_InternalDowncallHandler_isFfiProtoEnabled(J9VMThread *currentThread, J9Method *method)
{
	VM_BytecodeAction rc = EXECUTE_BYTECODE;
	J9JavaVM *vm = currentThread->javaVM;
	jboolean isFfiProtoOn = J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_FFI_PROTO) ? JNI_TRUE : JNI_FALSE;

	VM_OutOfLineINL_Helpers::returnSingle(currentThread, isFfiProtoOn, 0);
	return rc;
}

/**
 * openj9.internal.foreign.abi.InternalDowncallHandler: private static native void setNativeInvokeCache(MethodHandle handle);
 *
 * @brief Resolve the bounded handle via MethodHandleResolver.ffiCallLinkCallerMethod() in advance
 *        when calling into the interpreter via MH.linkToNative()
 *
 * @return void
 */
VM_BytecodeAction
OutOfLineINL_openj9_internal_foreign_abi_InternalDowncallHandler_setNativeInvokeCache(J9VMThread *currentThread, J9Method *method)
{
	VM_BytecodeAction rc = EXECUTE_BYTECODE;
	j9object_t invokeCache = NULL;
	j9object_t nativeMH = J9_JNI_UNWRAP_REFERENCE(currentThread->sp);

	/* The resolution is performed by MethodHandleResolver.ffiCallLinkCallerMethod()
	 * to obtain a MemberName object plus appendix in a two element object array.
	 */
	VM_OutOfLineINL_Helpers::buildInternalNativeStackFrame(currentThread, method);
	VM_VMHelpers::pushObjectInSpecialFrame(currentThread, nativeMH);
	invokeCache = resolveFfiCallInvokeHandle(currentThread, nativeMH);
	nativeMH = VM_VMHelpers::popObjectInSpecialFrame(currentThread);
	if (VM_VMHelpers::exceptionPending(currentThread)) {
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		goto done;
	}

	VM_AtomicSupport::writeBarrier();
	J9VMJAVALANGINVOKENATIVEMETHODHANDLE_SET_INVOKECACHE(currentThread, nativeMH, invokeCache);
	VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);

done:
	VM_OutOfLineINL_Helpers::returnVoid(currentThread, 1);
	return rc;
}
#endif /* JAVA_SPEC_VERSION >= 22 */
#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
