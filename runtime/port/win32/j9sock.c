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

/**
 * @file
 * @ingroup Port
 * @brief Sockets
 */

/**
 * Starting in Visual Studio 2013 (toolsVersion = 120), following functions gives compiler warnings :
 *
 * 'gethostbyaddr': Use getnameinfo() or GetNameInfoW() instead
 * 					or define _WINSOCK_DEPRECATED_NO_WARNINGS to disable deprecated API warnings
 * 'gethostbyname': Use getaddrinfo() or GetAddrInfoW() instead
 * 					or define _WINSOCK_DEPRECATED_NO_WARNINGS to disable deprecated API warnings
 * 'inet_addr'	  : Use inet_pton() or InetPton() instead
 * 					or define _WINSOCK_DEPRECATED_NO_WARNINGS to disable deprecated API warnings
 * 'inet_ntoa'	  : Use inet_ntop() or InetNtop() instead
 * 					or define _WINSOCK_DEPRECATED_NO_WARNINGS to disable deprecated API warnings
 *
 */

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "portsock.h"
#include "portpriv.h"
#include "j9sock.h"
#include "omrportptb.h"
#include "ut_j9prt.h"

#if defined(J9SMARTPHONE) && !defined(J9_USE_CONNECTION_MANAGER)
/* For backwards compatibility */
#define J9_USE_CONNECTION_MANAGER
#endif

#if defined(J9_USE_CONNECTION_MANAGER)
#define INITGUID
#include <initguid.h>
#include <connmgr.h>
#endif

#include <Iphlpapi.h>

typedef DWORD (WINAPI *GetAdaptersAddressesFunctionAddress) (ULONG, DWORD, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);
#include <limits.h>

#define VALIDATE_ALLOCATIONS 1

#define LOOP_BACK_NAME							"lo"
#define LOOP_BACK_DISPLAY_NAME			"MS TCP Loopback interface"
#define LOOP_BACK_IPV4_ADDRESS			"127.0.0.1"
#define LOOP_BACK_NUM_ADDRESSES		1

typedef struct selectFDSet_struct {
	int 	  			nfds;
	OSSOCKET 	sock;
	fd_set 			writeSet;
	fd_set			readSet;
	fd_set 			exceptionSet;
} selectFDSet_struct;

int32_t platformSocketOption(int32_t portableSocketOption);
static void initializeSocketStructure(j9socket_t sockHandle, OSSOCKET sock, BOOL useIPv4Socket);
void updateSocketState(j9socket_t socket, BOOL useIPv4Socket);
int32_t platformSocketLevel(int32_t portableSocketLevel);
static int32_t ensureConnected(struct J9PortLibrary *portLibrary);
static int32_t findError (int32_t errorCode);
int32_t map_protocol_family_J9_to_OS(int32_t addr_family);
BOOL isAnyAddress(j9sockaddr_t addr);
int32_t map_addr_family_J9_to_OS(int32_t addr_family);
int32_t map_sockettype_J9_to_OS(int32_t socket_type);
int internalCloseSocket(struct J9PortLibrary *portLibrary, j9socket_t sockHandle, BOOL closeIPv4Socket);
BOOL useIPv4Socket(j9socket_t sockHandle);

static void 
fill_addrinfo( OSADDRINFO *ipv6_result, j9addrinfo_t result, int flags  );

static int32_t
ipv4_getaddrinfo( struct J9PortLibrary *portLibrary, char *name,  const OSADDRINFO *addr_info_hints, OSADDRINFO **ipv6_result, int flags );



static int32_t
ensureConnected(struct J9PortLibrary *portLibrary)
{
	int32_t rc = 0;

#if defined(J9_USE_CONNECTION_MANAGER)
	HANDLE phConnection = NULL;
	DWORD pdwStatus = -1;

	CONNMGR_CONNECTIONINFO connectionInfo={0};
	HRESULT hr;
	HWND foregroundWindow = GetForegroundWindow();
	TCHAR windowName[8];

/*	if (PPG_sock_connection) {
		ConnMgrConnectionStatus(PPG_sock_connection, &pdwStatus);
		if (pdwStatus == CONNMGR_STATUS_CONNECTED) {
			return TRUE;
		}
		if (pdwStatus == CONNMGR_STATUS_CONNECTIONFAILED) {
			return FALSE;
		}
	} */

	GetClassName(foregroundWindow,windowName,8); 

	connectionInfo.cbSize = sizeof(connectionInfo);
	connectionInfo.dwParams = CONNMGR_PARAM_GUIDDESTNET;
	connectionInfo.dwFlags = 0;
	connectionInfo.dwPriority = CONNMGR_PRIORITY_USERINTERACTIVE ;
	connectionInfo.guidDestNet = IID_DestNetInternet;
 
	if (wcsstr(windowName,L"MIDPNG-")!=0) {
		connectionInfo.hWnd = foregroundWindow;
		connectionInfo.uMsg = WM_USER + 10;
	}
 
	hr = ConnMgrEstablishConnectionSync(&connectionInfo, &phConnection, 30000, &pdwStatus);
	return pdwStatus == CONNMGR_STATUS_CONNECTED;
#else
	return 0;
#endif
}


/**
 * @internal
 * Determines the proper portable error code to return given a native error code
 *
 * @param[in] errorCode The error code reported by the OS
 *
 * @return	the (negative) portable error code
 */
static int32_t
findError (int32_t errorCode)
{
	switch (errorCode)
	{
		case WSANOTINITIALISED:
			return J9PORT_ERROR_SOCKET_NOTINITIALIZED;
		case WSAENOPROTOOPT:
			return J9PORT_ERROR_SOCKET_OPTUNSUPP;	
		case WSAEINTR:
			return J9PORT_ERROR_SOCKET_INTERRUPTED;	
		case WSAENOTCONN:
			return J9PORT_ERROR_SOCKET_NOTCONNECTED;
		case WSAEWOULDBLOCK:
			return J9PORT_ERROR_SOCKET_WOULDBLOCK;
		case WSAECONNABORTED:
			return J9PORT_ERROR_SOCKET_TIMEOUT;
		case WSAECONNRESET:
			return J9PORT_ERROR_SOCKET_CONNRESET;
		case WSAENOBUFS:
			return J9PORT_ERROR_SOCKET_NOBUFFERS;
		case WSAEADDRINUSE:
			return J9PORT_ERROR_SOCKET_ADDRINUSE;
		case WSANO_DATA:
			return J9PORT_ERROR_SOCKET_NODATA;
		case WSAEOPNOTSUPP:
			return J9PORT_ERROR_SOCKET_OPNOTSUPP;
		case WSAEISCONN:
			return J9PORT_ERROR_SOCKET_ISCONNECTED;
		case WSAHOST_NOT_FOUND:
			return J9PORT_ERROR_SOCKET_HOSTNOTFOUND;
		case WSAEADDRNOTAVAIL:
			return J9PORT_ERROR_SOCKET_ADDRNOTAVAIL;
		case WSAEFAULT:
			return J9PORT_ERROR_SOCKET_OPTARGSINVALID;
		case WSAENOTSOCK:
			return J9PORT_ERROR_SOCKET_NOTSOCK ;
		case WSAECONNREFUSED:
			return J9PORT_ERROR_SOCKET_CONNECTION_REFUSED;
		case WSAEACCES:
			return J9PORT_ERROR_SOCKET_EACCES;
		case WSAEALREADY:
			return J9PORT_ERROR_SOCKET_ALREADYINPROGRESS;
		case WSAEINPROGRESS:
			return J9PORT_ERROR_SOCKET_EINPROGRESS;
		default:
			return J9PORT_ERROR_SOCKET_OPFAILED;
	}
}


/**
 * @internal updates the socketHandle by putting the socket in either the ipv4 or ipv6 slot
 */
int
internalCloseSocket(struct J9PortLibrary *portLibrary, j9socket_t sockHandle,  BOOL closeIPv4Socket)
{	
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uint8_t mask1 = SOCKET_USE_IPV4_MASK;
	int rc =0;

	if( closeIPv4Socket ) {
		if( sockHandle->flags & SOCKET_IPV4_OPEN_MASK ) {
			/* Don't bother to check the error -- not like we can do anything about it. */
			/*[PR 99075]*/
			shutdown(sockHandle->ipv4, 1);
			if(closesocket( sockHandle->ipv4 ) == SOCKET_ERROR) {
				rc = WSAGetLastError();
				J9SOCKDEBUG( "<closesocket failed, err=%d>\n", rc);
				switch(rc) {
					case WSAEWOULDBLOCK: 
						rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NBWITHLINGER);
						break;
					default: 
						rc = omrerror_set_last_error(rc, findError(rc));
				}
			}
			sockHandle->ipv4 = INVALID_SOCKET;
		}
	} else {
		if( sockHandle->flags & SOCKET_IPV6_OPEN_MASK ) {
			/* Don't bother to check the error -- not like we can do anything about it. */
			/*[PR 99075]*/
			shutdown(sockHandle->ipv6, 1);

			/*[PR 135824] - check for INVALID_SOCKET before closing */
			if (SOCKET_CAST( sockHandle->ipv6 ) != INVALID_SOCKET){
				if(closesocket( SOCKET_CAST( sockHandle->ipv6 ) ) == SOCKET_ERROR) {
					rc = WSAGetLastError();
					J9SOCKDEBUG( "<closesocket failed, err=%d>\n", rc);
					switch(rc) {
						case WSAEWOULDBLOCK: 
							rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NBWITHLINGER);
							break;
						default: 
							rc = omrerror_set_last_error(rc, findError(rc));
					}
				}
				sockHandle->ipv6 = INVALID_SOCKET;
			}
		}
	}


	updateSocketState( sockHandle, !closeIPv4Socket );
	return rc;
}


/**
 * @internal Returns TRUE if the address is 0.0.0.0 or ::0, FALSE otherwise
 */
BOOL
isAnyAddress(j9sockaddr_t addr)
{	
	uint32_t length = 0;
	uint8_t address[16];
	BOOL truth = TRUE;
	uint32_t i;
	uint32_t scope_id;

	/* get the address */
	j9sock_sockaddr_address6( NULL, addr, address, &length, &scope_id );

	for( i=0; i<length; i++ ) {
		if( address[ i ] != 0 ) {
				truth = FALSE;
				break;
		}
	}

	return truth;
}


/**
 * @internal Map the portable address family to the platform address family. 
 *
 * @param[in] addr_family The portable address family to convert
 *
 * @return	the platform family, or OS_AF_UNSPEC if none exists
 */
int32_t
map_addr_family_J9_to_OS(int32_t addr_family)
{
	switch( addr_family )
	{
		case J9ADDR_FAMILY_AFINET4:
			return OS_AF_INET4;
		case J9ADDR_FAMILY_AFINET6:
			return OS_AF_INET6;
	}
	return OS_AF_UNSPEC;
}


/**
 * @internal Map the portable address protocol to the platform protocol
 *
 * @param[in] addr_protocol The portable address protocol to convert
 *
 * @return	the platform family, or OS_PF_UNSPEC if none exists
 */
int32_t
map_protocol_family_J9_to_OS(int32_t addr_family)
{
	switch( addr_family )
	{
		case J9PROTOCOL_FAMILY_INET4:
			return OS_PF_INET4;
		case J9PROTOCOL_FAMILY_INET6:
			return OS_PF_INET6;
	}
	return OS_PF_UNSPEC;
}


/**
 * @internal Map the portable socket type to the platform type. 
 *
 * @param[in] addr_family The portable socket type to convert
 *
 * @return	the platform family, or OSSOCK_ANY if none exists
 */
int32_t
map_sockettype_J9_to_OS(int32_t socket_type)
{
	switch( socket_type )
	{
		case J9SOCKET_STREAM:
			return OSSOCK_STREAM;
		case J9SOCKET_DGRAM:
			return OSSOCK_DGRAM;
		case J9SOCKET_RAW:
			return OSSOCK_RAW;
		case J9SOCKET_RDM:
			return OSSOCK_RDM;
		case J9SOCKET_SEQPACKET:
			return OSSOCK_SEQPACKET;
	}
	return OSSOCK_ANY;
}


/**
 * @internal
 * Map the portable to the platform socket level.  Used to resolve the arguments of socket option functions.
 * Levels currently in use are:
 * \arg SOL_SOCKET, for most options
 * \arg IPPROTO_TCP, for the TCP noDelay option
 * \arg IPPROTO_IP, for the option operations associated with multicast (join, drop, interface)
 *
 * @param[in] portableSocketLevel The portable socket level to convert.
 *
 * @return	the platform socket level or a (negative) error code if no equivalent level exists.
 */
int32_t
platformSocketLevel(int32_t portableSocketLevel)
{
	switch(portableSocketLevel)
	{
		case J9_SOL_SOCKET: 
			return OS_SOL_SOCKET;
		case J9_IPPROTO_TCP: 
			return OS_IPPROTO_TCP;
		case J9_IPPROTO_IP: 
			return OS_IPPROTO_IP;
		case J9_IPPROTO_IPV6: 
			return OS_IPPROTO_IPV6;
	}
	return J9PORT_ERROR_SOCKET_SOCKLEVELINVALID;
}


/**
 * @internal
 * Map the portable to the platform socket options.  Used to resolve the arguments of socket option functions.
 * Options currently in supported are:
 * \arg SOL_LINGER, the linger timeout
 * \arg TCP_NODELAY, the buffering scheme implementing Nagle's algorithm
 * \arg IP_MULTICAST_TTL, the packet Time-To-Live
 * \arg IP_ADD_MEMBERSHIP, to join a multicast group
 * \arg  IP_DROP_MEMBERSHIP, to leave a multicast group
 * \arg IP_MULTICAST_IF, the multicast interface
 *
 * @param[in] portableSocketOption The portable socket option to convert.
 *
 * @return	the platform socket option or a (negative) error code if no equivalent option exists.
 */
int32_t
platformSocketOption(int32_t portableSocketOption)
{
	switch(portableSocketOption) 
	{
		case J9_SO_LINGER:
			return OS_SO_LINGER;
		case J9_SO_KEEPALIVE:
			return OS_SO_KEEPALIVE;
		case J9_TCP_NODELAY:
			return OS_TCP_NODELAY;
		case J9_MCAST_TTL:
			return OS_MCAST_TTL;
		case J9_MCAST_ADD_MEMBERSHIP:
			return OS_MCAST_ADD_MEMBERSHIP;
		case J9_MCAST_DROP_MEMBERSHIP:
			return OS_MCAST_DROP_MEMBERSHIP;
		case J9_MCAST_INTERFACE:
			return OS_MCAST_INTERFACE;
		case J9_MCAST_INTERFACE_2:
			return OS_MCAST_INTERFACE_2;
		case J9_IPV6_ADD_MEMBERSHIP:
			return OS_IPV6_ADD_MEMBERSHIP;
		case J9_IPV6_DROP_MEMBERSHIP:
			return OS_IPV6_DROP_MEMBERSHIP;		
		case J9_SO_REUSEADDR:
			return OS_SO_REUSEADDR;
		case J9_SO_SNDBUF:
			return OS_SO_SNDBUF;
		case J9_SO_RCVBUF:
			return OS_SO_RCVBUF;
		case J9_SO_BROADCAST:
			return OS_SO_BROADCAST;
		case J9_SO_OOBINLINE:
			return OS_SO_OOBINLINE;
		case J9_IP_MULTICAST_LOOP:
			return OS_MCAST_LOOP;
		case J9_IP_TOS:
			return OS_IP_TOS;
	}
	return J9PORT_ERROR_SOCKET_OPTUNSUPP;
}


/**
 * @internal updates the socketHandle by putting the socket in either the ipv4 or ipv6 slot
 */
void
updateSocketState(j9socket_t socket,  BOOL useIPv4Socket)
{	

	uint8_t mask1 = SOCKET_USE_IPV4_MASK;
	uint8_t mask2 = SOCKET_IPV4_OPEN_MASK;
	uint8_t mask3 = SOCKET_IPV6_OPEN_MASK;

	if( useIPv4Socket ) {
		/* Set the flags to "use IPv4" and "IPv6 not open"  */
		socket->flags = (socket->flags | mask1) & ~mask3; 
	} else {
		/* Set the flags to "use IPv6" and "IPv4 not open"  */
		socket->flags = (socket->flags  | mask3) & ~(mask1 | mask2); 
	}
}


/**
 * @internal initializes the socketHandle by putting the socket in either the ipv4 or ipv6 slot
 */
static void
initializeSocketStructure(j9socket_t sockHandle, OSSOCKET sock,  BOOL useIPv4Socket)
{	
	if( useIPv4Socket ) {
		sockHandle->ipv4 = sock;
		sockHandle->ipv6 = INVALID_SOCKET;
		sockHandle->flags = SOCKET_IPV4_OPEN_MASK;
	} else {
		sockHandle->ipv4 = INVALID_SOCKET;
		sockHandle->ipv6 = sock;
		sockHandle->flags = SOCKET_IPV6_OPEN_MASK;
	}

	updateSocketState( sockHandle, useIPv4Socket );
}


/**
 * @internal updates the socketHandle by putting the socket in either the ipv4 or ipv6 slot
 */
BOOL
useIPv4Socket(j9socket_t sockHandle)
{	
	return	(sockHandle->flags & SOCKET_USE_IPV4_MASK) == SOCKET_USE_IPV4_MASK;
}


/**
 * The accept function extracts the first connection on the queue of pending connections 
 * on socket sock. It then creates a new socket and returns a handle to the new socket. 
 * The newly created socket is the socket that will handle the actual the connection and 
 * has the same properties as socket sock.  
 *
 * The accept function can block the caller until a connection is present if no pending 
 * connections are present on the queue.
 *
 * @param[in] portLibrary The port library.
 * @param[in] serverSock  A j9socket_t  from which data will be read.
 * @param[in] addrHandle An optional pointer to a buffer that receives the address of the connecting 
 * entity, as known to the communications layer. The exact format of the addr parameter 
 * is determined by the address family established when the socket was created. 
 * @param[in] sockHandle A pointer to a j9socket_t  which will point to the newly created 
 * socket once accept returns successfully
 *
 * @return 
 * \arg 0 on success
 * \arg J9PORT_ERROR_SOCKET_BADSOCKET, on generic error
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED, if socket library uninitialized
 * \arg J9PORT_ERROR_SOCKET_INTERRUPTED, the call was cancelled
 * \arg J9PORT_ERROR_SOCKET_ADDRNOTAVAIL, the addr parameter is not valid
 * \arg J9PORT_ERROR_SOCKET_SYSTEMBUSY, if system busy handling other requests
 * \arg J9PORT_ERROR_SOCKET_SYSTEMFULL, is too many sockets are active
 * \arg J9PORT_ERROR_SOCKET_WOULDBLOCK, the socket is marked as nonblocking and no connections are present to be accepted., 
 */
