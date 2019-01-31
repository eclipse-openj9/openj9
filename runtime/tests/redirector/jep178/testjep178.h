/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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

/**
 * @file testjep178.h
 * @brief Provide common definitions for JEP178 tests. 
 */

#if !defined(_testjep178_h_)
#define _testjep178_h_ 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(WIN32)
#include <windows.h>
#elif defined(LINUX) || defined(AIXPPC) || defined(OSX)
#include <dlfcn.h>
#else
#include <dll.h>
#include "atoe.h"
#endif
#include "j9comp.h"
#include "jni.h"

#define J9JEP178_MAXIMUM_JVM_OPTIONS 10
#define J9PATH_LENGTH_MAXIMUM 1024
#define J9VM_OPTION_MAXIMUM_LENGTH 1024
#if defined(WIN32)
#define J9PATH_DIRECTORY_SEPARATOR '\\'
#define J9PATH_JVM_LIBRARY "jvm.dll"
#else /* defined(WIN32) */
#define J9PATH_DIRECTORY_SEPARATOR '/'
#if defined(OSX)
#define J9PATH_JVM_LIBRARY "libjvm.dylib"
#else /* defined(OSX) */
#define J9PATH_JVM_LIBRARY "libjvm.so"
#endif /* defined(OSX) */
#endif /* defined(WIN32) */

#endif /* !defined(_testjep178_h_) */
