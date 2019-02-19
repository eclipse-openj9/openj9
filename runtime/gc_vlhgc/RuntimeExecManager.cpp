/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "j9cfg.h"
#include "j2sever.h"
#include "rommeth.h"

#include "RuntimeExecManager.hpp"

#include "GlobalAllocationManagerTarok.hpp"

#define CLASS_NAME1 "java/lang/UNIXProcess"
#define CLASS_NAME2 "java/lang/ProcessImpl"
#define METHOD_NAME "forkAndExec"
#define METHOD_SIGNATURE_V6 "([B[BI[BI[BZLjava/io/FileDescriptor;Ljava/io/FileDescriptor;Ljava/io/FileDescriptor;)I"
#define METHOD_SIGNATURE_V7 "([B[BI[BI[B[IZ)I"
#define METHOD_SIGNATURE_V8 "(I[B[B[BI[BI[B[IZ)I"


typedef jint (JNICALL *forkAndExecNativeFunctionV6)(JNIEnv* env, jobject receiver, jobject arg1, jobject arg2, jint arg3, jobject arg4, jint arg5, jobject arg6, jboolean arg7, jobject arg8, jobject arg9, jobject arg10);
typedef jint (JNICALL *forkAndExecNativeFunctionV7)(JNIEnv* env, jobject receiver, jobject arg1, jobject arg2, jint arg3, jobject arg4, jint arg5, jobject arg6, jobject arg7, jboolean arg8);
typedef jint (JNICALL *forkAndExecNativeFunctionV8)(JNIEnv* env, jobject receiver, jint arg1, jobject arg2, jobject arg3, jobject arg4, jint arg5, jobject arg6, jint arg7, jobject arg8, jobject arg9, jboolean arg10);

MM_RuntimeExecManager::MM_RuntimeExecManager(MM_EnvironmentBase *env)
	: MM_BaseNonVirtual()
#if defined (LINUX) && !defined(J9ZTPF)
	, _savedForkAndExecNative(NULL)
#endif /* defined (LINUX) && !defined (J9ZTPF) */
{
	_typeId = __FUNCTION__;
}


bool
MM_RuntimeExecManager::initialize(MM_EnvironmentBase *env)
{
	bool result = true;
#if defined (LINUX) && !defined(J9ZTPF)
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	if (extensions->_numaManager.isPhysicalNUMASupported()) {
		J9JavaVM* javaVM = (J9JavaVM *)extensions->getOmrVM()->_language_vm;
		J9HookInterface **vmHookInterface = javaVM->internalVMFunctions->getVMHookInterface(javaVM);
		if (NULL == vmHookInterface) {
			result = false;
		} else if ((*vmHookInterface)->J9HookRegisterWithCallSite(vmHookInterface, J9HOOK_VM_JNI_NATIVE_BIND, jniNativeBindHook, OMR_GET_CALLSITE(), this)) {
			result = false;
		}
	}
#endif /* defined (LINUX) && !defined (J9ZTPF) */
	return result;
}

void
MM_RuntimeExecManager::tearDown(MM_EnvironmentBase *env)
{
#if defined (LINUX) && !defined(J9ZTPF)
	J9JavaVM* javaVM = (J9JavaVM *)env->getLanguageVM();
	J9HookInterface **vmHookInterface = javaVM->internalVMFunctions->getVMHookInterface(javaVM);
	if (NULL != vmHookInterface) {
		(*vmHookInterface)->J9HookUnregister(vmHookInterface, J9HOOK_VM_JNI_NATIVE_BIND, jniNativeBindHook, this);
	}

	_savedForkAndExecNative = NULL;
#endif /* defined (LINUX) && !defined (J9ZTPF) */
}

