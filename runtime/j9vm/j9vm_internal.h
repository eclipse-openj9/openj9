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

#ifndef j9vm_internal_h
#define j9vm_internal_h


#ifdef WIN32
#include <tchar.h>
#include <io.h>
#if !defined(O_SYNC)
#define O_SYNC 0x0800
#endif
#if !defined(O_DSYNC)
#define O_DSYNC 0x2000
#endif
#define JVM_DEFAULT_ERROR_BUFFER_SIZE 256
#endif /* WIN32 */

#if defined(LINUX)
#if !defined(_GNU_SOURCE)
/* defining _GNU_SOURCE allows the use of dladdr() in dlfcn.h */
#define _GNU_SOURCE
#endif /* !defined(_GNU_SOURCE) */
#include <dlfcn.h>
#endif

/* We need to do this to get the system includes to give xlC what it only wants to give GCC but not give xlC the things which it doesn't understand */
#if defined(LINUXPPC)
#include <features.h>
#undef __USE_GNU
#include <sys/socket.h>
#define __USE_GNU 1
#endif

/* Avoid renaming malloc/free */
#define LAUNCHERS
#include "jvm.h"

#if defined(J9UNIX)
#include <sys/socket.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <setjmp.h>
#include <sys/time.h>

/* On OSX, fstat64 is deprecated. So, fstat is used on OSX. */
#if defined(J9ZTPF) || defined(OSX)
#define J9FSTAT fstat
#else /* defined(J9ZTPF) || defined(OSX) */
#define J9FSTAT fstat64
#endif /* defined(J9ZTPF) || defined(OSX) */
#endif /* defined(J9UNIX) */


/* required for poll support on some Unix platforms (called in JVM_Available) */
#if defined(LINUX) || defined(OSX) || defined(AIXPPC) || defined(J9ZOS390)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined(LINUX) || defined(OSX)
#include <poll.h>
#elif defined(AIXPPC)
#include <sys/poll.h>
#elif defined(J9ZOS390)
#include <poll.h>
#else
#error No poll.h location on this platform
#endif
#endif

#ifdef RS6000
#include <sys/ldr.h>
#include <load.h>
#endif

#if defined(J9ZOS390)
#include <assert.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <dll.h>
#include <sys/ioctl.h>
#include "atoe.h"
#include <unistd.h>
#define dlsym	dllqueryfn
#define dlopen(a,b) 	dllload(a)
#define dlclose	dllfree
#define J9FSTAT fstat
#endif

#include "omrthread.h"

/**
 * Initializes the VM-interface from the supplied JNIEnv.
 */
void initializeVMI(void);

jclass
jvmDefineClassHelper(JNIEnv *env, jobject classLoaderObject,
	jstring className, jbyte * classBytes, jint offset, jint length, jobject protectionDomain, UDATA options);

#endif /* j9vm_internal_h */
