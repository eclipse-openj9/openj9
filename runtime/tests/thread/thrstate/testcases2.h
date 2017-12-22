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

#ifndef _TESTCASES2_H_INCLUDED
#define _TESTCASES2_H_INCLUDED

extern UDATA test_vBz_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vBfoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfoC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfOc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfVc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vBaoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaoC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* native thread is blocked on the same monitor */
extern UDATA test_vBaoc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaoC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNc_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNC_nB(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* native thread is blocked on a different monitor,
 * monitor is owned by an unattached thread
 */
extern UDATA test_vBaoc_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaoC_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOc_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVc_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVC_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNc_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNC_nB2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* native thread is blocked on a different monitor, 
 * monitor is owned by an attached thread
 */
extern UDATA test_vBaoc_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaoC_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOc_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVc_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVC_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNc_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNC_nB2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* native thread is waiting on the same monitor */
extern UDATA test_vBaoc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaoC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNc_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNC_nW(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* native thread is waiting on an unowned monitor */
extern UDATA test_vBaoc_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaoC_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOc_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVc_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVC_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNc_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNC_nW2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* native thread is waiting on a monitor owned by an attached thread */
extern UDATA test_vBaoc_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaoC_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOc_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVc_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaVC_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNc_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaNC_nW2_2(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

#endif /*_TESTCASES2_H_INCLUDED*/
