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

#include "j9.h"
#include "jclprots.h"
#include "j9cp.h"
#include "j9protos.h"
#include "omrlinkedlist.h"
#include "ut_j9jcl.h"
#include "j9port.h"
#include "j2sever.h"
#include "jclglob.h"
#include "jcl_internal.h"
#include "vmhook.h"

#include <string.h>
#include <assert.h>

#include "ArrayCopyHelpers.hpp"
#include "AtomicSupport.hpp"
#include "ObjectMonitor.hpp"
#include "VMHelpers.hpp"

extern "C" {

#define BUFFER_SIZE 128

jclass JNICALL
Java_sun_misc_Unsafe_defineClass__Ljava_lang_String_2_3BIILjava_lang_ClassLoader_2Ljava_security_ProtectionDomain_2(
	JNIEnv *env, jobject receiver, jstring className, jbyteArray classRep, jint offset, jint length, jobject classLoader, jobject protectionDomain)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	UDATA defineClassOptions = J9_FINDCLASS_FLAG_UNSAFE;

	if (NULL == classLoader) {
		J9JavaVM *vm = currentThread->javaVM;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		j9object_t classLoaderObject = J9CLASSLOADER_CLASSLOADEROBJECT(currentThread, vm->systemClassLoader);

		classLoader = vmFuncs->j9jni_createLocalRef(env, classLoaderObject);
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	return defineClassCommon(env, classLoader, className, classRep, offset, length, protectionDomain, &defineClassOptions, NULL, NULL, FALSE);
}

jclass JNICALL
Java_sun_misc_Unsafe_defineAnonymousClass(JNIEnv *env, jobject receiver, jclass hostClass, jbyteArray bytecodes, jobjectArray constPatches)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	UDATA defineClassOptions = (J9_FINDCLASS_FLAG_UNSAFE | J9_FINDCLASS_FLAG_ANON);

	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == bytecodes) {
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

	j9object_t protectionDomain = J9VMJAVALANGCLASS_PROTECTIONDOMAIN(currentThread, hostClassObject);
	jobject protectionDomainLocalRef = vmFuncs->j9jni_createLocalRef(env, protectionDomain);
	j9object_t hostClassLoader = J9VMJAVALANGCLASS_CLASSLOADER(currentThread, hostClassObject);

	if (NULL == hostClassLoader) {
		/* A NULL classLoader entry means that it is loaded by the systemLoader */
		hostClassLoader = vm->systemClassLoader->classLoaderObject;
	}
	jobject hostClassLoaderLocalRef = vmFuncs->j9jni_createLocalRef(env, hostClassLoader);

	J9ClassPatchMap cpPatchMap = {0, NULL};
	PORT_ACCESS_FROM_ENV(env);

	U_16 indexMap[BUFFER_SIZE];
	if (constPatches != NULL) {
		j9array_t patchArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(constPatches);
		cpPatchMap.size = (U_16)J9INDEXABLEOBJECT_SIZE(currentThread, patchArray);
		if (cpPatchMap.size <= BUFFER_SIZE) {
			cpPatchMap.indexMap = indexMap;
		} else {
			cpPatchMap.indexMap = (U_16 *)j9mem_allocate_memory(cpPatchMap.size * sizeof(U_16), J9MEM_CATEGORY_VM);

			if (cpPatchMap.indexMap == NULL) {
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				vmFuncs->internalExitVMToJNI(currentThread);
				return NULL;
			}
		}
	}

	vmFuncs->internalExitVMToJNI(currentThread);

	jsize length = env->GetArrayLength(bytecodes);

	/* acquires access internally */
	jclass anonClass = defineClassCommon(env, hostClassLoaderLocalRef, NULL,bytecodes, 0, length, protectionDomainLocalRef, &defineClassOptions, hostClazz, &cpPatchMap, FALSE);
	if (env->ExceptionCheck()) {
		return NULL;
	} else if (NULL == anonClass) {
		throwNewInternalError(env, NULL);
		return NULL;
	}

	if (constPatches != NULL) {
		vmFuncs->internalEnterVMFromJNI(currentThread);
		j9array_t patchArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(constPatches);
		J9Class *clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, J9_JNI_UNWRAP_REFERENCE(anonClass));
		U_32 *cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(clazz->romClass);
		J9ConstantPool *ramCP = J9_CP_FROM_CLASS(clazz);
		J9ROMConstantPoolItem *romCP = ramCP->romConstantPool;

		/* Get J9 constantpool mapped item for patch item, only support patching STRING entries has been added */
		for (U_16 i = 0; i < cpPatchMap.size; i++) {
			j9object_t item = J9JAVAARRAYOFOBJECT_LOAD(currentThread, patchArray, i);
			if (item != NULL) {
				if (J9_CP_TYPE(cpShapeDescription, cpPatchMap.indexMap[i]) == J9CPTYPE_STRING) {

					J9UTF8 *romString = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)&romCP[cpPatchMap.indexMap[i]]);

					/* For each patch object, search the RAM constantpool for identical string entries */
					for (U_16 j = 1; j < clazz->romClass->ramConstantPoolCount; j++) {
						if ((J9_CP_TYPE(cpShapeDescription, j) == J9CPTYPE_STRING)
							&& J9UTF8_EQUALS(romString, J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)&romCP[j]))
						) {
							J9RAMStringRef *ramStringRef = ((J9RAMStringRef *)ramCP) + j;
							J9STATIC_OBJECT_STORE(currentThread, clazz, &ramStringRef->stringObject, item);
						}
					}
				} else {
					/* Only J9CPTYPE_STRING is patched, other CP types are not supported */
					Assert_JCL_unreachable();
				}
			}
		}

		if (cpPatchMap.size > BUFFER_SIZE) {
			j9mem_free_memory(cpPatchMap.indexMap);
			cpPatchMap.indexMap = NULL;
		}
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	return anonClass;
}

