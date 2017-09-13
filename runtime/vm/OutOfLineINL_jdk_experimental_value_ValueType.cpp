/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include "OutOfLineINL.hpp"

#include "BytecodeAction.hpp"
#include "UnsafeAPI.hpp"

extern "C" {

#if defined(J9VM_OPT_VALHALLA_MVT)
/* jdk.experimental.value.ValueType: private static native java.lang.Class valueClassImpl(java.lang.Class); */
VM_BytecodeAction
OutOfLineINL_jdk_experimental_value_ValueType_valueClassImpl(J9VMThread *currentThread, J9Method *method)
{
	VM_BytecodeAction rc = EXECUTE_BYTECODE;
	j9object_t classObject = *(j9object_t*)currentThread->sp;
	if (NULL == classObject) {
		rc = THROW_NPE;
	} else {
		J9Class *sourceClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject);
		J9Class *valueClass = sourceClass->derivedValueType;
		VM_OutOfLineINL_Helpers::returnObject(currentThread, J9VM_J9CLASS_TO_HEAPCLASS(valueClass), 1);
	}
	return rc;
}
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */

}
