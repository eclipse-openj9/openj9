/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
/******************************************************\
		Common data types required for using the port socket API
\******************************************************/

#ifndef portsock_h
#define portsock_h

#if defined(LINUX) || defined(AIXPPC) || defined(OSX)
/* needed to pull in "struct timeval" */
#include <sys/select.h>
#elif defined(J9ZOS390)
#include <sys/time.h>
#endif

/* This file did redirect to the port socket header but that sort of reaching back into a private module is very unsafe and breaks with vpath since the header will no longer be in the root of port */
/* As a temporary solution, the structures we were looking for will be stored here.  Eventually, this file should be merged into the port_api.h */

#if defined(WIN32)
/* windows.h defined uintptr_t.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */
#define SOCKET_USE_IPV4_MASK '\x4'        /* 00000100 - this tells which one to pick when doing an operation */

/* Undefine the winsockapi because winsock2 defines it.  Removes warnings. */
#if defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
#undef _WINSOCKAPI_
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else /* defined(WIN32) */

#include <sys/socket.h>
#include <netinet/in.h>
#endif /* !defined(WIN32) */

#include "j9comp.h"

#define J9SOCK_BADSOCKET -1

#ifdef NI_MAXHOST
#define OSNIMAXHOST NI_MAXHOST
#define OSNIMAXSERV NI_MAXSERV
#else
#define OSNIMAXHOST 1025
#define OSNIMAXSERV 32
#endif

#if defined(WIN32)
#define ADDRINFO_FLAG_UNICODE 1
#endif

/** new structure for IPv6 addrinfo will either point to a hostent or 
	an addr info depending on the IPv6 support for this OS */
typedef struct j9addrinfo_struct {
		void *addr_info;
		int length;
#if defined(WIN32)
		int flags;
#endif
} j9addrinfo_struct;

typedef struct hostent OSHOSTENT;
typedef struct ip_mreq OSIPMREQ;
typedef struct ipv6_mreq OSIPMREQ6;

typedef struct linger OSLINGER;

#if defined(WIN32)
typedef SOCKET OSSOCKET;		/* as returned by socket() */
typedef SOCKADDR_IN OSSOCKADDR;	/* as used by bind() and friends */
typedef struct sockaddr_storage OSSOCKADDR_STORAGE; /* IPv6 */
#else /* !WIN32 */
typedef int OSSOCKET;		/* as returned by socket() */
typedef struct sockaddr_in OSSOCKADDR;	/* as used by bind() and friends */
#if !defined(J9ZTPF)  /* z/TPF doesn't have IPv6 */
typedef struct sockaddr_storage OSSOCKADDR_STORAGE; /* IPv6 */
#endif /* !defined(J9ZTPF) */
#endif /* !WIN32 */

#if defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX)
#define SOCKET_CAST(x) ((struct j9socket_struct*)  x)->sock
#else /* defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX) */
/* all the non-unix ports use this variant */
/* The sockets returned as a j9socket_t (j9socket_struct*) are not actual structs, we just pretend that the pointer is a struct and never dereference it.
*/
#ifdef NO_LVALUE_CASTING
#define SOCKET_CAST(x) (*((OSSOCKET *) &(x)))
#else /* NO_LVALUE_CASTING */
#define SOCKET_CAST(x) ((OSSOCKET)x)
#endif /* !NO_LVALUE_CASTING */
#endif /* defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX) */


typedef struct j9sockaddr_struct {
#if defined(IPv6_FUNCTION_SUPPORT) || (defined(WIN32))
	OSSOCKADDR_STORAGE addr;
#else
	OSSOCKADDR addr;
#endif
} j9sockaddr_struct;

typedef struct j9fdset_struct {
#if defined(LINUX) || defined(OSX)
	intptr_t fd;       /* fd == -1 indicates the j9fdset is "empty" fd. */
#else
	fd_set handle;
#endif /* defined(LINUX) || defined(OSX) */
} j9fdset_struct;

typedef struct j9timeval_struct {
	struct timeval time;
} j9timeval_struct;

/*
* Socket structure on windows requires 2 sockets due to the fact that microsoft does not
* handle ipv6-mapped ipv6 addresses.  Therefore we need to listen to sockets on both
* the ipv6 and ipv4 stacks, when in a mode where we support ipv6.
*/
typedef struct j9socket_struct {
#if defined(WIN32)
	OSSOCKET ipv4;
	OSSOCKET ipv6;
	uint8_t flags;
#else /* WIN32 */
	OSSOCKET sock;
	uint16_t family;
#endif /* !WIN32 */
} j9socket_struct;

typedef struct j9linger_struct {
	OSLINGER linger;
} j9linger_struct;

typedef struct j9hostent_struct {
	OSHOSTENT *entity;
} j9hostent_struct;

typedef struct j9ipmreq_struct {
	OSIPMREQ addrpair;
} j9ipmreq_struct;

/** new structure for IPv6 mulitcast requests */
typedef struct j9ipv6_mreq_struct {
#if defined(IPv6_FUNCTION_SUPPORT) || ((defined(WIN32)))
		OSIPMREQ6 mreq;
#else
		int dummy;
#endif
} j9ipv6_mreq_struct;

/* structure for returning either and IPV4 or IPV6 ip address */
typedef struct j9ipAddress_struct {
	union {
#if defined(IPv6_FUNCTION_SUPPORT) || ((defined(WIN32)))
		uint8_t					bytes[sizeof(struct in6_addr)];
#else
		uint8_t					bytes[sizeof(struct in_addr)];
#endif
		struct in_addr	 	inAddr;
#if defined(IPv6_FUNCTION_SUPPORT) || ((defined(WIN32)))
		struct in6_addr 	in6Addr;
#endif
		} addr;
	uint32_t 	length;
	uint32_t   scope;
}  j9ipAddress_struct;

/* structure for returning network interface information */
typedef struct j9NetworkInterface_struct{
	char *name;
	char *displayName;
	uint32_t numberAddresses;
	uint32_t index;
	struct j9ipAddress_struct  *addresses;
} j9NetworkInterface_struct ;

/* array of network interface structures */
typedef struct j9NetworkInterfaceArray_struct {
	uint32_t length;
	struct j9NetworkInterface_struct* elements;
} j9NetworkInterfaceArray_struct;

#endif     /* portsock_h */


