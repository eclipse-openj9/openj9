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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "j9port.h"
#include "portsock.h"

#include "j9exelibnls.h"
#include "exelib_internal.h"

/* Remote console socket, and old tty_shutdown function. */
static J9PortLibrary *_portLibrary;
static j9socket_t remoteConsoleSocket = NULL;
static IDATA remoteConsoleFile = -1;
static void (*old_tty_shutdown)(struct OMRPortLibrary *portLib);
static IDATA (*old_file_write)(struct OMRPortLibrary *portLib, IDATA fd, const void *buf, IDATA nbytes);

static IDATA remoteConsole_tosock_file_write (OMRPortLibrary *portLib, IDATA fd, const void *buf, IDATA nbytes);
static IDATA remoteConsole_tofile_startup (J9PortLibrary *portLib, const char *path);
static BOOLEAN remoteConsole_tosock_startup (J9PortLibrary *portLibrary, char *hName, char *port, j9linger_t lgrval);
static IDATA remoteConsole_tofile_file_write (OMRPortLibrary *portLib, IDATA fd, const void *buf, IDATA nbytes);
static void remoteConsole_tosock_shutdown (OMRPortLibrary *portLib);
static void remoteConsole_tofile_shutdown (OMRPortLibrary *portLib);
static IDATA remoteConsole_tosock_get_chars (OMRPortLibrary *portLib, char *s, UDATA length);


/* return 0 if no error occurs */
UDATA
remoteConsole_parseCmdLine(J9PortLibrary *portLibrary, UDATA lastLegalArg, char **argv)
{
	char buffer[256];
	char *port, *hName;
	char *filepath;
	UDATA i;
	j9linger_struct lgrval;

	/* Parse command line args for -io and -tofile */
	port = NULL;
	filepath = NULL;
	for (i = 1; i <= lastLegalArg; i++) {
		if (argv[i][0] == '-') {
			if ((strncmp(&argv[i][1], "IO", 2) == 0) ||
			    (strncmp(&argv[i][1], "io", 2) == 0)) {

				/* override any previous -tofile: setting */
				filepath = NULL;

				strcpy(buffer, &argv[i][3]);
				hName = buffer;
				if (*hName == ':') {
					hName++;
				}
				port = hName;
				while ((*port != '\0') && (*port != ':')) {
					port++;
				}
				if (*port != '\0') {
					*port = '\0';
					port++;
				} else {
					port = NULL;
				}
			} else if (strncmp(&argv[i][1], "tofile:", strlen("tofile:")) == 0) {
				/* override any previous -io: setting */
				port = NULL;
				filepath = &argv[i][8];
			}
		}
	}

	if (port) {
		return (TRUE != remoteConsole_tosock_startup(portLibrary, hName, port, &lgrval));
	} else if (filepath) {
		return (TRUE != remoteConsole_tofile_startup(portLibrary, filepath));
	}

	return 0;
}



static BOOLEAN 
remoteConsole_tosock_startup(J9PortLibrary *portLibrary, char *hName, char *port, j9linger_t lgrval)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	j9sockaddr_struct raddr;
	int err;
	U_16 nPort;

	remoteConsoleSocket = NULL;

	j9sock_startup();
		
	if(j9sock_socket(&remoteConsoleSocket, J9SOCK_AFINET, J9SOCK_STREAM, 0)){
		remoteConsoleSocket = NULL;
		goto print_message_and_fail;
	}
	/* the original sockaddr() defaulted loopback if no hName provided */
	if (0 == hName[0]) {
		hName = "localhost";
	}
	nPort = j9sock_htons((U_16)atoi(port));
	if(j9sock_sockaddr(&raddr, hName, nPort  )){
		goto print_message_and_fail;
	}
	if(j9sock_connect(remoteConsoleSocket, &raddr)){
		goto print_message_and_fail;
	}
	if(0 == j9sock_linger_init(lgrval, 1, 1)){
		err = j9sock_setopt_linger(remoteConsoleSocket, J9_SOL_SOCKET, J9_SO_LINGER, lgrval);
		if(err){
			goto print_message_and_fail;
		}
	}

	/* Replace the tty_shutdown function, but keep the old one so we can chain to it. */
	old_tty_shutdown = OMRPORT_FROM_J9PORT(PORTLIB)->tty_shutdown;
	old_file_write = OMRPORT_FROM_J9PORT(PORTLIB)->file_write;
	_portLibrary = PORTLIB;
	OMRPORT_FROM_J9PORT(PORTLIB)->tty_shutdown = remoteConsole_tosock_shutdown;

	/* Switch the port library to point at our remote control in and out functions */
	OMRPORT_FROM_J9PORT(PORTLIB)->tty_get_chars = remoteConsole_tosock_get_chars;
	OMRPORT_FROM_J9PORT(PORTLIB)->file_write = remoteConsole_tosock_file_write;

	return TRUE;

