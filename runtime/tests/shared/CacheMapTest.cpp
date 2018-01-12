/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#include "CacheMap.hpp"
#include "ClasspathItem.hpp"
#include "sharedconsts.h"
#include "j9port.h"
#include "main.h"
#include "j9.h"
#include <string.h>

extern "C" 
{ 
	IDATA testCacheMap(J9JavaVM* vm); 
}

class CacheMapTest {
	public:
		static IDATA createPathStringTest(J9VMThread* currentThread);
	private:
		static IDATA callCreatePathString(J9VMThread* currentThread, const char* className, const char* classpathEntry, IDATA protocol, const char* expectedResult);
};

IDATA 
CacheMapTest::callCreatePathString(J9VMThread* currentThread, const char* className, const char* classpathEntry, IDATA protocol, const char* expectedResult)
{
	char pathBuf[256];
	char smallBuf[11];
	char* pathBufPtr = (char*)pathBuf;
	char* smallBufPtr = (char*)smallBuf;
	bool doFreeBuffer;
	ClasspathEntryItem stackCpei;
	ClasspathEntryItem* cpei;
	IDATA result = PASS;
	UDATA classNameLength = (className ? strlen(className) : 0);
	const char* classNameTrace = (className ? className : "NULL");

	PORT_ACCESS_FROM_PORT(currentThread->javaVM->portLibrary);

	cpei = ClasspathEntryItem::newInstance(classpathEntry, (U_16)strlen(classpathEntry), protocol, &stackCpei);

	SH_CacheMap::createPathString(currentThread, currentThread->javaVM->sharedClassConfig, &pathBufPtr, 256, cpei, className, classNameLength, &doFreeBuffer);
	if (strcmp(pathBufPtr, expectedResult)) {
		result = 1;
	}
	if (doFreeBuffer) {
		result = 2;
	}
	SH_CacheMap::createPathString(currentThread, currentThread->javaVM->sharedClassConfig, &smallBufPtr, 11, cpei, className, classNameLength, &doFreeBuffer);
	if (strcmp(smallBufPtr, expectedResult)) {
		result = 3;
	}
	if (!doFreeBuffer) {
		result = 4;
	} else {
		j9mem_free_memory(smallBufPtr);
	}
	j9tty_printf(PORTLIB, "Testing classname \"%s\" with cpe \"%s\". \n   Result = \"%s\". Rc=%d\n\n", classNameTrace, classpathEntry, pathBufPtr, result);
	
	return result;
}

IDATA
CacheMapTest::createPathStringTest(J9VMThread* currentThread)
{
	IDATA result = PASS;

#ifdef WIN32
	result |= callCreatePathString(currentThread, NULL, "C:\\classes1", PROTO_DIR, "C:\\classes1");
	result |= callCreatePathString(currentThread, NULL, "C:\\classes2\\", PROTO_DIR, "C:\\classes2\\");
	result |= callCreatePathString(currentThread, "myClass1", "C:\\classes1", PROTO_DIR, "C:\\classes1\\myClass1.class");
	result |= callCreatePathString(currentThread, "myClass2", "C:\\classes2\\", PROTO_DIR, "C:\\classes2\\myClass2.class");
	result |= callCreatePathString(currentThread, "package1.myClass1", "C:\\classes1", PROTO_DIR, "C:\\classes1\\package1\\myClass1.class");
	result |= callCreatePathString(currentThread, "package1.package2.myClass2", "C:\\classes2", PROTO_DIR, "C:\\classes2\\package1\\package2\\myClass2.class");
	result |= callCreatePathString(currentThread, "package1.myClass3", "C:\\classes3\\", PROTO_DIR, "C:\\classes3\\package1\\myClass3.class");
	result |= callCreatePathString(currentThread, "package1.package2.myClass4", "C:\\classes4\\", PROTO_DIR, "C:\\classes4\\package1\\package2\\myClass4.class");
	result |= callCreatePathString(currentThread, "package1/myClass1", "C:\\classes1", PROTO_DIR, "C:\\classes1\\package1\\myClass1.class");
	result |= callCreatePathString(currentThread, "package1/package2/myClass2", "C:\\classes2", PROTO_DIR, "C:\\classes2\\package1\\package2\\myClass2.class");
	result |= callCreatePathString(currentThread, "package1/myClass3", "C:\\classes3\\", PROTO_DIR, "C:\\classes3\\package1\\myClass3.class");
	result |= callCreatePathString(currentThread, "package1/package2/myClass4", "C:\\classes4\\", PROTO_DIR, "C:\\classes4\\package1\\package2\\myClass4.class");
#else
	result |= callCreatePathString(currentThread, NULL, "/classes1", PROTO_DIR, "/classes1");
	result |= callCreatePathString(currentThread, NULL, "/classes2/", PROTO_DIR, "/classes2/");
	result |= callCreatePathString(currentThread, "myClass1", "/classes1", PROTO_DIR, "/classes1/myClass1.class");
	result |= callCreatePathString(currentThread, "myClass2", "/classes2/", PROTO_DIR, "/classes2/myClass2.class");
	result |= callCreatePathString(currentThread, "package1.myClass1", "/classes1", PROTO_DIR, "/classes1/package1/myClass1.class");
	result |= callCreatePathString(currentThread, "package1.package2.myClass2", "/classes2", PROTO_DIR, "/classes2/package1/package2/myClass2.class");
	result |= callCreatePathString(currentThread, "package1.myClass3", "/classes3/", PROTO_DIR, "/classes3/package1/myClass3.class");
	result |= callCreatePathString(currentThread, "package1.package2.myClass4", "/classes4/", PROTO_DIR, "/classes4/package1/package2/myClass4.class");
	result |= callCreatePathString(currentThread, "package1/myClass1", "/classes1", PROTO_DIR, "/classes1/package1/myClass1.class");
	result |= callCreatePathString(currentThread, "package1/package2/myClass2", "/classes2", PROTO_DIR, "/classes2/package1/package2/myClass2.class");
	result |= callCreatePathString(currentThread, "package1/myClass3", "/classes3/", PROTO_DIR, "/classes3/package1/myClass3.class");
	result |= callCreatePathString(currentThread, "package1/package2/myClass4", "/classes4/", PROTO_DIR, "/classes4/package1/package2/myClass4.class");
#endif

	return result;
}

IDATA 
testCacheMap(J9JavaVM* vm)
{
	UDATA success = PASS;
	UDATA rc = 0;
	
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	REPORT_START("CacheMap");

	SHC_TEST_ASSERT("createPathString", CacheMapTest::createPathStringTest(vm->mainThread), success, rc);

	REPORT_SUMMARY("CacheMap", success);

	return success;
}

