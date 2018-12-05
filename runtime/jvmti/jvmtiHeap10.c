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

#include "jvmtiHelpers.h"
#include "j9modron.h"
#include "jvmti_internal.h"
#include "HeapIteratorAPI.h"
#include "j9cp.h"
#include "omrgcconsts.h"

typedef struct J9JVMTIHeapIteratorData {
	J9JVMTIEnv * env;
	jvmtiHeapObjectFilter filter;
	jvmtiHeapObjectCallback callback;
	void * userData;
	J9Class * clazz;
} J9JVMTIHeapIteratorData;

typedef struct J9JVMTIObjectIteratorData {
	J9JVMTIEnv * env;
	jvmtiHeapRootCallback heapRootCallback;
	jvmtiStackReferenceCallback stackRefCallback;
	jvmtiObjectReferenceCallback objectRefCallback;
	void * userData;
} J9JVMTIObjectIteratorData;

typedef enum J9JVMTIHeapEventType {
	J9JVMTI_HEAP_EVENT_NONE,
	J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW,
	J9JVMTI_HEAP_EVENT_ROOT,
	J9JVMTI_HEAP_EVENT_STACK,
	J9JVMTI_HEAP_EVENT_OBJECT
} J9JVMTIHeapEventType;

typedef struct J9JVMTIHeapEvent {
	J9JVMTIHeapEventType type;
	jvmtiObjectReferenceKind refKind;
	jvmtiHeapRootKind rootKind;
	IDATA index;
} J9JVMTIHeapEvent;

static jvmtiIterationControl processHeapRoot (J9JVMTIObjectIteratorData * data, J9JVMTIObjectTag * entry, jlong size, jlong classTag, jvmtiHeapRootKind kind);
static jvmtiIterationControl processStackRoot (J9JVMTIObjectIteratorData * data, J9JVMTIObjectTag * entry, jlong size, jlong classTag, J9StackWalkState * walkState);
static jvmtiIterationControl wrapHeapIterationCallback(J9JavaVM * vm, J9MM_IterateObjectDescriptor *objectDesc, void * userData);
static J9JVMTIHeapEvent mapEventType (J9VMThread *vmThread, J9JVMTIObjectIteratorData * data, IDATA type, IDATA index, j9object_t referrer, j9object_t object);
static jvmtiIterationControl wrapObjectIterationCallback (j9object_t *slotPtr, j9object_t referrer, void *userData, IDATA type, IDATA referrerIndex, IDATA wasReportedBefore);
static jvmtiIterationControl processObjectRef (J9JVMTIObjectIteratorData * data, J9JVMTIObjectTag * entry, jlong size, jlong classTag, jvmtiObjectReferenceKind kind, jlong referrerTag, jint referrerIndex);


jvmtiError JNICALL
jvmtiIterateOverHeap(jvmtiEnv* env,
	jvmtiHeapObjectFilter object_filter,
	jvmtiHeapObjectCallback heap_object_callback,
	const void * user_data)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiIterateOverHeap_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9JVMTIHeapIteratorData iteratorData;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_tag_objects);

		ENSURE_VALID_HEAP_OBJECT_FILTER(object_filter);
		ENSURE_NON_NULL(heap_object_callback);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		ensureHeapWalkable(currentThread);

		iteratorData.env = (J9JVMTIEnv *)env;
		iteratorData.filter = object_filter;
		iteratorData.callback = (jvmtiHeapObjectCallback) J9_COMPATIBLE_FUNCTION_POINTER( heap_object_callback );
		iteratorData.userData = (void *) user_data;
		iteratorData.clazz = 0;

		/* Walk the heap */
		vm->memoryManagerFunctions->j9mm_iterate_all_objects(vm, vm->portLibrary, 0, wrapHeapIterationCallback, &iteratorData);

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiIterateOverHeap);
}

