
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
 
#if !defined(STD_STREAM_OUTPUT_HPP_)
#define STD_STREAM_OUTPUT_HPP_
 
#include "j9.h"
#include "j9cfg.h"

#include "VerboseOutputAgent.hpp"

class MM_EnvironmentBase;

/**
 * Output agent which directs verbosegc output to a standard stream.
 */
class MM_VerboseStandardStreamOutput : public MM_VerboseOutputAgent
{
private:
	typedef enum {
		STDERR = 1,
		STDOUT
	} StreamID;

	MM_VerboseStandardStreamOutput::StreamID _currentStream;

	bool initialize(MM_EnvironmentBase *env, const char *filename);
	virtual void tearDown(MM_EnvironmentBase *env);

	MM_VerboseStandardStreamOutput::StreamID getStreamID(MM_EnvironmentBase *env, const char *string);
	MMINLINE void setStream(MM_VerboseStandardStreamOutput::StreamID stream) { _currentStream = stream; }

public:
	
	virtual bool reconfigure(MM_EnvironmentBase *env, const char *filename, UDATA fileCount, UDATA iterations);
	
	/* Output Routine */
	virtual void formatAndOutput(J9VMThread *vmThread, UDATA indent, const char *format, ...);
	
	virtual void endOfCycle(J9VMThread *vmThread);
	
	void closeStream(MM_EnvironmentBase *env);
	
	static MM_VerboseStandardStreamOutput *newInstance(MM_EnvironmentBase *env, const char *filename);

	MM_VerboseStandardStreamOutput(MM_EnvironmentBase *env) :
		MM_VerboseOutputAgent(env, STANDARD_STREAM)
	{}
};

#endif /* STD_STREAM_OUTPUT_HPP_ */