/* IPv6 - In the case of IPv6 if the address is the any address, a socket will be created on 
 * both the IPv4 and IPv6 stack.  The Windows IPv6 stack does not implement the full
 * specification and a socket must be created to listen on both stacks for incoming calls.
 */
int32_t
j9sock_accept(struct J9PortLibrary *portLibrary, j9socket_t serverSock, j9sockaddr_t addrHandle, j9socket_t *sockHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	j9socket_t sock = 0;
	SOCKET _sc; 
	int32_t addrlen = sizeof(addrHandle->addr);

	Trc_PRT_sock_j9sock_accept_Entry(serverSock);
	
	if( useIPv4Socket( serverSock ) ) {
		((OSSOCKADDR*)&addrHandle->addr)->sin_family = OS_AF_INET4;
		_sc = accept( serverSock->ipv4, (struct sockaddr *)  &addrHandle->addr, &addrlen);
	}
	else {
		((OSSOCKADDR*)&addrHandle->addr)->sin_family = OS_AF_INET6;
		_sc = accept( serverSock->ipv6, (struct sockaddr *)  &addrHandle->addr, &addrlen);		
	}

	if(_sc == INVALID_SOCKET) {
		*sockHandle = (j9socket_t)INVALID_SOCKET;
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_accept_failure(rc);
		J9SOCKDEBUG( "<accept failed, err=%d>\n", rc); 
		switch(rc) {
				case WSAEINVAL: 
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NOTLISTENING);
				case WSAEOPNOTSUPP: 
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NOTSTREAMSOCK);
				default: 
					rc = omrerror_set_last_error(rc, findError(rc));
		}
		
		Trc_PRT_sock_j9sock_accept_Exit(rc);
		return rc;
	} else { 	
		*sockHandle = omrmem_allocate_memory(sizeof( struct j9socket_struct ) , OMRMEM_CATEGORY_PORT_LIBRARY);
		
		if (NULL == *sockHandle) {
			*sockHandle = (j9socket_t)INVALID_SOCKET;
			shutdown(_sc, 1);
			closesocket(_sc);
			rc = J9PORT_ERROR_SOCKET_NOBUFFERS;
			Trc_PRT_sock_j9sock_accept_failure_oom();
			Trc_PRT_sock_j9sock_accept_Exit(rc);
			return rc;
		}
		initializeSocketStructure( *sockHandle, _sc,  useIPv4Socket( serverSock )  ); 
	} 
	Trc_PRT_sock_j9sock_accept_socket_created(*sockHandle);
	Trc_PRT_sock_j9sock_accept_Exit(rc);
	return rc;
}


/**
 * The bind function is used on an unconnected socket before subsequent 
 * calls to the connect or listen functions. When a socket is created with a 
 * call to the socket function, it exists in a name space (address family), but 
 * it has no name assigned to it. Use bind to establish the local association 
 * of the socket by assigning a local name to an unnamed socket.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock j9socket_t which will be be associated with the specified name.
 * @param[in] addr Address to bind to socket.
 *
 * @return
 * \arg 0, on success
 * \arg J9PORT_ERROR_SOCKET_BADSOCKET, on generic error
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED, if socket library uninitialized
 * \arg J9PORT_ERROR_SOCKET_ADDRINUSE  A process on the machine is already bound to the same fully-qualified address 
 * and the socket has not been marked to allow address re-use with SO_REUSEADDR. 
 * \arg J9PORT_ERROR_SOCKET_ADDRNOTAVAIL The specified address is not a valid address for this machine 
 * \arg J9PORT_ERROR_SOCKET_SYSTEMBUSY, if system busy handling other requests
 * \arg J9PORT_ERROR_SOCKET_SYSTEMFULL, is too many sockets are active
 * \arg J9PORT_ERROR_SOCKET_BADADDR, the addr parameter is not a valid part of the user address space, 
 */
/* IPv6 - Since we may have 2 sockets open when we are in IPv6 mode we need to
 * close down the second socket.  At the bind stage we now know the IP address' family
 * so we can shut down the other socket.
 */
int32_t
j9sock_bind(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	BOOL isIPv4 = ((OSSOCKADDR *) &addr->addr)->sin_family == OS_AF_INET4;
	BOOL anyAddress = isAnyAddress( addr );
	OSSOCKET socket;
	struct sockaddr_in temp4Name;
	struct sockaddr_in6 temp6Name;
	int32_t addrlen;

	OSSOCKADDR_STORAGE anyAddr;
	/* use the IPv4 socket, if is an IPv4 address or where  IPv6 socket is null (when preferIPv4Stack=true), it will give a meaningful error msg */
	if( isIPv4 || (sock->ipv6 == -1)) { 
		socket = sock->ipv4;
	} else {
		socket = sock->ipv6;
	}

	 if(SOCKET_ERROR == bind( socket, (const struct sockaddr FAR*)&addr->addr,  sizeof(addr->addr))) {
		rc = WSAGetLastError();
		J9SOCKDEBUG( "<bind failed, err=%d>\n", rc);
		switch(rc) { 
				case WSAEINVAL: 
					return omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_ALREADYBOUND);
				default: 
					return omrerror_set_last_error(rc, findError(rc));
		}
	}

	/* If we are the any address, then we want to bind on the other IP stack as well, if that socket is still open */
	if( isAnyAddress && (( isIPv4 && (sock->flags & SOCKET_IPV6_OPEN_MASK )) || ( !isIPv4 && (sock->flags & SOCKET_IPV4_OPEN_MASK) ) ) ){
		memset( &anyAddr, 0, sizeof( OSSOCKADDR_STORAGE ) );

		if( isIPv4 ) { 
			socket = sock->ipv6;
			addrlen = sizeof(temp4Name);
			rc = getsockname( sock->ipv4, (struct sockaddr *) &temp4Name, &addrlen);
			((OSSOCKADDR_IN6 *) &anyAddr)->sin6_port = temp4Name.sin_port;
			((OSSOCKADDR *) &anyAddr)->sin_family = OS_AF_INET6;		
		} else {
			socket = sock->ipv4;
			addrlen = sizeof(temp6Name);
			rc = getsockname( sock->ipv6, (struct sockaddr *) &temp6Name, &addrlen);
			((OSSOCKADDR *) &anyAddr)->sin_port =  temp6Name.sin6_port;
			((OSSOCKADDR *) &anyAddr)->sin_family = OS_AF_INET4;
		}

		if (0 != rc) {
			rc = WSAGetLastError();
			J9SOCKDEBUG( "<getsockname in j9sock_bind failed, err=%d>\n", rc);
			return omrerror_set_last_error(rc, findError(rc));
		}
		
		if(SOCKET_ERROR == bind( socket, (const struct sockaddr FAR*)&anyAddr,  sizeof(addr->addr))) {
			rc = WSAGetLastError();
			J9SOCKDEBUG( "<bind failed, err=%d>\n", rc);
			switch(rc) { 
				case WSAEINVAL: 
					return omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_ALREADYBOUND);
				default: 
					return omrerror_set_last_error(rc, findError(rc));
			}
		}
	}

	/* close the other half of the socket, if we are not the ANY address ::0 or 0.0.0.0 */
	if( !anyAddress ) {
		internalCloseSocket(portLibrary, sock, !isIPv4 );
	}

	return rc;
}




/**
 * The close function closes a socket. Use it to release the socket descriptor socket so 
 * further references to socket will fail.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock j9socket_t  which will be closed.
 *
 * @return
 * \arg 0, on success
 * \arg J9PORT_ERROR_SOCKET_BADSOCKET, on generic error
 * \arg J9PORT_ERROR_SOCKET_SYSTEMBUSY, if system busy handling other requests
 * \arg J9PORT_ERROR_SOCKET_WOULDBLOCK,  the socket is marked as nonblocking and SO_LINGER is set to a nonzero time-out value.
 */
int32_t
j9sock_close(struct J9PortLibrary *portLibrary, j9socket_t *sock)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc1 = 0;
	int32_t rc2 = 0;
	j9socket_t theSocket = *sock;

	Trc_PRT_sock_j9sock_close_Entry(*sock);
	
	rc1 = internalCloseSocket(portLibrary, theSocket, TRUE );
	rc2 = internalCloseSocket(portLibrary, theSocket, FALSE );			
	
	omrmem_free_memory(theSocket);
	*sock = (j9socket_t) INVALID_SOCKET;

	if( rc1 == 0 ) {
		rc1 = rc2;
	}
	
	Trc_PRT_sock_j9sock_close_Exit(rc1);
	return rc1;
}


int32_t
j9sock_connect(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	DWORD socketType;
	int			socketTypeLen = sizeof(DWORD);
	byte nAddrBytes[J9SOCK_INADDR6_LEN];
#if defined(J9_USE_CONNECTION_MANAGER)
	BOOLEAN firstAttempt = TRUE;
#endif

	Trc_PRT_sock_j9sock_connect_Entry(sock, addr);
	
	J9SOCKDEBUG("connect %x\n", ((OSSOCKADDR *) &addr->addr)->sin_addr.s_addr);

	/* get the socket type, it should be the same for both sockets and one of the two should be open */
	if (sock->flags & SOCKET_IPV4_OPEN_MASK ) {
		if (getsockopt(sock->ipv4,  SOL_SOCKET, SO_TYPE, (char*)&socketType, &socketTypeLen) == SOCKET_ERROR) 	{
			Trc_PRT_sock_j9sock_connect_failure_cause("getsockopt(sock->ipv4,  SOL_SOCKET, SO_TYPE, (char*)&socketType, &socketTypeLen) == SOCKET_ERROR");
			rc = WSAGetLastError();
			Trc_PRT_sock_j9sock_connect_failure(rc);
			rc = omrerror_set_last_error(rc, findError(rc));
			Trc_PRT_sock_j9sock_connect_Exit(rc);
			return rc;
		}
	} 
	else {
		if (getsockopt(sock->ipv6,  SOL_SOCKET, SO_TYPE, (char*)&socketType, &socketTypeLen) == SOCKET_ERROR) 	{
			Trc_PRT_sock_j9sock_connect_failure_cause("getsockopt(sock->ipv6,  SOL_SOCKET, SO_TYPE, (char*)&socketType, &socketTypeLen) == SOCKET_ERROR");
			rc = WSAGetLastError();
			Trc_PRT_sock_j9sock_connect_failure(rc);
			rc = omrerror_set_last_error(rc, findError(rc));
			Trc_PRT_sock_j9sock_connect_Exit(rc);
			return rc;
		}
	}
#if defined(J9_USE_CONNECTION_MANAGER)
retry:
#endif
	/* here we need to do the connect based on the type of addressed passed in as well as the sockets which are open. If 
		a socket with a type that matches the type of the address passed in is open then we use that one.  Otherwise we
		use the socket that is open */
	if( (  (((OSSOCKADDR *) &addr->addr)->sin_family != OS_AF_UNSPEC) &&
	       ( (((OSSOCKADDR *) &addr->addr)->sin_family == OS_AF_INET4) ||
		     !(sock->flags & SOCKET_IPV6_OPEN_MASK )) &&
		     ( sock->flags & SOCKET_IPV4_OPEN_MASK ) ) ) {
		rc = connect( sock->ipv4, (const struct sockaddr FAR*) &addr->addr,  sizeof(addr->addr) ); 
		if (rc == SOCKET_ERROR) {
			Trc_PRT_sock_j9sock_connect_failure_cause(" SOCKET_ERROR == connect( sock->ipv4, (const struct sockaddr FAR*) &addr->addr,  sizeof(addr->addr) )");
			rc = WSAGetLastError();
		}
		if (socketType != SOCK_DGRAM) {
			internalCloseSocket(portLibrary, sock, FALSE );
		}
		else {	
			/* we don't actually want to close the sockets as connect can be called again on a datagram socket  but we 
			    still need to set the flag that tells us which socket to use */
			sock->flags = sock->flags | SOCKET_USE_IPV4_MASK;
		}
	} 
	else if( ((OSSOCKADDR *) &addr->addr)->sin_family == OS_AF_INET6 ) {
		rc = connect( sock->ipv6, (const struct sockaddr FAR*) &addr->addr,  sizeof(addr->addr) ); 
		rc = WSAGetLastError();
		if (socketType != SOCK_DGRAM) {
			internalCloseSocket(portLibrary, sock, TRUE );
		}
		else {	
			/* we don't actually want to close the sockets as connect can be called again on a datagram socket  but we 
			    still need to set the flag that tells us which socket to use. */
			sock->flags = sock->flags & ~SOCKET_USE_IPV4_MASK;
		}
	}
	else {
		if (socketType != SOCK_DGRAM){
			/* this should never occur */
			Trc_PRT_sock_j9sock_connect_failure_cause("socketType != SOCK_DGRAM");
			Trc_PRT_sock_j9sock_connect_Exit(J9PORT_ERROR_SOCKET_BADAF);
			return J9PORT_ERROR_SOCKET_BADAF;
		}

		/* for windows it seems to want to have it connect with an IN_ADDR any instead of with an 
		   UNSPEC family type so lets be accommodating */


		/* we need to disconnect on both sockets and swallow any expected errors */
		memset(nAddrBytes,0,J9SOCK_INADDR6_LEN);
		if(sock->flags & SOCKET_IPV4_OPEN_MASK ){
			j9sock_sockaddr_init6(portLibrary, addr, (uint8_t*) nAddrBytes, J9SOCK_INADDR_LEN, J9ADDR_FAMILY_AFINET4 , 0,0,0,sock);
			rc = connect( sock->ipv4, (const struct sockaddr FAR*) &addr->addr,  sizeof(addr->addr) ); 
			/* filter out acceptable errors */
			if( rc == SOCKET_ERROR ) {
				Trc_PRT_sock_j9sock_connect_failure_cause("rc == SOCKET_ERROR");
				rc = WSAGetLastError();
				Trc_PRT_sock_j9sock_connect_failure(rc);
				if (rc == WSAEAFNOSUPPORT || rc == WSAEADDRNOTAVAIL) {
					rc = 0;
				}
			}
		}

		if( rc == 0 && sock->flags & SOCKET_IPV6_OPEN_MASK ){
			j9sock_sockaddr_init6(portLibrary,addr, (uint8_t*) nAddrBytes, J9SOCK_INADDR_LEN, J9ADDR_FAMILY_AFINET6 , 0,0,0,sock);
			rc = connect( sock->ipv6, (const struct sockaddr FAR*) &addr->addr,  sizeof(addr->addr) ); 
			if( rc == SOCKET_ERROR ) {
				Trc_PRT_sock_j9sock_connect_failure_cause("rc == SOCKET_ERROR");
				rc = WSAGetLastError();
				Trc_PRT_sock_j9sock_connect_failure(rc);
				if (rc == WSAEAFNOSUPPORT || rc == WSAEADDRNOTAVAIL) {
					rc = 0;
				}
			}
		}
	}

	if( rc != 0 ) {
		int32_t portableError;
		J9SOCKDEBUG( "<connect failed, err=%d>\n", rc);
#if defined(J9_USE_CONNECTION_MANAGER)
		if (firstAttempt && (rc == WSAENETDOWN || rc == WSAEHOSTUNREACH)) {
			firstAttempt = FALSE;
			if (ensureConnected(portLibrary)) goto retry;
		}
		firstAttempt = FALSE;
#endif
		switch(rc) {
				case WSAEINVAL:
					portableError = J9PORT_ERROR_SOCKET_ALREADYBOUND;
					break;
				case WSAEWOULDBLOCK:
					/* We are not returning J9PORT_ERROR_SOCKET_WOULDBLOCK to be 
					 * consistent with the unix implementation */
					portableError = J9PORT_ERROR_SOCKET_EINPROGRESS;
					break;
				case WSAEAFNOSUPPORT:
					/* if it is a SOCK_DGRAM this is ok as posix says this may be returned when disconnecting */
					if (socketType == SOCK_DGRAM) {
						Trc_PRT_sock_j9sock_connect_Exit(0);
						return 0;
					}
					/* no break here as default is what we want if we do not return above */
				default:
					portableError = findError(rc);
		}
		
		Trc_PRT_sock_j9sock_connect_failure(rc);
		omrerror_set_last_error(rc, portableError);
		Trc_PRT_sock_j9sock_connect_Exit(portableError);
		return portableError;
	}
	
	Trc_PRT_sock_j9sock_connect_Exit(rc);
	return rc;
}


/**
 * Return an error message describing the last OS error that occurred.  The last
 * error returned is not thread safe, it may not be related to the operation that
 * failed for this thread.
 *
 * @param[in] portLibrary The port library
 *
 * @return	error message describing the last OS error, may return NULL.
 *
 * @internal
 * @note  This function gets the last error code from the OS and then returns
 * the corresponding string.  It is here as a helper function for JCL.  Once j9error
 * is integrated into the port library this function should probably disappear.
 */
const char *
j9sock_error_message(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	return omrerror_last_error_message();
}


