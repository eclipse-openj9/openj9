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

/*
 * $RCSfile: j9sockTest.c,v $
 * $Revision: 1.49 $
 * $Date: 2012-11-23 21:11:32 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library file system.
 * 
 * Exercise the API for port library socket system operations.  These functions 
 * can be found in the file @ref j9sock.c  
 * 
 * @note port library socket operations are optional in the port library table.
 * @note The default processing if memory mapping is not supported by the OS is to allocate
 *       memory and read in the file.  This is the equivalent of copy-on-write mmapping
 * 
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9cfg.h"
#include "j9port.h"
#include "portsock.h"
#include "j9socket.h"

#include "testHelpers.h"
#include "testProcessHelpers.h"
#include "j9fileTest.h"

static I_32 connectClientToServerNonBlocking(struct J9PortLibrary *portLibrary, const char* testName, char *addrStr, U_16 port, j9socket_t *session_client_socket, j9sockaddr_t session_client_addr);



#if 0
#define SOCKTEST_DBG
#endif

#ifdef IPv6_FUNCTION_SUPPORT
void printAddress(struct J9PortLibrary *portLibrary, const struct sockaddr *address) {
	char ipstr[INET_ADDRSTRLEN];
	PORT_ACCESS_FROM_PORT(portLibrary);

    switch(address->sa_family) {
        case AF_INET:
        	/*
            inet_ntop(AF_INET, &(((struct sockaddr_in *)address)->sin_addr), ipstr, sizeof ipstr);
    		j9tty_printf(portLibrary, "AF_INET %s\n", ipstr);
    		 */
            break;

        case AF_INET6:
        	/*
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)address)->sin6_addr), ipstr, sizeof ipstr);
            j9tty_printf(portLibrary, "AF_INET6 %s\n", ipstr);
             */
            break;

        default:
        	j9tty_printf(portLibrary, "Unknown address family: %s\n", ipstr);
    }

}
#endif /* IPv6_FUNCTION_SUPPORT */


/**
 * Verify port library properly setup to run j9sock tests
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on failure
 */
int
j9sock_verify_function_slots(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_verify_function_slots";

	reportTestEntry(portLibrary, testName);

	if (NULL == portLibrary->sock_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_startup is NULL\n");
	}

	if (NULL == portLibrary->sock_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_shutdown is NULL\n");
	}

	if (NULL == portLibrary->sock_htons) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_htons is NULL\n");
	}

	if (NULL == portLibrary->sock_write) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_write is NULL\n");
	}

	if (NULL == portLibrary->sock_sockaddr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_sockaddr is NULL\n");
	}

	if (NULL == portLibrary->sock_read) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_read is NULL\n");
	}

	if (NULL == portLibrary->sock_socket) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_socket is NULL\n");
	}

	if (NULL == portLibrary->sock_close) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_close is NULL\n");
	}

	if (NULL == portLibrary->sock_connect) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_connect is NULL\n");
	}

	if (NULL == portLibrary->sock_inetaddr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_inetaddr is NULL\n");
	}

	if (NULL == portLibrary->sock_gethostbyname) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_gethostbyname is NULL\n");
	}

	if (NULL == portLibrary->sock_hostent_addrlist) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_hostent_addrlist is NULL\n");
	}

	if (NULL == portLibrary->sock_sockaddr_init) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_sockaddr_init is NULL\n");
	}

	if (NULL == portLibrary->sock_linger_init) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_linger_init is NULL\n");
	}

	if (NULL == portLibrary->sock_setopt_linger) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->sock_setopt_linger is NULL\n");
	}
	

	return reportTestExit(portLibrary, testName);

}


/* 
 * Start a server: create socket, bind to an address/port and listen for clients
 * 
 * @param[in] portLibrary
 * @param[in] testname	the name of the calling test
 * @param[in] addrStr the address of the server
 * @param[in] port the server port
 * @param[in] server_socket	a pointer to the server socket. 
 * @param[in] server_addr 
 * 
 * @return 0 on success, non-zero otherwise
 */ 
