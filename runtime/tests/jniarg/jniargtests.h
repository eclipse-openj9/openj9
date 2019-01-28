
/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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


#include <stdio.h>
#include <stdarg.h>
#include "jni.h"
#include "j9comp.h"
#include "j9.h"
#include "j9port.h"

extern const jbyte test_jbyte[];
extern const jshort test_jshort[];
extern const jint test_jint[];
extern const jlong test_jlong[];
extern const jfloat test_jfloat[];
extern const jdouble test_jdouble[];
extern const jboolean test_jboolean[];
extern int jniTests;

extern void cFailure_jbyte(J9PortLibrary *portLib, char *functionName, int index, jbyte arg, jbyte expected);
extern void cFailure_jshort(J9PortLibrary *portLib, char *functionName, int index, jshort arg, jshort expected);
extern void cFailure_jint(J9PortLibrary *portLib, char *functionName, int index, jint arg, jint expected);
extern void cFailure_jlong(J9PortLibrary *portLib, char *functionName, int index, jlong arg, jlong expected);
extern void cFailure_jfloat(J9PortLibrary *portLib, char *functionName, int index, jfloat arg, jfloat expected);
extern void cFailure_jdouble(J9PortLibrary *portLib, char *functionName, int index, jdouble arg, jdouble expected);
extern void cFailure_jboolean(J9PortLibrary *portLib, char *functionName, int index, jboolean arg, jboolean expected);
extern J9JavaVM *getJ9JavaVM(JNIEnv * env);

void JNICALL Java_JniArgTests_logRetValError( JNIEnv *p_env, jobject p_this, jstring error_message );
void JNICALL Java_JniArgTests_summary( JNIEnv *p_env, jobject p_this );

/* Prototypes from args_01.c */
jbyte JNICALL Java_JniArgTests_nativeBBrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 );
jbyte JNICALL Java_JniArgTests_nativeBBBBrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 );
jbyte JNICALL Java_JniArgTests_nativeBSrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 );
jbyte JNICALL Java_JniArgTests_nativeBSBSrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 );
jbyte JNICALL Java_JniArgTests_nativeBIrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 );
jbyte JNICALL Java_JniArgTests_nativeBIBIrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 );
jbyte JNICALL Java_JniArgTests_nativeBJrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 );
jbyte JNICALL Java_JniArgTests_nativeBJBJrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 );
jbyte JNICALL Java_JniArgTests_nativeBFrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 );
jbyte JNICALL Java_JniArgTests_nativeBFBFrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 );
jbyte JNICALL Java_JniArgTests_nativeBDrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 );
jbyte JNICALL Java_JniArgTests_nativeBDBDrB( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 );
jbyte JNICALL Java_JniArgTests_nativeSBrB( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 );
jbyte JNICALL Java_JniArgTests_nativeSBSBrB( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 );
jbyte JNICALL Java_JniArgTests_nativeSSrB( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 );
jbyte JNICALL Java_JniArgTests_nativeSSSSrB( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 );
jbyte JNICALL Java_JniArgTests_nativeSIrB( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 );
jbyte JNICALL Java_JniArgTests_nativeSISIrB( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 );
jbyte JNICALL Java_JniArgTests_nativeSJrB( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 );
jbyte JNICALL Java_JniArgTests_nativeSJSJrB( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 );
jbyte JNICALL Java_JniArgTests_nativeSFrB( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 );
jbyte JNICALL Java_JniArgTests_nativeSFSFrB( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 );
jbyte JNICALL Java_JniArgTests_nativeSDrB( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 );
jbyte JNICALL Java_JniArgTests_nativeSDSDrB( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 );
jbyte JNICALL Java_JniArgTests_nativeIBrB( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 );
jbyte JNICALL Java_JniArgTests_nativeIBIBrB( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 );
jbyte JNICALL Java_JniArgTests_nativeISrB( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 );
jbyte JNICALL Java_JniArgTests_nativeISISrB( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 );
jbyte JNICALL Java_JniArgTests_nativeIIrB( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 );
jbyte JNICALL Java_JniArgTests_nativeIIIIrB( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 );
jbyte JNICALL Java_JniArgTests_nativeIJrB( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 );
jbyte JNICALL Java_JniArgTests_nativeIJIJrB( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 );
jbyte JNICALL Java_JniArgTests_nativeIFrB( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 );
jbyte JNICALL Java_JniArgTests_nativeIFIFrB( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 );
jbyte JNICALL Java_JniArgTests_nativeIDrB( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 );
jbyte JNICALL Java_JniArgTests_nativeIDIDrB( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 );
jbyte JNICALL Java_JniArgTests_nativeJBrB( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 );
jbyte JNICALL Java_JniArgTests_nativeJBJBrB( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 );
jbyte JNICALL Java_JniArgTests_nativeJSrB( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 );
jbyte JNICALL Java_JniArgTests_nativeJSJSrB( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 );
jbyte JNICALL Java_JniArgTests_nativeJIrB( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 );
jbyte JNICALL Java_JniArgTests_nativeJIJIrB( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 );
jbyte JNICALL Java_JniArgTests_nativeJJrB( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 );
jbyte JNICALL Java_JniArgTests_nativeJJJJrB( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 );
jbyte JNICALL Java_JniArgTests_nativeJFrB( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 );
jbyte JNICALL Java_JniArgTests_nativeJFJFrB( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 );
jbyte JNICALL Java_JniArgTests_nativeJDrB( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 );
jbyte JNICALL Java_JniArgTests_nativeJDJDrB( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 );
jbyte JNICALL Java_JniArgTests_nativeFBrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 );
jbyte JNICALL Java_JniArgTests_nativeFBFBrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 );

