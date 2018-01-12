/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#include "j9comp.h"
#include "j9.h"

#include "testHelpers.h"
#include "cfr.h"
#include "bcutil_api.h"
#include "stackmap_api.h"

static IDATA
testClass(J9PortLibrary *portLib, const char *classFileName, UDATA testMode)
{
	PORT_ACCESS_FROM_PORT(portLib);

	BOOLEAN testShared = testMode == 1;
	const char * testName = classFileName;
	U_8 *classFileBytes = NULL;
	U_32 classFileSize = 0;
	U_32 bytesRead = 0;
	/* TODO: create some verification buffers */
	struct J9BytecodeVerificationData * verifyBuffers = NULL;


	/* TODO: setup and test flags */
	/* flags must contain VM version */
	U_32 flags = BCT_JavaMaxMajorVersionShifted;

	/* scratch space for ROMClass creation */
	UDATA romClassBufferSize = 64 * 1024;
	U_8 *romClassBuffer = j9mem_allocate_memory(romClassBufferSize, J9MEM_CATEGORY_CLASSES);
	UDATA lineNumberBufferSize = 0;
	U_8 *lineNumberBuffer = NULL;
	UDATA varInfoBufferSize = 0;
	U_8 *varInfoBuffer = NULL;
	IDATA fd = 0;
	IDATA rc = 0;

	if (2 == testMode) {
		lineNumberBufferSize = 64 * 1024;
		lineNumberBuffer = j9mem_allocate_memory(lineNumberBufferSize, J9MEM_CATEGORY_CLASSES);
		varInfoBufferSize = 64 * 1024;
		varInfoBuffer = j9mem_allocate_memory(varInfoBufferSize, J9MEM_CATEGORY_CLASSES);
	}

	reportTestEntry(PORTLIB, classFileName);

	fd = j9file_open(classFileName, EsOpenRead, 0);
	classFileSize = (U_32)j9file_seek(fd, 0, EsSeekEnd);
	j9file_seek(fd, 0, EsSeekSet);
	classFileBytes = j9mem_allocate_memory(classFileSize, J9MEM_CATEGORY_CLASSES);
	bytesRead = (U_32)j9file_read(fd, classFileBytes, classFileSize);
	if (bytesRead != classFileSize) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to read classfile: %s \n", classFileName );
		goto _exit_test;
	}
	rc = j9bcutil_buildRomClassIntoBuffer(classFileBytes, classFileSize, PORTLIB, verifyBuffers, flags, 0, 0, romClassBuffer, romClassBufferSize, lineNumberBuffer, lineNumberBufferSize, varInfoBuffer, varInfoBufferSize, NULL);

	if ( BCT_ERR_NO_ERROR != rc ){
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to create ROMClass for class: %s \n", classFileName );
		goto _exit_test;
	}

	if (testShared) {
		rc = fixReturnBytecodes(PORTLIB, (J9ROMClass *)romClassBuffer);
		if ( BCT_ERR_NO_ERROR != rc ){
			outputErrorMessage(TEST_ERROR_ARGS, "Failed to fix return bytecodes for class: %s \n", classFileName );
			goto _exit_test;
		 }
	}


	rc = j9bcutil_compareRomClass(classFileBytes, classFileSize,
			PORTLIB, verifyBuffers, flags, 0, (J9ROMClass *)romClassBuffer);
	if ( BCT_ERR_NO_ERROR != rc ) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to match ROMClass for class: %s \n", classFileName );
	}

	_exit_test:
	return reportTestExit(PORTLIB, classFileName);
}

IDATA
j9dyn_testROMClassCompare(J9PortLibrary *portLib)
{
	PORT_ACCESS_FROM_PORT(portLib);
	IDATA rc = 0;
	UDATA test = 0;

	static const char *headings[] = {
			"j9dyn_testROMClassCompare",
			"j9dyn_testROMClassCompare (shared)",
			"j9dyn_testROMClassCompare (out of line debug)",
	};

	for ( test = 0; test < (sizeof(headings)/sizeof(char*)); test ++ ) {
		HEADING(PORTLIB, headings[test]);
		rc |= testClass(PORTLIB, "VM.class", test);
		rc |= testClass(PORTLIB, "PhantomReference.class", test);
		rc |= testClass(PORTLIB, "AbstractClassLoader$3.class", test);
		rc |= testClass(PORTLIB, "AttachHandler$1.class", test);
		rc |= testClass(PORTLIB, "ZipEntry.class", test);
		rc |= testClass(PORTLIB, "ZipStream.class", test);
		rc |= testClass(PORTLIB, "Example.class", test);

	}

	return rc;
}

