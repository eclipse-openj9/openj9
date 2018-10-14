/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

extern "C" {

/* sun.misc.Unsafe: public native void fullFence(); */
VM_BytecodeAction
OutOfLineINL_jdk_internal_misc_Unsafe_fullFence(J9VMThread *currentThread, J9Method *method)
{
	VM_UnsafeAPI::fullFence();
	VM_OutOfLineINL_Helpers::returnVoid(currentThread, 1);
	return EXECUTE_BYTECODE;
}

/* sun.misc.Unsafe: public final native java.lang.Object compareAndExchangeObjectVolatile(java.lang.Object obj, long offset, java.lang.Object compareValue, java.lang.Object swapValue); */
VM_BytecodeAction
OutOfLineINL_jdk_internal_misc_Unsafe_compareAndExchangeObjectVolatile(J9VMThread *currentThread, J9Method *method)
{
	j9object_t swapValue = *(j9object_t*)currentThread->sp;
	j9object_t compareValue = *(j9object_t*)(currentThread->sp + 1);
	UDATA offset = (UDATA)*(I_64*)(currentThread->sp + 2);
	j9object_t obj = *(j9object_t*)(currentThread->sp + 4);
	MM_ObjectAccessBarrierAPI _objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);

	VM_OutOfLineINL_Helpers::buildInternalNativeStackFrame(currentThread, method);
	j9object_t result = VM_UnsafeAPI::compareAndExchangeObject(currentThread, &_objectAccessBarrier, obj, offset, compareValue, swapValue);
	VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);

	VM_OutOfLineINL_Helpers::returnObject(currentThread, result, 6);
	return EXECUTE_BYTECODE;
}

/* sun.misc.Unsafe: public final native int compareAndExchangeIntVolatile(java.lang.Object obj, long offset, int compareValue, int swapValue); */
VM_BytecodeAction
OutOfLineINL_jdk_internal_misc_Unsafe_compareAndExchangeIntVolatile(J9VMThread *currentThread, J9Method *method)
{
	U_32 swapValue = *(U_32*)currentThread->sp;
	U_32 compareValue = *(U_32*)(currentThread->sp + 1);
	UDATA offset = (UDATA)*(I_64*)(currentThread->sp + 2);
	j9object_t obj = *(j9object_t*)(currentThread->sp + 4);
	MM_ObjectAccessBarrierAPI _objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);

	VM_OutOfLineINL_Helpers::buildInternalNativeStackFrame(currentThread, method);
	U_32 result = VM_UnsafeAPI::compareAndExchangeInt(currentThread, &_objectAccessBarrier, obj, offset, compareValue, swapValue);
	VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);

	VM_OutOfLineINL_Helpers::returnSingle(currentThread, result, 6);
	return EXECUTE_BYTECODE;
}

/* sun.misc.Unsafe: public final native long compareAndExchangeLongVolatile(java.lang.Object obj, long offset, long compareValue, long swapValue); */
VM_BytecodeAction
OutOfLineINL_jdk_internal_misc_Unsafe_compareAndExchangeLongVolatile(J9VMThread *currentThread, J9Method *method)
{
	U_64 swapValue = *(U_64*)currentThread->sp;
	U_64 compareValue = *(U_64*)(currentThread->sp + 2);
	UDATA offset = (UDATA)*(I_64*)(currentThread->sp + 4);
	j9object_t obj = *(j9object_t*)(currentThread->sp + 6);
	MM_ObjectAccessBarrierAPI _objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);

	VM_OutOfLineINL_Helpers::buildInternalNativeStackFrame(currentThread, method);
	U_64 result = VM_UnsafeAPI::compareAndExchangeLong(currentThread, &_objectAccessBarrier, obj, offset, compareValue, swapValue);
	VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);

	VM_OutOfLineINL_Helpers::returnDouble(currentThread, result, 8);
	return EXECUTE_BYTECODE;
}

}
