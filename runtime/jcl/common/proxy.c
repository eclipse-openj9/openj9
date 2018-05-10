/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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
#include "iohelp.h"
#include "jclglob.h"
#include "jclprots.h"

static jclass proxyDefineClass(
		JNIEnv * env,
		jobject classLoader,
		jstring className,
		jbyteArray classBytes,
		jint offset,
		jint length,
		jobject pd) {

	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9StackWalkState walkState;
	J9Class * clazz = NULL;

	/* Walk the stack to find the caller class loader and protection domain */
	vmFuncs->internalEnterVMFromJNI(currentThread);

	walkState.walkThread = currentThread;
	walkState.skipCount = 1;
	walkState.maxFrames = 1;
	walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_COUNT_SPECIFIED;
	vm->walkStackFrames(currentThread, &walkState);
	if (walkState.framesWalked == 0) {
		vmFuncs->internalExitVMToJNI(currentThread);
		throwNewInternalError(env, NULL);
		return NULL;
	}

	clazz = J9_CLASS_FROM_CP(walkState.constantPool);
	if (classLoader == NULL) {
		j9object_t classLoaderObject = J9CLASSLOADER_CLASSLOADEROBJECT(currentThread, clazz->classLoader);
		classLoader = vmFuncs->j9jni_createLocalRef(env, classLoaderObject);
	}
	if (pd == NULL) {
		j9object_t heapClass = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
		j9object_t protectionDomainDirectReference = J9VMJAVALANGCLASS_PROTECTIONDOMAIN(currentThread, heapClass);
		pd = vmFuncs->j9jni_createLocalRef(env, protectionDomainDirectReference);
	}

	vmFuncs->internalExitVMToJNI(currentThread);

	return defineClassCommon(env, classLoader, className, classBytes, offset, length, pd, 0, NULL);
}

jclass JNICALL Java_java_lang_reflect_Proxy_defineClassImpl(JNIEnv * env, jclass recvClass, jobject classLoader, jstring className, jbyteArray classBytes)
{
	if ( classBytes == NULL) {
		return	NULL;
	} else {
		return proxyDefineClass(env, classLoader, className, classBytes, 0, (*env)->GetArrayLength(env, classBytes), NULL);
	}
}


jclass JNICALL 
Java_java_lang_reflect_Proxy_defineClass0__Ljava_lang_ClassLoader_2Ljava_lang_String_2_3BII(JNIEnv * env, jclass recvClass, jobject classLoader, jstring className, jbyteArray classBytes, jint offset, jint length)
{
	return proxyDefineClass(env, classLoader, className, classBytes, offset, length, NULL);
}

jclass JNICALL 
Java_java_lang_reflect_Proxy_defineClass0__Ljava_lang_ClassLoader_2Ljava_lang_String_2_3BIILjava_lang_Object_2_3Ljava_lang_Object_2Ljava_lang_Object_2(JNIEnv * env, jclass recvClass, jobject classLoader, jstring className, jbyteArray classBytes, jint offset, jint length, jobject pd, jobject signers, jobject source)
{
	if (classLoader == NULL || pd == NULL) {
		return proxyDefineClass(env, classLoader, className, classBytes, offset, length, pd);
	} else {
		return defineClassCommon(env, classLoader, className, classBytes, offset, length, pd, 0, NULL);
	}
}

