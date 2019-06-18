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

#ifndef jvmimage_h
#define jvmimage_h

#define IS_WARM_RUN(javaVM) J9_ARE_ALL_BITS_SET(javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_RAMSTATE_WARM_RUN)
#define IS_COLD_RUN(javaVM) J9_ARE_ALL_BITS_SET(javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_RAMSTATE_COLD_RUN)
#define OMRPORT_FROM_IMAGE() getImagePortLibrary()

#define INITIAL_CLASSLOADER_TABLE_SIZE 3
#define INITIAL_CLASS_TABLE_SIZE 10
#define INITIAL_CLASSPATH_TABLE_SIZE 10

enum ImageRC {
	IMAGE_OK = BCT_ERR_NO_ERROR,
	IMAGE_ERROR = BCT_ERR_GENERIC_ERROR,
	Image_EnsureWideEnum = 0x1000000 /* force 4-byte enum */
};

/* forward struct declarations */
struct ImageTableHeader;
struct JVMImageHeader;

/*
 * allows us to dump this struct into file and reload easier
 * allocated space for image data is longer than sizeof(JVMImageHeader)
 */
typedef struct JVMImageHeader {
	UDATA imageSize; /* image size in bytes */
	uintptr_t heapAddress;
	J9WSRP classLoaderTable;
	J9WSRP classSegmentTable;
	J9WSRP classPathEntryTable;
} JVMImageHeader;

/* table allows us to walk through different stored structures */
typedef struct ImageTableHeader {
	J9WSRP tableHead;
	J9WSRP tableTail; /* tail needed for O(1) append */
	UDATA tableSize; /* table size in bytes */
	UDATA currentSize; /* current size in bytes */
} ImageTableHeader;

#endif
