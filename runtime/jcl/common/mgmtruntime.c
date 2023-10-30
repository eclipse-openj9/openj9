/*******************************************************************************
 * Copyright IBM Corp. and others 1998
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
#include "j9.h"
#include "j2sever.h"
#include "ut_j9jcl.h"

jobject JNICALL
Java_com_ibm_java_lang_management_internal_RuntimeMXBeanImpl_getNameImpl(JNIEnv *env, jobject beanInstance)
{
	UDATA pid;
	char hostname[256];
	char result[256];
	PORT_ACCESS_FROM_ENV( env );
	OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);

	pid = j9sysinfo_get_pid();
	omrsysinfo_get_hostname( hostname, 256 );
	j9str_printf( PORTLIB, result, 256, "%zu@%s", pid, hostname );

	return (*env)->NewStringUTF( env, result );
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_RuntimeMXBeanImpl_getStartTimeImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

	return (jlong)javaVM->managementData->vmStartTime;
}

jlong JNICALL
Java_com_ibm_java_lang_management_internal_RuntimeMXBeanImpl_getUptimeImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread*)env)->javaVM;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	I_64 criuTimeDeltaMillis = 0;
	I_64 timeNow = j9time_current_time_millis();

#if defined(J9VM_OPT_CRIU_SUPPORT)
	criuTimeDeltaMillis = javaVM->checkpointState.checkpointRestoreTimeDelta / J9PORT_TIME_NS_PER_MS;
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	Trc_JCL_MXBean_getUptimeImpl(env, timeNow, javaVM->managementData->vmStartTime, criuTimeDeltaMillis);

	return (jlong)(timeNow - javaVM->managementData->vmStartTime - criuTimeDeltaMillis);
}

jboolean JNICALL
Java_com_ibm_java_lang_management_internal_RuntimeMXBeanImpl_isBootClassPathSupportedImpl(JNIEnv *env, jobject beanInstance)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	if (J2SE_VERSION(javaVM) < J2SE_V11) {
		return JNI_TRUE;
	} else {
		return JNI_FALSE;
	}
}

#if JAVA_SPEC_VERSION < 19
/**
 * This is a static function.
 * @return process ID of the caller.
 */
jlong JNICALL
Java_com_ibm_lang_management_internal_ExtendedRuntimeMXBeanImpl_getProcessIDImpl(JNIEnv *env, jclass clazz)
{
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

	jlong pid;

	pid =  (jlong) j9sysinfo_get_pid();
	return pid;
}
#endif /* JAVA_SPEC_VERSION < 19 */

/**
 * @return vm idle state of the JVM
 */
jint JNICALL
Java_com_ibm_lang_management_internal_ExtendedRuntimeMXBeanImpl_getVMIdleStateImpl(JNIEnv *env, jclass clazz)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;

	return (jint)(javaVM->vmRuntimeStateListener.vmRuntimeState); 
}
