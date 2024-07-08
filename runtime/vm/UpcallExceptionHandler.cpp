/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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

#include "ut_j9vm.h"
#include "vm_internal.h"
#if JAVA_SPEC_VERSION >= 16
#include "ffi.h"
#include <setjmp.h>
#endif /* JAVA_SPEC_VERSION >= 16 */

extern "C" {

#if JAVA_SPEC_VERSION >= 16
/**
 * @brief Save the contents of registers in the call-out for longjmp in
 * the dispatcher to restore back to this call site whenever an exception
 * is captured in upcall.
 *
 * See the invocation of longjmp in native2InterpJavaUpcallImpl()
 * at UpcallVMHelpers.cpp for details.
 *
 * Note: this is a wrapper for setjmp() as a function calling
 * setjmp() can never be inlined as captured in compilation.
 *
 * @param currentThread[in] The pointer to the current J9VMThread
 * @param cif[in] The pointer to the ffi_cif structure
 * @param function[in] The pointer to the native function address
 * @param returnStorage[in] The pointer to the return value
 * @param values[in] The pointer to an array of the passed-in arguments
 * @param values_raw[in] The pointer to the ffi_raw structure for the defined FFI_NATIVE_RAW_API
 */
void
#if FFI_NATIVE_RAW_API
ffiCallWithSetJmpForUpcall(J9VMThread *currentThread, ffi_cif *cif, void *function, UDATA *returnStorage, void **values, ffi_raw *values_raw)
#else /* FFI_NATIVE_RAW_API */
ffiCallWithSetJmpForUpcall(J9VMThread *currentThread, ffi_cif *cif, void *function, UDATA *returnStorage, void **values)
#endif /* FFI_NATIVE_RAW_API */
{
	jmp_buf jmpBufferEnv = {};
	void *jmpBufEnvPtr = currentThread->jmpBufEnvPtr;

	/* We only need to restore back to the latest call-out from the dispatcher
	 * to throw the exception in the case of recursive calls, in which case
	 * the jump buffer (storing the contents of registers) should be allocated
	 * every time in downcall.
	 */
	currentThread->jmpBufEnvPtr = (void *)&jmpBufferEnv;

	if (!setjmp(jmpBufferEnv)) {
#if FFI_NATIVE_RAW_API
		ffi_ptrarray_to_raw(cif, values, values_raw);
		ffi_raw_call(cif, FFI_FN(function), returnStorage, values_raw);
#else /* FFI_NATIVE_RAW_API */
		ffi_call(cif, FFI_FN(function), returnStorage, values);
#endif /* FFI_NATIVE_RAW_API */
	}
	currentThread->jmpBufEnvPtr = jmpBufEnvPtr;
}

/**
 * @brief This function serves as a wrapper of longjmp that restore back to
 * the call site with all registered saved via setjmp whenever an exception
 * is captured in upcall.
 *
 * See the invocation of longjmp in native2InterpJavaUpcallImpl()
 * at UpcallVMHelpers.cpp for details.
 *
 * @param currentThread[in] The pointer to the current J9VMThread
 */
void longJumpWrapperForUpcall(J9VMThread *currentThread)
{
	jmp_buf *jmpBufEnvPtr = (jmp_buf *)(currentThread->jmpBufEnvPtr);
	Assert_VM_notNull(jmpBufEnvPtr);

#if defined(J9VM_ENV_DATA64) && defined(WIN32)
	/* longjmp() results in the OS walking the stack on Windows which calls C++ destructor
	 * that is not required in our code given the builder stack frames aren't walkable
	 * in our theory. To work around this issue, we have to set the Frame to 0 (undocumented
	 * in any Windows API) to get it work.
	 */
	((struct _JUMP_BUFFER*)jmpBufEnvPtr)->Frame = 0;
#endif /* defined(J9VM_ENV_DATA64) && defined(WIN32) */

	longjmp(*jmpBufEnvPtr, 1);
}

#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
