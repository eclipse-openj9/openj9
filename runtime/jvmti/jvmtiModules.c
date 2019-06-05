/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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
#include "jvmtiHelpers.h"
#include "jvmti_internal.h"
#include "j9cp.h"
#include "j2sever.h"

#define JVMTI_PACKAGE_NAME_BUFFER_LENGTH 80

static void
charReplace(J9UTF8 * utf8, char oldChar, char newChar) {
	IDATA i = 0;
	for (;i < J9UTF8_LENGTH(utf8); i++) {
		if (oldChar == J9UTF8_DATA(utf8)[i]) {
			J9UTF8_DATA(utf8)[i] = newChar;
		}
	}
}

static jvmtiError
addModuleExportsOrOpens(jvmtiEnv* jvmtiEnv, jobject fromModule, const char* pkgName, jobject toModule, BOOLEAN exports)
{
	J9VMThread *currentThread = NULL;
	J9JavaVM *vm = JAVAVM_FROM_ENV(jvmtiEnv);
	jvmtiError rc = JVMTI_ERROR_NONE;
	ENSURE_PHASE_LIVE(jvmtiEnv);
	ENSURE_NON_NULL(fromModule);
	ENSURE_NON_NULL(toModule);
	ENSURE_NON_NULL(pkgName);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9Class *toInstanceClazz = NULL;
		J9Class *moduleClass = NULL;
		BOOLEAN isUnnamedOrOpen = FALSE;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		J9Class *fromInstanceClazz = NULL;
		j9object_t fromModuleObj = NULL;
		J9Module *fromJ9Module = NULL;
		jclass moduleJClass = NULL;
		JNIEnv *env = (JNIEnv *) currentThread;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		fromModuleObj = J9_JNI_UNWRAP_REFERENCE(fromModule);
		fromJ9Module = J9OBJECT_ADDRESS_LOAD(currentThread, fromModuleObj, vm->modulePointerOffset);
		fromInstanceClazz = J9OBJECT_CLAZZ(currentThread, fromModuleObj);
		toInstanceClazz = J9OBJECT_CLAZZ(currentThread, J9_JNI_UNWRAP_REFERENCE(toModule));
		moduleClass = J9VMJAVALANGMODULE_OR_NULL(vm);

		Assert_JVMTI_notNull(moduleClass);

		if (!isSameOrSuperClassOf(moduleClass, fromInstanceClazz)
			|| !isSameOrSuperClassOf(moduleClass, toInstanceClazz)
		) {
			rc = JVMTI_ERROR_INVALID_MODULE;
		} else if (J9_IS_J9MODULE_UNNAMED(vm, fromJ9Module) || J9_IS_J9MODULE_OPEN(fromJ9Module)) {
			isUnnamedOrOpen = TRUE;
		} else if (NULL != strchr(pkgName, '/')) {
			rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
		} else  {
			IDATA len = strlen(pkgName);
			J9UTF8 *pkgNameUTF8 = NULL;
			char buf[JVMTI_PACKAGE_NAME_BUFFER_LENGTH + sizeof(pkgNameUTF8->length) + 1];
			PORT_ACCESS_FROM_VMC(currentThread);
			J9Module *foundModule = NULL;

			pkgNameUTF8 = (J9UTF8 *) buf;
			if (JVMTI_PACKAGE_NAME_BUFFER_LENGTH < len) {
				pkgNameUTF8 = j9mem_allocate_memory(len + sizeof(pkgNameUTF8->length) + 1, OMRMEM_CATEGORY_VM);
				if (NULL == pkgNameUTF8) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
					vmFuncs->internalExitVMToJNI(currentThread);
					goto done;
				}
			}

			J9UTF8_SET_LENGTH(pkgNameUTF8, (U_16) len);
			memcpy(J9UTF8_DATA(pkgNameUTF8), pkgName, len);
			charReplace(pkgNameUTF8, '.', '/');

			foundModule = vmFuncs->findModuleForPackageUTF8(currentThread, fromJ9Module->classLoader, pkgNameUTF8);

			if (foundModule != fromJ9Module) {
				rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
			}
			if (pkgNameUTF8 != (J9UTF8 *) buf) {
				j9mem_free_memory(pkgNameUTF8);
			}

			moduleJClass = (jclass) vmFuncs->j9jni_createLocalRef(env, moduleClass->classObject);
		}
		vmFuncs->internalExitVMToJNI(currentThread);
		if ((JVMTI_ERROR_NONE == rc) && (FALSE == isUnnamedOrOpen)) {
			jstring pkg = NULL;
			if (NULL == vm->addExports) {
				jmethodID addExports = (*env)->GetMethodID(env, moduleJClass, "implAddExportsOrOpens", "(Ljava/lang/String;Ljava/lang/Module;ZZ)V");

				if (NULL == addExports) {
					rc = JVMTI_ERROR_INTERNAL;
					goto done;
				}

				vm->addExports = addExports;
			}
			pkg = (*env)->NewStringUTF(env, pkgName);
			if (NULL == pkg) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				(*env)->CallObjectMethod(env,
										fromModule,
										vm->addExports,
										pkg,
										toModule,
										!exports,
										JNI_TRUE);
			}
			if ((*env)->ExceptionOccurred(env)) {
				rc = JVMTI_ERROR_INTERNAL;
			}
		}
	}