/**
 * Create a file descriptor (FD) set of one element.  The call may not be generally useful,
 * as it currently only supports a single FD and is assumed to be used in conjunction with the 
 * j9sock_select function.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP pointer to the socket to be added to the FD set.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_fdset_init(struct J9PortLibrary *portLibrary, j9socket_t socketP)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	j9fdset_t fdset;
	J9SocketPTB *ptBuffers = j9sock_ptb_get(portLibrary);

	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}

	if (NULL == ptBuffers->fdset) {
		ptBuffers->fdset = omrmem_allocate_memory(sizeof(struct j9fdset_struct), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == ptBuffers->fdset) {
			return J9PORT_ERROR_SOCKET_SYSTEMFULL;
		}
	}
	fdset = ptBuffers->fdset;
	memset(fdset, 0, sizeof(struct j9fdset_struct));

	FD_ZERO(&fdset->handle);
	if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
		FD_SET( socketP->ipv4, &fdset->handle);
	} 
	if( socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
		FD_SET( socketP->ipv6, &fdset->handle);
	}

	return 0;
}


/**
 * Answer the size information, as required by @ref j9sock_select(), to act on a j9fdset_t that has "handle" set.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle
 *
 * @return	the maximum size of the fdset, otherwise the (negative) error code.
 *
 * @note On Unix, the value was the maximum file descriptor plus one, although
 * on many flavors, the value is ignored in the select function.
 * On Windows, the value is ignored by the select function.
 */
int32_t
j9sock_fdset_size(struct J9PortLibrary *portLibrary, j9socket_t handle)
{
	return 0;
}


/**
 * Frees the memory created by the call to @ref j9sock_getaddrinfo().
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Hints on what results are returned and how the response if formed .
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_freeaddrinfo(struct J9PortLibrary *portLibrary, j9addrinfo_t handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uintptr_t nameHandle;
	void (WINAPI *func1)(PADDRINFOW pAddrInfo);
	/* If we have the IPv6 functions we free the memory for an addr info, otherwise we just set the pointer to null.
       The hostent structure returned by the IPv4 function is not supposed to be freed  */
	if( PPG_sock_IPv6_FUNCTION_SUPPORT ) {
		if (ADDRINFO_FLAG_UNICODE & handle->flags) {
			if (omrsl_open_shared_library("Ws2_32", &nameHandle, TRUE) == 0) {
				if (omrsl_lookup_name(nameHandle, "FreeAddrInfoW", (uintptr_t *)&func1, "VL") == 0) {
					func1(  (OSADDRINFO *)handle->addr_info );
				} 
				omrsl_close_shared_library(nameHandle);
			} 
		} else {
			freeaddrinfo ((struct addrinfo *)handle->addr_info);
		}

	}

	handle->addr_info = NULL;
	handle->length = 0;		

	return 0;	
}


/**
 * Answers a list of addresses as an opaque pointer in "result".
 * 
 * Use the following functions to extract the details:
 * \arg \ref j9sock_getaddrinfo_length
 * \arg \ref j9sock_getaddrinfo_name
 * \arg \ref j9sock_getaddrinfo_address
 * \arg \ref j9sock_getaddrinfo_family
 *
 * If the machine type supports IPv6 you can specify how you want the results returned with the following function:
 * \arg \ref j9sock_create_getaddrinfo_hints.
 * Passing the structure into a machine with only IPv4 support will have no effect.
 *
 * @param[in] portLibrary The port library.
 * @param[in] name The name of the host in either host name format or in IPv4 or IPv6 accepted notations.
 * @param[in] hints Hints on what results are returned and how the response if formed (can be NULL for default action).
 * @param[out] result An opaque pointer to a list of results (j9addrinfo_struct must be preallocated).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note you must free the "result" structure with @ref j9sock_freeaddrinfo to free up memory.  This must be done
 * before you make a subsequent call in the same thread to this function. 
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo(struct J9PortLibrary *portLibrary, char *name, j9addrinfo_t hints, j9addrinfo_t result)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	struct j9hostent_struct j9hostent;
	uint32_t addr = 0; 
	int32_t rc   = 0;
	OSADDRINFO *ipv6_result;
	OSADDRINFO *addr_info_hints = NULL;
	int count = 0;
#if defined(J9_USE_CONNECTION_MANAGER)
	BOOLEAN firstAttempt = TRUE;
#endif

	wchar_t  unicodeBuffer[UNICODE_BUFFER_SIZE], *unicodeName;
	uintptr_t handle;
	int (WINAPI *func)(PCWSTR pNodeName, PCWSTR pServiceName, const ADDRINFOW* pHints, PADDRINFOW* ppResult);
	/* If we have the IPv6 functions available we will call them, otherwise we'll call the IPv4 function */
	if( PPG_sock_IPv6_FUNCTION_SUPPORT ) {
		if( hints != NULL ) {
			addr_info_hints = (OSADDRINFO *) hints->addr_info;
		}
		
		/* Convert the filename from UTF8 to Unicode */
		unicodeName = port_convertFromUTF8(OMRPORTLIB, name, unicodeBuffer, UNICODE_BUFFER_SIZE);
		if(NULL == unicodeName) {
			return -1;
		}	

	if (omrsl_open_shared_library("Ws2_32", &handle, TRUE) == 0) {
		if (omrsl_lookup_name(handle, "GetAddrInfoW", (uintptr_t *)&func, "ILLLL") == 0) {
			if( 0 != func( unicodeName, NULL, addr_info_hints, &ipv6_result ) ) {
				int32_t errorCode = WSAGetLastError();
				J9SOCKDEBUG( "<getaddrinfo failed, err=%d>\n", errorCode);
				omrsl_close_shared_library(handle);
				return omrerror_set_last_error(errorCode, findError(errorCode));
			} else {
				fill_addrinfo( ipv6_result, result, ADDRINFO_FLAG_UNICODE );	
			}
		} else {
			/* cast OSADDRINFO objects to addrinfo instead of addrinfoW for this call */
			rc = ipv4_getaddrinfo(portLibrary, name, addr_info_hints, &ipv6_result, 0 );
			if(0 != rc) {
				omrsl_close_shared_library(handle);
				return rc;					
			} else {
				fill_addrinfo( ipv6_result, result, 0 ); 
			}
		}
		omrsl_close_shared_library(handle);
	} else {
		/* cast OSADDRINFO objects to addrinfo instead of addrinfoW for this call */
		rc = ipv4_getaddrinfo(portLibrary, name, addr_info_hints, &ipv6_result, 0 );
		if(0 != rc) {
			return rc;					
		} else {
			fill_addrinfo( ipv6_result, result, 0 );
		}
	}
	} else {

		if(0 != portLibrary->sock_inetaddr(portLibrary, name, &addr)) {
			if(0 != (rc = portLibrary->sock_gethostbyname(portLibrary, name, &j9hostent))) {
				return rc;
			}
		} else {
			if((0 != (rc = portLibrary->sock_gethostbyaddr(portLibrary, (char *)&addr, sizeof(addr), J9SOCK_AFINET, &j9hostent))) &&
			   (0 != portLibrary->sock_gethostbyname(portLibrary, name, &j9hostent))) {
				return rc;
			}
		}

		memset(result, 0, sizeof(struct j9addrinfo_struct));
		result->addr_info = (void *) j9hostent.entity;

		/* count the host names and the addresses */
		while( j9hostent.entity->h_addr_list[ count ] != 0 ) {
			count ++;
		}	
		result->length = count;

	}
	
	return 0;
}



/**
 * @internal to make code more readable, moved this often used piece into a static method
 */
static void 
fill_addrinfo( OSADDRINFO *ipv6_result, j9addrinfo_t result, int flags  ) {	
	int32_t count = 0;
	memset(result, 0, sizeof(struct j9addrinfo_struct));
	result->addr_info = (void *) ipv6_result;
#if defined(WIN32) 
	result->flags = flags;
#endif
	while( ipv6_result->ai_next != NULL ) {
		count++;
		ipv6_result = ipv6_result->ai_next;
	}            
	result->length = ++count; /* Have to add an extra, because we didn't count the first entry */
	
}


/**
 * @internal to make code more readable
 */
static int32_t
ipv4_getaddrinfo( struct J9PortLibrary *portLibrary, char *name, const OSADDRINFO *addr_info_hints, OSADDRINFO **ipv6_result, int flags )
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t result = -1;
	
#if defined(J9_USE_CONNECTION_MANAGER)
	BOOLEAN firstAttempt = TRUE;
retry:
#endif
	/* cast to addrinfo even when addrinfoW */
	result = getaddrinfo( name, NULL, (const struct addrinfo *)addr_info_hints, (struct addrinfo **)ipv6_result );
	
	if( 0 != result ) {

		int32_t errorCode = WSAGetLastError();

		J9SOCKDEBUG( "<getaddrinfo failed, err=%d>\n", errorCode);
#if defined(J9_USE_CONNECTION_MANAGER)
		if (firstAttempt && (errorCode == WSAENETDOWN || errorCode == WSAEHOSTUNREACH)) {
			firstAttempt = FALSE;
			if (ensureConnected(portLibrary)) goto retry;
		}
		firstAttempt = FALSE;
#endif
		return omrerror_set_last_error(errorCode, findError(errorCode));
		
	}
	return 0;

}


/**
 * Answers a uint8_t array representing the address at "index" in the structure returned from @ref j9sock_getaddrinfo, indexed starting at 0.
 * The address is in network byte order. 
 *
 * The address will either be 4 or 16 bytes depending on whether it is an OS_AF_INET  address or an OS_AF_INET6  address.  You can check
 * this will a call to @ref j9sock_getaddrinfo_family.  Therefore you should either check the family type before preallocating the "address"
 * or define it as 16 bytes.
 *
 * @param[in]   portLibrary The port library.
 * @param[in]   handle The result structure returned by @ref j9sock_getaddrinfo.
 * @param[out] address The address at "index" in a preallocated buffer.
 * @param[in]   index The address index into the structure returned by @ref j9sock_getaddrinfo.
 * @param[out] scope_id  The scope id associated with the address if applicable
 *
 * @return	
 * \arg 0, if no errors occurred, otherwise the (negative) error code
 * \arg J9PORT_ERROR_SOCKET_VALUE_NULL when we have have the old IPv4 gethostbyname call and the address indexed is out
 * of range.  This is because the address list and the host alias list are not the same length.  Just skip this entry.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_address(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, uint8_t *address, int index, uint32_t* scope_id)
{
	int32_t rc = 0;
	OSADDRINFO *addr;
    void *sock_addr;
	char ** addr_list;
    int i;
	
	/* If we have the IPv6 functions available we cast to an OSADDRINFO structure otherwise a OSHOSTENET structure */
	if( PPG_sock_IPv6_FUNCTION_SUPPORT ) {

	     addr = (OSADDRINFO *) handle->addr_info;
	     for( i=0; i<index; i++ ) {
		     addr = addr->ai_next;
         }
	     if( addr->ai_family == OS_AF_INET6 ) {			
			sock_addr = ((OSSOCKADDR_IN6 *)addr->ai_addr)->sin6_addr.s6_addr;
			memcpy( address, sock_addr, 16 );
			*scope_id =  ((OSSOCKADDR_IN6 *)addr->ai_addr)->sin6_scope_id;
         } else {
			sock_addr = &((OSSOCKADDR *)addr->ai_addr)->sin_addr .S_un.S_un_b;
			memcpy( address, sock_addr, 4 );
			*scope_id = 0;
	     }
    } else {

		/* initialize the scope id */
		*scope_id = 0;

		addr_list = ((OSHOSTENT *) handle->addr_info)->h_addr_list;
	    for( i=0; i<index; i++ ) { 
			if( addr_list[i] == NULL ) {
				return J9PORT_ERROR_SOCKET_VALUE_NULL;
            }
		}
        memcpy( address, addr_list[index], 4 );
    }
	return rc;
}



/**
 * Answers a hints structure as an opaque pointer in "result".
 * 
 * This hints structure is used to modify the results returned by a call to @ref j9sock_getaddrinfo.  There is one of
 * these structures per thread, so subsequent calls to this function will overwrite the same structure in memory.
 * The structure is cached in ptBuffers and is cleared when a call to @ref j9port_free_ptBuffer is made.
 *
 * This function is only works on IPv6 supported OS's.  If it is called on an OS that does not support IPv6 then
 * it essentially returns a NULL pointer, meaning it will have no effect on the call to @ref j9sock_getaddrinfo.
 *
 * See man pages, or MSDN doc on getaddrinfo for information on how the hints structure works.
 *
 * @param[in] portLibrary The port library.
 * @param[out] result The filled in (per thread) hints structure
 * @param[in] family A address family type
 * @param[in] socktype A socket type
 * @param[in] protocol A protocol family
 * @param[in] flags Flags for modifying the result
 *
 * @return
 * \arg 0, if no errors occurred, otherwise the (negative) error code
 * \arg J9PORT_ERROR_SOCKET_SYSTEMFULL -- when we can't allocate memory for the ptBuffers, or the hints structure
 *
 * @note current supported family types are:
 * \arg J9ADDR_FAMILY_UNSPEC
 * \arg J9ADDR_FAMILY_AFINET4
 * \arg J9ADDR_FAMILY_AFINET6
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_create_hints(struct J9PortLibrary *portLibrary, j9addrinfo_t *result, int16_t family, int32_t socktype, int32_t protocol, int32_t flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	OSADDRINFO *addrinfo;
	J9SocketPTB *ptBuffers = NULL;
	*result = NULL;

#define addrinfohints (ptBuffers->addr_info_hints).addr_info

	/* If we have the IPv6 functions available we fill in the structure, otherwise it is null */
	if( PPG_sock_IPv6_FUNCTION_SUPPORT ) {
		ptBuffers = j9sock_ptb_get(portLibrary);
		if (NULL == ptBuffers) {
			return J9PORT_ERROR_SOCKET_SYSTEMFULL;
		}

		if (!addrinfohints) {
			addrinfohints = omrmem_allocate_memory(sizeof(OSADDRINFO), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (!addrinfohints) {
				return J9PORT_ERROR_SOCKET_SYSTEMFULL;
			}
		}
		memset(addrinfohints, 0, sizeof(OSADDRINFO));

		addrinfo = (OSADDRINFO*) addrinfohints;
		addrinfo->ai_flags = flags;
		addrinfo->ai_family = map_addr_family_J9_to_OS( family );
		addrinfo->ai_socktype = map_sockettype_J9_to_OS( socktype );
		addrinfo->ai_protocol = map_protocol_family_J9_to_OS( protocol );
		*result = &ptBuffers->addr_info_hints;
    }

#undef addrinfohints

	return rc;
}



/**
 * Answers the family type of the address at "index" in the structure returned from @ref j9sock_getaddrinfo, indexed starting at 0.
 *
 * Currently the family types we support are J9SOCK_AFINET and J9SOCK_AFINET6.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref j9sock_getaddrinfo.
 * @param[out] family The family at "index".
 * @param[in] index The address index into the structure returned by @ref j9sock_getaddrinfo.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_family(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, int32_t *family, int index)
{
	int32_t rc = 0;
	OSADDRINFO *addr;
    int i;


	/* If we have the IPv6 functions then we'll cast to a OSADDRINFO otherwise we have a hostent */
	if( PPG_sock_IPv6_FUNCTION_SUPPORT ) {

	     addr = (OSADDRINFO *) handle->addr_info;
	     for( i=0; i<index; i++ ) {
		     addr = addr->ai_next;
         }

		if( addr->ai_family == OS_AF_INET4 ) {
			*family = J9ADDR_FAMILY_AFINET4;
		} else {
			*family = J9ADDR_FAMILY_AFINET6;
		}
    } else {

		*family = J9ADDR_FAMILY_AFINET4;

    }

	return rc;
}


/**
 * Answers the number of results returned from @ref j9sock_getaddrinfo.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref j9sock_getaddrinfo.
 * @param[out] length The number of results.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_length(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, int32_t *length)
{
	*length = handle->length;
	return 0;
}


/**
 * Answers the cannon name of the address at "index" in the structure returned from @ref j9sock_getaddrinfo, indexed starting at 0.
 * 
 * The preallocated buffer for "name" should be the size of the maximum host name: OSNIMAXHOST.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle The result structure returned by @ref j9sock_getaddrinfo.
 * @param[out] name The name of the address at "index" in a preallocated buffer.
 * @param[in] index The address index into the structure returned by @ref j9sock_getaddrinfo.
 *
 * @return
 * \arg 0, if no errors occurred, otherwise the (negative) error code.
 * \arg J9PORT_ERROR_SOCKET_VALUE_NULL when we have have the old IPv4 gethostbyname call and the name indexed is out
 * of range.  This is because the address list and the host alias list are not the same length.  Just skip this entry.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_getaddrinfo_name(struct J9PortLibrary *portLibrary, j9addrinfo_t handle, char *name, int index)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	char ** alias_list;
    int i;
	OSADDRINFO *addr;

	/* If we have the IPv6 functions available we cast to an OSADDRINFO structure otherwise a OSHOSTENET structure */
	if( PPG_sock_IPv6_FUNCTION_SUPPORT ) {

	     addr = (OSADDRINFO *) handle->addr_info;
	     for( i=0; i<index; i++ ) {
		     addr = addr->ai_next;
         }		
		 if( addr->ai_canonname == NULL ) {
			name[0] = 0;
		 } else {
			if (ADDRINFO_FLAG_UNICODE & handle->flags) {
				port_convertToUTF8(OMRPORTLIB, (const wchar_t*) addr->ai_canonname, name, OSNIMAXHOST);
			} else {
				strcpy( name, (char *) addr->ai_canonname );
			}
		 }
    } else {

		alias_list = ((OSHOSTENT *) handle->addr_info)->h_aliases;
	    for( i=0; i<index; i++ ) { 
			if( alias_list[i] == NULL ) {
				return J9PORT_ERROR_SOCKET_VALUE_NULL;
            }
		}
		if( alias_list[index] == NULL ) {
			name[0] = 0;
		} else {
			strcpy( name, alias_list[index] );
		}

    }

	return rc;
}