I_32 
startServer(struct J9PortLibrary *portLibrary, const char* testName, char *addrStr, U_16 port, j9socket_t *server_socket, j9sockaddr_t server_addr) {
	
	PORT_ACCESS_FROM_PORT(portLibrary);
	BOOLEAN value = 1;
	
	/* server_socket: create, bind */
	if (0 != j9sock_socket(server_socket, J9SOCK_AFINET, J9SOCK_STREAM, J9SOCK_DEFPROTOCOL)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating socket: %s\n", j9error_last_error_message());
		return -1;
	}

	if (0 != j9sock_sockaddr(server_addr, addrStr, j9sock_htons(port))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating address: %s\n", j9error_last_error_message());
		return -1;
	}

	if (0 != j9sock_setopt_bool(*server_socket, J9_SOL_SOCKET, J9_SO_REUSEADDR, &value)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error setting socket option: %s\n", j9error_last_error_message());
		return -1;
		
	}
	
	if (0 != j9sock_bind(*server_socket, server_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error binding socket: %s\n", j9error_last_error_message());
		return -1;
	}

	if (0 != j9sock_listen(*server_socket, 1)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error binding listening to socket: %s\n", j9error_last_error_message());
		return -1;
	}
	
	return 0;
}

/* 
 * Create the client socket and sock address, then connect to the server
 *  
 * @param[in] portLibrary
 * @param[in] testname	the name of the calling test
 * @param[in] addrStr the address of the server
 * @param[in] port the server port
 * @param[in] session_client_socket	a pointer to the client socket. 
 * @param[in] server_addr 
 * 
 * @return 0 on success, non-zero otherwise
 */ 
I_32 
connectClientToServer(struct J9PortLibrary *portLibrary, const char* testName, char *addrStr, U_16 port, j9socket_t *session_client_socket, j9sockaddr_t session_client_addr) {
	
	PORT_ACCESS_FROM_PORT(portLibrary);
	BOOLEAN value = 1;
	
	/* server_socket: create, bind */
	if (0 != j9sock_socket(session_client_socket, J9SOCK_AFINET, J9SOCK_STREAM, J9SOCK_DEFPROTOCOL)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating socket: %s\n", j9error_last_error_message());
		return -1;
	}

	if (0 != j9sock_sockaddr(session_client_addr, addrStr, j9sock_htons(port))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating address: %s\n", j9error_last_error_message());
		return -1;
	}

	if (0 != j9sock_setopt_bool(*session_client_socket, J9_SOL_SOCKET, J9_SO_REUSEADDR, &value)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error setting socket option: %s\n", j9error_last_error_message());
		return -1;
	}
	
	if (0 != j9sock_connect(*session_client_socket, session_client_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error connecting to socket: %s\n", j9error_last_error_message());
		return -1;
	}

	return 0;
}

/* 
 * Create the client socket and sock address, set the client 
 * socket to non-blocking then connect to the server
 *  
 * @param[in] portLibrary
 * @param[in] testname	the name of the calling test
 * @param[in] addrStr the address of the server
 * @param[in] port the server port
 * @param[in] session_client_socket	a pointer to the client socket. 
 * @param[in] server_addr 
 * 
 * @return 0 on success, non-zero otherwise
 */ 
static I_32 
connectClientToServerNonBlocking(struct J9PortLibrary *portLibrary, const char* testName, char *addrStr, U_16 port, j9socket_t *session_client_socket, j9sockaddr_t session_client_addr) {
#define MAX_LOOP_TIMES 300
	PORT_ACCESS_FROM_PORT(portLibrary);
	BOOLEAN value = 1;
	I_32 rc = 0;
	I_32 count = 0;
	I_32 expectedRC = 0;
	
	/* server_socket: create, bind */
	if (0 != j9sock_socket(session_client_socket, J9SOCK_AFINET, J9SOCK_STREAM, J9SOCK_DEFPROTOCOL)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating socket: %s\n", j9error_last_error_message());
		return -1;
	}

	if ( 0 != j9sock_set_nonblocking(*session_client_socket, TRUE) ) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error setting socket to non-blocking: %s\n", j9error_last_error_message());
		return -1;
	}
	
	if (0 != j9sock_sockaddr(session_client_addr, addrStr, j9sock_htons(port))) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating address: %s\n", j9error_last_error_message());
		return -1;
	}

	if (0 != j9sock_setopt_bool(*session_client_socket, J9_SOL_SOCKET, J9_SO_REUSEADDR, &value)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error setting socket option: %s\n", j9error_last_error_message());
		return -1;
	}
	
	while ( 0 != (rc = j9sock_connect(*session_client_socket, session_client_addr)) ) {
		count++;
		switch (count) {
			case 1:
				expectedRC = J9PORT_ERROR_SOCKET_EINPROGRESS;
				break;
			default:
				expectedRC = J9PORT_ERROR_SOCKET_ALREADYINPROGRESS;
				break;
		}
		
		if ( count > 1 ) {
			/*After the first try if j9sock_connect returns J9PORT_ERROR_SOCKET_ISCONNECTED 
			 * or J9PORT_ERROR_SOCKET_ALREADYBOUND we can stop trying
			 */
			if (J9PORT_ERROR_SOCKET_ISCONNECTED == rc) {
				outputComment(portLibrary, "j9sock_connect returned socket_isconnected\n");
				break;
			} else if (J9PORT_ERROR_SOCKET_ALREADYBOUND == rc) {
				outputComment(portLibrary, "j9sock_connect returned socket_alreadybound\n");
				break;
			}
		}
		
		if ( rc != expectedRC ) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error connecting to socket (expectedRC=%d, rc=%d, count=%d): %s\n", expectedRC, rc, count, j9error_last_error_message());
			return -1;
		} else if ( (2 <= count) && (MAX_LOOP_TIMES > count) ) {
			/* After the second try we have already tested what we want to test so
			 * just give the connect some time to complete 
			 */
			omrthread_sleep((I_64)100); /* 100 ms*/
		} else if ( MAX_LOOP_TIMES <= count ) {
			/* Connection attempt timed out */
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error connecting to socket, reached MAX_LOOP_TIMES limit (=%d)\n", MAX_LOOP_TIMES);
			return -1;
		}
	}
	
	outputComment(portLibrary, "Connected after %d attempt(s).\n", count);
	
	return 0;
#undef MAX_LOOP_TIMES
}

