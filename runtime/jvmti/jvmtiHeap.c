/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "jvmtinls.h"
#include "j9modron.h"
#include "jvmti_internal.h"
#include "HeapIteratorAPI.h"
#include "j9cp.h"
#include "omrgcconsts.h"

#undef JVMTI_HEAP_DEBUG

typedef struct J9JVMTIObjectTagMatch {
	J9JavaVM * vm;
	J9VMThread * currentThread;
	/* search... */
	jint forLen;
	const jlong * forTags;
	/* results... */
	jint count;
	jobject * objects;
	jlong * tags;
} J9JVMTIObjectTagMatch;


/**
 * Helper structure used to obtain referrer and referee tags 
 */
typedef struct jvmtiHeapTags {
	jlong	objectTag;
	jlong	classTag;
	jlong	referrerObjectTag;
	jlong	referrerClassTag;
} jvmtiHeapTags;


typedef enum J9JVMTIHeapEventType {
	J9JVMTI_HEAP_EVENT_NONE,                    /* Do not report a reference, do follow it */
	J9JVMTI_HEAP_EVENT_NONE_REPORT_NOFOLLOW,	/* Report the reference but do not follow it */
	J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW,			/* Do not report or follow a reference */
	J9JVMTI_HEAP_EVENT_ROOT,
	J9JVMTI_HEAP_EVENT_STACK,
	J9JVMTI_HEAP_EVENT_OBJECT
} J9JVMTIHeapEventType;

typedef struct J9JVMTIHeapEvent {
	IDATA                   gcType;
	J9JVMTIHeapEventType	type;
	jvmtiHeapReferenceKind	refKind;
	jvmtiHeapReferenceInfo	refInfo;
	jboolean       		hasRefInfo;
} J9JVMTIHeapEvent;


typedef enum J9JVMTIHeapIterationFlags {
	J9JVMTI_HI_INITIAL_OBJECT_REF = 1,
} J9JVMTIHeapIterationFlags;
 

typedef struct J9JVMTIHeapData {
	J9JVMTIEnv       * env;
	J9VMThread       * currentThread;
	jint               filter;         /** filter flags specified by the user */
	J9Class          * classFilter;    /** class filter specified by the user */
	void             * userData;       /** private user data to be passed back (to the user) via the callbacks */
	J9Class          * clazz;
	jvmtiError         rc;             /** jvmti error code */
	jvmtiIterationControl visitRc;
	J9JVMTIHeapIterationFlags flags;   /** private iteration control flags */

	J9JVMTIHeapEvent   event;         
	j9object_t         referrer;       /** The referrer object */
	j9object_t	       object;         /** The referee (refered to by referrer) */
	jlong	           objectSize;

	jvmtiHeapTags	 tags;

	const jvmtiHeapCallbacks *callbacks;
} J9JVMTIHeapData;




static UDATA copyObjectTags (J9JVMTIObjectTag * entry, J9JVMTIObjectTagMatch * results);
static UDATA countObjectTags (J9JVMTIObjectTag * entry, J9JVMTIObjectTagMatch * results);
static jvmtiIterationControl iterateThroughHeapCallback(J9JavaVM * vm, J9MM_IterateObjectDescriptor *objectDesc, void * userData);

static jvmtiIterationControl wrap_heapReferenceCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData);
static jvmtiIterationControl wrap_heapIterationCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData);
static jvmtiIterationControl wrap_arrayPrimitiveValueCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData);
static jvmtiIterationControl wrap_primitiveFieldCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData, IDATA wasReportedBefore);
static jvmtiIterationControl wrap_stringPrimitiveCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData);

static void updateObjectTag(J9JVMTIHeapData * iteratorData, j9object_t object, jlong * originalTag, jlong newTag);
static jvmtiPrimitiveType getArrayPrimitiveType(J9VMThread * vmThread, j9object_t  object, jlong * primitiveSize);
static jvmtiError getArrayPrimitiveElements(J9JVMTIHeapData * iteratorData, jvmtiPrimitiveType * primitiveType, void **elements, jint elementCount);

static jvmtiIterationControl followReferencesCallback(j9object_t * slotPtr, j9object_t referrer, void *userData, IDATA type, IDATA referrerIndex, IDATA wasReportedBefore);
static void mapEventType(J9JVMTIHeapData * data, IDATA type, jint index, j9object_t referrer, j9object_t object);
static void jvmtiFollowRefs_getTags(J9JVMTIHeapData * iteratorData, j9object_t  referrer, j9object_t  object); 
static UDATA jvmtiHeapFollowRefs_getStackData(J9JVMTIHeapData * iteratorData, J9StackWalkState *walkState);
static IDATA heapReferenceFilter(J9JVMTIHeapData * iteratorData);


#ifdef JVMTI_HEAP_DEBUG
static void jvmtiHeapFollowReferencesPrint(J9JavaVM * vm, j9object_t object, j9object_t referrer, J9JVMTIHeapData * iteratorData);
static const char * jvmtiHeapReferenceKindString(jvmtiHeapReferenceKind refKind);
static const char * jvmtiHeapRootKindString(jvmtiHeapRootKind refKind);
#endif

#ifdef JVMTI_HEAP_DEBUG
#define jvmtiHeapFR_printf(args) printf args
#define jvmtiHeapITH_printf(args) printf args
#else
#define jvmtiHeapFR_printf(args)
#define jvmtiHeapITH_printf(args)
#endif


#define JVMTI_HEAP_ERROR(err) \
	do { \
		iteratorData->rc = (err); \
		goto done; \
	} while(0)

#define JVMTI_HEAP_CHECK_RC(err) \
	do { \
		if ((err != JVMTI_ERROR_NONE)) { \
			goto done; \
		} \
	} while(0)

#define JVMTI_HEAP_CHECK_ITERATION_ABORT(rc) \
	do { \
		if ((rc == JVMTI_ITERATION_ABORT)) { \
			goto done; \
		} \
	} while(0)



jvmtiError JNICALL
jvmtiGetTag(jvmtiEnv* env,
	jobject object,
	jlong* tag_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;
	jlong rv_tag = 0;

	Trc_JVMTI_jvmtiGetTag_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9JVMTIObjectTag entry;
		J9JVMTIObjectTag * objectTag;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);
		ENSURE_CAPABILITY(env, can_tag_objects);

		ENSURE_JOBJECT_NON_NULL(object);
		ENSURE_NON_NULL(tag_ptr);

		entry.ref = *(j9object_t *)object;

		if ( entry.ref ) {

			/* Ensure exclusive access to tag table */
			omrthread_monitor_enter(((J9JVMTIEnv *)env)->mutex);

			objectTag = hashTableFind(((J9JVMTIEnv *)env)->objectTagTable, &entry);
			if (objectTag) {
				rv_tag = objectTag->tag;
			}

			omrthread_monitor_exit(((J9JVMTIEnv *)env)->mutex);

		} else {
			rc = JVMTI_ERROR_INVALID_OBJECT;
		}

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != tag_ptr) {
		*tag_ptr = rv_tag;
	}
	TRACE_JVMTI_RETURN(jvmtiGetTag);
}


jvmtiError JNICALL
jvmtiSetTag(jvmtiEnv* env,
	jobject object,
	jlong tag)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetTag_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9JVMTIObjectTag entry;
		J9JVMTIObjectTag * objectTag;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_START_OR_LIVE(env);
		ENSURE_CAPABILITY(env, can_tag_objects);

		ENSURE_JOBJECT_NON_NULL(object);

		entry.ref = *(j9object_t *)object;
		entry.tag = tag;

		if ( entry.ref ) {

			/* Ensure exclusive access to tag table */
			omrthread_monitor_enter(((J9JVMTIEnv *)env)->mutex);

			objectTag = hashTableFind(((J9JVMTIEnv *)env)->objectTagTable, &entry);
			if (objectTag) {
				if (tag) {
					objectTag->tag = tag;
				} else {
					hashTableRemove(((J9JVMTIEnv *)env)->objectTagTable, &entry);
				}
			} else {
				if (tag) {
					if ( hashTableAdd(((J9JVMTIEnv *)env)->objectTagTable, &entry) == NULL ) {
						rc = JVMTI_ERROR_OUT_OF_MEMORY;
					}
				}
			}

			omrthread_monitor_exit(((J9JVMTIEnv *)env)->mutex);

		} else {
			rc = JVMTI_ERROR_INVALID_OBJECT;
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiSetTag);
}


jvmtiError JNICALL
jvmtiForceGarbageCollection(jvmtiEnv* env)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiForceGarbageCollection_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		/* j9gc_modron_global_collect requires that the current thread has VM access */

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);

		vm->memoryManagerFunctions->j9gc_modron_global_collect(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiForceGarbageCollection);
}