/**
 * Answer information on the host referred to by the address.  The information includes name, aliases and
 * addresses for the nominated host (the latter being relevant on multi-homed hosts).
 * This call has only been tested for addresses of type AF_INET.
 *
 * @param[in] portLibrary The port library.
 * @param[in] addr Pointer to the binary-format (not null-terminated) address, in network byte order.
 * @param[in] length Length of the addr.
 * @param[in] type The type of the addr.
 * @param[out] handle Pointer to the j9hostent_struct, to be linked to the per thread platform hostent struct.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_gethostbyaddr(struct J9PortLibrary *portLibrary, char *addr, int32_t length, int32_t type, j9hostent_t handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	OSHOSTENT *result;
#if defined(J9_USE_CONNECTION_MANAGER)
	BOOLEAN firstAttempt = TRUE;
	
retry:
#endif
	result = gethostbyaddr(addr, length, type);
	if(result ==  0) {
		int32_t errorCode = WSAGetLastError();

		J9SOCKDEBUG( "<gethostbyaddr failed, err=%d>\n", errorCode);
#if defined(J9_USE_CONNECTION_MANAGER)
		if (firstAttempt && (errorCode == WSAENETDOWN || errorCode == WSAEHOSTUNREACH)) {
			firstAttempt = FALSE;
			if (ensureConnected(portLibrary)) goto retry;
		}
		firstAttempt = FALSE;
#endif
		return omrerror_set_last_error(errorCode, findError(errorCode));
	} else {
		memset(handle, 0, sizeof(struct j9hostent_struct));
		handle->entity = result;
	}
	return 0;
}


/**
 * Answer information on the host, specified by name.  The information includes host name,
 * aliases and addresses.
 *
 * @param[in] portLibrary The port library.
 * @param[in] name The host name string.
 * @param[out] handle Pointer to the j9hostent_struct (to be linked to the per thread platform hostent struct).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_gethostbyname(struct J9PortLibrary *portLibrary, const char *name, j9hostent_t handle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	OSHOSTENT *result;
#if defined(J9_USE_CONNECTION_MANAGER)
	BOOLEAN firstAttempt = TRUE;


retry:
#endif
	result = gethostbyname(name);
	if(result ==  0) {
		int32_t errorCode = WSAGetLastError();
		J9SOCKDEBUG( "<gethostbyname failed, err=%d>\n", errorCode);
#if defined(J9_USE_CONNECTION_MANAGER)
		if (firstAttempt && (errorCode == WSAENETDOWN || errorCode == WSAEHOSTUNREACH)) {
			firstAttempt = FALSE;
			if (ensureConnected(portLibrary)) goto retry;
		}
		firstAttempt = FALSE;
#endif
		return omrerror_set_last_error(errorCode, findError(errorCode));
	} else {
		memset(handle, 0, sizeof(struct j9hostent_struct));
		handle->entity = result;
	}
	return 0;
}


/**
 * Answer the name of the local host machine.
 *
 * @param[in] portLibrary The port library.
 * @param[in,out] buffer The string buffer to receive the name
 * @param[in] length The length of the buffer
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 */
int32_t
j9sock_gethostname(struct J9PortLibrary *portLibrary, char *buffer, int32_t length)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
 	if (0 != (gethostname(buffer, length))) {
		int32_t errorCode = WSAGetLastError();

		J9SOCKDEBUG( "<gethostname failed, err=%d>\n", errorCode);						
		return omrerror_set_last_error(errorCode, findError(errorCode));
	} 
	return 0;
}


/**
 * Answers the host name of the "in_addr" in a preallocated buffer.
 *
 * The preallocated buffer for "name" should be the size of the maximum host name: OSNIMAXHOST.
 * Currently only AF_INET and AF_INET6 are supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] in_addr The address we want to do a name lookup on
 * @param[in] sockaddr_size The size of "in_addr"
 * @param[out] name The hostname of the passed address in a preallocated buffer.
 * @param[in] name_length The length of the buffer pointed to by name
 * @param[in] flags Flags on how to form the response (see man pages or doc for getnameinfo)
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 *
 * @note Added for IPv6 support.
 * @note "flags" do not affect results on OS's that do not support the IPv6 calls.
 */
int32_t
j9sock_getnameinfo(struct J9PortLibrary *portLibrary, j9sockaddr_t in_addr, int32_t sockaddr_size, char *name, int32_t name_length, int flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	OSHOSTENT *result;
	OSSOCKADDR *addr;
	int size;
	int rc=0;
#if defined(J9_USE_CONNECTION_MANAGER)
	BOOLEAN firstAttempt = TRUE;
#endif

	/* If we have the IPv6 functions available we will call them, otherwise we'll call the IPv4 function */
	if( PPG_sock_IPv6_FUNCTION_SUPPORT ) {

		/* Windows code requires that the sockaddr structure be of the right type, rather than just checking to see if it is large enough */
		addr = (OSSOCKADDR *) &in_addr->addr;
		if( addr->sin_family == OS_AF_INET4) {
				sockaddr_size = sizeof( OSSOCKADDR );
		} else {
				sockaddr_size = sizeof( OSSOCKADDR_IN6 );
		}
#if defined(J9_USE_CONNECTION_MANAGER)
retryInfo:
#endif
		/* Bugzilla 124481: We should change this to use GetAddrInfoW on platforms that support it. */
		rc = getnameinfo(  (OSADDR *) &in_addr->addr, sockaddr_size, name, name_length, NULL, 0, flags );
		if(rc !=  0) {
			int32_t errorCode = WSAGetLastError();

			J9SOCKDEBUG( "<gethostbyaddr failed, err=%d>\n", errorCode);						
#if defined(J9_USE_CONNECTION_MANAGER)
		if (firstAttempt && (errorCode == WSAENETDOWN || errorCode == WSAEHOSTUNREACH)) {
			firstAttempt = FALSE;
			if (ensureConnected(portLibrary)) goto retryInfo;
		}
		firstAttempt = FALSE;
#endif
			return omrerror_set_last_error(errorCode, findError(errorCode));
		} 		
	} else {          /* IPv4 call */

		addr = (OSSOCKADDR *) &in_addr->addr;
		if( addr->sin_family == OS_AF_INET4) {
				size = 4;
		} else {
				size = 16;
		}
#if defined(J9_USE_CONNECTION_MANAGER)
retryHost:
#endif
		result = gethostbyaddr( (char *) &addr->sin_addr, size, addr->sin_family );
		if(result ==  0) {
			int32_t errorCode = WSAGetLastError();

			J9SOCKDEBUG( "<gethostbyaddr failed, err=%d>\n", errorCode);						
#if defined(J9_USE_CONNECTION_MANAGER)
		if (firstAttempt && (errorCode == WSAENETDOWN || errorCode == WSAEHOSTUNREACH)) {
			firstAttempt = FALSE;
			if (ensureConnected(portLibrary)) goto retryHost;
		}
		firstAttempt = FALSE;
#endif
			return omrerror_set_last_error(errorCode, findError(errorCode));
		} else {
			strcpy( name, result->h_name );
		}
	}
	return rc;
}


/**
 * Answer the value of the nominated boolean socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value.
 * @param[in] optlevel	 The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to retrieve.
 * @param[out] optval Pointer to the boolean to update with the option value.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getopt_bool(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  BOOLEAN *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	BOOL option = FALSE;
	int32_t optlen = sizeof(option);
	int32_t rc = 0;

	if(0 > platformLevel) {
		return platformLevel;
	}
	if(0 > platformOption) {
		return platformOption;
	}

	/* If both sockets are open we only need to query the IPv4 option as will both be set to the same value */

	if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
 		rc = getsockopt(socketP->ipv4, platformLevel, platformOption, (char*)&option, &optlen);
	} 
	else if(socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
		if(platformOption == IP_MULTICAST_LOOP) {
			platformLevel = IPPROTO_IPV6;
			platformOption = IPV6_MULTICAST_LOOP;
		}
 		rc = getsockopt(socketP->ipv6, platformLevel, platformOption, (char*)&option, &optlen);
	}

	if( rc != 0 ) {
		int32_t errorCode = WSAGetLastError();

		J9SOCKDEBUG( "<getsockopt (for bool) failed, err=%d>\n", errorCode);						
		return omrerror_set_last_error(errorCode, findError(errorCode));
	}
	*optval = (BOOLEAN)option;
	return 0;
}


/**
 * Answer the value of the nominated byte socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value.
 * @param[in] optlevel	 The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to retrieve.
 * @param[out] optval Pointer to the byte to update with the option value.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getopt_byte(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  uint8_t *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	uint32_t optTemp = 0;
	int32_t optlen = sizeof(optTemp);
	int32_t rc = 0;

	if(0 > platformLevel) {
		return platformLevel;
	}
	if(0 > platformOption){
		return platformOption;
	}

	/* If both sockets are open we only need to query the IPv4 option as will both be set to the same value */

	if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
 		rc = getsockopt( socketP->ipv4, platformLevel, platformOption, (char*)&optTemp, &optlen);
	} 
else if(socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
		if (platformOption == IP_MULTICAST_TTL) {
			platformLevel = IPPROTO_IPV6;
			platformOption = IPV6_MULTICAST_HOPS;
		}
 		rc = getsockopt( socketP->ipv6, platformLevel, platformOption, (char*)&optTemp, &optlen);
	}

	if( rc != 0 ) {
		int32_t errorCode = WSAGetLastError();

		J9SOCKDEBUG( "<getsockopt (for byte) failed, err=%d>\n", errorCode);
		return omrerror_set_last_error(errorCode, findError(errorCode));
	} else {
		*optval = (0xFF & optTemp);
	}
	return 0;
}


/**
 * Answer the value of the nominated integer socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to retrieve.
 * @param[out] optval Pointer to the integer to update with the option value.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getopt_int(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  int32_t *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	int32_t optlen = sizeof(*optval);
	int32_t rc = 0;

	if(0 > platformLevel) {
		return platformLevel;
	}
	if(0 > platformOption) {
		return platformOption;
	}

	/* If both sockets are open we only need to query the IPv4 option as will both be set to the same value */
	 /* unless the option is at the IPV6 proto level in which case we have to look at the IPV6 socket */
	if(( socketP->flags & SOCKET_IPV4_OPEN_MASK ) &&(platformLevel != OS_IPPROTO_IPV6)) {
 	 	rc = getsockopt(socketP->ipv4, platformLevel, platformOption, (char*)optval, &optlen);
	} 

else if(socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
 	 	rc = getsockopt(socketP->ipv6, platformLevel, platformOption, (char*)optval, &optlen);
	}
	if (0 != rc) {
		int32_t errorCode = WSAGetLastError();
		J9SOCKDEBUG( "<getsockopt (for int) failed, err=%d>\n", errorCode);
		return omrerror_set_last_error(errorCode, findError(errorCode));

	}
	return 0;
}


/**
 * Answer the value of the socket linger option.
 * See the @ref j9sock_linger_init for details of the linger behavior.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value
 * @param[in] optlevel The level within the IP stack at which the option is defined
 * @param[in] optname The name of the option to retrieve
 * @param[out] optval Pointer to the linger struct to update with the option value
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 */
int32_t
j9sock_getopt_linger(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9linger_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	int32_t optlen = sizeof(optval->linger);
	int32_t rc = 0;

	if(0 > platformLevel) {
		return platformLevel;
	}
	if(0 > platformOption) {
		return platformOption;
	}

	/* If both sockets are open we only need to query the IPv4 option as will both be set to the same value */

	if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
		rc = getsockopt( socketP->ipv4, platformLevel, platformOption, (char*)(&optval->linger), &optlen);
	} else if(socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
		rc = getsockopt( socketP->ipv6, platformLevel, platformOption, (char*)(&optval->linger), &optlen);
	}
	if (0 != rc) {
		int32_t errorCode = WSAGetLastError();

		J9SOCKDEBUG( "<getsockopt (for linger) failed, err=%d>\n", errorCode);
		return omrerror_set_last_error(errorCode, findError(errorCode));
	}
	return 0;
}


/**
 * Answer the value of the socket option, an address struct.
 * Currently only used to retrieve the interface of multicast sockets, 
 * but the more general call style has been used.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to query for the option value.
 * @param[in] optlevel	 The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to retrieve.
 * @param[out] optval Pointer to the sockaddr struct to update with the option value.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getopt_sockaddr(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname, j9sockaddr_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t platformLevel = platformSocketLevel( optlevel );
	int32_t platformOption = platformSocketOption( optname );
	int32_t optlen = sizeof(((OSSOCKADDR*)&optval->addr)->sin_addr);
	int32_t rc = 0;

	if(0 > platformLevel) {
		return platformLevel;
	}
	if(0 > platformOption) {
		return platformOption;
	}

		/* If both sockets are open we only need to query the IPv4 option as will both be set to the same value */
		if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
			rc = getsockopt( socketP->ipv4, platformLevel, platformOption, (char*)&((OSSOCKADDR*)&optval->addr)->sin_addr, &optlen);
		} else if(socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
			rc = getsockopt( socketP->ipv6, platformLevel, platformOption, (char*)&((OSSOCKADDR*)&optval->addr)->sin_addr, &optlen);
		}

		if (0 != rc) {
			int32_t errorCode = WSAGetLastError();

			J9SOCKDEBUG( "<getsockopt (for sockaddr) failed, err=%d>\n",  errorCode);
			return omrerror_set_last_error(errorCode, findError(errorCode));
		}

	return 0;
}


/**
 * Answer the remote name for the socket.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the socket to get the address of.
 * @param[out] addrHandle Pointer to the sockaddr struct to update with the address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getpeername(struct J9PortLibrary *portLibrary, j9socket_t handle, j9sockaddr_t addrHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t addrlen = sizeof(addrHandle->addr);

	if( handle->flags & SOCKET_IPV4_OPEN_MASK || !(handle->flags & SOCKET_IPV6_OPEN_MASK) ) {
		rc = getpeername( handle->ipv4, (struct sockaddr *) &addrHandle->addr, &addrlen );
	} else {
		rc = getpeername( handle->ipv6, (struct sockaddr *) &addrHandle->addr, &addrlen );
	}

	if( rc != 0 )	{
		rc = WSAGetLastError();
		J9SOCKDEBUG( "<getpeername failed, err=%d>\n", rc);
		switch(rc) 	{
			case WSAEINVAL:
				return omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NOTBOUND);
			default:
				return omrerror_set_last_error(rc, findError(rc));
		}
	}
	return rc;
}


/**
 * Answer the local name for the socket.  Note, the stack getsockname function
 * actually answers a sockaddr structure, not a string name as the function name
 * might imply.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the socket to get the address of.
 * @param[out] addrHandle Pointer to the sockaddr struct to update with the address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_getsockname(struct J9PortLibrary *portLibrary, j9socket_t handle, j9sockaddr_t addrHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t addrlen = sizeof(addrHandle->addr);


	if( handle->flags & SOCKET_IPV4_OPEN_MASK || !(handle->flags & SOCKET_IPV6_OPEN_MASK) ) {
	 	rc = getsockname( handle->ipv4, (struct sockaddr *) &addrHandle->addr, &addrlen);
	} else {
	 	rc = getsockname( handle->ipv6, (struct sockaddr *) &addrHandle->addr, &addrlen);
	}
	
	if( rc != 0) {
		rc = WSAGetLastError();
		J9SOCKDEBUG( "<getsockname failed, err=%d>\n", rc);
		switch(rc) {
			case WSAEINVAL:
				return omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NOTBOUND);
			default:
				return omrerror_set_last_error(rc, findError(rc));
		}
	}

	/* if both sockets are open we cannot return the address for either one as whichever one we return it is wrong in some 
	   cases. Therefore,  we reset the address to the ANY address and leave the port as is as it should be the same
       for both sockets (bind makes sure that when we open the two sockets we use the same port */
	if ((handle->flags & SOCKET_IPV4_OPEN_MASK) && (handle->flags & SOCKET_IPV6_OPEN_MASK)) {
		/* we know the address is any IPv4 as the IPv4 socket was used if both were open */
		(( struct sockaddr_in *) &addrHandle->addr)->sin_addr.S_un.S_addr = 0;
	}

	return rc;
}


/**
 * Answer the nominated element of the address list within the argument hostent struct.
 * The address is in network order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the hostent struct, in which to access the addr_list.
 * @param[in] index The index of the element within the addr_list to retrieve.
 *
 * @return	the address, in network order.
 */
int32_t
j9sock_hostent_addrlist(struct J9PortLibrary *portLibrary, j9hostent_t handle, uint32_t index)
{
	return 	*( (int32_t*)handle->entity->h_addr_list[index] );
}


/**
 * Answer a reference to the list of alternative names for the host within the argument hostent struct.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the hostent struct, in which to access the addr_list
 * @param[out] aliasList Pointer to the list of alternative names, to be updated 
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 */
int32_t
j9sock_hostent_aliaslist(struct J9PortLibrary *portLibrary, j9hostent_t handle, char ***aliasList)
{
	*aliasList = handle->entity->h_addr_list;
	return 0;
}


/**
 * Answer the host name (string) within the argument hostent struct.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the hostent struct, in which to access the hostName.
 * @param[out] hostName Host name string.
 *
 * @return	0, the function does not validate the name access.
 */
int32_t
j9sock_hostent_hostname(struct J9PortLibrary *portLibrary, j9hostent_t handle, char** hostName)
{
	*hostName = handle->entity->h_name;
	return 0;
}


/**
 * Answer the 32 bit host ordered argument, in network byte order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] val The 32 bit host ordered number.
 *
 * @return	the 32 bit network ordered number.
 */
int32_t
j9sock_htonl(struct J9PortLibrary *portLibrary, int32_t val)
{
	return htonl(val);
}


/**
 * Answer the 16 bit host ordered argument, in network byte order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] val The 16 bit host ordered number.
 *
 * @return	the 16 bit network ordered number.
 */
uint16_t
j9sock_htons(struct J9PortLibrary *portLibrary, uint16_t val)
{
	return htons(val);
}


