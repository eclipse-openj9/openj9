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
#ifndef _TESTCASES3_H_INCLUDED
#define _TESTCASES3_H_INCLUDED

/*
 * vmthread blocked, owns blocking object;
 * native thread blocked on a different raw monitor
 * e.g. attempting to reacquire vmaccess after acquiring an object monitor. 
 */
extern UDATA test_vBfOC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfOC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfOC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfOC_nBMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vBaOC_nBMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nBMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/*
 * vmthread blocked, owns blocking object; 
 * native thread waiting on a different raw monitor
 * e.g. attempting to reacquire vmaccess after acquiring an object monitor. 
 */
extern UDATA test_vBfOC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfOC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfOC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfOC_nWMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vBaOC_nWMoc(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBaOC_nWMNC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

/* 
 * vmthread's blocking object is flatlocked by another vmthread;
 * native thread owns the object monitor, or is waiting on it
 * e.g. blocked spinning on a flat object monitor
 */
extern UDATA test_vBfVC_nStMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfVC_nWMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfVC_nBMOC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

extern UDATA test_vBfVC_nStMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfVC_nWMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);
extern UDATA test_vBfVC_nBMVC(JNIEnv *env, BOOLEAN ignoreExpectedFailures);

#endif /*_TESTCASES3_H_INCLUDED*/
