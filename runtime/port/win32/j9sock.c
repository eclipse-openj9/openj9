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
#include <limits.h>

static int32_t ensureConnected(struct J9PortLibrary *portLibrary);
static int32_t findError (int32_t errorCode);
int32_t map_protocol_family_J9_to_OS(int32_t addr_family);
int32_t map_addr_family_J9_to_OS(int32_t addr_family);
int32_t map_sockettype_J9_to_OS(int32_t socket_type);

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
