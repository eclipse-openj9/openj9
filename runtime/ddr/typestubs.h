/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#ifndef TYPESTUBS_H
#define TYPESTUBS_H

//typedefs required to allow the pre-processed VM source to parse.
//
//For the purposes of the parser, we don't need the typedefs to be correct. We just need
//the name defined.

#define __inline__ inline
#define inline

typedef int va_list;
typedef int FILE;
#if !defined(TYPESTUB_CPLUSPLUS)
typedef int size_t;
#endif
typedef int wchar_t;
typedef int ucontext_t;
typedef int sigset_t;
typedef int fd_set;
typedef int pthread_mutex_t;
typedef int pthread_t;
typedef int pthread_key_t;
typedef int key_t;
typedef int pthread_cond_t;
typedef int pthread_condattr_t;
typedef int pthread_spinlock_t;
typedef int sem_t;
typedef int clockid_t;
typedef int uint32_t;
typedef int int32_t;
typedef int uintptr_t;
typedef int uint16_t;
typedef int int16_t;
typedef int uint8_t;
typedef int int8_t;
typedef int intptr_t;
typedef int uint64_t;
typedef int int64_t;
typedef int nodemask_t;
typedef int iconv_t;
typedef int cpu_set_t;

#define INT_MAX (size_t)(-1)

extern void * va_start(va_list, void*);
extern void va_end(va_list);

extern void * memset (void *, int, size_t);
extern void * memcpy (void *,void *,size_t);
extern int memcmp (void *, const void *, size_t);
extern int abs(int);
extern int strcmp(char *, char *);
extern int strncmp(const char *s1, const char *s2, size_t n);

#if defined(J9ZTPF)
struct sockaddr_in {
	int foo;
};
#else /* defined(J9ZTPF) */
struct sockaddr_storage {
	int foo;
};
#endif /* defined(J9ZTPF) */

struct timeval {
	int foo;
};

struct linger {
	int foo;
};

struct ipv6_mreq {
	int foo;
};

struct ip_mreq {
	int foo;
};

struct in6_addr {
	int foo;
};

struct in_addr {
	int foo;
};

typedef int SOCKADDR_IN6;

struct hostent {
	int foo;
};

struct sigaction {
	int foo;
};

void pthread_mutex_lock(void*);
void pthread_mutex_unlock(void*);
void __cs1(void*,void*,void*);
void __csg(void*,void*,void*);

#endif
