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
 * @brief exec
 */
#include <windows.h>
#include <psapi.h>
#include "j9port.h"
#include "omrportptb.h"
#include "portpriv.h"

typedef struct J9ProcessWin32Pipes {
	HANDLE inR; 
	HANDLE inW;
	HANDLE outR;
	HANDLE outW;
	HANDLE errR;
	HANDLE errW; 
	HANDLE inDup;
	HANDLE outDup;
	HANDLE errDup;
} J9ProcessWin32Pipes;

static int32_t findError (int32_t errorCode);
static void closeAndDestroyPipes(J9ProcessWin32Pipes *pipes);

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
	case ERROR_ACCESS_DENIED:
		return J9PORT_ERROR_PROCESS_NOPERMISSION;
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		return J9PORT_ERROR_NOTFOUND;
	case ERROR_INVALID_HANDLE:
		return J9PORT_ERROR_INVALID_HANDLE;
#if 0
	case ERROR_DISK_FULL:
		return J9PORT_ERROR_FILE_DISKFULL;
	case ERROR_FILE_EXISTS:
	case ERROR_ALREADY_EXISTS:
		return J9PORT_ERROR_FILE_EXIST;
	case ERROR_NOT_ENOUGH_MEMORY:
		return J9PORT_ERROR_FILE_SYSTEMFULL;
	case ERROR_LOCK_VIOLATION:
	  return J9PORT_ERROR_FILE_LOCK_NOREADWRITE;
#endif /* 0 */
	default:
		return J9PORT_ERROR_PROCESS_OPFAILED;
	}
}

/**
 * @internal
 * Determines the proper portable error code to return given a native error code
 * and call closeAndDestroyPipes(pipes) afterwards
 *
 * @param[in] errorCode The error code reported by the OS
 * @param[in] pipes 	all pipes in J9ProcessWin32Pipes struct
 *
 * @return	the (negative) portable error code
 */
static intptr_t
returnErrorAndClearup (int32_t errorCode, J9ProcessWin32Pipes *pipes)
{
	intptr_t rc = findError(errorCode);

	closeAndDestroyPipes(pipes);

	pipes->inW = NULL;
	pipes->outR = NULL;
	pipes->errR = NULL;

	return	rc;
}

/**
 * Closes and destroys all pipes in J9ProcessWin32Pipes struct
 */
static void
closeAndDestroyPipes(J9ProcessWin32Pipes *pipes)
{
	intptr_t i, numHandles;
	HANDLE *handles[] = {&pipes->inR,&pipes->inW,&pipes->inDup,
						&pipes->outR,&pipes->outW,&pipes->outDup,
						&pipes->errR,&pipes->errW,&pipes->errDup};
	
	numHandles = sizeof(handles) / sizeof(HANDLE *);
	
	for (i = 0; i < numHandles; i++ ) {
		if ( NULL != *handles[i] ) {
			CloseHandle(*handles[i]);
		}
	}
	
	ZeroMemory( pipes, sizeof(J9ProcessWin32Pipes) );
}

/**
 * Converts command string array into a single unicode string command line
 * 
 * @returns 0 upon success, negative portable error code upon failure
 */
