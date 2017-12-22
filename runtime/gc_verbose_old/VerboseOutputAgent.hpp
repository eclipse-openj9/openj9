
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

#if !defined(_OUTPUT_AGENT_HPP_)
#define _OUTPUT_AGENT_HPP_
 
#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "Base.hpp"

class MM_VerboseEventStream;
class MM_EnvironmentBase;
class MM_VerboseBuffer;

typedef enum {
	STANDARD_STREAM = 1,
	FILE_LOGGING = 2,
	TRACE = 3,
	HOOK = 4
} AgentType;

/** Text output at the start of verbosegc */
#define VERBOSEGC_HEADER_TEXT_LINE1 "<?xml version=\"1.0\" ?>"
#define VERBOSEGC_HEADER_TEXT_LINE2 "<verbosegc version=\"%s\">"
#define VERBOSEGC_HEADER_TEXT_ALL VERBOSEGC_HEADER_TEXT_LINE1 "\n\n" VERBOSEGC_HEADER_TEXT_LINE2 "\n\n"
/** Text output at the end of verbosegc */
#define VERBOSEGC_FOOTER_TEXT "</verbosegc>"

/**
 * The base class for an output agent - actual agents
 * subclass this.
 * 
 * @ingroup GC_verbose_output_agents
 */
class MM_VerboseOutputAgent : public MM_Base
{
protected:
	MM_VerboseOutputAgent *_nextAgent;
	AgentType _type;
	bool _isActive;
	
	MM_VerboseBuffer* _buffer;
	
	virtual void tearDown(MM_EnvironmentBase *env);

public:
	MMINLINE AgentType getType(void) { return _type; }
	
	MMINLINE bool isActive(void) { return _isActive; }
	MMINLINE void isActive(bool isActive) { _isActive = isActive; }

	MMINLINE MM_VerboseOutputAgent *getNextAgent(void) { return _nextAgent; }
	MMINLINE void setNextAgent(MM_VerboseOutputAgent *agent) { _nextAgent = agent; }
	
	void processEventStream(MM_EnvironmentBase *env, MM_VerboseEventStream *eventStream);
	
	virtual void formatAndOutput(J9VMThread *vmThread, UDATA indent, const char *format, ...) = 0;
	
	virtual void endOfCycle(J9VMThread *vmThread) = 0;
	
	virtual void closeStream(MM_EnvironmentBase *env) = 0;

	virtual void kill(MM_EnvironmentBase *env);
	
	virtual bool reconfigure(MM_EnvironmentBase *env, const char *filename, UDATA fileCount, UDATA iterations) = 0;
	
	MM_VerboseOutputAgent(MM_EnvironmentBase *env, AgentType type) :
		_nextAgent(NULL),
		_type(type),
		_isActive(false),
		_buffer(NULL)
	{}
};

#endif /* _OUTPUT_AGENT_HPP_ */
