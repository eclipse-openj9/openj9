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
 * $RCSfile: socket.c,v $
 * $Revision: 1.29 $
 * $Date: 2008-09-18 17:28:51 $
 */

#include "testHelpers.h"
#include "portsock.h"

#if 0

static const char CHALLENGE[]="Object Technology International";
static const char RESPONSE[]="OTI";
static U_8	buf[1024];
/*U_8>>char?*//*when [>50000], client can't pause*/
	/*=32708 OK. must <32709 though ELSE -11!!!!!!!!!(client)*/

static char LOCAL[]="localhost";
/*no const?*/
extern char serverName[99];
/*
 * I_32  (*sock_gethostbyaddr) ( struct J9PortLibrary *portLibrary, char *ad

 * I_16  (*sock_htons) ( struct J9PortLibrary *portLibrary, I_16 val ) ;
 * I_32  (*sock_htonl) ( struct J9PortLibrary *portLibrary, I_32 val ) ;
 * I_32  (*sock_shutdown_input) ( struct J9PortLibrary *portLibrary, j9socke
 * I_32  (*sock_shutdown_output) ( struct J9PortLibrary *portLibrary, j9sock
 * I_16  (*sock_ntohs) ( struct J9PortLibrary *portLibrary, I_16 val ) ;
 * I_32  (*sock_getsockname) ( struct J9PortLibrary *portLibrary, j9socket_t
 * I_32  (*sock_readfrom) ( struct J9PortLibrary *portLibrary, j9socket_t so
 * I_32  (*sock_writeto) ( struct J9PortLibrary *portLibrary, j9socket_t soc
 * I_32  (*sock_inetaddr) ( struct J9PortLibrary *portLibrary, char *addrStr

 * I_32  (*sock_sockaddr_create) ( struct J9PortLibrary *portLibrary, j9sock
 * I_16  (*sock_sockaddr_port) ( struct J9PortLibrary *portLibrary, j9sockad
 * I_32  (*sock_sockaddr_address) ( struct J9PortLibrary *portLibrary, j9soc
 
 * I_32  (*sock_linger_create) ( struct J9PortLibrary *portLibrary, j9linger
 * I_32  (*sock_linger_free) ( struct J9PortLibrary *portLibrary, j9linger_t
 * I_32  (*sock_linger_enabled) ( struct J9PortLibrary *portLibrary, j9linge
 * I_32  (*sock_linger_linger) ( struct J9PortLibrary *portLibrary, j9linger
 
 * I_32  (*sock_getopt_int) ( struct J9PortLibrary *portLibrary, j9socket_t
 * I_32  (*sock_setopt_int) ( struct J9PortLibrary *portLibrary, j9socket_t
 * I_32  (*sock_getopt_bool) ( struct J9PortLibrary *portLibrary, j9socket_t
 * I_32  (*sock_setopt_bool) ( struct J9PortLibrary *portLibrary, j9socket_t
 * I_32  (*sock_getopt_byte) ( struct J9PortLibrary *portLibrary, j9socket_t
 * I_32  (*sock_setopt_byte) ( struct J9PortLibrary *portLibrary, j9socket_t
 * I_32  (*sock_getopt_linger) ( struct J9PortLibrary *portLibrary, j9socket
 * I_32  (*sock_setopt_linger) ( struct J9PortLibrary *portLibrary, j9socket
 * I_32  (*sock_getopt_sockaddr) ( struct J9PortLibrary *portLibrary, j9sock
 * I_32  (*sock_setopt_sockaddr) ( struct J9PortLibrary *portLibrary, j9sock
 * I_32  (*sock_setopt_ipmreq) ( struct J9PortLibrary *portLibrary, j9socket

 * I_32  (*sock_ipmreq_create) ( struct J9PortLibrary *portLibrary, j9ipmreq
 * I_32  (*sock_ipmreq_free) ( struct J9PortLibrary *portLibrary, j9ipmreq_t

 * I_32  (*sock_setflag_read) ( struct J9PortLibrary *portLibrary, I_32 flag
 */

/*
 * Test and report on DNs characteristics of host {byname? byaddr??}
 */
