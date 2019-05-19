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
 * @brief process
 */
#include <stdio.h>
#include "j9port.h"
#include "portpriv.h"

/**
 * Creates a process and returns an opaque cookie representing the created process to be passed into 
 * other j9process functions.
 * 
 * The opaque cookie is an internally-declared J9ProcessHandle structure. Storage for the J9ProcessHandle 
 * struct is created here; any resources it holds onto are destroyed by @ref j9process_close(). 
 * 
 * Inheritable handles vary on a per-platform basis and are inherited by the child process, 
 * except for stdout, stderr and stdin which are represented by the port library flags 
 * J9PORT_PROCESS_STDOUT, J9PORT_PROCESS_STDERR, J9PORT_PROCESS_STDIN respectively. 
 * These standard handles are accessible by @ref j9process_getStream
 *
 * @param[in] 	portLibrary The port library
 * @param[in] 	command An array of NULL-terminated strings representing the command line to be executed,
 * 				where command[0] is the executable name
 * @param[in] 	commandLength The number of elements in the command line where by convention the first
 * 				element is the file to be executed
 * @param[in] 	env A pointer to the environment block for the new process
 * @param[in] 	envSize The number of environment variables in the environment block
 * @param[in] 	dir The full path to the directory which contains the new process
 * @param[in] 	options Bitmap used for additional options, 0 may be used for defaults
 *		J9PORT_PROCESS_IGNORE_OUTPUT
 *				ignores the child process' output, the pipes that capture J9PORT_PROCESS_STDOUT and
 *				J9PORT_PROCESS_STDERR are not created
 *		J9PORT_PROCESS_NONBLOCKING_IO
 *				Sets the J9PORT_PROCESS_STDIN, J9PORT_PROCESS_STDOUT and J9PORT_PROCESS_STDERR pipes
 *				to non-blocking
 *	 	J9PORT_PROCESS_PIPE_TO_PARENT
 *	 			Indicates that child process I/O will be connected to the parent process over a pipe.
 *	 			This is the default handling of child process standard I/O.
 *		J9PORT_PROCESS_INHERIT_STDIN
 *				Indicates that child process J9PORT_PROCESS_STDIN will be the same as those of the parent
 *				process. This is the normal behavior of most operating system command interpreters (shells).
 *		J9PORT_PROCESS_INHERIT_STDOUT
 *				Indicates that child process J9PORT_PROCESS_STDOUT will be the same as those of the parent
 *				process. This is the normal behavior of most operating system command interpreters (shells).
 *		J9PORT_PROCESS_INHERIT_STDERR
 *				Indicates that child process J9PORT_PROCESS_STDERR will be the same as those of the parent
 *				process. This is the normal behavior of most operating system command interpreters (shells).
 *		J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT
 *				Merges the child process J9PORT_PROCESS_STDERR into J9PORT_PROCESS_STDOUT.
 *		J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP
 *				Create the child in a new process group. An intended side-effect is that the child will not
 *				be killed if CTRL + C is issued in the console of the parent process.
 * @param[in] j9fdInput The file descriptor for standard input source
 * 				obtained via the j9file port library API or
 *				J9PORT_INVALID_FD to indicate that no file descriptor is being provided.
 * @param[in] j9fdOutput The file descriptor for standard output destination
 * 				obtained via the j9file port library API or
 *				J9PORT_INVALID_FD to indicate that no file descriptor is being provided.
 * @param[in] j9fdError The file descriptor for standard error destination
 * 				obtained via the j9file port library API or
 *				J9PORT_INVALID_FD to indicate that no file descriptor is being provided.
 * @param[out] processHandle Opaque struct to be passed to other j9process operations
 * 
 * pass J9PORT_INVALID_FD(-1) for file descriptor
 *
 * When both a valid file descriptor and flag were passed, flag setting overrides file descriptor check.
 *
 * @return 0 on success, otherwise negative error code
 * 
**/
intptr_t 
j9process_create(struct J9PortLibrary *portLibrary, const char *command[], uintptr_t commandLength, char *env[], uintptr_t envSize, const char *dir, uint32_t options, intptr_t j9fdInput, intptr_t j9fdOutput, intptr_t j9fdError, J9ProcessHandle *processHandle)
{
	return J9PORT_ERROR_PROCESS_OPFAILED;
}