done:
	return rc;
}

/**
 * Return all the modules in the JVM
 *
 * @param [in] env pointer to JVMTIEnv
 * @param [out] module_count_ptr pointer to module count
 * @param [out] modules_ptr pointer to array of modules
 *
 * @return jvmtiError, JVMTI_ERROR_NONE on success, non-zero on failure
 */
jvmtiError JNICALL
jvmtiGetAllModules(jvmtiEnv* jvmtiEnv, jint* module_count_ptr, jobject** modules_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(jvmtiEnv);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jint rv_module_count = 0;
	jobject *rv_modules = NULL;

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		J9ClassLoaderWalkState walkState;
		PORT_ACCESS_FROM_JAVAVM(vm);
		J9ClassLoader *classLoader = NULL;
		I_32 moduleCount = 0;
		jobject *modules = NULL;
		UDATA i = 0;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(jvmtiEnv);
		ENSURE_NON_NULL(module_count_ptr);
		ENSURE_NON_NULL(modules_ptr);

#ifdef J9VM_THR_PREEMPTIVE
		omrthread_monitor_enter(vm->classLoaderBlocksMutex);
		omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
#endif
		/* count the number of modules */
		classLoader = vmFuncs->allClassLoadersStartDo(&walkState, vm, 0);
		while (NULL != classLoader) {
			/* unnamedModule in the ClassLoader */
			moduleCount += 1;
			/* name modules in the ClassLoader */
			moduleCount += (I_32) hashTableGetCount(classLoader->moduleHashTable);

			classLoader= vmFuncs->allClassLoadersNextDo(&walkState);
		}
		vmFuncs->allClassLoadersEndDo(&walkState);

		/* the bootClassLoader unnamedModule is counted twice because it is an
		 * unnamed module that is also in the module Hashable */
		moduleCount -= 1;

		modules = (jobject*) j9mem_allocate_memory(sizeof(UDATA) * moduleCount, J9MEM_CATEGORY_JVMTI_ALLOCATE);
		if (NULL == modules) {
			rc = JVMTI_ERROR_OUT_OF_MEMORY;
		} else {
			classLoader = vmFuncs->allClassLoadersStartDo(&walkState, vm, 0);
			while (NULL != classLoader) {
				J9Module** modulePtr = NULL;
				J9HashTableState moduleWalkState;

				/* the unnamed module for the systemLoader is also in the hashtable so ignore it here */
				if (vm->systemClassLoader != classLoader) {
					/* unnamedModule in the ClassLoader */
					modules[i++] = (jobject)vmFuncs->j9jni_createLocalRef(
												(JNIEnv *) currentThread,
												J9VMJAVALANGCLASSLOADER_UNNAMEDMODULE(currentThread, classLoader->classLoaderObject));
				}

				/* named modules in the ClassLoader */
				modulePtr = (J9Module**)hashTableStartDo(classLoader->moduleHashTable, &moduleWalkState);
				while (NULL != modulePtr) {
					modules[i++] = (jobject)vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, (*modulePtr)->moduleObject);
					modulePtr = (J9Module**)hashTableNextDo(&moduleWalkState);
				}

				classLoader = vmFuncs->allClassLoadersNextDo(&walkState);
			}
			vmFuncs->allClassLoadersEndDo(&walkState);
			Assert_JVMTI_true(i == moduleCount);

			rv_module_count = moduleCount;
			rv_modules = modules;
		}


#ifdef J9VM_THR_PREEMPTIVE
		omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
		omrthread_monitor_exit(vm->classLoaderBlocksMutex);