jvmtiError JNICALL
jvmtiGetObjectsWithTags(jvmtiEnv* env,
	jint tag_count,
	const jlong* tags,
	jint* count_ptr,
	jobject** object_result_ptr,
	jlong** tag_result_ptr)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jint rv_count = 0;
	jobject *rv_object_result = NULL;
	jlong *rv_tag_result = NULL;

	Trc_JVMTI_jvmtiGetObjectsWithTags_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		jint i;
		J9JVMTIObjectTagMatch results;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_tag_objects);

		ENSURE_NON_NEGATIVE(tag_count);
		ENSURE_NON_NULL(tags);
		ENSURE_NON_NULL(count_ptr);

		if ( tag_count == 0 ) {
			JVMTI_ERROR(JVMTI_ERROR_NONE);
		}

		/* Verify supplied tags */
		for ( i = 0; i < tag_count; i++ ) {
			if ( tags[i] == 0 ) {
				JVMTI_ERROR(JVMTI_ERROR_ILLEGAL_ARGUMENT);
			}
		}

		/* Ensure exclusive access to tag table */
		omrthread_monitor_enter(((J9JVMTIEnv *)env)->mutex);

		memset(&results, 0, sizeof(J9JVMTIObjectTagMatch));

		results.vm = vm;
		results.currentThread = currentThread;
		results.forTags = tags;
		results.forLen = tag_count;

		hashTableForEachDo(((J9JVMTIEnv *)env)->objectTagTable, (J9HashTableDoFn) countObjectTags, &results);

		if (object_result_ptr) {
			results.objects = j9mem_allocate_memory(sizeof(jobject) * results.count, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (results.objects == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			}
		}

		if (tag_result_ptr && rc == JVMTI_ERROR_NONE) {
			results.tags = j9mem_allocate_memory(sizeof(jlong) * results.count, J9MEM_CATEGORY_JVMTI_ALLOCATE);
			if (results.tags == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			}
		}

		if (rc == JVMTI_ERROR_NONE) {

			rv_count = results.count;

			if (object_result_ptr) {
				rv_object_result = results.objects;
			}
			if (tag_result_ptr) {
				rv_tag_result = results.tags;
			}

			/* Fill in elements ... unwinds results.count */
			if (object_result_ptr || tag_result_ptr) {
				hashTableForEachDo(((J9JVMTIEnv *)env)->objectTagTable, (J9HashTableDoFn) copyObjectTags, &results);
			}

		} else {
			/* Failure: clean up any allocs */
			j9mem_free_memory(results.objects);
			j9mem_free_memory(results.tags);
		}

		omrthread_monitor_exit(((J9JVMTIEnv *)env)->mutex);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	if (NULL != count_ptr) {
		*count_ptr = rv_count;
	}
	if (NULL != object_result_ptr) {
		*object_result_ptr = rv_object_result;
	}
	if (NULL != tag_result_ptr) {
		*tag_result_ptr = rv_tag_result;
	}
	TRACE_JVMTI_RETURN(jvmtiGetObjectsWithTags);
}


static UDATA
copyObjectTags(J9JVMTIObjectTag * entry, J9JVMTIObjectTagMatch * results)
{
	J9JavaVM * vm = results->vm;
	JNIEnv * jniEnv = (JNIEnv *)results->currentThread;

	int i;

	for ( i = 0; i < results->forLen; i++ ) {
		if ( results->forTags[i] == entry->tag ) {
			jint slot = results->count - 1;

			/* Reverse fill */
			if (results->objects) {
				results->objects[slot] = (jobject) vm->internalVMFunctions->j9jni_createLocalRef( jniEnv, entry->ref );
			}
			if (results->tags) {
				results->tags[slot] = entry->tag;
			}
			results->count = slot;

			break;
		}
	}

	return 0;
}


static UDATA
countObjectTags(J9JVMTIObjectTag * entry, J9JVMTIObjectTagMatch * results)
{
	int i;

	for ( i = 0; i < results->forLen; i++ ) {
		if ( results->forTags[i] == entry->tag ) {
			results->count++;
			break;
		}
	}

	return 0;
}







/** 
 * \brief        Follow References starting at <code>initial_object</code> or Heap Roots 
 * \ingroup      jvmti.heap
 * 
 * @param env             jvmti environment
 * @param heap_filter     tag filter flags used to determine which objects are to be reported
 * @param klass           class filter 
 * @param initial_object  object to start following the references from. Set to null to start with the Heap Roots
 * @param callbacks       callbacks to be invoked for each reference type
 * @param user_data       user data to be passed back via the callbacks
 * @return                a jvmtiError value
 */
jvmtiError JNICALL
jvmtiFollowReferences(jvmtiEnv* env, jint heap_filter, jclass klass, jobject initial_object, const jvmtiHeapCallbacks* callbacks,
		      const void* user_data)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;

	Trc_JVMTI_jvmtiFollowReferences_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {

		J9JVMTIHeapData iteratorData;

		memset(&iteratorData, 0x00, sizeof(J9JVMTIHeapData));

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_tag_objects);
		/* heap_filter is a bitfield - JDK allows undefined bits to be declared, so no checking is needed for it */
		ENSURE_NON_NULL(callbacks);

		iteratorData.env = (J9JVMTIEnv *) env;
		iteratorData.currentThread = currentThread;
		iteratorData.filter = heap_filter;
		iteratorData.classFilter = klass ? J9VM_J9CLASS_FROM_JCLASS(currentThread, klass) : NULL;
		iteratorData.callbacks = callbacks;
		iteratorData.userData = (void *) user_data;
		iteratorData.clazz = 0;
		iteratorData.rc = JVMTI_ERROR_NONE;
		
		/* Do not report anything if the class filter set by the user is an interface class.  Quote from the spec:
		 * "If klass is an interface, no objects are reported. This applies to both the object and primitive callbacks." 
		 * iteratorData->classFilter is klass in this case. Checked against SUN behaviour to verify. 
		 * Don't even start iteration as there will never be any callbacks issued. */ 

		if (iteratorData.classFilter && (iteratorData.classFilter->romClass->modifiers & J9AccInterface)) {
			goto done;
		}

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);
		ensureHeapWalkable(currentThread);

		/* Walk the heap */
		if (initial_object) {
			j9object_t object = *(j9object_t *) initial_object;

			iteratorData.flags |= J9JVMTI_HI_INITIAL_OBJECT_REF;

			/* Call the callback with the initial object. The GC Reference Chain Walker
			 * does not call us back with the initial object. */
			 
			followReferencesCallback(&object, NULL, &iteratorData, J9GC_REFERENCE_TYPE_FIELD , 0, 0);

			vm->memoryManagerFunctions->j9gc_ext_reachable_from_object_do(
				currentThread, object, followReferencesCallback, &iteratorData, J9_MU_WALK_PREINDEX_INTERFACE_FIELDS);

		} else {
			vm->memoryManagerFunctions->j9gc_ext_reachable_objects_do(
				currentThread, followReferencesCallback, &iteratorData, J9_MU_WALK_OBJECT_BASE | 
				J9_MU_WALK_SKIP_JVMTI_TAG_TABLES | J9_MU_WALK_PREINDEX_INTERFACE_FIELDS);
		}


		if (iteratorData.rc != JVMTI_ERROR_NONE) {
			rc = iteratorData.rc;
		}

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiFollowReferences);
}


/** 
 * \brief       Internal callback invoked by the GC RootScanner to process object references
 * \ingroup     jvmti.heap
 * 
 * @param[in] slotPtr		Referee objected referred to by the referrer 
 * @param[in] referrer 		Referer object 
 * @param[in] userData		Private user data set by the caller of <code>jvmtiFollowReferences<code>
 * @param[in] type    		Reference type
 * @param[in] referrerIndex	Index of the referee in the referrer object 
 * @return 
 * 
 *	The following object references are reported when they are non-null:
 *
 *	    * Instance objects report references to each non-primitive instance fields (including inherited fields).
 *	    * Instance objects report a reference to the object type (class).
 *	    * Classes report a reference to the superclass and directly implemented/extended interfaces.
 *	    * Classes report a reference to the class loader, protection domain, signers, and resolved entries in the constant pool.
 *	    * Classes report a reference to each directly declared non-primitive static field.
 *	    * Arrays report a reference to the array type (class) and each array element.
 *	    * Primitive arrays report a reference to the array type.
 *
 */
