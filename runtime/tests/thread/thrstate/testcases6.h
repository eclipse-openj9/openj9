/*******************************************************************************
 * Copyright (c) 2008, 2008 IBM Corp. and others
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
#ifndef _TESTCASES6_H_INCLUDED
#define _TESTCASES6_H_INCLUDED

/*
 * sleeping states 
 */
extern UDATA test_vS_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nWTMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nWTMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nWTMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nST(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

#if 0
/* not possible */
extern UDATA test_vS_nP(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nS(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
#endif

/*
 * suspended
 */
extern UDATA test_vZ_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vZ_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vZ_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vZ_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

#endif /*_TESTCASES6_H_INCLUDED*/
