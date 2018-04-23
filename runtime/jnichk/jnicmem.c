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
#include "omrthread.h"
#include "jni.h"
#include "j9comp.h"
#include "vmaccess.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9protos.h"
#include "rommeth.h"
#include "j9cp.h"
#include "j9vmls.h"
#include "jnichknls.h"
#include "jnicheck.h"
#include "jnichk_internal.h"

#include <string.h>
#include <stdlib.h>
#include "j9port.h"
#include "ut_j9jni.h"


typedef struct MemRecord {
	JNIEnv* env;
	const char* acquireFunction;
	const void* memory;
	jobject object;
	jobject originalObject;
	UDATA frameOffset;
	U_32 crc;
} MemRecord;

static omrthread_monitor_t MemMonitor = NULL;
static J9Pool* MemPoolGlobal = NULL;

static jboolean jniCheckIsSameObject (JNIEnv * env, jobject ref1, jobject ref2);
static UDATA checkArrayCrc (JNIEnv * env, const char *acquireFunction, const char *releaseFunction, jobject object, const void *memory, jint mode, MemRecord * poolElement);
static UDATA getStackFrameOffset (J9VMThread* vmThread);
static void jniCheckDeleteGlobalRef (JNIEnv * env, jobject ref);
static jobject jniCheckNewGlobalRef (JNIEnv * env, jobject ref);

static U_32 calculateArrayCrc(J9VMThread *vmThread, J9IndexableObject *arrayPtr);

void 
jniRecordMemoryAcquire(JNIEnv* env, const char* functionName, jobject object, const void* memory, jint recordCRC) 
{
	MemRecord* poolElement;
	U_32 crc = 0;
	J9VMThread *vmThread = (J9VMThread *)env;
	PORT_ACCESS_FROM_VMC(vmThread);
	jobject ref = NULL;

	if (memory == NULL) {
		return;
	}

	if ( vmThread->javaVM->checkJNIData.options & JNICHK_VERBOSE) {
		PORT_ACCESS_FROM_VMC(vmThread);
		Trc_JNI_MemoryAcquire(env, functionName, memory);
		j9tty_printf(PORTLIB, "<JNI %s: buffer=0x%p>\n", functionName, memory);
	}

	if (recordCRC) {
		U_32 length;
		J9IndexableObject *arrayPtr;

		enterVM(vmThread);
		arrayPtr = *(J9IndexableObject**)object;
		length = J9INDEXABLEOBJECT_SIZE(vmThread, arrayPtr);
		length <<= ((J9ROMArrayClass*)(J9OBJECT_CLAZZ(vmThread, arrayPtr)->romClass))->arrayShape & 0x0000FFFF;
		exitVM(vmThread);
		crc = j9crc32(j9crc32(0, NULL, 0), (U_8*)memory, length);
	}

	/* allocate the ref outside of the mutex */
	ref = jniCheckNewGlobalRef(env, object);

	omrthread_monitor_enter(MemMonitor);

	poolElement = pool_newElement(MemPoolGlobal);
	if (poolElement == NULL) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_OUT_OF_MEMORY, functionName);
	} else {
		poolElement->env = env;
		poolElement->acquireFunction = functionName;
		poolElement->memory = memory;
		poolElement->object = ref;
		poolElement->originalObject = object;
		poolElement->frameOffset = getStackFrameOffset(vmThread);
		poolElement->crc = crc;
	}

	omrthread_monitor_exit(MemMonitor);

}


jint
jniCheckMemoryInit(J9JavaVM* javaVM) 
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	omrthread_monitor_t globalMonitor = omrthread_global_monitor();
	omrthread_monitor_enter(globalMonitor);
	if (MemMonitor == NULL) {
		if (omrthread_monitor_init_with_name(&MemMonitor, 0,"JNI Mem")) {
			Trc_JNI_MemoryInitMonitorError();
			j9tty_printf(PORTLIB, "Unable to initialize monitor\n");
			omrthread_monitor_exit(globalMonitor);
			return -1;
		}
	}
	omrthread_monitor_exit(globalMonitor);

	omrthread_monitor_enter(MemMonitor);
	if (MemPoolGlobal == NULL) {
		MemPoolGlobal = pool_new(sizeof(MemRecord),  0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, (j9memAlloc_fptr_t)pool_portLibAlloc, (j9memFree_fptr_t)pool_portLibFree, PORTLIB);
	}
	omrthread_monitor_exit(MemMonitor);

	if (MemPoolGlobal == NULL) {
		Trc_JNI_MemoryInitPoolError();
		j9tty_printf(PORTLIB, "Out of memory\n");
		return -1;
	}

	return 0;
}