static jvmtiIterationControl
followReferencesCallback(j9object_t *slotPtr, j9object_t referrer, void *userData, IDATA type, IDATA referrerIndex, IDATA wasReportedBefore)
{
	J9JVMTIHeapData * iteratorData = userData;
	J9JavaVM *vm = JAVAVM_FROM_ENV(iteratorData->env);
	J9VMThread *vmThread = iteratorData->currentThread;
	jvmtiIterationControl visitRc = JVMTI_ITERATION_ABORT;
	UDATA reportHeapReference = TRUE;
	j9object_t object = *slotPtr;
	J9Class *clazz;
	IDATA filterResult;


	/* Do not report uninitialized classes */
	if (J9VM_IS_UNINITIALIZED_HEAPCLASS(vmThread, object)) {
		return JVMTI_ITERATION_IGNORE;
	}

	iteratorData->object = object;
	iteratorData->referrer = referrer;
	
	/* Map GC event types to JVMTI types */
	mapEventType(iteratorData, type, (jint)referrerIndex, referrer, object);

	if (iteratorData->event.type == J9JVMTI_HEAP_EVENT_NONE) {
		return JVMTI_ITERATION_CONTINUE;
	}
	
	if (iteratorData->event.type == J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW) {
		return JVMTI_ITERATION_IGNORE;
	}

	clazz = J9OBJECT_CLAZZ(vmThread, iteratorData->object);
	iteratorData->clazz = clazz;

   
	/* Get the 4 tags required by the call back */
	jvmtiFollowRefs_getTags(iteratorData, iteratorData->referrer, iteratorData->object);


	/* Run it past the iteration control filter */
	filterResult = heapReferenceFilter(iteratorData);
	if (filterResult >= 0) {
		return filterResult;
	}


	iteratorData->objectSize = getObjectSize(vm, iteratorData->object);
 
	if (iteratorData->flags & J9JVMTI_HI_INITIAL_OBJECT_REF) {
		reportHeapReference = FALSE;
	}

	/* The user might not have set the heap ref callback */
	if (iteratorData->callbacks->heap_reference_callback && reportHeapReference == TRUE) {
		visitRc = wrap_heapReferenceCallback(vm, iteratorData);
		JVMTI_HEAP_CHECK_RC(iteratorData->rc);
		JVMTI_HEAP_CHECK_ITERATION_ABORT(visitRc);
	}

#ifdef JVMTI_HEAP_DEBUG
	jvmtiHeapFollowReferencesPrint(vm, iteratorData->object, iteratorData->referrer, iteratorData);
#endif


	/* Is this an Array Object? If so then issue a user callback and pass along the array primitive elements */ 
	if (iteratorData->callbacks->array_primitive_value_callback &&
			J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(clazz->romClass)) {
		visitRc = wrap_arrayPrimitiveValueCallback(vm, iteratorData);
		JVMTI_HEAP_CHECK_RC(iteratorData->rc);
		JVMTI_HEAP_CHECK_ITERATION_ABORT(visitRc);
	}

	/* Callback for all primitive fields of this object */
	if (iteratorData->callbacks->primitive_field_callback) { 
		visitRc = wrap_primitiveFieldCallback(vm, iteratorData, wasReportedBefore);
		JVMTI_HEAP_CHECK_RC(iteratorData->rc);
		JVMTI_HEAP_CHECK_ITERATION_ABORT(visitRc);
	}

	/* TODO: There has got to be a better way of checking if an object is a String or not */
	if (iteratorData->callbacks->string_primitive_value_callback) {
		J9UTF8 *clazzName = J9ROMCLASS_CLASSNAME(clazz->romClass);
		if (J9UTF8_LENGTH(clazzName) == 16 && !memcmp(J9UTF8_DATA(clazzName), "java/lang/String", 16)) {
			visitRc = wrap_stringPrimitiveCallback(vm, iteratorData);
			JVMTI_HEAP_CHECK_RC(iteratorData->rc);
			JVMTI_HEAP_CHECK_ITERATION_ABORT(visitRc);
		}
	}

	/* reset the initial object flag if we've reported its primitive fields, arrays and string values */
	if (reportHeapReference == FALSE) {
		iteratorData->flags &= ~J9JVMTI_HI_INITIAL_OBJECT_REF;
	}

	if (iteratorData->event.type == J9JVMTI_HEAP_EVENT_NONE_REPORT_NOFOLLOW) {
		return JVMTI_ITERATION_IGNORE;
	}

	return JVMTI_ITERATION_CONTINUE;

done:
	return JVMTI_ITERATION_ABORT;
}


/** 
 * \brief	Filter for heap reference walking 
 * \ingroup 
 * 
 * @param[in] iteratorData 
 * @return modron heap following control type
 * 
 *	Figure out whether a heap reference should be followed based on the class filter 
 *	or tags set by the user.
 */
static IDATA
heapReferenceFilter(J9JVMTIHeapData * iteratorData)
{

	/* If the class filter is set, check if this reference is what we are looking for,
	 * otherwise continue iterating */
	if ((iteratorData->classFilter != NULL) && (iteratorData->classFilter != iteratorData->clazz)) {
		if (iteratorData->event.type == J9JVMTI_HEAP_EVENT_NONE_REPORT_NOFOLLOW) {
			return JVMTI_ITERATION_IGNORE;
		} else {
			return JVMTI_ITERATION_CONTINUE;
		}
	}


	/* Does it pass the object filters specified by the user? */
	if (((iteratorData->filter & JVMTI_HEAP_FILTER_TAGGED) && iteratorData->tags.objectTag != (jlong) 0) ||     /* filter out tagged objects */
		((iteratorData->filter & JVMTI_HEAP_FILTER_UNTAGGED) && iteratorData->tags.objectTag == (jlong) 0)) {   /* filter out untagged objects */
		return JVMTI_ITERATION_CONTINUE;
	}

	/* Does it pass the class filters specified by the user? */
	if (((iteratorData->filter & JVMTI_HEAP_FILTER_CLASS_TAGGED) && iteratorData->tags.classTag != (jlong) 0) ||   /* filter out tagged classes */
		((iteratorData->filter & JVMTI_HEAP_FILTER_CLASS_UNTAGGED) && iteratorData->tags.classTag == (jlong) 0)) { /* filter out untagged classes */
		return JVMTI_ITERATION_CONTINUE;
	}

	return -1;
}


static jvmtiIterationControl
wrap_heapReferenceCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData)
{
	jvmtiIterationControl visitRc = JVMTI_ITERATION_ABORT;
	J9JVMTIHeapEvent * e = &iteratorData->event;


	jint length;
	jlong objectTag;
	jlong referrerObjectTag;

    /* Details about the reference. Set when the reference_kind is 
     *  JVMTI_HEAP_REFERENCE_FIELD, 
     *  JVMTI_HEAP_REFERENCE_STATIC_FIELD, 
     *  JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT, 
     *  JVMTI_HEAP_REFERENCE_CONSTANT_POOL, 
     *  JVMTI_HEAP_REFERENCE_STACK_LOCAL, 
     *  JVMTI_HEAP_REFERENCE_JNI_LOCAL. 
     *  Otherwise NULL. */

    switch (e->refKind) {
        case JVMTI_HEAP_REFERENCE_FIELD:
        case JVMTI_HEAP_REFERENCE_STATIC_FIELD:
        case JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT:
        case JVMTI_HEAP_REFERENCE_CONSTANT_POOL:
        	/* The indices are filled in while mapping between GC and JVMTI event types */
            break;

        case JVMTI_HEAP_REFERENCE_STACK_LOCAL:
        case JVMTI_HEAP_REFERENCE_JNI_LOCAL:
            if (iteratorData->referrer) {
                /* fill in the stack local and jni local data */
                if (jvmtiHeapFollowRefs_getStackData(iteratorData, (J9StackWalkState *) iteratorData->referrer) == 0)
                    return JVMTI_ITERATION_IGNORE;
            } else {
				/* No reference info available for heap roots, slime some bogus values 
				 * to prevent hprof from crashing. 
				 * TODO: this is a temporary hack which has to be corrected by the GC returning us valid
				 * referer/walkstate info for heap roots */
                if (e->refKind == JVMTI_HEAP_REFERENCE_STACK_LOCAL) {
                    e->refInfo.stack_local.thread_tag = 0;
                    e->refInfo.stack_local.thread_id = 0;
                    e->refInfo.stack_local.depth = -1;
                    e->refInfo.stack_local.method = NULL;
                    e->refInfo.stack_local.location = 0;
                    e->refInfo.stack_local.slot = 0;
                } else {
                    e->refInfo.jni_local.thread_tag = 0;
                    e->refInfo.jni_local.thread_id = 0;
                    e->refInfo.jni_local.depth = -1;
                    e->refInfo.jni_local.method = NULL;
                }               
            }
            break;

        default:
            /* According to the spec, on references other then above, we are required to pass a
             * NULL reference info structure */
            e->hasRefInfo = FALSE;
            break;
    }

	objectTag = iteratorData->tags.objectTag;
	referrerObjectTag = iteratorData->tags.referrerObjectTag;


	/* The callback wants to know the array size (if the referee is an array) */
	if (J9ROMCLASS_IS_ARRAY(iteratorData->clazz->romClass)) {
		length = J9INDEXABLEOBJECT_SIZE(iteratorData->currentThread, (J9IndexableObject *) iteratorData->object);
	} else {
		length = -1;
	}


	/* Callback the user */
	visitRc = iteratorData->callbacks->heap_reference_callback(
					e->refKind,
					e->hasRefInfo ? &e->refInfo : NULL,
					iteratorData->tags.classTag,
					(e->type == J9JVMTI_HEAP_EVENT_ROOT || e->type == J9JVMTI_HEAP_EVENT_STACK) ? 0 : iteratorData->tags.referrerClassTag,
					iteratorData->objectSize,
					&objectTag,
					(e->type == J9JVMTI_HEAP_EVENT_ROOT || e->type == J9JVMTI_HEAP_EVENT_STACK) ? NULL : &referrerObjectTag,
					length,
					iteratorData->userData);

		/* Update the object (referee) tag if it was changed by the user */
		if (objectTag != iteratorData->tags.objectTag) {
			updateObjectTag(iteratorData, iteratorData->object, &iteratorData->tags.objectTag, objectTag);
		}

		/* Update the referrer if it has changed. We ignore any objects that are Heap Roots as per spec */
		if (e->type != J9JVMTI_HEAP_EVENT_ROOT && e->type != J9JVMTI_HEAP_EVENT_STACK) {
			if (referrerObjectTag != iteratorData->tags.referrerObjectTag) 
				updateObjectTag(iteratorData, iteratorData->referrer, &iteratorData->tags.referrerObjectTag, referrerObjectTag);
		}

		if (visitRc & JVMTI_VISIT_ABORT) {
			return JVMTI_ITERATION_ABORT;
		}

		return JVMTI_ITERATION_CONTINUE;
}


/** 
 * \brief     Obtain callback data required for JNI or Stack Local references
 * \ingroup   jvmti.heap
 * 
 * @param[in] iteratorData	iteration structure containing misc data
 * @param[in] e                 helper structure containing reference details
 * @param[in] walkState         stack walker structure for the frame containing the reference
 * @return			none
 *
 */
