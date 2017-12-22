/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#ifndef JNI_CONVERT_H
#define JNI_CONVERT_H

#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JNI_GetStringPlatform(e, inst, outst, outlen, encode) \
	GetStringPlatform(e, inst, outst, outlen, encode)

#define JNI_GetStringPlatformLength(e, inst, outlen, encode) \
	GetStringPlatformLength(e, inst, outlen, encode)

#define JNI_NewStringPlatform(e, inst, outst, encode) \
	NewStringPlatform(e, inst, outst, encode)

jint GetStringPlatform(JNIEnv* env, jstring instr, char* outstr, jint outlen, const char* encoding);
jint GetStringPlatformLength(JNIEnv* env, jstring instr, jint* outlen, const char* encoding);
jint NewStringPlatform(JNIEnv* env, const char* instr, jstring* outstr, const char* encoding);
jint JNI_a2e_vsprintf(char *target, const char *format, va_list args);

#ifdef __cplusplus
}
#endif
#endif
