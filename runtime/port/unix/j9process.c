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

/**
 * @file 
 * @ingroup Port
 * @brief exec
 */

#include <errno.h>
#include <fcntl.h> 
#include <langinfo.h>
#include <nl_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#if defined(J9ZOS390)
#include <sys/stat.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h> 

/* a2e overrides nl_langinfo to return ASCII strings. We need the native EBCDIC string */
#if defined(J9ZOS390)
#include "atoe.h"
#if defined (nl_langinfo)
#undef nl_langinfo
#endif
#endif
#include "j9port.h"
#include "portpriv.h"
#include "ut_j9prt.h"
#include "omrutil.h"

#if 0
#define J9PROCESS_DEBUG
#endif

/* Some older platforms (Netwinder) don't declare CODESET */
#ifndef CODESET
#define CODESET _NL_CTYPE_CODESET_NAME
#endif

static intptr_t translateModifiedUtf8ToPlatform(struct J9PortLibrary *portLibrary, const uint8_t *inBuffer, uintptr_t inBufferSize,uint8_t **outBuffer);
static void freeTranslatedMemory(struct J9PortLibrary *portLibrary, char **newCommand, uintptr_t commandLength, char **execEnv, uintptr_t execEnvLen);

static int32_t findError (int32_t errorCode);
static int setFdCloexec(int fd);

static int setNonBlocking(int fd);

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
	case J9PORT_ERROR_PROCESS_INVALID_PARAMS:
		return J9PORT_ERROR_PROCESS_INVALID_PARAMS;
	case J9PORT_ERROR_PROCESS_INVALID_STREAMHANDLE:
		return J9PORT_ERROR_PROCESS_INVALID_STREAMHANDLE;
	case ENOENT:
		return J9PORT_ERROR_NOTFOUND;
	case EBADF:
		return J9PORT_ERROR_BADF;
	case EMFILE:
		return J9PORT_ERROR_FILE_SYSTEMFULL;
	default:
		return J9PORT_ERROR_PROCESS_OPFAILED;
	}
}

/**
 * @internal
 *
 * @param[in] fd File descriptor
 *
 * @return
 */
static int
setFdCloexec(int fd)
{
	int rc = -1;
	int flags;

	flags = fcntl(fd, F_GETFD);
	if (flags >= 0) {
		flags |= FD_CLOEXEC;
		rc = fcntl(fd, F_SETFD, flags);
	}
	return rc;
}

/**
 * @internal
 *
 * @param[in] fd File descriptor
 *
 * @return
 */
static int
setNonBlocking(int fd)
{
	int rc = -1;
#if defined(J9ZOS390)
	int flags;

	flags = fcntl(fd, F_GETFL);
	if (flags >= 0) {
	#if defined(O_NDELAY)
		flags |= O_NDELAY;
	#elif defined(FNDELAY)
		flags |= FNDELAY;
	#else
		#error Neither FNDELAY nor O_NDELAY are defined on this platform. \
		Either the _OPEN_SYS feature test macro or the _XOPEN_SOURCE_EXTENDED 1 feature \
		test macro must be installed to allow non-blocking operations on pipes.  
	#endif /* defined(O_NDELAY) */
		rc = fcntl(fd, F_SETFL, flags);
	}

#else
	int nbio = 1;
	
	rc = ioctl(fd, FIONBIO, &nbio);
#endif /* defined(J9ZOS390) */
	
	return rc;
}