static UDATA
jvmtiHeapFollowRefs_getStackData(J9JVMTIHeapData * iteratorData, J9StackWalkState *walkState)
{
	J9JVMTIHeapEvent * e = &iteratorData->event;
	jint slot = (jint) walkState->slotIndex;
	jint depth = (jint) walkState->framesWalked;
	J9Method * ramMethod = walkState->method;
	jmethodID method;
	J9JVMTIObjectTag search;
	J9JVMTIObjectTag * result;
	jlong threadID;


	/* Convert internal slot type to JVMTI type */
	switch (walkState->slotType) {
		case J9_STACKWALK_SLOT_TYPE_JNI_LOCAL:
			e->refKind = JVMTI_HEAP_REFERENCE_JNI_LOCAL;
			break;
		case J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL:
			e->refKind = JVMTI_HEAP_REFERENCE_STACK_LOCAL;
			break;
		default:
			/* Do not callback with stack slot types other then JNI or STACK Local */
			return 0;
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
	
	search.ref = (j9object_t ) walkState->walkThread->threadObject;
	result = hashTableFind(iteratorData->env->objectTagTable, &search);

	/* Figure out the Thread ID */

	threadID = J9VMJAVALANGTHREAD_TID(iteratorData->currentThread, walkState->walkThread->threadObject);
	

	switch (e->refKind) {
		case JVMTI_HEAP_REFERENCE_STACK_LOCAL:
			e->refInfo.stack_local.thread_tag = (result) ? result->tag : 0;
			e->refInfo.stack_local.thread_id = threadID;
			e->refInfo.stack_local.depth = depth;
			e->refInfo.stack_local.method = method;
			e->refInfo.stack_local.location = (jlocation) walkState->bytecodePCOffset;
			e->refInfo.stack_local.slot = slot;
			break;

		case JVMTI_HEAP_REFERENCE_JNI_LOCAL:

			e->refInfo.jni_local.thread_tag = (result) ? result->tag : 0;
			e->refInfo.jni_local.thread_id = threadID;
			e->refInfo.jni_local.depth = depth;
			e->refInfo.jni_local.method = method;
			break;

		default:
			break;
	}


	return 1;
}



#ifdef JVMTI_HEAP_DEBUG 
/** 
 * \brief	You call, we print 
 * \ingroup     jvmti.heap
 * 
 * @param[in] vm 
 * @param[in] e 
 * @param[in] object 
 * @param[in] referrer 
 * 
 */
static void
jvmtiHeapFollowReferencesPrint(J9JavaVM * vm, j9object_t object, j9object_t referrer, J9JVMTIHeapData * iteratorData)
{
	J9UTF8 *c, *rc;
	J9JVMTIHeapEvent * e = &iteratorData->event;
	const char *refName = jvmtiHeapReferenceKindString(e->refKind);
	char referrerName[256];
	char refereeName[256];
	J9Class* clazz;

	if (J9VM_IS_INITIALIZED_HEAPCLASS(iteratorData, object)) {
		clazz = J9VM_J9CLASS_FROM_HEAPCLASS(iteratorData->currentThread, object);
		c = J9ROMCLASS_CLASSNAME(clazz->romClass);
	} else { 
		clazz = J9OBJECT_CLAZZ(vm, object);
		c = J9ROMCLASS_CLASSNAME(clazz->romClass);
	}

	strncpy(refereeName, J9UTF8_DATA(c), J9UTF8_LENGTH(c));
	refereeName[J9UTF8_LENGTH(c)] = 0;


	if (e->type != J9JVMTI_HEAP_EVENT_STACK && referrer != NULL) {
		if (J9VM_IS_INITIALIZED_HEAPCLASS(vm, referrer)) {
			J9Class* referrerClazz = J9VM_J9CLASS_FROM_HEAPCLASS(iteratorData->currentThread, referrer);
			rc = J9ROMCLASS_CLASSNAME(referrerClazz->romClass);
		} else {  
			J9Class* referrerClazz = J9OBJECT_CLAZZ(iteratorData->currentThread, referrer);
			rc = J9ROMCLASS_CLASSNAME(referrerClazz->romClass);
		}
		strncpy(referrerName, J9UTF8_DATA(rc), J9UTF8_LENGTH(rc));
		referrerName[J9UTF8_LENGTH(rc)] = 0;
	} else {
		strcpy(referrerName, "null");
		rc = NULL;
	}

	jvmtiHeapFR_printf(("\tev=0x%02x [%d]  refType=0x%x [%-17s]  idx=%d   [%s] -> [%s] flds=%-3d  [%08llx:%08llx]->[%08llx:%08llx]\n", 
			   e->type, e->gcType, e->refKind, refName,
			   e->refInfo.field.index,
			   referrerName, refereeName,
			   clazz->romClass->romFieldCount,
			   (rc) ? iteratorData->tags.referrerClassTag & 0xffffffff : 0xffffffff,
			   (rc) ? iteratorData->tags.referrerObjectTag & 0xffffffff : 0xffffffff,
			   iteratorData->tags.classTag & 0xffffffff,
			   iteratorData->tags.objectTag & 0xffffffff));
	return;
}
#endif


/** 
 * \brief	Obtain tags for the Referrer and Referee objects 
 * \ingroup 	jvmti.heap
 * 
 * @param iteratorData	iteration structure containing misc data
 * @param referrer 	referrer object
 * @param object 	referee object referred to by referrer
 * 
 */
static void
jvmtiFollowRefs_getTags(J9JVMTIHeapData * iteratorData, j9object_t  referrer, j9object_t  object) 
{
	J9JVMTIObjectTag entry, *result;
	J9Class *clazz;

	/* get the object tag */
	entry.ref = object;
	result = hashTableFind(iteratorData->env->objectTagTable, &entry);
	iteratorData->tags.objectTag = (result == NULL) ? 0 : result->tag;
	
	/* get the class (of object) tag */
	clazz = J9OBJECT_CLAZZ(iteratorData->currentThread, object);
	entry.ref = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
	result = hashTableFind(iteratorData->env->objectTagTable, &entry);
	iteratorData->tags.classTag = (result == NULL) ? 0 : result->tag;

	/* The referrer argument for stack slot events carries metadata rather then
	 * the usual j9object, ignore it here */   
	if ((referrer != NULL) && (iteratorData->event.type != J9JVMTI_HEAP_EVENT_STACK)) {
		/* get the referrer object tag */
		entry.ref = referrer;
		result = hashTableFind(iteratorData->env->objectTagTable, &entry);
		iteratorData->tags.referrerObjectTag = (result == NULL) ? 0 : result->tag;

		/* get the referrer object class tag */
		clazz = J9OBJECT_CLAZZ(iteratorData->currentThread, referrer);
		entry.ref = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
		result = hashTableFind(iteratorData->env->objectTagTable, &entry);
		iteratorData->tags.referrerClassTag = (result == NULL) ? 0 : result->tag;
	} else {
		iteratorData->tags.referrerObjectTag = 0;
		iteratorData->tags.referrerClassTag = 0;
	}

	return;
}



/** 
 * \brief	GC event mapper 
 * \ingroup     jvmti.heap
 * 
 * @param[in] data         iteration structure containing misc data
 * @param[in] type         reference type obtained from the Root Scanner 
 * @param[in] index        index of the reference   
 * @param[in] referrer     referrer details
 * @return                 helper structure containing reference details
 * 
 *	Maps GC heap walking callbacks to a format defined in terms of what the 
 *	JVMTI spec is able to return to the user. 
 */
static void
mapEventType(J9JVMTIHeapData * data, IDATA type, jint index, j9object_t referrer, j9object_t object)
{
	U_32 modifiers;
	J9Class *clazz;
	J9JavaVM* vm = data->env->vm;
	J9JVMTIHeapEvent * event = &data->event;
	j9object_t protectionDomain;

	memset(event, 0x00, sizeof(J9JVMTIHeapEvent));
	event->gcType = type;
	event->type = J9JVMTI_HEAP_EVENT_NONE;
	event->hasRefInfo = FALSE;

	switch (type) {

		case J9GC_ROOT_TYPE_JNI_GLOBAL:
		case J9GC_ROOT_TYPE_JNI_WEAK_GLOBAL:
			event->type = J9JVMTI_HEAP_EVENT_ROOT;
			event->refKind = JVMTI_HEAP_REFERENCE_JNI_GLOBAL;
			break;

		case J9GC_ROOT_TYPE_VM_CLASS_SLOT:
			/* Known Classes class slots */
			event->type = J9JVMTI_HEAP_EVENT_ROOT;
			event->refKind = JVMTI_HEAP_REFERENCE_SYSTEM_CLASS;
			break;

		case J9GC_ROOT_TYPE_CLASS:
			/* Do not report or follow classes that dont belong to app or system classloaders, Jazz 33138 */
			clazz = J9OBJECT_CLAZZ(data->currentThread, object);
			if ((vm->systemClassLoader == clazz->classLoader) || (vm->applicationClassLoader == clazz->classLoader)) {
				event->type = J9JVMTI_HEAP_EVENT_ROOT;
				event->refKind = JVMTI_HEAP_REFERENCE_SYSTEM_CLASS;
			} else {
				event->refKind = JVMTI_HEAP_REFERENCE_OTHER;
				event->type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			}
			break;

		case J9GC_ROOT_TYPE_MONITOR:
		case J9GC_ROOT_TYPE_THREAD_MONITOR:
			event->type = J9JVMTI_HEAP_EVENT_ROOT;
			event->refKind = JVMTI_HEAP_REFERENCE_MONITOR;
			break;

		case J9GC_ROOT_TYPE_STACK_SLOT:
			event->type = J9JVMTI_HEAP_EVENT_STACK;
			event->refKind = JVMTI_HEAP_REFERENCE_STACK_LOCAL;
			event->hasRefInfo = TRUE;
			break;

		case J9GC_ROOT_TYPE_JNI_LOCAL:
			event->type = J9JVMTI_HEAP_EVENT_ROOT;
			event->refKind = JVMTI_HEAP_REFERENCE_JNI_LOCAL;
			event->hasRefInfo = TRUE;
			break;

		case J9GC_ROOT_TYPE_THREAD_SLOT:
			event->type = J9JVMTI_HEAP_EVENT_ROOT;
			event->refKind = JVMTI_HEAP_REFERENCE_THREAD;
			break;

		case J9GC_ROOT_TYPE_FINALIZABLE_OBJECT:
		case J9GC_ROOT_TYPE_UNFINALIZED_OBJECT:
			event->type = J9JVMTI_HEAP_EVENT_ROOT;
			event->refKind = JVMTI_HEAP_REFERENCE_OTHER;
			break;

		case J9GC_ROOT_TYPE_STRING_TABLE:
		case J9GC_ROOT_TYPE_REMEMBERED_SET:
		case J9GC_ROOT_TYPE_OWNABLE_SYNCHRONIZER_OBJECT:
			event->type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			event->refKind = JVMTI_HEAP_REFERENCE_OTHER;
			break;
				
		case J9GC_REFERENCE_TYPE_CLASS:
			event->type = J9JVMTI_HEAP_EVENT_OBJECT;
			event->refKind = JVMTI_HEAP_REFERENCE_CLASS;

			if (NULL != referrer) {
				clazz = J9OBJECT_CLAZZ(data->currentThread, referrer);
				if (J9VMJAVALANGCLASS_OR_NULL(vm) == clazz) {
					/*
					 * Do not report class references from Class objects,
					 * per JVMTI specification:
					 * "* 	Classes report a reference to the superclass and
					 * 		directly implemented/extended interfaces.
					 *  * 	Classes report a reference to the class loader,
					 * 		protection domain, signers, and resolved entries in the constant pool."
					 */
					event->type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
					break;
				}
				if (!J9VM_IS_INITIALIZED_HEAPCLASS(data->currentThread, referrer)) {
					/* referrer is an instance object, report its class reference but do not follow it.
					 * Its odd but follows the SUN behaviour. */
					event->type = J9JVMTI_HEAP_EVENT_NONE_REPORT_NOFOLLOW;
				}
			}

			break;

		case J9GC_REFERENCE_TYPE_SUPERCLASS:
			event->type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			
			if (J9VM_IS_INITIALIZED_HEAPCLASS(data->currentThread, data->referrer)) {
				clazz = J9VM_J9CLASS_FROM_HEAPCLASS(data->currentThread, data->referrer);
				/* only report the direct superclass */
				if (J9CLASS_DEPTH(clazz) - 1 == (UDATA)index) {
					event->type = J9JVMTI_HEAP_EVENT_OBJECT;
					event->refKind = JVMTI_HEAP_REFERENCE_SUPERCLASS;
				}
			}

			break;

		case J9GC_REFERENCE_TYPE_CLASS_ARRAY_CLASS:
			/* BOGUS: Slime the component classes onto the end of the constant pool after the class */
			Assert_JVMTI_true(J9VM_IS_INITIALIZED_HEAPCLASS(data->currentThread, referrer));
			clazz = J9VM_J9CLASS_FROM_HEAPCLASS(data->currentThread, referrer);
			modifiers = clazz->romClass->modifiers;
			/* Dont follow unverified references, unless they are interfaces. */
			if (!(modifiers & J9AccInterface) &&
				(clazz->initializeStatus & J9ClassInitStatusMask) == J9ClassInitUnverified) {
				event->type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			} else {
				event->type = J9JVMTI_HEAP_EVENT_OBJECT;
				event->refKind = JVMTI_HEAP_REFERENCE_CONSTANT_POOL;
				event->refInfo.constant_pool.index += 2 + (clazz->romClass->ramConstantPoolCount * 2);
				event->hasRefInfo = TRUE;
			}

			break;

		case J9GC_REFERENCE_TYPE_FIELD:
		case J9GC_REFERENCE_TYPE_WEAK_REFERENCE:

			/* If the referrer is a class object, do not report or follow its field refs as per spec */
			if (referrer) {
				clazz = J9OBJECT_CLAZZ(data->currentThread, referrer);
				if (J9VMJAVALANGCLASS_OR_NULL(vm) == clazz) {
					event->type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
					break;
				}
			}

			event->type = J9JVMTI_HEAP_EVENT_OBJECT;
			event->refKind = JVMTI_HEAP_REFERENCE_FIELD;
			event->refInfo.field.index = index;
			event->hasRefInfo = TRUE;
			break;

		case J9GC_REFERENCE_TYPE_ARRAY:
			event->type = J9JVMTI_HEAP_EVENT_OBJECT;
			event->refKind = JVMTI_HEAP_REFERENCE_ARRAY_ELEMENT;
			event->refInfo.array.index = index;
			event->hasRefInfo = TRUE;
			break;

		case J9GC_REFERENCE_TYPE_CLASSLOADER:
	        event->type = J9JVMTI_HEAP_EVENT_OBJECT;
			event->refKind = JVMTI_HEAP_REFERENCE_CLASS_LOADER;
			break;

		case J9GC_REFERENCE_TYPE_INTERFACE:
			event->type = J9JVMTI_HEAP_EVENT_OBJECT;
			event->refKind = JVMTI_HEAP_REFERENCE_INTERFACE;
			break;

		case J9GC_REFERENCE_TYPE_PROTECTION_DOMAIN:
			event->type = J9JVMTI_HEAP_EVENT_OBJECT;
			event->refKind = JVMTI_HEAP_REFERENCE_PROTECTION_DOMAIN;

			protectionDomain = J9VMJAVALANGCLASS_PROTECTIONDOMAIN(data->currentThread, data->object);
			if (protectionDomain) {
				data->object = protectionDomain;
			} else {
				event->type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			}

			break;

		case J9GC_REFERENCE_TYPE_STATIC:
			event->type = J9JVMTI_HEAP_EVENT_OBJECT;
			event->refKind = JVMTI_HEAP_REFERENCE_STATIC_FIELD;
			event->refInfo.field.index = index;
			event->hasRefInfo = TRUE;
			break;

		case J9GC_REFERENCE_TYPE_CONSTANT_POOL:
			event->type = J9JVMTI_HEAP_EVENT_OBJECT;
			event->refKind = JVMTI_HEAP_REFERENCE_CONSTANT_POOL;
			/* reference chain walker returns constant pool index based from 0, jvmti wants it based from 1 */
			event->refInfo.constant_pool.index = index + 1;
			event->hasRefInfo = TRUE;
			break;

		case J9GC_REFERENCE_TYPE_CLASS_NAME_STRING:
			event->type = J9JVMTI_HEAP_EVENT_OBJECT;
			event->refKind = JVMTI_HEAP_REFERENCE_OTHER;
			break;


		case J9GC_ROOT_TYPE_CLASSLOADER:
		case J9GC_ROOT_TYPE_JVMTI_TAG_REF:
		case J9GC_REFERENCE_TYPE_UNKNOWN:
		default:
			event->type = J9JVMTI_HEAP_EVENT_NONE_NOFOLLOW;
			event->refKind = JVMTI_HEAP_REFERENCE_OTHER;			
			break;
	}

	/* Cancel event if no callback */

	return;
}




/**         
 * \brief      Iterate Through Heap 
 * \ingroup    jvmti.heap
 * 
 * @param env             jvmti environment
 * @param heap_filter     tag filter flags used to determine which objects are to be reported
 * @param klass           class filter 
 * @param callbacks       callbacks to be invoked for each reference type
 * @param user_data       user data to be passed back via the callbacks
 * @return                a jvmtiError value
 */
jvmtiError JNICALL jvmtiIterateThroughHeap(jvmtiEnv* env,
					   jint heap_filter,
					   jclass klass,
					   const jvmtiHeapCallbacks* callbacks,
					   const void* user_data) 
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	J9VMThread * currentThread;
	jvmtiError rc;
	
	Trc_JVMTI_jvmtiIterateThroughHeap_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9InternalVMFunctions const *vmFuncs = vm->internalVMFunctions;
		J9JVMTIHeapData iteratorData;

		vmFuncs->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_tag_objects);
		/* heap_filter is a bitfield - JDK allows undefined bits to be declared, so no checking is needed for it */
		ENSURE_NON_NULL(callbacks);


		iteratorData.env = (J9JVMTIEnv *)env;
		iteratorData.currentThread = currentThread;
		iteratorData.filter = heap_filter;
		iteratorData.classFilter = klass ? J9VM_J9CLASS_FROM_JCLASS(currentThread, klass) : NULL;
		iteratorData.callbacks = callbacks;
		iteratorData.userData = (void *) user_data;
		iteratorData.clazz = 0;
		iteratorData.rc = JVMTI_ERROR_NONE;
	     
		/* Do not report anything if the class filter set by the user is an interface class.  Quote from the spec:
		 * "If klass is an interface, no objects are reported. This applies to both the object and primitive callbacks." 
		 * iteratorData->classFilter is klass in this case. Checked against SUN behaviour to verify. 
		 * Don't even start iteration as there will never be any callbacks issued. */ 

		if (iteratorData.classFilter && (iteratorData.classFilter->romClass->modifiers & J9AccInterface)) {
			goto done;
		}
    
		vmFuncs->acquireExclusiveVMAccess(currentThread);
		ensureHeapWalkable(currentThread);

		/* Walk the heap */
		vm->memoryManagerFunctions->j9mm_iterate_all_objects(vm, vm->portLibrary, 0, iterateThroughHeapCallback, &iteratorData);
		rc = iteratorData.rc;

		vmFuncs->releaseExclusiveVMAccess(currentThread);