#if defined (LINUX) && !defined(J9ZTPF)
void 
MM_RuntimeExecManager::jniNativeBindHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMJNINativeBindEvent *jniNativeBindEvent = (J9VMJNINativeBindEvent*)eventData;
	MM_RuntimeExecManager *runtimeExecManager = (MM_RuntimeExecManager*)userData;
	J9VMThread *vmThread = jniNativeBindEvent->currentThread;
	J9Method *nativeMethod = jniNativeBindEvent->nativeMethod;
	J9Class *methodClass = J9_CLASS_FROM_METHOD(nativeMethod);

	/* we know that the class we're looking for is a system class, so ignore everything in other class loaders */
	if (methodClass->classLoader == vmThread->javaVM->systemClassLoader) {
		J9UTF8* classNameUTF8 = J9ROMCLASS_CLASSNAME(methodClass->romClass);
		J9ROMMethod* romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(nativeMethod);
		J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 *methodSignature = J9ROMMETHOD_SIGNATURE(romMethod);
		BOOLEAN literalCheck = FALSE;
		
		if ((J2SE_VERSION(vmThread->javaVM) & J2SE_VERSION_MASK) <= J2SE_18) {
			/* Assuming J2SE_18, no other lower level supported */
			literalCheck = J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(classNameUTF8), J9UTF8_LENGTH(classNameUTF8), CLASS_NAME1);
		} else {
			/* J2SE_V11 and beyond */
			literalCheck = J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(classNameUTF8), J9UTF8_LENGTH(classNameUTF8), CLASS_NAME2);
		}

		if (literalCheck) {
			if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodName), METHOD_NAME)) {
				Trc_MM_RuntimeExecManager_jniNativeBindHook_foundMethod(vmThread, J9UTF8_LENGTH(classNameUTF8), J9UTF8_DATA(classNameUTF8), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
				if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(methodSignature), J9UTF8_LENGTH(methodSignature), METHOD_SIGNATURE_V6)) {
					Assert_MM_true(NULL == runtimeExecManager->_savedForkAndExecNative);
					runtimeExecManager->_savedForkAndExecNative = jniNativeBindEvent->nativeMethodAddress;
					jniNativeBindEvent->nativeMethodAddress = (void*)forkAndExecNativeV6;
				} else if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(methodSignature), J9UTF8_LENGTH(methodSignature), METHOD_SIGNATURE_V7)) {
					Assert_MM_true(NULL == runtimeExecManager->_savedForkAndExecNative);
					runtimeExecManager->_savedForkAndExecNative = jniNativeBindEvent->nativeMethodAddress;
					jniNativeBindEvent->nativeMethodAddress = (void*)forkAndExecNativeV7;
				} else if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(methodSignature), J9UTF8_LENGTH(methodSignature), METHOD_SIGNATURE_V8)) {
					Assert_MM_true(NULL == runtimeExecManager->_savedForkAndExecNative);
					runtimeExecManager->_savedForkAndExecNative = jniNativeBindEvent->nativeMethodAddress;
					jniNativeBindEvent->nativeMethodAddress = (void*)forkAndExecNativeV8;
				}
				Trc_MM_RuntimeExecManager_jniNativeBindHook_replacedMethod(vmThread, runtimeExecManager->_savedForkAndExecNative, jniNativeBindEvent->nativeMethodAddress);
			}
		}
	}
}

