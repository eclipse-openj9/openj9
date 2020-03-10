/*******************************************************************************
 * Copyright (c) 1998, 2020 IBM Corp. and others
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

/* This file did redirect to the port socket header but that sort of reaching back into a private module is very unsafe and breaks with vpath since the header will no longer be in the root of port */
/* As a temporary solution, the structures we were looking for will be stored here.  Eventually, this file should be merged into the port_api.h */

#if defined(WIN32)
/* windows.h defined uintptr_t.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */

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

#if defined(WIN32)
typedef SOCKADDR_IN OSSOCKADDR;	/* as used by bind() and friends */
#else /* !WIN32 */
typedef struct sockaddr_in OSSOCKADDR;	/* as used by bind() and friends */
#endif /* !WIN32 */

typedef struct j9hostent_struct {
	OSHOSTENT *entity;
} j9hostent_struct;

#endif     /* portsock_h */