static intptr_t
getUnicodeCmdline(struct J9PortLibrary *portLibrary, const char *command[], uintptr_t commandLength, wchar_t* *unicodeCmdline)
{
	char* needToBeQuoted;
	size_t length, l;
	intptr_t rc = 0;
	intptr_t i;
	char* ptr;
	const char* argi;
	char* commandAsString = NULL;
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	
	/* 
	 * Windows needs a char* command line unlike regular C exec* functions
	 * Therefore we need to rebuild the line that has been sliced in java...
	 * Subtle : if a token embeds a <space>, the token will be quoted (only
	 * 			if it hasn't been quoted yet) The quote char is "
	 * Note (see "XXX references in the code)
		 * Our CDev scanner/parser does not handle '"' correctly. A workaround is to close
		 * the '"' with another " , embedded in a C comment. 
	 */ 
	needToBeQuoted = (char *)omrmem_allocate_memory(commandLength, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == needToBeQuoted) {
		rc = J9PORT_ERROR_PROCESS_OPFAILED;
	} else {
		memset(needToBeQuoted, '\0', commandLength);

		length = commandLength; /*add 1 <blank> between each token + a reserved place for the last NULL*/
		for (i = (intptr_t)commandLength; --i >=0;) {
			intptr_t j;
			size_t commandILength;
			const char *commandStart;
			commandILength = strlen(command[i]);
			length += commandILength;
			/* check_for_embedded_space */
			if (commandILength>0) {
				commandStart = command[i];
				if (commandStart[0]!='"' /*"XXX*/ ) {
					for (j=0; j<(intptr_t)commandILength ; j++) {
						if (commandStart[j] == ' ' ) {
							needToBeQuoted[i] = '\1'; /* a random value, different from zero though*/
							length += 2; /* two quotes are added */
							if (commandILength > 1 && commandStart[commandILength-1] == '\\'
								&& commandStart[commandILength-2] != '\\')
								length++; /* need to double slash */
							break;
						}
					}
				}
			} /* end of check_for_embedded_space */
		}
		
		ptr = commandAsString = (char *)omrmem_allocate_memory(length, OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == commandAsString) {
			omrmem_free_memory(needToBeQuoted);
			rc = J9PORT_ERROR_PROCESS_OPFAILED;
		} else {
			uintptr_t k;
			
			for ( k = 0; k < commandLength ; k++ ) {
				l = strlen(argi = command[k]);	
				if (needToBeQuoted[k]) {
					*ptr++ = '"' /*"XXX*/ ; 
				} 
				memcpy(ptr, argi, l);
				ptr+=l;
				if (needToBeQuoted[k]) {
					if (l > 1 && *(ptr-1) == '\\' && *(ptr-2) != '\\') {
						*ptr++ = '\\';
					}
					*ptr++ ='"' /*"XXX*/ ;
				}
				*ptr++ = ' '; /* put a <blank> between each token */
			}
			*(ptr-1) = '\0'; /*commandLength > 0 ==> valid operation*/
			omrmem_free_memory(needToBeQuoted);

			*unicodeCmdline = (wchar_t *)omrmem_allocate_memory((length + 1) * 2, OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == *unicodeCmdline) {
				rc = J9PORT_ERROR_PROCESS_OPFAILED;
			} else {
				port_convertFromUTF8(OMRPORTLIB, commandAsString, *unicodeCmdline, length + 1);
				omrmem_free_memory(commandAsString);
			}
		}	
	}

	return rc;
}

