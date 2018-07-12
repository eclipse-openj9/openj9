/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
* @file classloadersearch.h
* @brief Public API for the adding paths to the boot classpath property or jars to the system classloader
*
*/

#ifndef _classloadersearch_h
#define _classloadersearch_h

#include "j9.h"
#include "j9comp.h"
#include "jvmti.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bits dictating where the specified classpath should be added to */

#define CLS_TYPE_ADD_TO_SYSTEM_PROPERTY		1		/* Add the specified classpath to the bootstrap classloader */
#define CLS_TYPE_ADD_TO_SYSTEM_CLASSLOADER	2		/* Add the specified classpath to the system classloader */

#define CLS_ERROR_NONE						JVMTI_ERROR_NONE
#define CLS_ERROR_NULL_POINTER				JVMTI_ERROR_NULL_POINTER
#define CLS_ERROR_OUT_OF_MEMORY				JVMTI_ERROR_OUT_OF_MEMORY
#define CLS_ERROR_INTERNAL 					JVMTI_ERROR_INTERNAL
#define CLS_ERROR_CLASS_LOADER_UNSUPPORTED	JVMTI_ERROR_CLASS_LOADER_UNSUPPORTED
#define CLS_ERROR_ILLEGAL_ARGUMENT			JVMTI_ERROR_ILLEGAL_ARGUMENT
#define CLS_ERROR_NOT_FOUND					JVMTI_ERROR_NOT_FOUND

#ifdef __cplusplus
}
#endif

#endif