/**
 * Answer the dotted IP string as an Internet address.
 *
 * @param[in] portLibrary The port library.
 * @param[out] addrStr The dotted IP string.
 * @param[in] addr Pointer to the Internet address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_inetaddr(struct J9PortLibrary *portLibrary, const char *addrStr, uint32_t *addr)
{
	int32_t rc = 0;
	uint32_t val;

	val = inet_addr(addrStr);
	if (INADDR_NONE == val) {
		J9SOCKDEBUGPRINT( "<inet_addr failed>\n" );
		rc = J9PORT_ERROR_SOCKET_ADDRNOTAVAIL;
	} else {
		*addr = val;
	}
	return rc;
}


/**
 * Answer the Internet address as a dotted IP string.
 *
 * @param[in] portLibrary The port library.
 * @param[out] addrStr The dotted IP string.
 * @param[in] nipAddr The Internet address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_inetntoa(struct J9PortLibrary *portLibrary, char **addrStr, uint32_t nipAddr)
{
	int32_t rc = 0;
	char* val;
	struct in_addr addr;
	addr.s_addr = nipAddr;
	val = inet_ntoa(addr);
	if (NULL == val) {
		J9SOCKDEBUGPRINT( "<inet_ntoa failed>\n" );
		rc = J9PORT_ERROR_SOCKET_ADDRNOTAVAIL;
	} else {
		*addrStr = val;
	}
	return rc;
}


/**
 * Initializes a new multicast membership structure.  The membership structure is used to join & leave
 * multicast groups @see j9sock_setopt_ipmreq.  The group may be joined using 0 (J9SOCK_INADDR_ANY)
 * as the local interface, in which case the default local address will be used.
 *
 * @param[in] portLibrary The port library.
 * @param[out] handle Pointer to the multicast membership struct.
 * @param[in] nipmcast The address, in network order, of the multicast group to join.
 * @param[in] nipinterface The address, in network order, of the local machine interface to join on.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_ipmreq_init(struct J9PortLibrary *portLibrary, j9ipmreq_t handle, uint32_t nipmcast, uint32_t nipinterface)
{
	memset(handle, 0, sizeof(struct j9ipmreq_struct));
	handle->addrpair.imr_multiaddr.s_addr = nipmcast;
	handle->addrpair.imr_interface.s_addr = nipinterface;

	return 0;
}


/**
 * Fills in a preallocated j9ipv6_mreq_struct
 *
 * @param[in] portLibrary The port library.
 * @param[out] handle A pointer to the j9ipv6_mreq_struct to populate.
 * @param[in] ipmcast_addr The ip mulitcast address.
 * @param[in] ipv6mr_interface The ip mulitcast interface.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_ipv6_mreq_init(struct J9PortLibrary *portLibrary, j9ipv6_mreq_t handle, uint8_t *ipmcast_addr, uint32_t ipv6mr_interface)
{
	memset(handle, 0, sizeof(struct j9ipmreq_struct));
	memcpy( handle->mreq.ipv6mr_multiaddr.u.Byte, ipmcast_addr, 16 );
	handle->mreq.ipv6mr_interface = ipv6mr_interface;
	return 0;
}


/**
 * Answer true if the linger is enabled in the argument linger struct.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the linger struct to be accessed.
 * @param[out] enabled Pointer to the boolean to be updated with the linger status.
 *
 * @return	0, the function does not validate the access.
 */
int32_t
j9sock_linger_enabled(struct J9PortLibrary *portLibrary, j9linger_t handle, BOOLEAN *enabled)
{
	*enabled = (BOOLEAN)(handle->linger.l_onoff);
	return 0;
}


/**
 * Initializes a new linger structure, enabled or disabled, with the timeout as specified.
 * Linger defines the behavior when unsent messages exist for a socket that has been sent close.
 * If linger is disabled, the default, close returns immediately and the stack attempts to deliver unsent messages.
 * If linger is enabled:
 * \arg if the timeout is 0, the close will block indefinitely until the messages are sent
 * \arg if the timeout is set, the close will return after the messages are sent or the timeout period expired
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the linger struct to be accessed.
 * @param[in] enabled Aero to disable, a non-zero value to enable linger.
 * @param[in] timeout	 0 to linger indefinitely or a positive timeout value (in seconds).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_linger_init(struct J9PortLibrary *portLibrary, j9linger_t handle, int32_t enabled, uint16_t timeout)
{
	memset(handle, 0, sizeof (struct j9linger_struct));
	handle->linger.l_onoff = enabled;
	handle->linger.l_linger = timeout;
	return 0;
}


/**
 * Answer the linger timeout value in the argument linger struct.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the linger struct to be accessed.
 * @param[out] linger Pointer to the integer, to be updated with the linger value (in seconds).
 *
 * @return	0, the function does not validate the access.
 */
int32_t
j9sock_linger_linger(struct J9PortLibrary *portLibrary, j9linger_t handle, uint16_t *linger)
{
	*linger = handle->linger.l_linger;
	return 0;
}


/**
 * Set the socket to listen for incoming connection requests.  This call is made prior to accepting requests,
 * via the @ref j9sock_accept function.  The backlog specifies the maximum length of the queue of pending connections,
 * after which further requests are rejected.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to modify.
 * @param[in] backlog The maximum number of queued requests.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
/*IPv6 - If both the IPv4 and IPv6 sockets are still open, then we need to listen on both of them.  
 */
int32_t
j9sock_listen(struct J9PortLibrary *portLibrary, j9socket_t sock, int32_t backlog)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;

	/* Listen on the IPv4 socket */
	if( sock->flags & SOCKET_IPV4_OPEN_MASK ) {
	 	if(listen( sock->ipv4, backlog ) == SOCKET_ERROR ) {
			rc = WSAGetLastError();
			J9SOCKDEBUG( "<listen failed, err=%d>\n", rc);
			switch(rc) {
					case WSAEINVAL:
						rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_BOUNDORCONN);
						break;
					default:
						rc = omrerror_set_last_error(rc, findError(rc));
			}
	    }
	}

	/* Listen on the IPv6 socket providing our return code is good */
	if( sock->flags & SOCKET_IPV6_OPEN_MASK && rc == 0 ) {
	 	if(listen( sock->ipv6, backlog ) == SOCKET_ERROR ) {
			rc = WSAGetLastError();
			J9SOCKDEBUG( "<listen failed, err=%d>\n", rc);
			switch(rc) {
					case WSAEINVAL:
						rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_BOUNDORCONN);
						break;
					default:
						rc = omrerror_set_last_error(rc, findError(rc));
			}
	    }
	}

	return rc;
}


/**
 * Answer the 32 bit network ordered argument, in host byte order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] val The 32 bit network ordered number.
 *
 * @return	the 32 bit host ordered number.
 */
int32_t
j9sock_ntohl(struct J9PortLibrary *portLibrary, int32_t val)
{
	return ntohl(val);
}


/**
 * Answer the 16-bit network ordered argument, in host byte order.
 *
 * @param[in] portLibrary The port library.
 * @param[in] val The 16-bit network ordered number.
 *
 * @return	the 16-bit host ordered number.
 */
uint16_t
j9sock_ntohs(struct J9PortLibrary *portLibrary, uint16_t val)
{
	return ntohs(val);
}

int32_t
j9sock_read(struct J9PortLibrary *portLibrary, j9socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t bytesRec = 0;
	int	socketTypeLen = sizeof(DWORD);
		Trc_PRT_sock_j9sock_read_Entry(sock, nbyte, flags);
		if( sock->flags & SOCKET_USE_IPV4_MASK || !(sock->flags & SOCKET_IPV6_OPEN_MASK) ) {
			bytesRec = recv( sock->ipv4, (char *)buf, nbyte, flags) ;
		} else {  /* If IPv6 is open */
			bytesRec = recv( sock->ipv6, (char *)buf, nbyte, flags) ;		
		}

	if(SOCKET_ERROR == bytesRec) {
		rc = WSAGetLastError();
		J9SOCKDEBUG( "<recv failed, err=%d>\n", rc);
		switch(rc) {
				case WSAEINVAL:
					Trc_PRT_sock_j9sock_read_failure(rc);
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NOTBOUND);
					break;
				case WSAEMSGSIZE:
					rc = omrerror_set_last_error(rc, WSAEMSGSIZE);
					/* [PR 115973] - Reference implementation truncates, does not throw SocketException in connected UDP case */
					rc = nbyte;
					break;
				default:
					Trc_PRT_sock_j9sock_read_failure(rc);
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}
	} else {
		rc = bytesRec;
	}
	
	Trc_PRT_sock_j9sock_read_Exit(rc);
	return rc;
}

/**
 * The read function receives data from a possibly connected socket.  Calling read will return as much 
 * information as is currently available up to the size of the buffer supplied.  If the information is too large
 * for the buffer, the excess will be discarded.  If no incoming  data is available at the socket, the read call 
 * blocks and waits for data to arrive.  It the address argument is not null, the address will be updated with
 * address of the message sender.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to read on.
 * @param[out] buf Pointer to the buffer where input bytes are written.
 * @param[in] nbyte The length of buf.
 * @param[in] flags Tthe flags, to influence this read.
 * @param[out] addrHandle	if provided, the address to be updated with the sender information.
 *
 * @return
 * \arg If no error occurs, return the number of bytes received.
 * \arg If the connection has been gracefully closed, return 0.
 * \arg Otherwise return the (negative) error code.
 */
int32_t
j9sock_readfrom(struct J9PortLibrary *portLibrary, j9socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, j9sockaddr_t addrHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t bytesRec = 0;
	int32_t addrlen;
	
	Trc_PRT_sock_j9sock_readfrom_Entry(sock, nbyte, flags, addrHandle);
	
	if(NULL == addrHandle) {
		addrlen = sizeof(*addrHandle);

		if( sock->flags & SOCKET_USE_IPV4_MASK || !(sock->flags & SOCKET_IPV6_OPEN_MASK) ) {
			bytesRec = recvfrom( sock->ipv4, (char *)buf, nbyte, flags, (struct sockaddr *) NULL, &addrlen);
		} else {  /* If IPv6 is open */
			bytesRec = recvfrom(sock->ipv6, (char *)buf, nbyte, flags, (struct sockaddr *) NULL, &addrlen);
		}
	} else {
		addrlen = sizeof(addrHandle->addr);
		if( sock->flags & SOCKET_USE_IPV4_MASK || !(sock->flags & SOCKET_IPV6_OPEN_MASK) ) {
			((OSSOCKADDR*)&addrHandle->addr)->sin_family = OS_AF_INET4;
			bytesRec = recvfrom(sock->ipv4, (char *)buf, nbyte, flags, (struct sockaddr *) &addrHandle->addr, &addrlen);
		} 
		else {  /* If IPv6 is open */
			((OSSOCKADDR*)&addrHandle->addr)->sin_family = OS_AF_INET6;
			bytesRec = recvfrom(sock->ipv6, (char *)buf, nbyte, flags, (struct sockaddr *) &addrHandle->addr, &addrlen);
		}
	}
	if(SOCKET_ERROR == bytesRec) {
		rc = WSAGetLastError();
		switch(rc) {
				case WSAEINVAL:
					Trc_PRT_sock_j9sock_readfrom_failure(rc);
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NOTBOUND);
					break;
				case WSAEMSGSIZE:
					rc = omrerror_set_last_error(rc, WSAEMSGSIZE);
					rc = nbyte;
					break;
				default:
					Trc_PRT_sock_j9sock_readfrom_failure(rc);
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}
	} else {
		rc = bytesRec;
	}
	
	Trc_PRT_sock_j9sock_readfrom_Exit(rc);
	return rc;
}


/**
 * The select function allows the state of sockets for read & write operations and exceptional conditions to be tested.
 * The function is used prior to a j9sock_read/readfrom, to control the period the operation may block for.
 * Depending upon the timeout specified:
 * \arg 0, return immediately with the status of the descriptors
 * \arg timeout, return when one of the descriptors is ready or after the timeout period has expired
 * \arg null, block indefinitely for a ready descriptor
 *
 * Note 1: j9fdset_t does not support multiple entries.
 * Note 2: one of readfd or writefd must contain a valid entry.
 *
 * @param[in] portLibrary The port library.
 * @param[in] nfds For all the sockets that are set in readfds, writefds and exceptfds, this is the maximum value returned
 *            by @ref j9sock_fdset_size().
 * @param[in] readfd The j9fdset_t representing the descriptor to be checked to see if data can be read without blocking.
 * @param[in] writefd The j9fdset_t representing the descriptor to be checked to see if data can be written without blocking.
 * @param[in] exceptfd_notSupported Not supported, must either by NULL or empty
 * @param[in] timeout Pointer to the timeout (a j9timeval struct).
 *
 * @return	Number of socket descriptors that are ready, otherwise return the (negative) error code.
 */
int32_t
j9sock_select(struct J9PortLibrary *portLibrary, int32_t nfds, j9fdset_t readfd, j9fdset_t writefd, j9fdset_t exceptfd_notSupported, j9timeval_t timeout)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t result = 0;

	Trc_PRT_sock_j9sock_select_Entry(nfds, readfd, writefd, exceptfd_notSupported, timeout == NULL ? 0 : timeout->time.tv_sec, timeout == NULL ? 0 : timeout->time.tv_usec);
	
	if (NULL != exceptfd_notSupported) {
		if (0 != &exceptfd_notSupported->handle.fd_count) {
			rc = omrerror_set_last_error_with_message(J9PORT_ERROR_SOCKET_ARGSINVALID, "exceptfd_notSupported cannot contain a valid fd");
			Trc_PRT_sock_j9sock_select_Exit(rc);
			return rc;
		}
	}

	/* NOTE: empirical evidence indicates that select() on windows always returns zero if timeout is set to zero */
	if (NULL == timeout) {
		result = select( nfds, &readfd->handle, &writefd->handle, NULL, NULL);
	} else {
		result = select( nfds, &readfd->handle, &writefd->handle, NULL, &timeout->time);
	}
	if(SOCKET_ERROR == result) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_select_failure(rc);
		J9SOCKDEBUG( "<select failed, err=%d>\n", rc);
		switch(rc) {
				case WSAEINVAL:
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_INVALIDTIMEOUT);
					break;
				default:
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}		
	} else {
		if(0 == result) {
			rc = J9PORT_ERROR_SOCKET_TIMEOUT;
			Trc_PRT_sock_j9sock_select_timeout();
		} else {
			rc = result;
		}
	}
	
	Trc_PRT_sock_j9sock_select_Exit(rc);
	return rc;
}


int32_t
j9sock_select_read(struct J9PortLibrary *portLibrary, j9socket_t j9socketP, int32_t secTime, int32_t uSecTime, BOOLEAN isNonBlocking)
{
	/* IPv6 - If both IPv4 and IPv6 sockets are open, the call to this function 
	 * will alternate selecting between the sockets.
	 */
	j9timeval_struct timeP;
	int32_t result =0;
	int32_t size = 0;
	J9SocketPTB *ptBuffers = NULL;

	Trc_PRT_sock_j9sock_select_read_Entry(j9socketP, secTime, uSecTime, isNonBlocking);
	
	ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		Trc_PRT_sock_j9sock_select_read_failure("NULL == ptBuffers");
		Trc_PRT_sock_j9sock_select_read_Exit(J9PORT_ERROR_SOCKET_SYSTEMFULL);
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}

/* The max fdset size per process is always expected to be less than a 32bit integer value.
 * Is this valid on a 64bit platform?
 */

	/*[PR 99074] Removed check for zero timeout */

	if (0 == secTime && 0 == uSecTime) {
		/* NOTE: empirical evidence indicates that select() on windows always returns zero if timeout is set to zero */
		/* [PR 134527] - win j9sock_select_read reports socket ready */
		if (isNonBlocking){
			/* add these checks, so that if only one socket is open return right away and avoid the loop below */
			if( ((j9socketP->flags & SOCKET_IPV4_OPEN_MASK) != 0) && ((j9socketP->flags & SOCKET_IPV6_OPEN_MASK)==0) ) {
				j9socketP->flags = j9socketP->flags | SOCKET_USE_IPV4_MASK;
				/* return value of 1 means there is data to be read */
				Trc_PRT_sock_j9sock_select_read_Exit(1);
				return 1;
			} else if( ((j9socketP->flags & SOCKET_IPV6_OPEN_MASK) != 0) && ((j9socketP->flags & SOCKET_IPV4_OPEN_MASK)==0) ) {
				j9socketP->flags = j9socketP->flags & ~SOCKET_USE_IPV4_MASK;
				/* return value of 1 means there is data to be read */
				Trc_PRT_sock_j9sock_select_read_Exit(1);
				return 1;
			}
			/* poll every 100 ms */
			j9sock_timeval_init( portLibrary, 0, 100*1000, &timeP );
		} else {
			uSecTime = 1;
			/* set time to smallest value, without setting to zero which returns 0 on Windows */
			j9sock_timeval_init( portLibrary, 0, uSecTime, &timeP );
		}
	} else {
		j9sock_timeval_init( portLibrary, secTime, uSecTime, &timeP );
	}

	for(;;) {
		result = j9sock_fdset_init( portLibrary,  j9socketP );
		if ( 0 != result) {
			Trc_PRT_sock_j9sock_select_read_failure("0 != j9sock_fdset_init( portLibrary, j9socketP )");
			Trc_PRT_sock_j9sock_select_read_Exit(result);
			return result;
		}
		size = j9sock_fdset_size( portLibrary, j9socketP );
		if ( 0 > size) {
			result = J9PORT_ERROR_SOCKET_FDSET_SIZEBAD;
		} else {
			result = j9sock_select(portLibrary, size, ptBuffers->fdset, NULL, NULL, &timeP);
		}
		/* break out of the loop if we should not be looping (timeout is zero) or if an error occured or data ready to be read */
		if ((J9PORT_ERROR_SOCKET_TIMEOUT != result) || (0 != secTime) || (0 !=uSecTime) ) {
			/* check which socket has activity after select call and set the appropriate flags */
			if (FD_ISSET(j9socketP->ipv6, ptBuffers->fdset)) {
				j9socketP->flags = j9socketP->flags & ~SOCKET_USE_IPV4_MASK;
			}
			/* update IPv4 last, so it will be used in the event both sockets had activity */
			if (FD_ISSET(j9socketP->ipv4, ptBuffers->fdset)) {
				j9socketP->flags = j9socketP->flags | SOCKET_USE_IPV4_MASK;
			}

			break;
		}
	}
	
	Trc_PRT_sock_j9sock_select_read_Exit(result);
	
	return result;
}

