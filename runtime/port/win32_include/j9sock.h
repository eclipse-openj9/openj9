/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#ifndef j9sock_h
#define j9sock_h

/******************************************************\
		Portable socket library implementation.
\******************************************************/

/* include common types from portsock.h */
#include "portsock.h"

#include "j9socket.h"
#include "j9comp.h"
#include "j9port.h"
#include "j9portpg.h"

#ifdef DEBUG
#define J9SOCKDEBUG(x, error) printf(x, error)
#define J9SOCKDEBUGPRINT(x) printf(x)
#else
#define J9SOCKDEBUG(x, error) 
#define J9SOCKDEBUGPRINT(x)
#endif

extern J9_CFUNC char * os_error_message (struct J9PortLibrary *portLibrary, int32_t errorNum );

/* os types */
typedef struct addrinfoW OSADDRINFO;  /* IPv6 - Unicode */

typedef SOCKADDR_IN6 OSSOCKADDR_IN6;  /* IPv6 */

/* defines added for IPv6 */
#define OS_AF_INET4 AF_INET
#define OS_AF_UNSPEC AF_UNSPEC
#define OS_PF_UNSPEC PF_UNSPEC
#define OS_PF_INET4 PF_INET

#define OS_AF_INET6 AF_INET6
#define OS_PF_INET6 PF_INET6
#define OS_INET4_ADDRESS_LENGTH INET_ADDRSTRLEN
#define OS_INET6_ADDRESS_LENGTH INET6_ADDRSTRLEN

/*
 * Socket Types
 */
#define OSSOCK_ANY			 0                                     /* for getaddrinfo hints */
#define OSSOCK_STREAM     SOCK_STREAM              /* stream socket */
#define OSSOCK_DGRAM      SOCK_DGRAM                /* datagram socket */
#define OSSOCK_RAW        SOCK_RAW                      /* raw-protocol interface */
#define OSSOCK_RDM        SOCK_RDM                       /* reliably-delivered message */
#define OSSOCK_SEQPACKET  SOCK_SEQPACKET    /* sequenced packet stream */


/* Per-thread buffer for platform-dependent socket information */
typedef struct J9SocketPTB {
	struct J9PortLibrary *portLibrary;
	j9addrinfo_struct addr_info_hints;
} J9SocketPTB;

#endif

