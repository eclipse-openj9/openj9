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

static jvmtiError JNICALL jvmtiForceEarlyReturn(jvmtiEnv* env, jthread thread, jvmtiParamTypes returnValueType, void *value);

jvmtiError JNICALL
jvmtiForceEarlyReturnObject(jvmtiEnv* env,
			    jthread thread,
			    jobject value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiForceEarlyReturnVoid_Entry(env);

	rc = jvmtiForceEarlyReturn(env, thread, JVMTI_TYPE_JOBJECT, value);

	return rc; 
}


jvmtiError JNICALL
jvmtiForceEarlyReturnInt(jvmtiEnv* env,
			 jthread thread,
			 jint value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiForceEarlyReturnVoid_Entry(env);

	rc = jvmtiForceEarlyReturn(env, thread, JVMTI_TYPE_JINT, &value);

	return rc; 
}


jvmtiError JNICALL
jvmtiForceEarlyReturnLong(jvmtiEnv* env,
			  jthread thread,
			  jlong value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiForceEarlyReturnVoid_Entry(env);

	rc = jvmtiForceEarlyReturn(env, thread, JVMTI_TYPE_JLONG, &value);

	return rc; 
}


jvmtiError JNICALL
jvmtiForceEarlyReturnFloat(jvmtiEnv* env,
			   jthread thread,
			   jfloat value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiForceEarlyReturnVoid_Entry(env);

	rc = jvmtiForceEarlyReturn(env, thread, JVMTI_TYPE_JFLOAT, &value);

	return rc; 
}


jvmtiError JNICALL
jvmtiForceEarlyReturnDouble(jvmtiEnv* env,
			    jthread thread,
			    jdouble value)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiForceEarlyReturnVoid_Entry(env);

	rc = jvmtiForceEarlyReturn(env, thread, JVMTI_TYPE_JDOUBLE, &value);

	return rc; 
}


jvmtiError JNICALL
jvmtiForceEarlyReturnVoid(jvmtiEnv* env,
			  jthread thread)
{
	jvmtiError rc;

	Trc_JVMTI_jvmtiForceEarlyReturnVoid_Entry(env);

	rc = jvmtiForceEarlyReturn(env, thread, JVMTI_TYPE_CVOID, NULL);

	return rc;
}


/** 
 * \brief	Force an early return from the specified thread's current method
 * \ingroup	jvmtiForceEarlyReturn
 * 
 * @param env 		jvmti environment
 * @param thread 	thread to force a return in
 * @param returnValueType	type of the value to be returned
 * @param value		value to return
 * @return		a jvmtiError code 
 * 
 *	
 */
static jvmtiError JNICALL
jvmtiForceEarlyReturn(jvmtiEnv* env,
		      jthread thread,
		      jvmtiParamTypes returnValueType,
		      void *returnValue)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9VMThread * targetThread;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_force_early_return);
		
		/* Check if the jthread is really a j.l.Thread. a NULL jthread indicates that
		 * the user wants to use the current thread hence defer the assignment to getVMThread */
		
		if (thread) {
			ENSURE_JTHREAD(currentThread, thread);
		}

		rc = getVMThread(currentThread, thread, &targetThread, TRUE, TRUE);
		if (rc == JVMTI_ERROR_NONE) {
			/* Does this thread need to be suspended at an event? */
			vm->internalVMFunctions->haltThreadForInspection(currentThread, targetThread);
			if ((currentThread != targetThread) && !(targetThread->publicFlags & J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND))  {
				/* A thread other then current must be suspended in order to force early return */
				rc = JVMTI_ERROR_THREAD_NOT_SUSPENDED;
			} else {
				J9StackWalkState walkState;

				rc = findDecompileInfo(currentThread, targetThread, 0, &walkState);
  				if (JVMTI_ERROR_NONE == rc) {
  					J9Method *method = walkState.userData3;
  					J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
  					if (romMethod->modifiers & J9AccNative) {
  						rc = JVMTI_ERROR_OPAQUE_FRAME;
  					} else {
  						J9UTF8 *sig = J9ROMMETHOD_SIGNATURE(romMethod);
  						U_8 *data = J9UTF8_DATA(sig);
  						U_16 length = J9UTF8_LENGTH(sig);
  						char signatureType = data[length - 1];
  						jvmtiParamTypes methodReturnType = JVMTI_TYPE_CVOID;

  						if (('[' == data[length - 2]) || (';' == signatureType)) {
  							signatureType = 'L';
  						}
  						switch(signatureType) {
						case 'I':
						case 'B':
						case 'C':
						case 'Z':
						case 'S':
							methodReturnType = JVMTI_TYPE_JINT;
							break;
						case 'J':
							methodReturnType = JVMTI_TYPE_JLONG;
							break;
						case 'F':
							methodReturnType = JVMTI_TYPE_JFLOAT;
							break;
						case 'D':
							methodReturnType = JVMTI_TYPE_JDOUBLE;
							break;
						case 'L':
							methodReturnType = JVMTI_TYPE_JOBJECT;
							break;
  						}
  						if (methodReturnType == returnValueType) {
  							if (NULL != walkState.jitInfo) {
  								if (NULL == vm->jitConfig->jitAddDecompilationForFramePop(currentThread, &walkState)) {
  									rc = JVMTI_ERROR_OUT_OF_MEMORY;
  									goto resume;
  								}
  							}
  							vm->internalVMFunctions->setHaltFlag(targetThread, J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT);
  							targetThread->ferReturnType = returnValueType;
  							switch (returnValueType) {
  							case JVMTI_TYPE_JINT:
  							case JVMTI_TYPE_JFLOAT:
  								memcpy(&targetThread->ferReturnValue, returnValue, 4);
  								break;
  							case JVMTI_TYPE_JLONG:
  							case JVMTI_TYPE_JDOUBLE:
  								memcpy(&targetThread->ferReturnValue, returnValue, 8);
  								break;
  							case JVMTI_TYPE_JOBJECT:
  								if (returnValue) {
  									targetThread->forceEarlyReturnObjectSlot = *((j9object_t*) returnValue);
  								} else {
  									targetThread->forceEarlyReturnObjectSlot = NULL;
  								}
  								break;
  							default:
  								break;
  							}
  						} else {
  							rc = JVMTI_ERROR_TYPE_MISMATCH;
  						}
					}
				}
			}
resume:
			vm->internalVMFunctions->resumeThreadForInspection(currentThread, targetThread);
			releaseVMThread(currentThread, targetThread);
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiPopFrame);
}