intptr_t 
j9process_create(struct J9PortLibrary *portLibrary, const char *command[], uintptr_t commandLength, char *env[], uintptr_t envSize, const char *dir, uint32_t options, intptr_t j9fdInput, intptr_t j9fdOutput, intptr_t j9fdError, J9ProcessHandle *processHandle)
{
	
	intptr_t rc = 0;
	STARTUPINFOW sinfo;
	PROCESS_INFORMATION pinfo;  
	SECURITY_ATTRIBUTES sAttrib;
	wchar_t* unicodeDir = NULL;
	wchar_t* unicodeCmdline = NULL;
	wchar_t* unicodeEnv = NULL;
	J9ProcessHandleStruct *processHandleStruct;
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	J9ProcessWin32Pipes pipes;
	DWORD dwMode = PIPE_NOWAIT;	/* it's only used if J9PORT_PROCESS_NONBLOCKING_IO is set */

	if (0 == commandLength) {
		return J9PORT_ERROR_PROCESS_COMMAND_LENGTH_ZERO;
	}

	processHandleStruct = (J9ProcessHandleStruct *)omrmem_allocate_memory(sizeof(J9ProcessHandleStruct), OMRMEM_CATEGORY_PORT_LIBRARY);

	ZeroMemory( &sinfo, sizeof(sinfo) );
	ZeroMemory( &pinfo, sizeof(pinfo) );
	ZeroMemory( &sAttrib, sizeof(sAttrib) );

	sinfo.cb = sizeof(sinfo);
	if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDIN, options) ||
			FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDOUT, options) ||
			FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDERR, options) ) {
		sinfo.dwFlags = STARTF_USESTDHANDLES;
	}

	/* Allow handle inheritance */
	sAttrib.bInheritHandle = 1;
	sAttrib.nLength = sizeof(sAttrib);

	ZeroMemory( &pipes, sizeof(J9ProcessWin32Pipes) );

	if ( FLAG_IS_SET(J9PORT_PROCESS_INHERIT_STDIN, options) ) {
		processHandleStruct->inHandle = J9PORT_INVALID_FD;
		if (sinfo.dwFlags == STARTF_USESTDHANDLES) {
			sinfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);;
		}
	} else if ( J9PORT_INVALID_FD != j9fdInput ) {
		processHandleStruct->inHandle = J9PORT_INVALID_FD;
		sinfo.hStdInput = (HANDLE)j9fdInput;
	} else {
		//	create input pipe
		if ( 0 == CreatePipe(&(pipes.inR), &(pipes.inW), &sAttrib, 512) ) {
			return returnErrorAndClearup(GetLastError(), &pipes);
		} else {
			if ( 0 == DuplicateHandle(GetCurrentProcess(), pipes.inW, GetCurrentProcess(),
					&(pipes.inDup), 0, FALSE, DUPLICATE_SAME_ACCESS) ) {
				return returnErrorAndClearup(GetLastError(), &pipes);
			}
			if ( 0 == CloseHandle(pipes.inW) ) {
				return returnErrorAndClearup(GetLastError(), &pipes);
			}
			pipes.inW = NULL;
			if ( FLAG_IS_SET(J9PORT_PROCESS_NONBLOCKING_IO, options) ) {
				if ( 0 == SetNamedPipeHandleState(pipes.inR, &dwMode, NULL, NULL) ) {
					return returnErrorAndClearup(GetLastError(), &pipes);
				}
				if ( 0 == SetNamedPipeHandleState(pipes.inDup, &dwMode, NULL, NULL) ) {
					return returnErrorAndClearup(GetLastError(), &pipes);
				}
			}
		}
		sinfo.hStdInput = pipes.inR;
		processHandleStruct->inHandle = (intptr_t) pipes.inDup;
	}

	if ( FLAG_IS_SET(J9PORT_PROCESS_INHERIT_STDOUT, options) ) {
		processHandleStruct->outHandle = J9PORT_INVALID_FD;
		if (sinfo.dwFlags == STARTF_USESTDHANDLES) {
			sinfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);;
		}
	} else  if ( J9PORT_INVALID_FD != j9fdOutput ) {
		if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_IGNORE_OUTPUT, options) ) {
			sinfo.hStdOutput = (HANDLE)j9fdOutput;
		}
		processHandleStruct->outHandle = J9PORT_INVALID_FD;
	} else {
		//	create output pipe
		if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_IGNORE_OUTPUT, options) ) {
			if ( 0 == CreatePipe(&(pipes.outR), &(pipes.outW), &sAttrib, 512) ) {
				return returnErrorAndClearup(GetLastError(), &pipes);
			} else {
				if ( 0 == DuplicateHandle(GetCurrentProcess(), pipes.outR, GetCurrentProcess(),
						&(pipes.outDup), 0, FALSE, DUPLICATE_SAME_ACCESS) ) {
					return returnErrorAndClearup(GetLastError(), &pipes);
				}
				if ( 0 == CloseHandle(pipes.outR) ) {
					return returnErrorAndClearup(GetLastError(), &pipes);
				}
				pipes.outR = NULL;
				if ( FLAG_IS_SET(J9PORT_PROCESS_NONBLOCKING_IO, options) ) {
					if ( 0 == SetNamedPipeHandleState(pipes.outW, &dwMode, NULL, NULL) ) {
						return returnErrorAndClearup(GetLastError(), &pipes);
					} else if ( 0 == SetNamedPipeHandleState(pipes.outDup, &dwMode, NULL, NULL) ) {
						return returnErrorAndClearup(GetLastError(), &pipes);
					}
				}
			}
			processHandleStruct->outHandle = (intptr_t) pipes.outDup;
		} else {
			processHandleStruct->outHandle = J9PORT_INVALID_FD;
		}
		sinfo.hStdOutput = pipes.outW;
	}

	if ( FLAG_IS_SET(J9PORT_PROCESS_INHERIT_STDERR, options) ) {
		processHandleStruct->errHandle = J9PORT_INVALID_FD;
		if (sinfo.dwFlags == STARTF_USESTDHANDLES) {
			sinfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
		}
	} else if ( J9PORT_INVALID_FD != j9fdError ) {
		if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_IGNORE_OUTPUT, options) ) {
			if ( FLAG_IS_SET(J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT, options) ) {
				sinfo.hStdError = sinfo.hStdOutput;
			} else {
				sinfo.hStdError = (HANDLE)j9fdError;
			}
		}
		processHandleStruct->errHandle = J9PORT_INVALID_FD;
	} else {
		if ( FLAG_IS_SET(J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT, options) ) {
			sinfo.hStdError = sinfo.hStdOutput;
			processHandleStruct->errHandle = J9PORT_INVALID_FD;
		} else {
			//	create error pipe
			if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_IGNORE_OUTPUT, options) ) {
				if ( 0 == CreatePipe(&(pipes.errR), &(pipes.errW), &sAttrib, 512) ) {
					return returnErrorAndClearup(GetLastError(), &pipes);
				} else {
					if ( 0 == DuplicateHandle(GetCurrentProcess(), pipes.errR, GetCurrentProcess(),
							&(pipes.errDup), 0, FALSE, DUPLICATE_SAME_ACCESS) ) {
						return returnErrorAndClearup(GetLastError(), &pipes);
					}
					if ( NULL != pipes.errR ) {
						if ( 0 == CloseHandle(pipes.errR) ) {
							return returnErrorAndClearup(GetLastError(), &pipes);
						}
					}
					pipes.errR = NULL;
					if ( FLAG_IS_SET(J9PORT_PROCESS_NONBLOCKING_IO, options) ) {
						if ( 0 == SetNamedPipeHandleState(pipes.errW, &dwMode, NULL, NULL) ) {
							return returnErrorAndClearup(GetLastError(), &pipes);
						} else if ( 0 == SetNamedPipeHandleState(pipes.errDup, &dwMode, NULL, NULL) ) {
							return returnErrorAndClearup(GetLastError(), &pipes);
						}
					}
				}
				processHandleStruct->errHandle = (intptr_t) pipes.errDup;
			} else {
				processHandleStruct->errHandle = J9PORT_INVALID_FD;
			}
			sinfo.hStdError = pipes.errW;
		}
	}

	/* Build the environment block */
	if (envSize) {
		uintptr_t i;
		size_t envLength, envRemaining;
		wchar_t* cursor;
		
		/* Length of strings + null terminators, excluding final null terminator */
		envRemaining = (size_t) envSize;		
		for (i = 0; i < envSize; i++) {
			envRemaining += strlen(env[i]);
		}
		unicodeEnv = (wchar_t *)omrmem_allocate_memory((envRemaining + 1) * 2, OMRMEM_CATEGORY_PORT_LIBRARY);		/* +1 for final null terminator, * 2 for unicode */
		if(NULL == unicodeEnv) {
			rc = J9PORT_ERROR_PROCESS_OPFAILED;
		} else {
			cursor = unicodeEnv;
			for (i = 0; i < envSize; i++) {
				port_convertFromUTF8(OMRPORTLIB, env[i], cursor, envRemaining);
				envLength = wcslen(cursor);
				cursor += envLength;
				*cursor++ = '\0';
				envRemaining -= envLength;
			}
			*cursor = '\0';
		}
	}

	if (0 == rc) {
		rc = getUnicodeCmdline(portLibrary, command, commandLength, &unicodeCmdline);
		if (0 == rc) {
			if (NULL != dir) {
				size_t length = strlen(dir);
				unicodeDir = (wchar_t*)omrmem_allocate_memory((length + 1) * 2, OMRMEM_CATEGORY_PORT_LIBRARY);
				if (NULL == unicodeDir) {
					rc = J9PORT_ERROR_PROCESS_OPFAILED;
				} else {
					port_convertFromUTF8(OMRPORTLIB, dir, unicodeDir, length + 1);
				}
			}



			if ( 0 == rc ) {

				DWORD creationFlags = CREATE_UNICODE_ENVIRONMENT;

				if ( FLAG_IS_SET(J9PORT_PROCESS_CREATE_NEW_PROCESS_GROUP, options)) {
					creationFlags |= CREATE_NEW_PROCESS_GROUP;
				}

				if ( 0 == CreateProcessW(	/* CreateProcessW returns 0 upon failure */
						NULL,
						unicodeCmdline,
						NULL,
						NULL,
						TRUE,
						creationFlags,	/* add DEBUG_ONLY_THIS_PROCESS for smoother debugging */
						unicodeEnv,
						unicodeDir,
						&sinfo,
						&pinfo)           /* Pointer to PROCESS_INFORMATION structure; */
				) {
					rc = findError(GetLastError());
				}else{
					processHandleStruct->procHandle = (intptr_t) pinfo.hProcess;
					/* Close Handles passed to child if there is any */
					if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDIN, options) ) {
						if ( J9PORT_INVALID_FD == j9fdInput) {
							CloseHandle(pipes.inR);
						} else {
							CloseHandle((HANDLE)j9fdInput);
						}
					}
					if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_IGNORE_OUTPUT, options) ) {
						if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDOUT, options) ) {
							if ( J9PORT_INVALID_FD == j9fdOutput ) {
								CloseHandle(pipes.outW);
							} else {
								CloseHandle((HANDLE)j9fdOutput);
							}
						}
						if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_INHERIT_STDERR, options) ) {
							if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_REDIRECT_STDERR_TO_STDOUT, options) ) {
								if ( J9PORT_INVALID_FD == j9fdError ) {
									CloseHandle(pipes.errW);
								} else {
									CloseHandle((HANDLE)j9fdError);
								}
							}
						}
					}
					CloseHandle(pinfo.hThread);	/*implicitly created, a leak otherwise*/
				}
			}
		}
	}	

	if (NULL != unicodeCmdline) omrmem_free_memory(unicodeCmdline);
	if (NULL != unicodeDir) omrmem_free_memory(unicodeDir);
	if (NULL != unicodeEnv) omrmem_free_memory(unicodeEnv);

	if ( 0 != rc ) {
		/* If an error occurred close all handles */
		closeAndDestroyPipes(&pipes);
	} else {
		*processHandle = processHandleStruct;
	}
	
	return rc;
	
}

