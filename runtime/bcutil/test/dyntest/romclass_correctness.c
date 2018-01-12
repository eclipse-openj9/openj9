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

typedef struct RCcorrectenessMethodConfig {
	UDATA testFlagON;
	UDATA testFlagOFF;
} RCcorrectenessMethodConfig;

typedef struct RCCorrectnessConfig {
	const char *classFileName;
	UDATA testFlagON;
	UDATA testFlagOFF;
	UDATA findClassFlags;
	UDATA numMethod;
	RCcorrectenessMethodConfig *methodInfo;
} RCCorrectnessConfig;

#define TEST_ALL_MASK                   ((UDATA)0xFFFFFFFF)
#define TEST_NONE_MASK                  ((UDATA)0x00000000)
/* class flags */
#define TEST_J9AccClassHasFinalFields   ((UDATA)0x00000001)
#define TEST_J9AccClassCloneable        ((UDATA)0x00000002)
#define TEST_J9AccClassReferenceWeak    ((UDATA)0x00000004)
#define TEST_J9AccClassReferenceSoft    ((UDATA)0x00000008)
#define TEST_J9AccClassReferencePhantom ((UDATA)0x00000010)
#define TEST_J9AccClassHasEmptyFinalize ((UDATA)0x00000020)
#define TEST_J9AccClassFinalizeNeeded   ((UDATA)0x00000040)
#define TEST_J9AccClassHasVerifyData    ((UDATA)0x00000080)
#define TEST_J9AccClassUnsafe           ((UDATA)0x00000100)
/* method flags */
#define TEST_J9AccMethodHasBackwardBranches    ((UDATA)0x00000200)
#define TEST_J9AccSynthetic                    ((UDATA)0x00000800)
#define TEST_J9AccMethodHasGenericSignature    ((UDATA)0x00001000)
#define TEST_J9AccMethodHasExceptionInfo       ((UDATA)0x00002000)
#define TEST_J9AccEmptyMethod                  ((UDATA)0x00008000)
#define TEST_J9AccForwarderMethod              ((UDATA)0x00010000)
#define TEST_J9AccGetterMethod                 ((UDATA)0x00020000)
#define TEST_J9AccMethodVTable                 ((UDATA)0x00040000)