jvmtiError JNICALL
jvmtiIterateOverInstancesOfClass(jvmtiEnv* env,
	jclass klass,
	jvmtiHeapObjectFilter object_filter,
	jvmtiHeapObjectCallback heap_object_callback,
	const void * user_data)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiIterateOverInstancesOfClass_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9JVMTIHeapIteratorData iteratorData;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_tag_objects);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_VALID_HEAP_OBJECT_FILTER(object_filter);
		ENSURE_NON_NULL(heap_object_callback);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		ensureHeapWalkable(currentThread);

		iteratorData.env = (J9JVMTIEnv *)env;
		iteratorData.filter = object_filter;
		iteratorData.callback = (jvmtiHeapObjectCallback) J9_COMPATIBLE_FUNCTION_POINTER( heap_object_callback );
		iteratorData.userData = (void *) user_data;
		iteratorData.clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);

		/* Check class has not been unloaded and walk the heap */
		if ( iteratorData.clazz ) {
			vm->memoryManagerFunctions->j9mm_iterate_all_objects(vm, vm->portLibrary, 0, wrapHeapIterationCallback, &iteratorData);
		} else {
			rc = JVMTI_ERROR_INVALID_CLASS;
		}

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiIterateOverInstancesOfClass);
}


jvmtiError JNICALL
jvmtiIterateOverObjectsReachableFromObject(jvmtiEnv* env,
	jobject object,
	jvmtiObjectReferenceCallback object_reference_callback,
	const void * user_data)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiIterateOverObjectsReachableFromObject_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		j9object_t slot;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_tag_objects);

		ENSURE_JOBJECT_NON_NULL(object);
		ENSURE_NON_NULL(object_reference_callback);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		ensureHeapWalkable(currentThread);

		slot = *(j9object_t*)object;

		if ( slot != NULL ) {
			J9JVMTIObjectIteratorData iteratorData;

			iteratorData.env = (J9JVMTIEnv *)env;
			iteratorData.heapRootCallback = NULL;
			iteratorData.stackRefCallback = NULL;
			iteratorData.objectRefCallback = (jvmtiObjectReferenceCallback) J9_COMPATIBLE_FUNCTION_POINTER( object_reference_callback );
			iteratorData.userData = (void *) user_data;

			/* Walk all slots that are reachable from this slot */
			vm->memoryManagerFunctions->j9gc_ext_reachable_from_object_do(currentThread, slot, wrapObjectIterationCallback, &iteratorData, 0);
		}

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiIterateOverObjectsReachableFromObject);
}


jvmtiError JNICALL
jvmtiIterateOverReachableObjects(jvmtiEnv* env,
	jvmtiHeapRootCallback heap_root_callback,
	jvmtiStackReferenceCallback stack_ref_callback,
	jvmtiObjectReferenceCallback object_ref_callback,
	const void * user_data)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiIterateOverReachableObjects_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9JVMTIObjectIteratorData iteratorData;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_tag_objects);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		ensureHeapWalkable(currentThread);

		iteratorData.env = (J9JVMTIEnv *)env;
		iteratorData.heapRootCallback = (jvmtiHeapRootCallback) J9_COMPATIBLE_FUNCTION_POINTER( heap_root_callback );
		iteratorData.stackRefCallback = (jvmtiStackReferenceCallback) J9_COMPATIBLE_FUNCTION_POINTER( stack_ref_callback );
		iteratorData.objectRefCallback = (jvmtiObjectReferenceCallback) J9_COMPATIBLE_FUNCTION_POINTER( object_ref_callback );
		iteratorData.userData = (void *) user_data;

		/* Walk all slots that are reachable from this slot */
		vm->memoryManagerFunctions->j9gc_ext_reachable_objects_do(currentThread, wrapObjectIterationCallback, &iteratorData,
			J9_MU_WALK_SKIP_JVMTI_TAG_TABLES | J9_MU_WALK_TRACK_VISIBLE_FRAME_DEPTH);

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiIterateOverReachableObjects);
}


