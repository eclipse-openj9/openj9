
/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "VerboseStandardStreamOutput.hpp"
#include "VerboseEvent.hpp"
#include "VerboseBuffer.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include <string.h>

#define INPUT_STRING_SIZE 236
#define INDENT_SPACER "  "

/**
 * Formats and output data.
 * Called by events, formats the data passed into verbose form and outputs it.
 * @param indent The current level of indentation.
 * @param format String to format the data into.
 */
void
MM_VerboseStandardStreamOutput::formatAndOutput(J9VMThread *vmThread, UDATA indent, const char *format, ...)
{
	char inputString[INPUT_STRING_SIZE];
	char localBuf[256];
	UDATA length;
	va_list args;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment((OMR_VMThread *)vmThread->omrVMThread);
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	localBuf[0] = '\0';
	for(UDATA i=0; i < indent; i++) {
		strcat(localBuf, INDENT_SPACER);
	}
	
	va_start(args, format);
	j9str_vprintf(inputString, INPUT_STRING_SIZE, format, args);
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

	/* If there's a problem with the buffering, we just write straight to the tty */
	if(STDERR == _currentStream){
		j9file_write_text(J9PORT_TTY_ERR, localBuf, length);
	} else {
		j9file_write_text(J9PORT_TTY_OUT, localBuf, length);
	}
}

/**
 * Create a new MM_VerboseStandardStreamOutput instance.
 */
MM_VerboseStandardStreamOutput *
MM_VerboseStandardStreamOutput::newInstance(MM_EnvironmentBase *env, const char *filename)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());

	MM_VerboseStandardStreamOutput *agent = (MM_VerboseStandardStreamOutput *)extensions->getForge()->allocate(sizeof(MM_VerboseStandardStreamOutput), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if(agent) {
		new(agent) MM_VerboseStandardStreamOutput(env);
		if(!agent->initialize(env, filename)) {
			agent->kill(env);
			agent = NULL;
		}
	}
	return agent;
}

/**
 * Initializes the MM_VerboseStandardStreamOutput instance.
 */
bool
MM_VerboseStandardStreamOutput::initialize(MM_EnvironmentBase *env, const char *filename)
{
	J9JavaVM* javaVM = (J9JavaVM *)env->getOmrVM()->_language_vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	const char* version = javaVM->memoryManagerFunctions->omrgc_get_version(env->getOmrVM());
	
	setStream(getStreamID(env, filename));
	
	if(STDERR == _currentStream){
		j9file_printf(PORTLIB, J9PORT_TTY_ERR, "\n" VERBOSEGC_HEADER_TEXT_ALL, version);
	} else {
		j9file_printf(PORTLIB, J9PORT_TTY_OUT, "\n" VERBOSEGC_HEADER_TEXT_ALL, version);
	}
	
	if(NULL == (_buffer = MM_VerboseBuffer::newInstance(env, INITIAL_BUFFER_SIZE))) {
		return false;
	}
	
	return true;
}

/**
 * Closes the agent's output stream.
 */
void
MM_VerboseStandardStreamOutput::closeStream(MM_EnvironmentBase *env)
{
	UDATA length = strlen(VERBOSEGC_FOOTER_TEXT);
	PORT_ACCESS_FROM_ENVIRONMENT(env);

	if(STDERR == _currentStream){
		j9file_write_text(J9PORT_TTY_ERR, VERBOSEGC_FOOTER_TEXT "\n", length + 1);
	} else {
		j9file_write_text(J9PORT_TTY_OUT, VERBOSEGC_FOOTER_TEXT "\n", length + 1);
	}
}

/**
 * Tear down the structures managed by the MM_VerboseStandardStreamOutput.
 * Tears down the verbose buffer.
 */
void
MM_VerboseStandardStreamOutput::tearDown(MM_EnvironmentBase *env)
{
	if(NULL != _buffer) {
		_buffer->kill(env);
	}
}

/**
 * Flushes the verbose buffer to the output stream.
 */
void
MM_VerboseStandardStreamOutput::endOfCycle(J9VMThread *vmThread)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	
	if(NULL != _buffer){
		if(STDERR == _currentStream){
			j9file_write_text(J9PORT_TTY_ERR, _buffer->contents(), _buffer->currentSize());
			j9file_write_text(J9PORT_TTY_ERR, "\n", 1);
		} else {
			j9file_write_text(J9PORT_TTY_OUT, _buffer->contents(), _buffer->currentSize());
			j9file_write_text(J9PORT_TTY_OUT, "\n", 1);
		}
		_buffer->reset();
	}
}

bool
MM_VerboseStandardStreamOutput::reconfigure(MM_EnvironmentBase *env, const char *filename, UDATA fileCount, UDATA iterations)
{
	setStream(getStreamID(env, filename));
	return true;
}

MM_VerboseStandardStreamOutput::StreamID
MM_VerboseStandardStreamOutput::getStreamID(MM_EnvironmentBase *env, const char *string)
{
	if(NULL == string) {
		return STDERR;
	}
	
	if(!strcmp(string, "stdout")) {
		return STDOUT;
	}
	
	return STDERR;
}