static I_32 reportDNS(struct J9PortLibrary *portLibrary,char *host)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_32 rc,addr;
	char* hostname;					/*now.. are there mem leak concerns??*/
	char** aliasList;
	char* dottedIP;
	U_32 i=0,num;
	j9hostent_struct handle;

	HEADING(PORTLIB,"DNS report:");

	if(strlen(host)>0){
		/*gethost, by NAME*/
		rc=j9sock_gethostbyname(host,&handle);	/*what if buf 2short? trunc?*/
		if(rc!=0){
			j9tty_printf(PORTLIB," Error getting host [%s] by name: %d\n",host,rc);
			goto ERROR;
		}
		j9tty_printf(PORTLIB," Got host [%s] by name.\n",host);
	} else {
		/*gethost, by addr??*/
	}

	rc=j9sock_hostent_hostname(&handle, &hostname);	/*ret 0, in win, anyway*/
	if(rc!=0)
		strcpy(hostname,"(error getting hostname)");

	rc=j9sock_hostent_aliaslist(&handle,&aliasList);	/*ret 0, in win*/
	if(rc!=0)
		strcpy(hostname,"(error getting aliasList)");


	j9tty_printf(PORTLIB," [%s]\n\n",hostname);

	while(aliasList[i] !=NULL){
		addr=j9sock_hostent_addrlist(&handle,i);
		rc=j9sock_inetntoa(&dottedIP,(U_32)addr, NULL);
		if(rc!=0){
			j9tty_printf(PORTLIB,"inetntoa failed: %d\n",rc);
			goto ERROR;
		}

		num=j9sock_ntohl(*(U_32*)(aliasList)[i]);	/*get bytes in host order*/
		j9tty_printf(PORTLIB," 0x%X [%s]\n",num,dottedIP);	/*%d i*/
		++i;
	}
	if(i==0) j9tty_printf(PORTLIB,"No alias\n");

	/*********/
	/*cleanup*/
	/*********/
	HEADING(PORTLIB,"END of DNS report");
	return 0;

ERROR:
	/*********/
	/*cleanup*/
	/*********/
	j9tty_printf(PORTLIB,"reportDNS failed\n");
	return -1;
}
/*******************************
 * startup routine for the tests
 ******************************/
static I_32
startupIP(struct J9PortLibrary *portLibrary,j9socket_ *socket, j9socket_t *netPipe, j9sockaddr_t addr, char *host, I_32 sockType, U_16 port)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_32 rc;
	
	/* create/open socket	[get socket number]
	 * note:  J9SOCK_AFINET is only addr family supported]
	 * sockTypes:  	J9SOCK_STREAM (stream socket), or J9SOCK_DGRAM (datagram)
	 * protocol:	creation param [only J9SOCK_DEFPROTOCOL supported]
	 */
	rc=j9sock_socket(socket, J9SOCK_AFINET, sockType, J9SOCK_DEFPROTOCOL);
	if(rc!=0){
		j9tty_printf(PORTLIB," Error creating new socket descriptor: %d\n",rc);
		/*maybe more desc later*/
		return rc;
	}
	/*j9tty_printf(PORTLIB," Created new socket descriptor\n");*/

	/* create new j9sockaddr	[with host:port]
	 * note:  AF_INET only address family supported now
	 */ 
	rc=j9sock_sockaddr(addr,host,port);	
	if(rc!=0){
		j9tty_printf(PORTLIB," Error creating new sockaddr struct: %d\n",rc);
		/*maybe more desc later*/
		return rc;
	}
	/*j9tty_printf(PORTLIB," Created new sockaddr struct\n");*/

	if(netPipe!=NULL){	/*not when client, or udp, etc..*/
		/* [associate Inet addr+port# with socket]
		 * bind socket to sockaddr struct {Inet addr+port#}.
		 */
		rc=j9sock_bind(*socket, *addr);		/*rc gets J9PORT_ERROR_SOCKET_xxxx*/
		if(rc!=0){
			j9tty_printf(PORTLIB," Error binding unconnected socket: %d\n",rc);
			/*maybe more desc later*/
			return rc;
		}
	j9tty_printf(PORTLIB," Bound unconnected socket, done startup\n");
	}
	return 0;
}
/********************************
 * shutdown routine for the tests
 ********************************/