intptr_t 
j9process_waitfor(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{
    intptr_t rc = J9PORT_ERROR_PROCESS_OPFAILED;
	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	
	if (WAIT_OBJECT_0 == WaitForSingleObject((HANDLE) processHandleStruct->procHandle, INFINITE)) {
		DWORD procstat = 0;

		if (0 != GetExitCodeProcess((HANDLE)processHandleStruct->procHandle, (LPDWORD)&procstat)) {
			processHandleStruct->exitCode = (intptr_t) procstat;
			rc = 0;
		}
	}

	return rc;
}

intptr_t 
j9process_terminate(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{
	
	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	
	if (!TerminateProcess((HANDLE)processHandleStruct->procHandle, 0)) {
		return J9PORT_ERROR_PROCESS_OPFAILED;
	}
	return 0;
}

intptr_t 
j9process_write(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, void *buffer, uintptr_t numBytes) 
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *)processHandle;
	
	intptr_t retVal =  omrfile_write(processHandleStruct->inHandle, buffer, (intptr_t)numBytes);
	if (retVal < 0) {
		return findError(omrerror_last_error_number());
	}
	return retVal;
}

intptr_t
j9process_read(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t flags, void *buffer, uintptr_t numBytes) 
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
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
	retVal = omrfile_read(sHandle, buffer, (intptr_t)numBytes);
	if (retVal < 0) {
		return findError(omrerror_last_error_number());
	}
	return retVal;
	
	
}

