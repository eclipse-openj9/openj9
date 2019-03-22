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
#define J2SE_18   0x08
#define J2SE_V11  0x0B            /* This refers Java 11 */
#define J2SE_V12  0x0C            /* This refers Java 12 */
#define J2SE_V13  0x0D            /* This refers Java 13 */

/**
 * Constant and Macro checking if JVM is in subdir
 */
#define J2SE_LAYOUT_VM_IN_SUBDIR 0x01
#define J2SE_IS_LAYOUT_VM_IN_SUBDIR(javaVM)        (J2SE_LAYOUT_VM_IN_SUBDIR == ((javaVM)->j2seVersion & J2SE_LAYOUT_VM_IN_SUBDIR))

#endif /* j2sever_h */
