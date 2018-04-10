/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

#include "OutOfLineINL.hpp"

#include "BytecodeAction.hpp"
#include "UnsafeAPI.hpp"
#include "j9vmnls.h"

#ifdef J9VM_OPT_PANAMA
#include "FFITypeHelpers.hpp"
#endif /* J9VM_OPT_PANAMA */

extern "C" {

#ifdef J9VM_OPT_PANAMA
/* java.lang.invoke.NativeMethodHandle: private native void initJ9NativeCalloutDataRef(java.lang.String[] argLayoutStrings); */
VM_BytecodeAction
OutOfLineINL_java_lang_invoke_NativeMethodHandle_initJ9NativeCalloutDataRef(J9VMThread *currentThread, J9Method *method)
{
	VM_BytecodeAction rc = EXECUTE_BYTECODE;
	J9JavaVM *vm = currentThread->javaVM;
	ffi_type **args = NULL;
	ffi_cif *cif = NULL;
	J9NativeCalloutData *nativeCalloutData = NULL;

	j9object_t methodHandle = *(j9object_t*)(currentThread->sp + 1);

	Assert_VM_Null(J9VMJAVALANGINVOKENATIVEMETHODHANDLE_J9NATIVECALLOUTDATAREF(currentThread, methodHandle));

	j9object_t methodType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(currentThread, methodHandle);
	J9Class *returnTypeClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, J9VMJAVALANGINVOKEMETHODTYPE_RETURNTYPE(currentThread, methodType));
	j9object_t argTypesObject = J9VMJAVALANGINVOKEMETHODTYPE_ARGUMENTS(currentThread, methodType);

	FFITypeHelpers FFIHelpers = FFITypeHelpers(currentThread);
	j9object_t argLayoutStringsObject = *(j9object_t*)currentThread->sp;
	bool layoutStringsExist = false;
	if (NULL != argLayoutStringsObject) {
		layoutStringsExist = true;
	}
	
	PORT_ACCESS_FROM_JAVAVM(vm);

	/**
	 * args[0] stores the ffi_type of the return argument of the method. The remaining args array stores the ffi_type of the input arguments.
	 * typeCount is the number of input arguments, plus one for the return argument.
	 */
	U_32 typeCount = J9INDEXABLEOBJECT_SIZE(currentThread, argTypesObject) + 1;
	args = (ffi_type **)j9mem_allocate_memory(sizeof(ffi_type *) * typeCount, OMRMEM_CATEGORY_VM);

	if (NULL == args) {
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		setNativeOutOfMemoryError(currentThread, 0, 0);
		goto done;
	}

	/* Zero out the memory because if an error occurs before all the entries in this array are initialized then the error
	* handling code at freeAllMemoryThenExit will attempt to free all the pointers, some of which will not be initialized.
	*/
	memset(args, 0, sizeof(ffi_type *) * typeCount);

	cif = (ffi_cif *)j9mem_allocate_memory(sizeof(ffi_cif), OMRMEM_CATEGORY_VM);
	if (NULL == cif) {
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		setNativeOutOfMemoryError(currentThread, 0, 0);
		goto freeArgsThenExit;
	}

	/* In the common case we expect that layoutStringsExist will be NULL, which is why args[0] is written first then layoutStringsExist is checked after */
	args[0] = (ffi_type *)FFIHelpers.getFFIType(returnTypeClass);
	if (layoutStringsExist) {
		j9object_t retlayoutStringObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, argLayoutStringsObject, 0);
		if (NULL != retlayoutStringObject) {
			UDATA structSize = FFIHelpers.getCustomFFIType(&(args[0]), retlayoutStringObject);
			if ((NULL == args[0]) || (NULL == args[0]->elements) || (0 == structSize)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				setNativeOutOfMemoryError(currentThread, 0, 0);
				FFIHelpers.freeStructFFIType(args[0]);
				goto freeCifAndArgsThenExit;
			}
		}
	}

	if (layoutStringsExist) {
		for (U_8 i = 0; i < typeCount-1; i++) {
			j9object_t layoutStringObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, argLayoutStringsObject, i+1);
			if (NULL == layoutStringObject) {
				args[i+1] = (ffi_type *)FFIHelpers.getFFIType(J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, J9JAVAARRAYOFOBJECT_LOAD(currentThread, argTypesObject, i)));
			} else {
				/**
				 * Pointers are converted to longs before the callout so the ffi type for pointers is sint64.
				 * Structs are also converted to longs before the callout, and are identified by the non-null layout string of the argument.
				 * The struct ffi type is created after the struct layout string is parsed in getCustomFFIType(ffi_type**, j9object_t).
				 */
				UDATA structSize = FFIHelpers.getCustomFFIType(&(args[i+1]), layoutStringObject);
				if (UDATA_MAX == structSize) {
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
					goto freeAllMemoryThenExit;
				}
			}
		}
	} else {
		for (U_8 i = 0; i < typeCount-1; i++) {
			args[i+1] = (ffi_type *)FFIHelpers.getFFIType(J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, J9JAVAARRAYOFOBJECT_LOAD(currentThread, argTypesObject, i)));
		}
	}

	/* typeCount-1 because ffi_prep_cif expects the number of input arguments of the method */
	if (FFI_OK != ffi_prep_cif(cif, FFI_DEFAULT_ABI, typeCount-1, args[0], &(args[1]))) {
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		goto freeAllMemoryThenExit;
	}

	nativeCalloutData = (J9NativeCalloutData *)j9mem_allocate_memory(sizeof(J9NativeCalloutData), OMRMEM_CATEGORY_VM);
	if (NULL == nativeCalloutData) {
		rc = GOTO_THROW_CURRENT_EXCEPTION;
		setNativeOutOfMemoryError(currentThread, 0, 0);
		goto freeAllMemoryThenExit;
	}
	nativeCalloutData->arguments = args;
	nativeCalloutData->cif = cif;

	J9VMJAVALANGINVOKENATIVEMETHODHANDLE_SET_J9NATIVECALLOUTDATAREF(currentThread, methodHandle, (void *)nativeCalloutData);

