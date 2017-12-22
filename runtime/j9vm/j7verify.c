/*******************************************************************************
 * Copyright (c) 2002, 2014 IBM Corp. and others
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

/**
 * @brief  This file contains implementations of the Sun VM interface (JVM_ functions)
 * needed by the verify dll.
 */

#include <stdlib.h>
#include <assert.h>
#include "j9.h"
#include "sunvmi_api.h"




jobject JNICALL
JVM_GetMethodIxLocalsCount(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetMethodIxLocalsCount() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetCPMethodNameUTF(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetCPMethodNameUTF() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxExceptionTableEntry(jint arg0, jint arg1, jint arg2, jint arg3, jint arg4)
{
	assert(!"JVM_GetMethodIxExceptionTableEntry() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxExceptionTableLength(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetMethodIxExceptionTableLength() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxMaxStack(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetMethodIxMaxStack() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxExceptionIndexes(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_GetMethodIxExceptionIndexes() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetCPFieldSignatureUTF(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetCPFieldSignatureUTF() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetClassMethodsCount(jint arg0, jint arg1)
{
	assert(!"JVM_GetClassMethodsCount() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetClassFieldsCount(jint arg0, jint arg1)
{
	assert(!"JVM_GetClassFieldsCount() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetClassCPTypes(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetClassCPTypes() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetClassCPEntriesCount(jint arg0, jint arg1)
{
	assert(!"JVM_GetClassCPEntriesCount() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetCPMethodSignatureUTF(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetCPMethodSignatureUTF() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetCPFieldModifiers(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_GetCPFieldModifiers() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetCPMethodModifiers(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_GetCPMethodModifiers() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_IsSameClassPackage(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_IsSameClassPackage() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetCPMethodClassNameUTF(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetCPMethodClassNameUTF() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetCPFieldClassNameUTF(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetCPFieldClassNameUTF() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetCPClassNameUTF(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetCPClassNameUTF() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxArgsSize(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetMethodIxArgsSize() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxModifiers(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetMethodIxModifiers() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_IsConstructorIx(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_IsConstructorIx() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxByteCodeLength(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetMethodIxByteCodeLength() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxByteCode(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_GetMethodIxByteCode() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetFieldIxModifiers(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetFieldIxModifiers() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_FindClassFromClass(jint arg0, jint arg1, jint arg2, jint arg3)
{
	assert(!"JVM_FindClassFromClass() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetClassNameUTF(jint arg0, jint arg1)
{
	assert(!"JVM_GetClassNameUTF() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxNameUTF(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetMethodIxNameUTF() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxSignatureUTF(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetMethodIxSignatureUTF() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_GetMethodIxExceptionsCount(jint arg0, jint arg1, jint arg2)
{
	assert(!"JVM_GetMethodIxExceptionsCount() stubbed!");
	return NULL;
}



jobject JNICALL
JVM_ReleaseUTF(jint arg0)
{
	assert(!"JVM_ReleaseUTF() stubbed!");
	return NULL;
}


