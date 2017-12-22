/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "fastJNI.h"

#include "j9protos.h"
#include "j9consts.h"
#include "objhelp.h"
#include "VMHelpers.hpp"

extern "C" {

/* private static final native int getCPTypeAt(Class clazz, int index);
 * Return the result of J9_CP_TYPE(J9Class->romClass->cpShapeDescription, index).
 * See the JNI version in sun_reflect_ConstantPool.c
 */
jint JNICALL
Fast_java_lang_invoke_MethodHandle_getCPTypeAt(J9VMThread *vmThread, j9object_t constantPoolOop, jint cpIndex)
{
	UDATA cpType = J9CPTYPE_UNUSED;

	if (NULL == constantPoolOop) {
		setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9Class const *ramClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, constantPoolOop);
		J9ROMClass const *romClass = ramClass->romClass;

		if ((0 <= cpIndex) && ((U_32)cpIndex < romClass->romConstantPoolCount)) {
			U_32 *cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);
			cpType = J9_CP_TYPE(cpShapeDescription, cpIndex);
		} else {
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		}
	}
	return (jint)cpType;
}

J9_FAST_JNI_METHOD_TABLE(java_lang_invoke_MethodHandle)
	J9_FAST_JNI_METHOD("getCPTypeAt", "(Ljava/lang/Class;I)I", Fast_java_lang_invoke_MethodHandle_getCPTypeAt,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
J9_FAST_JNI_METHOD_TABLE_END

}