static I_32 
shutdownIP(struct J9PortLibrary *portLibrary,j9socket_t *socket, j9socket_t *netPipe, j9sockaddr_t *addr)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_32 rc;
	
	/* close network socket */
	if(netPipe != NULL && *netPipe==NULL)
		j9tty_printf(PORTLIB," network socket *netPipe==NULL!\n");
	if(netPipe !=NULL && *netPipe!=NULL && j9sock_isSocketValid( *netPipe ) ){
		rc=j9sock_close(netPipe);
		if(rc!=0){
			j9tty_printf(PORTLIB," Error closing network socket: %d\n",rc);
			return rc;
		}
		/*j9tty_printf(PORTLIB," Closed network socket\n");*/
	}
	/*close socket new j9sockaddr */ 
	if( j9sock_socketIsValid( *socket ) ) {
		rc=j9sock_close(socket);
	
		if(rc!=0){
			j9tty_printf(PORTLIB," Error closing socket: %d\n",rc);
			return rc;
		}
	}
	/*j9tty_printf(PORTLIB," Closed socket\n");*/

	j9tty_printf(PORTLIB," Freed sockaddr struct, done shutdown\n");
	return 0;
}
/********************************
 * waitForRead
 ********************************/
static I_32
waitForRead(struct J9PortLibrary *portLibrary, j9socket_t socket, U_32 wait_seconds)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_32 rc;

	rc=j9sock_select_read(socket, wait_seconds,0, FALSE);
	if(rc!=0){
		j9tty_printf(PORTLIB,"   Error on select: %d\n",rc);
		/*maybe more desc later*/
		/*return rc;*/				/*what does +1 mean??!?!?!*/
	}
	return 0;
}
/********************************
 * waitForWrite, similar to REad
 ********************************/
static I_32
waitForWrite(struct J9PortLibrary *portLibrary, j9socket_t socket, U_32 wait_seconds)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	I_32 rc;

	/* TODO: create a select_write call
	rc=j9sock_select_write(socket, wait_seconds,0, FALSE);
	*/
	rc = 0;
	if(rc!=0){
		j9tty_printf(PORTLIB,"   Error on select: %d\n",rc);
		/*maybe more desc later*/
		/*return rc;*/				/*what does +1 mean??!?!?!*/
	}
	return 0;
}
/******************************
 * server half of the TCP test
 ******************************/
