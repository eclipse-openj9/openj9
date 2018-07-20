/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
#include "j9protos.h"
#include "j9consts.h"
#include "j2sever.h"
#include "jvminit.h"
#include "zipsup.h"
#include "zip_api.h"
#include "ut_j9vm.h"
#include "classloadersearch.h"


static UDATA addToSystemProperty(J9JavaVM * vm, const char * propertyName, const char * segment);
static UDATA addZipToLoader(J9JavaVM * vm, const char * filename, J9ClassLoader * classLoader, BOOLEAN enforceJarRestriction);
#define INSTRUMENT_MODULE "java.instrument"
static UDATA loadModule(J9JavaVM *vm, const char *moduleName, jobject *modulePtr);


/**
 * @brief Append a single pathSegment to the bootstrap classloader
 * @param *vm
 * @param *pathSegment				a single path segment specifying the path to be added
 * @param classLoaderType			bits specifying which class loaders the path should be added to
 * @param enforceJarRestriction 	when true, fail if the pathSegment is not a jar file and we are adding to the system classloader
 * @return error code
 */
UDATA
addToBootstrapClassLoaderSearch(J9JavaVM * vm, const char * pathSegment,
		UDATA classLoaderType, BOOLEAN enforceJarRestriction)
{
	UDATA rc = CLS_ERROR_NONE;

	Trc_VM_addToBootstrapClassLoaderSearch_Entry(pathSegment);

	if (pathSegment == NULL) {
		return CLS_ERROR_NULL_POINTER;
	}

	if (classLoaderType & CLS_TYPE_ADD_TO_SYSTEM_PROPERTY) {
		if ((J2SE_VERSION(vm) & J2SE_VERSION_MASK) < J2SE_19) {
			rc = addToSystemProperty(vm, BOOT_PATH_SYS_PROP, pathSegment);
		} else {
			rc = addToSystemProperty(vm, BOOT_CLASS_PATH_APPEND_PROP, pathSegment);
		}
		if (rc != CLS_ERROR_NONE) {
			goto done;
		}
	}

	if (classLoaderType & CLS_TYPE_ADD_TO_SYSTEM_CLASSLOADER) {
		rc = addZipToLoader(vm, pathSegment, vm->systemClassLoader, enforceJarRestriction);
		if (rc != CLS_ERROR_NONE) {
			goto done;
		}
	}

done:
	Trc_VM_addToBootstrapClassLoaderSearch_Exit();

	return rc;
}



/**
 * @brief Append a single pathSegment to the system classloader
 * @param *vm
 * @param *pathSegment	a single path segment specifying the path to be added
 * @param enforceJarRestriction 	when true, fail if the pathSegment is not a jar file
 *
 * @return error code
 */
UDATA
addToSystemClassLoaderSearch(J9JavaVM * vm, const char* pathSegment,
		UDATA classLoaderType, BOOLEAN enforceJarRestriction)
{
	UDATA rc = CLS_ERROR_NONE;

	Trc_VM_addToSystemClassLoaderSearch_Entry(pathSegment);

	if (pathSegment == NULL) {
		return CLS_ERROR_NULL_POINTER;
	}

	if (classLoaderType & CLS_TYPE_ADD_TO_SYSTEM_PROPERTY) {
		rc = addToSystemProperty(vm, "java.class.path", pathSegment);
		if (rc != CLS_ERROR_NONE) {
			goto done;
		}
	}

	if (classLoaderType & CLS_TYPE_ADD_TO_SYSTEM_CLASSLOADER) {
		rc = addZipToLoader(vm, pathSegment, vm->applicationClassLoader, enforceJarRestriction);
		if (rc != CLS_ERROR_NONE) {
			goto done;
		}
	}

done:
	Trc_VM_addToSystemClassLoaderSearch_Exit();

	return rc;
}



static UDATA
addToSystemProperty(J9JavaVM * vm, const char * propertyName, const char * segment)
{
	J9VMSystemProperty * systemProperty;
	UDATA rc = CLS_ERROR_NONE;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Use the system classloader mutex as we are dealing with the boot classpaths */

	omrthread_monitor_enter(vm->systemPropertiesMutex);

	if (getSystemProperty(vm, propertyName, &systemProperty) == J9SYSPROP_ERROR_NONE) {
		char * newValue;
		UDATA currentLength = strlen(systemProperty->value);

		newValue = j9mem_allocate_memory(currentLength + strlen(segment) + 2, OMRMEM_CATEGORY_VM);	/* +1 for nul, +1 for separator */
		if (newValue == NULL) {
			rc = CLS_ERROR_OUT_OF_MEMORY;
		} else {
			strcpy(newValue, systemProperty->value);
			if (currentLength != 0) {
				newValue[currentLength] = (char) j9sysinfo_get_classpathSeparator();
				newValue[currentLength + 1] = '\0';
			}
			strcat(newValue, segment);
			setSystemProperty(vm, systemProperty, newValue);
			j9mem_free_memory(newValue);
		}
	} else {
		rc = CLS_ERROR_INTERNAL;
	}

	omrthread_monitor_exit(vm->systemPropertiesMutex);

	return rc;
}

