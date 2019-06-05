
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

#include "modronnls.h"

#include "VerboseFileLoggingOutput.hpp"
#include "VerboseEvent.hpp"
#include "GCExtensions.hpp"
#include "VerboseBuffer.hpp"
#include "EnvironmentBase.hpp"

#include <string.h>

/**
 * Create a new MM_VerboseFileLoggingOutput instance.
 * @return Pointer to the new MM_VerboseFileLoggingOutput.
 */
MM_VerboseFileLoggingOutput *
MM_VerboseFileLoggingOutput::newInstance(MM_EnvironmentBase *env, char *filename, UDATA numFiles, UDATA numCycles)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	
	MM_VerboseFileLoggingOutput *agent = (MM_VerboseFileLoggingOutput *)extensions->getForge()->allocate(sizeof(MM_VerboseFileLoggingOutput), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(agent) {
		new(agent) MM_VerboseFileLoggingOutput(env);
		if(!agent->initialize(env, filename, numFiles, numCycles)) {
			agent->kill(env);
			agent = NULL;
		}
	}
	return agent;
}

/**
 * Initializes the MM_VerboseFileLoggingOutput instance.
 * @return true on success, false otherwise
 */
bool
MM_VerboseFileLoggingOutput::initialize(MM_EnvironmentBase *env, const char *filename, UDATA numFiles, UDATA numCycles)
{
	_numFiles = numFiles;
	_numCycles = numCycles;
	
	if((_numFiles > 0) && (_numCycles > 0)) {
		_mode = rotating_files;
	} else {
		_mode = single_file;
	}
	
	if (!initializeTokens(env)) {
		return false;
	}
	
	if (!initializeFilename(env, filename)) {
		return false;
	}
	
	IDATA initialFile = findInitialFile(env);
	if (initialFile < 0) {
		return false;
	}
	_currentFile = initialFile;
	
	if(!openFile(env)) {
		return false;
	}
	
	if(NULL == (_buffer = MM_VerboseBuffer::newInstance(env, INITIAL_BUFFER_SIZE))) {
		return false;
	}
	
	return true;
}

/**
 * Tear down the structures managed by the MM_VerboseFileLoggingOutput.
 * Tears down the verbose buffer.
 */
void
MM_VerboseFileLoggingOutput::tearDown(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	
	if(NULL != _buffer) {
		_buffer->kill(env);
	}
	
	j9str_free_tokens(_tokens);
	extensions->getForge()->free(_filename);
}

/**
 * Initialize the _tokens field.
 * for backwards compatibility with Sovereign, alias %p to be the same as %pid
 */
bool 
MM_VerboseFileLoggingOutput::initializeTokens(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	char pidBuffer[64];
	
	_tokens = j9str_create_tokens(j9time_current_time_millis());
	if (_tokens == NULL) {
		return false;
	}
	
	if (sizeof(pidBuffer) < j9str_subst_tokens(pidBuffer, sizeof(pidBuffer), "%pid", _tokens)) {
		return false;
	}
	
	if (j9str_set_token(PORTLIB, _tokens, "p", "%s", pidBuffer)) {
		return false;
	}
	
	return true;
}

/**
 * Initialize the _filename field based on filename.
 *
 * Since token substitution only supports tokens starting with %, all # characters in the 
 * filename will be replaced with %seq (unless the # is already preceded by an odd number 
 * of % signs, in which case it is replaced with seq).
 *
 * e.g.  foo#   --> foo%seq
 *       foo%#  --> foo%seq
 *       foo%%# --> foo%%%seq
 * 
 * If %seq or %# is not specified, and if rotating logs have been requested, ".%seq" is 
 * appended to the log name.
 * 
 * If the resulting filename is too large to fit in the buffer, it is truncated.
 * 
 * @param[in] env the current environment
 * @param[in] filename the user specified filename
 * 
 * @return true on success, false on failure
 */