static J9JVMTIHeapEvent
mapEventType(J9VMThread *vmThread, J9JVMTIObjectIteratorData * data, IDATA type, IDATA index, j9object_t referrer, j9object_t object)
{
	J9JVMTIHeapEvent event = {0};
	J9Class *clazz;
	J9JavaVM* vm = data->env->vm;

	event.type = J9JVMTI_HEAP_EVENT_NONE;
	event.index = index;

	/* BOGUS: current constant pool slime index: +1 (class) +3 (array class components) = 4 */
	switch (type) {

		case J9GC_ROOT_TYPE_JNI_GLOBAL:
		case J9GC_ROOT_TYPE_JNI_WEAK_GLOBAL:
			event.type = J9JVMTI_HEAP_EVENT_ROOT;
			event.rootKind = JVMTI_HEAP_ROOT_JNI_GLOBAL;
			break;

		case J9GC_ROOT_TYPE_VM_CLASS_SLOT:
			event.type = J9JVMTI_HEAP_EVENT_ROOT;
			event.rootKind = JVMTI_HEAP_ROOT_SYSTEM_CLASS;
			break;

		case J9GC_ROOT_TYPE_CLASS:
			/* Do not report or follow classes that dont belong to app or system classloaders, Jazz 33138 */
			clazz = J9OBJECT_CLAZZ(vmThread, object);
			if ((vm->systemClassLoader == clazz->classLoader) || (vm->applicationClassLoader == clazz->classLoader)) {
				event.type = J9JVMTI_HEAP_EVENT_ROOT;
				event.refKind = JVMTI_REFERENCE_CLASS;
				event.rootKind = JVMTI_HEAP_ROOT_SYSTEM_CLASS;
			} else {
				event.refKind = JVMTI_REFERENCE_CLASS;
				event.type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			}
			break;

		case J9GC_ROOT_TYPE_MONITOR:
		case J9GC_ROOT_TYPE_THREAD_MONITOR:
			event.type = J9JVMTI_HEAP_EVENT_ROOT;
			event.rootKind = JVMTI_HEAP_ROOT_MONITOR;
			break;

		case J9GC_ROOT_TYPE_STACK_SLOT:
			event.type = J9JVMTI_HEAP_EVENT_STACK;
			event.rootKind = JVMTI_HEAP_ROOT_STACK_LOCAL;
			break;

		case J9GC_ROOT_TYPE_JNI_LOCAL:
			event.type = J9JVMTI_HEAP_EVENT_ROOT;
			event.rootKind = JVMTI_HEAP_ROOT_JNI_LOCAL;
			break;

		case J9GC_ROOT_TYPE_THREAD_SLOT:
			event.type = J9JVMTI_HEAP_EVENT_ROOT;
			event.rootKind = JVMTI_HEAP_ROOT_THREAD;
			break;

		case J9GC_ROOT_TYPE_FINALIZABLE_OBJECT:
		case J9GC_ROOT_TYPE_UNFINALIZED_OBJECT:
			event.type = J9JVMTI_HEAP_EVENT_ROOT;
			event.rootKind = JVMTI_HEAP_ROOT_OTHER;
			break;


		case J9GC_ROOT_TYPE_STRING_TABLE:
		case J9GC_ROOT_TYPE_REMEMBERED_SET:
		case J9GC_ROOT_TYPE_OWNABLE_SYNCHRONIZER_OBJECT:
			event.type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			event.rootKind = JVMTI_HEAP_ROOT_OTHER;
			break;

		case J9GC_REFERENCE_TYPE_CLASS:
			/* BOGUS: Classes return their superclass for the ref_type_class.. slime j.l.c. in at the end of the constant pool */ 
			if (J9VM_IS_INITIALIZED_HEAPCLASS(vmThread, referrer)) {
				clazz = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, referrer);
				event.type = J9JVMTI_HEAP_EVENT_OBJECT;
				event.refKind = JVMTI_REFERENCE_CONSTANT_POOL;
				event.index = 1 + (clazz->romClass->ramConstantPoolCount * 2);
			} else {
				event.type = J9JVMTI_HEAP_EVENT_OBJECT;
				event.refKind = JVMTI_REFERENCE_CLASS;
			}
			break;

		case J9GC_REFERENCE_TYPE_SUPERCLASS:
			/* only report the direct superclass */
			if (0 == event.index) {
				event.type = J9JVMTI_HEAP_EVENT_OBJECT;
				event.refKind = JVMTI_REFERENCE_CLASS;
			} else {
				event.type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			}
			break;

		case J9GC_REFERENCE_TYPE_CLASS_ARRAY_CLASS:
			/* BOGUS: Slime the component classes onto the end of the constant pool after the class */ 
			Assert_JVMTI_true(J9VM_IS_INITIALIZED_HEAPCLASS(vmThread, referrer));
			clazz = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, referrer);
			event.type = J9JVMTI_HEAP_EVENT_OBJECT;
			event.refKind = JVMTI_REFERENCE_CONSTANT_POOL;
			event.index += 2 + (clazz->romClass->ramConstantPoolCount * 2);
			break;

		case J9GC_REFERENCE_TYPE_FIELD:
		case J9GC_REFERENCE_TYPE_WEAK_REFERENCE:
			event.type = J9JVMTI_HEAP_EVENT_OBJECT;
			event.refKind = JVMTI_REFERENCE_FIELD;
			break;

		case J9GC_REFERENCE_TYPE_ARRAY:
			event.type = J9JVMTI_HEAP_EVENT_OBJECT;
			event.refKind = JVMTI_REFERENCE_ARRAY_ELEMENT;
			break;

		case J9GC_REFERENCE_TYPE_CLASSLOADER:
			event.type = J9JVMTI_HEAP_EVENT_OBJECT;
			event.refKind = JVMTI_REFERENCE_CLASS_LOADER;
			break;

		case J9GC_REFERENCE_TYPE_PROTECTION_DOMAIN:
			event.type = J9JVMTI_HEAP_EVENT_OBJECT;
			event.refKind = JVMTI_REFERENCE_PROTECTION_DOMAIN;
			break;

		case J9GC_REFERENCE_TYPE_INTERFACE:
			event.type = J9JVMTI_HEAP_EVENT_OBJECT;
			event.refKind = JVMTI_REFERENCE_INTERFACE;
			break;

		case J9GC_REFERENCE_TYPE_STATIC:
			event.type = J9JVMTI_HEAP_EVENT_OBJECT;
			event.refKind = JVMTI_REFERENCE_STATIC_FIELD;
			break;

		case J9GC_REFERENCE_TYPE_CONSTANT_POOL:
			event.type = J9JVMTI_HEAP_EVENT_OBJECT;
			event.refKind = JVMTI_REFERENCE_CONSTANT_POOL;
			/* reference chain walker returns constant pool index based from 0, jvmti wants it based from 1 */
			event.index++;
			break;

		case J9GC_REFERENCE_TYPE_CLASS_NAME_STRING:
			event.type = J9JVMTI_HEAP_EVENT_OBJECT;
			event.refKind = JVMTI_REFERENCE_FIELD;
			break;

		case J9GC_ROOT_TYPE_CLASSLOADER:
		case J9GC_ROOT_TYPE_JVMTI_TAG_REF:
		case J9GC_REFERENCE_TYPE_UNKNOWN:
		default:
			event.type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			break;
	}

	/* Cancel event if no callback */

	if (event.type == J9JVMTI_HEAP_EVENT_ROOT && data->heapRootCallback == NULL) {
		event.type = J9JVMTI_HEAP_EVENT_NONE;
	}

	if (event.type == J9JVMTI_HEAP_EVENT_STACK && data->stackRefCallback == NULL) {
		event.type = J9JVMTI_HEAP_EVENT_NONE;
	}

	if (event.type == J9JVMTI_HEAP_EVENT_OBJECT && data->objectRefCallback == NULL) {
		event.type = J9JVMTI_HEAP_EVENT_NONE;
	}

	return event;
}


