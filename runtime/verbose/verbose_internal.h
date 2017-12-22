/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef verbose_internal_h
#define verbose_internal_h

/**
* @file verbose_internal.h
* @brief Internal prototypes used within the VERBOSE module.
*
* This file contains implementation-private function prototypes and
* type definitions for the VERBOSE module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "verbose_api.h"
#include "jni.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- verbose.c ---------------- */

#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)) 
/**
* @brief
* @param *dynamicLoadBuffers
* @return void
*/
void hookDynamicLoadReporting(J9TranslationBufferSet *dynamicLoadBuffers);
#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */

#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT)) 
/**
 * Reports stats related to dynamic loading of class
 * @param javaVM - pointer to J9JavaVM
 * @param loader - class loader
 * @param romClass - the class for which stats are displayed
 * @param [in/out] localBuffer - contains values for entryIndex, loadLocationType and cpEntryUsed. This pointer can't be NULL.
 * @return void
 */
void
reportDynloadStatistics(struct J9JavaVM *javaVM, struct J9ClassLoader *loader, struct J9ROMClass *romClass, struct J9TranslationLocalBuffer *localBuffer);

#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */


/* ---------------- errormessageframeworkcfr.c ---------------- */

/**
 * Generate the detailed error messages in the case of static verification
 * @param javaVM - pointer to J9JavaVM
 * @param error - pointer to J9CfrError
 * @param className - pointer to the class name
 * @param classNameLength - the length of class name
 * @param initMsgBuffer - initial message buffer that holds the verification error messages
 * @param msgBufferLength - the length of the error message buffer
 * @return pointer to the generated error messages on success; otherwise, return NULL.
 */
U_8*
generateJ9CfrExceptionDetails(J9JavaVM *javaVM, J9CfrError *error, U_8* className, UDATA classNameLength, U_8* initMsgBuffer, UDATA* msgBufferLength);


/* ---------------- errormessageframeworkrtv.c ---------------- */

/**
 * Generate the detailed error messages in the case of runtime verification
 * @param error - pointer to J9BytecodeVerificationData
 * @param initMsgBuffer - initial buffer to hold the verification error messages
 * @param msgBufferLength - the length of the error message buffer
 * @return pointer to the generated error messages on success; otherwise, return NULL.
 */
U_8*
generateJ9RtvExceptionDetails(J9BytecodeVerificationData* error, U_8* initMsgBuffer, UDATA* msgBufferLength);

#ifdef __cplusplus
}
#endif

#endif /* verbose_internal_h */
