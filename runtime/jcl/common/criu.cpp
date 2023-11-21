/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "jni.h"
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jcl_internal.h"

extern "C" {

#if defined(J9VM_OPT_CRIU_SUPPORT)

jlong JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_getCheckpointRestoreNanoTimeDeltaImpl(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread *)env;

	return currentThread->javaVM->checkpointState.checkpointRestoreTimeDelta;
}

jlong JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_getLastRestoreTimeImpl(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread *)env;

	return currentThread->javaVM->checkpointState.lastRestoreTimeMillis;
}

jboolean JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_isCheckpointAllowedImpl(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	jboolean res = JNI_FALSE;

	if (currentThread->javaVM->internalVMFunctions->isCheckpointAllowed(currentThread)) {
		res = JNI_TRUE;
	}

	return res;
}

jboolean JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_isCRIUSupportEnabledImpl(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	jboolean res = JNI_FALSE;

	if (currentThread->javaVM->internalVMFunctions->isCRIUSupportEnabled(currentThread)) {
		res = JNI_TRUE;
	}

	return res;
}

jboolean JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_enableCRIUSecProviderImpl(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	jboolean res = JNI_FALSE;

	if (currentThread->javaVM->internalVMFunctions->enableCRIUSecProvider(currentThread)) {
		res = JNI_TRUE;
	}

	return res;
}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
}