/**
 * Initialization function called during VM bootstrap (Unsafe.<clinit>).
 */
void JNICALL
Java_sun_misc_Unsafe_registerNatives(JNIEnv *env, jclass clazz)
{
	jfieldID fid;

	Trc_JCL_sun_misc_Unsafe_registerNatives_Entry(env);

	/* If Unsafe has a static int field called INVALID_FIELD_OFFSET, set it to -1 */

	fid = env->GetStaticFieldID(clazz, "INVALID_FIELD_OFFSET", "I");
	if (fid == NULL) {
		env->ExceptionClear();
	} else {
		env->SetStaticIntField(clazz, fid, -1);
	}

	Trc_JCL_sun_misc_Unsafe_registerNatives_Exit(env);
}


jint JNICALL
Java_sun_misc_Unsafe_pageSize(JNIEnv *env, jobject receiver)
{
	PORT_ACCESS_FROM_ENV(env);

	return (jint)j9vmem_supported_page_sizes()[0];
}


jint JNICALL
Java_sun_misc_Unsafe_getLoadAverage(JNIEnv *env, jobject receiver, jdoubleArray loadavg, jint nelems)
{
	/* stub implementation */
	assert(!"Java_sun_misc_Unsafe_getLoadAverage is unimplemented");
	return -1;
}


jlong JNICALL
Java_sun_misc_Unsafe_allocateMemory(JNIEnv *env, jobject receiver, jlong size)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jlong result = 0;
	UDATA actualSize = (UDATA)size;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if ((size < 0) || (size != (jlong)actualSize)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
	} else {
		result = (jlong)(UDATA)unsafeAllocateMemory(currentThread, actualSize, TRUE);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}


jlong JNICALL
Java_sun_misc_Unsafe_allocateDBBMemory(JNIEnv *env, jobject receiver, jlong size)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jlong result = 0;
	UDATA actualSize = (UDATA)size;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if ((size < 0) || (size != (jlong)actualSize)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
	} else {
		result = (jlong)(UDATA)unsafeAllocateDBBMemory(currentThread, actualSize, TRUE);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

jlong JNICALL
Java_jdk_internal_misc_Unsafe_allocateDBBMemory(JNIEnv *env, jobject receiver, jlong size)
{
	return Java_sun_misc_Unsafe_allocateDBBMemory(env, receiver, size);
}


void JNICALL
Java_sun_misc_Unsafe_freeMemory(JNIEnv *env, jobject receiver, jlong address)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	unsafeFreeMemory(currentThread, (void*)(UDATA)address);
}


void JNICALL
Java_sun_misc_Unsafe_freeDBBMemory(JNIEnv *env, jobject receiver, jlong address)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	unsafeFreeDBBMemory(currentThread, (void*)(UDATA)address);
}

void JNICALL
Java_jdk_internal_misc_Unsafe_freeDBBMemory(JNIEnv *env, jobject receiver, jlong address)
{
	Java_sun_misc_Unsafe_freeDBBMemory(env, receiver, address);
}

jlong JNICALL
Java_sun_misc_Unsafe_reallocateMemory(JNIEnv *env, jobject receiver, jlong address, jlong size)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jlong result = 0;
	UDATA actualSize = (UDATA)size;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if ((size < 0) || (size != (jlong)actualSize)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
	} else {
		result = (jlong)(UDATA)unsafeReallocateMemory(currentThread, (void*)(UDATA)address, actualSize);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}


jlong JNICALL
Java_sun_misc_Unsafe_reallocateDBBMemory(JNIEnv *env, jobject receiver, jlong address, jlong size)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jlong result = 0;
	UDATA actualSize = (UDATA)size;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if ((size < 0) || (size != (jlong)actualSize)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
	} else {
		result = (jlong)(UDATA)unsafeReallocateDBBMemory(currentThread, (void*)(UDATA)address, actualSize);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

jlong JNICALL
Java_jdk_internal_misc_Unsafe_reallocateDBBMemory(JNIEnv *env, jobject receiver, jlong address, jlong size)
{
	return Java_sun_misc_Unsafe_reallocateDBBMemory(env, receiver, address, size);
}

void JNICALL
Java_sun_misc_Unsafe_ensureClassInitialized(JNIEnv *env, jobject receiver, jclass clazz)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == clazz) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t classObject = J9_JNI_UNWRAP_REFERENCE(clazz);
		J9Class *j9clazz =  J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject);
		if (VM_VMHelpers::classRequiresInitialization(currentThread, j9clazz)) {
			vmFuncs->initializeClass(currentThread, j9clazz);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}


