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

#include "jclprots.h"

#include <string.h>
#include <stdlib.h>

#include "j9.h"
#include "jcl.h"
#include "j9consts.h"
#include "jni.h"
#include "j9protos.h"
#include "jclprots.h"
#include "ut_j9jcl.h"
#include "VM_MethodHandleKinds.h"

#include "rommeth.h"
#include "j9vmnls.h"
#include "j9vmconstantpool.h"
#include "j9jclnls.h"

static BOOLEAN
accessCheckFieldType(J9VMThread *currentThread, J9Class* lookupClass, J9Class* type, J9UTF8 *lookupSig)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9BytecodeVerificationData *verifyData = vm->bytecodeVerificationData;
	BOOLEAN result = TRUE;

	/* If the verifier isn't enabled, accept the access check unconditionally */
	if (NULL != verifyData) {
		U_8 *lookupSigData = J9UTF8_DATA(lookupSig);
		/* Only check reference types (not primitive types) */
		if ('L' == *lookupSigData) {
			J9ClassLoader *lookupClassloader = lookupClass->classLoader;
			J9ClassLoader *typeClassloader = type->classLoader;
			if (typeClassloader != lookupClassloader) {
				/* Different class loaders - check class loading constraint */
				j9thread_monitor_enter(vm->classTableMutex);
				if (verifyData->checkClassLoadingConstraintForNameFunction(
						currentThread,
						lookupClassloader,
						typeClassloader,
						&lookupSigData[1],
						&lookupSigData[1],
						J9UTF8_LENGTH(lookupSig) - 2, 
						TRUE) != 0) {
					result = FALSE;
				}
				j9thread_monitor_exit(vm->classTableMutex);
			}
		}
	}
	return result;
}

jlong JNICALL
Java_java_lang_invoke_FieldVarHandle_lookupField(JNIEnv *env, jobject handle, jclass lookupClass, jstring name, jstring signature, jclass type, jboolean isStatic, jclass accessClass)
{
	J9UTF8 *sigUTF = NULL;
	J9Class *j9LookupClass;			/* J9Class for java.lang.Class lookupClass */
	J9Class *definingClass = NULL;
	UDATA field = 0;
	UDATA romField = 0;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	PORT_ACCESS_FROM_ENV(env);

	vmFuncs->internalEnterVMFromJNI(vmThread);

	sigUTF = allocateJ9UTF8(env, signature);
	if (NULL == sigUTF) {
		Assert_JCL_notNull(vmThread->currentException);
		goto _cleanup;
	}

	j9LookupClass = J9VM_J9CLASS_FROM_JCLASS(vmThread, lookupClass);

	field = lookupField(env, isStatic, j9LookupClass, name, sigUTF, &definingClass, &romField, accessClass);

	if (NULL != vmThread->currentException) {
		goto _cleanup;
	}

	/* Check signature for classloader visibility */
	if (!accessCheckFieldType(vmThread, j9LookupClass, J9VM_J9CLASS_FROM_JCLASS(vmThread, type), sigUTF)) {
		setClassLoadingConstraintLinkageError(vmThread, definingClass, sigUTF);
		goto _cleanup;
	}

	J9VMJAVALANGINVOKEVARHANDLE_SET_MODIFIERS(vmThread, J9_JNI_UNWRAP_REFERENCE(handle), ((J9ROMFieldShape*) romField)->modifiers);

_cleanup:
	vmFuncs->internalExitVMToJNI(vmThread);
	j9mem_free_memory(sigUTF);
	return field;
}

jlong JNICALL
Java_java_lang_invoke_FieldVarHandle_unreflectField(JNIEnv *env, jobject handle, jobject reflectField, jboolean isStatic)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9ReflectFunctionTable *reflectFunctions = &(vm->reflectFunctions);
	J9JNIFieldID *fieldID = NULL;
	j9object_t fieldObject = NULL;
	UDATA fieldOffset = 0;

	vmFuncs->internalEnterVMFromJNI(vmThread);
	/* Can't fail as we know the field is not null */
	fieldObject = J9_JNI_UNWRAP_REFERENCE(reflectField);
	fieldID = reflectFunctions->idFromFieldObject(vmThread, NULL, fieldObject);

	fieldOffset = fieldID->offset;
	if (isStatic) {
		/* ensure this is correctly tagged so that the JIT targets using Unsafe will correctly detect this is static */
		fieldOffset |= J9_SUN_STATIC_FIELD_OFFSET_TAG;
	}

	J9VMJAVALANGINVOKEVARHANDLE_SET_MODIFIERS(vmThread, J9_JNI_UNWRAP_REFERENCE(handle), fieldID->field->modifiers);

	vmFuncs->internalReleaseVMAccess(vmThread);
	return fieldOffset;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_get(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

void JNICALL
Java_java_lang_invoke_VarHandle_set(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getVolatile(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

void JNICALL
Java_java_lang_invoke_VarHandle_setVolatile(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getOpaque(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

void JNICALL
Java_java_lang_invoke_VarHandle_setOpaque(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAcquire(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

void JNICALL
Java_java_lang_invoke_VarHandle_setRelease(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
}

jboolean JNICALL
Java_java_lang_invoke_VarHandle_compareAndSet(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return JNI_FALSE;
}

jboolean JNICALL
Java_java_lang_invoke_VarHandle_compareAndExchangeVolatile(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return JNI_FALSE;
}

jboolean JNICALL
Java_java_lang_invoke_VarHandle_compareAndExchangeAcquire(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return JNI_FALSE;
}

jboolean JNICALL
Java_java_lang_invoke_VarHandle_compareAndExchangeRelease(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return JNI_FALSE;
}

jboolean JNICALL
Java_java_lang_invoke_VarHandle_weakCompareAndSet(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return JNI_FALSE;
}

jboolean JNICALL
Java_java_lang_invoke_VarHandle_weakCompareAndSetAcquire(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return JNI_FALSE;
}

jboolean JNICALL
Java_java_lang_invoke_VarHandle_weakCompareAndSetRelease(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return JNI_FALSE;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndSet(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndAdd(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_addAndGet(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}