static jvmtiIterationControl
processHeapRoot(J9JVMTIObjectIteratorData * data, J9JVMTIObjectTag * entry, jlong size, jlong classTag, jvmtiHeapRootKind kind)
{
	return data->heapRootCallback(
		kind,
		classTag,
		size,
		&entry->tag,
		data->userData);
}


static jvmtiIterationControl
processObjectRef(J9JVMTIObjectIteratorData * data, J9JVMTIObjectTag * entry, jlong size, jlong classTag, jvmtiObjectReferenceKind kind, jlong referrerTag, jint referrerIndex)
{
	return data->objectRefCallback(
		kind,
		classTag,
		size,
		&entry->tag,
		referrerTag,
		referrerIndex,
		data->userData);
}


static jvmtiIterationControl
processStackRoot(J9JVMTIObjectIteratorData * data, J9JVMTIObjectTag * entry, jlong size, jlong classTag, J9StackWalkState * walkState)
{
	jvmtiHeapRootKind kind;
	jint slot = (jint) walkState->slotIndex;
	jint depth = (jint) walkState->framesWalked;
	J9Method * ramMethod = walkState->method;
	jmethodID method;
	J9JVMTIObjectTag search;
	J9JVMTIObjectTag * result;

	/* Convert internal slot type to JVMTI type */

	switch (walkState->slotType) {
		case J9_STACKWALK_SLOT_TYPE_JNI_LOCAL:
			kind = JVMTI_HEAP_ROOT_JNI_LOCAL;
			break;
		case J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL:
			kind = JVMTI_HEAP_ROOT_STACK_LOCAL;
			break;
		default:
			kind = JVMTI_HEAP_ROOT_JNI_LOCAL;
			ramMethod = NULL;
			break;
	}

	/* If there's no method, set slot, method and depth to -1 */

	if (ramMethod == NULL) {
		slot = -1;
		depth = -1;
		method = (jmethodID) -1;
	} else {
		/* Cheating here - should be current thread, but the walk thread will do */
		method = getCurrentMethodID(walkState->walkThread, ramMethod);
		if (method == NULL) {
			slot = -1;
			depth = -1;
			method = (jmethodID) -1;
		}
	}

	/* Find thread tag */

	search.ref = (j9object_t) walkState->walkThread->threadObject;
	result = hashTableFind(data->env->objectTagTable, &search);
	
	/* Call the callback */

	return data->stackRefCallback(
		kind,
		classTag,
		size,
		&entry->tag,
		result ? result->tag : 0,
		depth,
		method,
		slot,
		data->userData);
}


