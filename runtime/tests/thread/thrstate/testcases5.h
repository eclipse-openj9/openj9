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
#ifndef _TESTCASES5_H_INCLUDED
#define _TESTCASES5_H_INCLUDED

/* parked states */
/*
 * We can't test ownable synchronizers properly without an SE class library.
 * The parkBlocker is an arbitrary object.
 * Ownable synchronizers are tested by the ThreadMXBean6 test suite.
 */

/* parked with no park blocker */
extern UDATA test_vPm_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPm_nP(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPm_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPm_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPm_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPm_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPm_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPm_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* these suspended states are pretty unusual */
extern UDATA test_vPm_nZP(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPm_nZBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPm_nZWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vPTm_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTm_nPT(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTm_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTm_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTm_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTm_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTm_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTm_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* parked on an unowned object */
extern UDATA test_vPMoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMoc_nP(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMoc_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMoc_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMoc_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMoc_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMoc_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMoc_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vPTMoc_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMoc_nPT(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMoc_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMoc_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMoc_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMoc_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMoc_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMoc_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* parked on an object owned by self */
extern UDATA test_vPMOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMOC_nP(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMOC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMOC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMOC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMOC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMOC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMOC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vPTMOC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMOC_nPT(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMOC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMOC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMOC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMOC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMOC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMOC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* parked on an object owned by another thread */
extern UDATA test_vPMVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMVC_nP(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMVC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMVC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPMVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vPTMVC_nSt(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMVC_nPT(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMVC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMVC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vPTMVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

#endif /*_TESTCASES5_H_INCLUDED*/
