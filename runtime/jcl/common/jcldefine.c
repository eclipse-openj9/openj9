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

#include "j9.h"
#include "j9consts.h"
#include "jclprots.h"
#include "j9protos.h"



jclass 
defineClassCommon(JNIEnv *env, jobject classLoaderObject,
	jstring className, jbyteArray classRep, jint offset, jint length, jobject protectionDomain, UDATA options, J9Class *hostClass)
{
#ifdef J9VM_OPT_DYNAMIC_LOAD_SUPPORT

/* Try a couple of GC passes (1 doesn't sem to be enough), but don't try forever */
#define MAX_RETRY_COUNT 2 

	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9TranslationBufferSet *dynFuncs;
	J9ClassLoader *classLoader;
	UDATA retried = FALSE;
	UDATA utf8Length;
	U_8 *utf8Name;
	U_8 *classBytes = NULL;
	J9Class *clazz = NULL;
	jclass result;
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA isContiguousClassBytes = 0;
	J9ROMClass *loadedClass = NULL;
	U_8 *tempClassBytes = NULL;
	I_32 tempLength = 0;
	J9TranslationLocalBuffer localBuffer = {J9_CP_INDEX_NONE, LOAD_LOCATION_UNKNOWN, NULL};

	if (vm->dynamicLoadBuffers == NULL) {
		throwNewInternalError(env, "Dynamic loader is unavailable");
		return NULL;
	}
	dynFuncs = vm->dynamicLoadBuffers;

	if (classRep == NULL) {
		throwNewNullPointerException(env, NULL);
		return NULL;
	}

	vmFuncs->internalEnterVMFromJNI(currentThread);
	isContiguousClassBytes = J9ISCONTIGUOUSARRAY(vm, *(J9IndexableObject **)classRep);
	if (!isContiguousClassBytes) {
		vmFuncs->internalReleaseVMAccess(currentThread);
		/* Make a "flat" copy of classRep */
		if (length < 0) {
			throwNewIndexOutOfBoundsException(env, NULL);
			return NULL;
		}
		classBytes = j9mem_allocate_memory(length, J9MEM_CATEGORY_CLASSES);
		if (classBytes == NULL) {
			throwNativeOOMError(env, 0, 0);
			return NULL;
		}
		(*env)->GetByteArrayRegion(env, classRep, offset, length, (jbyte *)classBytes);
		if ((*env)->ExceptionCheck(env)) {
			j9mem_free_memory(classBytes);
			return NULL;
		}
		vmFuncs->internalEnterVMFromJNI(currentThread);
	}

	/* Allocate and initialize a UTF8 copy of the Unicode class-name */
	if (className == NULL) {
		utf8Length = 0;
		utf8Name = NULL;
	} else {
		utf8Length = vmFuncs->getStringUTF8Length(currentThread, J9_JNI_UNWRAP_REFERENCE(className));
		/* For now, use malloc, but this should use stack allocation for short class-names */
		utf8Name = j9mem_allocate_memory(utf8Length+1, J9MEM_CATEGORY_CLASSES);
		if (utf8Name == NULL) {
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
		if (UDATA_MAX == vmFuncs->copyStringToUTF8Helper(
			currentThread, J9_JNI_UNWRAP_REFERENCE(className), TRUE, J9_STR_XLAT, utf8Name, utf8Length + 1)
		) {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			goto done;
		}
	}

	if (isContiguousClassBytes) {
		/* For ARRAYLETS case, we get free range checking from GetByteArrayRegion JNI call */
		if ((offset < 0) || (length < 0) || 
	      (((U_32)offset + (U_32)length) > J9INDEXABLEOBJECT_SIZE(currentThread, *(J9IndexableObject **)classRep))) {
			vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGINDEXOUTOFBOUNDSEXCEPTION, NULL);
			goto done;
		}
	}

	classLoader = J9VMJAVALANGCLASSLOADER_VMREF(currentThread, J9_JNI_UNWRAP_REFERENCE(classLoaderObject));

	if (NULL == classLoader) {
		classLoader = vmFuncs->internalAllocateClassLoader(vm, J9_JNI_UNWRAP_REFERENCE(classLoaderObject));
		if (NULL == classLoader) {
			goto done;
		}
	}

retry:

	omrthread_monitor_enter(vm->classTableMutex);

	if (vmFuncs->hashClassTableAt(classLoader, utf8Name, utf8Length) != NULL) {
		/* Bad, we have already defined this class - fail */
		omrthread_monitor_exit(vm->classTableMutex);
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, (UDATA *)*(j9object_t*)className);
		goto done;
	}

	if (isContiguousClassBytes) {
		/* Always re-load the classBytes pointer, as GC required in OutOfMemory case may have moved things */
		/* TMP_J9JAVACONTIGUOUSARRAYOFBYTE_EA returns I_8 * (since Java bytes are I_8) so this cast will silence the warning */
		classBytes = (U_8 *) TMP_J9JAVACONTIGUOUSARRAYOFBYTE_EA(currentThread, *(J9IndexableObject **)classRep, offset);
	}

	tempClassBytes = classBytes;
	tempLength = length;

	/* skip if we are anonClass */
	if (J9_ARE_NO_BITS_SET(options, J9_FINDCLASS_FLAG_ANON)) {
		/* Check for romClass cookie, it indicates that we are  defining a class out of a JXE not from class bytes */

		loadedClass = vmFuncs->romClassLoadFromCookie(currentThread, utf8Name, utf8Length, classBytes, (UDATA) length);

		if (NULL != loadedClass) {
			/* An existing ROMClass is found in the shared class cache.
			 * If -Xshareclasses:enableBCI is present, need to give VM a chance to trigger ClassFileLoadHook event.
			 */
			if ((NULL == vm->sharedClassConfig) || (0 == vm->sharedClassConfig->isBCIEnabled(vm))) {
				clazz = vmFuncs->internalCreateRAMClassFromROMClass(currentThread,
																			classLoader,
																			loadedClass,
																			0,
																			NULL,
																			protectionDomain ? *(j9object_t*)protectionDomain : NULL,
																			NULL,
																			J9_CP_INDEX_NONE,
																			LOAD_LOCATION_UNKNOWN,
																			NULL,
																			hostClass);
				/* Done if a class was found or and exception is pending, otherwise try to define the bytes */
				if ((clazz != NULL) || (currentThread->currentException != NULL)) {
					goto done;
				} else {
					loadedClass = NULL;
				}
			} else {
				tempClassBytes = J9ROMCLASS_INTERMEDIATECLASSDATA(loadedClass);
				tempLength = loadedClass->intermediateClassDataLength;
				options |= J9_FINDCLASS_FLAG_SHRC_ROMCLASS_EXISTS;
			}
		}
	}

	/* The defineClass helper requires you hold the class table mutex and releases it for you */
	
	clazz = dynFuncs->internalDefineClassFunction(currentThread, 
                                              utf8Name, utf8Length,
                                              tempClassBytes, (UDATA) tempLength, NULL, 
                                              classLoader, 
                                              protectionDomain ? *(j9object_t*)protectionDomain : NULL,
                                              options | J9_FINDCLASS_FLAG_THROW_ON_FAIL | J9_FINDCLASS_FLAG_NO_CHECK_FOR_EXISTING_CLASS,
                                              loadedClass,
											  hostClass,
											  &localBuffer);

	/* If OutOfMemory, try a GC to free up some memory */
	
	if (currentThread->privateFlags & J9_PRIVATE_FLAGS_CLOAD_NO_MEM) {
		if (!retried) {
			/*Trc_VM_internalFindClass_gcAndRetry(vmThread);*/
			currentThread->javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(currentThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
			retried = TRUE;
			goto retry;
		}
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
	}

done:
	if ((clazz == NULL) && (currentThread->currentException == NULL)) {
		/* should not get here -- throw the default exception just in case */
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGCLASSFORMATERROR, NULL);
	}
			
	result = vmFuncs->j9jni_createLocalRef(env, J9VM_J9CLASS_TO_HEAPCLASS(clazz));

	vmFuncs->internalReleaseVMAccess(currentThread);

	j9mem_free_memory(utf8Name);

	if (!isContiguousClassBytes) {
		j9mem_free_memory(classBytes);
	}

	return result;

#else /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */
	throwNewInternalError(env, "Dynamic loading not supported");
	return NULL;
#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */
}
