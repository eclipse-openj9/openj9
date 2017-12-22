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

#ifndef _TESTCASES1_H_INCLUDED
#define _TESTCASES1_H_INCLUDED

extern UDATA test_vNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vBfVC_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVC_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWaoc_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vWTaoc_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMoc_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTm_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vS_nNull(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* started */
extern UDATA test_v0_nStm(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nStMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_v0_nStMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nStMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nStMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nStMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nStMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nStMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nStMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* started | dead */
extern UDATA test_v0_nDm(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nDMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nDMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nDMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nDMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nDMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nDMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nDMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nDMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* started | suspended */
extern UDATA test_v0_nZm(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* started | suspended | blocked */
/* the suspended flag is ignored */
extern UDATA test_v0_nZBm(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZBMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZBMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZBMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZBMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZBMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nZBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* started | blocked */
extern UDATA test_v0_nBm(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* started | waiting */
extern UDATA test_v0_nWm(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nWMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nWMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nWMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nWMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nWMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* native thread blocked/waiting on an object monitor */
extern UDATA test_v0_nBMVC_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nWMVC_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* started | blocked | notified */
extern UDATA test_v0_nBNm(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBNMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBNMoC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBNMOc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBNMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBNMNc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBNMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBNMVc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_v0_nBNMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* started | sleeping */
extern UDATA test_v0_nS(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
/* started | sleeping | interruptible */
extern UDATA test_v0_nSI(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
/* started | sleeping | interruptible | interrupted */
extern UDATA test_v0_nSII(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

#endif /*_TESTCASES1_H_INCLUDED*/