/* Prototypes from args_02.c */
jbyte JNICALL Java_JniArgTests_nativeFSrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 );
jbyte JNICALL Java_JniArgTests_nativeFSFSrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 );
jbyte JNICALL Java_JniArgTests_nativeFIrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 );
jbyte JNICALL Java_JniArgTests_nativeFIFIrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 );
jbyte JNICALL Java_JniArgTests_nativeFJrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 );
jbyte JNICALL Java_JniArgTests_nativeFJFJrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 );
jbyte JNICALL Java_JniArgTests_nativeFFrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 );
jbyte JNICALL Java_JniArgTests_nativeFFFFrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 );
jbyte JNICALL Java_JniArgTests_nativeFDrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 );
jbyte JNICALL Java_JniArgTests_nativeFDFDrB( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 );
jbyte JNICALL Java_JniArgTests_nativeDBrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 );
jbyte JNICALL Java_JniArgTests_nativeDBDBrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 );
jbyte JNICALL Java_JniArgTests_nativeDSrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 );
jbyte JNICALL Java_JniArgTests_nativeDSDSrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 );
jbyte JNICALL Java_JniArgTests_nativeDIrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 );
jbyte JNICALL Java_JniArgTests_nativeDIDIrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 );
jbyte JNICALL Java_JniArgTests_nativeDJrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 );
jbyte JNICALL Java_JniArgTests_nativeDJDJrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 );
jbyte JNICALL Java_JniArgTests_nativeDFrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 );
jbyte JNICALL Java_JniArgTests_nativeDFDFrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 );
jbyte JNICALL Java_JniArgTests_nativeDDrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 );
jbyte JNICALL Java_JniArgTests_nativeDDDDrB( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 );
jshort JNICALL Java_JniArgTests_nativeBBrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 );
jshort JNICALL Java_JniArgTests_nativeBBBBrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 );
jshort JNICALL Java_JniArgTests_nativeBSrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 );
jshort JNICALL Java_JniArgTests_nativeBSBSrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 );
jshort JNICALL Java_JniArgTests_nativeBIrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 );
jshort JNICALL Java_JniArgTests_nativeBIBIrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 );
jshort JNICALL Java_JniArgTests_nativeBJrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 );
jshort JNICALL Java_JniArgTests_nativeBJBJrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 );
jshort JNICALL Java_JniArgTests_nativeBFrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 );
jshort JNICALL Java_JniArgTests_nativeBFBFrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 );
jshort JNICALL Java_JniArgTests_nativeBDrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 );
jshort JNICALL Java_JniArgTests_nativeBDBDrS( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 );
jshort JNICALL Java_JniArgTests_nativeSBrS( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 );
jshort JNICALL Java_JniArgTests_nativeSBSBrS( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 );
jshort JNICALL Java_JniArgTests_nativeSSrS( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 );
jshort JNICALL Java_JniArgTests_nativeSSSSrS( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 );
jshort JNICALL Java_JniArgTests_nativeSIrS( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 );
jshort JNICALL Java_JniArgTests_nativeSISIrS( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 );
jshort JNICALL Java_JniArgTests_nativeSJrS( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 );
jshort JNICALL Java_JniArgTests_nativeSJSJrS( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 );
jshort JNICALL Java_JniArgTests_nativeSFrS( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 );
jshort JNICALL Java_JniArgTests_nativeSFSFrS( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 );
jshort JNICALL Java_JniArgTests_nativeSDrS( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 );
jshort JNICALL Java_JniArgTests_nativeSDSDrS( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 );
jshort JNICALL Java_JniArgTests_nativeIBrS( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 );
jshort JNICALL Java_JniArgTests_nativeIBIBrS( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 );
jshort JNICALL Java_JniArgTests_nativeISrS( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 );
jshort JNICALL Java_JniArgTests_nativeISISrS( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 );

/* Prototypes from args_03.c */
jshort JNICALL Java_JniArgTests_nativeIIrS( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 );
jshort JNICALL Java_JniArgTests_nativeIIIIrS( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 );
jshort JNICALL Java_JniArgTests_nativeIJrS( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 );
jshort JNICALL Java_JniArgTests_nativeIJIJrS( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 );
jshort JNICALL Java_JniArgTests_nativeIFrS( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 );
jshort JNICALL Java_JniArgTests_nativeIFIFrS( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 );
jshort JNICALL Java_JniArgTests_nativeIDrS( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 );
jshort JNICALL Java_JniArgTests_nativeIDIDrS( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 );
jshort JNICALL Java_JniArgTests_nativeJBrS( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 );
jshort JNICALL Java_JniArgTests_nativeJBJBrS( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 );
jshort JNICALL Java_JniArgTests_nativeJSrS( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 );
jshort JNICALL Java_JniArgTests_nativeJSJSrS( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 );
jshort JNICALL Java_JniArgTests_nativeJIrS( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 );
jshort JNICALL Java_JniArgTests_nativeJIJIrS( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 );
jshort JNICALL Java_JniArgTests_nativeJJrS( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 );
jshort JNICALL Java_JniArgTests_nativeJJJJrS( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 );
jshort JNICALL Java_JniArgTests_nativeJFrS( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 );
jshort JNICALL Java_JniArgTests_nativeJFJFrS( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 );
jshort JNICALL Java_JniArgTests_nativeJDrS( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 );
jshort JNICALL Java_JniArgTests_nativeJDJDrS( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 );
jshort JNICALL Java_JniArgTests_nativeFBrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 );
jshort JNICALL Java_JniArgTests_nativeFBFBrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 );
jshort JNICALL Java_JniArgTests_nativeFSrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 );
jshort JNICALL Java_JniArgTests_nativeFSFSrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 );
jshort JNICALL Java_JniArgTests_nativeFIrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 );
jshort JNICALL Java_JniArgTests_nativeFIFIrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 );
jshort JNICALL Java_JniArgTests_nativeFJrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 );
jshort JNICALL Java_JniArgTests_nativeFJFJrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 );
jshort JNICALL Java_JniArgTests_nativeFFrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 );
jshort JNICALL Java_JniArgTests_nativeFFFFrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 );
jshort JNICALL Java_JniArgTests_nativeFDrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 );
jshort JNICALL Java_JniArgTests_nativeFDFDrS( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 );
jshort JNICALL Java_JniArgTests_nativeDBrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 );
jshort JNICALL Java_JniArgTests_nativeDBDBrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 );
jshort JNICALL Java_JniArgTests_nativeDSrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 );
jshort JNICALL Java_JniArgTests_nativeDSDSrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 );
jshort JNICALL Java_JniArgTests_nativeDIrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 );
jshort JNICALL Java_JniArgTests_nativeDIDIrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 );
jshort JNICALL Java_JniArgTests_nativeDJrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 );
jshort JNICALL Java_JniArgTests_nativeDJDJrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 );
jshort JNICALL Java_JniArgTests_nativeDFrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 );
jshort JNICALL Java_JniArgTests_nativeDFDFrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 );
jshort JNICALL Java_JniArgTests_nativeDDrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 );
jshort JNICALL Java_JniArgTests_nativeDDDDrS( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 );
jint JNICALL Java_JniArgTests_nativeBBrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 );
jint JNICALL Java_JniArgTests_nativeBBBBrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 );
jint JNICALL Java_JniArgTests_nativeBSrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 );
jint JNICALL Java_JniArgTests_nativeBSBSrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 );
jint JNICALL Java_JniArgTests_nativeBIrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 );
jint JNICALL Java_JniArgTests_nativeBIBIrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 );

