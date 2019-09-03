/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

#include <stdlib.h>
#include <string.h>

#include "iohelp.h"
#include "j9.h"
#include "omrgcconsts.h"
#include "j9jclnls.h"
#include "j9port.h"
#include "j9protos.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jni.h"
#include "ut_j9jcl.h"

/* require for new string merging primitives */
#include "stackwalk.h"
#include "omrlinkedlist.h"
#include "j9cp.h"
#include "rommeth.h"
#include "vmhook.h"
#include "HeapIteratorAPI.h"


typedef struct AllInstancesData {
	J9Class *clazz;             /* class being looked for */
	J9VMThread *vmThread;       
	J9IndexableObject *target;  /* the array */
	UDATA size;                 /* size of the array */
	UDATA storeIndex;           /* current index being stored */
	UDATA instanceCount;        /* count of instances found regardless if they are being collected in the array or not */
} AllInstancesData;

typedef struct J9HeapStatisticsTableEntry {
	J9Class *clazz; /* hash table key */
	UDATA objectCount; /* number of instances of the class */
	UDATA objectSize;
	UDATA aggregateSize;
} J9HeapStatisticsTableEntry;

static UDATA hasConstructor(J9VMThread *vmThread, J9StackWalkState *state);
static jvmtiIterationControl collectInstances(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objDesc, void *state);
static int hasActiveConstructor(J9VMThread *vmThread, J9Class *clazz);
static UDATA allInstances (JNIEnv * env, jclass clazz, jobjectArray target);
static J9HashTable *collectHeapStatistics(J9VMThread *vmThread);
static jvmtiIterationControl updateHeapStatistics(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objDesc, void *state);
static UDATA heapStatisticsHashEqualFn(void *leftKey, void *rightKey, void *userData);
static UDATA heapStatisticsHashFn(void *key, void *userData);
static UDATA printHeapStatistics(JNIEnv *env,J9HeapStatisticsTableEntry **statsArray,
		UDATA numClasses, char *stringBuffer, UDATA bufferSize);
static int compareByAggregateSize(const void *a, const void *b);

void JNICALL
Java_com_ibm_oti_vm_VM_localGC(JNIEnv *env, jclass clazz)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vm->memoryManagerFunctions->j9gc_modron_local_collect(currentThread);
	vmFuncs->internalExitVMToJNI(currentThread);
}


void JNICALL
Java_com_ibm_oti_vm_VM_globalGC(JNIEnv *env, jclass clazz)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vm->memoryManagerFunctions->j9gc_modron_global_collect(currentThread);
	vmFuncs->internalExitVMToJNI(currentThread);
}


jint JNICALL Java_com_ibm_oti_vm_VM_getClassPathCount(JNIEnv * env, jclass clazz)
{
#if defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)
	J9JavaVM *javaVM;
	J9ClassLoader *classLoader;

	javaVM = ((J9VMThread *) env)->javaVM;
	classLoader = (J9ClassLoader*)javaVM->systemClassLoader;
	return (jint)classLoader->classPathEntryCount;
#else
	/* No dynamic loader. */
	return 0;
#endif
}





/* Java_test_StringCompress_allInstances
 * Fill in all the instances of clazz into target.
 * return the count of the instances in the system
 * if the count is > than the array size, there are more strings than fit.
 * if count is <= the array size, the array may have null references. 
 * static void allInstances (Class, Object[])
 */
jint JNICALL 
Java_com_ibm_oti_vm_VM_allInstances(JNIEnv * env, jclass unused, jclass clazz, jobjectArray target ) 
{
	jint count = 0;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM;
	
	if (OMR_GC_ALLOCATION_TYPE_SEGREGATED != javaVM->gcAllocationType) {
		UDATA savedGCFlags = 0;

		/* enterVMFromJNI(vmThread); */
		javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);
		javaVM->internalVMFunctions->acquireExclusiveVMAccess(vmThread);

		savedGCFlags = vmThread->javaVM->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK;
		if (savedGCFlags == 0) { /* if the flags was not set, you set it */
				vmThread->javaVM->requiredDebugAttributes |= J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK;
		}
		javaVM->memoryManagerFunctions->j9gc_modron_global_collect(vmThread);
		if (savedGCFlags == 0) { /* if you set it, you have to unset it */
			vmThread->javaVM->requiredDebugAttributes &= ~J9VM_DEBUG_ATTRIBUTE_ALLOW_USER_HEAP_WALK;
		}
	 	count = (jint)allInstances(env, clazz, target);

		javaVM->internalVMFunctions->releaseExclusiveVMAccess(vmThread);
		javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);
	}

	return count;
}