static I_32
testTCPserver(struct J9PortLibrary *portLibrary,char *host, U_16 port)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	j9socket_t socket, netPipe;
	j9sockaddr_struct addr;
	I_32 rc;

	HEADING(PORTLIB,"TCP server");

	rc=startupIP(PORTLIB,&socket, &netPipe, &addr, host, J9SOCK_STREAM, port);
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error starting up server: %d\n",rc);
		goto shutdown;		/*return rc;*/	/*should still shutdown, you know*/
	}

	j9tty_printf(PORTLIB,"  Server waiting on port %d...\n",port);

	/*listen before accept*/
	rc=j9sock_listen(socket, 3);	/*accept backlog of 3, others refused*/
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error on j9sock_listen: %d\n",rc);
		return rc;
	}
	/*j9tty_printf(PORTLIB,"  done j9sock_listen\n");*/

	/*wait for client*/
	netPipe=NULL;
	rc=j9sock_accept(socket, &addr, &netPipe);
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error on j9sock_accept: %d\n",rc);
		return rc;
	}
	/*j9tty_printf(PORTLIB,"  done j9sock_accept\n");*/

	if(netPipe==NULL){
		j9tty_printf(PORTLIB,"  No client connection actually received... :(\n");
		goto shutdown;
	}

	/* wait for CHALLENGE */
	rc=waitForRead(PORTLIB,netPipe, 10);
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error on waitForRead: %d\n",rc);
		return rc;
	}
	/* read: CHALLENGE +ve=#bytes read, 0=conn. gracefully closed, -ve=ERR*/
	rc=j9sock_read(netPipe, buf, sizeof(buf), 0);	/*Flag=0?!*/
	if(rc<0){
		j9tty_printf(PORTLIB,"  Error on read: %d\n",rc);
		return rc;
	}
	j9tty_printf(PORTLIB,"  read %d bytes [0=conn. closed]\n",rc);
	
	if(strcmp(buf,CHALLENGE)==0){
		j9tty_printf(PORTLIB," -challenge matched!\n");
	} else {
		j9tty_printf(PORTLIB,"  Server got: %s\n",buf);
		j9tty_printf(PORTLIB,"  [Expected : %s]",CHALLENGE);
		j9tty_printf(PORTLIB," -challenge not matched!\n");
	}

	/*send back RESPONSE*/
	j9tty_printf(PORTLIB,"  sending back response: %s\n",RESPONSE);
	rc=waitForWrite(PORTLIB,netPipe, 10);
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error on waitForWrite: %d\n",rc);
		return rc;
	}
	
	strcpy(buf,RESPONSE);			/*need to?*/
	
	/* write: RESPONSE +ve=#bytes written,  -ve=ERR*/
	rc=j9sock_write(netPipe, buf, sizeof(buf), J9SOCK_NOFLAGS);	/*Flag=0?!*/
	if(rc<0){
		j9tty_printf(PORTLIB,"  Error on write: %d\n",rc);
		return rc;
	}
	j9tty_printf(PORTLIB,"  wrote %d bytes [0=conn. closed]\n",rc);	rc=0;/*!error!*/


shutdown:
	rc=shutdownIP(PORTLIB,&socket, &netPipe, &(&addr));
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error shuting down server: %d\n",rc);
		return rc;
	}
	return 0;
}
/******************************
 * client half of the TCP test
 *****************************/
static I_32
testTCPclient(struct J9PortLibrary *portLibrary,char *host, U_16 port)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	j9socket_t socket;
	j9sockaddr_struct addr;
	I_32 rc;

	HEADING(PORTLIB,"TCP client");
	
	rc=startupIP(PORTLIB,&socket, NULL, &addr, host, J9SOCK_STREAM, port);
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error starting up client: %d\n",rc);
		goto shutdown;	/*return rc;*/	/*should still shutdown, you know*/
	}

	rc=j9sock_connect(socket, addr);
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error connecting to %s:%d (%d)\n",host,port,rc);
		goto shutdown;	/*return rc;*/	/*should still shutdown, you know*/
	}
	j9tty_printf(PORTLIB,"  connected to %s on port %d...\n",host,port);

	/*send CHALLENGE*/
	j9tty_printf(PORTLIB,"  sending challenge: %s\n","",CHALLENGE);
	rc=waitForWrite(PORTLIB,socket, 10);
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error on waitForWrite: %d\n",rc);
		return rc;
	}
	
	strcpy(buf,CHALLENGE);			/*need to?*/
	
	/* write: CHALLENGE +ve=#bytes written,  -ve=ERR*/
	rc=j9sock_write(socket, buf, sizeof(buf), J9SOCK_NOFLAGS);	/* NEED size buf + 1???*/
	if(rc<0){
		j9tty_printf(PORTLIB,"  Error on write: %d\n",rc);
		return rc;
	}
	j9tty_printf(PORTLIB,"  wrote %d bytes [0=conn. closed?]\n",rc);



	/* wait for RESPONSE */
	rc=waitForRead(PORTLIB,socket, 10);
	if(rc!=0){
		j9tty_printf(PORTLIB,"  Error on waitForRead: %d\n",rc);
		goto shutdown; /*return rc;*/
	}
	/* read: RESPONSE +ve=#bytes read, 0=conn. gracefully closed, -ve=ERR*/
	rc=j9sock_read(socket, buf, sizeof(buf), 0);	/*Flag=0?!*/
	if(rc<0){
		j9tty_printf(PORTLIB,"  Error on read: %d\n",rc);
		goto shutdown; /*return rc;*/
	}
	j9tty_printf(PORTLIB,"  read %d bytes [0=conn. closed]\n",rc);	rc=0;/*!error!*/
	j9tty_printf(PORTLIB,"  We recv'ed: %s\n",buf);
	
	if(strcmp(buf,RESPONSE)==0){
		j9tty_printf(PORTLIB," -matched!\n");
	} else {
		j9tty_printf(PORTLIB,"  [Expected : %s]",RESPONSE);
		j9tty_printf(PORTLIB," -no match!\n");
	}