/**
 * Wait in the current thread until the specified process has finished executing
 * 
 * @param[in] portLibrary The port library 
 * @param[in] processHandle Opaque cookie representing the process to wait for
 *  
 * @return 0 if no errors occurred, otherwise negative error code
 *
**/  
intptr_t 
j9process_waitfor(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{
	return J9PORT_ERROR_PROCESS_OPFAILED;
}

/**
 * Terminates a process. 
 * 
 * @j9ref j9process_close must be called to free all the resources contained held onto by J9ProcessHandle
 *
 * @param[in] portLibrary The port library 
 * @param[in] processHandle Opaque cookie representing the process to terminate
 * 
 * @return 0 on success, otherwise negative error code
 *
**/ 
intptr_t 
j9process_terminate(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{
	return J9PORT_ERROR_PROCESS_OPFAILED;
}

/**
 * Write to the standard input stream of the process
 * 
 * @param[in] portLibrary The port library 
 * @param[in] processHandle Opaque cookie representing the process to be used
 * @param[in] buffer buffer containing the bytes to be written.
 * @param[in] numBytes The number of bytes to be written.
 * 
 * @return the number of characters that were written to buffer on success, otherwise negative error code
 *
**/ 

intptr_t 
j9process_write(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, void *buffer, uintptr_t numBytes) 
{
	return J9PORT_ERROR_PROCESS_OPFAILED;
}

/**
 * Read from the specified stream of the process. Blocks if there is nothing to read 
 * unless J9PORT_PROCESS_NONBLOCKING_IO was passed into @ref j9process_create(). 
 * @ref j9process_get_available() can be called to see if there is anything available 
 * to be read without blocking.
 *
 * @param[in] portLibrary The port library 
 * @param[in] processHandle Opaque cookie representing the process to be used
 * @param[in] flags flag specifying the stream to read from
 *		J9PORT_PROCESS_STDOUT
 *		J9PORT_PROCESS_STDERR
 * @param[in] buffer the buffer to read data into
 * @param[in] numBytes The number of bytes to read
 * 
 * @return the number of characters that were read from the buffer on success, otherwise negative error code
 *
**/ 
intptr_t 
j9process_read(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t flags, void *buffer, uintptr_t numBytes) 
{
	return J9PORT_ERROR_PROCESS_OPFAILED;
}

/**
 * Return the number of bytes available to be read without blocking
 * 
 * @param[in] portLibrary The port library 
 * @param[in] processHandle Opaque cookie representing the process to be used
 * @param[in] flags flag specifying the stream to read from
 *		J9PORT_PROCESS_STDOUT
 *		J9PORT_PROCESS_STDERR 
 * 
 * @return the number of bytes that are available on success, otherwise negative error code
 *
**/ 
intptr_t 
j9process_get_available(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t flags) 
{
	return J9PORT_ERROR_PROCESS_OPFAILED;
}

/**
 * By default options (value 0), j9process_close closes all handles of the process and destroy
 * the J9ProcessHandle structure as well as any resources it is holding onto.
 * This is the clean-up function for J9ProcessHandles and always needs to be called to clean
 * them up even after calls to @ref j9process_waitfor() and @ref j9process_terminate().
 * Safe to call while child is still alive.
 * 
 * Note that this does not terminate nor affect execution of the process.
 * 
 * @param[in] portLibrary The port library 
 * @param[in] processHandle Opaque cookie representing the process to use
 * @param[in] options Bitmap used for additional options, 0 may be used for defaults
 *		J9PORT_PROCESS_DO_NOT_CLOSE_STREAMS
 *			don't close stdin/sdtout/err streams,
 *			caller is responsible for close these streams separately via j9file_close()
 * 
 * @return 0 on success, otherwise negative error code
 *
**/ 
intptr_t 
j9process_close(struct J9PortLibrary *portLibrary, J9ProcessHandle *processHandle, uint32_t options)
{
	return J9PORT_ERROR_PROCESS_OPFAILED;
}

/**
 * Returns the specified stream of the process specified by processHandle.
 *
 * @param[in] portLibrary The port library 
 * @param[in] processHandle Opaque cookie representing the process to use
 * @param[in] streamFlag flag specifying the stream to return
 *		J9PORT_PROCESS_STDIN
 *		J9PORT_PROCESS_STDOUT
 *		J9PORT_PROCESS_STDERR
 * @param[out] stream The stream returned to the user
 * 
 * @return 0 on success, otherwise negative error code
 *
**/  
intptr_t 
j9process_getStream(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t streamFlag, intptr_t *stream) 
{
	return J9PORT_ERROR_PROCESS_OPFAILED;
}

/**
 * Returns whether or not the process has completed.
 *
 * @param[in] portLibrary The port library 
 * @param[in] processHandle Opaque cookie representing the process to use
 * 
 * @return 1 if the process has completed, 0 if not yet, otherwise negative error code.
 *
**/ 
intptr_t 
j9process_isComplete(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{
	return J9PORT_ERROR_PROCESS_OPFAILED;	
}

/**
 * Returns the exit code of the specified process. The behavior is undefined
 * if this function is called on a running process. @ref j9process_isComplete()
 * and @ref j9process_waitfor() must be used in order to ensure that the
 * process has completed and also in order to set the value that will be returned
 * by this function.
 * 
 * @param[in] portLibrary The port library 
 * @param[in] processHandle Opaque cookie representing the process to use
 * 
 * @return the exit code
 *
 *
**/ 
intptr_t 
j9process_get_exitCode(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{
	return J9PORT_ERROR_PROCESS_OPFAILED;	
}

/**
 * Returns the amount of non-shareable memory that a specified process has consumed.
 * This is typically private storage as well as copy-on-write pages that have been
 * dirtied.
 *
 * @param[in] portLibrary The port library
 * @param[in] pid The ID of the process to query, zero for the current process.
 *
 * @return The number of private bytes consumed, zero on error.
**/
uintptr_t
j9process_get_private_bytes(struct J9PortLibrary *portLibrary, uintptr_t pid)
{
	return 0;
}


