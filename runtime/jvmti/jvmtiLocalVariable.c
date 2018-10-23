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
#include "objhelp.h"

static jvmtiError jvmtiGetOrSetLocal (jvmtiEnv* env, jthread thread, jint depth, jint slot, void* value_ptr, char signature, jboolean isSet, jboolean getLocalInstance);


jvmtiError JNICALL
jvmtiGetLocalInstance(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jobject* value_ptr)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetLocalInstance_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);
	ENSURE_NON_NULL(value_ptr);

	*value_ptr = NULL;
	rc = jvmtiGetOrSetLocal(env, thread, depth, 0, value_ptr, 'L', FALSE, TRUE);

done:
	TRACE_JVMTI_RETURN(jvmtiGetLocalInstance);
}


jvmtiError JNICALL
jvmtiGetLocalObject(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jobject* value_ptr)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetLocalObject_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);
	ENSURE_NON_NULL(value_ptr);

	*value_ptr = NULL;
	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, value_ptr, 'L', FALSE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiGetLocalObject);
}


jvmtiError JNICALL
jvmtiGetLocalInt(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jint* value_ptr)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetLocalInt_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);
	ENSURE_NON_NULL(value_ptr);

	*value_ptr = 0;
	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, value_ptr, 'I', FALSE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiGetLocalInt);
}


jvmtiError JNICALL
jvmtiGetLocalLong(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jlong* value_ptr)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetLocalLong_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);
	ENSURE_NON_NULL(value_ptr);

	*value_ptr = 0;
	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, value_ptr, 'J', FALSE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiGetLocalLong);
}


jvmtiError JNICALL
jvmtiGetLocalFloat(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jfloat* value_ptr)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetLocalFloat_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);
	ENSURE_NON_NULL(value_ptr);

	*(jint*)value_ptr = 0;
	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, value_ptr, 'F', FALSE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiGetLocalFloat);
}


jvmtiError JNICALL
jvmtiGetLocalDouble(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jdouble* value_ptr)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiGetLocalDouble_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);
	ENSURE_NON_NULL(value_ptr);

	*(jlong*)value_ptr = 0;
	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, value_ptr, 'D', FALSE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiGetLocalDouble);
}


jvmtiError JNICALL
jvmtiSetLocalObject(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jobject value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetLocalObject_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);

	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, (j9object_t*) value, 'L', TRUE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiSetLocalObject);
}


jvmtiError JNICALL
jvmtiSetLocalInt(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jint value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetLocalInt_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);

	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, &value, 'I', TRUE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiSetLocalInt);
}


jvmtiError JNICALL
jvmtiSetLocalLong(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jlong value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetLocalLong_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);

	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, &value, 'J', TRUE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiSetLocalLong);
}


jvmtiError JNICALL
jvmtiSetLocalFloat(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jfloat value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetLocalFloat_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);

	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, &value, 'F', TRUE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiSetLocalFloat);
}


jvmtiError JNICALL
jvmtiSetLocalDouble(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	jdouble value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiSetLocalDouble_Entry(env);

	ENSURE_PHASE_LIVE(env);
	ENSURE_CAPABILITY(env, can_access_local_variables);

	ENSURE_NON_NEGATIVE(depth);

	rc = jvmtiGetOrSetLocal(env, thread, depth, slot, &value, 'D', TRUE, FALSE);

done:
	TRACE_JVMTI_RETURN(jvmtiSetLocalDouble);
}