int32_t
j9sock_set_nonblocking(struct J9PortLibrary *portLibrary, j9socket_t socketP, BOOLEAN nonblocking)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	uint32_t param = (uint32_t)nonblocking;
	
	Trc_PRT_sock_j9sock_setnonblocking_Entry(socketP, nonblocking);
	
	/* If both the IPv4 and IPv6 socket are open then we want to set the option on both.  If only one is open,
		then we set it just on that one.  */

	if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
		rc  = ioctlsocket( socketP->ipv4, FIONBIO, &param);
	}

	if( rc == 0 && socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
		rc = ioctlsocket( socketP->ipv6, FIONBIO, &param);
	}

	if( rc != 0 ) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_setnonblocking_failure(rc);
		J9SOCKDEBUG( "<set_nonblocking (for bool) failed, err=%d>\n", rc );
		switch(rc) {
			case WSAEINVAL:
				rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_OPTARGSINVALID);
			default:
				rc = omrerror_set_last_error(rc, findError(rc));
		}
	}

	Trc_PRT_sock_j9sock_setnonblocking_Exit(rc);
	return rc;
}

/**
 * Ensure the flag designated is set in the argument.  This is used to construct arguments for the 
 * j9sock_read/readfrom/write/writeto calls with optional flags, such as J9SOCK_MSG_PEEK.
 *
 * @param[in] portLibrary The port library.
 * @param[in] flag The operation flag to set in the argument.
 * @param[in] arg Pointer to the argument to set the flag bit in.
 *
 * @return	0 if no error occurs, otherwise return the (negative) error code.
 */
int32_t
j9sock_setflag(struct J9PortLibrary *portLibrary, int32_t flag, int32_t *arg)
{
	int32_t rc = 0;
	if(flag == J9SOCK_MSG_PEEK) {
		*arg |= MSG_PEEK;
	} 
	else if (flag == J9SOCK_MSG_OOB) {
		*arg |= MSG_OOB;
	} 
	else {
		rc = J9PORT_ERROR_SOCKET_UNKNOWNFLAG;
	}
	return rc;
}


/**
 * Set the value of the nominated boolean socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[out] optval Pointer to the boolean to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_bool(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  BOOLEAN *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	BOOL option;
	int32_t optlen;

	Trc_PRT_sock_j9sock_setopt_bool_Entry(socketP, optlevel, optname, optval == NULL ? 0 : *optval);

	if (NULL == optval) {
		rc = J9PORT_ERROR_SOCKET_OPTARGSINVALID;
		Trc_PRT_sock_j9sock_setopt_bool_Exit(rc);
		return rc;
	}
	
	option = (BOOL)*optval;
	optlen = sizeof(option);
	
	if(0 > platformLevel) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("bool", "0 > platformLevel");
		Trc_PRT_sock_j9sock_setopt_bool_Exit(platformLevel);
		return platformLevel;
	}
	if(0 > platformOption) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("bool", "0 > platformOption");
		Trc_PRT_sock_j9sock_setopt_bool_Exit(platformOption);
		return platformOption;
	}

	/* If both the IPv4 and IPv6 socket are open then we want to set the option on both.  If only one is open,
		then we set it just on that one.  */

	if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
		rc  = setsockopt( socketP->ipv4, platformLevel, platformOption, (char*)&option, optlen);
	}
	if( rc == 0 && socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
		if((platformOption == IP_MULTICAST_LOOP)&&(platformLevel == OS_IPPROTO_IP)) {
			platformLevel = IPPROTO_IPV6;
			platformOption = IPV6_MULTICAST_LOOP;
		}
		rc = setsockopt( socketP->ipv6, platformLevel, platformOption, (char*)&option, optlen);
	}
	if( rc != 0 ) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_setopt_failure_code("bool", rc);
		J9SOCKDEBUG( "<setsockopt (for bool) failed, err=%d>\n", rc);
		switch(rc) {
			case WSAEINVAL:
				rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_OPTARGSINVALID);
				break;
			default:
				rc = omrerror_set_last_error(rc, findError(rc));
				break;
		}
	}

	Trc_PRT_sock_j9sock_setopt_bool_Exit(rc);
	
	return rc;
}


/**
 * Set the value of the nominated byte socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[out] optval Pointer to the byte to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_byte(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  uint8_t *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	int32_t result = 0;
	uint32_t optTemp;
	int32_t optlen;

	Trc_PRT_sock_j9sock_setopt_byte_Entry(socketP, optlevel, optname, optval == NULL ? 0 : *optval);
	
	if(0 > platformLevel) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("byte","0 > platformLevel");
		Trc_PRT_sock_j9sock_setopt_byte_Exit(platformLevel);
		return platformLevel;
	}
	if(0 > platformOption) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("byte", "0 > platformOption");
		Trc_PRT_sock_j9sock_setopt_byte_Exit(platformOption);
		return platformOption;
	}

	/* If both the IPv4 and IPv6 socket are open then we want to set the option on both.  If only one is open,
		then we set it just on that one.  */

	if(platformOption == IP_MULTICAST_TTL) {
		optTemp = (uint32_t)(*optval);
		optlen = sizeof(optTemp);
	
		if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
			result = setsockopt(socketP->ipv4, platformLevel, platformOption, (char*)&optTemp, optlen);
		}
		if( result == 0 && socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
			platformLevel = IPPROTO_IPV6;
			platformOption = IPV6_MULTICAST_HOPS;
			result = setsockopt(socketP->ipv6, platformLevel, platformOption, (char*)&optTemp, optlen);
		}
	} else {
		optlen = sizeof(*optval);
		if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
			result = setsockopt(socketP->ipv4, platformLevel, platformOption, (char*)optval, optlen);
		}
		if( result == 0 && socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
			result = setsockopt(socketP->ipv6, platformLevel, platformOption, (char*)optval, optlen);
		}
	}
 	if(0 != result) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_setopt_failure_code("byte", rc);
		J9SOCKDEBUG( "<setsockopt (for byte) failed, err=%d>\n", rc);
		switch(rc) {
				case WSAEINVAL:
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_OPTARGSINVALID);
					break;
				default:
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}
	}
 	
 	Trc_PRT_sock_j9sock_setopt_byte_Exit(rc);
 	
	return rc;
}


/**
 * Set the value of the nominated integer socket option.
 * Refer to the private platformSocketLevel & platformSocketOption functions for details of the options
 * supported.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[out] optval Pointer to the integer to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_int(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  int32_t *optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	int32_t optlen;

	Trc_PRT_sock_j9sock_setopt_int_Entry(socketP, optlevel, optname, optval == NULL ? 0 : *optval);
	
	optlen = sizeof(*optval);
	
	if(0 > platformLevel) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("int","0 > platformLevel");
		Trc_PRT_sock_j9sock_setopt_int_Exit(platformLevel);
		return platformLevel;
	}
	if(0 > platformOption) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("int","0 > platformOption");
		Trc_PRT_sock_j9sock_setopt_int_Exit(platformOption);
		return platformOption;
	}

	/* If both the IPv4 and IPv6 socket are open then we want to set the option on both.  If only one is open,
		then we set it just on that one.  Also if the option is at the IPV6 level we only set it if the 
		IPV6 socket is open  */
	if(( socketP->flags & SOCKET_IPV4_OPEN_MASK )&&( OS_IPPROTO_IPV6 != platformLevel)){
		rc = setsockopt( socketP->ipv4, platformLevel, platformOption, (char*)optval, optlen);
	}
	
	if( rc == 0 && socketP->flags & SOCKET_IPV6_OPEN_MASK ){
		/* set the option on the IPv6 socket unless it is IP_TOS which is not supported on IPv6 sockets */
		if( !(( OS_IPPROTO_IP == platformLevel)&&(OS_IP_TOS==platformOption))){
			rc = setsockopt( socketP->ipv6, platformLevel, platformOption, (char*)optval, optlen);
		}
	}

 	if(rc != 0) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_setopt_failure_code("int", rc);
		J9SOCKDEBUG( "<setsockopt (for int) failed, err=%d>\n", rc);
		switch(rc) {
				case WSAEINVAL:
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_OPTARGSINVALID);
					break;
				default:
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}
	}
 	
 	Trc_PRT_sock_j9sock_setopt_int_Exit(rc);
 	
	return rc;
}


/**
 * Set the multicast request on this socket. 
 * Currently this is used to join or leave the nominated multicast group on the local interface.
 * 	It may be more generally useful, so a generic 'setop' function has been defined.
 * 
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[out] optval Pointer to the ipmreq struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_ipmreq(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9ipmreq_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	int32_t optlen;

	Trc_PRT_sock_j9sock_setopt_ipmreq_Entry(socketP, optlevel, optname);
	
	optlen = sizeof(optval->addrpair);
	
	if(0 > platformLevel) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("ipmreq","0 > platformLevel");
		Trc_PRT_sock_j9sock_setopt_ipmreq_Exit(platformLevel);
		return platformLevel;
	}
	if(0 > platformOption) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("ipmreq","0 > platformOption");
		Trc_PRT_sock_j9sock_setopt_ipmreq_Exit(platformOption);
		return platformOption;
	}

	if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
 		rc = setsockopt(socketP->ipv4, platformLevel, platformOption, (char*)(&optval->addrpair), optlen);
	}

	if( rc != 0 ) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_setopt_failure_code("ipmreq",rc);
		J9SOCKDEBUG( "<setsockopt (for ipmreq) failed, err=%d>\n", rc);
		switch(rc) {
				case WSAEINVAL:
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_OPTARGSINVALID);
					break;
				default:
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}
	}
	
	Trc_PRT_sock_j9sock_setopt_ipmreq_Exit(rc);
	
	return rc;
}


/**
 * Set the multicast request on this socket for IPv6 sockets. 
 * Currently this is used to join or leave the nominated multicast group on the local interface.
 * 	It may be more generally useful, so a generic 'setop' function has been defined.t.
 *
 * Supported families are OS_AF_INET and OS_AF_INET6 
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[out] optval Pointer to the ipmreq struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_setopt_ipv6_mreq(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9ipv6_mreq_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	int32_t optlen;

	Trc_PRT_sock_j9sock_setopt_ipv6_mreq_Entry(socketP, optlevel, optname);
	
	optlen = sizeof(optval->mreq);
	
	if(0 > platformLevel) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("ipv6_mreq","0 > platformLevel");
		Trc_PRT_sock_j9sock_setopt_ipv6_mreq_Exit(platformLevel);
		return platformLevel;
	}
	if(0 > platformOption) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("ipv6_mreq","0 > platformOption");
		Trc_PRT_sock_j9sock_setopt_ipv6_mreq_Exit(platformOption);
		return platformOption;
	}

	if (socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
	 	rc = setsockopt(socketP->ipv6, platformLevel, platformOption, (char*)(&optval->mreq), optlen);
	}
	else {
		/* this option is not supported on this socket */
		Trc_PRT_sock_j9sock_setopt_failure_cause("ipv6_mreq","this option is not supported on this socket");
		Trc_PRT_sock_j9sock_setopt_ipv6_mreq_Exit(J9PORT_ERROR_SOCKET_SOCKLEVELINVALID);
		return J9PORT_ERROR_SOCKET_SOCKLEVELINVALID;
	}

	if( rc != 0 ) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_setopt_failure_code("ipv6_mreq",rc);
		J9SOCKDEBUG( "<setsockopt (for ipmreq) failed, err=%d>\n",rc );
		switch(rc) {
				case WSAEINVAL:
					rc = J9PORT_ERROR_SOCKET_OPTARGSINVALID;
					break;
				default:
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}
	}
	
	Trc_PRT_sock_j9sock_setopt_ipv6_mreq_Exit(rc);
	
	return rc;
}


/**
 * Set the linger value on the socket. 
 * See the @ref j9sock_linger_init for details of the linger behavior.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[out] optval Pointer to the linger struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_linger(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname,  j9linger_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	int32_t optlen;

	Trc_PRT_sock_j9sock_setopt_linger_Entry(socketP, optlevel, optname);
	
	optlen = sizeof(optval->linger);
	
	if(0 > platformLevel) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("linger","0 > platformLevel");
		Trc_PRT_sock_j9sock_setopt_linger_Exit(platformLevel);
		return platformLevel;
	}
	if(0 > platformOption) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("linger","0 > platformOption");
		Trc_PRT_sock_j9sock_setopt_linger_Exit(platformOption);
		return platformOption;
	}

	if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
 		rc = setsockopt( socketP->ipv4, platformLevel, platformOption, (char*)(&optval->linger), optlen);
	}

	if( rc == 0 && socketP->flags & SOCKET_IPV6_OPEN_MASK ) {
		rc = setsockopt( socketP->ipv6, platformLevel, platformOption, (char*)(&optval->linger), optlen);
	}

	if( rc != 0 ) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_setopt_failure_code("linger", rc);
		J9SOCKDEBUG( "<setsockopt (for linger) failed, err=%d>\n", rc);
		switch(rc) {
				case WSAEINVAL:
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_OPTARGSINVALID);
					break;
				default:
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}
	}
	
	Trc_PRT_sock_j9sock_setopt_linger_Exit(rc);
	
	return rc;
}


/**
 * Set the sockaddr for the socket.
 * Currently used to set the interface of multicast sockets, but the more general call style is used,
 * in case it is more generally useful.
 *
 * @param[in] portLibrary The port library.
 * @param[in] socketP Pointer to the socket to set the option in.
 * @param[in] optlevel The level within the IP stack at which the option is defined.
 * @param[in] optname The name of the option to set.
 * @param[out] optval Pointer to the j9sockaddr struct to update the socket option with.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_setopt_sockaddr(struct J9PortLibrary *portLibrary, j9socket_t socketP, int32_t optlevel, int32_t optname, j9sockaddr_t optval)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t platformLevel = platformSocketLevel(optlevel);
	int32_t platformOption = platformSocketOption(optname);
	int32_t optlen;

	Trc_PRT_sock_j9sock_setopt_sockaddr_Entry(socketP, optlevel, optname);
	
	/* It is safe to cast to this as this method is only used with IPv4 addresses */
	optlen = sizeof(((OSSOCKADDR*)&optval->addr)->sin_addr);

	if (0 > platformLevel) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("sockaddr","0 > platformLevel");
		Trc_PRT_sock_j9sock_setopt_sockaddr_Exit(platformLevel);
		return platformLevel;
	}
	if (0 > platformOption) {
		Trc_PRT_sock_j9sock_setopt_failure_cause("sockaddr","0 > platformOption");
		Trc_PRT_sock_j9sock_setopt_sockaddr_Exit(platformOption);
		return platformOption;
	}

	if( socketP->flags & SOCKET_IPV4_OPEN_MASK ) {
		rc = setsockopt( socketP->ipv4, platformLevel, platformOption, (char *) &((OSSOCKADDR*)&optval->addr)->sin_addr, optlen);
	}

	if( rc != 0 ) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_setopt_failure_code("sockaddr", rc);
		J9SOCKDEBUG("<setsockopt (for sockaddr) failed, err=%d>\n", rc);
		switch(rc)
		{
				case WSAEINVAL:
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_OPTARGSINVALID);
					break;
				default:
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}
	}
	
	Trc_PRT_sock_j9sock_setopt_sockaddr_Exit(rc);
	
	return rc;
}


/**
 * Terminates use of the socket library.  No sockets should be in use or the results
 * of this operation are unpredictable.  Frees any resources held by the socket library.
 *
 * @param[in] portLibrary The port library.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code
 */
int32_t
j9sock_shutdown(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t result = 0;

	result = WSACleanup();
	if(SOCKET_ERROR == result ) {
		int32_t errorCode = WSAGetLastError();

		J9SOCKDEBUG( "<WSACleanup() failed, err=%d>\n", errorCode );
		return omrerror_set_last_error(errorCode, findError(errorCode));
	}
	j9sock_ptb_shutdown(portLibrary);
	return 0;
}


/**
 * The shutdown_input function disables the input stream on a socket. Any subsequent reads from the socket
 * will fail.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Socket to close input stream on.
 *
 * @return
 * \arg  0, on success
 * \arg J9PORT_ERROR_SOCKET_OPFAILED, on generic error
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED, if the library is not initialized
*/
/* IPv6 - If we still have 2 sockets open, then close the input on both.  May happen with ::0 and 0.0.0.0.
 */
int32_t
j9sock_shutdown_input(struct J9PortLibrary *portLibrary, j9socket_t sock)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	/* If IPv4 is open or IPv6 is not open.  Previously we called it every time, even if the socket was closed. */
  	if( sock->flags & SOCKET_USE_IPV4_MASK || ! (sock->flags & SOCKET_IPV6_OPEN_MASK) ) { 
		rc =shutdown(sock->ipv4, 0);
	}
	
	if( sock->flags & SOCKET_IPV6_OPEN_MASK ) {
		rc =shutdown(sock->ipv6, 0);
	}

	if( rc == SOCKET_ERROR )  {
		int32_t errorCode = WSAGetLastError();

		J9SOCKDEBUG( "<shutdown_input failed, err=%d>\n", errorCode);
		return omrerror_set_last_error(errorCode, findError(errorCode));
	}
	return 0;
}


/**
 * The shutdown_output function disables the output stream on a socket. Any subsequent writes to the socket
 * will fail.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Socket to close output stream on.
 *
 * @return
 * \arg  0, on success
 * \arg J9PORT_ERROR_SOCKET_OPFAILED, on generic error
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED, if the library is not initialized
 */
int32_t
j9sock_shutdown_output(struct J9PortLibrary *portLibrary, j9socket_t sock)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;

  	if( sock->flags & SOCKET_USE_IPV4_MASK || ! (sock->flags & SOCKET_IPV6_OPEN_MASK) ) { 
		rc = shutdown( sock->ipv4 , 1);
	}
	
	if( sock->flags & SOCKET_IPV6_OPEN_MASK) {
		rc = shutdown( sock->ipv6 , 1);
	}

	if( rc == SOCKET_ERROR) {
		int32_t errorCode = WSAGetLastError();

		J9SOCKDEBUG( "<shutdown_output failed, err=%d>\n", errorCode);
		return omrerror_set_last_error(errorCode, findError(errorCode));
	}
	return 0;
}