static void
verifyClassFlags(J9PortLibrary *portLib, const char *testName, J9ROMClass *romClass, RCCorrectnessConfig *testData )
{
	PORT_ACCESS_FROM_PORT(portLib);

	if ( TEST_J9AccClassHasFinalFields & testData->testFlagON ) {
		if (0 == J9ROMCLASS_HAS_FINAL_FIELDS(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccClassHasFinalFields set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassCloneable & testData->testFlagON ) {
		if (0 == J9ROMCLASS_IS_CLONEABLE(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccClassCloneable set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassReferenceWeak & testData->testFlagON ) {
		if (0 == J9ROMCLASS_REFERENCE_WEAK(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccClassReferenceWeak set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassReferenceSoft & testData->testFlagON ) {
		if (0 == J9ROMCLASS_REFERENCE_SOFT(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccClassReferenceSoft set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassReferencePhantom & testData->testFlagON ) {
		if (0 == J9ROMCLASS_REFERENCE_PHANTOM(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccClassReferencePhantom set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassHasEmptyFinalize & testData->testFlagON ) {
		if (0 == J9ROMCLASS_HAS_EMPTY_FINALIZE(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccClassHasEmptyFinalize set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassFinalizeNeeded & testData->testFlagON ) {
		if (0 == J9ROMCLASS_FINALIZE_NEEDED(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccClassFinalizeNeeded set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassHasVerifyData & testData->testFlagON ) {
		if (0 == J9ROMCLASS_HAS_VERIFY_DATA(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccClassHasVerifyData set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassUnsafe & testData->testFlagON ) {
		if (0 == J9ROMCLASS_IS_UNSAFE(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccClassUnsafe set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccSynthetic & testData->testFlagON ) {
		if (0 == J9ROMCLASS_IS_SYNTHETIC(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s does not have J9AccSynthetic set\n", testData->classFileName );
		}
	}

	if ( TEST_J9AccClassHasFinalFields & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_HAS_FINAL_FIELDS(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccClassHasFinalFields set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassCloneable & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_IS_CLONEABLE(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccClassCloneable set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassReferenceWeak & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_REFERENCE_WEAK(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccClassReferenceWeak set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassReferenceSoft & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_REFERENCE_SOFT(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccClassReferenceSoft set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassReferencePhantom & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_REFERENCE_PHANTOM(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccClassReferencePhantom set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassHasEmptyFinalize & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_HAS_EMPTY_FINALIZE(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccClassHasEmptyFinalize set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassFinalizeNeeded & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_FINALIZE_NEEDED(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccClassFinalizeNeeded set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassHasVerifyData & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_HAS_VERIFY_DATA(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccClassHasVerifyData set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccClassUnsafe & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_IS_UNSAFE(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccClassUnsafe set\n", testData->classFileName );
		}
	}
	if ( TEST_J9AccSynthetic & testData->testFlagOFF ) {
		if (0 != J9ROMCLASS_IS_SYNTHETIC(romClass)) {
			outputErrorMessage(TEST_ERROR_ARGS, "ROMClass for class: %s has J9AccSynthetic set\n", testData->classFileName );
		}
	}

}

static IDATA
testClass(J9PortLibrary *portLib, RCCorrectnessConfig *testData )
{
	PORT_ACCESS_FROM_PORT(portLib);

	const char * testName = testData->classFileName;
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
	IDATA fd = 0;
	IDATA rc = 0;

	reportTestEntry(PORTLIB, testData->classFileName);

	fd = j9file_open(testData->classFileName, EsOpenRead, 0);
	if (-1 == fd){
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to open classfile: %s \n", testData->classFileName );
		goto _exit_test;
	}
	
	classFileSize = (U_32)j9file_seek(fd, 0, EsSeekEnd);
	j9file_seek(fd, 0, EsSeekSet);
	classFileBytes = j9mem_allocate_memory(classFileSize, J9MEM_CATEGORY_CLASSES);
	bytesRead = (U_32)j9file_read(fd, classFileBytes, classFileSize);
	if (bytesRead != classFileSize) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to read classfile: %s \n", testData->classFileName );
		goto _exit_test;
	}
	rc = j9bcutil_buildRomClassIntoBuffer(classFileBytes, classFileSize, PORTLIB, verifyBuffers, flags, 0, testData->findClassFlags, romClassBuffer, romClassBufferSize, NULL, 0, NULL, 0, NULL);

	if ( BCT_ERR_NO_ERROR != rc ) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to create ROMClass for class: %s \n", testData->classFileName );
		goto _exit_test;
	}

	/* TODO: use various methods to check that flags, methods, etc... exist on newly created ROMClass */
	verifyClassFlags(PORTLIB, testData->classFileName, (J9ROMClass *)romClassBuffer, testData );
_exit_test:
	if (-1 != fd) {
		j9file_close(fd);
	}
	return reportTestExit(PORTLIB, testData->classFileName);
}




RCCorrectnessConfig test1 = {
		"VM.class",

		TEST_J9AccClassHasVerifyData,

		TEST_J9AccClassHasFinalFields |
		TEST_J9AccClassCloneable |
		TEST_J9AccClassReferenceWeak |
		TEST_J9AccClassReferenceSoft |
		TEST_J9AccClassReferencePhantom |
		TEST_J9AccClassHasEmptyFinalize |
		TEST_J9AccClassFinalizeNeeded   |
		TEST_J9AccSynthetic,

		0,

		0,
		NULL
};

RCCorrectnessConfig test2 = {
		"PhantomReference.class",

		TEST_J9AccClassReferenceSoft |
		TEST_J9AccClassReferencePhantom |
		TEST_J9AccClassReferenceWeak |
		TEST_J9AccClassHasVerifyData,

		TEST_J9AccClassHasFinalFields |
		TEST_J9AccClassCloneable |
		TEST_J9AccClassHasEmptyFinalize |
		TEST_J9AccClassFinalizeNeeded   |
		TEST_J9AccSynthetic,

		0,

		0,
		NULL
};

RCCorrectnessConfig test3 = {
		"AbstractClassLoader$3.class",

		TEST_J9AccClassHasFinalFields |
		TEST_J9AccClassHasVerifyData,

		TEST_J9AccClassReferencePhantom |
		TEST_J9AccClassReferenceWeak |
		TEST_J9AccClassCloneable |
		TEST_J9AccClassReferenceSoft |
		TEST_J9AccClassFinalizeNeeded   |
		TEST_J9AccSynthetic,

		0,

		0,
		NULL
};

RCCorrectnessConfig test4 = {
		"AttachHandler$1.class",

		TEST_J9AccClassHasVerifyData |
		TEST_J9AccSynthetic ,

		TEST_J9AccClassHasFinalFields |
		TEST_J9AccClassCloneable |
		TEST_J9AccClassReferenceWeak |
		TEST_J9AccClassReferenceSoft |
		TEST_J9AccClassReferencePhantom |
		TEST_J9AccClassHasEmptyFinalize |
		TEST_J9AccClassFinalizeNeeded,

		0,

		0,
		NULL
};

RCCorrectnessConfig test5 = {
		"ZipEntry.class",

		TEST_J9AccClassCloneable |
		TEST_J9AccClassHasVerifyData,

		TEST_J9AccClassHasFinalFields |
		TEST_J9AccClassHasEmptyFinalize |
		TEST_J9AccClassReferenceWeak |
		TEST_J9AccClassReferenceSoft |
		TEST_J9AccClassReferencePhantom |
		TEST_J9AccClassFinalizeNeeded   |
		TEST_J9AccSynthetic,

		0,

		0,
		NULL
};


RCCorrectnessConfig test6 = {
		"ZipStream.class",

		TEST_J9AccClassFinalizeNeeded   |
		TEST_J9AccClassHasVerifyData,

		TEST_J9AccClassHasFinalFields |
		TEST_J9AccClassCloneable |
		TEST_J9AccClassReferenceWeak |
		TEST_J9AccClassReferenceSoft |
		TEST_J9AccClassReferencePhantom |
		TEST_J9AccClassHasEmptyFinalize |
		TEST_J9AccSynthetic,

		0,

		0,
		NULL
};

RCCorrectnessConfig test7 = {
		"PhantomReference.class",

		TEST_J9AccClassUnsafe,

		0,

		J9_FINDCLASS_FLAG_UNSAFE,

		0,
		NULL
};

IDATA
j9dyn_testROMClassCorrectness(J9PortLibrary *portLib)
{
	PORT_ACCESS_FROM_PORT(portLib);
	IDATA rc = 0;

	HEADING(PORTLIB, "j9dyn_testROMClassCorrectness");
	rc |= testClass(PORTLIB, &test1);
	rc |= testClass(PORTLIB, &test2);
	rc |= testClass(PORTLIB, &test3);
	rc |= testClass(PORTLIB, &test4);
	rc |= testClass(PORTLIB, &test5);
	rc |= testClass(PORTLIB, &test6);
	rc |= testClass(PORTLIB, &test7);
	return rc;
}