void JNICALL
Java_sun_misc_Unsafe_park(JNIEnv *env, jobject receiver, jboolean isAbsolute, jlong time)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vmFuncs->threadParkImpl(currentThread, (IDATA)isAbsolute, (I_64)time);
	vmFuncs->internalExitVMToJNI(currentThread);
}


void JNICALL
Java_sun_misc_Unsafe_unpark(JNIEnv *env, jobject receiver, jthread thread)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vmFuncs->threadUnparkImpl(currentThread, J9_JNI_UNWRAP_REFERENCE(thread));
	vmFuncs->internalExitVMToJNI(currentThread);
}


void JNICALL
Java_sun_misc_Unsafe_throwException(JNIEnv *env, jobject receiver, jthrowable exception)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == exception) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		currentThread->currentException = J9_JNI_UNWRAP_REFERENCE(exception);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}


void JNICALL
Java_sun_misc_Unsafe_monitorEnter(JNIEnv *env, jobject receiver, jobject obj)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == obj) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		UDATA monresult = vmFuncs->objectMonitorEnter(currentThread, J9_JNI_UNWRAP_REFERENCE(obj));
		if (0 == monresult) {
oom:
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		} else if (J9_UNEXPECTED(!VM_ObjectMonitor::recordJNIMonitorEnter(currentThread, (j9object_t)monresult))) {
			vmFuncs->objectMonitorExit(currentThread, (j9object_t)monresult);
			goto oom;
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}


void JNICALL
Java_sun_misc_Unsafe_monitorExit(JNIEnv *env, jobject receiver, jobject obj)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == obj) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		IDATA monresult = vmFuncs->objectMonitorExit(currentThread, object);
		if (J9THREAD_ILLEGAL_MONITOR_STATE == monresult) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
		} else {
			VM_ObjectMonitor::recordJNIMonitorExit(currentThread, object);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}


jboolean JNICALL
Java_sun_misc_Unsafe_tryMonitorEnter(JNIEnv *env, jobject receiver, jobject obj)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jboolean entered = JNI_TRUE;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == obj) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (!VM_ObjectMonitor::inlineFastObjectMonitorEnter(currentThread, object)) {
			if (vmFuncs->objectMonitorEnterNonBlocking(currentThread, object) <= J9_OBJECT_MONITOR_BLOCKING) {
				entered = JNI_FALSE;
			}
		}
		if (entered) {
			VM_ObjectMonitor::recordJNIMonitorEnter(currentThread, object);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return entered;
}

/**
 * Calculates log(2) of the alignment of a value.
 * Only alignments between 1 and 8 are handled (anything larger will
 * be treated as if it's 8-aligned).
 *
 * @param value[in] the value to check
 *
 * @returns the log(2) of the alignment, a value between 0 and 3
 */
static VMINLINE UDATA
determineAlignment(UDATA value)
{
	UDATA alignment = 0;
	if (J9_ARE_NO_BITS_SET(value, 7)) {
		alignment = 3;
	} else if (J9_ARE_NO_BITS_SET(value, 3)) {
		alignment = 2;
	} else if (J9_ARE_NO_BITS_SET(value, 1)) {
		alignment = 1;
	}
	return alignment;
}

/**
 * Calculates the minimum common alignment of three values.
 * See determineAlignment,
 *
 * @param sourceOffset[in] the first value
 * @param destOffset[in] the second value
 * @param size[in] the third value
 *
 * @returns the minimum common alignment of the three inputs
 */
static VMINLINE UDATA
determineCommonAlignment(UDATA sourceOffset, UDATA destOffset, UDATA size)
{
	UDATA alignment = determineAlignment(sourceOffset);
	UDATA destAlignment = determineAlignment(destOffset);
	if (destAlignment < alignment) {
		alignment = destAlignment;
	}
	UDATA sizeAlignment = determineAlignment(size);
	if (sizeAlignment < alignment) {
		alignment = sizeAlignment;
	}
	return alignment;
}

/**
 * Copy actualSize bytes from source object to destination.
 * Helper method for copyMemory and copyMemoryByte.
 *
 * @param currentThread
 * @param sourceObject object to copy from
 * @param sourceOffset location in srcObject to start copy
 * @param destObject object to copy into
 * @param destOffset location in destObject to start copy
 * @param actualSize the number of bytes to be copied
 * @param logElementSize common alignment of offsets and actualSize
 * @param sourceIndex index into source array
 * @param destIndex index into destination array
 * @param elementCount elements to copy in array
 */
