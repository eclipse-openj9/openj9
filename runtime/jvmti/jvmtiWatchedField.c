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
#include "jvmti_internal.h"

static jvmtiError setFieldWatch (jvmtiEnv* env, jclass klass, jfieldID field, UDATA isModification);
static jvmtiError clearFieldWatch (jvmtiEnv* env, jclass klass, jfieldID field, UDATA isModification);


jvmtiError JNICALL
jvmtiSetFieldAccessWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetFieldAccessWatch_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_generate_field_access_events);

	rc = setFieldWatch(env, klass, field, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiSetFieldAccessWatch);
}


jvmtiError JNICALL
jvmtiClearFieldAccessWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiClearFieldAccessWatch_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_generate_field_access_events);

	rc = clearFieldWatch(env, klass, field, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiClearFieldAccessWatch);
}


jvmtiError JNICALL
jvmtiSetFieldModificationWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetFieldModificationWatch_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_generate_field_modification_events);

	rc = setFieldWatch(env, klass, field, TRUE);

done:
	TRACE_JVMTI_RETURN(jvmtiSetFieldModificationWatch);
}


jvmtiError JNICALL
jvmtiClearFieldModificationWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiClearFieldModificationWatch_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_generate_field_modification_events);

	rc = clearFieldWatch(env, klass, field, TRUE);

done:
	TRACE_JVMTI_RETURN(jvmtiClearFieldModificationWatch);
}


static jvmtiError
setFieldWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	UDATA isModification)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread * currentThread;

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class *clazz = NULL;
		J9JNIFieldID *fieldID = NULL;
		UDATA localFieldIndex = 0;
		UDATA fieldCount = 0;
		UDATA *watchBits = NULL;
		UDATA watchBit = 0;
		J9JVMTIWatchedClass *watchedClass = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JFIELDID_NON_NULL(field);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		fieldID = (J9JNIFieldID*)field;
		localFieldIndex = fieldID->index - fieldID->declaringClass->romClass->romMethodCount;
		fieldCount = clazz->romClass->romFieldCount;
		watchedClass = hashTableFind(j9env->watchedClasses, &clazz);
		if (NULL == watchedClass) {
			J9JVMTIWatchedClass exemplar = { clazz, NULL };
			watchedClass = hashTableAdd(j9env->watchedClasses, &exemplar);
			if (NULL == watchedClass) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				UDATA fieldCount = clazz->romClass->romFieldCount;
				if (fieldCount <= J9JVMTI_WATCHED_FIELDS_PER_UDATA) {
					watchedClass->watchBits = (UDATA*)0;
				} else {
					UDATA allocSize = sizeof(UDATA) * J9JVMTI_WATCHED_FIELD_ARRAY_INDEX(fieldCount + (J9JVMTI_WATCHED_FIELDS_PER_UDATA - 1));
					PORT_ACCESS_FROM_VMC(currentThread);
					watchBits = j9mem_allocate_memory(allocSize, J9MEM_CATEGORY_JVMTI);
					if (NULL == watchBits) {
						hashTableRemove(j9env->watchedClasses, watchedClass);
						rc = JVMTI_ERROR_OUT_OF_MEMORY;
					} else {
						memset(watchBits, 0, allocSize);
						watchedClass->watchBits = watchBits;
					}
				}
			}
		}

		if (rc == JVMTI_ERROR_NONE) {
			if (fieldCount <= J9JVMTI_WATCHED_FIELDS_PER_UDATA) {
				watchBits = (UDATA*)&watchedClass->watchBits;
			} else {
				watchBits = watchedClass->watchBits;	
			}
			watchBits += J9JVMTI_WATCHED_FIELD_ARRAY_INDEX(localFieldIndex);
			if (isModification) {
				watchBit = J9JVMTI_WATCHED_FIELD_MODIFICATION_BIT(localFieldIndex);	
			} else {
				watchBit = J9JVMTI_WATCHED_FIELD_ACCESS_BIT(localFieldIndex);	
			}
			if (*watchBits & watchBit) {
				rc = JVMTI_ERROR_DUPLICATE;
			} else {
				*watchBits |= watchBit;
				/* Tag this class and all its subclasses as having watched fields.
				 * If this class is already tagged, so are the subclasses.
				 */
				if (J9_ARE_NO_BITS_SET(clazz->classFlags, J9ClassHasWatchedFields)) {
					J9SubclassWalkState subclassState;
					J9Class *subclass = allSubclassesStartDo(clazz, &subclassState, TRUE);
					while (NULL != subclass) {
						subclass->classFlags |= J9ClassHasWatchedFields;
						subclass = allSubclassesNextDo(&subclassState);
					}
				}
				if (J9_FSD_ENABLED(vm)) {
					vm->jitConfig->jitDataBreakpointAdded(currentThread);
				}
				if (isModification) {
					hookEvent(j9env, JVMTI_EVENT_FIELD_MODIFICATION);
				} else {
					hookEvent(j9env, JVMTI_EVENT_FIELD_ACCESS);
				}
			}
		}

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	return rc;
}


static jvmtiError
clearFieldWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	UDATA isModification)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread * currentThread;

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Class *clazz = NULL;
		J9JNIFieldID *fieldID = NULL;
		UDATA localFieldIndex = 0;
		UDATA *watchBits = NULL;
		UDATA watchBit = 0;
		J9JVMTIWatchedClass *watchedClass = NULL;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JFIELDID_NON_NULL(field);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, klass);
		fieldID = (J9JNIFieldID*)field;
		localFieldIndex = fieldID->index - fieldID->declaringClass->romClass->romMethodCount;
		watchedClass = hashTableFind(j9env->watchedClasses, &clazz);
		if (NULL == watchedClass) {
			rc = JVMTI_ERROR_NOT_FOUND;
		} else {
			UDATA fieldCount = clazz->romClass->romFieldCount;
			if (fieldCount <= J9JVMTI_WATCHED_FIELDS_PER_UDATA) {
				watchBits = (UDATA*)&watchedClass->watchBits;
			} else {
				watchBits = watchedClass->watchBits;	
			}
			watchBits += J9JVMTI_WATCHED_FIELD_ARRAY_INDEX(localFieldIndex);
			if (isModification) {
				watchBit = J9JVMTI_WATCHED_FIELD_MODIFICATION_BIT(localFieldIndex);	
			} else {
				watchBit = J9JVMTI_WATCHED_FIELD_ACCESS_BIT(localFieldIndex);	
			}
			if (0 == (*watchBits & watchBit)) {
				rc = JVMTI_ERROR_NOT_FOUND;
			} else {
				*watchBits &= ~watchBit;
				if (J9_FSD_ENABLED(vm)) {
					vm->jitConfig->jitDataBreakpointRemoved(currentThread);
				}
				/* Consider checking for no remaining watches on the class
				 * and removing the watched fields bit from the class (and
				 * subclasses) and removing the hash table entry.
				 */
			}
		}

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	return rc;
}



