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
#if !defined(OSCACHETESTSYSV_HPP_INCLUDED)
#define OSCACHETESTSYSV_HPP_INCLUDED

extern "C" {
#include "j9.h"
#include "exelib_api.h"
#include "main.h"
}

#define SHM_REGIONSIZE 1024*1024

#define OSCACHETESTSYSV_CMDLINE_PREFIX "_oscsysvt_"

#define OSCACHETEST_CMDLINE_MULTIPLECREATE OSCACHETESTSYSV_CMDLINE_PREFIX "mc"
#define OSCACHETEST_CMDLINE_MUTEX OSCACHETESTSYSV_CMDLINE_PREFIX "mutex"
#define OSCACHETEST_CMDLINE_MUTEXHANG OSCACHETESTSYSV_CMDLINE_PREFIX "mutexhang"
#define OSCACHETEST_CMDLINE_DESTROY OSCACHETESTSYSV_CMDLINE_PREFIX "destroy"

class SH_OSCacheTestSysv {
public:
	/* Main function to run all the tests */
	static IDATA runTests(J9JavaVM* vm, struct j9cmdlineOptions* arg, const char *cmdline);
	static J9VMThread *currentThread;
	
private:
	/* Each test is a private function in the class */
	static IDATA testBasic(J9PortLibrary *portLibrary, J9JavaVM *vm);
	static IDATA testMultipleCreate(J9PortLibrary* portLibrary, J9JavaVM *vm, struct j9cmdlineOptions* arg, UDATA child);
	static IDATA testConstructor(J9PortLibrary *portLibrary, J9JavaVM *vm);
	static IDATA testFailedConstructor(J9PortLibrary* portLibrary, J9JavaVM *vm);
	static IDATA testGetAllCacheStatistics(J9JavaVM* vm);
	static IDATA testMutex(J9PortLibrary* portLibrary, J9JavaVM *vm, struct j9cmdlineOptions* arg, UDATA child);
	static IDATA testMutexHang(J9PortLibrary * portLibrary, J9JavaVM *vm, struct j9cmdlineOptions* arg, UDATA child);
	static IDATA testSize(J9PortLibrary *portLibrary, J9JavaVM *vm);
	static IDATA testDestroy(J9PortLibrary *portLibrary, J9JavaVM *vm, struct j9cmdlineOptions* arg, UDATA child);
};

#endif /* OSCACHETESTSYSV_HPP_INCLUDED */
