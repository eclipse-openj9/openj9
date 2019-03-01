/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#ifndef j2sever_h
#define j2sever_h

#include "j9cfg.h" /* for JAVA_SPEC_VERSION */

/**
 * Constants for supported J2SE versions.
 */
#define J2SE_18   0x0800
#define J2SE_V11  0x0B00            /* This refers Java 11 */
#define J2SE_V12  0x0C00            /* This refers Java 12 */
#define J2SE_V13  0x0D00            /* This refers Java 13 */
/* Shared class cache is using JAVA_SPEC_VERSION_FROM_J2SE(j2seVersion) to get the Java version.
 * So bits 9 to 16 of the J2SE constant should match the java version number.
 */

/**
 * Masks for extracting major and full versions.
 */
#define J2SE_VERSION_MASK 0xFF00
#define J2SE_RELEASE_MASK 0xFFF0
#define J2SE_SERVICE_RELEASE_MASK 0xFFFF

#define J2SE_JAVA_SPEC_VERSION_SHIFT 8
/* J2SE_CURRENT_VERSION is the current Java version supported by VM for a JCL level. */
#define J2SE_CURRENT_VERSION (JAVA_SPEC_VERSION << J2SE_JAVA_SPEC_VERSION_SHIFT)

/**
 * Masks and constants for describing J2SE shapes.
 */
#define J2SE_LAYOUT_VM_IN_SUBDIR 0x100000
#define J2SE_LAYOUT_MASK 		0xF00000
#define J2SE_LAYOUT(javaVM) 	((javaVM)->j2seVersion & J2SE_LAYOUT_MASK)

/**
 * Macro to extract the J2SE version given a J9JavaVM.
 */
#define J2SE_VERSION(javaVM) 	((javaVM)->j2seVersion & J2SE_SERVICE_RELEASE_MASK)

/**
 * Macro to extract J2SE version given a JNIEnv.
 */
#define J2SE_VERSION_FROM_ENV(env) J2SE_VERSION(((J9VMThread*)env)->javaVM)

/**
 * Macro to extract java spec version from J2SE version.
 */
#define JAVA_SPEC_VERSION_FROM_J2SE(j2seVersion) 	((j2seVersion) >> J2SE_JAVA_SPEC_VERSION_SHIFT)

#endif /* j2sever_h */