/* Prototypes from args_04.c */
jint JNICALL Java_JniArgTests_nativeBJrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 );
jint JNICALL Java_JniArgTests_nativeBJBJrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 );
jint JNICALL Java_JniArgTests_nativeBFrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 );
jint JNICALL Java_JniArgTests_nativeBFBFrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 );
jint JNICALL Java_JniArgTests_nativeBDrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 );
jint JNICALL Java_JniArgTests_nativeBDBDrI( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 );
jint JNICALL Java_JniArgTests_nativeSBrI( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 );
jint JNICALL Java_JniArgTests_nativeSBSBrI( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 );
jint JNICALL Java_JniArgTests_nativeSSrI( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 );
jint JNICALL Java_JniArgTests_nativeSSSSrI( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 );
jint JNICALL Java_JniArgTests_nativeSIrI( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 );
jint JNICALL Java_JniArgTests_nativeSISIrI( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 );
jint JNICALL Java_JniArgTests_nativeSJrI( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 );
jint JNICALL Java_JniArgTests_nativeSJSJrI( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 );
jint JNICALL Java_JniArgTests_nativeSFrI( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 );
jint JNICALL Java_JniArgTests_nativeSFSFrI( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 );
jint JNICALL Java_JniArgTests_nativeSDrI( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 );
jint JNICALL Java_JniArgTests_nativeSDSDrI( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 );
jint JNICALL Java_JniArgTests_nativeIBrI( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 );
jint JNICALL Java_JniArgTests_nativeIBIBrI( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 );
jint JNICALL Java_JniArgTests_nativeISrI( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 );
jint JNICALL Java_JniArgTests_nativeISISrI( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 );
jint JNICALL Java_JniArgTests_nativeIIrI( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 );
jint JNICALL Java_JniArgTests_nativeIIIIrI( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 );
jint JNICALL Java_JniArgTests_nativeIJrI( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 );
jint JNICALL Java_JniArgTests_nativeIJIJrI( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 );
jint JNICALL Java_JniArgTests_nativeIFrI( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 );
jint JNICALL Java_JniArgTests_nativeIFIFrI( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 );
jint JNICALL Java_JniArgTests_nativeIDrI( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 );
jint JNICALL Java_JniArgTests_nativeIDIDrI( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 );
jint JNICALL Java_JniArgTests_nativeJBrI( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 );
jint JNICALL Java_JniArgTests_nativeJBJBrI( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 );
jint JNICALL Java_JniArgTests_nativeJSrI( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 );
jint JNICALL Java_JniArgTests_nativeJSJSrI( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 );
jint JNICALL Java_JniArgTests_nativeJIrI( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 );
jint JNICALL Java_JniArgTests_nativeJIJIrI( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 );
jint JNICALL Java_JniArgTests_nativeJJrI( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 );
jint JNICALL Java_JniArgTests_nativeJJJJrI( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 );
jint JNICALL Java_JniArgTests_nativeJFrI( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 );
jint JNICALL Java_JniArgTests_nativeJFJFrI( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 );
jint JNICALL Java_JniArgTests_nativeJDrI( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 );
jint JNICALL Java_JniArgTests_nativeJDJDrI( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 );
jint JNICALL Java_JniArgTests_nativeFBrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 );
jint JNICALL Java_JniArgTests_nativeFBFBrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 );
jint JNICALL Java_JniArgTests_nativeFSrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 );
jint JNICALL Java_JniArgTests_nativeFSFSrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 );
jint JNICALL Java_JniArgTests_nativeFIrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 );
jint JNICALL Java_JniArgTests_nativeFIFIrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 );
jint JNICALL Java_JniArgTests_nativeFJrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 );
jint JNICALL Java_JniArgTests_nativeFJFJrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 );