intptr_t 
j9process_create(struct J9PortLibrary *portLibrary, const char *command[], uintptr_t commandLength, char *env[], uintptr_t envSize, const char *dir, uint32_t options, intptr_t j9fdInput, intptr_t j9fdOutput, intptr_t j9fdError, J9ProcessHandle *processHandle)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	const char *cmd = NULL;
	int grdpid, rc = 0;
	int	i;
	int newFD[3][2];
	int forkedChildProcess[2];
	int pipeFailed = 0;
	int errorNumber = 0;
	J9ProcessHandleStruct *processHandleStruct;
	char ** newCommand;
	uintptr_t newCommandSize;
	char **execEnv = NULL;

	if (0 == commandLength) {
		return J9PORT_ERROR_PROCESS_COMMAND_LENGTH_ZERO;
	}

	for (i = 0; i < 3; i++) {
		newFD[i][0] = J9PORT_INVALID_FD;
		newFD[i][1] = J9PORT_INVALID_FD;
	}
	/* Build the new io pipes (in/out/err) */
	if (FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDIN, options)) {
		if ( J9PORT_INVALID_FD == j9fdInput ) {
			if ( -1 == pipe(newFD[0]) ) {
				pipeFailed = 1;
			} else {
				if ( FLAG_IS_SET(J9PORT_PROCESS_NONBLOCKING_IO, options) ) {
					setNonBlocking(newFD[0][0]);
					setNonBlocking(newFD[0][1]);
				}
			}
		}
	}
	if (FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDOUT, options)) {
		if (FLAG_IS_NOT_SET(J9PORT_PROCESS_IGNORE_OUTPUT, options)) {
			if (J9PORT_INVALID_FD == j9fdOutput) {
				if ( -1 == pipe(newFD[1]) ) {
					pipeFailed = 1;
				} else {
					if ( FLAG_IS_SET(J9PORT_PROCESS_NONBLOCKING_IO, options) ) {
						setNonBlocking(newFD[1][0]);
						setNonBlocking(newFD[1][1]);
					}
				}
			}
		}
	}
	if (FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDERR, options)) {
		if (FLAG_IS_NOT_SET(J9PORT_PROCESS_IGNORE_OUTPUT, options)) {
			if ((J9PORT_INVALID_FD == j9fdError) && (FLAG_IS_NOT_SET(J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT, options))) {
				if (-1 == pipe(newFD[2])) {
					pipeFailed = 1;
				} else {
					if (FLAG_IS_SET(J9PORT_PROCESS_NONBLOCKING_IO, options)) {
						setNonBlocking(newFD[2][0]);
						setNonBlocking(newFD[2][1]);
					}
				}
			}
		}
	}
	if (-1 == pipe(forkedChildProcess)) { /* pipe for synchronization */
		forkedChildProcess[0] = J9PORT_INVALID_FD;
		forkedChildProcess[1] = J9PORT_INVALID_FD;
		pipeFailed = 1;
	}

	if (pipeFailed) {
		for (i = 0; i < 3; i++) {
			if ( J9PORT_INVALID_FD != newFD[i][0] ) {
				close(newFD[i][0]);
				close(newFD[i][1]);
			}
		}
		if (J9PORT_INVALID_FD != forkedChildProcess[0]) {
			close(forkedChildProcess[0]);
			close(forkedChildProcess[1]);
		}
		return J9PORT_ERROR_PROCESS_OPFAILED;
	}

	setFdCloexec(forkedChildProcess[0]);
	setFdCloexec(forkedChildProcess[1]);

	/* Create a NULL terminated array to contain the command for call to execv[p|e]()
	 * We could do this in the child, but if mem_allocate_memory fails, it's easier to diagnose
	 * failure in parent. Remember to free the memory
	 */
	newCommandSize = (commandLength + 1) * sizeof(uintptr_t);
	newCommand = omrmem_allocate_memory(newCommandSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == newCommand) {
		return J9PORT_ERROR_PROCESS_OPFAILED;
	}
	memset(newCommand, 0, newCommandSize);

	{
		for (i = 0 ; i < commandLength ; i++) {
			intptr_t translateStatus = translateModifiedUtf8ToPlatform(portLibrary, (const uint8_t *)command[i], strlen(command[i]), (uint8_t**) &(newCommand[i]));
			if (0 != translateStatus) {
				/* most likely out of memory, free the strings we just converted. */
				freeTranslatedMemory(portLibrary, newCommand, commandLength, execEnv, envSize);
				return translateStatus;
			}
		}
		cmd = newCommand[0];
	}

	/* If we were giving an environment, it needs to be converted from UTF-8 to platform encoding */
	if (envSize > 0) {
		uintptr_t execEnvSize = sizeof(uintptr_t) * (envSize + 1) /* NULL-terminator*/;
		execEnv = omrmem_allocate_memory(execEnvSize, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == execEnv) {
			/* most likely out of memory, free the strings we just converted. */
			freeTranslatedMemory(portLibrary, newCommand, commandLength, execEnv, envSize);
			return J9PORT_ERROR_PROCESS_OPFAILED;
		}
		memset(execEnv, 0, execEnvSize);

		for (i = 0 ; i < envSize; i++) {
			intptr_t translateStatus = 0;
			if (NULL == env[i]) {
				Trc_PRT_Assert_ShouldNeverHappen();
			}
			translateStatus = translateModifiedUtf8ToPlatform(portLibrary, (const uint8_t *)env[i], strlen(env[i]), (uint8_t**) &(execEnv[i]));
			if (0 != translateStatus) {
				/* most likely out of memory, free the strings we just converted. */
				freeTranslatedMemory(portLibrary, newCommand, commandLength, execEnv, envSize);
				return translateStatus;
			}
		}

		execEnv[envSize] = NULL;
	}

	newCommand[commandLength] = NULL;

	grdpid = fork();
	if (grdpid == 0) {
		/* Child process */
		char dummy = '\0';

		/* Redirect pipes so grand-child inherits new pipes */
		if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDIN, options) ) {
			if ( J9PORT_INVALID_FD == j9fdInput )	{
				rc = dup2(newFD[0][0], 0);
			} else {
				rc = dup2(j9fdInput - FD_BIAS, 0);
			}
		}
		if ( -1 != rc ) {
			if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_IGNORE_OUTPUT, options) ) {
				if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDOUT, options) ) {
					if ( J9PORT_INVALID_FD == j9fdOutput ) {
						rc = dup2(newFD[1][1], 1);
					} else {
						rc = dup2(j9fdOutput - FD_BIAS, 1);
					}
				}
				if ( -1 != rc ) {
					if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT, options) ) {
						if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDERR, options) ) {
							if ( J9PORT_INVALID_FD == j9fdError ) {
								rc = dup2(newFD[2][1], 2);
							} else {
								rc = dup2(j9fdError - FD_BIAS, 2);
							}
						}
					} else {
						rc = dup2(1, 2);
					}
				}
			} else {
				/* redirect stdout and stderr to null fd to avoid sending output to parent's stdout and stderr streams */
				int nullFD;
#if defined(J9ZOS390)
				/* can't use file_open on z/OS because the file descriptor returned has an FD_BIAS
				 * applied to it which causes it not to be recognized by dup2
				 */
				nullFD = open("/dev/null", O_RDWR);
#else /* defined(J9ZOS390) */
				nullFD = (int)omrfile_open("/dev/null", EsOpenWrite, 0);
#endif /* defined(J9ZOS390) */
				rc = dup2(nullFD, 1);
				if ( -1 != rc ) {
					rc = dup2(nullFD, 2);
				}
			}
		}

		/* tells the parent that that very process is running */
		rc = write(forkedChildProcess[1], (void *) &dummy, 1);

		if ( (-1 != rc) && dir) {
			rc = chdir(dir);
		}