#endif
done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}
	if (NULL != module_count_ptr) {
		*module_count_ptr = rv_module_count;
	}
	if (NULL != modules_ptr) {
		*modules_ptr = rv_modules;
	}
	return rc;
}

/**
 * Return the module object for a named module associated with
 * classloader that contains the package name
 *
 * @param [in] env pointer to JVMTIEnv
 * @param [in] class_loader classLoader owning module
 * @param [in] package_name package name, must not contain '.' only '/'
 * @param [out] module_ptr pointer to module
 *
 * @return jvmtiError, JVMTI_ERROR_NONE on success, non-zero on failure
 */
jvmtiError JNICALL
jvmtiGetNamedModule(jvmtiEnv* jvmtiEnv, jobject class_loader, const char* package_name, jobject* module_ptr)
{
	J9JavaVM *vm = JAVAVM_FROM_ENV(jvmtiEnv);
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread *currentThread = NULL;
	jobject rv_module = NULL;

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		J9ClassLoader *vmClassLoader = NULL;
		J9Module *vmModule = NULL;
		J9UTF8 *packageName = NULL;
		char buf[J9VM_PACKAGE_NAME_BUFFER_LENGTH + sizeof(packageName->length) + 1];
		UDATA packageNameLength = 0;
		PORT_ACCESS_FROM_JAVAVM(vm);

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(jvmtiEnv);
		ENSURE_NON_NULL(module_ptr);
		ENSURE_NON_NULL(package_name);

		*module_ptr = NULL;

		if (NULL == class_loader) {
			vmClassLoader = vm->systemClassLoader;
		} else {
			j9object_t thisClassloader = NULL;
			J9Class *thisClassLoaderClass = NULL;
			J9Class *classLoaderClass = J9VMJAVALANGCLASSLOADER_OR_NULL(vm);
			thisClassloader = J9_JNI_UNWRAP_REFERENCE(class_loader);
			thisClassLoaderClass = J9OBJECT_CLAZZ(currentThread, thisClassloader);

			if (!isSameOrSuperClassOf(classLoaderClass, thisClassLoaderClass)) {
				rc = JVMTI_ERROR_ILLEGAL_ARGUMENT;
				goto done;
			}

			vmClassLoader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, thisClassloader);
		}

		packageNameLength = strlen(package_name);
		packageName = (J9UTF8 *) buf;
		if (packageNameLength > JVMTI_PACKAGE_NAME_BUFFER_LENGTH) {
			packageName = j9mem_allocate_memory(packageNameLength + sizeof(packageName->length) + 1, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (NULL == packageName) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
				goto done;
			}
		}

		J9UTF8_SET_LENGTH(packageName, (U_16) packageNameLength);
		memcpy(J9UTF8_DATA(packageName), package_name, packageNameLength);
		J9UTF8_DATA(packageName)[packageNameLength] = '\0';
		vmModule = vmFuncs->findModuleForPackageUTF8(currentThread, vmClassLoader, packageName);
		if (NULL != vmModule) {
			rv_module = (jobject)vmFuncs->j9jni_createLocalRef((JNIEnv *) currentThread, vmModule->moduleObject);
		}
		if (packageName != (J9UTF8 *) buf) {
			j9mem_free_memory(packageName);
		}
done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}
	if (NULL != module_ptr) {
		*module_ptr = rv_module;
	}
	return rc;
}

/**
 * Add a read edge fromModule to toModule. This is a noop if fromModule is unnamed.
 *
 * @param [in] fromModule module being read
 * @param [in] toModule module receiving read access
 *
 * @return JVMTI_ERROR_NONE if successful, JVMTI_ERROR_INVALID_MODULE if either module is
 * invalid and JVMTI_ERROR_NULL_POINTER if either module is NULL
 */