static UDATA
addZipToLoader(J9JavaVM * vm, const char * filename, J9ClassLoader * classLoader, BOOLEAN enforceJarRestriction)
{
	I_32 zipResult;
	J9ZipFile zipFile;
	J9VMThread * currentThread;
	UDATA rc = CLS_ERROR_NONE;

	/* Handle JCLs which don't have class loaders */

	if (classLoader == NULL) {
		return CLS_ERROR_CLASS_LOADER_UNSUPPORTED;
	}

	/* Validate the zip file */
	if (enforceJarRestriction == TRUE) {
		zipResult = zip_openZipFile(vm->portLibrary, (char *) filename, &zipFile, NULL, J9ZIP_OPEN_NO_FLAGS);
		if (zipResult) {
			return CLS_ERROR_ILLEGAL_ARGUMENT;
		}
		zip_releaseZipFile(vm->portLibrary, &zipFile);
	}

	if ((classLoader == vm->systemClassLoader) && (J2SE_VERSION(vm) >= J2SE_19)) {
		if (0 == addJarToSystemClassLoaderClassPathEntries(vm, filename)) {
			rc = CLS_ERROR_OUT_OF_MEMORY;
		}
	} else {
		/* Call appendToClassPathForInstrumentation */
		currentThread = currentVMThread(vm);
		if (currentThread != NULL) {
			JNIEnv * env = (JNIEnv *) currentThread;
			jobject classLoaderRef = NULL;
			jstring filenameString = NULL;
			jclass classLoaderClass = NULL;
			jmethodID mid;

			internalEnterVMFromJNI(currentThread);
			classLoaderRef = j9jni_createLocalRef(env, classLoader->classLoaderObject);
			internalExitVMToJNI(currentThread);
			if (classLoaderRef == NULL) {
				rc = CLS_ERROR_OUT_OF_MEMORY;
				goto cleanup;
			}

			filenameString = (*env)->NewStringUTF(env, filename);
			if (filenameString == NULL) {
				rc = CLS_ERROR_OUT_OF_MEMORY;
				goto cleanup;
			}

			classLoaderClass = (*env)->GetObjectClass(env, classLoaderRef);
			if (classLoaderClass == NULL) {
				rc = CLS_ERROR_OUT_OF_MEMORY;
				goto cleanup;
			}
			mid = (*env)->GetMethodID(env, classLoaderClass, "appendToClassPathForInstrumentation", "(Ljava/lang/String;)V");
			if (mid == NULL) {
				rc = CLS_ERROR_CLASS_LOADER_UNSUPPORTED;
				goto cleanup;
			}

			(*env)->CallVoidMethod(env, classLoaderRef, mid, filenameString);
			if ((*env)->ExceptionCheck(env)) {
				rc = CLS_ERROR_OUT_OF_MEMORY;
			} else if (J2SE_SHAPE(vm) >= J2SE_SHAPE_B165) {
				rc = loadModule(vm, INSTRUMENT_MODULE, NULL);
			}
	cleanup:
			(*env)->ExceptionClear(env);
			(*env)->DeleteLocalRef(env, classLoaderRef);
			(*env)->DeleteLocalRef(env, filenameString);
			(*env)->DeleteLocalRef(env, classLoaderClass);
		}
	}

	return rc;
}

jclass
getJimModules(J9VMThread *currentThread)
{
	J9JavaVM *vm = NULL;
	vm = currentThread->javaVM;
	if (NULL == vm->jimModules) {
		const J9InternalVMFunctions *vmFuncs = NULL;
		/* no need for mutual exclusion since repeating this is harmless */
		J9Class *j9JimModules = NULL;
		internalEnterVMFromJNI(currentThread);
		vmFuncs = vm->internalVMFunctions;
		j9JimModules = vmFuncs->internalFindKnownClass(currentThread,
				J9VMCONSTANTPOOL_JDKINTERNALMODULEMODULES,
				J9_FINDKNOWNCLASS_FLAG_INITIALIZE | J9_FINDKNOWNCLASS_FLAG_NON_FATAL);
		if (NULL != j9JimModules) {
			/*
			 * The address of j9class->classObject is considered to be a JNI global reference.
			 * Note that the class in question is not a candidate for unloading.
			 */
			vm->jimModules = (jclass)&j9JimModules->classObject;
		} else {
			currentThread->currentException = NULL;
			currentThread->privateFlags &= ~J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
		}
		internalExitVMToJNI(currentThread);
	}
	return vm->jimModules;
}

/**
 * Loads the named module using jdk.internal.module.Modules.loadModule()
 *
 * @param [in] vm pointer to JVM
 * @param [in] moduleName name of module to load
 * @param [out] modulePtr pointer to return module object.  May be null.
 *
 * @return CLS_ERROR_NONE on success, other value on failure
 */
static UDATA
loadModule(J9JavaVM *vm, const char *moduleName, jobject *modulePtr)
{
	UDATA rc = CLS_ERROR_NONE;
	J9VMThread *currentThread = currentVMThread(vm);
	JNIEnv *env = (JNIEnv *) currentThread;
	jstring moduleNameString = (*env)->NewStringUTF(env, moduleName);
	jclass jimModules = getJimModules(currentThread);
	jobject vmModule = NULL;
	jmethodID loadModule = NULL;

	if (NULL == moduleNameString) {
		rc = CLS_ERROR_OUT_OF_MEMORY;
		goto done;
	}
	if (NULL == jimModules) {
		rc = CLS_ERROR_NOT_FOUND;
		goto done;
	}

	if (NULL != modulePtr) {
		*modulePtr = NULL;
	}

	loadModule = (*env)->GetStaticMethodID(env, jimModules, "loadModule", "(Ljava/lang/String;)Ljava/lang/Module;");
	if (NULL == loadModule) {
		rc = CLS_ERROR_NOT_FOUND;
		goto done;
	}

	vmModule = (*env)->CallStaticObjectMethod(env, jimModules, loadModule, moduleNameString);
	if ((*env)->ExceptionOccurred(env)) {
		rc = CLS_ERROR_INTERNAL;
	} else if (NULL != modulePtr) {
		*modulePtr = vmModule;
	}
done:
	return rc;
}