static VMINLINE void
copyMemorySub(J9VMThread* currentThread, j9object_t sourceObject, UDATA sourceOffset, j9object_t destObject,
		UDATA destOffset, UDATA actualSize, UDATA logElementSize, UDATA sourceIndex, UDATA destIndex,
		UDATA elementCount)
{
	if (NULL == sourceObject) {
		if (NULL == destObject) {
			UDATA sourceEnd = sourceOffset + actualSize;
			if ((destOffset > sourceOffset) && (destOffset < sourceEnd)) {
				alignedBackwardsMemcpy(currentThread, (void*)(destOffset + actualSize), (void*)sourceEnd, actualSize, logElementSize);
			} else {
				alignedMemcpy(currentThread, (void*)destOffset, (void*)sourceOffset, actualSize, logElementSize);
			}
		} else {
			VM_ArrayCopyHelpers::memcpyToArray(currentThread, destObject, logElementSize, destIndex, elementCount, (void*)sourceOffset);
		}
	} else if (NULL == destObject) {
		VM_ArrayCopyHelpers::memcpyFromArray(currentThread, sourceObject, logElementSize, sourceIndex, elementCount, (void*)destOffset);
	} else {
		VM_ArrayCopyHelpers::primitiveArrayCopy(currentThread, sourceObject, sourceIndex, destObject, destIndex, elementCount, logElementSize);
	}
}

/**
 * Copy actualSize bytes from source object to destination.
 *
 * @param currentThread
 * @param sourceObject object to copy from
 * @param sourceOffset location in srcObject to start copy
 * @param destObject object to copy into
 * @param destOffset location in destObject to start copy
 * @param actualSize the number of bytes to be copied
 */
static VMINLINE void
copyMemory(J9VMThread* currentThread, j9object_t sourceObject, UDATA sourceOffset, j9object_t destObject,
		UDATA destOffset, UDATA actualSize)
{
	/* Because array data is always 8-aligned, only the alignment of the offsets (and byte size) need be considered */
	UDATA const headerSize = J9VMTHREAD_CONTIGUOUS_HEADER_SIZE(currentThread);
	UDATA logElementSize = determineCommonAlignment(sourceOffset, destOffset, actualSize);
	UDATA sourceIndex = (sourceOffset - headerSize) >> logElementSize;
	UDATA destIndex = (destOffset - headerSize) >> logElementSize;
	UDATA elementCount = actualSize >> logElementSize;

	copyMemorySub(currentThread, sourceObject, sourceOffset, destObject, destOffset, actualSize, logElementSize, sourceIndex,
			destIndex, elementCount);
}

/**
 * Copy byte from source object to destination.
 *
 * @param currentThread
 * @param sourceObject object to copy from
 * @param sourceOffset location in srcObject to start copy
 * @param destObject object to copy into
 * @param destOffset location in destObject to start copy
 */
static VMINLINE void
copyMemoryByte(J9VMThread* currentThread, j9object_t sourceObject, UDATA sourceOffset, j9object_t destObject,
		UDATA destOffset)
{
	UDATA const headerSize = J9VMTHREAD_CONTIGUOUS_HEADER_SIZE(currentThread);
	UDATA sourceIndex = sourceOffset - headerSize;
	UDATA destIndex = destOffset - headerSize;

	copyMemorySub(currentThread, sourceObject, sourceOffset, destObject, destOffset,
			(UDATA)1, (UDATA)0, sourceIndex, destIndex, (UDATA)1);
}

void JNICALL
Java_sun_misc_Unsafe_copyMemory__Ljava_lang_Object_2JLjava_lang_Object_2JJ(JNIEnv *env, jobject receiver, jobject srcBase, jlong srcOffset, jobject dstBase, jlong dstOffset, jlong size)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	UDATA sourceOffset = (UDATA)srcOffset;
	UDATA destOffset = (UDATA)dstOffset;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	UDATA actualSize = (UDATA)size;
	if ((size < 0) || (size != (jlong)actualSize)) {
illegal:
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
	} else {
		j9object_t sourceObject = NULL;
		j9object_t destObject = NULL;
		if (NULL != srcBase) {
			sourceObject = J9_JNI_UNWRAP_REFERENCE(srcBase);
			J9Class *clazz = J9OBJECT_CLAZZ(currentThread, sourceObject);
			if (!J9CLASS_IS_ARRAY(clazz)) {
				goto illegal;
			}
			if (!J9ROMCLASS_IS_PRIMITIVE_TYPE(((J9ArrayClass*)clazz)->componentType->romClass)) {
				goto illegal;
			}
		}
		if (NULL != dstBase) {
			destObject = J9_JNI_UNWRAP_REFERENCE(dstBase);
			J9Class *clazz = J9OBJECT_CLAZZ(currentThread, destObject);
			if (!J9CLASS_IS_ARRAY(clazz)) {
				goto illegal;
			}
			if (!J9ROMCLASS_IS_PRIMITIVE_TYPE(((J9ArrayClass*)clazz)->componentType->romClass)) {
				goto illegal;
			}
		}

		copyMemory(currentThread, sourceObject, sourceOffset, destObject, destOffset, actualSize);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}


jlong JNICALL
Java_sun_misc_Unsafe_objectFieldOffset(JNIEnv *env, jobject receiver, jobject field)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jlong offset = 0;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == field) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9JNIFieldID *fieldID = vm->reflectFunctions.idFromFieldObject(currentThread, NULL, J9_JNI_UNWRAP_REFERENCE(field));
		J9ROMFieldShape *romField = fieldID->field;
		if (NULL == romField) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		} else if (J9_ARE_ANY_BITS_SET(romField->modifiers, J9AccStatic)) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		} else {
			offset = (jlong)fieldID->offset + J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return offset;
}


