/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
#include "VMHelpers.hpp"
#include "AtomicSupport.hpp"

extern "C" {

/* java.lang.VMAccess: protected static native Class findClassOrNull(String className, ClassLoader classLoader); */
j9object_t JNICALL
Fast_java_lang_VMAccess_findClassOrNull(J9VMThread *currentThread, j9object_t className, j9object_t classloaderObject)
{
	j9object_t classObject = NULL;
	if (NULL == className) {
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9JavaVM *vm = currentThread->javaVM;
		J9Class *j9Class = NULL;
		J9ClassLoader *loader = NULL;
		/* This native is only called for the boot loader, so vmRef is guaranteed to be initialized. */
		if (NULL != classloaderObject) {
			loader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, classloaderObject);
		} else {
			loader = vm->systemClassLoader;
		}
		if (CLASSNAME_VALID_NON_ARRARY == verifyQualifiedName(currentThread, className)) {
			j9Class = internalFindClassString(currentThread, NULL, className, loader, J9_FINDCLASS_FLAG_USE_LOADER_CP_ENTRIES);
			if (VM_VMHelpers::exceptionPending(currentThread)) {
				J9Class *exceptionClass = J9VMJAVALANGCLASSNOTFOUNDEXCEPTION(vm);
				/* If the current exception is ClassNotFoundException, discard it. */
				if (exceptionClass == J9OBJECT_CLAZZ(currentThread, currentThread->currentException)) {
					VM_VMHelpers::clearException(currentThread);
				}
			} else {
				classObject = J9VM_J9CLASS_TO_HEAPCLASS(j9Class);
			}
		}
	}
	return classObject;
}

/* com.ibm.oti.vm.VM: public static final native int getClassPathEntryType(java.lang.ClassLoader classLoader, int cpIndex); */
jint JNICALL
Fast_com_ibm_oti_vm_VM_getClassPathEntryType(J9VMThread *currentThread, j9object_t classLoaderObject, jint cpIndex)
{
	jint type = CPE_TYPE_UNUSABLE;
	/* This native is only called for the boot loader, so vmRef is guaranteed to be initialized */
	J9ClassLoader *classLoader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, classLoaderObject);
	/* Bounds check the parameter */
	if ((cpIndex >= 0) && (cpIndex < (jint)classLoader->classPathEntryCount)) {
		J9JavaVM *vm = currentThread->javaVM;
		/* Check the flags of the translation data struct */
		J9TranslationBufferSet *translationData = vm->dynamicLoadBuffers;
		if (NULL != translationData) {
			/* Initialize the class path entry */
			J9ClassPathEntry *cpEntry = classLoader->classPathEntries + cpIndex;
			type = (jint)initializeClassPathEntry(vm, cpEntry);
		}
	}
	return type;
}

/* com.ibm.oti.vm.VM: protected static native int getCPIndexImpl(Class targetClass); */
jint JNICALL
Fast_com_ibm_oti_vm_VM_getCPIndexImpl(J9VMThread *currentThread, j9object_t targetClass)
{
	J9Class *j9clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, targetClass);
	J9JavaVM *vm = currentThread->javaVM;

	omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
	J9ClassLocation *classLocation = findClassLocationForClass(currentThread, j9clazz);
	omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);

	/* (classLocation->locationType < 0) points to generated classes which don't have location.
	 * But, their class location has been hacked to properly retrieve package information.
	 * Such location types must return J9_CP_INDEX_NONE for classpath index.
	 */
	if ((NULL == classLocation) || (classLocation->locationType < 0)) {
		return (jint)J9_CP_INDEX_NONE;
	} else {
		return (jint)classLocation->entryIndex;
	}
}

/* com.ibm.oti.vm.VM: public static final native void initializeClassLoader(ClassLoader classLoader, int loaderType, boolean parallelCapable); */
void JNICALL
Fast_com_ibm_oti_vm_VM_initializeClassLoader(J9VMThread *currentThread, j9object_t classLoaderObject, jint loaderType, jboolean parallelCapable)
{
	if (NULL != J9VMJAVALANGCLASSLOADER_VMREF(currentThread, classLoaderObject)) {
internalError:
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
	} else {
		J9JavaVM *vm = currentThread->javaVM;
		if (J9_CLASSLOADER_TYPE_BOOT == loaderType) {
			/* if called with bootLoader, assign the system one to this instance */
			J9ClassLoader *classLoaderStruct = vm->systemClassLoader;
			j9object_t loaderObject = J9CLASSLOADER_CLASSLOADEROBJECT(currentThread, classLoaderStruct);
			if (NULL != loaderObject) {
				goto internalError;
			}
			J9CLASSLOADER_SET_CLASSLOADEROBJECT(currentThread, classLoaderStruct, classLoaderObject);
			if (parallelCapable) {
				classLoaderStruct->flags |= J9CLASSLOADER_PARALLEL_CAPABLE;
			}
			VM_AtomicSupport::writeBarrier();
			J9VMJAVALANGCLASSLOADER_SET_VMREF(currentThread, classLoaderObject, classLoaderStruct);
			TRIGGER_J9HOOK_VM_CLASS_LOADER_INITIALIZED(vm->hookInterface, currentThread, classLoaderStruct);

			J9ClassWalkState classWalkState;
			J9Class* clazz = allClassesStartDo(&classWalkState, vm, classLoaderStruct);
			while (NULL != clazz) {
				J9VMJAVALANGCLASS_SET_CLASSLOADER(currentThread, clazz->classObject, classLoaderObject);
				clazz = allClassesNextDo(&classWalkState);
			}
			allClassesEndDo(&classWalkState);
		} else {
			J9ClassLoader *classLoaderStruct = internalAllocateClassLoader(vm, classLoaderObject);
			if (J9_CLASSLOADER_TYPE_PLATFORM == loaderType) {
				vm->platformClassLoader = classLoaderStruct;
			}
		}
	}
}

/* com.ibm.oti.vm.VM: static private final native boolean isBootstrapClassLoader(ClassLoader loader); */
jboolean JNICALL
Fast_com_ibm_oti_vm_VM_isBootstrapClassLoader(J9VMThread *currentThread, j9object_t classLoaderObject)
{
	jboolean isBoot = JNI_FALSE;
	if (NULL == classLoaderObject) {
		isBoot = JNI_TRUE;
	} else {
		J9ClassLoader *loader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, classLoaderObject);
		if (loader == currentThread->javaVM->systemClassLoader) {
			isBoot = JNI_TRUE;
		}
	}
	return isBoot;
}

J9_FAST_JNI_METHOD_TABLE(com_ibm_oti_vm_VM)
	J9_FAST_JNI_METHOD("findClassOrNull", "(Ljava/lang/String;Ljava/lang/ClassLoader;)Ljava/lang/Class;", Fast_java_lang_VMAccess_findClassOrNull,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("getClassPathEntryType", "(Ljava/lang/ClassLoader;I)I", Fast_com_ibm_oti_vm_VM_getClassPathEntryType,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("getCPIndexImpl", "(Ljava/lang/Class;)I", Fast_com_ibm_oti_vm_VM_getCPIndexImpl,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("initializeClassLoader", "(Ljava/lang/ClassLoader;IZ)V", Fast_com_ibm_oti_vm_VM_initializeClassLoader,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("isBootstrapClassLoader", "(Ljava/lang/ClassLoader;)Z", Fast_com_ibm_oti_vm_VM_isBootstrapClassLoader,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
J9_FAST_JNI_METHOD_TABLE_END

}