intptr_t 
j9process_get_available(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle, uintptr_t flags) 
{
	

	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	intptr_t retVal = 0, availBytes = 0;
	HANDLE sHandle;
		
	switch(flags) 
	{
	    case J9PORT_PROCESS_STDOUT :
	    	sHandle = (HANDLE)processHandleStruct->outHandle; break;
	    case J9PORT_PROCESS_STDERR :
	    	sHandle = (HANDLE)processHandleStruct->errHandle; break;
	    default:
	    	return J9PORT_ERROR_PROCESS_INVALID_PARAMS;
	}
	retVal = PeekNamedPipe((HANDLE)sHandle,
						   NULL,
						   (DWORD)0,
						   NULL,
						   (LPDWORD)&availBytes,
						   NULL);
	if (!retVal) {
		return findError(GetLastError());
	}
	return availBytes;
	
	
}

intptr_t 
j9process_close(struct J9PortLibrary *portLibrary, J9ProcessHandle *processHandle, uint32_t options)
{
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) *processHandle;
	int32_t error = 0;
	
	if (!CloseHandle((HANDLE) processHandleStruct->procHandle)){
		error = J9PORT_ERROR_PROCESS_CLOSE_PROCHANDLE;
	}
	if ( FLAG_IS_NOT_SET(J9PORT_PROCESS_DO_NOT_CLOSE_STREAMS, options) ) {
		if (J9PORT_INVALID_FD != processHandleStruct->inHandle) {
			if (!CloseHandle((HANDLE) processHandleStruct->inHandle)){
				error = J9PORT_ERROR_PROCESS_CLOSE_INHANDLE;
			}
		}
		if (J9PORT_INVALID_FD != processHandleStruct->outHandle) {
			if (!CloseHandle((HANDLE) processHandleStruct->outHandle)){
				error = J9PORT_ERROR_PROCESS_CLOSE_OUTHANDLE;
			}
		}

		if (J9PORT_INVALID_FD != processHandleStruct->errHandle) {
			if (!CloseHandle((HANDLE) processHandleStruct->errHandle)){
				error = J9PORT_ERROR_PROCESS_CLOSE_ERRHANDLE;
			}
		}
	}
	omrmem_free_memory(processHandleStruct);
	processHandleStruct = *processHandle = NULL;
	if (0 != error) {
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
	intptr_t rc = J9PORT_ERROR_PROCESS_OPFAILED;

	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;
	intptr_t procstat = WaitForSingleObject((HANDLE) processHandleStruct->procHandle, 0);
	
	switch(procstat)
	{
		case WAIT_OBJECT_0: 
			{
				DWORD exCode = 0;

				if (0 != GetExitCodeProcess((HANDLE)processHandleStruct->procHandle, (LPDWORD)&exCode)) {
					processHandleStruct->exitCode = (intptr_t) exCode;
					rc = 1;
				}
			}
			break;
		case WAIT_TIMEOUT:
			rc = 0; 
			break;
		case WAIT_ABANDONED:
		case WAIT_FAILED:
		default:
			break;
	}
	
	return rc;
}


