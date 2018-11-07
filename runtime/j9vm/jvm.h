/*******************************************************************************
 * Copyright (c) 2002, 2018 IBM Corp. and others
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


#ifndef jvm_h
#define jvm_h

#ifdef __cplusplus
extern "C" {
#endif

#if defined(IBM_MVS)
/* J2SE class libraries include jvm.h but don't define J9ZOS390. We do that here on their behalf. */
#ifndef J9ZOS390
#define J9ZOS390
#endif
#endif


#ifdef WIN32

#ifdef BOOLEAN
/* There is a collision between J9's definition of BOOLEAN and WIN32 headers */
#define BOOLEAN_COLLISION_DETECTED BOOLEAN
#undef BOOLEAN
#endif /* #ifdef BOOLEAN */

/* Undefine the winsockapi because winsock2 defines it.  Removes warnings. */
#if defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#undef _WINSOCKAPI_
#endif
#include <winsock2.h>
#define PATH_MAX MAX_PATH

#ifdef BOOLEAN_COLLISION_DETECTED
#define BOOLEAN UDATA
#undef BOOLEAN_COLLISION_DETECTED
#endif /* #ifdef BOOLEAN_COLLISION_DETECTED */

#endif /* WIN32 */

#ifdef J9ZOS390
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#define PATH_MAX 1023
#endif /* J9ZOS390 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <setjmp.h>

#include <jni.h>
#include "j9comp.h"		/* for definition */

#if defined(J9UNIX)
#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64
#endif
#include <unistd.h>
#include <sys/param.h>
#include <dirent.h>
#endif /* defined(J9UNIX) */

/* AIX has a #define for MAXPATHLEN in /usr/include/param.h which defines MAXPATHLEN as PATH_MAX+1 */
#ifndef MAXPATHLEN
#ifdef AIX
#define MAXPATHLEN   (PATH_MAX+1)
#else
#define MAXPATHLEN PATH_MAX
#endif
#endif

#ifndef O_TEMPORARY
#define O_TEMPORARY  0x10000	/* non-standard flag on Unix */
#endif

/*
 * JVM constants
 */
#define JVM_INTERFACE_VERSION	4

#define JVM_MAXPATHLEN	PATH_MAX

#define JVM_IO_OK	0
#define JVM_IO_ERR	-1
#define JVM_IO_INTR	-2

#define JVM_O_RDWR	O_RDWR
#define JVM_O_CREAT	O_CREAT
#define JVM_O_EXCL	O_EXCL
#define JVM_O_DELETE	O_TEMPORARY

#define JVM_EEXIST	-100

#define JVM_SIGTERM	SIGTERM

#define J9_SIDECAR_MAX_OBJECT_INSPECTION_AGE 10000L

#define JVM_ZIP_HOOK_STATE_OPEN 1
#define JVM_ZIP_HOOK_STATE_CLOSED 2
#define JVM_ZIP_HOOK_STATE_RESET 3
	
#if defined(J9UNIX)
typedef sigjmp_buf* pre_block_t;
#else /* defined(J9UNIX) */
typedef void* pre_block_t;
#endif /* defined(J9UNIX) */

struct sockaddr;                                                                               /* suppress warning messages */

#ifndef LAUNCHERS
#define J9MALLOC_STRINGIFY(X) #X
#define J9MALLOC_TOSTRING(X) J9MALLOC_STRINGIFY(X)

#if !defined(CURRENT_MEMORY_CATEGORY)
#define CURRENT_MEMORY_CATEGORY J9MEM_CATEGORY_SUN_JCL
#endif
#define malloc(a) JVM_RawAllocateInCategory( (a), __FILE__ ":" J9MALLOC_TOSTRING(__LINE__), CURRENT_MEMORY_CATEGORY )
#define realloc(p, a) JVM_RawReallocInCategory( (p), (a), __FILE__ ":" J9MALLOC_TOSTRING(__LINE__), CURRENT_MEMORY_CATEGORY )
#define calloc(n, a) JVM_RawCallocInCategory( (n), (a), __FILE__ ":" J9MALLOC_TOSTRING(__LINE__), CURRENT_MEMORY_CATEGORY )
#define free(a) JVM_RawFree( (a) )
#endif

/*
 * JVM methods
 */
/* ---------------- jvm.c ---------------- */

/**
* @brief
* @param env
* @param instr
* @param outstr
* @param outlen
* @param encoding
* @return jint
*/
jint 
GetStringPlatform(JNIEnv* env, jstring instr, char* outstr, jint outlen, const char* encoding);


/**
* @brief
* @param env
* @param instr
* @param outlen
* @param encoding
* @return jint
*/
jint 
GetStringPlatformLength(JNIEnv* env, jstring instr, jint* outlen, const char* encoding);

/**
* @brief
* @param *dir
* @param *file
* @return int
*/
int isFileInDir(char *dir, char *file);

/**
* @brief
* @param stream
* @param format
* @param ...
* @return int
*/
int
jio_fprintf(FILE * stream, const char * format, ...);


/**
* @brief
* @param str
* @param n
* @param format
* @param ...
* @return int
*/
int
jio_snprintf(char * str, int n, const char * format, ...);

struct sockaddr;


/**
* @brief
* @param (*func)(void)
* @return void
*/
void JNICALL
JVM_OnExit(void (*func)(void));


/**
* @brief
* @param **pvm
* @param **penv
* @param *vm_args
* @return jint
*/
jint JNICALL
JNI_CreateJavaVM(JavaVM **pvm, void **penv, void *vm_args);


/**
* @brief
* @param **vmBuf
* @param bufLen
* @param *nVMs
* @return jint
*/
jint JNICALL
JNI_GetCreatedJavaVMs(JavaVM **vmBuf, jsize bufLen, jsize *nVMs);


/**
* @brief
* @param *vm_args
* @return jint
*/
jint JNICALL
JNI_GetDefaultJavaVMInitArgs(void *vm_args);


/* JVM functions are prototyped in the redirector/forwarders.ftl file
 * which allows us to keep prototypes and forwarders in sync. */
#include "generated.h"

#ifdef __cplusplus
}
#endif

#endif     /* jvm_h */
  

