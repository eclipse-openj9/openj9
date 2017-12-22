/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#ifndef dbgext_api_h
#define dbgext_api_h

/**
* @file dbgext_api.h
* @brief Public API for the DBGEXT module.
*
* This file contains public function prototypes and
* type definitions for the DBGEXT module.
*
*/

#include "j9.h"
#include "j9comp.h"
/*#include "j9generated.h"*/

#ifdef __cplusplus
extern "C" {
#endif


/* ---------------- trdbgext.c ---------------- */

/**
* @brief
* @param *args
* @return void
*/
void 
dbgext_trprint(const char *args);


/* -- Note:  the methods below are not implemented by the dbgext module but are required to be implemented by modules
 * which want to link in dbgext so they are listed here as they form part of the public API.
 * These functions should probably be moved out to a different header file to specify the parts of the API which are
 * required but not implemented by the module since mixing them both in this one header makes it mean too many things.
 */

/**
* @brief
* @param pattern
* @param patternLength
* @param patternAlignment
* @param startSearchFrom
* @param bytesSearched
* @return void*
*/
void*
dbgFindPattern(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA* bytesSearched);


/**
* @brief
* @param args
* @return UDATA
*/
UDATA 
dbgGetExpression (const char* args);


/**
* @brief
* @param address
* @param *structure
* @param size
* @param *bytesRead
* @return void
*/
void 
dbgReadMemory (UDATA address, void *structure, UDATA size, UDATA *bytesRead);


/**
* @brief
* @param message
* @return void
*/
void 
dbgWriteString (const char* message);

/* /--Note end */

#ifdef __cplusplus
}
#endif

#endif /* dbgext_api_h */

