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

#ifndef TEST_HELPERS_H_INCLUDED
#define TEST_HELPERS_H_INCLUDED

/*
 * $RCSfile: testHelpers.h,v $
 * $Revision: 1.52 $
 * $Date: 2013-03-21 14:42:10 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Port library testing
 * 
 * Definitions, types and functions common to port library test suites.
 * @todo Ripped Sunny's stuff out, doesn't belong here
 * @todo delete HEADING from main.c
 */
#include "j9cfg.h"
#include "j9comp.h"
#include "j9port.h"

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Test case status
 * @{
 */
#define TEST_PASS 0
#define TEST_FAIL 1
/** @} */

#define EXIT_OUT_OF_MEMORY 37
 
#define BOUNDCPUS "-boundcpus:"

/**
 * Save some typing, when displaying an error message need the 
 * portlibrary, file name, line number and testname used
 */
#define PORTTEST_ERROR_ARGS portLibrary, __FILE__, __LINE__, testName

/*
 * Forward declarations, not under doxygen control
 */
void operationNotSupported(J9PortLibrary *portLibrary, const char *operationName);
void reportTestEntry(J9PortLibrary *portLibrary, const char *testName);
int reportTestExit(J9PortLibrary *portLibrary, const char *testName);
void outputComment(J9PortLibrary *portLibrary, const char *format, ...);
void outputErrorMessage(J9PortLibrary *portLibrary, const char *fileName, I_32 lineNumber, const char* testName, const char *format, ...);
void HEADING(J9PortLibrary *portLibrary, const char *string);
UDATA verifyFileExists(struct J9PortLibrary *portLibrary, const char *pltestFileName, I_32 lineNumber, const char *testName, const char *fileName);
void dumpTestFailuresToConsole(struct J9PortLibrary *portLibrary);
void deleteControlDirectory(struct J9PortLibrary *portLibrary, char* baseDir);
void getPortLibraryMemoryCategoryData(struct J9PortLibrary *portLibrary, UDATA * blocks, UDATA * bytes);
int startsWith(char *s, char *prefix);
UDATA raiseSEGV(J9PortLibrary* portLibrary, void* arg);

#ifdef __cplusplus
}
#endif

#endif /* TEST_HELPERS_H_INCLUDED */