static jvmtiIterationControl
wrapHeapIterationCallback(J9JavaVM * vm, J9MM_IterateObjectDescriptor *objectDesc, void * userData)
{
	j9object_t object = objectDesc->object;
	J9JVMTIHeapIteratorData * iteratorData = userData;

	if ( iteratorData->clazz == NULL ||isSameOrSuperClassOf(iteratorData->clazz, J9OBJECT_CLAZZ_VM(vm, object))) {
		J9JVMTIObjectTag entry;
		J9JVMTIObjectTag * objectTag;

		/* Do not report uninitialized classes */
		if (J9VM_IS_UNINITIALIZED_HEAPCLASS_VM(vm, object)) {
			return JVMTI_ITERATION_CONTINUE;
		}

		entry.ref = object;
		objectTag = hashTableFind(iteratorData->env->objectTagTable, &entry);

		if ( (iteratorData->filter == JVMTI_HEAP_OBJECT_EITHER) ||
		     (iteratorData->filter == JVMTI_HEAP_OBJECT_TAGGED && objectTag != NULL) ||
		     (iteratorData->filter == JVMTI_HEAP_OBJECT_UNTAGGED && objectTag == NULL) )
		{
			jvmtiIterationControl rc;
			J9JVMTIObjectTag * classTag;
			jlong objectSize;
			jlong tag = objectTag ? objectTag->tag : 0;
			J9Class *clazz;

			clazz = J9OBJECT_CLAZZ_VM(vm, object);
			entry.ref = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
			classTag = hashTableFind(iteratorData->env->objectTagTable, &entry);

			objectSize = getObjectSize(vm, object);

			rc = iteratorData->callback(
					classTag ? classTag->tag : 0,
					objectSize,
					&tag,
					iteratorData->userData);

			if (objectTag) {
				/* object was tagged before callback */
				if (tag) {
					/* still tagged, update the tag */
					objectTag->tag = tag;
				} else {
					/* no longer tagged, remove the table entry */
					entry.ref = object;
					hashTableRemove(iteratorData->env->objectTagTable, &entry);
				}
			} else {
				/* object was untagged before callback */
				if (tag) {
					/* now tagged, add table entry */
					entry.ref = object;
					entry.tag = tag;
					hashTableAdd(iteratorData->env->objectTagTable, &entry);
				}
			}
		
			return rc;
		}
	}

	return JVMTI_ITERATION_CONTINUE;
}