jvmtiError JNICALL
jvmtiAddModuleReads(jvmtiEnv* jvmtiEnv, jobject fromModule, jobject toModule)
{
	J9VMThread *currentThread = NULL;
	J9JavaVM *vm = JAVAVM_FROM_ENV(jvmtiEnv);
	jvmtiError rc = JVMTI_ERROR_NONE;
	ENSURE_PHASE_LIVE(jvmtiEnv);
	ENSURE_NON_NULL(fromModule);
	ENSURE_NON_NULL(toModule);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		J9Class *fromInstanceClazz = NULL;
		J9Class *toInstanceClazz = NULL;
		J9Class *moduleClass = NULL;
		BOOLEAN isUnnamed = FALSE;
		j9object_t fromModuleObj = NULL;
		J9Module *fromJ9Module = NULL;
		jclass moduleJClass = NULL;
		JNIEnv *env = (JNIEnv *) currentThread;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		fromModuleObj = J9_JNI_UNWRAP_REFERENCE(fromModule);
		fromJ9Module = J9OBJECT_ADDRESS_LOAD(currentThread, fromModuleObj, vm->modulePointerOffset);
		fromInstanceClazz = J9OBJECT_CLAZZ(currentThread, fromModuleObj);
		toInstanceClazz = J9OBJECT_CLAZZ(currentThread, J9_JNI_UNWRAP_REFERENCE(toModule));
		moduleClass = J9VMJAVALANGMODULE_OR_NULL(vm);

		Assert_JVMTI_notNull(moduleClass);

		if (!isSameOrSuperClassOf(moduleClass, fromInstanceClazz)
			|| !isSameOrSuperClassOf(moduleClass, toInstanceClazz)
		) {
			rc = JVMTI_ERROR_INVALID_MODULE;
		} else if (J9_IS_J9MODULE_UNNAMED(vm, fromJ9Module)) {
			isUnnamed = TRUE;
		}
		moduleJClass = (jclass) vmFuncs->j9jni_createLocalRef(env, moduleClass->classObject);

		vmFuncs->internalExitVMToJNI(currentThread);

		if ((JVMTI_ERROR_NONE == rc) && (FALSE == isUnnamed)) {
			JNIEnv *env = (JNIEnv *) currentThread;

			if (NULL == vm->addReads) {
				jmethodID addReads = (*env)->GetMethodID(env, moduleJClass, "implAddReads", "(Ljava/lang/Module;Z)V");

				if (NULL == addReads) {
					rc = JVMTI_ERROR_INTERNAL;
					goto done;
				}

				vm->addReads = addReads;
			}
			(*env)->CallObjectMethod(env, fromModule, vm->addReads, toModule, JNI_TRUE);
			if ((*env)->ExceptionOccurred(env)) {
				rc = JVMTI_ERROR_INTERNAL;
			}
		}
	}
done:
	return rc;
}


/**
 * Export package in fromModule to toModule. This is a noop if fromModule is unnamed or open.
 *
 * @param [in] fromModule module containing package
 * @param [in] pkgName name must not contain '/' only '.'
 * @param [in] toModule module receiving access to package
 *
 * @return JVMTI_ERROR_NONE if successful, JVMTI_ERROR_INVALID_MODULE if either module is
 * invalid and JVMTI_ERROR_NULL_POINTER if either module is NULL
 */
jvmtiError JNICALL
jvmtiAddModuleExports(jvmtiEnv* jvmtiEnv, jobject fromModule, const char* pkgName, jobject toModule)
{
	return addModuleExportsOrOpens(jvmtiEnv, fromModule, pkgName, toModule, TRUE);
}

/**
 * Open package in fromModule to toModule. This is a noop if fromModule is unnamed or open.
 *
 * @param [in] fromModule module containing package
 * @param [in] pkgName name must not contain '/' only '.'
 * @param [in] toModule module receiving access to package
 *
 * @return JVMTI_ERROR_NONE if successful, JVMTI_ERROR_INVALID_MODULE if either module is
 * invalid and JVMTI_ERROR_NULL_POINTER if either module is NULL
 */
jvmtiError JNICALL
jvmtiAddModuleOpens(jvmtiEnv* jvmtiEnv, jobject fromModule, const char* pkgName, jobject toModule)
{

	return addModuleExportsOrOpens(jvmtiEnv, fromModule, pkgName, toModule, FALSE);
}

/**
 * Add service to the set of services that the module uses. NOOP if its
 * an unnamed module
 *
 * @param [in] env pointer to JVMTIEnv
 * @param [in] module module owning service
 * @param [in] service service to be added
 *
 * @return jvmtiError, JVMTI_ERROR_NONE on success, non-zero on failure
 */
