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

static jvmtiError verifyLocation (J9JavaVM* vm, J9Method * ramMethod, jlong location);



jvmtiError JNICALL
jvmtiSetBreakpoint(jvmtiEnv* env,
	jmethodID method,
	jlocation location)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiSetBreakpoint_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Method * ramMethod;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_generate_breakpoint_events);

		ENSURE_JMETHODID_NON_NULL(method);

		/* Ensure the location is valid for the method */

		ramMethod = ((J9JNIMethodID *) method)->method;
		rc = verifyLocation(vm, ramMethod, location);
		if (rc == JVMTI_ERROR_NONE) {
			J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
			J9JVMTIAgentBreakpoint * agentBreakpoint;

			/* If this agent already has a breakpoint here, error */

			agentBreakpoint = findAgentBreakpoint(currentThread, j9env, ramMethod, (IDATA) location);
			if (agentBreakpoint != NULL) {
				rc = JVMTI_ERROR_DUPLICATE;
			} else {

				/* All breakpoint modification must take place under exclusive VM access */

				vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

				/* Reserve a slot in the agent breakpoint list */

				agentBreakpoint = pool_newElement(j9env->breakpoints);
				if (agentBreakpoint == NULL) {
					rc = JVMTI_ERROR_OUT_OF_MEMORY;
				} else {
					/* Install the breakpoint */

					agentBreakpoint->method = getCurrentMethodID(currentThread, ramMethod);
					if (agentBreakpoint->method == NULL) {
						rc = JVMTI_ERROR_OUT_OF_MEMORY;
					} else {
						agentBreakpoint->location = (IDATA) location;
						rc = installAgentBreakpoint(currentThread, agentBreakpoint);
						if (rc != JVMTI_ERROR_NONE) {
							pool_removeElement(j9env->breakpoints, agentBreakpoint);
						}
					}
				}

				vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
			}
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiSetBreakpoint);
}


jvmtiError JNICALL
jvmtiClearBreakpoint(jvmtiEnv* env,
	jmethodID method,
	jlocation location)
{
	J9JavaVM * vm = JAVAVM_FROM_ENV(env);
	jvmtiError rc;
	J9VMThread * currentThread;

	Trc_JVMTI_jvmtiClearBreakpoint_Entry(env);

	rc = getCurrentVMThread(vm, &currentThread);
	if (rc == JVMTI_ERROR_NONE) {
		J9Method * ramMethod;

		vm->internalVMFunctions->internalEnterVMFromJNI(currentThread);

		ENSURE_PHASE_LIVE(env);
		ENSURE_CAPABILITY(env, can_generate_breakpoint_events);

		ENSURE_JMETHODID_NON_NULL(method);

		/* Ensure the location is valid for the method */

		ramMethod = ((J9JNIMethodID *) method)->method;
		rc = verifyLocation(vm, ramMethod, location);
		if (rc == JVMTI_ERROR_NONE) {
			J9JVMTIEnv * j9env = (J9JVMTIEnv *) env;
			J9JVMTIAgentBreakpoint * agentBreakpoint;

			/* If this agent does not have a breakpoint here, error */

			agentBreakpoint = findAgentBreakpoint(currentThread, j9env, ramMethod, (IDATA) location);
			if (agentBreakpoint == NULL) {
				rc = JVMTI_ERROR_NOT_FOUND;
			} else {
				/* All breakpoint modification must take place under exclusive VM access */

				vm->internalVMFunctions->acquireExclusiveVMAccess(currentThread);

				/* Delete the breakpoint */

				deleteAgentBreakpoint(currentThread, j9env, agentBreakpoint);

				vm->internalVMFunctions->releaseExclusiveVMAccess(currentThread);
			}
		}
done:
		vm->internalVMFunctions->internalExitVMToJNI(currentThread);
	}

	TRACE_JVMTI_RETURN(jvmtiClearBreakpoint);
}


static jvmtiError
verifyLocation(J9JavaVM * vm, J9Method * ramMethod, jlong location)
{
	jvmtiError rc = JVMTI_ERROR_INVALID_LOCATION;
	J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);

	if ((location >= 0) && (location < (jlocation) J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod))) {
		/* Walk bytecodes, and make sure location points at the start of one */
		rc = JVMTI_ERROR_NONE;
	}

	return rc;
}



