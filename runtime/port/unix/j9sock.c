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

/* For the Z/OS 1.9 compiler */
#define _OPEN_SYS_IF_EXT

#include "j9sock.h"
#include "omrmutex.h"
#include "portpriv.h"
#include "omrportptb.h"
#include "ut_j9prt.h"
#include <fcntl.h>
#include <string.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* for struct in_addr */
#include <sys/ioctl.h>
#include <net/if.h> /* for struct ifconf */
#if defined(LINUX) || defined(OSX)
#include <arpa/inet.h>
#endif

#if defined(J9ZTPF) || defined(OSX)
#undef GLIBC_R
#endif /* defined(J9ZTPF) || defined(OSX) */

#if defined(J9ZOS390)
#include "atoe.h"
#endif


static int32_t findError (int32_t errorCode);
static int32_t map_protocol_family_J9_to_OS( int32_t addr_family);
static int32_t map_addr_family_J9_to_OS( int32_t addr_family);
static int32_t map_sockettype_J9_to_OS( int32_t socket_type);
static int32_t findHostError (int herr);

/*the number of times to retry a gethostby* call if the return code is TRYAGAIN*/
#define MAX_RETRIES 50


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
		case J9PORT_ERROR_SOCKET_UNIX_EBADF:
			return J9PORT_ERROR_SOCKET_BADDESC;
		case J9PORT_ERROR_SOCKET_UNIX_ENOBUFS:
			return J9PORT_ERROR_SOCKET_NOBUFFERS;
		case J9PORT_ERROR_SOCKET_UNIX_EOPNOTSUP:
			return J9PORT_ERROR_SOCKET_OPNOTSUPP;
		case J9PORT_ERROR_SOCKET_UNIX_ENOPROTOOPT:
			return J9PORT_ERROR_SOCKET_OPTUNSUPP;
		case J9PORT_ERROR_SOCKET_UNIX_EINVAL:
			return J9PORT_ERROR_SOCKET_SOCKLEVELINVALID;
		case J9PORT_ERROR_SOCKET_UNIX_ENOTSOCK:
			return J9PORT_ERROR_SOCKET_NOTSOCK;
		case J9PORT_ERROR_SOCKET_UNIX_EINTR:
			return J9PORT_ERROR_SOCKET_INTERRUPTED;
		case J9PORT_ERROR_SOCKET_UNIX_ENOTCONN:
			return J9PORT_ERROR_SOCKET_NOTCONNECTED;
		case J9PORT_ERROR_SOCKET_UNIX_EAFNOSUPPORT:
			return J9PORT_ERROR_SOCKET_BADAF;
		/* note: J9PORT_ERROR_SOCKET_UNIX_ECONNRESET not included because it has the same
		 * value as J9PORT_ERROR_SOCKET_UNIX_CONNRESET and they both map to J9PORT_ERROR_SOCKET_CONNRESET */
		case J9PORT_ERROR_SOCKET_UNIX_CONNRESET:
			return J9PORT_ERROR_SOCKET_CONNRESET;
		case J9PORT_ERROR_SOCKET_UNIX_EAGAIN:
#if defined(J9ZOS390)
		case EWOULDBLOCK:	
#endif
			return J9PORT_ERROR_SOCKET_WOULDBLOCK;
		case J9PORT_ERROR_SOCKET_UNIX_EPROTONOSUPPORT:
			return J9PORT_ERROR_SOCKET_BADPROTO;
		case J9PORT_ERROR_SOCKET_UNIX_EFAULT:
			return J9PORT_ERROR_SOCKET_ARGSINVALID;
		case J9PORT_ERROR_SOCKET_UNIX_ETIMEDOUT:
			return J9PORT_ERROR_SOCKET_TIMEOUT;
		case J9PORT_ERROR_SOCKET_UNIX_CONNREFUSED:
			return J9PORT_ERROR_SOCKET_CONNECTION_REFUSED;
		case J9PORT_ERROR_SOCKET_UNIX_ENETUNREACH:
			return J9PORT_ERROR_SOCKET_ENETUNREACH;
		case J9PORT_ERROR_SOCKET_UNIX_EACCES:
			return J9PORT_ERROR_SOCKET_EACCES;
		case J9PORT_ERROR_SOCKET_UNIX_EINPROGRESS:
			return J9PORT_ERROR_SOCKET_EINPROGRESS;
		case J9PORT_ERROR_SOCKET_UNIX_EALREADY:	
			return J9PORT_ERROR_SOCKET_ALREADYINPROGRESS;
		case J9PORT_ERROR_SOCKET_UNIX_EISCONN:	
			return J9PORT_ERROR_SOCKET_ISCONNECTED;
		case J9PORT_ERROR_SOCKET_UNIX_EADDRINUSE:
			return J9PORT_ERROR_SOCKET_ADDRINUSE;
		case J9PORT_ERROR_SOCKET_UNIX_EADDRNOTAVAIL:
			return J9PORT_ERROR_SOCKET_ADDRNOTAVAIL;