#if !defined(J9ZTPF)
		if	(-1 != rc) {
			if ( FLAG_IS_SET(J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP, options) ) {
				pid_t sessionID = setsid();
				if (-1 == sessionID) {
					rc = -1;
				}
			}
		}
#endif /* !defined(J9ZTPF) */

		if (-1 != rc) {
			/* ===try to perform the execv : on success, it does not return ===== */
			if (envSize == 0) {
				rc = execvp(cmd, newCommand);
			} else {
				rc = execve(cmd, newCommand, execEnv);
			}
			/* ===================================================== */
		}

		/* if we get here ==> tell the parent that the execv failed ! Send the error number. */
		write(forkedChildProcess[1], &errno, sizeof(errno));
		close(forkedChildProcess[0]);
		close(forkedChildProcess[1]);
		/* If the exec failed, we must exit or there will be two VM processes running. */
		exit(rc);
	} else {
		/* In the parent process */
		char dummy;

		freeTranslatedMemory(portLibrary, newCommand, commandLength, execEnv, envSize);

		if (J9PORT_INVALID_FD != newFD[0][0]) {
			close(newFD[0][0]);
		}
		if (J9PORT_INVALID_FD != newFD[1][1]) {
			close(newFD[1][1]);
		}
		if (J9PORT_INVALID_FD != newFD[2][1]) {
			close(newFD[2][1]);
		}

		if(grdpid == -1) { /* the fork failed */
			/* close the open pipes */
			close(forkedChildProcess[0]);
			close(forkedChildProcess[1]);
			if (J9PORT_INVALID_FD != newFD[0][1]) {
				close(newFD[0][1]);
			}
			if (J9PORT_INVALID_FD != newFD[1][0]) {
				close(newFD[1][0]);
			}
			if (J9PORT_INVALID_FD != newFD[2][0]) {
				close(newFD[2][0]);
			}
			return findError(errno);
		}


		/* Store the rw handles to the childs io */
		processHandleStruct = (J9ProcessHandleStruct *)omrmem_allocate_memory(sizeof(J9ProcessHandleStruct), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDIN, options)) {
			if (J9PORT_INVALID_FD == j9fdInput) { /* STDIN */
				processHandleStruct->inHandle = (intptr_t) newFD[0][1];
			} else {
				processHandleStruct->inHandle = J9PORT_INVALID_FD;
			}
		} else {
			processHandleStruct->inHandle = J9PORT_INVALID_FD;
		}
		if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_IGNORE_OUTPUT, options) ) {
			if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDOUT, options) ) {
				if ( J9PORT_INVALID_FD == j9fdOutput ) {
					processHandleStruct->outHandle = (intptr_t) newFD[1][0];
				} else {
					processHandleStruct->outHandle = J9PORT_INVALID_FD;
				}
			} else {
				processHandleStruct->outHandle = J9PORT_INVALID_FD;
			}
			if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDERR, options) ) {
				if ( (J9PORT_INVALID_FD == j9fdError) && (FLAG_IS_NOT_SET(J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT, options)) ) {
					processHandleStruct->errHandle = (intptr_t) newFD[2][0];
				} else {
					processHandleStruct->errHandle = J9PORT_INVALID_FD;
				}
			} else {
				processHandleStruct->errHandle = J9PORT_INVALID_FD;
			}
		} else {
			processHandleStruct->outHandle = J9PORT_INVALID_FD;
			processHandleStruct->errHandle = J9PORT_INVALID_FD;
		}

		processHandleStruct->procHandle = (intptr_t) grdpid;
		processHandleStruct->pid = (int32_t) grdpid;

		/* let the forked child start. */
		close(forkedChildProcess[1]);
		read(forkedChildProcess[0], &dummy, 1);

		/* [PR CMVC 143339] OTTBLD: jclmaxtest_jit_G1 Test_Runtime failure
		 * Instead of using timeout to determine if child process has been created successfully,
		 * a single block read/write pipe call is used, if process creation failed, errorNumber
		 * with sizeof(errno) will be returned, otherwise, read will fail due to pipe closure.
		 */
		rc = read(forkedChildProcess[0], &errorNumber, sizeof(errno));
		close(forkedChildProcess[0]);

		if(rc == sizeof(errno)) {
			return findError(errorNumber);
		}
		*processHandle = processHandleStruct;
		return 0;
	}

	return J9PORT_ERROR_PROCESS_OPFAILED;

}