shutdown:
	if(shutdownIP(PORTLIB,&socket, NULL, &(&addr)) || rc){	/*if shutdown fail, YOU KNOW*/
		j9tty_printf(PORTLIB,"  Error shuting down client: rc=%d\n",rc);
		return rc;
	}
	return 0;
}

#endif

/* NOT DONE - need to test UDP */


/***********************************************************
 * sock_test, the entry point to the socket testing module
 ***********************************************************/
int 
socket_test(struct J9PortLibrary *portLibrary,BOOLEAN isClient,char* serverName) 
{

	PORT_ACCESS_FROM_PORT(portLibrary);
#if 0
	I_32 rc;
	char buf[64];
	U_16 i,portMin=45000, portMax=45003;	/*[50000,60000)*/
#endif
		/*fancy heading*/
	HEADING(PORTLIB,"Socket test");

#if 0

	j9tty_printf(PORTLIB,"IN SOCKET TEST %d %d %d check..\n",1,2,3);

	/*initialize socket library*/
	rc=j9sock_startup();
	if(rc!=0){
		j9tty_printf(PORTLIB,"Error initializing sockets: %d\n",rc);
		return rc;
	}
	j9tty_printf(PORTLIB,"Sockets initialized!\n");



	/*gethostname*/
	rc=j9sock_gethostname(buf,(I_32)sizeof(buf));/*whatif buf too short?trunc?*/
	if(rc!=0){
		j9tty_printf(PORTLIB,"Error on gethostname of local machine: %d\n",rc);
		return rc;
	}
	j9tty_printf(PORTLIB,"(local) Hostname=[%s]\n",buf);



if(strlen(serverName)==0){
	j9tty_printf(PORTLIB,"defaulting to localhost\n");
	strcpy(serverName,LOCAL);	/*defaults to using localhost*/
}

#if 0

	if(isClient){
		if(rc= testUDPclient(PORTLIB,serverName,5140)){	/*4499*/
			j9tty_printf(PORTLIB,"Failed to test UDP! (%d)\n",rc);
			return rc;
		}
		goto shutdown; 	/*return 0;*/
	}else{
		if(rc= testUDPserver(PORTLIB,LOCAL,5140)){
			j9tty_printf(PORTLIB,"Failed to test UDP! (%d)\n",rc);
			return rc;
		}
		goto shutdown; 	/*return 0;*/
	}

#else
/*********************************************************************/
	if(isClient){
		for(i=portMin;i<portMax;i++)	/*4500,[5000,8000)*/
		if(rc= testTCPclient(PORTLIB,serverName,i)){	/*serverName*/
			j9tty_printf(PORTLIB,"Failed to test TCP! (%d)\n",rc);
			return rc;
		}
		goto shutdown; 	/*return 0;*/
	}else{
		for(i=portMin;i<portMax;i++)
			if(rc= testTCPserver(PORTLIB,LOCAL,i)){	/*serverName*/
				j9tty_printf(PORTLIB,"Failed to test TCP! (%d)\n",rc);
				return rc;
			}
		goto shutdown; 	/*return 0;*/
	}
/*********************************************************************/
#endif





shutdown:
	/*terminates use of socket library [MAKE SURE NO SOCKETS ARE IN USE!!]*/
	
	rc=j9sock_shutdown();				/*always return 0*/
	if(rc!=0){
		j9tty_printf(PORTLIB,"Error freeing socket resources: %d\n",rc);
		return rc;
	}
	j9tty_printf(PORTLIB,"Sockets all shut down\n");

#endif
	
	j9tty_printf(PORTLIB,"SOCKET test done!\n\n");
	return 0;
}
 