#if (defined(J9VM_OPT_DEBUG_INFO_SERVER)) 
static jvmtiError
jvmtiGetOrSetLocal(jvmtiEnv* env,
	jthread thread,
	jint depth,
	jint slot,
	void* value_ptr,
	char signature,
	jboolean isSet,
	jboolean getLocalInstance)
{
	J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
	J9JavaVM * vm = j9env->vm;
	jvmtiError rc;
	J9VMThread * currentThread;

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);
		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			J9StackWalkState walkState;
			UDATA objectFetched = FALSE;

			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);
			rc = findDecompileInfo(currentThread, targetThread, (UDATA)depth, &walkState);
			if (JVMTI_ERROR_NONE == rc) {
				UDATA validateRC;
				UDATA slotValid = TRUE;
				UDATA * slotAddress;
				J9Method *ramMethod = walkState.userData3;
				U_32 offsetPC = (U_32)(UDATA)walkState.userData4;
				J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);

				if (getLocalInstance) {
					if (romMethod->modifiers & J9AccStatic) {
						/* GetLocalInstance illegal on static methods */
						validateRC = J9_SLOT_VALIDATE_ERROR_INVALID_SLOT;
					} else if (romMethod->modifiers & J9AccNative) {
						/* GetLocalInstance legal on non-static native methods */
						validateRC = J9_SLOT_VALIDATE_ERROR_NONE;
					} else {
						/* GetLocalInstance == GetLocalObject for non-static, non-native methods */
						validateRC = validateLocalSlot(currentThread, ramMethod, offsetPC, (U_32) slot, signature, TRUE);
					}
				} else {
					validateRC = validateLocalSlot(currentThread, ramMethod, offsetPC, (U_32) slot, signature, TRUE);
				}
				switch (validateRC) {
					case J9_SLOT_VALIDATE_ERROR_LOCAL_MAP_MISMATCH:
						slotValid = FALSE;
						/* intentional fall-through */
					case J9_SLOT_VALIDATE_ERROR_NONE:
						slotAddress = walkState.arg0EA - slot;
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
						if (walkState.jitInfo != NULL) {
							UDATA inlineDepth = (UDATA)walkState.userData2;
							slotAddress = vm->jitConfig->jitLocalSlotAddress(currentThread, &walkState, slot, inlineDepth);
							if (NULL == slotAddress) {
								rc = JVMTI_ERROR_OUT_OF_MEMORY;
								break;
							}
						}
#endif
						if (isSet) {
							if (slotValid) {
								switch(signature) {
									case 'D':
									case 'J':
										memcpy(slotAddress - 1, value_ptr, sizeof(U_64));
										break;
									case 'L':
										/* Perform type check? */
										*((j9object_t*) slotAddress) = (value_ptr == NULL ? NULL : *((j9object_t*) value_ptr));
										break;
									default:
										*((jint *) slotAddress) = *((jint *) value_ptr);
										break;
								}
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
								if (J9_FSD_ENABLED(vm)) {
									vm->jitConfig->jitStackLocalsModified(currentThread, &walkState);
								}
#endif
							}
						} else {
							switch(signature) {
								case 'D':
								case 'J':
									if (slotValid) {
										memcpy(value_ptr, slotAddress - 1, sizeof(U_64));
									} else {
										memset(value_ptr, 0, sizeof(U_64));
									}
									break;
								case 'L':
									/* CMVC 109592 - Must not modify the stack while a thread is halted for inspection - this includes creation of JNI local refs */
									objectFetched = TRUE;
									PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, (slotValid ? *((j9object_t*) slotAddress) : NULL));
									break;
								default:
									*((jint *) value_ptr) = (slotValid ? *((jint *) slotAddress) : 0);
									break;
							}
						}
						break;
					case J9_SLOT_VALIDATE_ERROR_INVALID_SLOT:
						rc = JVMTI_ERROR_INVALID_SLOT;
						break;
					case J9_SLOT_VALIDATE_ERROR_NATIVE_METHOD:
						rc = JVMTI_ERROR_OPAQUE_FRAME;
						break;
					case J9_SLOT_VALIDATE_ERROR_OUT_OF_MEMORY:
						rc = JVMTI_ERROR_OUT_OF_MEMORY;
						break;
					case J9_SLOT_VALIDATE_ERROR_TYPE_MISMATCH:
						rc = JVMTI_ERROR_TYPE_MISMATCH;
						break;
					default:
						rc = JVMTI_ERROR_INTERNAL;
						break;
				}
			} else {
				rc = JVMTI_ERROR_NO_MORE_FRAMES;
			}

			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
			if (objectFetched) {
				j9object_t obj = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				
				*((jobject *) value_ptr) = vm->internalVMFunctions->j9jni_createLocalRef((JNIEnv *) currentThread, obj);
			}
			releaseVMThread(currentThread, targetThread);
		}
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	return rc;
}

#endif /* J9VM_OPT_DEBUG_INFO_SERVER */