done:
		vmFuncs->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiIterateThroughHeap);
}





/** 
 * \brief      Heap Iteration callback
 * \ingroup    jvmti.heap
 * 
 * @param[in] vmThread
 * @param[in] object     object being iterated over by the GC Root Scanner
 * @param[in] userData   our private data, cast it to <code>J9JVMTIHeapData</code>
 * @return               none 
 * 
 */
static jvmtiIterationControl
iterateThroughHeapCallback(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void * userData)
{
	j9object_t object = objectDesc->object;
	J9JVMTIHeapData * iteratorData = userData;
	jvmtiIterationControl visitRc = JVMTI_ITERATION_CONTINUE;
	J9Class *clazz;

	/* Do not report uninitialized classes */
	if (J9VM_IS_UNINITIALIZED_HEAPCLASS_VM(vm, object)) {
		return JVMTI_ITERATION_CONTINUE;
	}

	clazz = J9OBJECT_CLAZZ_VM(vm, object);

	/* If the class filter is set, check if this references is what we are looking for,
	 * otherwise continue iterating */
	if ((iteratorData->classFilter != NULL) && (iteratorData->classFilter != clazz)) {
		return JVMTI_ITERATION_CONTINUE;
	}
 
	/* Get the tags required by the call back */
	jvmtiFollowRefs_getTags(iteratorData, NULL, object);


	/* Does it pass the object filters specified by the user? */
	if (((iteratorData->filter & JVMTI_HEAP_FILTER_TAGGED) && iteratorData->tags.objectTag != (jlong) 0)  ||   /* filter out tagged objects */
		((iteratorData->filter & JVMTI_HEAP_FILTER_UNTAGGED) && iteratorData->tags.objectTag == (jlong) 0)) {  /* filter out untagged objects */
		return JVMTI_ITERATION_CONTINUE;
	}

	/* Does it pass the class filters specified by the user? */
	if (((iteratorData->filter & JVMTI_HEAP_FILTER_CLASS_TAGGED) && iteratorData->tags.classTag != (jlong) 0) ||    /* filter out tagged classes */
		((iteratorData->filter & JVMTI_HEAP_FILTER_CLASS_UNTAGGED) && iteratorData->tags.classTag == (jlong) 0)) {  /* filter out untagged classes */
		return JVMTI_ITERATION_CONTINUE;
	}


#ifdef JVMTI_HEAP_DEBUG	
	{
		J9UTF8 *c = J9ROMCLASS_CLASSNAME(clazz->romClass);
		jvmtiHeapFR_printf(("Class: [%*s] fields=%d class_tag=0x%x\n", 
				   J9UTF8_LENGTH(c), J9UTF8_DATA(c), clazz->romClass->romFieldCount, iteratorData->tags.classTag));
	}
#endif


	/* Remember some of the data that is used by most of the subsequent callbacks. 
	 * We want to avoid recomputing them */
	iteratorData->clazz = clazz;
	iteratorData->object = object;
	iteratorData->objectSize = getObjectSize(vm, object);

	if (iteratorData->callbacks->heap_iteration_callback) {
		visitRc = wrap_heapIterationCallback(vm, iteratorData);
		JVMTI_HEAP_CHECK_RC(iteratorData->rc);
		JVMTI_HEAP_CHECK_ITERATION_ABORT(visitRc);
	}
	
	/* Is this an Array Object? If so then issue a user callback and pass along the array primitive elements */ 
	if (iteratorData->callbacks->array_primitive_value_callback && J9ROMCLASS_IS_ARRAY(clazz->romClass)) {
		visitRc = wrap_arrayPrimitiveValueCallback(vm, iteratorData);
		JVMTI_HEAP_CHECK_RC(iteratorData->rc);
		JVMTI_HEAP_CHECK_ITERATION_ABORT(visitRc);
	}
	
	/* Callback for all primitive fields of this object */
	if (iteratorData->callbacks->primitive_field_callback) {
		visitRc = wrap_primitiveFieldCallback(vm, iteratorData, 0);
		JVMTI_HEAP_CHECK_RC(iteratorData->rc);
		JVMTI_HEAP_CHECK_ITERATION_ABORT(visitRc);
	}

	/* TODO: There has got to be a better way of checking if an object is a String or not */
	if (iteratorData->callbacks->string_primitive_value_callback) {
		J9UTF8 *clazzName = J9ROMCLASS_CLASSNAME(clazz->romClass);
		if (J9UTF8_LENGTH(clazzName) == 16 && !memcmp(J9UTF8_DATA(clazzName), "java/lang/String", 16)) {
			visitRc = wrap_stringPrimitiveCallback(vm, iteratorData);
			JVMTI_HEAP_CHECK_RC(iteratorData->rc);
			JVMTI_HEAP_CHECK_ITERATION_ABORT(visitRc);
		}
	}

	return JVMTI_ITERATION_CONTINUE;

done:
	return JVMTI_ITERATION_ABORT;
}