intptr_t 
j9process_waitfor(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{
	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	pid_t retVal = 0;
	int StatusLocation = -1;
	
	retVal = waitpid((pid_t)processHandleStruct->procHandle, &StatusLocation, 0);
	if (retVal==(pid_t)processHandleStruct->procHandle){
		if(WIFEXITED(StatusLocation)!=0) {
			processHandleStruct->exitCode = WEXITSTATUS(StatusLocation);
		}
		return 0;
	} else {
		return findError(errno);
	}
	
}


intptr_t 
j9process_terminate(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{

	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	int32_t rc = kill((pid_t)processHandleStruct->procHandle, SIGTERM);
	
	if (rc != 0) {
		return findError(errno);
	}
	return 0;
}

intptr_t 
j9process_write(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, void *buffer, uintptr_t numBytes) 
{

	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	
	intptr_t retVal =  write(processHandleStruct->inHandle, buffer, numBytes);
	if(retVal<0){
		return findError(errno);
	}
	return retVal;
}

intptr_t 
j9process_read(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t flags, void *buffer, uintptr_t numBytes)
{

	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	intptr_t retVal, sHandle;

	switch(flags) 
	{
	    case J9PORT_PROCESS_STDOUT :
	    	sHandle = processHandleStruct->outHandle; break;
	    case J9PORT_PROCESS_STDERR :
	    	sHandle = processHandleStruct->errHandle; break;
	    default:
			return J9PORT_ERROR_PROCESS_INVALID_PARAMS;
	}
	retVal = read(sHandle, buffer, numBytes);
	/* a return of 0 from the read() function indicates an error  */
	if ( retVal == 0 ) {
		retVal = -1;
	}
	
	if(retVal<0){
		return findError(errno);
	}
	
#if defined(J9ZOS390)
	/* We need to convert whatever we read back to ASCII 
	 * because it will have been written out in EBCDIC */
	if ( -1 == __e2a_s(buffer) ){
		retVal = J9PORT_ERROR_PROCESS_OPFAILED;
	}
#endif
	
	return retVal;
}

intptr_t 
j9process_get_available(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t flags) 
{

	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	int avail, rc, sHandle;
	
#if defined(J9ZOS390)
	struct stat fileStats;
#endif
	
	switch(flags) 
	{
	    case J9PORT_PROCESS_STDOUT :
	    	sHandle = (int)processHandleStruct->outHandle; break;
	    case J9PORT_PROCESS_STDERR :
	    	sHandle = (int)processHandleStruct->errHandle; break;
	    default:
	    	return J9PORT_ERROR_PROCESS_INVALID_PARAMS;
	}

#if defined(J9ZOS390)
	rc = fstat(sHandle,&fileStats);
	if ( 0 == rc ) {
		avail = (int)fileStats.st_size;
	}
#else
	rc = ioctl((int)sHandle, FIONREAD, &avail);
#endif
	
	if (rc == -1) {
		return J9PORT_ERROR_PROCESS_OPFAILED;
	}
	return (intptr_t)avail;
}

intptr_t 
j9process_close(struct J9PortLibrary *portLibrary, J9ProcessHandle *processHandle, uint32_t options)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) *processHandle;
	int32_t error = 0;
	
	if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_DO_NOT_CLOSE_STREAMS, options) ) {
		if (J9PORT_INVALID_FD != processHandleStruct->inHandle){
			if (0 != close((int) processHandleStruct->inHandle)){
				error |= J9PORT_ERROR_PROCESS_CLOSE_INHANDLE;
			}
		}
		if (J9PORT_INVALID_FD != processHandleStruct->outHandle){
			if (0 != close((int) processHandleStruct->outHandle)) {
				error |= J9PORT_ERROR_PROCESS_CLOSE_OUTHANDLE;
			}
		}
		if (J9PORT_INVALID_FD != processHandleStruct->errHandle){
			if (0 != (close((int) processHandleStruct->errHandle) != 0)) {
				error |= J9PORT_ERROR_PROCESS_CLOSE_ERRHANDLE;
			}
		}
	}
	omrmem_free_memory(processHandleStruct);
	processHandleStruct = *processHandle = NULL;
	if (0 != error){
		return findError(error);
	}
	return 0;
}

