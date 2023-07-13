/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#ifndef _Included_org_eclipse_openj9_criu_CRIUSupport
#define _Included_org_eclipse_openj9_criu_CRIUSupport

#include <jni.h>
#include "j9.h"

extern "C" {

void JNICALL
Java_org_eclipse_openj9_criu_CRIUSupport_checkpointJVMImpl(JNIEnv *env, jclass unused, jstring imagesDir, jboolean leaveRunning, jboolean shellJob, jboolean extUnixSupport, jint logLevel, jstring logFile, jboolean fileLocks, jstring workDir, jboolean tcpEstablished, jboolean autoDedup, jboolean trackMemory, jboolean unprivileged, jstring optionsFile, jstring envFile);

jobject JNICALL
Java_org_eclipse_openj9_criu_CRIUSupport_getRestoreSystemProperites(JNIEnv *env, jclass unused);

jboolean JNICALL
Java_org_eclipse_openj9_criu_CRIUSupport_setupJNIFieldIDsAndCRIUAPI(JNIEnv *env, jclass unused);
} /* extern "C" */

#endif