void 
jniRecordMemoryRelease(JNIEnv* env, const char* acquireFunction, const char* releaseFunction, jobject object, const void* memory, jint checkCRC, jint mode) 
{
	MemRecord* poolElement;
	pool_state poolState;
	J9VMThread *vmThread = (J9VMThread *)env;
	UDATA isCopy = 1;
	PORT_ACCESS_FROM_VMC(vmThread);

	switch (mode) {
		case 0:
		case JNI_COMMIT:
		case JNI_ABORT:
			break;
		default:
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_UNRECOGNIZED_MODE, releaseFunction, mode);
			return;
	}

	if ( vmThread->javaVM->checkJNIData.options & JNICHK_VERBOSE) {
		PORT_ACCESS_FROM_VMC(vmThread);
		Trc_JNI_MemoryRelease(env, releaseFunction, memory);
		j9tty_printf(PORTLIB, "<JNI %s: buffer=%p>\n", releaseFunction, memory);
	}

	omrthread_monitor_enter(MemMonitor);

	poolElement = pool_startDo(MemPoolGlobal, &poolState);
	for(;;) {
		if (poolElement == NULL) {
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_BAD_POINTER, releaseFunction, memory);
			break;
		}

		if (poolElement->env == env && poolElement->memory == memory) {

			/* exit the mutex so that we can safely use JNI functions without risking deadlock */
			omrthread_monitor_exit(MemMonitor);

			if (!jniCheckIsSameObject(env, poolElement->object, object)) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_WRONG_OBJECT, 
					releaseFunction,
					memory, 
					poolElement->originalObject,
					object);
			} else if (strcmp(poolElement->acquireFunction, acquireFunction) != 0) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_WRONG_FUNCTION,
					releaseFunction, 
					releaseFunction, 
					memory, 
					poolElement->acquireFunction);
			} else {
				if (checkCRC) {
					isCopy = checkArrayCrc(env, acquireFunction, releaseFunction, object,
						  memory, mode, poolElement);
				}
			}

			if (!isCopy || mode != JNI_COMMIT) {
				jniCheckDeleteGlobalRef(env, poolElement->object);

				omrthread_monitor_enter(MemMonitor);
				pool_removeElement(MemPoolGlobal, (void*)poolElement);
				omrthread_monitor_exit(MemMonitor);
			}

			/* we've already released the mutex. We must now return */
			return;

		}
		poolElement = pool_nextDo(&poolState);
	}

	omrthread_monitor_exit(MemMonitor);
	return;
}


