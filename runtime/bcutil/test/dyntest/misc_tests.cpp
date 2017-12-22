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
#include "../../ConstantPoolMap.hpp"
#include "bcutil_api.h"
#include "rommeth.h"

#define GET_BE_U16(x) ( (((U_16) x[0])<< 8) | ((U_16) x[1]) )


static IDATA
testGetNextStackMapFrame(J9PortLibrary *portLib, const char *classFileName)
{
	PORT_ACCESS_FROM_PORT(portLib);

	/*
		 * create class with/without stack map
		 *
		 * get the stack map
		 *
		 * if stack map walk the frames
		 *
		 * ensure that we are at the end (or within padding of)
		 */

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
	U_8 *romClassBuffer = (U_8*)j9mem_allocate_memory(romClassBufferSize, J9MEM_CATEGORY_CLASSES);
	IDATA fd = 0;
	IDATA rc = 0;

	reportTestEntry(PORTLIB, classFileName);

	fd = j9file_open(classFileName, EsOpenRead, 0);
	classFileSize = (U_32)j9file_seek(fd, 0, EsSeekEnd);
	j9file_seek(fd, 0, EsSeekSet);
	classFileBytes = (U_8*)j9mem_allocate_memory(classFileSize, J9MEM_CATEGORY_CLASSES);
	bytesRead = (U_32)j9file_read(fd, classFileBytes, classFileSize);
	if (bytesRead != classFileSize) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to read classfile: %s \n", classFileName );
		return reportTestExit(PORTLIB, classFileName);
	}
	rc = j9bcutil_buildRomClassIntoBuffer(classFileBytes, classFileSize, PORTLIB, verifyBuffers, flags, 0, 0, romClassBuffer, romClassBufferSize, NULL, 0, NULL, 0, NULL);
	if ( BCT_ERR_NO_ERROR != rc ) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to create ROMClass for class: %s \n", classFileName );
		return reportTestExit(PORTLIB, classFileName);
	}

	J9ROMClass *romClass = (J9ROMClass *)romClassBuffer;
	J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
	UDATA romMethodCount = 0;

	while (romMethodCount < romClass->romMethodCount) {
		/* get stack map */
		U_32 *stackMap = getStackMapInfoForROMMethod(romMethod);

		/* if stack map exists, walk frames */
		if (NULL != stackMap) {
			UDATA *frame = NULL;
			UDATA *prevFrame = NULL;
			U_16 stackFrameIndex = 0;
			U_16 stackFrameCount = GET_BE_U16((((U_8*)stackMap)+sizeof(U_32)));;
			/* get the first stack frame */
			frame = getNextStackMapFrame(stackMap, frame);
			while(stackFrameIndex < stackFrameCount) {
				prevFrame = frame;
				frame = getNextStackMapFrame(stackMap, frame);
				stackFrameIndex ++;
			}
			U_32 stackMapSize = *stackMap;
			/* validate that last frame is within the size of the stack map */
			if ( UDATA(stackMap)+UDATA(stackMapSize) < UDATA(prevFrame) ) {
				outputErrorMessage(TEST_ERROR_ARGS, "Bad Stack Map present in ROMClass for class: %s \n", classFileName );
				return reportTestExit(PORTLIB, classFileName);
			}
		}
		romMethod = J9_NEXT_ROM_METHOD(romMethod);
		romMethodCount ++;
	}

	return reportTestExit(PORTLIB, classFileName);
}




extern "C" {
IDATA
j9dyn_testMisc(J9PortLibrary *portLib)
{
	PORT_ACCESS_FROM_PORT(portLib);
	IDATA rc = 0;

	HEADING(PORTLIB, "j9dyn_testMisc");
	rc |= testGetNextStackMapFrame(PORTLIB, "VM.class");
	rc |= testGetNextStackMapFrame(PORTLIB, "PhantomReference.class");
	rc |= testGetNextStackMapFrame(PORTLIB, "AbstractClassLoader$3.class");
	rc |= testGetNextStackMapFrame(PORTLIB, "AttachHandler$1.class");
	rc |= testGetNextStackMapFrame(PORTLIB, "ZipEntry.class");
	rc |= testGetNextStackMapFrame(PORTLIB, "ZipStream.class");
	return rc;
}
}