intptr_t 
j9process_getStream(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t streamFlag, intptr_t *stream) 
{
	intptr_t streamToReturn, rc = J9PORT_INVALID_FD;
	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	
	switch (streamFlag) {
		case J9PORT_PROCESS_STDIN:
			if (processHandleStruct->inHandle) {
				rc = 0;
				streamToReturn = processHandleStruct->inHandle;
			}
			break;
		case J9PORT_PROCESS_STDOUT:
			if (processHandleStruct->outHandle) {
				rc = 0;
				streamToReturn = processHandleStruct->outHandle;
			}
			break;
		case J9PORT_PROCESS_STDERR:
			if (processHandleStruct->errHandle) {
				rc = 0;
				streamToReturn = processHandleStruct->errHandle;
			}
			break;
		default:
			rc = J9PORT_ERROR_PROCESS_INVALID_STREAMFLAG;
	}
	
	if (0 == rc) {
		*stream = streamToReturn;
	} else if (J9PORT_INVALID_FD == rc) {
		rc = J9PORT_ERROR_PROCESS_INVALID_STREAMHANDLE;
	} 
	
	return rc;
}

intptr_t 
j9process_isComplete(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle)
{

	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	int32_t status = 0;
	pid_t retVal = 0;
	
	retVal = waitpid((pid_t)processHandleStruct->procHandle, &status, WNOHANG | WUNTRACED);
	if (retVal==-1) { 
		return 1;
	} else if (retVal==(pid_t)processHandleStruct->procHandle) {
		if (WIFEXITED(status) | WIFSIGNALED(status) /*| WIFSTOPPED(status)*/) {
			if (WIFEXITED(status)!=0){
				processHandleStruct->exitCode = WEXITSTATUS(status);
			}
			return 1;			
		} else {
			return 0;
		}
	}
	return 0;
}