jint
MM_RuntimeExecManager::forkAndExecNativeV6(JNIEnv* jniEnv, jobject receiver, jobject arg1, jobject arg2, jint arg3, jobject arg4, jint arg5, jobject arg6, jboolean arg7, jobject arg8, jobject arg9, jobject arg10)
{
	J9VMThread *vmThread = (J9VMThread*)jniEnv;
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(vmThread);
	jint result = 0;
	
	Trc_MM_RuntimeExecManager_forkAndExecNativeV6_Entry(vmThread);
	
	MM_GlobalAllocationManagerTarok *allocationManager = (MM_GlobalAllocationManagerTarok *)MM_GCExtensions::getExtensions(env)->globalAllocationManager;
	MM_RuntimeExecManager *runtimeExecManager = &allocationManager->_runtimeExecManager;
	forkAndExecNativeFunctionV6 savedForkAndExecNative = (forkAndExecNativeFunctionV6)runtimeExecManager->_savedForkAndExecNative;
	MM_AllocationContextTarok *allocationContext = (MM_AllocationContextTarok *)env->getAllocationContext();
	
	if (env->getCommonAllocationContext() == allocationContext) {
		/* this isn't an affinitized thread, so do nothing */
		result = savedForkAndExecNative(jniEnv, receiver, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
	} else {
		/* de-affinitize the thread while we call the fork/exec native */
		allocationContext->clearNumaAffinityForThread(env);
		result = savedForkAndExecNative(jniEnv, receiver, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
		allocationContext->setNumaAffinityForThread(env);
	}

	Trc_MM_RuntimeExecManager_forkAndExecNativeV6_Exit(vmThread);
	
	return result;
}

jint
MM_RuntimeExecManager::forkAndExecNativeV7(JNIEnv* jniEnv, jobject receiver, jobject arg1, jobject arg2, jint arg3, jobject arg4, jint arg5, jobject arg6, jobject arg7, jboolean arg8)
{
	J9VMThread *vmThread = (J9VMThread*)jniEnv;
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(vmThread);
	jint result = 0;
	
	Trc_MM_RuntimeExecManager_forkAndExecNativeV7_Entry(vmThread);
	
	MM_GlobalAllocationManagerTarok *allocationManager = (MM_GlobalAllocationManagerTarok *)MM_GCExtensions::getExtensions(env)->globalAllocationManager;
	MM_RuntimeExecManager *runtimeExecManager = &allocationManager->_runtimeExecManager;
	forkAndExecNativeFunctionV7 savedForkAndExecNative = (forkAndExecNativeFunctionV7)runtimeExecManager->_savedForkAndExecNative;
	MM_AllocationContextTarok *allocationContext = (MM_AllocationContextTarok *)env->getAllocationContext();
	
	if (env->getCommonAllocationContext() == allocationContext) {
		/* this isn't an affinitized thread, so do nothing */
		result = savedForkAndExecNative(jniEnv, receiver, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
	} else {
		/* de-affinitize the thread while we call the fork/exec native */
		allocationContext->clearNumaAffinityForThread(env);
		result = savedForkAndExecNative(jniEnv, receiver, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
		allocationContext->setNumaAffinityForThread(env);
	}
	
	Trc_MM_RuntimeExecManager_forkAndExecNativeV7_Exit(vmThread);
	
	return result;
}

jint
MM_RuntimeExecManager::forkAndExecNativeV8(JNIEnv* jniEnv, jobject receiver, jint arg1, jobject arg2, jobject arg3, jobject arg4, jint arg5, jobject arg6, jint arg7, jobject arg8, jobject arg9, jboolean arg10)
{
	J9VMThread *vmThread = (J9VMThread*)jniEnv;
	MM_EnvironmentVLHGC *env = MM_EnvironmentVLHGC::getEnvironment(vmThread);
	jint result = 0;

	Trc_MM_RuntimeExecManager_forkAndExecNativeV8_Entry(vmThread);

	MM_GlobalAllocationManagerTarok *allocationManager = (MM_GlobalAllocationManagerTarok *)env->getExtensions()->globalAllocationManager;
	MM_RuntimeExecManager *runtimeExecManager = &allocationManager->_runtimeExecManager;
	forkAndExecNativeFunctionV8 savedForkAndExecNative = (forkAndExecNativeFunctionV8)runtimeExecManager->_savedForkAndExecNative;
	MM_AllocationContextTarok *allocationContext = (MM_AllocationContextTarok *)env->getAllocationContext();

	if (env->getCommonAllocationContext() == allocationContext) {
		/* this isn't an affinitized thread, so do nothing */
		result = savedForkAndExecNative(jniEnv, receiver, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
	} else {
		/* de-affinitize the thread while we call the fork/exec native */
		allocationContext->clearNumaAffinityForThread(env);
		result = savedForkAndExecNative(jniEnv, receiver, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
		allocationContext->setNumaAffinityForThread(env);
	}

	Trc_MM_RuntimeExecManager_forkAndExecNativeV8_Exit(vmThread);

	return result;
}
#endif /* defined (LINUX) && !defined(J9ZTPF) */
