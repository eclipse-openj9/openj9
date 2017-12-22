/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

static jvmtiError setFieldWatch (jvmtiEnv* env, jclass klass, jfieldID field, UDATA flag);
static jvmtiError clearFieldWatch (jvmtiEnv* env, jclass klass, jfieldID field, UDATA flag);


jvmtiError JNICALL
jvmtiSetFieldAccessWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetFieldAccessWatch_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_generate_field_access_events);

	rc = setFieldWatch(env, klass, field, J9JVMTI_WATCH_FIELD_ACCESS);

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

	rc = clearFieldWatch(env, klass, field, J9JVMTI_WATCH_FIELD_ACCESS);

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

	rc = setFieldWatch(env, klass, field, J9JVMTI_WATCH_FIELD_MODIFICATION);

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

	rc = clearFieldWatch(env, klass, field, J9JVMTI_WATCH_FIELD_MODIFICATION);

done:
	TRACE_JVMTI_RETURN(jvmtiClearFieldModificationWatch);
}


static jvmtiError
setFieldWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	UDATA flag)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread * currentThread;

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		pool_state poolState;
		J9JVMTIWatchedField * watchedField;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JFIELDID_NON_NULL(field);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		/* Scan currently watched fields to see if this agent is already watching */

		watchedField = pool_startDo(j9env->watchedFieldPool, &poolState);
		while (watchedField != NULL) {
			if (watchedField->fieldID == field) {
				break;
			}
			watchedField = pool_nextDo(&poolState);
		}

		/* If a watch already exists, tag it with the new request (access or modify), or error if it is already tagged */

		if (watchedField != NULL) {
			if (watchedField->flags & flag) {
				rc = JVMTI_ERROR_DUPLICATE;
			} else {
				watchedField->flags |= flag;
			}
		} else {
			watchedField = pool_newElement(j9env->watchedFieldPool);
			if (watchedField == NULL) {
				rc = JVMTI_ERROR_OUT_OF_MEMORY;
			} else {
				watchedField->flags = flag;
				watchedField->fieldID = field;
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
				if (J9_FSD_ENABLED(vm)) {
					vm->jitConfig->jitDataBreakpointAdded(currentThread);
				}
#endif
			}
		}

		if (rc == JVMTI_ERROR_NONE) {
			if (flag == J9JVMTI_WATCH_FIELD_MODIFICATION) {
				hookEvent(j9env, JVMTI_EVENT_FIELD_MODIFICATION);
			} else {
				hookEvent(j9env, JVMTI_EVENT_FIELD_ACCESS);
			}
		}

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	return rc;
}


static jvmtiError
clearFieldWatch(jvmtiEnv* env,
	jclass klass,
	jfieldID field,
	UDATA flag)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	jvmtiError rc = JVMTI_ERROR_NONE;
	J9VMThread * currentThread;

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		pool_state poolState;
		J9JVMTIWatchedField * watchedField;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_JCLASS_NON_NULL(klass);
		ENSURE_JFIELDID_NON_NULL(field);

		vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

		/* Scan currently watched fields to see if this agent is watching the field */

		watchedField = pool_startDo(j9env->watchedFieldPool, &poolState);
		while (watchedField != NULL) {
			if (watchedField->fieldID == field) {
				break;
			}
			watchedField = pool_nextDo(&poolState);
		}

		/* If the field is not being watched at all, or not watched with the requested flag (access or modify), error */

		if ((watchedField == NULL) || !(watchedField->flags & flag)) {
			rc = JVMTI_ERROR_NOT_FOUND;
		} else {
			if (flag == J9JVMTI_WATCH_FIELD_MODIFICATION) {
				unhookEvent(j9env, JVMTI_EVENT_FIELD_MODIFICATION);
			} else {
				unhookEvent(j9env, JVMTI_EVENT_FIELD_ACCESS);
			}
			watchedField->flags &= ~flag;
			if (watchedField->flags == 0) {
				pool_removeElement(j9env->watchedFieldPool, watchedField);
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
				if (J9_FSD_ENABLED(vm)) {
					vm->jitConfig->jitDataBreakpointRemoved(currentThread);
				}
#endif
			}
		}

		vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);

done:
		vm->internalVMFunctions->internalReleaseVMAccess(currentThread);
	}

	return rc;
}