/**
 * Return a String object containing a summary of the classes on the heap, 
 * the number of instances, and their aggregate size.
 * This string inserts Unix-style line separators.  The caller is responsible for translating them if necessary.
 */
jstring JNICALL
Java_openj9_tools_attach_diagnostics_base_DiagnosticUtils_getHeapClassStatisticsImpl(JNIEnv * env, jclass unused)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9HashTable *statsTable = NULL;
	J9HashTableState hashTableState;
	BOOLEAN outOfMemory = FALSE;
	j9object_t stringObject = NULL;
	J9HeapStatisticsTableEntry **statsArray = NULL;
	jstring stringObjectRef = NULL;
	UDATA numClasses = 0;

	PORT_ACCESS_FROM_ENV(env);

	vmFuncs->internalEnterVMFromJNI(vmThread);

	vmFuncs->acquireExclusiveVMAccess(vmThread);
	statsTable = collectHeapStatistics(vmThread);
	vmFuncs->releaseExclusiveVMAccess(vmThread);

	if (NULL != statsTable) {
		numClasses = hashTableGetCount(statsTable);
		statsArray = j9mem_allocate_memory(numClasses * sizeof(J9HeapStatisticsTableEntry*), J9MEM_CATEGORY_VM_JCL);
	}
	if (NULL == statsArray) {
		outOfMemory = TRUE;
	} else {
		UDATA cursor = 0;
		UDATA printedLength = 0;
		UDATA bufferSize = 0;
		J9HeapStatisticsTableEntry *entry = (J9HeapStatisticsTableEntry *) hashTableStartDo(statsTable, &hashTableState);
		/* build a list of pointers to the hash table entries */
		while (NULL != entry) {
			entry->aggregateSize = entry->objectSize * entry->objectCount;
			statsArray[cursor] = entry;
			cursor += 1;
			entry = (J9HeapStatisticsTableEntry *) hashTableNextDo(&hashTableState);
		}
		numClasses = cursor; /* adjust the length in case the hash table contained nulls */
		qsort(statsArray, numClasses, sizeof(J9HeapStatisticsTableEntry*), compareByAggregateSize);
		do {
			char *stringBuffer = NULL;
			bufferSize += numClasses * 80; /* try incrementally larger sizes */
			stringBuffer = (char *) j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_VM_JCL);
			if (NULL == stringBuffer) {
				outOfMemory = TRUE;
				break;
			}
			printedLength = printHeapStatistics(env, statsArray, numClasses, stringBuffer, bufferSize);
			if (printedLength > 0) {
				stringObject = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmThread,
						(U_8 *) stringBuffer, printedLength, J9_STR_XLAT);
				stringObjectRef = vmFuncs->j9jni_createLocalRef(env, stringObject);
			}
			j9mem_free_memory(stringBuffer);
		} while (0 == printedLength);
		/* Need to keep the table until this point since statsArray contained pointer to its members. */
		hashTableFree(statsTable);
		j9mem_free_memory(statsArray);
	}

	if (outOfMemory) {
		Trc_JCL_heapStatisticsOOM(vmThread);
		vm->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
	}
	vm->internalVMFunctions->internalExitVMToJNI(vmThread);

	return stringObjectRef;
}

/**
 * Arguments are pointers to entries in the statsArray,
 * which are themselves pointers to entry in the hash table.
 * Compare such that the list is sorted in descending order.
 */