/* returns non-zero if the memory is a copied buffer */
static UDATA 
checkArrayCrc(JNIEnv * env, const char *acquireFunction, const char *releaseFunction, jobject object,
	const void *memory, jint mode, MemRecord * poolElement)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	PORT_ACCESS_FROM_VMC(vmThread);

	U_32 length;
	U_32 bufCrc, arrayCrc;
	UDATA isCopy;
	J9IndexableObject *arrayPtr;
	UDATA isContiguousArray = 0;

	enterVM(vmThread);
	arrayPtr = *(J9IndexableObject**)object;
	length = J9INDEXABLEOBJECT_SIZE(vmThread, arrayPtr);
	length <<= ((J9ROMArrayClass*)(J9OBJECT_CLAZZ(vmThread, arrayPtr)->romClass))->arrayShape & 0x0000FFFF;

	isContiguousArray = J9ISCONTIGUOUSARRAY(vm, arrayPtr);
	if (isContiguousArray) {
		U_8* arrayData = (U_8*)TMP_J9JAVACONTIGUOUSARRAYOFBYTE_EA(vmThread, arrayPtr, 0); 
		isCopy = arrayData != memory;
		arrayCrc = j9crc32(j9crc32(0, NULL, 0), arrayData, length);
	} else {
		isCopy = TRUE;
		arrayCrc = calculateArrayCrc(vmThread, arrayPtr);	
	}

	exitVM(vmThread);
	bufCrc = isCopy ? j9crc32(j9crc32(0, NULL, 0), (U_8 *) memory, length) : arrayCrc;

	if (!isCopy) {
		if (mode == JNI_COMMIT) {
			jniCheckAdviceNLS(env, 
				J9NLS_JNICHK_IGNORING_JNI_COMMIT,
				releaseFunction,
				poolElement->acquireFunction);
		} else if (mode == JNI_ABORT && bufCrc != poolElement->crc) {
			jniCheckAdviceNLS(env, 
				J9NLS_JNICHK_IGNORING_JNI_ABORT,
				releaseFunction,
				poolElement->crc,
				bufCrc, 
				poolElement->acquireFunction);
		}
	} else {
		switch (mode) {
		case 0:
			if (bufCrc == poolElement->crc) {
				jniCheckAdviceNLS(env, J9NLS_JNICHK_CONSIDER_JNI_ABORT, releaseFunction);
			}
			/* FALL THROUGH */
		case JNI_COMMIT:
			if (isCopy && arrayCrc != poolElement->crc) {
				jniCheckWarningNLS(env, 
						J9NLS_JNICHK_ARRAY_DATA_MODIFIED,
						releaseFunction, 
						poolElement->acquireFunction,
						releaseFunction, 
						poolElement->crc, 
						arrayCrc, 
						poolElement->acquireFunction);
			}
		}
	}

	/* in the case of JNI_COMMIT, record the new crc for next time */
	poolElement->crc = bufCrc;

	return isCopy;
}



void 
jniCheckForUnreleasedMemory(JNIEnv* env) 
{
	MemRecord* poolElement;
	pool_state poolState;
	UDATA frameOffset = getStackFrameOffset((J9VMThread*)env);

	omrthread_monitor_enter(MemMonitor);

	poolElement = pool_startDo(MemPoolGlobal, &poolState);

	while (poolElement) {
		if (poolElement->env == env) {
			if ( poolElement->frameOffset == frameOffset ) {
				jniCheckWarningNLS(env, 
					J9NLS_JNICHK_ARRAY_MEMORY_NOT_RELEASED,
					poolElement->memory,
					poolElement->acquireFunction);

				/* now invalidate the frame entry so that we don't report it again */
				poolElement->frameOffset = 0;
			}
		}
		poolElement = pool_nextDo(&poolState);
	}

	omrthread_monitor_exit(MemMonitor);
	return;
}


static UDATA
getStackFrameOffset(J9VMThread* vmThread) {
	return (U_8*)vmThread->stackObject->end - (U_8*)vmThread->arg0EA;
}


static jobject
jniCheckNewGlobalRef(JNIEnv * env, jobject ref)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	jobject globalRef;

	enterVM(vmThread);

	globalRef = vmThread->javaVM->internalVMFunctions->j9jni_createGlobalRef(env, *(j9object_t*)ref, JNI_FALSE);

	exitVM(vmThread);

	return globalRef;
}



static void
jniCheckDeleteGlobalRef(JNIEnv * env, jobject ref)
{
	J9VMThread *vmThread = (J9VMThread *) env;

	enterVM(vmThread);

	vmThread->javaVM->internalVMFunctions->j9jni_deleteGlobalRef(env, ref, JNI_FALSE);

	exitVM(vmThread);
}



static jboolean
jniCheckIsSameObject(JNIEnv * env, jobject ref1, jobject ref2)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	jboolean same;

	if ((ref1 == NULL) || (ref2 == NULL)) {
		return ref1 == ref2;
	}
	if (ref1 == ref2) {
		return JNI_TRUE;
	}

	{
		enterVM(vmThread);
		same = *(j9object_t*)ref1 == *(j9object_t*)ref2;
		exitVM(vmThread);
	}

	return same;
}