bool 
MM_VerboseFileLoggingOutput::initializeFilename(MM_EnvironmentBase *env, const char *filename)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	
	if (_mode == rotating_files) {
		const char* read = filename;

		/* count the number of hashes in the source string */
		UDATA hashCount = 0;
		for (read = filename; '\0' != *read; read++) {
			if ('#' == *read) {
				hashCount++;
			}
		}
		
		/* allocate memory for the copied template filename */ 
		UDATA nameLen = strlen(filename) + 1;
		if (hashCount > 0) {
			/* each # expands into %seq, so for each # add 3 to len */
			nameLen += hashCount * (sizeof("seq") - 1);
		} else {
			/* if there are no hashes we may append .%seq to the end */
			nameLen += sizeof(".%seq") - 1;
		}

		_filename = (char*)extensions->getForge()->allocate(nameLen, MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
		if (NULL == _filename) {
			return false;
		}
		
		/* copy the original filename into the allocated memory, expanding #s to %seq */
		bool foundSeq = false;
		bool oddPercents = false;
		char* write = _filename;
		for (read = filename; '\0' != *read; read++) {
			/* check to see if %seq appears in the source filename */
			if (oddPercents && (0 == strncmp(read, "seq", 3))) {
				foundSeq = true;
			}

			if ('#' == *read) {
				strcpy(write, oddPercents ? "seq" : "%seq");
				write += strlen(write);
			} else {
				*write++ = *read;
			}
			oddPercents = ('%' == *read) ? !oddPercents : false;
		}

		*write = '\0';
		
		if ( (false == foundSeq) && (0 == hashCount) ) {
			strcpy(write, ".%seq");
		}
	} else {
		_filename = (char*)extensions->getForge()->allocate(strlen(filename) + 1, MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
		if (NULL == _filename) {
			return false;
		}
		strcpy(_filename, filename);
	}

	return true;
}

/**
 * Formats and output data.
 * Called by events, formats the data passed into verbose form and outputs it.
 * @param indent The current level of indentation.
 * @param format String to format the data into.
 */
void
MM_VerboseFileLoggingOutput::formatAndOutput(J9VMThread *vmThread, UDATA indent, const char *format, ...)
{
	char inputString[VGC_INPUT_STRING_SIZE];
	char localBuf[VGC_INPUT_STRING_SIZE];
	UDATA length;
	va_list args;
	
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment((OMR_VMThread *)vmThread->omrVMThread);
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	localBuf[0] = '\0';
	for(UDATA i=0; i < indent; i++) {
		strcat(localBuf, VGC_INDENT_SPACER);
	}
	
	va_start(args, format);
	j9str_vprintf(inputString, VGC_INPUT_STRING_SIZE - strlen(localBuf), format, args);
	va_end(args);
	
	strcat(localBuf, inputString);
	strcat(localBuf, "\n");
	length = strlen(localBuf);
	
	if(NULL != _buffer) {
		if(_buffer->add(env, localBuf)) {
			/* Added successfully - return */
			return;
		}
	}

	/* If there's a problem with the buffering, we just write straight to the file or stderr */
	if(-1 != _logFileDescriptor) {
		j9file_write_text(_logFileDescriptor, localBuf, length);
	} else {
		j9file_write_text(J9PORT_TTY_ERR, localBuf, length);
	}
}

/**
 * Generate an expanded filename based on currentFile.
 * The caller is responsible for freeing the returned memory.
 * 
 * @param env the current thread
 * @param currentFile the current file number to substitute into the filename template
 * 
 * @return NULL on failure, allocated memory on success 
 */
char* 
MM_VerboseFileLoggingOutput::expandFilename(MM_EnvironmentBase *env, UDATA currentFile)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());

	if (_mode == rotating_files) {
		j9str_set_token(PORTLIB, _tokens, "seq", "%03zu", currentFile + 1); /* plus one so the filenames start from .001 instead of .000 */
	}
	
	UDATA len = j9str_subst_tokens(NULL, 0, _filename, _tokens);
	char *filenameToOpen = (char*)extensions->getForge()->allocate(len, MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if (NULL != filenameToOpen) {
		j9str_subst_tokens(filenameToOpen, len, _filename, _tokens);
	}
	return filenameToOpen;
	
}

/**
 * Probe the file system for existing files. Determine
 * the first number which is unused, or the number of the oldest
 * file if all numbers are used.
 * @return the first file number to use (starting at 0), or -1 on failure
 */
IDATA 
MM_VerboseFileLoggingOutput::findInitialFile(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	I_64 oldestTime = J9CONST64(0x7FFFFFFFFFFFFFFF); /* the highest possible time. */
	IDATA oldestFile = 0;

	if (_mode != rotating_files) {
		/* nothing to do */
		return 0;
	}

	for (UDATA currentFile = 0; currentFile < _numFiles; currentFile++) {
		char *filenameToOpen = expandFilename(env, currentFile);
		if (NULL == filenameToOpen) {
			return -1;
		}

		I_64 thisTime = j9file_lastmod(filenameToOpen);
		extensions->getForge()->free(filenameToOpen);
		
		if (thisTime < 0) {
			/* file doesn't exist, or some other problem reading the file */
			oldestFile = currentFile;
			break;
		} else if (thisTime < oldestTime) {
			oldestTime = thisTime;
			oldestFile = currentFile;
		}
	}
	
	return oldestFile; 
}