#if !defined(J9ZTPF)
		case J9PORT_ERROR_SOCKET_UNIX_ENOSR:
			return J9PORT_ERROR_SOCKET_NOSR;
#endif /* !defined(J9ZTPF) */
		case J9PORT_ERROR_SOCKET_UNIX_EIO:
			return J9PORT_ERROR_SOCKET_IO;
		case J9PORT_ERROR_SOCKET_UNIX_EISDIR:
			return J9PORT_ERROR_SOCKET_ISDIR;
		case J9PORT_ERROR_SOCKET_UNIX_ELOOP:
			return J9PORT_ERROR_SOCKET_LOOP;
		case J9PORT_ERROR_SOCKET_UNIX_ENOENT:
			return J9PORT_ERROR_SOCKET_NOENT;
		case J9PORT_ERROR_SOCKET_UNIX_ENOTDIR:
			return J9PORT_ERROR_SOCKET_NOTDIR;
		case J9PORT_ERROR_SOCKET_UNIX_EROFS:
			return J9PORT_ERROR_SOCKET_ROFS;
		case J9PORT_ERROR_SOCKET_UNIX_ENOMEM:
			return J9PORT_ERROR_SOCKET_NOMEM;
		case J9PORT_ERROR_SOCKET_UNIX_ENAMETOOLONG:
			return J9PORT_ERROR_SOCKET_NAMETOOLONG;
		default:
			return J9PORT_ERROR_SOCKET_OPFAILED;
	}
}





/**
 * @internal Determines the proper portable error code to return given a native host error code
 *
 * @return	the (negative) error code
 */
static int32_t
findHostError (int herr)
{
	switch (herr)
	{
		case J9PORT_ERROR_SOCKET_UNIX_NORECOVERY:
			return J9PORT_ERROR_SOCKET_NORECOVERY;
		case J9PORT_ERROR_SOCKET_UNIX_HOSTNOTFOUND:
			return J9PORT_ERROR_SOCKET_HOSTNOTFOUND;
		case J9PORT_ERROR_SOCKET_UNIX_NODATA:
			return J9PORT_ERROR_SOCKET_NODATA;
		case J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN:
			return J9PORT_ERROR_SOCKET_INTERRUPTED;
		default:
			return J9PORT_ERROR_SOCKET_BADSOCKET;
	}
}





/**
 * @internal Map the portable address family to the platform address family. 
 *
 * @param	addr_family	the portable address family to convert
 *
 * @return	the platform family, or OS_AF_UNSPEC if none exists
 */
static int32_t
map_addr_family_J9_to_OS( int32_t addr_family)
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
 * @param	addr_protocol  the portable address protocol to convert
 *
 * @return	the platform family, or OS_PF_UNSPEC if none exists
 */
static int32_t
map_protocol_family_J9_to_OS( int32_t addr_family)
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
 * @param	addr_family	the portable socket type to convert
 *
 * @return	the platform family, or OSSOCK_ANY if none exists
 */