/** 
 * \brief	Wrapper for the Heap Iteration <code>jvmtiHeapIterationCallback</code> user callback 
 * \ingroup 	jvmti.heap
 * 
 * @param[in] vm 		JavaVM structure
 * @param[in] iteratorData	iteration structure containing misc data
 * @return                      none 
 * 
 */
static jvmtiIterationControl
wrap_heapIterationCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData)
{
	U_32 arrayLength;
	jboolean isArray;
	J9Class *clazz;
	jint visitRc = 0;
	jlong tag;

	/* Determine if the object is an Array and get the array length */
	clazz = J9OBJECT_CLAZZ(iteratorData->currentThread, iteratorData->object);
	if (J9ROMCLASS_IS_ARRAY(clazz->romClass)) {
		arrayLength = J9INDEXABLEOBJECT_SIZE(iteratorData->currentThread, (J9IndexableObject *) iteratorData->object);
		isArray = 1;
	} else {
		isArray = 0;
	}

	tag =  iteratorData->tags.objectTag;

	/* Call the user supplied heap iteration callback */
	visitRc = iteratorData->callbacks->heap_iteration_callback(iteratorData->tags.classTag,
								   iteratorData->objectSize,
								   &tag,
								   (isArray) ? arrayLength : -1,
								   iteratorData->userData);

	/* The user might have changed or removed the tag we have passed in. Make sure we 
	 * account for any changes. */
	updateObjectTag(iteratorData, iteratorData->object, &iteratorData->tags.objectTag, tag);

	
	/* Did the user tell us to stop iterating? */
	if (visitRc & JVMTI_VISIT_ABORT) {
		return JVMTI_ITERATION_ABORT;
	}

	return JVMTI_ITERATION_CONTINUE;
}



/** 
 * \brief	Wrapper for the Primitive Value <code>jvmtiArrayPrimitiveValueCallback</code> user callback 
 * \ingroup 	jvmti.heap
 * 
 * @param[in] vm            JavaVM structure
 * @param[in] iteratorData  iteration structure containing misc data
 * @return  none 
 * 
 */
static jvmtiIterationControl
wrap_arrayPrimitiveValueCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData)
{
	J9JVMTIEnv *env = iteratorData->env;
	jvmtiError rc;
	jvmtiIterationControl visitRc = JVMTI_ITERATION_ABORT;
	jlong tag;

	jvmtiPrimitiveType primitiveType;
	jint elementCount = J9INDEXABLEOBJECT_SIZE(iteratorData->currentThread, (J9IndexableObject *) iteratorData->object);
	void *elements;

	rc = getArrayPrimitiveElements(iteratorData, &primitiveType, &elements, elementCount);
	if (rc != JVMTI_ERROR_NONE) {
		return JVMTI_ITERATION_ABORT;
	}


	if (primitiveType != 0 && NULL != elements) {

		tag = iteratorData->tags.objectTag;
		
#ifdef JVMTI_HEAP_DEBUG
		jvmtiHeapFR_printf(("\t\tArray Primitive type=%d  element_count=%d  [tag=0x%08x]\n",
				    primitiveType, elementCount, tag));
#endif

		/* Call the user supplied callback with the array elements */
		visitRc = iteratorData->callbacks->array_primitive_value_callback(iteratorData->tags.classTag, 
										  iteratorData->objectSize,
										  &tag,
										  elementCount,
										  primitiveType,
										  elements,
										  iteratorData->userData);

		updateObjectTag(iteratorData, iteratorData->object, &iteratorData->tags.objectTag, tag);

		/* Deallocate the element buffer allocated by getArrayPrimitiveElements() */
		if (NULL != elements)
			jvmtiDeallocate((jvmtiEnv *) env, (unsigned char *) elements);

		/* Did the user tell us to stop iterating? */
		if (visitRc & JVMTI_VISIT_ABORT) {
			return JVMTI_ITERATION_ABORT;
		}
	}

	return JVMTI_ITERATION_CONTINUE;
}


/** 
 * \brief	Wrapper for the Field Primitive <code>jvmtiPrimitiveFieldCallback</code> user callback
 * \ingroup 	jvmti.heap
 * 
 * @param[in] vm            JavaVM structure
 * @param[in] iteratorData  iteration structure containing misc data
 * @return                  none 
 * 
 * Beware ye who enter here.. This code conforms to all the !nice undocumented quirks inherent to
 * the corresponding SDK implementation. The field ordering and especially the field indexing is
 * tricky and must adhere to the spec.
 *
 * Primitive field callbacks for an instance/class are issued on the first reference to that object.
 * The heap walker gives us references only (as it should) and no notification to walk primitive fields. 
 * In retrospect perhaps the primitive field walk functionality should be pushed down to that level instead. 
 */