print_message_and_fail:
	/* J9NLS_EXELIB_UNABLE_REDIRECT_IO=Unable to redirect console I/O to: %s:%d\n */
	j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_UNABLE_REDIRECT_IO, hName, (U_16)atoi(port));
	if (remoteConsoleSocket)  {
		j9sock_close(&remoteConsoleSocket);
	}
	remoteConsoleSocket = NULL;
	return FALSE;
}


static IDATA
remoteConsole_tosock_file_write(struct OMRPortLibrary *portLib, IDATA fd, const void *buf, IDATA nbytes)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	if ((fd == J9PORT_TTY_OUT) || (fd == J9PORT_TTY_ERR)) {
		/* J9 socket functions take an I_32 not an IDATA for buffer length, this
		 * will fail in the unlikely event that a single write to TTY exceeds the
		 * sizeof(I_32). */
		return j9sock_write(remoteConsoleSocket, (U_8 *)buf, (I_32)nbytes, J9SOCK_NOFLAGS);
	} else {
		return old_file_write(portLib, fd, buf, nbytes);
	}
}



static IDATA
remoteConsole_tosock_get_chars(struct OMRPortLibrary *portLib, char *s, UDATA length)
{
	/*
	 * J9 socket functions take I_32 for buffer sizes, however since we are replacing
	 * tty_get_chars() functionality we must conform to that prototype.
	 *
	 * This implementation will fail if length exceeds sizeof(I_32)
	 */
	PORT_ACCESS_FROM_PORT(_portLibrary);
	return j9sock_read(remoteConsoleSocket, (U_8 *)s, (I_32)length, J9SOCK_NOFLAGS);
}



static void
remoteConsole_tosock_shutdown(struct OMRPortLibrary *portLib)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	/* Close the socket. */
	j9sock_close(&remoteConsoleSocket);

	/* Chain to the old tty_shutdown. */
	old_tty_shutdown(portLib);
}



static void
remoteConsole_tofile_shutdown(struct OMRPortLibrary *portLib)
{
	PORT_ACCESS_FROM_PORT(_portLibrary);
	j9file_close(remoteConsoleFile);
	remoteConsoleFile = -1;

	/* Chain to the old tty_shutdown. */
	old_tty_shutdown(portLib);
}



static IDATA
remoteConsole_tofile_file_write(struct OMRPortLibrary *portLib, IDATA fd, const void *buf, IDATA nbytes)
{
	if ((fd == J9PORT_TTY_OUT) || (fd == J9PORT_TTY_ERR)) {
		fd = remoteConsoleFile;
	}
	return old_file_write(portLib, fd, buf, nbytes);
}



static IDATA
remoteConsole_tofile_startup(struct J9PortLibrary *portLib, const char *path)
{
	/*
	 * Assume portLibrary is already started, don't need to call
	 * portLib->file_startup().
	 */

	PORT_ACCESS_FROM_PORT(portLib);
	remoteConsoleFile =
		j9file_open(path, EsOpenCreateNew|EsOpenWrite, 0644);
	if (remoteConsoleFile == -1) {
		if (j9error_last_error_number() == J9PORT_ERROR_FILE_EXIST) {
			size_t oldPathLen;
			char *oldPath;

			oldPathLen = strlen(path) + 4 + 1;
			oldPath = j9mem_allocate_memory(oldPathLen, OMRMEM_CATEGORY_VM);
			if (oldPath) {
				j9str_printf(PORTLIB, oldPath, oldPathLen, "%s.old", path);
				j9file_unlink(oldPath);
				if (j9file_move(path, oldPath) == -1) {
					j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_EXELIB_LOGFILE_BACKUP_FAILED, path);
					/* continue even if old log file couldn't be backed up */
				}
				j9mem_free_memory(oldPath);
				oldPath = NULL;
			}

			remoteConsoleFile =
				j9file_open(path, EsOpenCreate|EsOpenTruncate|EsOpenWrite, 0644);
		}
	}

	if (remoteConsoleFile == -1) {
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_EXELIB_COULD_NOT_OPEN_FILE, path);
		return FALSE;
	}

	old_file_write = OMRPORT_FROM_J9PORT(PORTLIB)->file_write;
	old_tty_shutdown = OMRPORT_FROM_J9PORT(PORTLIB)->tty_shutdown;

	OMRPORT_FROM_J9PORT(PORTLIB)->file_write = remoteConsole_tofile_file_write;
	OMRPORT_FROM_J9PORT(PORTLIB)->tty_shutdown = remoteConsole_tofile_shutdown;
	return TRUE;
}