jvmtiError JNICALL
jvmtiAddModuleUses(jvmtiEnv* jvmtiEnv, jobject module, jclass service)
{
	J9VMThread *currentThread = NULL;
	J9JavaVM *vm = JAVAVM_FROM_ENV(jvmtiEnv);
	jvmtiError rc = JVMTI_ERROR_NONE;
	ENSURE_PHASE_LIVE(jvmtiEnv);
	ENSURE_NON_NULL(module);
	ENSURE_NON_NULL(service);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		j9object_t moduleObject = NULL;
		j9object_t serviceClassObject = NULL;
		J9Class *moduleJ9Class = NULL;
		J9Class *jlClass = NULL;
		BOOLEAN isUnnamed = FALSE;
		jclass moduleJClass = NULL;
		JNIEnv *env = (JNIEnv *) currentThread;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		moduleObject = J9_JNI_UNWRAP_REFERENCE(module);
		serviceClassObject = J9_JNI_UNWRAP_REFERENCE(service);
		moduleJ9Class = J9VMJAVALANGMODULE_OR_NULL(vm);
		jlClass = J9VMJAVALANGCLASS_OR_NULL(vm);

		Assert_JVMTI_notNull(moduleJ9Class);
		Assert_JVMTI_notNull(jlClass);

		if (!isSameOrSuperClassOf(moduleJ9Class, J9OBJECT_CLAZZ(currentThread, moduleObject))) {
			rc = JVMTI_ERROR_INVALID_MODULE;
		} else if (!isSameOrSuperClassOf(jlClass, J9OBJECT_CLAZZ(currentThread, serviceClassObject))) {
			rc = JVMTI_ERROR_INVALID_CLASS;
		} else if (J9_IS_J9MODULE_UNNAMED(vm, J9OBJECT_ADDRESS_LOAD(currentThread, moduleObject, vm->modulePointerOffset))) {
			isUnnamed = TRUE;
		}

		moduleJClass = (jclass) vmFuncs->j9jni_createLocalRef(env, moduleJ9Class->classObject);

		vmFuncs->internalExitVMToJNI(currentThread);

		if ((rc == JVMTI_ERROR_NONE) && (FALSE == isUnnamed)) {
			JNIEnv *env = (JNIEnv *) currentThread;
			if (NULL == vm->addUses) {
				jmethodID addUses = NULL;

				addUses = (*env)->GetMethodID(env, moduleJClass, "implAddUses", "(Ljava/lang/Class;)V");
				if (NULL == addUses) {
					rc = JVMTI_ERROR_INTERNAL;
					goto done;
				}

				vm->addUses = addUses;
			}
			(*env)->CallObjectMethod(env, module, vm->addUses, service);
			if ((*env)->ExceptionOccurred(env)) {
				rc = JVMTI_ERROR_INTERNAL;
			}
		}
	}

done:
	return rc;
}

/**
 * Add service to the set of services that the module provides. NOOP if its
 * an unnamed module
 *
 * @param [in] env pointer to JVMTIEnv
 * @param [in] module module being updated
 * @param [in] service service to be added
 * @param [in] implClass implementer of the service
 *
 * @return jvmtiError, JVMTI_ERROR_NONE on success, non-zero on failure
 */
jvmtiError JNICALL
jvmtiAddModuleProvides(jvmtiEnv* jvmtiEnv,
		jobject module,
		jclass service,
		jclass implClass)
{
	J9VMThread *currentThread = NULL;
	J9JavaVM *vm = JAVAVM_FROM_ENV(jvmtiEnv);
	jvmtiError rc = JVMTI_ERROR_NONE;
	ENSURE_PHASE_LIVE(jvmtiEnv);
	ENSURE_NON_NULL(module);
	ENSURE_NON_NULL(service);
	ENSURE_NON_NULL(implClass);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		j9object_t moduleObject = NULL;
		j9object_t serviceClassObject = NULL;
		j9object_t implClassObject = NULL;
		J9Class *moduleJ9Class = NULL;
		J9Class *jlClass = NULL;
		BOOLEAN isUnnamed = FALSE;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		moduleObject = J9_JNI_UNWRAP_REFERENCE(module);
		serviceClassObject = J9_JNI_UNWRAP_REFERENCE(service);
		implClassObject = J9_JNI_UNWRAP_REFERENCE(implClass);
		moduleJ9Class = J9VMJAVALANGMODULE_OR_NULL(vm);
		jlClass = J9VMJAVALANGCLASS_OR_NULL(vm);

		Assert_JVMTI_notNull(moduleJ9Class);
		Assert_JVMTI_notNull(jlClass);

		if (!isSameOrSuperClassOf(moduleJ9Class, J9OBJECT_CLAZZ(currentThread, moduleObject))) {
			rc = JVMTI_ERROR_INVALID_MODULE;
		} else if (!isSameOrSuperClassOf(jlClass, J9OBJECT_CLAZZ(currentThread, serviceClassObject))
			|| !isSameOrSuperClassOf(jlClass, J9OBJECT_CLAZZ(currentThread, implClassObject))
		) {
			rc = JVMTI_ERROR_INVALID_CLASS;
		} else if (J9_IS_J9MODULE_UNNAMED(vm, J9OBJECT_ADDRESS_LOAD(currentThread, moduleObject, vm->modulePointerOffset))) {
			isUnnamed = TRUE;
		}

		vmFuncs->internalExitVMToJNI(currentThread);

		if ((rc == JVMTI_ERROR_NONE) && (FALSE == isUnnamed)) {
			JNIEnv *env = (JNIEnv *) currentThread;
			jclass jimModules = vmFuncs->getJimModules(currentThread);
			if (NULL == jimModules) {
				rc = JVMTI_ERROR_INTERNAL;
				goto done;
			}
			if (NULL == vm->addProvides) {
				jmethodID addProvides = (*env)->GetStaticMethodID(env, jimModules, "addProvides", "(Ljava/lang/Module;Ljava/lang/Class;Ljava/lang/Class;)V");

				if (NULL == addProvides) {
					rc = JVMTI_ERROR_INTERNAL;
					goto done;
				}

				vm->addProvides = addProvides;
			}
			(*env)->CallStaticVoidMethod(env, jimModules, vm->addProvides, module, service, implClass);
		}
	}