/**
 * Open two sockets and talk to eachother.
 * 
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sock_test1_basic(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test1_basic";
	char *address = "localhost";
	char *message = "hey, what's up?";
	U_8 *cursor = NULL;
	U_16 port = 8888;
	int lenMessagePlusTerminator;
	int bytesLeft, bytesWritten, bytesRead;
	BOOLEAN value = 1;
	char bufferA[EsMaxPath], bufferB[EsMaxPath];
	IDATA selectReadRC = -1;
	
	j9socket_struct server_socketStruct, session_client_socketStruct, session_server_socketStruct;
	j9socket_t server_socket = &server_socketStruct;
	j9socket_t session_client_socket = &session_client_socketStruct;
	j9socket_t session_server_socket = &session_server_socketStruct;
	
	j9sockaddr_struct server_socket_addrStruct, session_client_addrStruct, session_server_addrStruct;
	j9sockaddr_t server_addr = &server_socket_addrStruct;
	j9sockaddr_t session_client_addr = &session_client_addrStruct;
	j9sockaddr_t session_server_addr = &session_server_addrStruct;
	
	reportTestEntry(portLibrary, testName);
		
	/* printf("\nj9sock_test1 : session_server_addr->addr: %p  \n", session_server_addr->addr);fflush(stdout); */
	
	lenMessagePlusTerminator = (int)strlen(message) +1;
	memset(bufferA, 0, sizeof(bufferA));
	memset(bufferB, 0, sizeof(bufferB));

	if (0 != startServer(portLibrary, testName, address, port, &server_socket, server_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error starting server: %s\n", j9error_last_error_message());
	}
	
	if (0 != connectClientToServer(portLibrary, testName, address, port, &session_client_socket, session_client_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating/connecting client: %s\n", j9error_last_error_message());
	}

	/* TODO: DEFECT it appears that the address is not optional as win32 does not test for NULL in sock_accept. Fix or DOC */
	if (0 != j9sock_accept(server_socket, session_server_addr, &session_server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error accepting connection: %s\n", j9error_last_error_message());
		goto exit;
	}
	
	/*****************************************************
	 * 
	 * We have two sockets that can talk to each other
	 * 
	 *****************************************************/

	/* nothing to read, this should timeout  */
	selectReadRC = j9sock_select_read(session_client_socket, 0, 1, FALSE);
	if (J9PORT_ERROR_SOCKET_TIMEOUT != selectReadRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_select_read: expected rc = %i, received rc: %i : %s\n", J9PORT_ERROR_SOCKET_TIMEOUT, selectReadRC, j9error_last_error_message());
		goto exit;
	}

	/* Write full message to session_server_socket */
	cursor = (U_8*)message;
	bytesLeft = lenMessagePlusTerminator;
	bytesWritten = 0;
	for (;;) {
		bytesWritten = j9sock_write(session_server_socket, cursor, bytesLeft, 0);
		if (bytesWritten < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "full message was not written to server_socket %s\n", j9error_last_error_message());
			goto exit;
		} 
		
		bytesLeft -=bytesWritten;
		cursor += bytesWritten;
		if (0 == bytesLeft) {
			break;
		}
	}
	
	/*
	 * we should be able to read from this socket.
	 * CMVC 182679: 1 us appears too short.  Increase to 1 ms
	 */
	selectReadRC = j9sock_select_read(session_client_socket, 0, 1000, FALSE);
	if (1 != selectReadRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_select_read: expected rc = 1, received rc: %i : %s\n", selectReadRC, j9error_last_error_message());
		goto exit;
	}

	/* Read full message from session_client_socket */
	cursor = (U_8*)bufferB;
	bytesLeft = lenMessagePlusTerminator;
	bytesRead = 0;
	for (;;) {
		bytesRead = j9sock_read(session_client_socket, cursor, EsMaxPath - bytesRead, 0);
		if (bytesRead < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "full message not received on session_client_socket%s\n", j9error_last_error_message());
			goto exit;
		}
		bytesLeft -=bytesRead;
		cursor +=bytesRead;
		if (0 == bytesLeft) {
			break;
		}

	}
	
	j9tty_printf(portLibrary, "\t session_client_socket received: \"%s\"\n ",bufferB);
	
	/* Close the sockets */
	if (0 != j9sock_close(&session_server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing session_server_socket_socket: %s\n", j9error_last_error_message());
	}
	if (0 != j9sock_close(&session_client_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing session_client_socket: %s\n", j9error_last_error_message());
	}
	if (0 != j9sock_close(&server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing server_socket: %s\n", j9error_last_error_message());
	}
	return reportTestExit(portLibrary, testName);
	
	exit:
	/* cleanup on failure, there should be no harm in calling close on a socket that's already closed */
	j9sock_close(&session_server_socket);
	j9sock_close(&session_client_socket);
	j9sock_close(&server_socket);

	return reportTestExit(portLibrary, testName);
}

/**
 * Basic test of an fdset with only one socket;
 * 
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sock_test2_fdset_basic(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test2_fdset_basic";
	j9socket_struct sock_struct1;
	j9socket_t sock1;
	j9fdset_struct fdsetStruct; 
	j9fdset_t fdset = &fdsetStruct;
	
	sock1 = &sock_struct1;
	
	reportTestEntry(portLibrary, testName);
	

	j9sock_fdset_zero(fdset);

	if (0 != j9sock_socket(&sock1, J9SOCK_AFINET, J9SOCK_STREAM, J9SOCK_DEFPROTOCOL)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating socket: %s\n", j9error_last_error_message());
		goto exit;
	}	
	
	if (TRUE == j9sock_fdset_isset(sock1, fdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should not include sock1: %s\n", j9error_last_error_message());
		goto exit;
	}
	
	j9sock_fdset_set(sock1, fdset);
	
	if (FALSE == j9sock_fdset_isset(sock1, fdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should include sock1: %s\n", j9error_last_error_message());
		goto exit;
	}
	
	j9sock_fdset_clr(sock1, fdset);
	
	if (TRUE == j9sock_fdset_isset(sock1, fdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should not include sock1: %s\n", j9error_last_error_message());
		goto exit;
	}
	
	j9sock_fdset_set(sock1, fdset);
	j9sock_fdset_zero(fdset);
	
	if (TRUE == j9sock_fdset_isset(sock1, fdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should be empty: %s\n", j9error_last_error_message());
		goto exit;
	}
	
	exit:
	return reportTestExit(portLibrary, testName);

}

/**
 * Basic test of an fdset with only one socket;
 * NOTE: Windows and Linux (?unix?) behave differently for select:
 * 	If the timeout is set to zero:
 * 		Windows: returns 0, regardless of whether any sockets are ready for reads/writes
 * 		Linux: returns the number of sockets that are ready, as expected.
 * 
 * 1. Establish a connection between the server and client sockets
 * 2. Once the connection has been made, a write should not block, but a read should, check this
 * 3. Write some data to the server socket
 * 4. The client socket should not block on a read, check this
 * 5. Read and validate the data
 *
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sock_test3_fdset_basic_select(struct J9PortLibrary *portLibrary) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test3_fdset_basic_select";
	char *address = "localhost";
	char *message = "hey, what's up?";
	U_8 *cursor = NULL;
	U_16 port = 8889;
	int lenMessagePlusTerminator;
	int bytesLeft, bytesWritten, bytesRead;
	BOOLEAN value = 1;
	char bufferA[EsMaxPath], bufferB[EsMaxPath];
	j9fdset_struct readFdsetStruct, writeFdsetStruct;
	j9fdset_t readFdset = &readFdsetStruct;
	j9fdset_t writeFdset = &writeFdsetStruct;

	I_32 nfds;
	struct j9timeval_struct timeP;
	I_32 selectRC = 0;
	
	j9socket_struct server_socketStruct, session_client_socketStruct, session_server_socketStruct;
	j9socket_t server_socket = &server_socketStruct;
	j9socket_t session_client_socket = &session_client_socketStruct;
	j9socket_t session_server_socket = &session_server_socketStruct;
	
	j9sockaddr_struct server_socket_addrStruct, session_client_addrStruct, session_server_addrStruct;
	j9sockaddr_t server_addr = &server_socket_addrStruct;
	j9sockaddr_t session_client_addr = &session_client_addrStruct;
	j9sockaddr_t session_server_addr = &session_server_addrStruct;
	
	reportTestEntry(portLibrary, testName);
		
	/* printf("\nj9sock_test1 : session_server_addr->addr: %p  \n", session_server_addr->addr);fflush(stdout); */
	
	lenMessagePlusTerminator = (int)strlen(message) +1;
	memset(bufferA, 0, sizeof(bufferA));
	memset(bufferB, 0, sizeof(bufferB));

	if (0 != startServer(portLibrary, testName, address, port, &server_socket, server_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error starting server: %s\n", j9error_last_error_message());
	}
	
	if (0 != connectClientToServer(portLibrary, testName, address, port, &session_client_socket, session_client_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating/connecting client: %s\n", j9error_last_error_message());
	}

	/* TODO: DEFECT it appears that the address is not optional as win32 does not test for NULL in sock_accept. Fix or DOC */
	if (0 != j9sock_accept(server_socket, session_server_addr, &session_server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error accepting connection: %s\n", j9error_last_error_message());
		goto exit;
	}
	
	/*****************************************************
	 * 
	 * We have two sockets that can talk to eachother 
	 * 
	 *****************************************************/
	/* Note, Windows select() returns "0" regardless of state of sockets when timeout is set to 0 */
	j9sock_timeval_init(0, 1, &timeP);
	
	/* we should be able to write without blocking, test it */
	j9sock_fdset_zero(writeFdset);
	j9sock_fdset_set(session_server_socket, writeFdset);
	nfds = j9sock_fdset_size(session_server_socket);
	
	selectRC = j9sock_select(nfds, NULL, writeFdset, NULL, &timeP);

	if (selectRC <= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error in j9sock_select: %s\n", j9error_last_error_message());
		goto exit;
	} else if (1 != selectRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_select should have returned exactly one socket in the read fdset: %s\n", j9error_last_error_message());
		goto exit;
	}
	if (TRUE != j9sock_fdset_isset(session_server_socket, writeFdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "session_server_socket should be set %s\n", j9error_last_error_message());
		goto exit;
	}
	if (FALSE != j9sock_fdset_isset(session_client_socket, writeFdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "session_client_socket should NOT be set %s\n", j9error_last_error_message());
		goto exit;
	}

	/* test that a read would block as we haven't written anything yet, we should get a timeout from any call to select at this point */
	j9sock_fdset_zero(readFdset);
	j9sock_fdset_set(session_client_socket, readFdset);
	nfds = j9sock_fdset_size(session_client_socket);
	selectRC = j9sock_select(nfds, readFdset, NULL, NULL, &timeP);

	if (selectRC != J9PORT_ERROR_SOCKET_TIMEOUT) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "expected J9PORT_ERROR_SOCKET_TIMEOUT from j9sock_select, instead got: %i, %s\n", selectRC, j9error_last_error_message());
		goto exit;
	}

	if (TRUE == j9sock_fdset_isset(session_client_socket, readFdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "session_client_socket should NOT be set %s\n", j9error_last_error_message());

		/* Read message from session_client_socket */
		bytesRead = j9sock_read(session_client_socket, cursor, EsMaxPath, 0);
		if (bytesRead > 0) {
			j9tty_printf(portLibrary, "\t session_client_socket received: \"%s\"\n ",bufferB);
		}

		goto exit;
	}
	
	/* Write full message to session_server_socket */
	cursor = (U_8*)message;
	bytesLeft = lenMessagePlusTerminator;
	bytesWritten = 0;
	for (;;) {
		bytesWritten = j9sock_write(session_server_socket, cursor, bytesLeft , 0);
		if (bytesWritten < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "full message was not written to server_socket %s\n", j9error_last_error_message());
			goto exit;
		} 
		
		bytesLeft -=bytesWritten;
		cursor += bytesWritten;
		if (0 == bytesLeft) {
			break;
		}
	}
	j9sock_fdset_set(session_client_socket, readFdset);

	/* Note, Windows select() returns "0" regardless of state of sockets when timeout is set to 0 */
	j9sock_timeval_init(30, 0, &timeP);

	j9sock_fdset_zero(readFdset);
	j9sock_fdset_set(session_client_socket, readFdset);
	nfds = j9sock_fdset_size(session_client_socket);
	
	/* we've written to the server socket so the client should have something to read, this j9sock_select should return right away */
	selectRC = j9sock_select(nfds, readFdset, NULL, NULL,     &timeP/*NULL */);
	
	if (selectRC < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error in j9sock_select: %s\n", j9error_last_error_message());
		goto exit;
	} else if (1 != selectRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_select should not have set more than one socket: %s\n", j9error_last_error_message());
		goto exit;		
	}
	
	if (TRUE != j9sock_fdset_isset(session_client_socket, readFdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "session_client_socket should be set %s\n", j9error_last_error_message());
		goto exit;
	} 
	
	if (FALSE!= j9sock_fdset_isset(session_server_socket, readFdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "session_server_socket should NOT be set %s\n", j9error_last_error_message());
		goto exit;
	} 
	
	/* Read full message from session_client_socket */
	cursor = (U_8*)bufferB;
	bytesLeft = lenMessagePlusTerminator;
	bytesRead = 0;
	for (;;) {
		bytesRead = j9sock_read(session_client_socket, cursor, EsMaxPath - bytesRead, 0);
		if (bytesRead < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "full message not received on session_client_socket%s\n", j9error_last_error_message());
			goto exit;
		}
		bytesLeft -=bytesRead;
		cursor +=bytesRead;
		if (0 == bytesLeft) {
			break;
		}
	}
	
	j9tty_printf(portLibrary, "\t session_client_socket received: \"%s\"\n ",bufferB);
	
	/* Close the sockets */
	if (0 != j9sock_close(&session_server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing session_server_socket_socket: %s\n", j9error_last_error_message());
	}
	if (0 != j9sock_close(&session_client_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing session_client_socket: %s\n", j9error_last_error_message());
	}
	if (0 != j9sock_close(&server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing server_socket: %s\n", j9error_last_error_message());
	}	
	return reportTestExit(portLibrary, testName);

	exit:
	/* cleanup on failure, there should be no harm in calling close on a socket that's already closed */
	j9sock_close(&session_server_socket);
	j9sock_close(&session_client_socket);
	j9sock_close(&server_socket);
	
	return reportTestExit(portLibrary, testName);

}

/**
 * More advanced tests of fdset support with only one socket;
 * 
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sock_test4_fdset_advanced(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test4_fdset_advanced";
#if defined WIN32
#define NUM_SOCKS FD_SETSIZE /* FD_SETSIZE is defined in Winsock2.h and is something like 64*/
#else	
#define NUM_SOCKS 1 /* FD_SETSIZE is much bigger on linux, so much so that it can't handle creating so many file descriptor's for the sockets */
#endif
	j9socket_struct sockStructArray[NUM_SOCKS];
	j9socket_t sockArray[NUM_SOCKS];
	IDATA i;
	j9fdset_struct fdsetStruct; 
	j9fdset_t fdset = &fdsetStruct;
	
	reportTestEntry(portLibrary, testName);
	
#if defined(SOCKTEST_DBG)
	j9tty_printf(portLibrary, "\n FD_SETSIZE = %u\n", FD_SETSIZE);
#endif
	
	j9sock_fdset_zero(fdset);

	for (i=0; i< NUM_SOCKS; i++) {
		sockArray[i] =  &sockStructArray[i];
	}

	for (i=0; i< NUM_SOCKS; i++) {
		if (0 != j9sock_socket(&sockArray[i], J9SOCK_AFINET, J9SOCK_STREAM, J9SOCK_DEFPROTOCOL)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating sock[%i]: %s\n", i, j9error_last_error_message());
			goto exit;
		}	
	}

	for (i=0; i< NUM_SOCKS; i++) {
		if (TRUE == j9sock_fdset_isset(sockArray[i], fdset)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should not include sock[%i]: %s\n", i,  j9error_last_error_message());
			goto exit;
		}
	}
	
	for (i=0; i< NUM_SOCKS; i++) {
		j9sock_fdset_set(sockArray[i], fdset);
	}
	
	for (i=0; i< NUM_SOCKS; i++) {
		if (FALSE == j9sock_fdset_isset(sockArray[i], fdset)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should include sock[%i]: %s\n", i, j9error_last_error_message());
			goto exit;
		}
	}

	for (i=0; i< NUM_SOCKS; i++) {
		j9sock_fdset_clr(sockArray[i], fdset);
	}	
		
	for (i=0; i< NUM_SOCKS; i++) {
		if (TRUE == j9sock_fdset_isset(sockArray[i], fdset)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should not include sock[%i]: %s\n", i,  j9error_last_error_message());
			goto exit;
		}
	}
	
	for (i=0; i< NUM_SOCKS; i++) {
		j9sock_fdset_set(sockArray[i], fdset);
	}
	
	j9sock_fdset_zero(fdset);
	
	for (i=0; i< NUM_SOCKS; i++) {
		if (TRUE == j9sock_fdset_isset(sockArray[i], fdset)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should not include sock[%i]: %s\n", i,  j9error_last_error_message());
			goto exit;
		}
	}
	
	/* we should be able to set the same socket twice */
	j9sock_fdset_set(sockArray[0], fdset);
	j9sock_fdset_set(sockArray[0], fdset);
	if (FALSE == j9sock_fdset_isset(sockArray[0], fdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should include sock[0]: %s\n", j9error_last_error_message());
		goto exit;
	}

	j9sock_fdset_zero(fdset);
	if (TRUE == j9sock_fdset_isset(sockArray[0], fdset)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error: fdset should not include sock[0]: %s\n", j9error_last_error_message());
		goto exit;
	}


	exit:
	return reportTestExit(portLibrary, testName);
#undef NUM_SOCKS

}

/**
 * Basic testing of setting socket options.
 * 
 * Check that we don't fail when setting certain options, 
 * then get the value of the option and check that it's what we just set it to.
 * 
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sock_test5_basic_options(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test5_basic_options";
	char *address = "localhost";
	U_16 port = 8890;
	BOOLEAN setValue = 1;
	BOOLEAN getValue = 0;
	int i=0;
#define LEVEL 0
#define OPTION 1
#define NUM_TEST_FIVE_OPTIONS  3

#if defined(J9ZOS390) 
#define NUM_TEST_FIVE_OPTIONS_TO_TEST 1
#elif defined(WIN32)
#define NUM_TEST_FIVE_OPTIONS_TO_TEST 2
#else
#define NUM_TEST_FIVE_OPTIONS_TO_TEST NUM_TEST_FIVE_OPTIONS
#endif

	I_32 options[NUM_TEST_FIVE_OPTIONS][2] = { 	{J9_SOL_SOCKET, J9_SO_REUSEADDR},
												/* {J9_IPPROTO_IP, J9_TCP_NODELAY}, */
												{J9_IPPROTO_TCP, J9_TCP_NODELAY},
												{J9_IPPROTO_IP, J9_IP_MULTICAST_LOOP}};
	
	j9socket_struct server_socketStruct, session_server_socketStruct;
	j9socket_t server_socket = &server_socketStruct;
	j9socket_t session_server_socket = &session_server_socketStruct;
	
	j9sockaddr_struct server_socket_addrStruct, session_server_addrStruct;
	j9sockaddr_t server_addr = &server_socket_addrStruct;
	j9sockaddr_t session_server_addr = &session_server_addrStruct;
	
	reportTestEntry(portLibrary, testName);
	
	j9tty_printf(PORTLIB, "\t%u options will be tested on this platform\n", NUM_TEST_FIVE_OPTIONS_TO_TEST );

	for (i = 0; i < NUM_TEST_FIVE_OPTIONS_TO_TEST; i++) {
		/* server_socket: create, bind */
		if (0 != j9sock_socket(&server_socket, J9SOCK_AFINET, J9SOCK_STREAM, J9SOCK_DEFPROTOCOL)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating socket: %s\n", j9error_last_error_message());
			return -1;
		}
	
		if (0 != j9sock_sockaddr(server_addr, address, j9sock_htons(port))) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating address: %s\n", j9error_last_error_message());
			return -1;
		}

		/* set the option */
		if (0 != j9sock_setopt_bool(server_socket, options[i][LEVEL], options[i][OPTION], &setValue)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error setting socket option index %i: %s\n", i, j9error_last_error_message());
			return -1;
			
		}
	
		/* check that the value is what we just set it to */
		if (0 != j9sock_getopt_bool(server_socket, options[i][LEVEL], options[i][OPTION], &getValue)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error setting socket option index %i: %s\n", i, j9error_last_error_message());
			return -1;
			
		}
		
		if (setValue != getValue) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_getopt_bool() was not returned the value set by j9sock_setopt_bool(): index %i, setValue %i, getValue %i: %s\n", i, setValue, getValue, j9error_last_error_message());
			return -1;
		}
	
		if (0 != j9sock_bind(server_socket, server_addr)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error binding socket, index %i: %s, \n", i, j9error_last_error_message());
			return -1;
		}
	
		if (0 != j9sock_listen(server_socket, 1)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error listening to socket, index %i: %s\n", i, j9error_last_error_message());
			return -1;
		}	
	
		if (0 != j9sock_close(&server_socket)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing server_socket, index %i: %s\n", i, j9error_last_error_message());
		}
	}

	return reportTestExit(portLibrary, testName);
#undef NUM_TEST_FIVE_OPTIONS
#undef LEVEL
#undef OPTION
}