void JNICALL
Java_sun_misc_Unsafe_setMemory__Ljava_lang_Object_2JJB(JNIEnv *env, jobject receiver, jobject obj, jlong offset, jlong size, jbyte value)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	UDATA actualSize = (UDATA)size;
	if ((size < 0) || (size != (jlong)actualSize)) {
illegal:
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
	} else if (NULL == obj) {
		/* NULL object - raw memset */
		memset((void*)(UDATA)offset, (int)value, actualSize);
	} else {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		J9Class *clazz = J9OBJECT_CLAZZ(currentThread, object);
		if (!J9CLASS_IS_ARRAY(clazz)) {
			goto illegal;
		}
		if (!J9ROMCLASS_IS_PRIMITIVE_TYPE(((J9ArrayClass*)clazz)->componentType->romClass)) {
			goto illegal;
		}
		offset -= J9VMTHREAD_CONTIGUOUS_HEADER_SIZE(currentThread);
		VM_ArrayCopyHelpers::primitiveArrayFill(currentThread, object, (UDATA)offset, actualSize, (U_8)value);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}


jobject JNICALL
Java_sun_misc_Unsafe_staticFieldBase__Ljava_lang_reflect_Field_2(JNIEnv *env, jobject receiver, jobject field)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jobject base = NULL;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == field) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9JNIFieldID *fieldID = vm->reflectFunctions.idFromFieldObject(currentThread, NULL, J9_JNI_UNWRAP_REFERENCE(field));
		J9ROMFieldShape *romField = fieldID->field;
		if (NULL == romField) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		} else if (J9_ARE_NO_BITS_SET(romField->modifiers, J9AccStatic)) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		} else {
			base = vmFuncs->j9jni_createLocalRef(env, J9VM_J9CLASS_TO_HEAPCLASS(fieldID->declaringClass));
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return base;
}


jlong JNICALL
Java_sun_misc_Unsafe_staticFieldOffset(JNIEnv *env, jobject receiver, jobject field)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jlong offset = 0;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == field) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9JNIFieldID *fieldID = vm->reflectFunctions.idFromFieldObject(currentThread, NULL, J9_JNI_UNWRAP_REFERENCE(field));
		J9ROMFieldShape *romField = fieldID->field;
		if (NULL == romField) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		} else if (J9_ARE_NO_BITS_SET(romField->modifiers, J9AccStatic)) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		} else {
			offset = fieldID->offset | J9_SUN_STATIC_FIELD_OFFSET_TAG;

			if (J9_ARE_ANY_BITS_SET(romField->modifiers, J9AccFinal)) {
				offset |= J9_SUN_FINAL_FIELD_OFFSET_TAG;
			}
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return offset;
}

jboolean JNICALL
Java_sun_misc_Unsafe_unalignedAccess0(JNIEnv *env, jobject receiver)
{
	return JNI_TRUE;
}

jboolean JNICALL
Java_sun_misc_Unsafe_isBigEndian0(JNIEnv *env, jobject receiver)
{
#if defined(J9VM_ENV_LITTLE_ENDIAN)
	return JNI_FALSE;
#else
	return JNI_TRUE;
#endif
}

jobject JNICALL
Java_sun_misc_Unsafe_getUncompressedObject(JNIEnv *env, jobject receiver, jlong address)
{
	/* stub implementation */
	assert(!"Java_sun_misc_Unsafe_getUncompressedObject is unimplemented");
	return NULL;
}

jclass JNICALL
Java_sun_misc_Unsafe_getJavaMirror(JNIEnv *env, jobject receiver, jlong address)
{
	/* stub implementation */
	assert(!"Java_sun_misc_Unsafe_getJavaMirror is unimplemented");
	return NULL;
}

jlong JNICALL
Java_sun_misc_Unsafe_getKlassPointer(JNIEnv *env, jobject receiver, jobject address)
{
	/* stub implementation */
	assert(!"Java_sun_misc_Unsafe_getKlassPointer is unimplemented");
	return (jlong) 0;
}

jboolean JNICALL
Java_jdk_internal_misc_Unsafe_shouldBeInitialized(JNIEnv *env, jobject receiver, jclass clazz)
{
	jboolean result = JNI_FALSE;

	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	if (NULL == clazz) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t classObject = J9_JNI_UNWRAP_REFERENCE(clazz);
		J9Class *j9clazz =  J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, classObject);
		if (VM_VMHelpers::classRequiresInitialization(currentThread, j9clazz)) {
			result = JNI_TRUE;
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return result;
}

jlong JNICALL
Java_jdk_internal_misc_Unsafe_objectFieldOffset1(JNIEnv *env, jobject receiver, jclass clazz, jstring name)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	j9object_t fieldObj = NULL;
	jlong offset = 0;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	fieldObj = getFieldObjHelper(currentThread, clazz, name);
	if (NULL != fieldObj) {
		J9JNIFieldID *fieldID = NULL;
		J9ROMFieldShape *romField = NULL;

		fieldID = vm->reflectFunctions.idFromFieldObject(currentThread, J9_JNI_UNWRAP_REFERENCE(clazz), fieldObj);
		romField = fieldID->field;
		if (NULL == romField) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		} else if (J9_ARE_ANY_BITS_SET(romField->modifiers, J9AccStatic)) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		} else {
			offset = (jlong)fieldID->offset + J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return offset;
}

