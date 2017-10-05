/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
#include "ArrayCopyHelpers.hpp"
#include "j9.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jvminit.h"
#include "j9protos.h"
#include "vmhook_internal.h"
#include "j9jclnls.h"

extern "C" {

jboolean JNICALL Java_java_lang_ClassLoader_isVerboseImpl(JNIEnv *env, jclass clazz)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

	return ( (javaVM->verboseLevel & VERBOSE_CLASS) == VERBOSE_CLASS );
}


jclass JNICALL 
Java_java_lang_ClassLoader_defineClassImpl(JNIEnv *env, jobject receiver, jstring className, jbyteArray classRep, jint offset, jint length, jobject protectionDomain)
{
#ifdef J9VM_OPT_DYNAMIC_LOAD_SUPPORT
	if (className != NULL) {
		J9VMThread *currentThread = (J9VMThread *)env;
		J9JavaVM *vm = currentThread->javaVM;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		if (CLASSNAME_INVALID == vmFuncs->verifyQualifiedName(currentThread, J9_JNI_UNWRAP_REFERENCE(className))) {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR, (UDATA *)*(j9object_t*)className);
			vmFuncs->internalReleaseVMAccess(currentThread);
			return NULL;
		}

		vmFuncs->internalReleaseVMAccess(currentThread);
	}
#endif

	return defineClassCommon(env, receiver, className, classRep, offset, length, protectionDomain, 0, NULL);
}

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
		vmFuncs->internalReleaseVMAccess(currentThread);
	}
	/* If any error occcurred, throw OutOfMemoryError */
	if (0 == newCount) {
		throwNativeOOMError(env, J9NLS_JCL_UNABLE_TO_CREATE_CLASS_PATH_ENTRY);
	}
	return newCount;
}

} /* extern "C" */
