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

#include "j9.h"
#include "jcl.h"


void JNICALL Java_java_lang_Compiler_enable(JNIEnv *env, jclass clazz)
{
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	J9VMThread *currentThread = (J9VMThread *) env;
	J9JavaVM *vm = currentThread->javaVM;
	J9JITConfig * jitConfig = vm->jitConfig;

	if ((jitConfig != NULL) && (jitConfig->enableJit != NULL)) {
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		vmFuncs->internalEnterVMFromJNI(currentThread);
		vmFuncs->internalReleaseVMAccess(currentThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
		jitConfig->enableJit(jitConfig);
	}
#endif
}


void JNICALL Java_java_lang_Compiler_disable(JNIEnv *env, jclass clazz)
{
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	J9VMThread *currentThread = (J9VMThread *) env;
	J9JavaVM *vm = currentThread->javaVM;
	J9JITConfig * jitConfig = vm->jitConfig;

	if ((jitConfig != NULL) && (jitConfig->disableJit != NULL)) {
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		vmFuncs->internalEnterVMFromJNI(currentThread);
		vmFuncs->internalReleaseVMAccess(currentThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
		jitConfig->disableJit(jitConfig);
	}
#endif
}


jobject JNICALL Java_java_lang_Compiler_commandImpl(JNIEnv *env, jclass clazz, jobject cmd)
{
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	J9VMThread *currentThread = (J9VMThread *) env;
	J9JavaVM *vm = currentThread->javaVM;
	J9JITConfig * jitConfig = vm->jitConfig;

	if ((cmd != NULL) && (jitConfig != NULL) && (jitConfig->command != NULL)) {
		jclass stringClass = (*env)->FindClass(env, "java/lang/String");

		if (stringClass != NULL) {
			jclass intClass = (*env)->FindClass(env, "java/lang/Integer");

			if (intClass != NULL) {
				jmethodID mid = (*env)->GetMethodID(env, intClass, "<init>", "(I)V");

				if (mid != NULL) {
					if ((*env)->IsInstanceOf(env, cmd, stringClass)) {
						const char * commandString = (const char *) (*env)->GetStringUTFChars(env, cmd, NULL);

						if (commandString != NULL) {
							I_32 result = 0;
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
							J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
							vmFuncs->internalEnterVMFromJNI(currentThread);
							vmFuncs->internalReleaseVMAccess(currentThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
							result = jitConfig->command(currentThread, commandString);
							(*env)->ReleaseStringUTFChars(env, cmd, commandString);
							return (*env)->NewObject(env, intClass, mid, result);
						}
					}
				}
			}
		}
	}
#endif
	return NULL;
}


jboolean JNICALL Java_java_lang_Compiler_compileClassImpl(JNIEnv *env, jclass clazz, jclass compileClass)
{
	jboolean rc = JNI_FALSE;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	J9VMThread *currentThread = (J9VMThread *) env;
	J9JavaVM *vm = currentThread->javaVM;
	J9JITConfig * jitConfig = vm->jitConfig;

	if ((compileClass != NULL) && (jitConfig != NULL) && (jitConfig->compileClass != NULL)) {
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		vmFuncs->internalEnterVMFromJNI(currentThread);
		vmFuncs->internalReleaseVMAccess(currentThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
		rc = (jboolean)jitConfig->compileClass(currentThread, compileClass);
	}
#endif
	return rc;
}


jboolean JNICALL Java_java_lang_Compiler_compileClassesImpl(JNIEnv *env, jclass clazz, jstring nameRoot)
{
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	J9VMThread *currentThread = (J9VMThread *) env;
	J9JavaVM *vm = currentThread->javaVM;
	J9JITConfig * jitConfig = vm->jitConfig;

	if ((nameRoot != NULL) && (jitConfig != NULL) && (jitConfig->compileClasses != NULL)) {
		const char * pattern;

		pattern = (const char *) (*env)->GetStringUTFChars(env, nameRoot, NULL);
		if (pattern != NULL) {
			jboolean rc;

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
			J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
			vmFuncs->internalEnterVMFromJNI(currentThread);
			vmFuncs->internalReleaseVMAccess(currentThread);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
			rc = (jboolean) jitConfig->compileClasses(currentThread, pattern);
			(*env)->ReleaseStringUTFChars(env, nameRoot, pattern);
			return rc;
		}
	}
#endif
	return JNI_FALSE;
}