/**
 * Writes modified memory in an address range from cache to main memory.
 * Uses memory barriers before and after writeback to ensure ordering.
 *
 * On x86, the writeback is done using the CLWB instruction if available
 * for performance, falling back to CLFLUSHOPT then CLFLUSH otherwise.
 *
 * @param addr address of block to write back to memory
 * @param len length of block being written
 */
void JNICALL
Java_jdk_internal_misc_Unsafe_writebackMemory(JNIEnv *env, jobject receiver, jlong addr, jlong len)
{
/* Exclude Windows since it does not support GCC assembly syntax,
 * and Linux is the only target actually specified in JEP 352.
 */
#if (defined(J9X86) || defined(J9HAMMER)) && !defined(WIN32)
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	uintptr_t cacheLineSize = vm->dCacheLineSize;

	if (cacheLineSize > 0) {
		uintptr_t const uaddr = (uintptr_t)(U_64)addr;
		uintptr_t const ulen = (uintptr_t)(U_64)len;
		uintptr_t const firstCacheLine = uaddr & ~(cacheLineSize - 1);
		uintptr_t const lastCacheLine = (uaddr + ((0 == ulen) ? 0 : (ulen - 1))) & ~(cacheLineSize - 1);

		/* adjust for pre-increment in the loops below */
		uintptr_t cacheLine = firstCacheLine - cacheLineSize;

		VM_AtomicSupport::readWriteBarrier();
		switch (vm->cpuCacheWritebackCapabilities) {
		case J9PORT_X86_FEATURE_CLWB:
			do {
				cacheLine += cacheLineSize;
				asm volatile("clwb %0" : "+m" (*(U_8 *)cacheLine));
			} while (lastCacheLine != cacheLine);
			break;
		case J9PORT_X86_FEATURE_CLFLUSHOPT:
			do {
				cacheLine += cacheLineSize;
				asm volatile("clflushopt %0" : "+m" (*(U_8 *)cacheLine));
			} while (lastCacheLine != cacheLine);
			break;
		case J9PORT_X86_FEATURE_CLFSH:
			do {
				cacheLine += cacheLineSize;
				asm volatile("clflush %0" : "+m" (*(U_8 *)cacheLine));
			} while (lastCacheLine != cacheLine);
			break;
		default:
			goto error;
		}
		VM_AtomicSupport::readWriteBarrier();
		return;
	}
error:
#endif /* x86 */

	jclass exceptionClass = env->FindClass("java/lang/RuntimeException");
	if (NULL == exceptionClass) {
		/* Just return if we can't load the exception class. */
		return;
	}
	env->ThrowNew(exceptionClass, "writebackMemory not supported");
}

/**
 * Checks if the platform supports writeback from cache to memory. This
 * function checks if the necessary CPU or OS features for writeback are
 * enabled.
 * On x86, these are the CLFLUSH, CLFLUSHOPT and CLWB instructions.
 */
jboolean JNICALL
Java_jdk_internal_misc_Unsafe_isWritebackEnabled(JNIEnv *env, jclass clazz)
{
	jboolean result = JNI_FALSE;
#if (defined(J9X86) || defined(J9HAMMER)) && !defined(WIN32)
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	if (vm->dCacheLineSize > 0) {
		switch (vm->cpuCacheWritebackCapabilities) {
		case J9PORT_X86_FEATURE_CLWB:
		case J9PORT_X86_FEATURE_CLFLUSHOPT:
		case J9PORT_X86_FEATURE_CLFSH:
			result = JNI_TRUE;
			break;
		default:
			break;
		}
	}
#endif /* x86 */
	return result;
}