intptr_t 
j9process_get_exitCode(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{
	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	
	return processHandleStruct->exitCode;
}

/* @internal
 * Convert modified UTF8 to the platform encoding. The translated string is terminated with a null character.
 * This functions allocates the output buffer; is the caller's responsibility to free the buffer.
 * Modified UTF8 is the VM internal character code. See "Modified UTF-8 Strings" in the JNI specification.
 * @param[in] portLibrary The port library.
 * @param[in] inBuffer Pointer to the beginning of the string.
 * @param[in] inBytes Size of the string, not including any terminating null.
 * @param[out] outBuffer Set to the allocated output buffer
 * @return 0 on success, negative error code on failure
 * @note
 */
static intptr_t
translateModifiedUtf8ToPlatform(struct J9PortLibrary *portLibrary, const uint8_t *inBuffer, uintptr_t inBufferSize,uint8_t **outBuffer)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uint8_t *result = 0;
	int32_t bufferLength = 0;
	int32_t resultLength = 0;

	*outBuffer = NULL;
	bufferLength = 	omrstr_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
			(char *)inBuffer, inBufferSize, NULL, 0);
	/* get the size of the platform string */

	if (bufferLength < 0 ) {
		return bufferLength; /* some error occurred */
	} else {
		bufferLength += MAX_STRING_TERMINATOR_LENGTH;
	}

	result = omrmem_allocate_memory(bufferLength, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == result)  {
		return J9PORT_ERROR_STRING_MEM_ALLOCATE_FAILED;
	}

	resultLength = omrstr_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
			(char *)inBuffer, inBufferSize, (char *)result, bufferLength);
	/* do the conversion */

	if (resultLength < 0 ) {
		omrmem_free_memory(result);
		return resultLength; /* some error occurred */
	} else {
		Assert_PRT_true(resultLength + MAX_STRING_TERMINATOR_LENGTH == bufferLength);
		/* should get the same length both times */

		memset(result + resultLength, 0, MAX_STRING_TERMINATOR_LENGTH);
		/* null terminate */

		*outBuffer = result;
		return 0;
	}
}

/* @internal
 * Checks newCommand and execEnv for non-NULL, and if non-NULL, free their contents.
 * @param[in] char **newCommand Array of utf8 strings
 * @param[in] uintptr_t commandLength size of newCommand array
 * @param[in] char **execEnv Array of utf8 strings
 * @param[in] uintptr_t execEnvLen size of execEnv array
 */
static void
freeTranslatedMemory(struct J9PortLibrary *portLibrary, char **newCommand, uintptr_t commandLength, char **execEnv, uintptr_t execEnvLen)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	uintptr_t i;
	if (NULL != newCommand) {
		for (i = 0; i < commandLength; i++) {
			if (NULL != newCommand[i]) {
				omrmem_free_memory(newCommand[i]);
			}
		}
		omrmem_free_memory(newCommand);
	}

	if (NULL != execEnv) {
		for (i = 0; i < execEnvLen; i++) {
			if (NULL != execEnv[i]) {
				omrmem_free_memory(execEnv[i]);
			}
		}
		omrmem_free_memory(execEnv);
	}
}