intptr_t 
j9process_get_exitCode(struct J9PortLibrary *portLibrary, J9ProcessHandle processHandle) 
{
	J9ProcessHandleStruct *processHandleStruct = (J9ProcessHandleStruct *) processHandle;

	return processHandleStruct->exitCode;
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
	OMRPORT_ACCESS_FROM_J9PORT(portLibrary);
	HANDLE hProcess;
	PSAPI_WORKING_SET_INFORMATION workingSet;
	PSAPI_WORKING_SET_INFORMATION* dynamicWorkingSet = 0;
	ULONG privateBytes = 0;
	size_t dynamicSize;
	BOOL ok;

	if (0 == pid) {
		hProcess = GetCurrentProcess();
	} else {
		hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
		if (0 == hProcess) {
			return 0;
		}
	}

	/* Figure out then allocate enough space to interrogate working set */
	QueryWorkingSet(hProcess, &workingSet, sizeof(PSAPI_WORKING_SET_INFORMATION));
	dynamicSize = sizeof(PSAPI_WORKING_SET_INFORMATION) + ((workingSet.NumberOfEntries * 2) * sizeof(PSAPI_WORKING_SET_BLOCK));
	dynamicWorkingSet = omrmem_allocate_memory(dynamicSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (0 != dynamicWorkingSet) {
		memset(dynamicWorkingSet, 0, dynamicSize);
		ok = QueryWorkingSet(hProcess, dynamicWorkingSet, (DWORD)dynamicSize);
		if (ok) {
			ULONG i;
			ULONG privatePages = 0;
			BOOL isWow64;

			/* Count the number of entries where PSAPI_WORKING_SET_INFORMATION::WorkingSetInfo[nI].Shared is false.
			 * Multiply that by the bytes-per-page size (SYSTEM_INFO::dwPageSize) using GetSystemInfo() to get the
			 * total private working set.
			 */
			for (i=0; i < dynamicWorkingSet->NumberOfEntries; i++) {
				if (!dynamicWorkingSet->WorkingSetInfo[i].Shared) {
					privatePages++;
				}
			}

			if (IsWow64Process(GetCurrentProcess(), &isWow64)) {
				SYSTEM_INFO sysinfo;
				if (TRUE == isWow64) {
					GetNativeSystemInfo(&sysinfo);
				} else {
					GetSystemInfo(&sysinfo);
				}
				privateBytes = privatePages * sysinfo.dwPageSize;
			}
		}
		omrmem_free_memory(dynamicWorkingSet);
	}

	CloseHandle(hProcess);
	return privateBytes;
}