/* Prototypes from args_05.c */
jint JNICALL Java_JniArgTests_nativeFFrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 );
jint JNICALL Java_JniArgTests_nativeFFFFrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 );
jint JNICALL Java_JniArgTests_nativeFDrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 );
jint JNICALL Java_JniArgTests_nativeFDFDrI( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 );
jint JNICALL Java_JniArgTests_nativeDBrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 );
jint JNICALL Java_JniArgTests_nativeDBDBrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 );
jint JNICALL Java_JniArgTests_nativeDSrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 );
jint JNICALL Java_JniArgTests_nativeDSDSrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 );
jint JNICALL Java_JniArgTests_nativeDIrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 );
jint JNICALL Java_JniArgTests_nativeDIDIrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 );
jint JNICALL Java_JniArgTests_nativeDJrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 );
jint JNICALL Java_JniArgTests_nativeDJDJrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 );
jint JNICALL Java_JniArgTests_nativeDFrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 );
jint JNICALL Java_JniArgTests_nativeDFDFrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 );
jint JNICALL Java_JniArgTests_nativeDDrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 );
jint JNICALL Java_JniArgTests_nativeDDDDrI( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 );
jlong JNICALL Java_JniArgTests_nativeBBrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 );
jlong JNICALL Java_JniArgTests_nativeBBBBrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 );
jlong JNICALL Java_JniArgTests_nativeBSrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 );
jlong JNICALL Java_JniArgTests_nativeBSBSrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 );
jlong JNICALL Java_JniArgTests_nativeBIrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 );
jlong JNICALL Java_JniArgTests_nativeBIBIrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 );
jlong JNICALL Java_JniArgTests_nativeBJrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 );
jlong JNICALL Java_JniArgTests_nativeBJBJrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 );
jlong JNICALL Java_JniArgTests_nativeBFrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 );
jlong JNICALL Java_JniArgTests_nativeBFBFrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 );
jlong JNICALL Java_JniArgTests_nativeBDrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 );
jlong JNICALL Java_JniArgTests_nativeBDBDrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 );
jlong JNICALL Java_JniArgTests_nativeSBrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 );
jlong JNICALL Java_JniArgTests_nativeSBSBrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 );
jlong JNICALL Java_JniArgTests_nativeSSrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 );
jlong JNICALL Java_JniArgTests_nativeSSSSrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 );
jlong JNICALL Java_JniArgTests_nativeSIrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 );
jlong JNICALL Java_JniArgTests_nativeSISIrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 );
jlong JNICALL Java_JniArgTests_nativeSJrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 );
jlong JNICALL Java_JniArgTests_nativeSJSJrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 );
jlong JNICALL Java_JniArgTests_nativeSFrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 );
jlong JNICALL Java_JniArgTests_nativeSFSFrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 );
jlong JNICALL Java_JniArgTests_nativeSDrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 );
jlong JNICALL Java_JniArgTests_nativeSDSDrJ( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 );
jlong JNICALL Java_JniArgTests_nativeIBrJ( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 );
jlong JNICALL Java_JniArgTests_nativeIBIBrJ( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 );
jlong JNICALL Java_JniArgTests_nativeISrJ( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 );
jlong JNICALL Java_JniArgTests_nativeISISrJ( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 );
jlong JNICALL Java_JniArgTests_nativeIIrJ( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 );
jlong JNICALL Java_JniArgTests_nativeIIIIrJ( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 );
jlong JNICALL Java_JniArgTests_nativeIJrJ( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 );
jlong JNICALL Java_JniArgTests_nativeIJIJrJ( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 );
jlong JNICALL Java_JniArgTests_nativeIFrJ( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 );
jlong JNICALL Java_JniArgTests_nativeIFIFrJ( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 );

/* Prototypes from args_06.c */
jlong JNICALL Java_JniArgTests_nativeIDrJ( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 );
jlong JNICALL Java_JniArgTests_nativeIDIDrJ( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 );
jlong JNICALL Java_JniArgTests_nativeJBrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 );
jlong JNICALL Java_JniArgTests_nativeJBJBrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 );
jlong JNICALL Java_JniArgTests_nativeJSrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 );
jlong JNICALL Java_JniArgTests_nativeJSJSrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 );
jlong JNICALL Java_JniArgTests_nativeJIrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 );
jlong JNICALL Java_JniArgTests_nativeJIJIrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 );
jlong JNICALL Java_JniArgTests_nativeJJrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 );
jlong JNICALL Java_JniArgTests_nativeJJJJrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 );
jlong JNICALL Java_JniArgTests_nativeJFrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 );
jlong JNICALL Java_JniArgTests_nativeJFJFrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 );
jlong JNICALL Java_JniArgTests_nativeJDrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 );
jlong JNICALL Java_JniArgTests_nativeJDJDrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 );
jlong JNICALL Java_JniArgTests_nativeFBrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 );
jlong JNICALL Java_JniArgTests_nativeFBFBrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 );
jlong JNICALL Java_JniArgTests_nativeFSrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 );
jlong JNICALL Java_JniArgTests_nativeFSFSrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 );
jlong JNICALL Java_JniArgTests_nativeFIrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 );
jlong JNICALL Java_JniArgTests_nativeFIFIrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 );
jlong JNICALL Java_JniArgTests_nativeFJrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 );
jlong JNICALL Java_JniArgTests_nativeFJFJrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 );
jlong JNICALL Java_JniArgTests_nativeFFrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 );
jlong JNICALL Java_JniArgTests_nativeFFFFrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 );
jlong JNICALL Java_JniArgTests_nativeFDrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 );
jlong JNICALL Java_JniArgTests_nativeFDFDrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 );
jlong JNICALL Java_JniArgTests_nativeDBrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 );
jlong JNICALL Java_JniArgTests_nativeDBDBrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 );
jlong JNICALL Java_JniArgTests_nativeDSrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 );
jlong JNICALL Java_JniArgTests_nativeDSDSrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 );
jlong JNICALL Java_JniArgTests_nativeDIrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 );
jlong JNICALL Java_JniArgTests_nativeDIDIrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 );
jlong JNICALL Java_JniArgTests_nativeDJrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 );
jlong JNICALL Java_JniArgTests_nativeDJDJrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 );
jlong JNICALL Java_JniArgTests_nativeDFrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 );
jlong JNICALL Java_JniArgTests_nativeDFDFrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 );
jlong JNICALL Java_JniArgTests_nativeDDrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 );
jlong JNICALL Java_JniArgTests_nativeDDDDrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 );
jfloat JNICALL Java_JniArgTests_nativeBBrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 );
jfloat JNICALL Java_JniArgTests_nativeBBBBrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 );
jfloat JNICALL Java_JniArgTests_nativeBSrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 );
jfloat JNICALL Java_JniArgTests_nativeBSBSrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 );
jfloat JNICALL Java_JniArgTests_nativeBIrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 );
jfloat JNICALL Java_JniArgTests_nativeBIBIrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 );
jfloat JNICALL Java_JniArgTests_nativeBJrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 );
jfloat JNICALL Java_JniArgTests_nativeBJBJrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 );
jfloat JNICALL Java_JniArgTests_nativeBFrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 );
jfloat JNICALL Java_JniArgTests_nativeBFBFrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 );
jfloat JNICALL Java_JniArgTests_nativeBDrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 );
jfloat JNICALL Java_JniArgTests_nativeBDBDrF( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 );