static int
compareByAggregateSize(const void *a, const void *b)
{
	const J9HeapStatisticsTableEntry *aEntry = *(const J9HeapStatisticsTableEntry* const *) a;
	const J9HeapStatisticsTableEntry *bEntry = *(const J9HeapStatisticsTableEntry* const *) b;
	int result = 0;
	if (bEntry->aggregateSize < aEntry->aggregateSize) {
		result = -1;
	} else if (bEntry->aggregateSize > aEntry->aggregateSize) {
		result = 1;
	}
	return result;
}

static UDATA
printHeapStatistics(JNIEnv *env,J9HeapStatisticsTableEntry **statsArray,
		UDATA numClasses, char *stringBuffer, UDATA bufferSize)
{
	char *bufferCursor = stringBuffer;
	UDATA classCursor = 0;
	UDATA cumulativeCount = 0;
	UDATA cumulativeSize = 0;
	UDATA result = 0;

	PORT_ACCESS_FROM_ENV(env);

	result = j9str_printf(PORTLIB, bufferCursor, bufferSize,
			"%5s %14s %14s    %s\n-------------------------------------------------\n",
			"num", "object count", "total size", "class name"
	);
	bufferCursor += result;
	bufferSize -= result;
	for (classCursor = 0; (result > 0) && (classCursor < numClasses); ++classCursor) {
		J9Class *currentClass = statsArray[classCursor]->clazz;
		result = j9str_printf(PORTLIB, bufferCursor, bufferSize,
				"%5d %14zu %14zu    ",
				classCursor + 1, statsArray[classCursor]->objectCount,
				statsArray[classCursor]->aggregateSize
		);
		bufferCursor += result;
		bufferSize -= result;
		if (J9CLASS_IS_ARRAY(currentClass)) {
			J9ArrayClass *arrayClazz = (J9ArrayClass*)currentClass;
			UDATA arity = arrayClazz->arity;
			J9Class *leafComponentType = arrayClazz->leafComponentType;
			J9ROMClass *leafROMClass = leafComponentType->romClass;
			J9UTF8 *leafName = J9ROMCLASS_CLASSNAME(leafROMClass);
			UDATA isPrimitive = J9ROMCLASS_IS_PRIMITIVE_TYPE(leafROMClass);
			UDATA i = 0;
			for (i = 0; i < arity; ++i) {
				result = j9str_printf(PORTLIB, bufferCursor, bufferSize, "[");
				bufferCursor += result;
				bufferSize -= result;
			}
			if (isPrimitive) {
				result = j9str_printf(PORTLIB, bufferCursor, bufferSize, "%c\n",
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(leafComponentType->arrayClass->romClass))[1]);
			} else {
				result = j9str_printf(PORTLIB, bufferCursor, bufferSize, "L%.*s;\n",
						J9UTF8_LENGTH(leafName), J9UTF8_DATA(leafName));
			}
		} else {
			J9UTF8 *className = J9ROMCLASS_CLASSNAME(currentClass->romClass);
			result = j9str_printf(PORTLIB, bufferCursor, bufferSize, "%.*s\n",
					J9UTF8_LENGTH(className), J9UTF8_DATA(className));
		}
		bufferCursor += result;
		bufferSize -= result;
		cumulativeCount += statsArray[classCursor]->objectCount;
		cumulativeSize += statsArray[classCursor]->aggregateSize;
	}
	result = j9str_printf(PORTLIB, bufferCursor, bufferSize,
			"%5s %14zd %14zd\n",
			"Total", cumulativeCount, cumulativeSize
	);
	bufferCursor += result;
	return (result > 0) ? (bufferCursor - stringBuffer) : 0;
}