/**
 * Tests that j9sock_connect on a non-blocking socket will return 
 * J9PORT_ERROR_SOCKET_EINPROGRESS if it cannot immediately establish 
 * a connection and will return J9PORT_ERROR_SOCKET_ALREADYINPROGRESS
 * if j9sock_connect is called again while it is trying to connect.
 * 
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sock_test6_nonblocking_connect(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test6_nonblocking_connect";
	char *address = "localhost";
	U_16 port = 8888;

	j9socket_struct server_socketStruct, session_client_socketStruct, session_server_socketStruct;
	j9socket_t server_socket = &server_socketStruct;
	j9socket_t session_client_socket = &session_client_socketStruct;
	j9socket_t session_server_socket = &session_server_socketStruct;

	j9sockaddr_struct server_socket_addrStruct, session_client_addrStruct, session_server_addrStruct;
	j9sockaddr_t server_addr = &server_socket_addrStruct;
	j9sockaddr_t session_client_addr = &session_client_addrStruct;
	j9sockaddr_t session_server_addr = &session_server_addrStruct;

	reportTestEntry(portLibrary, testName);


	if (0 != startServer(portLibrary, testName, address, port, &server_socket, server_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error starting server: %s\n", j9error_last_error_message());
	} else {
		if (0 != connectClientToServerNonBlocking(portLibrary, testName, address, port, &session_client_socket, session_client_addr)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating/connecting client: %s\n", j9error_last_error_message());
		}
		
		if (0 != j9sock_close(&session_client_socket)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing session_client_socket: %s\n", j9error_last_error_message());
		}
	}

	if (0 != j9sock_close(&server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing server_socket: %s\n", j9error_last_error_message());
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Tests that 
 * 	For nonblocking sockets:
 * 		1. calling j9sock_read when there is nothing to read returns J9PORT_ERROR_SOCKET_WOULDBLOCK
 * 		2. calling j9sock_read when there is something to read works fine.
 * 
 * This implicitly test j9sock_set_nonblocking()
 * 
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sock_test7_nonblocking_read(struct J9PortLibrary *portLibrary) 
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test7_nonblocking_read";
	char *address = "localhost";
	char *message = "hey, what's up?";
	U_8 *cursor = NULL;
	U_16 port = 8888;
	int lenMessagePlusTerminator;
	int bytesLeft, bytesWritten, bytesRead;
	BOOLEAN value = 1;
	char bufferA[EsMaxPath], bufferB[EsMaxPath];
	
	j9socket_struct server_socketStruct, session_client_socketStruct, session_server_socketStruct;
	j9socket_t server_socket = &server_socketStruct;
	j9socket_t session_client_socket = &session_client_socketStruct;
	j9socket_t session_server_socket = &session_server_socketStruct;
	
	j9sockaddr_struct server_socket_addrStruct, session_client_addrStruct, session_server_addrStruct;
	j9sockaddr_t server_addr = &server_socket_addrStruct;
	j9sockaddr_t session_client_addr = &session_client_addrStruct;
	j9sockaddr_t session_server_addr = &session_server_addrStruct;
	
	reportTestEntry(portLibrary, testName);
		
	/* printf("\nj9sock_test1 : session_server_addr->addr: %p  \n", session_server_addr->addr);fflush(stdout); */
	
	lenMessagePlusTerminator = (int)strlen(message) +1;
	memset(bufferA, 0, sizeof(bufferA));
	memset(bufferB, 0, sizeof(bufferB));

	if (0 != startServer(portLibrary, testName, address, port, &server_socket, server_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error starting server: %s\n", j9error_last_error_message());
	}
	
	if (0 != connectClientToServer(portLibrary, testName, address, port, &session_client_socket, session_client_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating/connecting client: %s\n", j9error_last_error_message());
	}

	if (0 != j9sock_accept(server_socket, session_server_addr, &session_server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error accepting connection: %s\n", j9error_last_error_message());
		goto exit;
	}
	
	/*****************************************************
	 * 
	 * We have two sockets that can talk to eachother 
	 * 
	 *****************************************************/
		
	/* TEST: 
	 * Set the client socket to blocking, 
	 * Don't write anything to the server 
	 * When we call j9sock_read() on the client we should get J9PORT_ERROR_SOCKET_WOULDBLOCK */

	/* Set client to nonblocking */ 
	if ( 0 != j9sock_set_nonblocking(session_client_socket, TRUE) ) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_set_nonblocking: %s\n", j9error_last_error_message());
		return -1;
	}
	
	/* Try to read when nothing had been written to the server */
	cursor = (U_8*)bufferB;
	bytesRead = j9sock_read(session_client_socket, cursor, EsMaxPath, 0);
	if (J9PORT_ERROR_SOCKET_WOULDBLOCK != bytesRead) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_read on nonblocking socket (with no queued data) did not return J9PORT_ERROR_SOCKET_WOULDBLOCK. "
				"\tInstead it returned: %i. Expected: %i", bytesRead, J9PORT_ERROR_SOCKET_WOULDBLOCK);
	}
		
	/* Write full message to session_server_socket */
	cursor = (U_8*)message;
	bytesLeft = lenMessagePlusTerminator;
	bytesWritten = 0;
	for (;;) {
		bytesWritten = j9sock_write(session_server_socket, cursor, bytesLeft , 0);
		if (bytesWritten < 0) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "full message was not written to server_socket %s\n", j9error_last_error_message());
			goto exit;
		} 
		
		bytesLeft -=bytesWritten;
		cursor += bytesWritten;
		if (0 == bytesLeft) {
			break;
		}
	}
	
	/* Read full message from session_client_socket */
	cursor = (U_8*)bufferB;
	bytesLeft = lenMessagePlusTerminator;
	bytesRead = 0;
	for (;;) {
		bytesRead = j9sock_read(session_client_socket, cursor, EsMaxPath - bytesRead, 0);
		if (bytesRead < 0) {
			if (J9PORT_ERROR_SOCKET_WOULDBLOCK == bytesRead) {
				/* just because we wrote to the server doesn't mean the data was queued on the client in time for this call to j9sock_read */
				continue;
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Error reading from socket. j9sock_read returned: %i", bytesRead);
				goto exit;
			}
		}
		bytesLeft -=bytesRead;
		cursor +=bytesRead;
		if (0 == bytesLeft) {
			break;
		}

	}
	
	j9tty_printf(portLibrary, "\t session_client_socket received: \"%s\"\n ",bufferB);
	
	/* Close the sockets */
	if (0 != j9sock_close(&session_server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing session_server_socket_socket: %s\n", j9error_last_error_message());
	}
	if (0 != j9sock_close(&session_client_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing session_client_socket: %s\n", j9error_last_error_message());
	}
	if (0 != j9sock_close(&server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing server_socket: %s\n", j9error_last_error_message());
	}
	exit:

	return reportTestExit(portLibrary, testName);
}


/**
 * Tests that j9sock_select() and j9sock_select_read() on a closed socket will 
 * return that the socket is ready and that a subsequent j9sock_read() will fail
 *
 * @ref j9sock.c
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9sock_test8_select_read_closed_socket(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test8_select_read_closed_socket";
	char *address = "localhost";
	U_16 port = 8888;
	j9fdset_struct readFdsetStruct;
	j9fdset_t readFdset = &readFdsetStruct;
	I_32 nfds, selectRC, readRC;
	U_8 buffer = 0;
	struct j9timeval_struct timeP;

	j9socket_struct server_socketStruct, session_client_socketStruct, session_server_socketStruct;
	j9socket_t server_socket = &server_socketStruct;
	j9socket_t session_client_socket = &session_client_socketStruct;
	j9socket_t session_server_socket = &session_server_socketStruct;

	j9sockaddr_struct server_socket_addrStruct, session_client_addrStruct, session_server_addrStruct;
	j9sockaddr_t server_addr = &server_socket_addrStruct;
	j9sockaddr_t session_client_addr = &session_client_addrStruct;
	j9sockaddr_t session_server_addr = &session_server_addrStruct;

	reportTestEntry(portLibrary, testName);

	if (0 != startServer(portLibrary, testName, address, port, &server_socket, server_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error starting server: %s\n", j9error_last_error_message());
	}

	if (0 != connectClientToServer(portLibrary, testName, address, port, &session_client_socket, session_client_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating/connecting client: %s\n", j9error_last_error_message());
	}

	if (0 != j9sock_accept(server_socket, session_server_addr, &session_server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error accepting connection: %s\n", j9error_last_error_message());
		goto exit;
	}


	/* Set client to nonblocking */
	if ( 0 != j9sock_set_nonblocking(session_client_socket, TRUE) ) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_set_nonblocking: %s\n", j9error_last_error_message());
		return -1;
	}


	/* do a j9sock_select_read with a timeout before any data is written to
	 * the client to ensure that j9sock_select_read times out
	 */
	selectRC = j9sock_select_read(session_client_socket, 0, (I_32) 10000, TRUE);

	j9tty_printf(portLibrary, "\t j9sock_select_read(session_client_socket) returned %d \n ", selectRC);

	if (J9PORT_ERROR_SOCKET_TIMEOUT != selectRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_select_read should have returned J9PORT_ERROR_SOCKET_TIMEOUT\n", j9error_last_error_message());
	}


	/* Close session_server_socket */
	if (0 != j9sock_close(&session_server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing session_server_socket_socket: %s\n", j9error_last_error_message());
	}

	/* create an fdset and add the session_client to it */
	j9sock_fdset_zero(readFdset);
	j9sock_fdset_set(session_client_socket, readFdset);
	nfds = j9sock_fdset_size(session_client_socket);
	j9sock_timeval_init(30, 0, &timeP);

	/* The session_server_socket was closed above, test that j9sock_select() and j9sock_select_read()
	 * return 1 indicating that operations on the client socket will not block
	 */
	selectRC = j9sock_select(nfds, readFdset, NULL, NULL,     &timeP/*NULL */);
	j9tty_printf(portLibrary, "\t j9sock_select(session_client_socket) returned %d \n ", selectRC);
	if (selectRC < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error in j9sock_select: %s\n", j9error_last_error_message());
	} else if (1 != selectRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_select should not have set more than one socket: %s\n", j9error_last_error_message());
	}

	selectRC = j9sock_select_read(session_client_socket, 0, (I_32) 100000, TRUE);
	j9tty_printf(portLibrary, "\t j9sock_select_read(session_client_socket) returned %d \n ", selectRC);
	if (selectRC < 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error in j9sock_select_read: %s\n", j9error_last_error_message());
	} else if (1 != selectRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_select_read should not have set more than one socket: %s\n", j9error_last_error_message());
	}

	/* check that j9sock_read() returns 0 */
	readRC = j9sock_read(session_client_socket, &buffer, 1, 0);
	if (0 != readRC) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9sock_read should have returned 0 but returned %d instead\n", readRC);
	}

	/* Close the sockets */
	if (0 != j9sock_close(&session_client_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing session_client_socket: %s\n", j9error_last_error_message());
	}
	if (0 != j9sock_close(&server_socket)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error closing server_socket: %s\n", j9error_last_error_message());
	}

exit:
	return reportTestExit(portLibrary, testName);
}

I_32
startServerForIPv6(struct J9PortLibrary *portLibrary, const char* testName, j9socket_t *server_socket, unsigned char *sa, I_32 family, U_32 scopeID, j9addrinfo_struct *res, I_32 index) {

	PORT_ACCESS_FROM_PORT(portLibrary);
	BOOLEAN value = 1;

	if (0 != j9sock_socket(server_socket, family, J9SOCK_STREAM, J9SOCK_DEFPROTOCOL)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating socket: %s\n", j9error_last_error_message());
		return -1;
	}

	if (0 != j9sock_setopt_bool(*server_socket, J9_SOL_SOCKET, J9_SO_REUSEADDR, &value)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error setting socket option: %s\n", j9error_last_error_message());
		return -1;
	}
/*
	if (0 != j9sock_getaddrinfo_address(&res, sa, index, &scopeID)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_address: %s\n", j9error_last_error_message());
		return -1;
		}

	if (0 != j9sock_bind(*server_socket, server_addr)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error binding socket: %s\n", j9error_last_error_message());
		return -1;
		}

#if defined(SOCKTEST_DBG) && defined (IPv6_FUNCTION_SUPPORT)
	printAddress(portLibrary, (struct sockaddr *) server_addr);
#endif
	if (0 != j9sock_listen(*server_socket, 1)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error binding listening to socket: %s\n", j9error_last_error_message());
		return -1;
	}
*/
	return 0;
}

I_32
j9sock_test10_get_addrinfo(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test10_get_addrinfo";
#ifdef IPv6_FUNCTION_SUPPORT
	j9socket_struct server_socketStruct;
	j9socket_t server_socket = &server_socketStruct;
	j9addrinfo_struct res;
	j9addrinfo_t hints;
	unsigned char *sa;
	struct addrinfo *addrinf;
	BOOLEAN value = 1;
	I_32 length = 0;
	I_32 i = 0;
#endif

	reportTestEntry(portLibrary, testName);

#ifdef IPv6_FUNCTION_SUPPORT

	memset(&hints, 0, sizeof(hints));
	if (0 != j9sock_getaddrinfo_create_hints(&hints, (I_16) J9ADDR_FAMILY_UNSPEC, SOCK_STREAM, J9PROTOCOL_FAMILY_UNSPEC, 0)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_create_hints: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (NULL == hints) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_create_hints, hints == NULL\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_getaddrinfo("localhost", hints, &res)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_getaddrinfo_length(&res, &length)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_length: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	for (i = 0; i < length; i++) {
		U_32 scopeID = 0;
		I_32 family, protocol, socktype = 0;

		I_32 bytesToCopy = 0;

		if (0 != j9sock_getaddrinfo_family(&res, &family, i)){
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_family: %s\n", j9error_last_error_message());
			return reportTestExit(portLibrary, testName);
		}
		bytesToCopy = (J9ADDR_FAMILY_AFINET4 == family) ? J9SOCK_INADDR_LEN	: J9SOCK_INADDR6_LEN;
		addrinf = res.addr_info;

#if defined(SOCKTEST_DBG)
		j9tty_printf(portLibrary, "# of results: %d\n", length);
		j9tty_printf(portLibrary, "family: %d\n", family);
		if (family == J9ADDR_FAMILY_AFINET4) {
			j9tty_printf(portLibrary, "socktype: %d\n", &addrinf.ai_socktype);
			j9tty_printf(portLibrary, "protocol: %d\n", &addrinf.ai_protocol);
		}
#endif
/*		if (0 != startServerForIPv6(portLibrary, testName, &server_socket, sa, family, scopeID, &res, i)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "error starting server: %s\n", j9error_last_error_message());
		}
*/
	}

	if (0 != j9sock_freeaddrinfo(&res)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_freeaddrinfo: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}
#endif
	return reportTestExit(portLibrary, testName);
}

I_32
j9sock_test11_getnameinfo(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9sock_test11_getnameinfo";
#ifdef IPv6_FUNCTION_SUPPORT
	j9socket_struct server_socketStruct;
	j9socket_t server_socket = &server_socketStruct;
	j9sockaddr_struct local_addrStruct;
	j9sockaddr_t local_addr = &local_addrStruct;
	char hostname[1025];
	char hostname2[1025];
	char hostname3[1025];
	j9addrinfo_struct res;
	j9addrinfo_t hints;
	I_32 family;
	I_32 i = 0;
	U_8 nipAddr[16];
	I_32 numOfAddrinfo = 0;
	U_32 scope_id = 0;
	I_32 index = 0;
	OSSOCKADDR *sockaddr;
	I_32 nameLength = 0;
#define CANONNAMEFLAG 0x02
#endif

	reportTestEntry(portLibrary, testName);
#ifdef IPv6_FUNCTION_SUPPORT
	memset(&hints, 0, sizeof(hints));

	if (0 != j9sock_getaddrinfo_create_hints(&hints, (I_16) J9ADDR_FAMILY_UNSPEC, SOCK_STREAM, J9PROTOCOL_FAMILY_UNSPEC, CANONNAMEFLAG)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_create_hints: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (NULL == hints) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_create_hints, hints == NULL\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_getaddrinfo("127.0.0.1", hints, &res)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_getaddrinfo_length(&res, &numOfAddrinfo)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling getaddrinfo_length: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	for (i = 0; i < numOfAddrinfo; i++) {
	    if (0!= j9sock_getaddrinfo_family(&res, &family, i)){
	    	outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_family: %s\n", j9error_last_error_message());
	    	return reportTestExit(portLibrary, testName);
	    }
	    if ((family == J9ADDR_FAMILY_AFINET4)){
	    	if (0 != j9sock_getaddrinfo_address(&res, (U_8 *) nipAddr, i, &scope_id)){
	    		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_address: %s\n", j9error_last_error_message());
	    		return reportTestExit(portLibrary, testName);
	    	}
	    	if (0 != j9sock_getaddrinfo_name(&res, hostname3, 0)){
	    		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getaddrinfo_name: %s\n", j9error_last_error_message());
	    		return reportTestExit(portLibrary, testName);
	    	}
	    	break;
	    }
	}

	if (0 != j9sock_socket(&server_socket, J9SOCK_AFINET, J9SOCK_STREAM, J9SOCK_DEFPROTOCOL)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error creating socket: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_sockaddr_init6(local_addr, nipAddr, 4, family, 0, 0, 0, server_socket)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_sockaddr_init6: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if (0 != j9sock_freeaddrinfo(&res)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_freeaddrinfo: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	if((0 != j9sock_getnameinfo(local_addr, sizeof(struct sockaddr), hostname, sizeof(hostname), 0))){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_getnameinfo: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}
	if (0 != j9sock_gethostname(hostname2, sizeof(hostname2))){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "error calling j9sock_gethostname: %s\n", j9error_last_error_message());
		return reportTestExit(portLibrary, testName);
	}

	/* compare the output of getnameinfo with that of j9sock_gethostname expecting same results,
	 * depending on the platform and configuration, can return a number of different results */

	/* use the shortest name for a generous compare */
	nameLength = strlen(hostname);
	if (strlen(hostname2) < nameLength){
		nameLength = strlen(hostname2);
	}
	if ( (0 == strncmp(hostname, hostname2, nameLength))
			|| (0 == strncmp(hostname, "localhost", strlen("localhost"))) || (0 == strncmp(hostname, "loopback", strlen("loopback")))
			|| (0 == strncmp(hostname, "127.0.0.1", strlen("127.0.0.1")))){
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "test fails: getnameinfo returns %s, gethostname returns %s\n", hostname, hostname2);
		return reportTestExit(portLibrary, testName);
	}

#endif
	return reportTestExit(portLibrary, testName);
}


/**
 * Verify port file system.
 * 
 * Exercise all API related to port library sockets found in
 * @ref j9sock.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9sock_runTests(struct J9PortLibrary *portLibrary)
{
		
	PORT_ACCESS_FROM_PORT(portLibrary);
	
	int rc = TEST_FAIL;
	
	HEADING(portLibrary, "Sockets");
	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9sock_verify_function_slots(portLibrary);

	if (TEST_PASS == rc) {
		rc |= j9sock_test1_basic(portLibrary);
		rc |= j9sock_test2_fdset_basic(portLibrary);
		rc |= j9sock_test3_fdset_basic_select(portLibrary);
		rc |= j9sock_test4_fdset_advanced(portLibrary);
		rc |= j9sock_test5_basic_options(portLibrary);
		rc |= j9sock_test6_nonblocking_connect(portLibrary);
		rc |= j9sock_test7_nonblocking_read(portLibrary);
		/* j9sock_test8_select_read_closed_socket doesn't work on Windows and applies only to Harmony port library. */
		rc |= j9sock_test10_get_addrinfo(portLibrary);
		rc |= j9sock_test11_getnameinfo(portLibrary);
	}

	/* Output results */
	j9tty_printf(PORTLIB, "\nsocket tests done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;

}
