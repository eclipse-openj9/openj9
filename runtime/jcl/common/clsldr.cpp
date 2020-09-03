/*******************************************************************************
 * Copyright (c) 1998, 2020 IBM Corp. and others
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
#include "ArrayCopyHelpers.hpp"
#include "VMHelpers.hpp"
#include "j9.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jvminit.h"
#include "j9protos.h"
#include "vmhook_internal.h"
#include "j9jclnls.h"

#if JAVA_SPEC_VERSION >= 15
/* The same values are defined in MethodHandles.java */
#define CLASSOPTION_FLAG_NESTMATE 1
#define CLASSOPTION_FLAG_STRONG  2
#endif /* JAVA_SPEC_VERSION >= 15 */


extern "C" {

jboolean JNICALL Java_java_lang_ClassLoader_isVerboseImpl(JNIEnv *env, jclass clazz)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

	return ( (javaVM->verboseLevel & VERBOSE_CLASS) == VERBOSE_CLASS );
}

jclass JNICALL
Java_java_lang_ClassLoader_defineClassImpl(JNIEnv *env, jobject receiver, jstring className, jbyteArray classRep, jint offset, jint length, jobject protectionDomain)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
	UDATA options = 0;

	if (NULL == protectionDomain) {
		/*
		 * Only trusted code has access to JavaLangAccess.defineClass();
		 * callers only provide a NULL protectionDomain when exemptions
		 * are required.
		 */
		options |= J9_FINDCLASS_FLAG_UNSAFE;
	}

	jclass result = defineClassCommon(env, receiver, className, classRep, offset, length, protectionDomain, &options, NULL, NULL, TRUE);

	if (J9_ARE_ANY_BITS_SET(options, J9_FINDCLASS_FLAG_NAME_IS_INVALID) && (NULL == result) && (NULL == currentThread->currentException)) {
		/*
		 * Now that we know the class is not exempt, throw NoClassDefFoundError as we
		 * would have liked to have done above before we called defineClassCommon().
		 */
		vmFuncs->internalEnterVMFromJNI(currentThread);
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR, (UDATA *)*(j9object_t *)className);
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	return result;
}

#if JAVA_SPEC_VERSION >= 15
jclass JNICALL
Java_java_lang_ClassLoader_defineClassImpl1(JNIEnv *env, jobject receiver, jclass hostClass, jstring className, jbyteArray classRep, jobject protectionDomain, jboolean init, jint flags, jobject classData)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	UDATA options = 0;
	
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == classRep) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
		vmFuncs->internalExitVMToJNI(currentThread);
		return NULL;
	}
	if (NULL == hostClass) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		vmFuncs->internalExitVMToJNI(currentThread);
		return NULL;
	}

	j9object_t hostClassObject = J9_JNI_UNWRAP_REFERENCE(hostClass);
	J9Class *hostClazz =  J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, hostClassObject);

	vmFuncs->internalExitVMToJNI(currentThread);

	options |= (J9_FINDCLASS_FLAG_HIDDEN | J9_FINDCLASS_FLAG_UNSAFE);
	if (J9_ARE_ALL_BITS_SET(flags, CLASSOPTION_FLAG_NESTMATE)) {
		options |= J9_FINDCLASS_FLAG_CLASS_OPTION_NESTMATE;
	}
	if (J9_ARE_ALL_BITS_SET(flags, CLASSOPTION_FLAG_STRONG)) {
		options |= J9_FINDCLASS_FLAG_CLASS_OPTION_STRONG;
	} else {
		options |= J9_FINDCLASS_FLAG_ANON;
	}
	
	jsize length = env->GetArrayLength(classRep);

	jclass result = defineClassCommon(env, receiver, className, classRep, 0, length, protectionDomain, &options, hostClazz, NULL, FALSE);
	if (env->ExceptionCheck()) {
		return NULL;
	} else if (NULL == result) {
		throwNewInternalError(env, NULL);
		return NULL;
	}

	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL != classData) {
		j9object_t classDataObject = J9_JNI_UNWRAP_REFERENCE(classData);
		j9object_t resultClassObject = J9_JNI_UNWRAP_REFERENCE(result);
		J9VMJAVALANGCLASS_SET_CLASSDATA(currentThread, resultClassObject, classDataObject);
	}
	
	j9object_t classObject = J9_JNI_UNWRAP_REFERENCE(result);
	J9Class *j9clazz =  J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject);
	if (init) {
		if (VM_VMHelpers::classRequiresInitialization(currentThread, j9clazz)) {
			vmFuncs->initializeClass(currentThread, j9clazz);
			j9clazz = J9_CURRENT_CLASS(j9clazz);
		}
	} else {
		/* Should link the class, see https://github.com/eclipse/openj9/issues/10297 */
		vmFuncs->prepareClass(currentThread, j9clazz);
	}

	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}
#endif /* JAVA_SPEC_VERSION >= 15 */


jboolean JNICALL
Java_java_lang_ClassLoader_foundJavaAssertOption(JNIEnv *env, jclass ignored)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

	return J9_ARE_ALL_BITS_SET(javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_FOUND_JAVA_ASSERT_OPTION);
}

jint JNICALL
Java_com_ibm_oti_vm_BootstrapClassLoader_addJar(JNIEnv *env, jobject receiver, jbyteArray jarPath)
{
	jint newCount = 0;
	J9VMThread * currentThread = (J9VMThread *) env;
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions * vmFuncs = vm->internalVMFunctions;

	PORT_ACCESS_FROM_JAVAVM(vm);

	UDATA jarPathSize = (UDATA)env->GetArrayLength(jarPath);
	char *jarPathBuffer = (char *)j9mem_allocate_memory(jarPathSize + 1, J9MEM_CATEGORY_CLASSES);

	if (NULL != jarPathBuffer) {
		/* Use exclusive access to modify the array */
		vmFuncs->internalEnterVMFromJNI(currentThread);
		vmFuncs->acquireExclusiveVMAccess(currentThread);
		VM_ArrayCopyHelpers::memcpyFromArray(currentThread, J9_JNI_UNWRAP_REFERENCE(jarPath), (UDATA)0, (UDATA)0, jarPathSize, (void*)jarPathBuffer);
		jarPathBuffer[jarPathSize] = 0; /* null character terminated */
		newCount = (jint)addJarToSystemClassLoaderClassPathEntries(vm, jarPathBuffer);
		j9mem_free_memory(jarPathBuffer);
		vmFuncs->releaseExclusiveVMAccess(currentThread);
		vmFuncs->internalExitVMToJNI(currentThread);
	}
	/* If any error occurred, throw OutOfMemoryError */
	if (0 == newCount) {
		vmFuncs->throwNativeOOMError(env, J9NLS_JCL_UNABLE_TO_CREATE_CLASS_PATH_ENTRY);
	}
	return newCount;
}

} /* extern "C" */