/* register jdk.internal.misc.Unsafe natives common to Java 9, 10 and beyond */
static void
registerJdkInternalMiscUnsafeNativesCommon(JNIEnv *env, jclass clazz) {
	/* clazz can't be null */
	JNINativeMethod natives[] = {
		{
			(char*)"defineClass0",
			(char*)"(Ljava/lang/String;[BIILjava/lang/ClassLoader;Ljava/security/ProtectionDomain;)Ljava/lang/Class;",
			(void*)&Java_sun_misc_Unsafe_defineClass__Ljava_lang_String_2_3BIILjava_lang_ClassLoader_2Ljava_security_ProtectionDomain_2
		},
		{
			(char*)"defineAnonymousClass0",
			(char*)"(Ljava/lang/Class;[B[Ljava/lang/Object;)Ljava/lang/Class;",
			(void*)&Java_sun_misc_Unsafe_defineAnonymousClass
		},
		{
			(char*)"pageSize",
			(char*)"()I",
			(void*)&Java_sun_misc_Unsafe_pageSize
		},
		{
			(char*)"getLoadAverage0",
			(char*)"([DI)I",
			(void*)&Java_sun_misc_Unsafe_getLoadAverage
		},
		{
			(char*)"shouldBeInitialized0",
			(char*)"(Ljava/lang/Class;)Z",
			(void*)&Java_jdk_internal_misc_Unsafe_shouldBeInitialized
		},
		{
			(char*)"allocateMemory0",
			(char*)"(J)J",
			(void*)&Java_sun_misc_Unsafe_allocateMemory
		},
		{
			(char*)"freeMemory0",
			(char*)"(J)V",
			(void*)&Java_sun_misc_Unsafe_freeMemory
		},
		{
			(char*)"reallocateMemory0",
			(char*)"(JJ)J",
			(void*)&Java_sun_misc_Unsafe_reallocateMemory
		},
		{
			(char*)"ensureClassInitialized0",
			(char*)"(Ljava/lang/Class;)V",
			(void*)&Java_sun_misc_Unsafe_ensureClassInitialized
		},
		{
			(char*)"park",
			(char*)"(ZJ)V",
			(void*)&Java_sun_misc_Unsafe_park
		},
		{
			(char*)"unpark",
			(char*)"(Ljava/lang/Object;)V",
			(void*)&Java_sun_misc_Unsafe_unpark
		},
		{
			(char*)"throwException",
			(char*)"(Ljava/lang/Throwable;)V",
			(void*)&Java_sun_misc_Unsafe_throwException
		},
		{
			(char*)"copyMemory0",
			(char*)"(Ljava/lang/Object;JLjava/lang/Object;JJ)V",
			(void*)&Java_sun_misc_Unsafe_copyMemory__Ljava_lang_Object_2JLjava_lang_Object_2JJ
		},
		{
			(char*)"objectFieldOffset0",
			(char*)"(Ljava/lang/reflect/Field;)J",
			(void*)&Java_sun_misc_Unsafe_objectFieldOffset
		},
		{
			(char*)"setMemory0",
			(char*)"(Ljava/lang/Object;JJB)V",
			(void*)&Java_sun_misc_Unsafe_setMemory__Ljava_lang_Object_2JJB
		},
		{
			(char*)"staticFieldBase0",
			(char*)"(Ljava/lang/reflect/Field;)Ljava/lang/Object;",
			(void*)&Java_sun_misc_Unsafe_staticFieldBase__Ljava_lang_reflect_Field_2
		},
		{
			(char*)"staticFieldOffset0",
			(char*)"(Ljava/lang/reflect/Field;)J",
			(void*)&Java_sun_misc_Unsafe_staticFieldOffset
		},
		{
			(char*)"unalignedAccess0",
			(char*)"()Z",
			(void*)&Java_sun_misc_Unsafe_unalignedAccess0
		},
		{
			(char*)"isBigEndian0",
			(char*)"()Z",
			(void*)&Java_sun_misc_Unsafe_isBigEndian0
		},
		{
			(char*)"getUncompressedObject",
			(char*)"(J)Ljava/lang/Object;",
			(void*)&Java_sun_misc_Unsafe_getUncompressedObject
		},
	};
	jint numNatives = sizeof(natives)/sizeof(JNINativeMethod);
	env->RegisterNatives(clazz, natives, numNatives);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	clearNonZAAPEligibleBit(env, clazz, natives, numNatives);
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
}

/* register jdk.internal.misc.Unsafe natives for Java 10 */
static void
registerJdkInternalMiscUnsafeNativesJava10(JNIEnv *env, jclass clazz) {
	/* clazz can't be null */
	JNINativeMethod natives[] = {
		{
			(char*)"objectFieldOffset1",
			(char*)"(Ljava/lang/Class;Ljava/lang/String;)J",
			(void*)&Java_jdk_internal_misc_Unsafe_objectFieldOffset1
		}
	};
	jint numNatives = sizeof(natives)/sizeof(JNINativeMethod);
	env->RegisterNatives(clazz, natives, numNatives);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	clearNonZAAPEligibleBit(env, clazz, natives, numNatives);
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
}

/* register jdk.internal.misc.Unsafe natives for Java 14 */
static void
registerJdkInternalMiscUnsafeNativesJava14(JNIEnv *env, jclass clazz) {
	/* clazz can't be null */
	JNINativeMethod natives[] = {
		{
			(char*)"writebackMemory",
			(char*)"(JJ)V",
			(void*)&Java_jdk_internal_misc_Unsafe_writebackMemory
		},
		{
			(char*)"isWritebackEnabled",
			(char*)"()Z",
			(void*)&Java_jdk_internal_misc_Unsafe_isWritebackEnabled
		}
	};
	jint numNatives = sizeof(natives)/sizeof(JNINativeMethod);
	env->RegisterNatives(clazz, natives, numNatives);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	clearNonZAAPEligibleBit(env, clazz, natives, numNatives);
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
}

/* class jdk.internal.misc.Unsafe only presents in Java 9 and beyond */
void JNICALL
Java_jdk_internal_misc_Unsafe_registerNatives(JNIEnv *env, jclass clazz)
{
	J9VMThread *currentThread = (J9VMThread*)env;

	Java_sun_misc_Unsafe_registerNatives(env, clazz);
	registerJdkInternalMiscUnsafeNativesCommon(env, clazz);
	if (J2SE_VERSION(currentThread->javaVM) >= J2SE_V11) {
		registerJdkInternalMiscUnsafeNativesJava10(env, clazz);
	}
	if (J2SE_VERSION(currentThread->javaVM) >= J2SE_V14) {
		registerJdkInternalMiscUnsafeNativesJava14(env, clazz);
	}
}

