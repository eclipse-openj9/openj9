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

#ifndef algorithm_test_internal_h
#define algorithm_test_internal_h

/**
* @file algorithm_test_internal.h
* @brief Internal prototypes used within the ALGORITHM_TEST module.
*
* This file contains implementation-private function prototypes and
* type definitions for the ALGORITHM_TEST module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "algorithm_test_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- algotest.c ---------------- */

struct j9cmdlineOptions;
/**
* @brief
* @param arg
* @return UDATA
*/
UDATA 
signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg);

/* ---------------- argscantest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *passCount
* @param *failCount
* @return I_32
*/
extern J9_CFUNC I_32 
verifyArgScan (J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);


/* ---------------- simplepooltest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *passCount
* @param *failCount
* @return I_32
*/
I_32
verifySimplePools(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);


/* ---------------- wildcardtest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *passCount
* @param *failCount
* @return I_32
*/
I_32 
verifyWildcards (J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);

/* ---------------- sendslottest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *passCount
* @param *failCount
* @return I_32
*/
I_32 
verifySendSlots(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);

/* ---------------- srphashtabletest.c ---------------- */

/**
* @brief
* @param *portLib
* @param *passCount
* @param *failCount
* @return I_32
*/
I_32
verifySRPHashtable(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);

/**
 * @brief
* @param *portLib
* @param *passCount
* @param *failCount
* @return I_32
 */
I_32
verifyPrimeNumberHelper(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);

#ifdef __cplusplus
}
#endif

#endif /* algorithm_test_internal_h */