/* Prototypes from args_07.c */
jfloat JNICALL Java_JniArgTests_nativeSBrF( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 );
jfloat JNICALL Java_JniArgTests_nativeSBSBrF( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 );
jfloat JNICALL Java_JniArgTests_nativeSSrF( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 );
jfloat JNICALL Java_JniArgTests_nativeSSSSrF( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 );
jfloat JNICALL Java_JniArgTests_nativeSIrF( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 );
jfloat JNICALL Java_JniArgTests_nativeSISIrF( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 );
jfloat JNICALL Java_JniArgTests_nativeSJrF( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 );
jfloat JNICALL Java_JniArgTests_nativeSJSJrF( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 );
jfloat JNICALL Java_JniArgTests_nativeSFrF( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 );
jfloat JNICALL Java_JniArgTests_nativeSFSFrF( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 );
jfloat JNICALL Java_JniArgTests_nativeSDrF( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 );
jfloat JNICALL Java_JniArgTests_nativeSDSDrF( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 );
jfloat JNICALL Java_JniArgTests_nativeIBrF( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 );
jfloat JNICALL Java_JniArgTests_nativeIBIBrF( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 );
jfloat JNICALL Java_JniArgTests_nativeISrF( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 );
jfloat JNICALL Java_JniArgTests_nativeISISrF( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 );
jfloat JNICALL Java_JniArgTests_nativeIIrF( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 );
jfloat JNICALL Java_JniArgTests_nativeIIIIrF( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 );
jfloat JNICALL Java_JniArgTests_nativeIJrF( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 );
jfloat JNICALL Java_JniArgTests_nativeIJIJrF( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 );
jfloat JNICALL Java_JniArgTests_nativeIFrF( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 );
jfloat JNICALL Java_JniArgTests_nativeIFIFrF( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 );
jfloat JNICALL Java_JniArgTests_nativeIDrF( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 );
jfloat JNICALL Java_JniArgTests_nativeIDIDrF( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 );
jfloat JNICALL Java_JniArgTests_nativeJBrF( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 );
jfloat JNICALL Java_JniArgTests_nativeJBJBrF( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 );
jfloat JNICALL Java_JniArgTests_nativeJSrF( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 );
jfloat JNICALL Java_JniArgTests_nativeJSJSrF( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 );
jfloat JNICALL Java_JniArgTests_nativeJIrF( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 );
jfloat JNICALL Java_JniArgTests_nativeJIJIrF( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 );
jfloat JNICALL Java_JniArgTests_nativeJJrF( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 );
jfloat JNICALL Java_JniArgTests_nativeJJJJrF( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 );
jfloat JNICALL Java_JniArgTests_nativeJFrF( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 );
jfloat JNICALL Java_JniArgTests_nativeJFJFrF( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 );
jfloat JNICALL Java_JniArgTests_nativeJDrF( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 );
jfloat JNICALL Java_JniArgTests_nativeJDJDrF( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 );
jfloat JNICALL Java_JniArgTests_nativeFBrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 );
jfloat JNICALL Java_JniArgTests_nativeFBFBrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 );
jfloat JNICALL Java_JniArgTests_nativeFSrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 );
jfloat JNICALL Java_JniArgTests_nativeFSFSrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 );
jfloat JNICALL Java_JniArgTests_nativeFIrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 );
jfloat JNICALL Java_JniArgTests_nativeFIFIrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 );
jfloat JNICALL Java_JniArgTests_nativeFJrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 );
jfloat JNICALL Java_JniArgTests_nativeFJFJrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 );
jfloat JNICALL Java_JniArgTests_nativeFFrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 );
jfloat JNICALL Java_JniArgTests_nativeFFFFrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 );
jfloat JNICALL Java_JniArgTests_nativeFDrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 );
jfloat JNICALL Java_JniArgTests_nativeFDFDrF( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 );
jfloat JNICALL Java_JniArgTests_nativeDBrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 );
jfloat JNICALL Java_JniArgTests_nativeDBDBrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 );