/*
 * Determine if memory addresses overlap.
 * Will always return false if either object is an array.
 *
 * @param sourceObject
 * @param sourceOffset memory address
 * @param destObject
 * @param destOffset memory address
 * @param actualCopySize size of memory after address
 * @return true for no memory overlap, otherwise false
 */
jboolean
 memOverlapIsNone(j9object_t sourceObject, UDATA sourceOffset, j9object_t destObject, UDATA destOffset, UDATA actualCopySize) {
	jboolean result = JNI_FALSE;
	if (sourceObject != destObject) {
		result = JNI_TRUE;
	} else if ((sourceObject == NULL) && (destObject == NULL)) {
		if (sourceOffset > (destOffset + actualCopySize)) {
			result = JNI_TRUE;
		} else if (destOffset > (sourceOffset + actualCopySize)) {
			result = JNI_TRUE;
		}
	}
	return result;
}

/*
 * Determine if memory addresses overlap, and is it is exactly aligned.
 * Will always return false if either object is an array. Assumes we know
 * that there is at least some memory overlap (memOverlapIsNone is false).
 *
 * @param sourceOffset memory address
 * @param destOffset memory address
 * @return true for no memory overlap, otherwise false
 */
jboolean
 memOverlapIsUnaligned(UDATA sourceOffset, UDATA destOffset) {
	return (sourceOffset != destOffset);
}

void JNICALL
Java_jdk_internal_misc_Unsafe_copySwapMemory0(JNIEnv *env, jobject receiver, jobject srcBase, jlong srcOffset,
		jobject dstBase, jlong dstOffset, jlong copySize, jlong elemSize)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	UDATA sourceOffset = (UDATA)srcOffset;
	UDATA destOffset = (UDATA)dstOffset;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	UDATA actualCopySize = (UDATA)copySize;
	UDATA actualElementSize = (UDATA)elemSize;

	if ((copySize < 0) || (copySize != (jlong)actualCopySize) || (elemSize != (jlong)actualElementSize)) {
illegal:
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
	} else if ( ! (2 == elemSize || 4 == elemSize || 8 == elemSize)) {
		/* verify that element size is supported for swapping */
		goto illegal;
	} else if (0 != (copySize % elemSize)) {
		/* verify that size to be copied is a multiple of element size */
		goto illegal;
	} else {
		j9object_t sourceObject = NULL;
		j9object_t destObject = NULL;
		if (NULL != srcBase) {
			sourceObject = J9_JNI_UNWRAP_REFERENCE(srcBase);
			J9Class *clazz = J9OBJECT_CLAZZ(currentThread, sourceObject);
			if (!J9CLASS_IS_ARRAY(clazz)) {
				goto illegal;
			}
			if (!J9ROMCLASS_IS_PRIMITIVE_TYPE(((J9ArrayClass*)clazz)->componentType->romClass)) {
				goto illegal;
			}
		}
		if (NULL != dstBase) {
			destObject = J9_JNI_UNWRAP_REFERENCE(dstBase);
			J9Class *clazz = J9OBJECT_CLAZZ(currentThread, destObject);
			if (!J9CLASS_IS_ARRAY(clazz)) {
				goto illegal;
			}
			if (!J9ROMCLASS_IS_PRIMITIVE_TYPE(((J9ArrayClass*)clazz)->componentType->romClass)) {
				goto illegal;
			}
		}

		jboolean overlapIsNone = memOverlapIsNone(sourceObject, sourceOffset, destObject, destOffset, actualCopySize);
		if (!overlapIsNone) {
			/* copy source data to destination first if source and destination memory is overlapping
			 * and the overlap is unaligned. This will prevent any errors during swapping.
			 */
			if (memOverlapIsUnaligned(sourceOffset, destOffset)) {
				copyMemory(currentThread, sourceObject, sourceOffset, destObject, destOffset, actualCopySize);
			}
		}

		/* use temporary byte for swap if memory is overlapping */
		UDATA tempOffset = 0;
		for (UDATA elementOffset = 0; elementOffset < actualCopySize; elementOffset += actualElementSize) {
			for (UDATA elementIndex = 0; elementIndex < (actualElementSize / 2); elementIndex++) {
				UDATA lowerIndex = elementOffset + elementIndex;
				UDATA upperIndex = elementOffset + actualElementSize - 1 - elementIndex;

				if (overlapIsNone) {
					copyMemoryByte(currentThread, sourceObject, sourceOffset + upperIndex, destObject, destOffset + lowerIndex);
					copyMemoryByte(currentThread, sourceObject, sourceOffset + lowerIndex, destObject, destOffset + upperIndex);
				} else { /* exact or unaligned overlap */
					copyMemoryByte(currentThread, destObject, destOffset + lowerIndex, NULL, (UDATA)&tempOffset);
					copyMemoryByte(currentThread, destObject, destOffset + upperIndex, destObject, destOffset + lowerIndex);
					copyMemoryByte(currentThread, NULL, (UDATA)&tempOffset, destObject, destOffset + upperIndex);
				}
			}
		}
	}
	vmFuncs->internalReleaseVMAccess(currentThread);
}

} /* extern "C" */
