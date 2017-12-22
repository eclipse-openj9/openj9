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

#ifndef callconv_api_h
#define callconv_api_h

/**
* @file callconv_api.h
* @brief Public API for the CALLCONV module.
*
* This file contains public function prototypes and
* type definitions for the CALLCONV module.
*
*/

#include "j9.h"
#include "j9comp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- callroot.c ---------------- */

/**
* @brief
* @param void
* @return void
*/
void
testAllBlocks(void);


/* ---------------- callconvmain.c ---------------- */

/**
* @brief
* @param void
* @return void
*/
void 
asmCallFailure(void);


/**
* @brief
* @param *detail
* @return void
*/
void 
asmFailure(char *detail);


/**
* @brief
* @param void
* @return void
*/
void 
callFailure(void);


/**
* @brief
* @param void
* @return void
*/
void 
callOutFailure(void);


/**
* @brief
* @param *detailedName
* @param index
* @return void
*/
void 
cFailure(char *detailedName, int index);


/**
* @brief
* @param void
* @return void
*/
void 
dumpSummary(void);


/**
* @brief
* @param *detailedName
* @return void
*/
void 
returnFailure(char *detailedName);


/**
* @brief
* @param *testname
* @return void
*/
void 
setAsmTestName(char *testname);


/**
* @brief
* @param *testname
* @return I_32
*/
I_32 
shouldRun(char *testname);


/**
* @brief
* @param d1
* @param d2
* @param d3
* @param d4
* @param d5
* @param d6
* @param d7
* @param d8
* @param d9
* @param d10
* @return IDATA
*/
IDATA 
sorbet(double d1, double d2, double d3, double d4, double d5, double d6, double d7, double d8, double d9, double d10);


/**
* @brief
* @param count
* @param ...
* @return void
*/
void 
testStackAlignment(I_32 count, ...);


#ifdef __cplusplus
}
#endif

#endif /* callconv_api_h */