/**
 * Creates a new j9sockaddr, referring to the specified port and address.  The only address family currently supported
 * is AF_INET.
 *
 * @param[in] portLibrary The port library.
 * @param[out] handle Pointer to the j9sockaddr struct, to be allocated.
 * @param[in] addrStr The target host, as either a name or dotted ip string.
 * @param[in] port The target port, in host order.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_sockaddr(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, const char *addrStr, uint16_t port)
{
	int32_t rc = 0;
	uint32_t addr = 0;
	j9hostent_struct host_t;

	if(0 != portLibrary->sock_inetaddr(portLibrary, addrStr, &addr)) {
		memset(&host_t, 0, sizeof (struct j9hostent_struct));
		if (0 != (rc = portLibrary->sock_gethostbyname(portLibrary, addrStr, &host_t))) {
			return rc;
		} else {
			addr = portLibrary->sock_hostent_addrlist(portLibrary, &host_t, 0);
		}
	} 
	rc = portLibrary->sock_sockaddr_init(portLibrary, handle, J9SOCK_AFINET, addr, port);
	return rc;
}



/**
 * Answer the address, in network order, of the j9sockaddr argument.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the j9sockaddr struct to access.
 *
 * @return	the address (there is no validation on the access).
 */
int32_t
j9sock_sockaddr_address(struct J9PortLibrary *portLibrary, j9sockaddr_t handle)
{
	return ((OSSOCKADDR*)&handle->addr)->sin_addr.s_addr;
}


/**
 * Answers the IP address of a structure and its length, in a preallocated buffer.
 *
 * Preallocated buffer "address" should be 16 bytes.  "length" tells you how many bytes were used 4 or 16.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle A populated j9sockaddr_t.
 * @param[out] address The IPv4 or IPv6 address in network byte order.
 * @param[out] length The number of bytes of the address (4 or 16).
 * @param[out] scope_id the scope id for the address if appropriate
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_sockaddr_address6(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, uint8_t *address, uint32_t *length, uint32_t* scope_id)
{
	OSSOCKADDR *ipv4;
	OSSOCKADDR_IN6 *ipv6;

	ipv4 = (OSSOCKADDR *) &handle->addr;
	if( ipv4->sin_family == OS_AF_INET4 ) {
		memcpy( address, &ipv4->sin_addr, 4 );
		*length = 4;
		*scope_id = 0;
	}
	else {
		ipv6 = (OSSOCKADDR_IN6 *) &handle->addr;
		memcpy( address, &ipv6->sin6_addr, 16 );
		*length = 16;
		*scope_id = ipv6->sin6_scope_id;
	}

	return 0;
}


/**
 * Answers the family name of a j9sockaddr_struct.
 *
 * Supported families are OS_AF_INET and OS_AF_INET6 
 *
 * @param[in] portLibrary The port library.
 * @param[out] family The family name of the address.
 * @param[in] handle A populated j9sockaddr_t.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 *
 * @note Added for IPv6 support.
 */
int32_t
j9sock_sockaddr_family(struct J9PortLibrary *portLibrary, int16_t *family, j9sockaddr_t handle)
{
	OSSOCKADDR *ipv4;
	
	ipv4 = (OSSOCKADDR *) &handle->addr;
	if( ipv4->sin_family == OS_AF_INET4 ) {
		*family = J9ADDR_FAMILY_AFINET4;
	} else {
		*family = J9ADDR_FAMILY_AFINET6;
	} 

	return 0;
}

int32_t
j9sock_sockaddr_init(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, int16_t family, uint32_t nipAddr, uint16_t portNetworkOrder)
{
    OSSOCKADDR *sockaddr;
	memset(handle, 0, sizeof (struct j9sockaddr_struct));
    sockaddr = (OSSOCKADDR*)&handle->addr;
	sockaddr->sin_family = family;
	sockaddr->sin_addr.s_addr = nipAddr ;
	sockaddr->sin_port = portNetworkOrder;

	return 0;
}

int32_t
j9sock_sockaddr_init6(struct J9PortLibrary *portLibrary, j9sockaddr_t handle, uint8_t *addr, int32_t addrlength, int16_t family, uint16_t nPort,uint32_t flowinfo, uint32_t scope_id, j9socket_t sock)
{
    	OSSOCKADDR *sockaddr;
	OSSOCKADDR_IN6 *sockaddr_6;
	memset(handle, 0, sizeof (struct j9sockaddr_struct));

	if( family == J9ADDR_FAMILY_AFINET4 ) {
    		sockaddr = (OSSOCKADDR*)&handle->addr;
		memcpy( &sockaddr->sin_addr.s_addr, addr, addrlength) ;
		sockaddr->sin_port = nPort;
		sockaddr->sin_family = OS_AF_INET4;
	} 
	else if ( family == J9ADDR_FAMILY_AFINET6 ) {
		sockaddr_6 = (OSSOCKADDR_IN6*)&handle->addr;
		memcpy( &sockaddr_6->sin6_addr.s6_addr, addr, addrlength) ;
		sockaddr_6->sin6_port = nPort;
		sockaddr_6->sin6_family = OS_AF_INET6;
		sockaddr_6->sin6_scope_id = scope_id;
		sockaddr_6->sin6_flowinfo   = htonl(flowinfo);
	}
	else {
		sockaddr = (OSSOCKADDR*)&handle->addr;
		sockaddr->sin_family = map_addr_family_J9_to_OS(family);
	}

	return 0;
}


/**
 * Answer the port, in network order, of the j9sockaddr argument.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the j9sockaddr struct to access.
 *
 * @return	the port (there is no validation on the access).
 */
uint16_t
j9sock_sockaddr_port(struct J9PortLibrary *portLibrary, j9sockaddr_t handle)
{
	if( ((OSSOCKADDR*)&handle->addr)->sin_family == OS_AF_INET4 ) {
		return ((OSSOCKADDR*)&handle->addr)->sin_port;
	} else {
		return ((OSSOCKADDR_IN6*)&handle->addr)->sin6_port;		
	}

}


/**
 * Creates a new socket descriptor and any related resources.
 *
 * @param[in] portLibrary The port library.
 * @param[out]	handle Pointer pointer to the j9socket struct, to be allocated
 * @param[in] family The address family (currently, only J9SOCK_AFINET is supported)
 * @param[in] socktype Specifies what type of socket is created
 * \arg J9SOCK_STREAM, for a stream socket
 * \arg J9SOCK_DGRAM, for a datagram socket
 * @param[in] protocol Type/family specific creation parameter (currently, only J9SOCK_DEFPROTOCOL supported).
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_socket(struct J9PortLibrary *portLibrary, j9socket_t *handle, int32_t family, int32_t socktype,  int32_t protocol)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	OSSOCKET ipv4 = INVALID_SOCKET;
	OSSOCKET ipv6 = INVALID_SOCKET;


	Trc_PRT_sock_j9sock_socket_Entry(handle, family, socktype, protocol);
	
	/* Initialize the handle to invalid */
	*handle = (j9socket_t) -1;

	if(family != J9ADDR_FAMILY_AFINET6 && family != J9ADDR_FAMILY_AFINET4 && family != J9ADDR_FAMILY_UNSPEC) {
		Trc_PRT_sock_j9sock_socket_failure_cause("family != J9ADDR_FAMILY_AFINET6 && family != J9ADDR_FAMILY_AFINET4 && family != J9ADDR_FAMILY_UNSPEC");
		rc = J9PORT_ERROR_SOCKET_BADAF;
	}
	if((socktype != J9SOCK_STREAM) && (socktype != J9SOCK_DGRAM) ) {
		Trc_PRT_sock_j9sock_socket_failure_cause("(socktype != J9SOCK_STREAM) && (socktype != J9SOCK_DGRAM)");
		rc = J9PORT_ERROR_SOCKET_BADTYPE;
	}
	if(protocol != J9SOCK_DEFPROTOCOL ) {
		Trc_PRT_sock_j9sock_socket_failure_cause("protocol != J9SOCK_DEFPROTOCOL");
		rc = J9PORT_ERROR_SOCKET_BADPROTO;
	}
	if (rc == 0) {
		if( family == J9ADDR_FAMILY_AFINET4 ||  family == J9ADDR_FAMILY_UNSPEC ) {
			ipv4 = socket(AF_INET, ((socktype == J9SOCK_STREAM) ? SOCK_STREAM : SOCK_DGRAM),  0);
			if( ipv4 == INVALID_SOCKET ) {
				rc = WSAGetLastError();
				Trc_PRT_sock_j9sock_socket_creation_failure(rc);
				J9SOCKDEBUG( "<socket failed, err=%d>\n", rc );
				switch(rc) {
					case WSAENOBUFS:
						rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_SYSTEMFULL);
						break;
					default:
						rc = omrerror_set_last_error(rc, findError(rc));
				}
			}
		}
		if( rc ==0 && ( family == J9ADDR_FAMILY_AFINET6 ||  family ==  J9ADDR_FAMILY_UNSPEC ) ) {

			ipv6 = socket(AF_INET6, ((socktype == J9SOCK_STREAM) ? SOCK_STREAM : SOCK_DGRAM),  0);

			if( ipv6 == INVALID_SOCKET ) {
				rc = WSAGetLastError();
				Trc_PRT_sock_j9sock_socket_creation_failure(rc);
				J9SOCKDEBUG( "<socket failed, err=%d>\n", rc);
				switch(rc) {
					case WSAENOBUFS:
						rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_SYSTEMFULL);
						break;
					case WSAEAFNOSUPPORT:
						rc = 0;
						break;
					default:
						rc = omrerror_set_last_error(rc, findError(rc));
				}
				if (rc != 0) {
					/* close the open IPv4 socket because socket() call fails
					 * and the user would never get to use this socket as we
					 * would return in error from IPv6 socket creation failure */
					internalCloseSocket(portLibrary, *handle, TRUE);	
				}
			}	
		}
	}
	
	if (0 == rc) {
		*handle = omrmem_allocate_memory(sizeof( struct j9socket_struct ), OMRMEM_CATEGORY_PORT_LIBRARY);

		if (NULL == *handle) {

			*handle = (j9socket_t)INVALID_SOCKET;
			
			if (INVALID_SOCKET != ipv4) {
				shutdown(ipv4, 1);
				closesocket(ipv4);
				ipv4 = INVALID_SOCKET;
			}
			if (INVALID_SOCKET != ipv6) {
				shutdown(ipv6, 1);
				closesocket(ipv6);
				ipv6 = INVALID_SOCKET;
			}
			rc = J9PORT_ERROR_SOCKET_NOBUFFERS;
			Trc_PRT_sock_j9sock_socket_failure_oom();
			Trc_PRT_sock_j9sock_socket_Exit(rc);
			return rc;
		}
		
		/* Initialize the new structure to show that the IPv4 structure is to be used, and the 2 sockets are invalid */
		(*handle)->ipv4 = INVALID_SOCKET;
		(*handle)->ipv6 = INVALID_SOCKET;
		(*handle)->flags = SOCKET_USE_IPV4_MASK;


		if( ipv4 != INVALID_SOCKET ) {
			/* adjust flags to show IPv4 socket is open for business */
			(*handle)->flags = (*handle)->flags | SOCKET_IPV4_OPEN_MASK;
			(*handle)->ipv4 = ipv4; 	
		}

		if( ipv6 != INVALID_SOCKET ) {
			(*handle)->ipv6 = ipv6; 
			if( family == J9ADDR_FAMILY_AFINET6 ) {
				/* set flags to show use IPV6 and IPv6 socket open */
				(*handle)->flags = ~SOCKET_USE_IPV4_MASK & SOCKET_IPV6_OPEN_MASK;
			} else {
				/* adjust flags to show IPv6 is open for business */
				(*handle)->flags = (*handle)->flags | SOCKET_IPV6_OPEN_MASK;
			}
		}
		
		Trc_PRT_sock_j9sock_socket_created(*handle);
	} 
	
	Trc_PRT_sock_j9sock_socket_Exit(rc);
	
	return rc;
}


/**
 * Determines whether or not the socket is valid.
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to the j9socket struct, to be allocated.
 *
 * @return	0 if invalid, non-zero for valid.
 */
int32_t
j9sock_socketIsValid(struct J9PortLibrary *portLibrary, j9socket_t handle)
{
	if( handle == (void *) NULL || handle == (void *) INVALID_SOCKET ) {
		return FALSE;
	}

	/* If either socket is open, then return TRUE, otherwise return FALSE */
	return handle->flags & SOCKET_BOTH_OPEN_MASK;
}


/**
 * Initiate the use of sockets by a process.  This function must be called before any other socket calls.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg J9PORT_ERROR_STARTUP_SOCK
 * \arg J9PORT_ERROR_SOCKET_OPFAILED
 * \arg J9PORT_ERROR_SOCKET_NOTINITIALIZED
 */
int32_t
j9sock_startup(struct J9PortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;

	if (0 != j9sock_ptb_init(portLibrary)) {
		rc = J9PORT_ERROR_STARTUP_SOCK;
	} else {
		WSADATA wsaData;
	
		PPG_sock_IPv6_FUNCTION_SUPPORT = 1;

		/* On windows we need to figure out if we have IPv6 support functions.
		 * We set a flag to indicate which set of functions to use, IPv6 API or IPv4 API.
		 */
		if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {

			if (WSAStartup(MAKEWORD(1,1), &wsaData) != 0) {
				rc = WSAGetLastError();
				J9SOCKDEBUG( "<WSAStartup() failed, err=%d>", rc);
				rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NOTINITIALIZED);
			}

			PPG_sock_IPv6_FUNCTION_SUPPORT = 0;
		} else if( LOBYTE( wsaData.wVersion ) == 1 )  {
			PPG_sock_IPv6_FUNCTION_SUPPORT = 0;
		}

#if defined(J9_USE_CONNECTION_MANAGER)
		PPG_sock_connection = NULL;
#endif
	}
	return rc;
}


/**
 * Create a time structure, representing the timeout period defined in seconds & microSeconds.
 * Timeval's are used as timeout arguments in the @ref j9sock_select function.
 *
 * @param[in] portLibrary The port library.
 * @param[in] secTime The integer component of the timeout value (in seconds).
 * @param[in] uSecTime The fractional component of the timeout value (in microseconds).
 * @param[out] timeP Pointer pointer to the j9timeval_struct to be allocated.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_timeval_init(struct J9PortLibrary *portLibrary, uint32_t secTime, uint32_t uSecTime, j9timeval_t timeP)
{
	memset(timeP, 0, sizeof (struct j9timeval_struct));
	timeP->time.tv_sec = secTime;
	timeP->time.tv_usec = uSecTime;

	return 0;
}


/**
 * The write function writes data to a connected socket.  The successful completion of a write 
 * does not indicate that the data was successfully delivered.  If no buffer space is available 
 * within the transport system to hold the data to be transmitted, send will block.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to send on
 * @param[in] buf The bytes to be sent
 * @param[in] nbyte The number of bytes to send
 * @param[in] flags The flags to modify the send behavior
 *
 * @return	If no error occur, return the total number of bytes sent, which can be less than the 
 * 'nbyte' for nonblocking sockets, otherwise the (negative) error code
 */
int32_t
j9sock_write(struct J9PortLibrary *portLibrary, j9socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t rc = 0;
	int32_t bytesSent = 0;

	Trc_PRT_sock_j9sock_write_Entry(sock, buf, nbyte, flags);
	
	if( sock->flags & SOCKET_USE_IPV4_MASK ) {
		bytesSent = send(sock->ipv4, (char *)buf, nbyte, flags);
	} else {
		bytesSent = send(sock->ipv6, (char *)buf, nbyte, flags);
	}
	if(SOCKET_ERROR == bytesSent) {
		rc = WSAGetLastError();
		Trc_PRT_sock_j9sock_write_failure(rc);
		J9SOCKDEBUG( "<send failed, err=%d>\n", rc);
		switch(rc){
				case WSAEINVAL:
					rc = omrerror_set_last_error(rc, J9PORT_ERROR_SOCKET_NOTBOUND);
					break;
				default:
					rc = omrerror_set_last_error(rc, findError(rc));
					break;
		}
	} else {
		rc = bytesSent;
	}
	
	Trc_PRT_sock_j9sock_write_Exit(rc);
	
	return rc;
}


/**
 * The writeto function writes data to a datagram socket.  The successful completion of a writeto
 * does not indicate that the data was successfully delivered.  If no buffer space is available 
 * within the transport system to hold the data to be transmitted, writeto will block.
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock Pointer to the socket to send on
 * @param[in] buf The bytes to be sent
 * @param[in] nbyte The number of bytes to send
 * @param[in] flags The flags to modify the send behavior
 * @param [in] addrHandle The network address to send the datagram to
 *
 * @return	If no error occur, return the total number of bytes sent, otherwise the (negative) error code.
 */
int32_t
j9sock_writeto(struct J9PortLibrary *portLibrary, j9socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, j9sockaddr_t addrHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int32_t bytesSent = 0;
#if defined(J9_USE_CONNECTION_MANAGER)
	BOOLEAN firstAttempt = TRUE;
	Trc_PRT_sock_j9sock_writeto_Entry(sock, buf, nbyte, flags, addrHandle);

retry:
#else
	Trc_PRT_sock_j9sock_writeto_Entry(sock, buf, nbyte, flags, addrHandle);
#endif
	if( ((((OSSOCKADDR *) &addrHandle->addr)->sin_family == OS_AF_INET4) && (sock->flags & SOCKET_IPV4_OPEN_MASK) ) || 
		((((OSSOCKADDR *) &addrHandle->addr)->sin_family == OS_AF_INET6) && !(sock->flags & SOCKET_IPV6_OPEN_MASK)) ){
		bytesSent = sendto( sock->ipv4, (char *)buf, nbyte, flags, (const struct sockaddr *) &(addrHandle->addr), sizeof(addrHandle->addr));
	} else {
		bytesSent = sendto( sock->ipv6, (char *)buf, nbyte, flags, (const struct sockaddr *) &(addrHandle->addr), sizeof(addrHandle->addr));
	}

	if(SOCKET_ERROR == bytesSent) {
		int32_t errorCode = WSAGetLastError();
		Trc_PRT_sock_j9sock_writeto_failure(errorCode);
		J9SOCKDEBUG( "<sendto failed, err=%d>\n", errorCode );
#if defined(J9_USE_CONNECTION_MANAGER)
		if (firstAttempt && (errorCode == WSAENETDOWN || errorCode == WSAEHOSTUNREACH)) {
			firstAttempt = FALSE;
			if (ensureConnected(portLibrary)) goto retry;
		}
		firstAttempt = FALSE;
#endif
		errorCode = omrerror_set_last_error(errorCode, findError(errorCode));
		Trc_PRT_sock_j9sock_writeto_Exit(errorCode);
		return errorCode;
	} else {
		Trc_PRT_sock_j9sock_writeto_Exit(bytesSent);
		return bytesSent;
	}
}