static int32_t
map_sockettype_J9_to_OS( int32_t socket_type)
{
	switch( socket_type )
	{
		case J9SOCKET_STREAM:
			return OSSOCK_STREAM;
		case J9SOCKET_DGRAM:
			return OSSOCK_DGRAM;
		case J9SOCKET_RAW:
			return OSSOCK_RAW;
#if !defined(J9ZTPF)
		case J9SOCKET_RDM:
			return OSSOCK_RDM;
		case J9SOCKET_SEQPACKET:
			return OSSOCK_SEQPACKET;
#endif /*!defined(J9ZTPF) */
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

#ifdef IPv6_FUNCTION_SUPPORT
	freeaddrinfo(  (OSADDRINFO *)handle->addr_info );
#endif 

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
	/* If we have the IPv6 functions available we will call them, otherwise we'll call the IPv4 function */
#ifdef IPv6_FUNCTION_SUPPORT
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	OSADDRINFO *ipv6_result;
	OSADDRINFO *addr_info_hints = NULL;
	int count = 0;

	if( hints != NULL ) {
		addr_info_hints = (OSADDRINFO *) hints->addr_info;
	}

	if (0 != getaddrinfo(name, NULL, addr_info_hints, &ipv6_result)) {
		int32_t errorCode = errno;
		J9SOCKDEBUG( "<getaddrinfo failed, err=%d>\n", errorCode);
		return omrerror_set_last_error(errorCode, findError(errorCode));
	}

	memset(result, 0, sizeof(struct j9addrinfo_struct));
	result->addr_info = (void *) ipv6_result;
	while (NULL != ipv6_result->ai_next) {
		count++;
		ipv6_result = ipv6_result->ai_next;
	}
	result->length = ++count; /* Have to add an extra, because we didn't count the first entry */
	
	return 0;

#else /* IPv6_FUNCTION_SUPPORT */

	int32_t rc    = 0;
	uint32_t addr  = 0;
	uint32_t count = 0;
	struct j9hostent_struct j9hostent;

	if(0 != portLibrary->sock_inetaddr(portLibrary, name, &addr)) {
		if(0 != (rc = portLibrary->sock_gethostbyname(portLibrary, name, &j9hostent))) {
			return rc;
		}
	} else {
		if(0 != (rc = portLibrary->sock_gethostbyaddr(portLibrary, (char *)&addr, sizeof(addr), J9SOCK_AFINET, &j9hostent))) {
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

	return 0;
#endif /* IPv6_FUNCTION_SUPPORT */
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
#ifndef IPv6_FUNCTION_SUPPORT
	char ** addr_list;
#endif /* IPv6_FUNCTION_SUPPORT */
	int i;

	/* If we have the IPv6 functions available we cast to an OSADDRINFO structure otherwise a OSHOSTENET structure */
#ifdef IPv6_FUNCTION_SUPPORT
	addr = (OSADDRINFO *) handle->addr_info;
	for( i=0; i<index; i++ ) {
		addr = addr->ai_next;
	}
	if( addr->ai_family == OS_AF_INET6 ) {
		sock_addr = ((OSSOCKADDR_IN6 *)addr->ai_addr)->sin6_addr.s6_addr;
		memcpy( address, sock_addr, 16 );
		*scope_id =  ((OSSOCKADDR_IN6 *)addr->ai_addr)->sin6_scope_id;
	} else {
		sock_addr = &((OSSOCKADDR *)addr->ai_addr)->sin_addr.s_addr;
		memcpy( address, sock_addr, 4 );
	}
#else
	addr_list = ((OSHOSTENT *) handle->addr_info)->h_addr_list;
	for( i=0; i<index; i++ ) {
		if( addr_list[i] == NULL ) {
			return J9PORT_ERROR_SOCKET_VALUE_NULL;
		}
	}
	memcpy( address, addr_list[index], 4 );
#endif

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
	int32_t rc = 0;
#if defined(IPv6_FUNCTION_SUPPORT)
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	OSADDRINFO *addrinfo;
	J9SocketPTB *ptBuffers = NULL;
#endif /* IPv6_FUNCTION_SUPPORT */

	*result = NULL;

	/* If we have the IPv6 functions available we fill in the structure, otherwise it is null */
#if defined(IPv6_FUNCTION_SUPPORT)
#define addrinfohints (ptBuffers->addr_info_hints).addr_info

	/* Initialized the pt buffers if necessary */
	ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}

	if (NULL == addrinfohints) {
		addrinfohints = omrmem_allocate_memory(sizeof(OSADDRINFO), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == addrinfohints) {
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

#undef addrinfohints
#endif  /*IPv6_FUNCTION_SUPPORT*/

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
#ifdef IPv6_FUNCTION_SUPPORT
	addr = (OSADDRINFO *) handle->addr_info;
	for( i=0; i<index; i++ ) {
		addr = addr->ai_next;
	}

	if( addr->ai_family == OS_AF_INET4 ) {
		*family = J9ADDR_FAMILY_AFINET4;
	} else {
		*family = J9ADDR_FAMILY_AFINET6;
	}
#else
	*family = J9ADDR_FAMILY_AFINET4;
#endif

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
	int32_t rc = 0;
#ifndef IPv6_FUNCTION_SUPPORT
	char ** alias_list;
#endif /* IPv6_FUNCTION_SUPPORT */
	int i;
	OSADDRINFO *addr;

	/* If we have the IPv6 functions available we cast to an OSADDRINFO structure otherwise a OSHOSTENET structure */
#ifdef IPv6_FUNCTION_SUPPORT

	addr = (OSADDRINFO *) handle->addr_info;
	for( i=0; i<index; i++ ) {
		addr = addr->ai_next;
	}
	if( addr->ai_canonname == NULL ) {
		name[0] = 0;
	} else {
		strcpy( name, addr->ai_canonname );
	}
#else
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
#endif

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
#if defined(J9ZTPF) || defined(OSX) /* TODO: OSX: Revisit this after java -version works */ 
    int herr = NO_RECOVERY;
    OMRPORT_ACCESS_FROM_J9PORT(portLibrary);

    J9SOCKDEBUGH("<gethostbyaddr failed, err=%d>\n", herr);
    return omrerror_set_last_error(herr, findHostError(herr));

#else /* defined(J9ZTPF) || defined(OX) */

#if !HOSTENT_DATA_R
	OSHOSTENT *result;
#endif
#if GLIBC_R || OTHER_R
	BOOLEAN allocBuffer = FALSE;
#endif
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int herr = 0;
	int i = 0;

	J9SocketPTB *ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}
#define hostentBuffer (&ptBuffers->hostent)

	/* PR 94621 - one of several threadsafe gethostbyaddr calls must be made depending on the current platform */
	/* if there is a transient error (J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN), try making the call again */
	for (i = 0; i < MAX_RETRIES; i ++) {
#if HOSTENT_DATA_R
#define dataBuffer (ptBuffers->hostent_data)
		if (NULL == dataBuffer) {
			dataBuffer = omrmem_allocate_memory(sizeof(OSHOSTENT_DATA), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == dataBuffer) {
				return J9PORT_ERROR_SOCKET_SYSTEMFULL;
			}
		}
		herr = gethostbyaddr_r(addr,length,type, hostentBuffer, dataBuffer);
#undef dataBuffer
#elif GLIBC_R || OTHER_R
#define buffer (ptBuffers->gethostBuffer)
#define bufferSize (ptBuffers->gethostBufferSize)
		if (NULL == buffer) {
			bufferSize = GET_HOST_BUFFER_SIZE;
		}

		while (TRUE) {
			if (allocBuffer == TRUE || (NULL == buffer)) {
				/* The buffer is allocated bufferSize + EXTRA_SPACE, while gethostby*_r is only aware of bufferSize
				 * because there seems to be a bug on Linux 386 where gethostbyname_r writes past the end of the
				 * buffer.  This bug has not been observed on other platforms, but EXTRA_SPACE is added anyway as a precaution.*/
				buffer = omrmem_allocate_memory(bufferSize + EXTRA_SPACE, OMRMEM_CATEGORY_PORT_LIBRARY);
				if (NULL == buffer) {
					return J9PORT_ERROR_SOCKET_SYSTEMFULL;
				}
				allocBuffer = FALSE;
			}
#if GLIBC_R
			gethostbyaddr_r(addr,length,type, hostentBuffer, buffer, bufferSize, &result, &herr);
#elif OTHER_R
			result = gethostbyaddr_r(addr,length,type, hostentBuffer, buffer, bufferSize, &herr);
#endif /* GLIBC_R */
			/* allocate more space if the buffer is too small */
			if (ERANGE == herr) {
				omrmem_free_memory(buffer);
				bufferSize *= 2;
				allocBuffer = TRUE;
			} else {
				break;
			}
		}
#undef buffer
#undef bufferSize
#endif
		if (herr != J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN) {
			break;
		}
	}

#if HOSTENT_DATA_R
	if (0 != herr) {
		herr = h_errno;
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr);
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#else
	if (NULL == result) {
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr );
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#endif
	else {
		memset(handle, 0, sizeof(struct j9hostent_struct));
#if HOSTENT_DATA_R
		handle->entity = hostentBuffer;
#else
		handle->entity = result;
#endif
	}

#undef hostentBuffer

	return 0;
#endif /* defined(J9ZTPF) */
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
#if defined(OSX) /* TODO: OSX: Revisit this after java -version works */
	return -1;
#else /* OSX */

#if !HOSTENT_DATA_R
	OSHOSTENT *result;
#endif
#if GLIBC_R || OTHER_R
	BOOLEAN allocBuffer = FALSE;
#endif
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	int herr = 0;
	int i = 0;

	J9SocketPTB *ptBuffers = j9sock_ptb_get(portLibrary);
	if (NULL == ptBuffers) {
		return J9PORT_ERROR_SOCKET_SYSTEMFULL;
	}
#define hostentBuffer (&ptBuffers->hostent)

	/* PR 94621 - one of several threadsafe gethostbyname calls must be made depending on the current platform */
	/* if there is a transient error (J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN), try making the call again */
	for (i = 0; i < MAX_RETRIES; i ++) {
#if HOSTENT_DATA_R
#define dataBuffer (ptBuffers->hostent_data)
		if (NULL == dataBuffer) {
			dataBuffer = omrmem_allocate_memory(sizeof(OSHOSTENT_DATA), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == dataBuffer) {
				return J9PORT_ERROR_SOCKET_SYSTEMFULL;
			}
		}
		herr = gethostbyname_r(name, hostentBuffer, dataBuffer);
#undef dataBuffer
#elif GLIBC_R || OTHER_R
#define buffer (ptBuffers->gethostBuffer)
#define bufferSize (ptBuffers->gethostBufferSize)
		if (!buffer) {
			bufferSize = GET_HOST_BUFFER_SIZE;
		}
		
		while (TRUE) {
			if (allocBuffer == TRUE || !buffer) {
				/* The buffer is allocated bufferSize + EXTRA_SPACE, while gethostby*_r is only aware of bufferSize
				 * because there seems to be a bug on Linux 386 where gethostbyname_r writes past the end of the
				 * buffer.  This bug has not been observed on other platforms, but EXTRA_SPACE is added anyway as a precaution.*/
				buffer = omrmem_allocate_memory(bufferSize + EXTRA_SPACE, OMRMEM_CATEGORY_PORT_LIBRARY);
				if (!buffer) {
					return J9PORT_ERROR_SOCKET_SYSTEMFULL;
				}
				allocBuffer = FALSE;
			}
#if GLIBC_R
			gethostbyname_r(name, hostentBuffer, buffer, bufferSize, &result, &herr);
#elif OTHER_R
			result = gethostbyname_r((char *)name, hostentBuffer, buffer, bufferSize, &herr);
#endif /* GLIBC_R */
			/* allocate more space if the buffer is too small */
			if (ERANGE == herr) {
				omrmem_free_memory(buffer);
				bufferSize *= 2;
				allocBuffer = TRUE;
			} else {
				break;
			}
		}
#undef buffer
#undef bufferSize
#endif
		if (herr != J9PORT_ERROR_SOCKET_UNIX_TRYAGAIN) {
			break;
		}
	}

#if HOSTENT_DATA_R
	if (0 != herr) {
		herr = h_errno;
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr );
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#else
	if (NULL == result) {
		J9SOCKDEBUGH( "<gethostbyaddr failed, err=%d>\n", herr );
		return omrerror_set_last_error(herr, findHostError(herr));
	}
#endif
	else {
		memset(handle, 0, sizeof(struct j9hostent_struct));
#if HOSTENT_DATA_R
		handle->entity = hostentBuffer;
#else
		handle->entity = result;
#endif
	}

#undef hostentBuffer

	return 0;
#endif /* !OSX */
}





/**
 * Answer the dotted IP string as an Internet address.
 *
 * @param[in] portLibrary The port library.
 * @param[in] addrStr The dotted IP string.
 * @param[out] addr Pointer to the Internet address.
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
j9sock_inetaddr(struct J9PortLibrary *portLibrary, const char *addrStr, uint32_t *addr)
{
	int32_t rc = 0;
	uint32_t val;

	val = inet_addr((char *)addrStr);
	if (-1 == val) {
		J9SOCKDEBUGPRINT( "<inet_addr failed>\n" );
		rc = J9PORT_ERROR_SOCKET_ADDRNOTAVAIL;
	} else {
		*addr = val;
	}
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
	int32_t rc = 0;
	if (0 != j9sock_ptb_init(portLibrary)) {
		rc = J9PORT_ERROR_STARTUP_SOCK;
	}
	return rc;
}
