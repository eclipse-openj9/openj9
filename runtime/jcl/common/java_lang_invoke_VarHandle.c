/*******************************************************************************
 * Copyright IBM Corp. and others 2016
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

#if defined(J9VM_OPT_METHOD_HANDLE) && (JAVA_SPEC_VERSION >= 11)
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
		if (IS_CLASS_SIGNATURE(*lookupSigData)) {
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
						TRUE,
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
	J9UTF8 *signatureUTF8 = NULL;
	char signatureUTF8Buffer[256];
	J9Class *j9LookupClass;			/* J9Class for java.lang.Class lookupClass */
	J9Class *definingClass = NULL;
	UDATA field = 0;
	UDATA romField = 0;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	j9object_t varHandle = NULL;
	PORT_ACCESS_FROM_ENV(env);

	vmFuncs->internalEnterVMFromJNI(vmThread);

	signatureUTF8 = vmFuncs->copyStringToJ9UTF8WithMemAlloc(vmThread, J9_JNI_UNWRAP_REFERENCE(signature), J9_STR_NONE, "", 0, signatureUTF8Buffer, sizeof(signatureUTF8Buffer));

	if (signatureUTF8 == NULL) {
		vmFuncs->setNativeOutOfMemoryError(vmThread, 0, 0);
		Assert_JCL_notNull(vmThread->currentException);
		goto _cleanup;
	}

	j9LookupClass = J9VM_J9CLASS_FROM_JCLASS(vmThread, lookupClass);

	field = lookupField(env, isStatic, j9LookupClass, name, signatureUTF8, &definingClass, &romField, accessClass);

	if (NULL != vmThread->currentException) {
		goto _cleanup;
	}

	/* Check signature for classloader visibility */
	if (!accessCheckFieldType(vmThread, j9LookupClass, J9VM_J9CLASS_FROM_JCLASS(vmThread, type), signatureUTF8)) {
		setClassLoadingConstraintLinkageError(vmThread, definingClass, signatureUTF8);
		goto _cleanup;
	}

	varHandle = J9_JNI_UNWRAP_REFERENCE(handle);
	J9VMJAVALANGINVOKEVARHANDLE_SET_MODIFIERS(vmThread, varHandle, ((J9ROMFieldShape *)romField)->modifiers);

	/* StaticFieldVarHandle and InstanceFieldVarHandle extend FieldVarHandle. For StaticFieldVarHandle, isStatic
	 * is true, and the definingClass field is set to the owner of the static field. For InstanceFieldVarHandle,
	 * isStatic is false, and the definingClass field is set to the lookupClass, which is also the instance class,
	 * in FieldVarHandle's constructor.
	 */
	if (isStatic) {
		Assert_JCL_notNull(definingClass);
		J9VMJAVALANGINVOKEFIELDVARHANDLE_SET_DEFININGCLASS(vmThread, varHandle, J9VM_J9CLASS_TO_HEAPCLASS(definingClass));
	}

_cleanup:
	vmFuncs->internalExitVMToJNI(vmThread);

	if (signatureUTF8 != (J9UTF8*)signatureUTF8Buffer) {
		j9mem_free_memory(signatureUTF8);
	}

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

	vmFuncs->internalExitVMToJNI(vmThread);
	return fieldOffset;
}
#endif /* defined(J9VM_OPT_METHOD_HANDLE) && (JAVA_SPEC_VERSION >= 11) */

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
Java_java_lang_invoke_VarHandle_compareAndExchange(JNIEnv *env, jobject handle, jobject args)
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

jboolean JNICALL
Java_java_lang_invoke_VarHandle_weakCompareAndSetPlain(JNIEnv *env, jobject handle, jobject args)
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
Java_java_lang_invoke_VarHandle_getAndSetAcquire(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndSetRelease(JNIEnv *env, jobject handle, jobject args)
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
Java_java_lang_invoke_VarHandle_getAndAddAcquire(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndAddRelease(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndBitwiseAnd(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndBitwiseAndAcquire(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndBitwiseAndRelease(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndBitwiseOr(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndBitwiseOrAcquire(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndBitwiseOrRelease(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndBitwiseXor(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndBitwiseXorAcquire(JNIEnv *env, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_VarHandle_getAndBitwiseXorRelease(JNIEnv *env, jobject handle, jobject args)
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
