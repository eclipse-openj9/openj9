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
		Portable socket library header.
\******************************************************/

#ifndef j9socket_h
#define j9socket_h

#include <stddef.h>
#include "j9porterror.h"

/* Socket types, stream & datagram */
#define J9SOCK_AFINET 2
#define J9SOCK_INADDR_ANY (uint32_t)0
#define J9SOCK_NOFLAGS (uint32_t)0			/* The default flag argument value, as in a recv */
#define J9SOCK_INADDR_LEN 4				/* The length in bytes of a binary IPv4 internet address */
#define J9SOCK_INADDR6_LEN 16            /* The length in bytes of a binary IPv6 internet address */

/* For getaddrinfo (IPv6) -- socket types */
#define J9SOCKET_ANY			 0       /* for getaddrinfo hints */
#define J9SOCKET_STREAM     1        /* stream socket */
#define J9SOCKET_DGRAM       2        /* datagram socket */
#define J9SOCKET_RAW           3        /* raw-protocol interface */
#define J9SOCKET_RDM           4         /* reliably-delivered message */
#define J9SOCKET_SEQPACKET  5     /* sequenced packet stream */

/** address family */
#define J9ADDR_FAMILY_UNSPEC 0                /* IPv6 */
#define J9ADDR_FAMILY_AFINET4 2                /* IPv6 */
#define J9ADDR_FAMILY_AFINET6 23              /* IPv6 */

/** protocol family */
#define J9PROTOCOL_FAMILY_UNSPEC   J9ADDR_FAMILY_UNSPEC                /* IPv6 */
#define J9PROTOCOL_FAMILY_INET4        J9ADDR_FAMILY_AFINET4                /* IPv6 */
#define J9PROTOCOL_FAMILY_INET6        J9ADDR_FAMILY_AFINET6                /* IPv6 */

/* Platform Constants */
typedef struct j9sockaddr_struct *j9sockaddr_t;
typedef struct j9hostent_struct *j9hostent_t;
typedef struct j9addrinfo_struct *j9addrinfo_t;                               /* IPv6 */

#endif     /* j9socket_h */


