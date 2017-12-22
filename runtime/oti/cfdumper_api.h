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

#ifndef cfdumper_api_h
#define cfdumper_api_h

/**
* @file cfdumper_api.h
* @brief Public API for the CFDUMPER module.
*
* This file contains public function prototypes and
* type definitions for the CFDUMPER module.
*
*/

#include "j9.h"
#include "j9comp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- main.c ---------------- */

/**
* @brief
* @param *arg
* @return UDATA
*/
UDATA 
signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg);

/* ---------------- romdump.c ---------------- */

/**
* @brief
* @param portLib
* @param romClass
* @param baseAddress
* @param nestingThreshold
* @param validateRangeCallback
* @return void
*/
void
j9bcutil_linearDumpROMClass(J9PortLibrary *portlib, J9ROMClass *romClass, void *baseAddress, UDATA nestingThreshold,
		BOOLEAN(*validateRangeCallback)(J9ROMClass*, void*, UDATA, void*));

/**
* @brief
* @param portLib
* @param romClass
* @param baseAddress
* @param queries
* @param numQueries
* @param validateRangeCallback
* @return void
*/
void
j9bcutil_queryROMClass(J9PortLibrary *portLib, J9ROMClass *romClass, void *baseAddress, const char **queries, UDATA numQueries,
		BOOLEAN(*validateRangeCallback)(J9ROMClass*, void*, UDATA, void*));

/**
* @brief
* @param portLib
* @param romClass
* @param baseAddress
* @param commaSeparatedQueries
* @param validateRangeCallback
* @return void
*/
void
j9bcutil_queryROMClassCommaSeparated(J9PortLibrary *portLib, J9ROMClass *romClass, void *baseAddress, const char *commaSeparatedQueries,
		BOOLEAN(*validateRangeCallback)(J9ROMClass*, void*, UDATA, void*));

/**
* @brief
* @param portLib
* @param romClass
* @param validateRangeCallback
* @return void
*/
void
j9bcutil_linearDumpROMClassXML(J9PortLibrary *portlib, J9ROMClass *romClass,
		BOOLEAN(*validateRangeCallback)(J9ROMClass*, void*, UDATA, void*));

#ifdef __cplusplus
}
#endif

#endif /* cfdumper_api_h */