/**
 * Opens the file to log output to and prints the header.
 * @return true on success, false otherwise
 */
bool
MM_VerboseFileLoggingOutput::openFile(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	J9JavaVM* javaVM = (J9JavaVM *)env->getOmrVM()->_language_vm;
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(javaVM);
	const char* version = javaVM->memoryManagerFunctions->omrgc_get_version(env->getOmrVM());
	
	char *filenameToOpen = expandFilename(env, _currentFile);
	if (NULL == filenameToOpen) {
		return false;
	}
	
	_logFileDescriptor = j9file_open(filenameToOpen, EsOpenRead | EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if(-1 == _logFileDescriptor) {
		char *cursor = filenameToOpen;
		/**
		 * This may have failed due to directories in the path not being available.
		 * Try to create these directories and attempt to open again before failing.
		 */
		while ( (cursor = strchr(++cursor, DIR_SEPARATOR)) != NULL ) {
			*cursor = '\0';
			j9file_mkdir(filenameToOpen);
			*cursor = DIR_SEPARATOR;
		}

		/* Try again */
		_logFileDescriptor = j9file_open(filenameToOpen, EsOpenRead | EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
		if (-1 == _logFileDescriptor) {
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_GC_UNABLE_TO_OPEN_FILE, filenameToOpen);
			extensions->getForge()->free(filenameToOpen);
			return false;
		}
	}

	extensions->getForge()->free(filenameToOpen);
	
	j9file_printf(PORTLIB, _logFileDescriptor, VERBOSEGC_HEADER_TEXT_ALL, version);
	
	return true;
}

/**
 * Prints the footer and closes the file being logged to.
 */
void
MM_VerboseFileLoggingOutput::closeFile(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	
	if(-1 != _logFileDescriptor){
		UDATA length = strlen(VERBOSEGC_FOOTER_TEXT "\n");
		j9file_write_text(_logFileDescriptor, VERBOSEGC_FOOTER_TEXT "\n", length);
		j9file_close(_logFileDescriptor);
		_logFileDescriptor = -1;
	}
}

/**
 * Closes the agent's output stream.
 */
void
MM_VerboseFileLoggingOutput::closeStream(MM_EnvironmentBase *env)
{
	closeFile(env);
}

/**
 * Flushes the verbose buffer to the output stream.
 * Also cycles the output files if necessary.
 */
void
MM_VerboseFileLoggingOutput::endOfCycle(J9VMThread *vmThread)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment((OMR_VMThread *)vmThread->omrVMThread);
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	
	if(-1 == _logFileDescriptor) {
		/* we open the file at the end of the cycle so can't have a final empty file at the end of a run */
		openFile(env);
	}
	
	if(NULL != _buffer) {
		if(-1 != _logFileDescriptor){
			j9file_write_text(_logFileDescriptor, _buffer->contents(), _buffer->currentSize());
			j9file_write_text(_logFileDescriptor, "\n", 1);
		} else {
			j9file_write_text(J9PORT_TTY_ERR, _buffer->contents(), _buffer->currentSize());
			j9file_write_text(J9PORT_TTY_ERR, "\n", 1);
		}
		_buffer->reset();
	}
	
	if(rotating_files == _mode) {
		_currentCycle = (_currentCycle + 1) % _numCycles;
		if(0 == _currentCycle) {
			closeFile(env);
			_currentFile = (_currentFile + 1) % _numFiles;
		}
	}
}

/**
 * Reconfigures the agent according to the parameters passed.
 * Required for Dynamic verbose gc configuration.
 * @param filename The name of the file or output stream to log to.
 * @param fileCount The number of files to log to.
 * @param iterations The number of gc cycles to log to each file.
 */
bool
MM_VerboseFileLoggingOutput::reconfigure(MM_EnvironmentBase *env, const char *filename, UDATA numFiles, UDATA numCycles)
{
	closeFile(env);
	return initialize(env, filename, numFiles, numCycles);
}