static jvmtiIterationControl
wrap_primitiveFieldCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData, IDATA wasReportedBefore)
{
	J9Class *clazz;
	jvmtiHeapReferenceKind fieldKind;
	J9VMThread * currentThread;
	U_32 walkFlags; 
	J9ROMFullTraversalFieldOffsetWalkState state;
	J9ROMFieldShape * field;
	jvmtiError rc;
	jvmtiIterationControl visitRc = JVMTI_ITERATION_ABORT;
	jint fieldIndex = 0;

	/* Check if the referee was already visited and had its primitive fields callback issued. Duplicate SUN behavior
	 * by issuing the callback only ONCE, even if there are multiple references to the referee. The spec is of course 
	 * unclear on this point */
	if (0 != wasReportedBefore) {
		return JVMTI_ITERATION_CONTINUE;
	}

	/* Get the JNIEnv structure */
	rc = getCurrentVMThread(vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		JVMTI_HEAP_ERROR(rc);
	} 

	/* If the object is a Class or an Interface, then set the kind to JVMTI_HEAP_REFERENCE_STATIC_FIELD */
	if (J9VM_IS_INITIALIZED_HEAPCLASS(iteratorData->currentThread, iteratorData->object) || (iteratorData->clazz->romClass->modifiers & J9AccInterface)) {
		fieldKind = JVMTI_HEAP_REFERENCE_STATIC_FIELD;
		clazz = J9VM_J9CLASS_FROM_HEAPCLASS(iteratorData->currentThread, iteratorData->object);
		walkFlags = J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_PREINDEX_INTERFACE_FIELDS ;
	} else {
		fieldKind = JVMTI_HEAP_REFERENCE_FIELD;
		clazz = iteratorData->clazz;
		walkFlags = J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_PREINDEX_INTERFACE_FIELDS;
	}

	field = vm->internalVMFunctions->fullTraversalFieldOffsetsStartDo(vm, clazz, &state, walkFlags);

#ifdef JVMTI_HEAP_DEBUG	
	{
		J9UTF8 *c = J9ROMCLASS_CLASSNAME(clazz->romClass);
		jvmtiHeapFR_printf(("\t\tClass: [%*s] fields=%d 0x%08x referenceIndexOffset=%d\n", 
							J9UTF8_LENGTH(c), J9UTF8_DATA(c), clazz->romClass->romFieldCount, clazz, state.referenceIndexOffset));
	}
#endif

	while (field != NULL) {

		IDATA offset;
		void* valuePtr; 
		jvalue primitiveValue;
		jvmtiHeapReferenceInfo fieldInfo;
		jvmtiPrimitiveType primitiveType;
		jlong tag = iteratorData->tags.objectTag;
		J9UTF8 * fieldSignature = J9ROMFIELDSHAPE_SIGNATURE(field); 

		/* We are not interested in Object fields */
		if (field->modifiers & J9FieldFlagObject) {
#ifdef JVMTI_HEAP_DEBUG
			{
				J9UTF8 * fieldName = J9ROMFIELDSHAPE_NAME(field);
				char name[256], sig[256];
				strncpy(name, J9UTF8_DATA(fieldName), J9UTF8_LENGTH(fieldName));
				strncpy(sig, J9UTF8_DATA(fieldSignature), J9UTF8_LENGTH(fieldSignature));
				name[J9UTF8_LENGTH(fieldName)] = 0;
				sig[J9UTF8_LENGTH(fieldSignature)] = 0;
			    /* printf("field: [%*s]\n", J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName)); */
				printf("\t\t%s Object             [idx=%-2d | %d] [cia=%d] [Cclazz=0x%08x]                     [%08llx:%08llx] [%s] [%s] \n", 
					   (field->modifiers & J9AccStatic) ? "S" : "I",
					   state.referenceIndexOffset + state.fieldOffsetWalkState.result.index - 1, clazz->romClass->romFieldCount, 
					   state.referenceIndexOffset,
					   state.currentClass,
					   iteratorData->tags.classTag & 0xffffffff,
					   tag & 0xffffffff,
					   name, sig 
					  ); 
			}
#endif
			goto nextField;
		}


		if (field->modifiers & J9AccStatic) {
			
			/* Static field of an instance reference, skip it */
			if (fieldKind == JVMTI_HEAP_REFERENCE_FIELD) {
				goto nextField;
			}

			valuePtr = (U_8 *)(state.fieldOffset + (UDATA)state.currentClass->ramStatics);
		} else {

			/* Instance field of a static field reference, skip it */
			if (fieldKind == JVMTI_HEAP_REFERENCE_STATIC_FIELD) {
				goto nextField;
			}

			offset = state.fieldOffset;
			valuePtr = (U_8*) iteratorData->object + offset + sizeof(J9Object);
		}

 		
		/* Ignore any inherited static fields to mirror SUN behaviour (not spec'ed) */
		if ((fieldKind == JVMTI_HEAP_REFERENCE_STATIC_FIELD) && (state.currentClass != clazz)) {
			goto nextField; 
		}

		/* The field index must reflect the index order returned by jvmtiGetClassFields in addition to being offset */
		fieldInfo.field.index = (jint)(state.fieldOffsetWalkState.result.index + state.classIndexAdjust + state.referenceIndexOffset - 1);

		primitiveValue.j = (jlong) 0;
		fillInJValue(J9UTF8_DATA(fieldSignature)[0], &primitiveValue, valuePtr, NULL);
		rc = getPrimitiveType(fieldSignature, &primitiveType);
		if (JVMTI_ERROR_NONE == rc) {

#ifdef JVMTI_HEAP_DEBUG
			{
				char name[256], sig[256];
				strncpy(name, J9UTF8_DATA(fieldName), J9UTF8_LENGTH(fieldName));
				strncpy(sig, J9UTF8_DATA(fieldSignature), J9UTF8_LENGTH(fieldSignature));
				name[J9UTF8_LENGTH(fieldName)] = 0;
				sig[J9UTF8_LENGTH(fieldSignature)] = 0;
				printf("\t\t%s Primitive type=%d  [idx=%-2d | %d] [cia=%d] [C=0x%08x][CC=0x%08x] [%08llx:%08llx] [%s] [%s] [val=0x%llx]\n",
					   (field->modifiers & J9AccStatic) ? "S" : "I",
					   primitiveType, fieldInfo.field.index, clazz->romClass->romFieldCount, state.referenceIndexOffset,
					   clazz, state.currentClass,
					   iteratorData->tags.classTag & 0xffffffff,
					   tag & 0xffffffff,
					   name, sig, *((jlong*) valuePtr) );
			}
#endif

			/* Call the user with the field data */
			visitRc = iteratorData->callbacks->primitive_field_callback(
						fieldKind,
						&fieldInfo,
						iteratorData->tags.classTag,
						&tag,
						primitiveValue, 
						primitiveType, 
						iteratorData->userData);  

			/* The user might have changed or removed the tag we have passed in. Make sure we 
			 * account for any changes. */
			updateObjectTag(iteratorData, iteratorData->object, &iteratorData->tags.objectTag, tag);

			/* Did the user tell us to stop iterating? */
			if (visitRc & JVMTI_VISIT_ABORT) {
				return JVMTI_ITERATION_ABORT;
			}
		} else {
			JVMTI_HEAP_ERROR(rc);
		}


nextField:
		fieldIndex++;

		/* Go look at the next field */
		field = vm->internalVMFunctions->fullTraversalFieldOffsetsNextDo(&state);
	}

	return JVMTI_ITERATION_CONTINUE;

done:
	return JVMTI_ITERATION_ABORT;
}


/** 
 * \brief	Wrapper for the String Primitive <code>jvmtiStringPrimitiveValueCallback</code> user callback 
 * \ingroup 	jvmti.heap
 * 
 * @param[in] vm 		JavaVM structure
 * @param[in] iteratorData	iteration structure containing misc data
 * @return                      none 
 * 
 */
static jvmtiIterationControl
wrap_stringPrimitiveCallback(J9JavaVM * vm, J9JVMTIHeapData * iteratorData)
{
	jvmtiIterationControl rc = JVMTI_ITERATION_ABORT;
	jlong tag;
	jint stringLength, i;
	jchar * stringValue;
	PORT_ACCESS_FROM_JAVAVM(vm);
	j9object_t bytes = J9VMJAVALANGSTRING_VALUE(iteratorData->currentThread, iteratorData->object);
	UDATA offset = 0;

	/* Ignore uninitialized string */
	if (NULL == bytes) {
		return JVMTI_ITERATION_CONTINUE;
	}

	/* Get the Unicode string data */

	stringLength = J9VMJAVALANGSTRING_LENGTH(iteratorData->currentThread, iteratorData->object);
	stringValue = j9mem_allocate_memory(stringLength * sizeof(jchar), J9MEM_CATEGORY_JVMTI);
	if (NULL == stringValue) {
		JVMTI_HEAP_ERROR(JVMTI_ERROR_OUT_OF_MEMORY);
	}

	if (IS_STRING_COMPRESSED(iteratorData->currentThread, iteratorData->object)) {
		/* Decompress the string byte at a time, this probably can't be
		 */
		for (i = 0; i < stringLength; i++) {
			stringValue[i] = J9JAVAARRAYOFBYTE_LOAD(iteratorData->currentThread, bytes, offset);
			offset++;
		}
	} else {
		/* Copy the string jchar at a time. We currently do not have any
		 * access barrier macros to do it in one shot.
		 * TODO: modify when macros get added */
		for (i = 0; i < stringLength; i++) {
			stringValue[i] = J9JAVAARRAYOFCHAR_LOAD(iteratorData->currentThread, bytes, offset);
			offset++;
		}
	}

	tag = iteratorData->tags.objectTag;

	/* Call the user callback with the Unicode string */

	rc = iteratorData->callbacks->string_primitive_value_callback(iteratorData->tags.classTag,
									  iteratorData->objectSize,
									  &tag,
									  (const jchar *) stringValue,
									  stringLength,
									  iteratorData->userData);

	j9mem_free_memory(stringValue);

	/* The user might have changed or removed the tag we have passed in. Make sure we
	 * account for any changes. */
	updateObjectTag(iteratorData, iteratorData->object, &iteratorData->tags.objectTag, tag);

	/* Did the user tell us to stop iterating? */
	if (rc & JVMTI_VISIT_ABORT) {
		return JVMTI_ITERATION_ABORT;
	}

	return JVMTI_ITERATION_CONTINUE;

done:
	return JVMTI_ITERATION_ABORT;

}