static int32_t
addAdapterIpv6(struct J9PortLibrary *portLibrary, struct j9NetworkInterface_struct *interfaces, uint32_t currentAdapterIndex,
		IP_ADAPTER_ADDRESSES *currentAdapter, IP_ADAPTER_ADDRESSES *adaptersList, char *baseName, uintptr_t adapterIndex)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uintptr_t nameLength;
	uint32_t numAddresses =0;
	IP_ADAPTER_UNICAST_ADDRESS *currentIPAddress = NULL;
	uint32_t pseudoLoopbackFound = 0;
	IP_ADAPTER_ADDRESSES *tempAdapter = NULL;
	uint32_t currentIPAddressIndex = 0;

	/* set the index for the interface */
	/* return non-zero Ipv6IfIndex for Ipv6 enabled host, otherwise return IfIndex for Ipv4 only host */
	if (currentAdapter->Ipv6IfIndex != 0) {
		interfaces[currentAdapterIndex].index = currentAdapter->Ipv6IfIndex;
	} else {
		interfaces[currentAdapterIndex].index = currentAdapter->IfIndex;
	}

	/* get the name and display name for the adapter */
	if (adapterIndex != -1) {
		nameLength = omrstr_printf(NULL, 0, "%s%d", baseName, adapterIndex);
	} else {
		nameLength = strlen(baseName) + 1;
	}
	interfaces[currentAdapterIndex].name = omrmem_allocate_memory(nameLength, OMRMEM_CATEGORY_PORT_LIBRARY);
#if (defined(VALIDATE_ALLOCATIONS))
	if (NULL == interfaces[currentAdapterIndex].name) {
		return J9PORT_ERROR_SOCKET_NOBUFFERS; 
	}
#endif
	if (-1 != adapterIndex) {
		omrstr_printf(interfaces[currentAdapterIndex].name, nameLength, "%s%d", baseName, adapterIndex);
	} else {
		strcpy(interfaces[currentAdapterIndex].name, baseName);
	}

	nameLength = WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS,currentAdapter->Description, -1,NULL,0,NULL,NULL);

	if (0 != nameLength) {
		interfaces[currentAdapterIndex].displayName = omrmem_allocate_memory(nameLength + 1, OMRMEM_CATEGORY_PORT_LIBRARY);
	} else {
		interfaces[currentAdapterIndex].displayName = omrmem_allocate_memory(1, OMRMEM_CATEGORY_PORT_LIBRARY);
		interfaces[currentAdapterIndex].displayName[0] = 0;
	}
#if (defined(VALIDATE_ALLOCATIONS))
	if (NULL == interfaces[currentAdapterIndex].displayName) {
		return J9PORT_ERROR_SOCKET_NOBUFFERS; 
	}
#endif
	if (nameLength != 0) {
		WideCharToMultiByte(OS_ENCODING_CODE_PAGE, OS_ENCODING_WC_FLAGS,currentAdapter->Description, -1, interfaces[currentAdapterIndex].displayName , (int)nameLength, NULL,NULL);
		interfaces[currentAdapterIndex].displayName[nameLength] = 0;
	}

	/* now get the interface information */

	/* first count the number of IP addresses and allocate the memory required for the ip address info that will be returned*/
	numAddresses = 0;
	currentIPAddress = currentAdapter->FirstUnicastAddress;
	while(currentIPAddress) {
		numAddresses = numAddresses + 1;
		currentIPAddress = currentIPAddress->Next;
	}

	/* if this is the loopback address then we need to add the addresses from the Loopback Pseudo-Interface */
	pseudoLoopbackFound = 0;
	if ((NULL != currentAdapter->FirstUnicastAddress)&&
		(AF_INET == ((struct sockaddr_in*)(currentAdapter->FirstUnicastAddress->Address.lpSockaddr))->sin_family ) &&
		( 127 == ((struct sockaddr_in*)(currentAdapter->FirstUnicastAddress->Address.lpSockaddr))->sin_addr.S_un.S_un_b.s_b1)
	   ){
		/* find the pseudo interface and get the first unicast address */
		tempAdapter = adaptersList;
		pseudoLoopbackFound = 0;
		while((tempAdapter)&&(0==pseudoLoopbackFound)) {
			if (strcmp(tempAdapter->AdapterName,pseudoLoopbackGUID) == 0) {
				pseudoLoopbackFound = 1;
			} else{
				tempAdapter = tempAdapter->Next;
			}
		}

		if (1 == pseudoLoopbackFound){
			/* now if we found the adapter add the count for the addresses on it */
			currentIPAddress = tempAdapter->FirstUnicastAddress;
			while(currentIPAddress) {
				numAddresses = numAddresses + 1;
				currentIPAddress = currentIPAddress->Next;
			}

			/* also if we found the pseudo interface we must have to use the interface id associated with this interface */
			interfaces[currentAdapterIndex].index = tempAdapter->Ipv6IfIndex;
		}
	}

	interfaces[currentAdapterIndex].addresses = omrmem_allocate_memory(numAddresses * sizeof(j9ipAddress_struct), OMRMEM_CATEGORY_PORT_LIBRARY);
#if (defined(VALIDATE_ALLOCATIONS))
	if (NULL == interfaces[currentAdapterIndex].addresses) {
		return J9PORT_ERROR_SOCKET_NOBUFFERS; 
	}
#endif
	interfaces[currentAdapterIndex].numberAddresses = numAddresses;

	/* now get the actual ip address info */
	currentIPAddressIndex	=0;
	currentIPAddress = currentAdapter->FirstUnicastAddress;
	while(currentIPAddress) {
		if (currentIPAddress->Address.iSockaddrLength == sizeof(struct sockaddr_in6)) {
			memcpy(interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].addr.bytes,
					  &(((struct sockaddr_in6*) currentIPAddress->Address.lpSockaddr)->sin6_addr.u.Byte),
					 sizeof(struct in6_addr));
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].length = sizeof(struct in6_addr);
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].scope = ((struct sockaddr_in6*) currentIPAddress->Address.lpSockaddr)->sin6_scope_id;
		} else	if (currentIPAddress->Address.iSockaddrLength == sizeof(struct sockaddr_in6_old)) {
			memcpy(interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].addr.bytes,
						  &(((struct sockaddr_in6_old*) currentIPAddress->Address.lpSockaddr)->sin6_addr.u.Byte),
					 sizeof(struct in6_addr));
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].length = sizeof(struct in6_addr);
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].scope = 0;
		} else {
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].addr.inAddr.S_un.S_addr = 
																				((struct sockaddr_in*) currentIPAddress->Address.lpSockaddr)->sin_addr.S_un.S_addr;
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].length = sizeof(struct in_addr);
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].scope = 0;
		}

		currentIPAddress = currentIPAddress->Next;
		currentIPAddressIndex = currentIPAddressIndex + 1;
	}

	/* now add in the addresses from the loopback pseudo-interface if appropriate */
	if (1  == pseudoLoopbackFound) {
		currentIPAddress = tempAdapter->FirstUnicastAddress;
		while(currentIPAddress) {
			if (currentIPAddress->Address.iSockaddrLength == sizeof(struct sockaddr_in6)) {
				memcpy(interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].addr.bytes,
						  &(((struct sockaddr_in6*) currentIPAddress->Address.lpSockaddr)->sin6_addr.u.Byte),
						 sizeof(struct in6_addr));
				interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].length = sizeof(struct in6_addr);
				interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].scope = ((struct sockaddr_in6*) currentIPAddress->Address.lpSockaddr)->sin6_scope_id;
			} else	if (currentIPAddress->Address.iSockaddrLength == sizeof(struct sockaddr_in6_old)) {
				memcpy(interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].addr.bytes,
							  &(((struct sockaddr_in6_old*) currentIPAddress->Address.lpSockaddr)->sin6_addr.u.Byte),
						 sizeof(struct in6_addr));
				interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].length = sizeof(struct in6_addr);
				interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].scope = 0;
			} else {
				interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].addr.inAddr.S_un.S_addr = 
																					((struct sockaddr_in*) currentIPAddress->Address.lpSockaddr)->sin_addr.S_un.S_addr;
				interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].length = sizeof(struct in_addr);
				interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].scope = 0;
			}

			currentIPAddress = currentIPAddress->Next;
			currentIPAddressIndex = currentIPAddressIndex + 1;
		}
	}
	return 0;
}

static int32_t
addAdapterIpv4(struct J9PortLibrary *portLibrary, struct j9NetworkInterface_struct *interfaces, uint32_t currentAdapterIndex,
		IP_ADAPTER_INFO *currentAdapter, IP_ADAPTER_INFO *adaptersList, char *baseName, uintptr_t adapterIndex)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uintptr_t nameLength;
	uint32_t numAddresses =0;
	IP_ADDR_STRING *currentIPAddress = NULL;
	uint32_t pseudoLoopbackFound = 0;
	IP_ADAPTER_ADDRESSES *tempAdapter = NULL;
	uint32_t currentIPAddressIndex = 0;

	/* Harmony code rely on index to check network interface status */
	interfaces[currentAdapterIndex].index = currentAdapter->Index;

	/* get the name and display name for the adapter */
	if (-1 != adapterIndex) {
		nameLength = omrstr_printf(NULL, 0, "%s%d", baseName, adapterIndex);
	} else {
		nameLength = strlen(baseName) + 1;
	}
	interfaces[currentAdapterIndex].name = omrmem_allocate_memory(nameLength, OMRMEM_CATEGORY_PORT_LIBRARY);
#if (defined(VALIDATE_ALLOCATIONS))
	if (NULL == interfaces[currentAdapterIndex].name) {
		return J9PORT_ERROR_SOCKET_NOBUFFERS; 
	}
#endif
	
	if (-1 != adapterIndex) {
		omrstr_printf(interfaces[currentAdapterIndex].name, nameLength, "%s%d", baseName, adapterIndex);
	} else {
		strcpy(interfaces[currentAdapterIndex].name, baseName);
	}

	nameLength = strlen(currentAdapter->Description);
	interfaces[currentAdapterIndex].displayName = omrmem_allocate_memory(nameLength+1, OMRMEM_CATEGORY_PORT_LIBRARY);
#if (defined(VALIDATE_ALLOCATIONS))
	if (NULL == interfaces[currentAdapterIndex].displayName) {
		return J9PORT_ERROR_SOCKET_NOBUFFERS; 
	}
#endif
	strncpy(interfaces[currentAdapterIndex].displayName,currentAdapter->Description,nameLength);
	interfaces[currentAdapterIndex].displayName[nameLength] = 0;	

	/* now get the interface information */

	/* first count the number of IP addresses and allocate the memory required for the ip address info that will be returned*/
	numAddresses = 0;
	currentIPAddress = &(currentAdapter->IpAddressList);
	while(currentIPAddress) {
		unsigned long addr = inet_addr(currentIPAddress->IpAddress.String);

		/* don't count the any address which seems to be returned as the first address for interfaces with no addresses */
		if ( (addr != INADDR_NONE) && (addr != INADDR_ANY) ) {
			numAddresses = numAddresses + 1;
		}
		currentIPAddress = currentIPAddress->Next;
	}
	interfaces[currentAdapterIndex].addresses = omrmem_allocate_memory(numAddresses * sizeof(j9ipAddress_struct), OMRMEM_CATEGORY_PORT_LIBRARY);
#if (defined(VALIDATE_ALLOCATIONS))
	if (NULL == interfaces[currentAdapterIndex].addresses) {
		return J9PORT_ERROR_SOCKET_NOBUFFERS; 
	}
#endif
	interfaces[currentAdapterIndex].numberAddresses = numAddresses;

	/* now get the actual ip address info */
	currentIPAddressIndex	=0;
	currentIPAddress = &(currentAdapter->IpAddressList);
	while(currentIPAddress) {
		unsigned long addr = inet_addr(currentIPAddress->IpAddress.String);

		/* don't count the any address which seems to be returned as the first address for interfaces with no addresses */
		if ( (addr != INADDR_NONE) && (addr != INADDR_ANY) ) {
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].addr.inAddr.S_un.S_addr = inet_addr(currentIPAddress->IpAddress.String);
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].length = sizeof(struct in_addr);
			interfaces[currentAdapterIndex].addresses[currentIPAddressIndex].scope = 0;
			currentIPAddressIndex = currentIPAddressIndex + 1;
		}
		currentIPAddress = currentIPAddress->Next;
	}
	return 0;
}

/**
 * Queries and returns the information for the network interfaces that are currently active within the system. 
 * Applications are responsible for freeing the memory returned via the handle.
 *
 * @param[in] portLibrary The port library.
 * @param[in,out] array Pointer to structure with array of network interface entries
 * @param[in] boolean which indicates if we should prefer the IPv4 stack or not
 *
 * @return	0 on success, negative portable error code on failure.
			J9PORT_ERROR_SOCKET_NORECOVERY if system calls required to get the info fail, J9PORT_ERROR_SOCKET_NOBUFFERS if memory allocation fails
 */
int32_t
j9sock_get_network_interfaces(struct J9PortLibrary *portLibrary, struct j9NetworkInterfaceArray_struct *array,BOOLEAN preferIPv4Stack)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}


/**
 * Frees the memory allocated for the j9NetworkInterface_struct array passed in
 *
 * @param[in] portLibrary The port library.
 * @param[in] handle Pointer to array of network interface structures to be freed
 *
 * @return 0 on success
*/
int32_t
j9sock_free_network_interface_struct(struct J9PortLibrary *portLibrary, struct j9NetworkInterfaceArray_struct* array )
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}


/**
 * Establish a connection to a peer with a timeout.  This function is called repeatedly in order to carry out the connect and to allow
 * other tasks to proceed on certain platforms.  The caller must first call with step = J9_SOCK_STEP_START, if the result is J9_ERROR_SOCKET_NOTCONNECTED
 * it will then call it with step = CHECK until either another error or 0 is returned to indicate the connect is complete.  Each time the function should sleep for no more than
 * timeout milliseconds.  If the connect succeeds or an error occurs, the caller must always end the process by calling the function with step = J9_SOCK_STEP_DONE
 *
 * @param[in] portLibrary The port library.
 * @param[in] sock pointer to the unconnected local socket.
 * @param[in] addr	pointer to the sockaddr, specifying remote host/port.
 * @param[in] timeout  timeout in milliseconds 
 * @param[in,out] pointer to context pointer.  Filled in on first call and then to be passed into each subsequent call
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_connect_with_timeout(struct J9PortLibrary *portLibrary, j9socket_t sock, j9sockaddr_t addr, uint32_t timeout, uint32_t step, uint8_t** context)
{
	return J9PORT_ERROR_SOCKET_BADSOCKET;
}

/* Note: j9fdset_t does not support multiple entries. */
void
j9sock_fdset_zero(struct J9PortLibrary *portLibrary, j9fdset_t j9fdset) 
{

	FD_ZERO(&j9fdset->handle);
	return;

}

/* Note: j9fdset_t does not support multiple entries. */
void
j9sock_fdset_set(struct J9PortLibrary *portLibrary, j9socket_t aSocket, j9fdset_t j9fdset) 
{
	
	if( aSocket->flags & SOCKET_IPV4_OPEN_MASK ) {
		FD_SET( aSocket->ipv4, &j9fdset->handle);
	} 
	if( aSocket->flags & SOCKET_IPV6_OPEN_MASK ) {
		FD_SET( aSocket->ipv6, &j9fdset->handle);
	}
	
	return;
}

/* Note: j9fdset_t does not support multiple entries. */
void
j9sock_fdset_clr(struct J9PortLibrary *portLibrary, j9socket_t aSocket, j9fdset_t j9fdset) 
{
	
	if( aSocket->flags & SOCKET_IPV4_OPEN_MASK ) {
		FD_CLR( aSocket->ipv4, &j9fdset->handle);
	} 
	if( aSocket->flags & SOCKET_IPV6_OPEN_MASK ) {
		FD_CLR( aSocket->ipv6, &j9fdset->handle);
	}
	
	return;
}

/* @internal Check if the ipv4 or ipv6 sockets in the fdset are set if they are open
 * If both are set (as in FD_ISSET returned non-zero for both), defer to the ipv4 socket is it takes precedence
 *
 * Note: j9fdset_t does not support multiple entries.
 */ 
BOOLEAN
j9sock_fdset_isset(struct J9PortLibrary *portLibrary, j9socket_t aSocket, j9fdset_t j9fdset) 
{
	
	BOOLEAN rc = FALSE;
	int ipv6FdIsSet, ipv4FdIsSet = 0;
	
	if( aSocket->flags & SOCKET_IPV6_OPEN_MASK ) {
		ipv6FdIsSet = FD_ISSET(aSocket->ipv6, &j9fdset->handle);
		if (0 != ipv6FdIsSet) {
			rc = TRUE;
			aSocket->flags = aSocket->flags & ~SOCKET_USE_IPV4_MASK;
		}
	}
	if( aSocket->flags & SOCKET_IPV4_OPEN_MASK ) {
		/* update IPv4 last, so it will be used in the event both sockets had activity */
		ipv4FdIsSet = FD_ISSET(aSocket->ipv4, &j9fdset->handle);
		if (0 != ipv4FdIsSet) {
			rc = TRUE;
			aSocket->flags = aSocket->flags | SOCKET_USE_IPV4_MASK;
		}
	}

	return rc;
}