/* Prototypes from args_08.c */
jfloat JNICALL Java_JniArgTests_nativeDSrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 );
jfloat JNICALL Java_JniArgTests_nativeDSDSrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 );
jfloat JNICALL Java_JniArgTests_nativeDIrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 );
jfloat JNICALL Java_JniArgTests_nativeDIDIrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 );
jfloat JNICALL Java_JniArgTests_nativeDJrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 );
jfloat JNICALL Java_JniArgTests_nativeDJDJrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 );
jfloat JNICALL Java_JniArgTests_nativeDFrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 );
jfloat JNICALL Java_JniArgTests_nativeDFDFrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 );
jfloat JNICALL Java_JniArgTests_nativeDDrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 );
jfloat JNICALL Java_JniArgTests_nativeDDDDrF( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 );
jdouble JNICALL Java_JniArgTests_nativeBBrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2 );
jdouble JNICALL Java_JniArgTests_nativeBBBBrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jbyte arg3, jbyte arg4 );
jdouble JNICALL Java_JniArgTests_nativeBSrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2 );
jdouble JNICALL Java_JniArgTests_nativeBSBSrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jshort arg2, jbyte arg3, jshort arg4 );
jdouble JNICALL Java_JniArgTests_nativeBIrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2 );
jdouble JNICALL Java_JniArgTests_nativeBIBIrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jint arg2, jbyte arg3, jint arg4 );
jdouble JNICALL Java_JniArgTests_nativeBJrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2 );
jdouble JNICALL Java_JniArgTests_nativeBJBJrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jlong arg2, jbyte arg3, jlong arg4 );
jdouble JNICALL Java_JniArgTests_nativeBFrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2 );
jdouble JNICALL Java_JniArgTests_nativeBFBFrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jfloat arg2, jbyte arg3, jfloat arg4 );
jdouble JNICALL Java_JniArgTests_nativeBDrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2 );
jdouble JNICALL Java_JniArgTests_nativeBDBDrD( JNIEnv *p_env, jobject p_this, jbyte arg1, jdouble arg2, jbyte arg3, jdouble arg4 );
jdouble JNICALL Java_JniArgTests_nativeSBrD( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2 );
jdouble JNICALL Java_JniArgTests_nativeSBSBrD( JNIEnv *p_env, jobject p_this, jshort arg1, jbyte arg2, jshort arg3, jbyte arg4 );
jdouble JNICALL Java_JniArgTests_nativeSSrD( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2 );
jdouble JNICALL Java_JniArgTests_nativeSSSSrD( JNIEnv *p_env, jobject p_this, jshort arg1, jshort arg2, jshort arg3, jshort arg4 );
jdouble JNICALL Java_JniArgTests_nativeSIrD( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2 );
jdouble JNICALL Java_JniArgTests_nativeSISIrD( JNIEnv *p_env, jobject p_this, jshort arg1, jint arg2, jshort arg3, jint arg4 );
jdouble JNICALL Java_JniArgTests_nativeSJrD( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2 );
jdouble JNICALL Java_JniArgTests_nativeSJSJrD( JNIEnv *p_env, jobject p_this, jshort arg1, jlong arg2, jshort arg3, jlong arg4 );
jdouble JNICALL Java_JniArgTests_nativeSFrD( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2 );
jdouble JNICALL Java_JniArgTests_nativeSFSFrD( JNIEnv *p_env, jobject p_this, jshort arg1, jfloat arg2, jshort arg3, jfloat arg4 );
jdouble JNICALL Java_JniArgTests_nativeSDrD( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2 );
jdouble JNICALL Java_JniArgTests_nativeSDSDrD( JNIEnv *p_env, jobject p_this, jshort arg1, jdouble arg2, jshort arg3, jdouble arg4 );
jdouble JNICALL Java_JniArgTests_nativeIBrD( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2 );
jdouble JNICALL Java_JniArgTests_nativeIBIBrD( JNIEnv *p_env, jobject p_this, jint arg1, jbyte arg2, jint arg3, jbyte arg4 );
jdouble JNICALL Java_JniArgTests_nativeISrD( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2 );
jdouble JNICALL Java_JniArgTests_nativeISISrD( JNIEnv *p_env, jobject p_this, jint arg1, jshort arg2, jint arg3, jshort arg4 );
jdouble JNICALL Java_JniArgTests_nativeIIrD( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2 );
jdouble JNICALL Java_JniArgTests_nativeIIIIrD( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4 );
jdouble JNICALL Java_JniArgTests_nativeIJrD( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2 );
jdouble JNICALL Java_JniArgTests_nativeIJIJrD( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4 );
jdouble JNICALL Java_JniArgTests_nativeIFrD( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2 );
jdouble JNICALL Java_JniArgTests_nativeIFIFrD( JNIEnv *p_env, jobject p_this, jint arg1, jfloat arg2, jint arg3, jfloat arg4 );
jdouble JNICALL Java_JniArgTests_nativeIDrD( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2 );
jdouble JNICALL Java_JniArgTests_nativeIDIDrD( JNIEnv *p_env, jobject p_this, jint arg1, jdouble arg2, jint arg3, jdouble arg4 );
jdouble JNICALL Java_JniArgTests_nativeJBrD( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2 );
jdouble JNICALL Java_JniArgTests_nativeJBJBrD( JNIEnv *p_env, jobject p_this, jlong arg1, jbyte arg2, jlong arg3, jbyte arg4 );
jdouble JNICALL Java_JniArgTests_nativeJSrD( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2 );
jdouble JNICALL Java_JniArgTests_nativeJSJSrD( JNIEnv *p_env, jobject p_this, jlong arg1, jshort arg2, jlong arg3, jshort arg4 );

