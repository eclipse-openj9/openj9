/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "VerboseGCInterface.h"

#include "GCExtensions.hpp"

extern "C" {
/**
 * Return the verbose gc API function table.
 * This routine should return a table whose functions do nothing (default behavior).
 * @return the verbose GC API function table.
 */
void *
getVerboseGCFunctionTable(J9JavaVM *javaVM)
{
	void *result = NULL;
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	if (NULL != extensions) {
		result = &extensions->verboseFunctionTable;
	}
	return result;	
}

/**
 * Dummy function for verbose function table.
 * @param filename The name of the file or output stream to log to.
 * @param numFiles The number of files to log to.
 * @param numCycles The number of gc cycles to log to each file.
 * @return 0
 */
UDATA
dummygcDebugVerboseStartupLogging(J9JavaVM *javaVM, char* filename, UDATA numFiles, UDATA numCycles)
{
	return 0;
}

/**
 * Dummy function for verbose function table.
 * @param releaseVerboseStructures Indicator of whether it is safe to free the infrastructure.
 */
void
dummygcDebugVerboseShutdownLogging(J9JavaVM *javaVM, UDATA releaseVerboseStructures)
{
}

UDATA
dummyconfigureVerbosegc(J9JavaVM *javaVM, int enable, char* filename, UDATA numFiles, UDATA numCycles)
{
	/* The verbose DLL isn't loaded, so call back into the VM to load it. This will
	 * reconfigure the function table to point at the functions in the verbose DLL. */
	if(JNI_OK != javaVM->internalVMFunctions->postInitLoadJ9DLL(javaVM, J9_VERBOSE_DLL_NAME, NULL)) {
		/* we failed to load the library */
		return 0;
	}

	/* Now that the function table has been reconfigured, we can call the right function to
	 * start/stop verbosegc. */	
	J9MemoryManagerVerboseInterface *vrbFuncTable = (J9MemoryManagerVerboseInterface *)javaVM->memoryManagerFunctions->getVerboseGCFunctionTable(javaVM);
	return vrbFuncTable->configureVerbosegc(javaVM, enable, filename, numFiles, numCycles);
}

UDATA
dummyQueryVerbosegc(J9JavaVM *javaVM)
{
	return 0;
}

/**
 * Dummy function for verbose function table.
 */
void
dummygcDumpMemorySizes(J9JavaVM *javaVM)
{
}

/**
 * Initialises the verbose function table with the dummy routines.
 * @param table Pointer to the Verbose function table.
 */
void
initializeVerboseFunctionTableWithDummies(J9MemoryManagerVerboseInterface *table)
{
	table->gcDebugVerboseStartupLogging = dummygcDebugVerboseStartupLogging;
	table->gcDebugVerboseShutdownLogging = dummygcDebugVerboseShutdownLogging;
	table->gcDumpMemorySizes = dummygcDumpMemorySizes;
	table->configureVerbosegc = dummyconfigureVerbosegc;
	table->queryVerbosegc = dummyQueryVerbosegc;
}

} /* extern "C" */