done:
	VM_OutOfLineINL_Helpers::returnVoid(currentThread, 2);
	return rc;
freeAllMemoryThenExit:
	for (U_32 i = 0; i < typeCount; i++) {
		FFIHelpers.freeStructFFIType(args[i]);
	}
freeCifAndArgsThenExit:
	j9mem_free_memory(cif);
freeArgsThenExit:
	j9mem_free_memory(args);
	goto done;
}

/* java.lang.invoke.NativeMethodHandle: private native void freeJ9NativeCalloutDataRef(); */
VM_BytecodeAction
OutOfLineINL_java_lang_invoke_NativeMethodHandle_freeJ9NativeCalloutDataRef(J9VMThread *currentThread, J9Method *method)
{
	J9JavaVM *vm = currentThread->javaVM;
	j9object_t methodHandle = *(j9object_t*)currentThread->sp;

	j9object_t methodType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(currentThread, methodHandle);
	j9object_t argTypesObject = J9VMJAVALANGINVOKEMETHODTYPE_ARGUMENTS(currentThread, methodType);
	U_32 argumentCount = J9INDEXABLEOBJECT_SIZE(currentThread, argTypesObject) + 1;
	J9NativeCalloutData *nativeCalloutData = J9VMJAVALANGINVOKENATIVEMETHODHANDLE_J9NATIVECALLOUTDATAREF(currentThread, methodHandle);

	/* Manually synchronize methodHandle since declaring the method as synchronized in Java will have no effect (for all INLs). */
	j9object_t methodHandleLock = (j9object_t)objectMonitorEnter(currentThread, methodHandle);
	if (NULL == methodHandleLock) {
		setNativeOutOfMemoryError(currentThread, J9NLS_VM_FAILED_TO_ALLOCATE_MONITOR);
		goto done;
	}

	J9VMJAVALANGINVOKENATIVEMETHODHANDLE_SET_J9NATIVECALLOUTDATAREF(currentThread, methodHandle, NULL);

	if (NULL != nativeCalloutData) {
		PORT_ACCESS_FROM_JAVAVM(vm);

		Assert_VM_notNull(nativeCalloutData);
		Assert_VM_notNull(nativeCalloutData->arguments);
		Assert_VM_notNull(nativeCalloutData->cif);

		FFITypeHelpers FFIHelpers = FFITypeHelpers(currentThread);
		for (U_8 i = 0; i < argumentCount; i++) {
			FFIHelpers.freeStructFFIType(nativeCalloutData->arguments[i]);
		}
		j9mem_free_memory(nativeCalloutData->arguments);
		nativeCalloutData->arguments = NULL;

		j9mem_free_memory(nativeCalloutData->cif);
		nativeCalloutData->cif = NULL;

		j9mem_free_memory(nativeCalloutData);
		nativeCalloutData = NULL;
	}

	objectMonitorExit(currentThread, methodHandleLock);

done:
	VM_OutOfLineINL_Helpers::returnVoid(currentThread, 1);
	return EXECUTE_BYTECODE;
}
#endif /* J9VM_OPT_PANAMA */

}
