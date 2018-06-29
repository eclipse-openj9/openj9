/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

#ifndef DDR_H_
#define DDR_H_
#ifdef __cplusplus
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#include "windows.h"
#endif

#if defined(LINUX) || defined(AIXPPC) || defined(OSX)
#include <dlfcn.h> /* dlopen,dlsym */
#endif

#if defined(J9ZOS390)
#include <dll.h>
#endif

#include <jni.h>
#include "j9.h"
#include "j9protos.h"
#include "j9port.h"

#define TARGET_ARCH_UNKNOWN           0
#define TARGET_ARCH_X86_32            1
#define TARGET_ARCH_X86_64            2
#define TARGET_ARCH_S390_31           3
#define TARGET_ARCH_S390_64           4
#define TARGET_ARCH_PPC_32            5
#define TARGET_ARCH_PPC_64            6
#define TARGET_ARCH_ARM               7
#define TARGET_ARCH_IA64              8

#define STRING_MATCHED 0

#define DDREXT_REGIONS_SUCCESS 0
#define DDREXT_REGIONS_INVALID_HANDLE -1
#define DDREXT_REGIONS_NO_MORE_REGIONS -2
#define DDREXT_REGIONS_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM -3
#define DDREXT_REGIONS_UNEXPECTED_BAD_RETURN_CODE -4

typedef struct ddrExtEnv {
	JavaVM		* jvm;
	JNIEnv		* env;
	jobject		  ddrContext;
} ddrExtEnv;

typedef struct ddrRegister {
	char * name;
	unsigned long long value;
} ddrRegister;


ddrExtEnv *  ddrGetEnv();
JavaVM *     ddrStart();
void         ddrStop();
void* dbgFindPatternInRange(U_8* pattern, UDATA patternLength, UDATA patternAlignment, U_8* startSearchFrom, UDATA bytesToSearch, UDATA* bytesSearched);
BOOLEAN      ddrHasExited();

/**
 * Stop JVM that runs DDR
 */
void dbgext_stopddr(const char *args);

/**
 * Start JVM and initialize DDR
 */
void dbgext_startddr(const char *args);
 
/**
 * DDR Command wrapper
 */
void dbgext_ddr(const char *args);


#ifdef __cplusplus
}
#endif
#endif /* DDR_H_ */
