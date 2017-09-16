/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#if !defined(VM_OUTOFLINEINL_HPP_)
#define VM_OUTOFLINEINL_HPP_

#include "j9.h"
#include "BytecodeAction.hpp"

extern "C" {

typedef VM_BytecodeAction J9OutOfLineINLMethod(J9VMThread *, J9Method *);

J9OutOfLineINLMethod OutOfLineINL_jdk_internal_misc_Unsafe_fullFence;
J9OutOfLineINLMethod OutOfLineINL_jdk_internal_misc_Unsafe_compareAndExchangeObjectVolatile;
J9OutOfLineINLMethod OutOfLineINL_jdk_internal_misc_Unsafe_compareAndExchangeIntVolatile;
J9OutOfLineINLMethod OutOfLineINL_jdk_internal_misc_Unsafe_compareAndExchangeLongVolatile;
#if defined(J9VM_OPT_VALHALLA_MVT)
J9OutOfLineINLMethod OutOfLineINL_jdk_experimental_value_ValueType_valueClassImpl;
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */
#if defined(J9VM_OPT_PANAMA)
J9OutOfLineINLMethod OutOfLineINL_java_lang_invoke_NativeMethodHandle_initJ9NativeCalloutDataRef;
#endif /* defined(J9VM_OPT_PANAMA) */
}

class VM_OutOfLineINL_Helpers {
public:
	static VMINLINE void
	returnVoid(J9VMThread *currentThread, UDATA slotCount)
	{
		currentThread->pc += 3;
		currentThread->sp += slotCount;
	}

	static VMINLINE void
	returnSingle(J9VMThread *currentThread, I_32 value, UDATA slotCount)
	{
		currentThread->pc += 3;
		currentThread->sp += (slotCount - 1);
		*(I_32 *)currentThread->sp = value;
	}

	static VMINLINE void
	returnDouble(J9VMThread *currentThread, I_64 value, UDATA slotCount)
	{
		currentThread->pc += 3;
		currentThread->sp += (slotCount - 2);
		*(I_64 *)currentThread->sp = value;
	}

	static VMINLINE void
	returnObject(J9VMThread *currentThread, j9object_t value, UDATA slotCount)
	{
		currentThread->pc += 3;
		currentThread->sp += (slotCount - 1);
		*(j9object_t *)currentThread->sp = value;
	}

	static VMINLINE void
	returnThis(J9VMThread *currentThread, UDATA slotCount)
	{
		currentThread->pc += 3;
		currentThread->sp += (slotCount - 1);
	}
};

#endif /* VM_OUTOFLINEINL_HPP_ */
