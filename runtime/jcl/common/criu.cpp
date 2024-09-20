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

	return currentThread->javaVM->checkpointState.lastRestoreTimeInNanoseconds;
}

jlong JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_getProcessRestoreStartTimeImpl(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread *)env;

	return currentThread->javaVM->checkpointState.processRestoreStartTimeInNanoseconds;
}

jboolean JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_isCheckpointAllowedImpl(JNIEnv *env, jclass unused)
{
	J9JavaVM *vm = ((J9VMThread *)env)->javaVM;
	jboolean res = JNI_FALSE;

	if (vm->internalVMFunctions->isCheckpointAllowed(vm)) {
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

jboolean JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_setupJNIFieldIDsAndCRIUAPI(JNIEnv *env, jclass unused)
{
	jclass currentExceptionClass = NULL;
	IDATA systemReturnCode = 0;
	const char *nlsMsgFormat = NULL;

	return ((J9VMThread *)env)->javaVM->internalVMFunctions->setupJNIFieldIDsAndCRIUAPI(env, &currentExceptionClass, &systemReturnCode, &nlsMsgFormat) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_checkpointJVMImpl(JNIEnv *env,
		jclass unused,
		jstring imagesDir,
		jboolean leaveRunning,
		jboolean shellJob,
		jboolean extUnixSupport,
		jint logLevel,
		jstring logFile,
		jboolean fileLocks,
		jstring workDir,
		jboolean tcpEstablished,
		jboolean autoDedup,
		jboolean trackMemory,
		jboolean unprivileged,
		jstring optionsFile,
		jstring environmentFile,
		jlong ghostFileLimit)
{
	((J9VMThread*)env)->javaVM->internalVMFunctions->criuCheckpointJVMImpl(env, imagesDir, leaveRunning, shellJob, extUnixSupport, logLevel, logFile, fileLocks, workDir, tcpEstablished, autoDedup, trackMemory, unprivileged, optionsFile, environmentFile, ghostFileLimit);
}

jobject JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_getRestoreSystemProperites(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	jobject systemProperties = NULL;

	if (NULL != vm->checkpointState.restoreArgsList) {
		systemProperties = vm->internalVMFunctions->getRestoreSystemProperites(currentThread);
	}

	return systemProperties;
}

#if defined(J9VM_OPT_CRAC_SUPPORT)
jstring JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_getCRaCCheckpointToDirImpl(JNIEnv *env, jclass unused)
{
	jstring result = NULL;
	char *cracCheckpointToDir = ((J9VMThread*)env)->javaVM->checkpointState.cracCheckpointToDir;

	if (NULL != cracCheckpointToDir) {
		result = env->NewStringUTF(cracCheckpointToDir);
	}

	return result;
}

jboolean JNICALL
Java_openj9_internal_criu_InternalCRIUSupport_isCRaCSupportEnabledImpl(JNIEnv *env, jclass unused)
{
	return J9_ARE_ANY_BITS_SET(((J9VMThread*)env)->javaVM->checkpointState.flags, J9VM_CRAC_IS_CHECKPOINT_ENABLED);
}
#endif /* defined(J9VM_OPT_CRAC_SUPPORT) */
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
}
