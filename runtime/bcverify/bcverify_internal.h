/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#ifndef bcverify_internal_h
#define bcverify_internal_h

/**
* @file bcverify_internal.h
* @brief Internal prototypes used within the BCVERIFY module.
*
* This file contains implementation-private function prototypes and
* type definitions for the BCVERIFY module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "bcverify_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Store verification failure info to the J9BytecodeVerificationData
 * structure for outputting detailed error message.
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param errorDetailCode - error code defined in detailed message
 * @param errorCurrentFramePosition - the location of data type in the current frame when error occurs
 * @param errorTargetType - excepted data type when error occurs
 * @param errorTempData - extra data used for non-compatible type issue
 * @param currentPC - current pc value
 */
void
storeVerifyErrorData (J9BytecodeVerificationData * verifyData, I_16 errorDetailCode, U_32 errorCurrentFramePosition, UDATA errorTargetType, UDATA errorTempData, IDATA currentPC);

#ifdef __cplusplus
}
#endif

#endif /* bcverify_internal_h */