/* Prototypes from args_09.c */
jdouble JNICALL Java_JniArgTests_nativeJIrD( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2 );
jdouble JNICALL Java_JniArgTests_nativeJIJIrD( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4 );
jdouble JNICALL Java_JniArgTests_nativeJJrD( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2 );
jdouble JNICALL Java_JniArgTests_nativeJJJJrD( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4 );
jdouble JNICALL Java_JniArgTests_nativeJFrD( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2 );
jdouble JNICALL Java_JniArgTests_nativeJFJFrD( JNIEnv *p_env, jobject p_this, jlong arg1, jfloat arg2, jlong arg3, jfloat arg4 );
jdouble JNICALL Java_JniArgTests_nativeJDrD( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2 );
jdouble JNICALL Java_JniArgTests_nativeJDJDrD( JNIEnv *p_env, jobject p_this, jlong arg1, jdouble arg2, jlong arg3, jdouble arg4 );
jdouble JNICALL Java_JniArgTests_nativeFBrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2 );
jdouble JNICALL Java_JniArgTests_nativeFBFBrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jbyte arg2, jfloat arg3, jbyte arg4 );
jdouble JNICALL Java_JniArgTests_nativeFSrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2 );
jdouble JNICALL Java_JniArgTests_nativeFSFSrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jshort arg2, jfloat arg3, jshort arg4 );
jdouble JNICALL Java_JniArgTests_nativeFIrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2 );
jdouble JNICALL Java_JniArgTests_nativeFIFIrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jint arg2, jfloat arg3, jint arg4 );
jdouble JNICALL Java_JniArgTests_nativeFJrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2 );
jdouble JNICALL Java_JniArgTests_nativeFJFJrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jlong arg2, jfloat arg3, jlong arg4 );
jdouble JNICALL Java_JniArgTests_nativeFFrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2 );
jdouble JNICALL Java_JniArgTests_nativeFFFFrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4 );
jdouble JNICALL Java_JniArgTests_nativeFDrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2 );
jdouble JNICALL Java_JniArgTests_nativeFDFDrD( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4 );
jdouble JNICALL Java_JniArgTests_nativeDBrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2 );
jdouble JNICALL Java_JniArgTests_nativeDBDBrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jbyte arg2, jdouble arg3, jbyte arg4 );
jdouble JNICALL Java_JniArgTests_nativeDSrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2 );
jdouble JNICALL Java_JniArgTests_nativeDSDSrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jshort arg2, jdouble arg3, jshort arg4 );
jdouble JNICALL Java_JniArgTests_nativeDIrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2 );
jdouble JNICALL Java_JniArgTests_nativeDIDIrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jint arg2, jdouble arg3, jint arg4 );
jdouble JNICALL Java_JniArgTests_nativeDJrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2 );
jdouble JNICALL Java_JniArgTests_nativeDJDJrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jlong arg2, jdouble arg3, jlong arg4 );
jdouble JNICALL Java_JniArgTests_nativeDFrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2 );
jdouble JNICALL Java_JniArgTests_nativeDFDFrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4 );
jdouble JNICALL Java_JniArgTests_nativeDDrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2 );
jdouble JNICALL Java_JniArgTests_nativeDDDDrD( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4 );
jlong JNICALL Java_JniArgTests_nativeIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIrJ( JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5, jint arg6, jint arg7, jint arg8, jint arg9, jint arg10, jint arg11, jint arg12, jint arg13, jint arg14, jint arg15, jint arg16, jint arg17, jint arg18, jint arg19, jint arg20, jint arg21, jint arg22, jint arg23, jint arg24, jint arg25, jint arg26, jint arg27, jint arg28, jint arg29, jint arg30, jint arg31, jint arg32, jint arg33, jint arg34, jint arg35, jint arg36, jint arg37, jint arg38, jint arg39, jint arg40, jint arg41, jint arg42, jint arg43, jint arg44, jint arg45, jint arg46, jint arg47, jint arg48, jint arg49, jint arg50, jint arg51, jint arg52, jint arg53, jint arg54, jint arg55, jint arg56, jint arg57, jint arg58, jint arg59, jint arg60, jint arg61, jint arg62, jint arg63, jint arg64, jint arg65, jint arg66, jint arg67, jint arg68, jint arg69, jint arg70, jint arg71, jint arg72, jint arg73, jint arg74, jint arg75, jint arg76, jint arg77, jint arg78, jint arg79, jint arg80, jint arg81, jint arg82, jint arg83, jint arg84, jint arg85, jint arg86, jint arg87, jint arg88, jint arg89, jint arg90, jint arg91, jint arg92, jint arg93, jint arg94, jint arg95, jint arg96, jint arg97, jint arg98, jint arg99, jint arg100, jint arg101, jint arg102, jint arg103, jint arg104, jint arg105, jint arg106, jint arg107, jint arg108, jint arg109, jint arg110, jint arg111, jint arg112, jint arg113, jint arg114, jint arg115, jint arg116, jint arg117, jint arg118, jint arg119, jint arg120, jint arg121, jint arg122, jint arg123, jint arg124, jint arg125, jint arg126, jint arg127, jint arg128, jint arg129, jint arg130, jint arg131, jint arg132, jint arg133, jint arg134, jint arg135, jint arg136, jint arg137, jint arg138, jint arg139, jint arg140, jint arg141, jint arg142, jint arg143, jint arg144, jint arg145, jint arg146, jint arg147, jint arg148, jint arg149, jint arg150, jint arg151, jint arg152, jint arg153, jint arg154, jint arg155, jint arg156, jint arg157, jint arg158, jint arg159, jint arg160, jint arg161, jint arg162, jint arg163, jint arg164, jint arg165, jint arg166, jint arg167, jint arg168, jint arg169, jint arg170, jint arg171, jint arg172, jint arg173, jint arg174, jint arg175, jint arg176, jint arg177, jint arg178, jint arg179, jint arg180, jint arg181, jint arg182, jint arg183, jint arg184, jint arg185, jint arg186, jint arg187, jint arg188, jint arg189, jint arg190, jint arg191, jint arg192, jint arg193, jint arg194, jint arg195, jint arg196, jint arg197, jint arg198, jint arg199, jint arg200, jint arg201, jint arg202, jint arg203, jint arg204, jint arg205, jint arg206, jint arg207, jint arg208, jint arg209, jint arg210, jint arg211, jint arg212, jint arg213, jint arg214, jint arg215, jint arg216, jint arg217, jint arg218, jint arg219, jint arg220, jint arg221, jint arg222, jint arg223, jint arg224, jint arg225, jint arg226, jint arg227, jint arg228, jint arg229, jint arg230, jint arg231, jint arg232, jint arg233, jint arg234, jint arg235, jint arg236, jint arg237, jint arg238, jint arg239, jint arg240, jint arg241, jint arg242, jint arg243, jint arg244, jint arg245, jint arg246, jint arg247, jint arg248, jint arg249, jint arg250, jint arg251, jint arg252, jint arg253, jint arg254 );
jint JNICALL Java_JniArgTests_nativeIJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJrI( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jlong arg3, jlong arg4, jlong arg5, jlong arg6, jlong arg7, jlong arg8, jlong arg9, jlong arg10, jlong arg11, jlong arg12, jlong arg13, jlong arg14, jlong arg15, jlong arg16, jlong arg17, jlong arg18, jlong arg19, jlong arg20, jlong arg21, jlong arg22, jlong arg23, jlong arg24, jlong arg25, jlong arg26, jlong arg27, jlong arg28, jlong arg29, jlong arg30, jlong arg31, jlong arg32, jlong arg33, jlong arg34, jlong arg35, jlong arg36, jlong arg37, jlong arg38, jlong arg39, jlong arg40, jlong arg41, jlong arg42, jlong arg43, jlong arg44, jlong arg45, jlong arg46, jlong arg47, jlong arg48, jlong arg49, jlong arg50, jlong arg51, jlong arg52, jlong arg53, jlong arg54, jlong arg55, jlong arg56, jlong arg57, jlong arg58, jlong arg59, jlong arg60, jlong arg61, jlong arg62, jlong arg63, jlong arg64, jlong arg65, jlong arg66, jlong arg67, jlong arg68, jlong arg69, jlong arg70, jlong arg71, jlong arg72, jlong arg73, jlong arg74, jlong arg75, jlong arg76, jlong arg77, jlong arg78, jlong arg79, jlong arg80, jlong arg81, jlong arg82, jlong arg83, jlong arg84, jlong arg85, jlong arg86, jlong arg87, jlong arg88, jlong arg89, jlong arg90, jlong arg91, jlong arg92, jlong arg93, jlong arg94, jlong arg95, jlong arg96, jlong arg97, jlong arg98, jlong arg99, jlong arg100, jlong arg101, jlong arg102, jlong arg103, jlong arg104, jlong arg105, jlong arg106, jlong arg107, jlong arg108, jlong arg109, jlong arg110, jlong arg111, jlong arg112, jlong arg113, jlong arg114, jlong arg115, jlong arg116, jlong arg117, jlong arg118, jlong arg119, jlong arg120, jlong arg121, jlong arg122, jlong arg123, jlong arg124, jlong arg125, jlong arg126, jlong arg127 );
jlong JNICALL Java_JniArgTests_nativeJJJJJJJJJJJJrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jlong arg2, jlong arg3, jlong arg4, jlong arg5, jlong arg6, jlong arg7, jlong arg8, jlong arg9, jlong arg10, jlong arg11, jlong arg12 );
jlong JNICALL Java_JniArgTests_nativeIJJJJJJJJJJJJrJ( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jlong arg3, jlong arg4, jlong arg5, jlong arg6, jlong arg7, jlong arg8, jlong arg9, jlong arg10, jlong arg11, jlong arg12, jlong arg13 );
jlong JNICALL Java_JniArgTests_nativeIJIJIJIJIJIJrJ( JNIEnv *p_env, jobject p_this, jint arg1, jlong arg2, jint arg3, jlong arg4, jint arg5, jlong arg6, jint arg7, jlong arg8, jint arg9, jlong arg10, jint arg11, jlong arg12 );
jlong JNICALL Java_JniArgTests_nativeJIJIJIJIJIJIrJ( JNIEnv *p_env, jobject p_this, jlong arg1, jint arg2, jlong arg3, jint arg4, jlong arg5, jint arg6, jlong arg7, jint arg8, jlong arg9, jint arg10, jlong arg11, jint arg12 );
jlong JNICALL Java_JniArgTests_nativeFFFFFFFFFFFFrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jfloat arg2, jfloat arg3, jfloat arg4, jfloat arg5, jfloat arg6, jfloat arg7, jfloat arg8, jfloat arg9, jfloat arg10, jfloat arg11, jfloat arg12 );
jlong JNICALL Java_JniArgTests_nativeDDDDDDDDDDDDrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jdouble arg2, jdouble arg3, jdouble arg4, jdouble arg5, jdouble arg6, jdouble arg7, jdouble arg8, jdouble arg9, jdouble arg10, jdouble arg11, jdouble arg12 );
jlong JNICALL Java_JniArgTests_nativeFDFDFDFDFDFDrJ( JNIEnv *p_env, jobject p_this, jfloat arg1, jdouble arg2, jfloat arg3, jdouble arg4, jfloat arg5, jdouble arg6, jfloat arg7, jdouble arg8, jfloat arg9, jdouble arg10, jfloat arg11, jdouble arg12 );
jlong JNICALL Java_JniArgTests_nativeDFDFDFDFDFDFrJ( JNIEnv *p_env, jobject p_this, jdouble arg1, jfloat arg2, jdouble arg3, jfloat arg4, jdouble arg5, jfloat arg6, jdouble arg7, jfloat arg8, jdouble arg9, jfloat arg10, jdouble arg11, jfloat arg12 );
jlong JNICALL Java_JniArgTests_nativeBBSSIJFDIFDFDFDBBSSIJFDrJ( JNIEnv *p_env, jobject p_this, jbyte arg1, jbyte arg2, jshort arg3, jshort arg4, jint arg5, jlong arg6, jfloat arg7, jdouble arg8, jint arg9, jfloat arg10, jdouble arg11, jfloat arg12, jdouble arg13, jfloat arg14, jdouble arg15, jbyte arg16, jbyte arg17, jshort arg18, jshort arg19, jint arg20, jlong arg21, jfloat arg22, jdouble arg23 );

jboolean JNICALL Java_JniArgTests_nativeIZrZ(JNIEnv *p_env, jobject p_this, jint value, jbooleanArray backChannel);
jboolean JNICALL Java_JniArgTests_nativeZIZIZIZIZIrZ(JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5, jint arg6, jint arg7, jint arg8, jint arg9, jint arg10);
jboolean JNICALL Java_JniArgTests_nativeIZIZIZIZIZrZ(JNIEnv *p_env, jobject p_this, jint arg1, jboolean arg2, jint arg3, jboolean arg4, jint arg5, jboolean arg6, jint arg7, jboolean arg8, jint arg9, jboolean arg10);
jboolean JNICALL Java_JniArgTests_nativeZZZZZZZZZZrZ(JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5, jint arg6, jint arg7, jint arg8, jint arg9, jint arg10);
jboolean JNICALL Java_JniArgTests_nativeIIIIIZZZZZrZ(JNIEnv *p_env, jobject p_this, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5, jboolean arg6, jboolean arg7, jboolean arg8, jboolean arg9, jboolean arg10);