static jvmtiIterationControl
wrapObjectIterationCallback(j9object_t *slotPtr, j9object_t referrer, void *userData, IDATA type, IDATA referrerIndex, IDATA wasReportedBefore)
{
	jvmtiIterationControl returnCode = JVMTI_ITERATION_CONTINUE;
	J9JVMTIObjectIteratorData * iteratorData = userData;
	j9object_t object = *slotPtr;
	J9JavaVM *vm = JAVAVM_FROM_ENV(iteratorData->env);
	J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);
	J9JVMTIHeapEvent event = mapEventType(vmThread, iteratorData, type, referrerIndex, referrer, object);

	if (( event.type != J9JVMTI_HEAP_EVENT_NONE ) && ( event.type != J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW )) {
		J9JVMTIObjectTag entry;
		J9JVMTIObjectTag * result;
		J9Class* clazz;
		jlong classTag;
		jlong referrerTag = 0;
		jlong objectSize;

		/* Do not report uninitialized classes */
		if (J9VM_IS_UNINITIALIZED_HEAPCLASS(vmThread, object)) {
			return JVMTI_ITERATION_IGNORE;
		}

		clazz = J9OBJECT_CLAZZ(vmThread, object);
		entry.ref = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
		result = hashTableFind(iteratorData->env->objectTagTable, &entry);
		classTag = result ? result->tag : 0;

		if ( referrer && (event.type != J9JVMTI_HEAP_EVENT_STACK)) {
			entry.ref = referrer;
			result = hashTableFind(iteratorData->env->objectTagTable, &entry);
			referrerTag = result ? result->tag : 0;
		}

		objectSize = getObjectSize(vm, object);

		entry.ref = object;
		entry.tag = 0;
		result = hashTableFind(iteratorData->env->objectTagTable, &entry);
		if ( result == NULL ) {
			result = &entry;
		}

		switch (event.type) {
			case J9JVMTI_HEAP_EVENT_ROOT:
				returnCode = processHeapRoot(iteratorData, result, objectSize, classTag, event.rootKind);
				break;
			case J9JVMTI_HEAP_EVENT_STACK:
				returnCode = processStackRoot(iteratorData, result, objectSize, classTag, (J9StackWalkState *)  referrer);
				break;
			case J9JVMTI_HEAP_EVENT_OBJECT:
				returnCode = processObjectRef(iteratorData, result, objectSize, classTag, event.refKind, referrerTag, (jint)event.index);
				break;
			default:
				break;
		}

		if ( &entry == result ) {
			/* Tag wasn't set, but now is... */
			if (result->tag != 0) {
				hashTableAdd(iteratorData->env->objectTagTable, result);
			}
		} else {
			/* Tag was set, but now isn't... */
			if (result->tag == 0) {
				hashTableRemove(iteratorData->env->objectTagTable, result);
			}
		}
	} else if (J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW == event.type) {
		returnCode = JVMTI_ITERATION_IGNORE;
	}

	return returnCode;
}



