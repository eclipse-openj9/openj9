
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
 
#if !defined(FILE_LOG_OUTPUT_HPP_)
#define FILE_LOG_OUTPUT_HPP_
 
#include "j9.h"
#include "j9cfg.h"

#include "VerboseOutputAgent.hpp"

#define VGC_INPUT_STRING_SIZE 256
#define VGC_INDENT_SPACER "  "

enum {
	single_file = 0,
	rotating_files
};

/**
 * Output agent which directs verbosegc output to file.
 */
class MM_VerboseFileLoggingOutput : public MM_VerboseOutputAgent
{
/* fields */
public:
protected:
private:
	char *_filename; /**< the filename template supplied from the command line */
	UDATA _numFiles; /**< number of files to rotate through */
	UDATA _numCycles; /**< number of cycles in each file */
	
	UDATA _mode; /**< rotation mode -- single_file or rotating_files */
	UDATA _currentFile; /**< zero-based index of current rotating file */
	UDATA _currentCycle; /**< current GC cycle within this file */

	IDATA _logFileDescriptor; /**< the file being written to */
	
	J9StringTokens *_tokens; /**< tokens used during filename expansion */

/* methods */
public:
	virtual void formatAndOutput(J9VMThread *vmThread, UDATA indent, const char *format, ...);
	
	virtual bool reconfigure(MM_EnvironmentBase *env, const char* filename, UDATA fileCount, UDATA iterations);

	bool openFile(MM_EnvironmentBase *env);
	void closeFile(MM_EnvironmentBase *env);
	
	void closeStream(MM_EnvironmentBase *env);

	virtual void endOfCycle(J9VMThread *vmThread);
	
	static MM_VerboseFileLoggingOutput *newInstance(MM_EnvironmentBase *env, char* filename, UDATA fileCount, UDATA iterations);

	MM_VerboseFileLoggingOutput(MM_EnvironmentBase *env)
		: MM_VerboseOutputAgent(env, FILE_LOGGING)
		, _filename(NULL)
		, _mode(single_file)
		, _currentFile(0)
		, _currentCycle(0)
		, _logFileDescriptor(-1)
		, _tokens(NULL)
	{}

protected:

private:
	bool initialize(MM_EnvironmentBase *env, const char *filename, UDATA numFiles, UDATA numCycles);
	virtual void tearDown(MM_EnvironmentBase *env);
	IDATA findInitialFile(MM_EnvironmentBase *env);
	bool initializeFilename(MM_EnvironmentBase *env, const char *filename);
	bool initializeTokens(MM_EnvironmentBase *env);
	char* expandFilename(MM_EnvironmentBase *env, UDATA currentFile);

};

#endif /* FILE_LOG_OUTPUT_HPP_ */