/* The string that keeps its original bytes is string1.
 * String2 has its bytes set to be string1-> bytes if the offsets already match and the bytes are not already set to the same value
 * The bytes being set already could happen frequently as this primitive will be used repeatedly to remerge strings in the runtime
*/
jint JNICALL 
Java_com_ibm_oti_vm_VM_setCommonData(JNIEnv * env, jclass unused, jobject string1, jobject string2 ) 
{
	UDATA allowMerge = 0;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *javaVM = vmThread->javaVM; 

	if (OMR_GC_ALLOCATION_TYPE_SEGREGATED != javaVM->gcAllocationType) {
		if (string1 == NULL || string2 == NULL) {
			allowMerge = 0;
		} else {
			j9object_t unwrappedString1 = NULL;
			j9object_t unwrappedString2 = NULL;
			j9object_t stringBytes1 = NULL;
			j9object_t stringBytes2 = NULL;

			allowMerge = 1;
			javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);

			unwrappedString1 = J9_JNI_UNWRAP_REFERENCE(string1);
			unwrappedString2 = J9_JNI_UNWRAP_REFERENCE(string2);

			stringBytes1 = J9VMJAVALANGSTRING_VALUE(vmThread, unwrappedString1);
			stringBytes2 = J9VMJAVALANGSTRING_VALUE(vmThread, unwrappedString2);

			 /* skip merging of bytes if they are already the same. As this primitive is called repeatedly, then its likely the character data will be the same
			 * on repeat calls. 
			 */

			if (stringBytes1 == stringBytes2) {
				allowMerge = 0;
			}

			if (allowMerge) {
				/* set the duplicate string (string2) bytes to be (original) string1's bytes */
				J9VMJAVALANGSTRING_SET_VALUE(vmThread, unwrappedString2, stringBytes1);
			}

			javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);
		}
	}

	
	return (jint)allowMerge;
}


static UDATA 
allInstances (JNIEnv * env, jclass clazz, jobjectArray target) 
{
	J9VMThread *vmThread = (J9VMThread *) env;
	AllInstancesData data;

	J9Class *unwrappedObj = clazz ? J9VM_J9CLASS_FROM_JCLASS(vmThread, clazz) : NULL;
	J9IndexableObject *unwrappedArray = target ? *(J9IndexableObject**)target : NULL;

	if (unwrappedObj == NULL) {
		return 0;
	}

	if (hasActiveConstructor (vmThread, unwrappedObj)) {
			/* printf ("\nFOUND A CONSTRUCTOR for the class, allInstances aborted"); */
		return 0;
	}

	data.clazz = unwrappedObj;
	data.target = unwrappedArray;
	data.vmThread = vmThread;
	if (unwrappedArray != NULL) {
		data.size =  J9INDEXABLEOBJECT_SIZE (vmThread, unwrappedArray);
	} else {
		data.size =  0;
	}
	data.storeIndex = 0;
	data.instanceCount = 0;

	vmThread->javaVM->memoryManagerFunctions->j9mm_iterate_all_objects(vmThread->javaVM, vmThread->javaVM->portLibrary, 0, collectInstances, &data);

	return data.instanceCount;
}


static jvmtiIterationControl
collectInstances (J9JavaVM *vm, J9MM_IterateObjectDescriptor *objDesc, void *state)
{
	AllInstancesData *data = (AllInstancesData *) state;
	j9object_t obj = objDesc->object;

	if (J9OBJECT_CLAZZ_VM(vm, obj) == data->clazz) {
		data->instanceCount++;
		 /* fill in the array only if one was passed in */
		if (data->target != NULL) { 
			 /* if no room, you can ignore the object and not store it */
			if (data->storeIndex < data->size) { 
 				J9JAVAARRAYOFOBJECT_STORE_VM(vm, data->target, (I_32)data->storeIndex, obj);
				data->storeIndex++;
			}
		}
	}
	return JVMTI_ITERATION_CONTINUE;
}


static int  
hasActiveConstructor(J9VMThread *vmThread, J9Class *clazz) 
{
	J9JavaVM *vm = vmThread->javaVM;
	J9VMThread * walkThread;

	walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (walkThread != NULL) {
		J9StackWalkState walkState;
		walkState.walkThread = walkThread;
		walkState.flags = J9_STACKWALK_ITERATE_FRAMES;
		walkState.skipCount = 0;
		walkState.userData1 = (void*) clazz;
		walkState.userData2 = (void*) 0;
		walkState.frameWalkFunction = hasConstructor;

		vm->walkStackFrames(vmThread, &walkState);
		if (walkState.userData2 == (void*)1) {
			return 1;
		}
		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}
	return 0;
}