/** 
 * \brief     Obtains a byte packed representation of Array primitive elements 
 * \ingroup   jvmti.heap
 * 
 * @param[in] iteratorData    iteration structure containing misc data 
 * @param[out] primitiveType  type of the array primitive elements
 * @param[out] elements       caller allocated buffer used to pass back the array elements
 * @param[in] elementCount    number of elements to return via the <code>elements</code> buffer
 * @return                    jvmtiError code 
 * 
 */
static jvmtiError
getArrayPrimitiveElements(J9JVMTIHeapData * iteratorData,
			  jvmtiPrimitiveType *primitiveType, 
			  void **elements, 
			  jint elementCount)
{
	int          i;
	jvmtiError   rc;
	void       * array;
	j9object_t   object = iteratorData->object;
	J9JVMTIEnv * env = iteratorData->env;
	jlong        elementSize;
	J9VMThread * currentThread;


	/* Get the JNIEnv structure */
	rc = getCurrentVMThread(env->vm, &currentThread);
	if (rc != JVMTI_ERROR_NONE) {
		JVMTI_HEAP_ERROR(rc);
	}


	/* Get the type of the array primitive */
	*primitiveType = getArrayPrimitiveType(currentThread, object, &elementSize);
	if (*primitiveType == 0) {
		JVMTI_HEAP_ERROR(JVMTI_ERROR_NONE);
	}

	rc = jvmtiAllocate((jvmtiEnv *) env, elementSize * elementCount, (unsigned char **) &array);
	if (rc != JVMTI_ERROR_NONE) {
		JVMTI_HEAP_ERROR(rc);
	}

	switch (*primitiveType) {
		case JVMTI_PRIMITIVE_TYPE_BOOLEAN:
			for (i = 0; i < elementCount; i++)
				*((U_8 *) array+i) = J9JAVAARRAYOFBOOLEAN_LOAD(currentThread, object, i);
			break;

		case JVMTI_PRIMITIVE_TYPE_BYTE:
			for (i = 0; i < elementCount; i++)
				*((I_8 *) array+i) = J9JAVAARRAYOFBYTE_LOAD(currentThread, object, i);
			break;

		case JVMTI_PRIMITIVE_TYPE_CHAR:
			for (i = 0; i < elementCount; i++)
				*((U_16 *) array+i) = J9JAVAARRAYOFCHAR_LOAD(currentThread, object, i);
			break;

		case JVMTI_PRIMITIVE_TYPE_SHORT:
			for (i = 0; i < elementCount; i++)
				*((I_16 *) array+i) = J9JAVAARRAYOFSHORT_LOAD(currentThread, object, i);
			break;

		case JVMTI_PRIMITIVE_TYPE_INT:
			for (i = 0; i < elementCount; i++)
				*((I_32 *) array+i) = J9JAVAARRAYOFINT_LOAD(currentThread, object, i);
			break;

		case JVMTI_PRIMITIVE_TYPE_FLOAT:
			for (i = 0; i < elementCount; i++)
				*((U_32 *) array+i) = J9JAVAARRAYOFFLOAT_LOAD(currentThread, object, i);
			break;

		case JVMTI_PRIMITIVE_TYPE_LONG:
			for (i = 0; i < elementCount; i++)
				*((I_64 *) array+i) = J9JAVAARRAYOFLONG_LOAD(currentThread, object, i);
			break;

		case JVMTI_PRIMITIVE_TYPE_DOUBLE:
			for (i = 0; i < elementCount; i++)
				*((U_64 *) array+i) = J9JAVAARRAYOFDOUBLE_LOAD(currentThread, object, i);
			break;

		default:
			break;
	}

	*elements = array;
	return rc;

done:
	*elements = NULL;
	return rc;
}

/** 
 * \brief     Obtain the Array primitive type and size and map it to JVMTI types 
 * \ingroup 
 * 
 * @param[in] vm             JavaVM structure 
 * @param[in] object         Array object
 * @param[out] primitiveSize size of one array primitive element
 * @return                   Array primitive type
 * 
 */
static jvmtiPrimitiveType
getArrayPrimitiveType(J9VMThread* vmThread, j9object_t  object, jlong *primitiveSize)
{
	J9JavaVM *vm = vmThread->javaVM;
	J9ArrayClass *arrayClass = (J9ArrayClass *) J9OBJECT_CLAZZ(vmThread, object);

	switch (J9CLASS_SHAPE(arrayClass)) {
		case OBJECT_HEADER_SHAPE_WORDS:
			/* char/short */
			if (arrayClass->leafComponentType == vm->charReflectClass) {
				*primitiveSize = sizeof(U_16);
				return JVMTI_PRIMITIVE_TYPE_CHAR;
			} else {
				*primitiveSize = sizeof(I_16);
				return JVMTI_PRIMITIVE_TYPE_SHORT;
			}			

		case OBJECT_HEADER_SHAPE_BYTES:
			/* byte/boolean */
			if (arrayClass->leafComponentType == vm->booleanReflectClass) {
				*primitiveSize = sizeof(U_8);
				return JVMTI_PRIMITIVE_TYPE_BOOLEAN;
			} else {
				*primitiveSize = sizeof(I_8);
				return JVMTI_PRIMITIVE_TYPE_BYTE;
			}

		case OBJECT_HEADER_SHAPE_LONGS:
			/* 32 ints/floats */
			if (arrayClass->leafComponentType == vm->intReflectClass) {
				*primitiveSize = sizeof(I_32);
				return JVMTI_PRIMITIVE_TYPE_INT;
			} else {
				*primitiveSize = sizeof(U_32);
				return JVMTI_PRIMITIVE_TYPE_FLOAT;
			}

		case OBJECT_HEADER_SHAPE_DOUBLES:
			/* long/double floats */
			if (arrayClass->leafComponentType == vm->doubleReflectClass) {
				*primitiveSize = sizeof(U_64);
				return JVMTI_PRIMITIVE_TYPE_DOUBLE;
			} else {
				*primitiveSize = sizeof(I_64);
				return JVMTI_PRIMITIVE_TYPE_LONG;
			}

	}

	return 0; 
}


/** 
 * \brief	Update the tag of the specified object 
 * \ingroup 	jvmti.heap
 * 
 * @param[in] iteratorData	iteration structure containing misc data 
 * @param[in] object		the object being tagged/untagged
 * @param[in/out] originalTag	the address of the original tag
 * @param[in] newTag		the value of the new tag
 * 
 *	Helper used to update the object tag. The caller specifies a newTag.
 *	If the newTag is found to be different from the one in the hash table (also specified
 *	by , change
 *	the hashtable to contain the passed tag. If the passed tag is 0, then the
 *	object tag is removed. 
 */
static void
updateObjectTag(J9JVMTIHeapData * iteratorData, j9object_t object, jlong *originalTag, jlong newTag)
{
	J9JVMTIObjectTag entry;
	J9JVMTIObjectTag *resultTag;
	
	/* The callback could have added or removed the tag. Modify the hashtable entry to
	 * account for it */
	if (*originalTag != 0) {
		/* object was tagged before callback */
		if (newTag) {
			if (*originalTag != newTag) {
				/* was and still is tagged, but the user has changed the tag */
				entry.ref = object;
				resultTag = hashTableFind(iteratorData->env->objectTagTable, &entry);
				resultTag->tag = newTag;
			}
		} else {
			/* no longer tagged, remove the table entry */
			entry.ref = object;
			hashTableRemove(iteratorData->env->objectTagTable, &entry);
			*originalTag = 0;
		}
	} else {
		/* object was untagged before callback */
		if (newTag) {
			/* now tagged, add table entry */
			entry.ref = object;
			entry.tag = newTag;
			resultTag = hashTableAdd(iteratorData->env->objectTagTable, &entry);
			*originalTag = resultTag->tag;
		}
	}
}


#ifdef JVMTI_HEAP_DEBUG 
static const char *
jvmtiHeapReferenceKindString(jvmtiHeapReferenceKind refKind)
{
	static const char *referenceNames[] = {
		"",
		"CLASS",
		"FIELD",
		"ARRAY_ELEMENT",
		"CLASS_LOADER",
		"SIGNERS",
		"PROTECTION_DOMAIN",
		"INTERFACE",
		"STATIC_FIELD",
		"CONSTANT_POOL",
		"SUPERCLASS",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"JNI_GLOBAL",
		"SYSTEM_CLASS",
		"MONITOR",
		"STACK_LOCAL",
		"JNI_LOCAL",
		"THREAD",
		"OTHER",
	};

	if (refKind > JVMTI_HEAP_REFERENCE_OTHER)
		return referenceNames[0];

	return referenceNames[refKind];

}
#endif
 
