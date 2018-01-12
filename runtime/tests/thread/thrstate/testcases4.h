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
#ifndef _TESTCASES4_H_INCLUDED
#define _TESTCASES4_H_INCLUDED

/* vmthread waiting */

/* native thread is runnable/waiting/blocked/notifed on the same monitor */
extern UDATA test_vWaoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vWaVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vWaOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaOC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaOC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaOC_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/*
 * native thread is blocked/waiting on a different raw monitor
 * e.g. attempting to reacquire vmaccess after the wait
 * e.g. or locking publicFlagsMutex to update flags
 */

extern UDATA test_vWaoc_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vWaVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaVC_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* vmthread timed waiting */

/* native thread is runnable/waiting/blocked/notifed on the same monitor */
extern UDATA test_vWTaoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vWTaVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vWTaOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaOC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaOC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaOC_nBN(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/*
 * native thread is blocked/waiting on a different raw monitor
 * e.g. attempting to reacquire vmaccess after the wait
 * e.g. or locking publicFlagsMutex to update flags
 */

extern UDATA test_vWTaoc_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vWTaVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaVC_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

#endif /*_TESTCASES4_H_INCLUDED*/