static UDATA 
hasConstructor(J9VMThread *vmThread, J9StackWalkState *state)
{
	J9Method *method = state->method;
	J9Class *classToFind = (J9Class*) state->userData1;

	if (method == NULL) {
		return J9_STACKWALK_KEEP_ITERATING;
	} else {
		J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
	
		/* if the method is a constructor in the class we're looking for then quit looking for more
		 * a constructor is a non-static (to filter clinit) method which starts with '<'. 
		 */
		if (methodClass == classToFind) {
			if (!(romMethod->modifiers & J9AccStatic)) {
				if (J9UTF8_DATA(methodName)[0] == '<') {
					state->userData2 = (void*)1;
					return J9_STACKWALK_STOP_ITERATING;
				}
			}
		}
		return J9_STACKWALK_KEEP_ITERATING;
	}
}

static J9HashTable *
collectHeapStatistics(J9VMThread *vmThread)
{
	J9JavaVM *vm = vmThread->javaVM;
	J9HashTable *hashTable = hashTableNew(
			OMRPORT_FROM_J9PORT(vm->portLibrary),
			J9_GET_CALLSITE(),
			0, /* let the system choose the initial size of table */
			sizeof(J9HeapStatisticsTableEntry),
			sizeof(U_8*),
			0,
			J9MEM_CATEGORY_CLASSES,
			heapStatisticsHashFn,
			heapStatisticsHashEqualFn,
			NULL,
			vm
	);

	if (NULL != hashTable) {
		if (vm->memoryManagerFunctions->j9mm_iterate_all_objects(vmThread->javaVM,
				vm->portLibrary, 0, updateHeapStatistics, hashTable)
				!= JVMTI_ITERATION_CONTINUE) {
			hashTableFree(hashTable);
			hashTable = NULL;
		}
	}
	return hashTable;
}

static jvmtiIterationControl
updateHeapStatistics(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objDesc, void *state)
{
	J9HashTable *hashTable = (J9HashTable *) state;
	j9object_t obj = objDesc->object;
	J9Class *clazz = J9OBJECT_CLAZZ_VM(vm, obj);
	struct J9HeapStatisticsTableEntry query;
	struct J9HeapStatisticsTableEntry *result = NULL;
	jvmtiIterationControl status = JVMTI_ITERATION_CONTINUE;

	query.clazz = clazz;
	result = hashTableFind(hashTable, &query);
	if (NULL == result) {
		query.objectCount = 1;
		query.objectSize = vm->memoryManagerFunctions->j9gc_get_object_size_in_bytes(vm, obj);
		result = hashTableAdd(hashTable, &query);
		if (NULL == result) {
			J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);
			Trc_JCL_heapStatisticsOOM(vmThread);
			vm->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
			status = JVMTI_ITERATION_ABORT;
		}
	} else {
		result->objectCount += 1;
	}
	return status;
}

static UDATA
heapStatisticsHashEqualFn(void *leftKey, void *rightKey, void *userData)
{
	J9HeapStatisticsTableEntry *entryA = leftKey;
	J9HeapStatisticsTableEntry *entryB = rightKey;
	return entryA->clazz == entryB->clazz;
}

static UDATA
heapStatisticsHashFn(void *key, void *userData)
{
	J9HeapStatisticsTableEntry *entry = key;
	return (UDATA) entry->clazz;
}

/*
 * Dump a String to stderr using the port library.
 *
 * @param[in] str - a java.lang.String object
 */
void JNICALL
Java_com_ibm_oti_vm_VM_dumpString(JNIEnv * env, jclass clazz, jstring str)
{
	PORT_ACCESS_FROM_ENV(env);

	if (str == NULL) {
		j9tty_printf(PORTLIB, "null");
	} else {
		/* Note: GetStringUTFChars nul-terminates the resulting chars, even though the spec does not say so */
		const char * utfChars = (const char *) (*env)->GetStringUTFChars(env, str, NULL);

		if (utfChars != NULL) {
			Trc_JCL_com_ibm_oti_vm_VM_dumpString(env, utfChars);
			j9tty_printf(PORTLIB, "%s", utfChars);
			(*env)->ReleaseStringUTFChars(env, str, utfChars);
		}
	}
}