done:
	return rc;
}

/**
 * Determines whether a module is modifiable. Currently, no criteria
 * is specified to tag modules as modifiable. So, is_modifiable_module_ptr
 * is set to JNI_TRUE for all modules. In case of error, is_modifiable_module_ptr
 * is set to JNI_FALSE.
 *
 * @param [in]     env                      pointer to jvmtiEnv
 * @param [in]     module                   module being checked
 * @param [in/out] is_modifiable_module_ptr points to the boolean result
 *
 * @return jvmtiError, JVMTI_ERROR_NONE on success
 *         jvmtiError, JVMTI_ERROR_INVALID_MODULE if module is not a module object
 *         jvmtiError, JVMTI_ERROR_NULL_POINTER if module is NULL
 *         jvmtiError, JVMTI_ERROR_NULL_POINTER if is_modifiable_module_ptr is NULL
 *         jvmtiError, one of the universal errors
 */
jvmtiError JNICALL
jvmtiIsModifiableModule(jvmtiEnv* env,
		jobject module,
		jboolean* is_modifiable_module_ptr)
{
	J9VMThread *currentThread = NULL;
	J9JavaVM *vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc = JVMTI_ERROR_NONE;
	jboolean rv_is_modifiable_module = JNI_FALSE;

	ENSURE_PHASE_LIVE(env);
	ENSURE_NON_NULL(module);
	ENSURE_NON_NULL(is_modifiable_module_ptr);

	rc = getCurrentVMThread(vm, &currentThread);
	if (JVMTI_ERROR_NONE == rc) {
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		j9object_t moduleObject = NULL;
		J9Class *moduleJ9Class = NULL;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		moduleObject = J9_JNI_UNWRAP_REFERENCE(module);
		moduleJ9Class = J9VMJAVALANGMODULE_OR_NULL(vm);

		Assert_JVMTI_notNull(moduleJ9Class);

		if (!isSameOrSuperClassOf(moduleJ9Class, J9OBJECT_CLAZZ(currentThread, moduleObject))) {
			rc = JVMTI_ERROR_INVALID_MODULE;
		} else if (J9_IS_J9MODULE_UNNAMED(vm, J9OBJECT_ADDRESS_LOAD(currentThread, moduleObject, vm->modulePointerOffset))) {
			rv_is_modifiable_module = JNI_TRUE;
		} else {
			/* No criteria specified to tag modules as unmodifiable.
			 * Assuming all modules are modifiable at this point.
			 * Thus, setting is_modifiable_module_ptr to JNI_TRUE for all modules.
			 * REMINDER: Track future changes to the definition of jvmtiIsModifiableModule,
			 * and appropriately update the implementation.
			 */
			rv_is_modifiable_module = JNI_TRUE;
		}

		vmFuncs->internalExitVMToJNI(currentThread);
	}

done:
	if (NULL != is_modifiable_module_ptr) {
		*is_modifiable_module_ptr = rv_is_modifiable_module;
	}
	return rc;
}