void
jniCheckFlushJNICache(JNIEnv* env)
{
#if (defined(J9VM_GC_JNI_ARRAY_CACHE)) 
	J9VMThread* vmThread = (J9VMThread*)env;
	OMRPORT_ACCESS_FROM_J9VMTHREAD(vmThread);

	/* 
	 * check to see if memory functions have been overridden (e.g. -memorycheck)
	 * If so, defect the JNI cache so that memory is checked immediately.
	 */
	if (omrport_isFunctionOverridden(offsetof(OMRPortLibrary, mem_free_memory))) {
		vmThread->javaVM->internalVMFunctions->cleanupVMThreadJniArrayCache(vmThread);
	}
#endif /* J9VM_GC_JNI_ARRAY_CACHE */
}

/*
 * Calculate the CRC for an array the slow way for non-contiguous arrays.
 */
static U_32 
calculateArrayCrc(J9VMThread *vmThread, J9IndexableObject *arrayPtr)
{
	U_32 arrayCrc;
	U_32 numElements;
	U_32 index;
	J9UTF8* arrayClassName;

	arrayCrc = j9crc32(0, NULL, 0);

	arrayClassName = J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmThread, arrayPtr)->romClass);
	numElements = J9INDEXABLEOBJECT_SIZE(vmThread, arrayPtr);

	/* all primitive array classes have names like [Z, [J, etc. */
	switch (J9UTF8_DATA(arrayClassName)[1]) {
	case 'Z':
		for (index = 0; index < numElements; index++) {
			jboolean element = J9JAVAARRAYOFBOOLEAN_LOAD(vmThread, arrayPtr, index);
			arrayCrc = j9crc32(arrayCrc, (U_8*)&element, sizeof(element));
		}
		break;
	case 'B':
		for (index = 0; index < numElements; index++) {
			jbyte element = J9JAVAARRAYOFBYTE_LOAD(vmThread, arrayPtr, index);
			arrayCrc = j9crc32(arrayCrc, (U_8*)&element, sizeof(element));
		}
		break;
	case 'C':
		for (index = 0; index < numElements; index++) {
			jchar element = J9JAVAARRAYOFCHAR_LOAD(vmThread, arrayPtr, index);
			arrayCrc = j9crc32(arrayCrc, (U_8*)&element, sizeof(element));
		}
		break;
	case 'D':
		for (index = 0; index < numElements; index++) {
			U_64 element = J9JAVAARRAYOFDOUBLE_LOAD(vmThread, arrayPtr, index);
			arrayCrc = j9crc32(arrayCrc, (U_8*)&element, sizeof(element));
		}
		break;
	case 'F':
		for (index = 0; index < numElements; index++) {
			U_32 element = J9JAVAARRAYOFFLOAT_LOAD(vmThread, arrayPtr, index);
			arrayCrc = j9crc32(arrayCrc, (U_8*)&element, sizeof(element));
		}
		break;
	case 'I':
		for (index = 0; index < numElements; index++) {
			jint element = J9JAVAARRAYOFINT_LOAD(vmThread, arrayPtr, index);
			arrayCrc = j9crc32(arrayCrc, (U_8*)&element, sizeof(element));
		}
		break;
	case 'J':
		for (index = 0; index < numElements; index++) {
			jlong element = J9JAVAARRAYOFLONG_LOAD(vmThread, arrayPtr, index);
			arrayCrc = j9crc32(arrayCrc, (U_8*)&element, sizeof(element));
		}
		break;
	case 'S':
		for (index = 0; index < numElements; index++) {
			jshort element = J9JAVAARRAYOFSHORT_LOAD(vmThread, arrayPtr, index);
			arrayCrc = j9crc32(arrayCrc, (U_8*)&element, sizeof(element));
		}
		break;
	default:
		/* impossible */
		arrayCrc = 0xDEAD8096;
	}

	return arrayCrc;
}

