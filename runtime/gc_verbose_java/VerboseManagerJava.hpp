
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

#if !defined(VERBOSEMANAGERJAVA_HPP_)
#define VERBOSEMANAGERJAVA_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseManager.hpp"
#include "VerboseWriter.hpp"

class MM_EnvironmentBase;
class MM_VerboseHandlerOutput;
class MM_VerboseWriterChain;

/**
 * @ingroup GC_verbose_engine
 */
class MM_VerboseManagerJava : public MM_VerboseManager
{
	/*
	 * Data members
	 */
private:
	/* Pointers to the Hook interface */
	J9HookInterface** _mmHooks;

protected:

public:
	
	/*
	 * Function members
	 */
private:

protected:

	virtual MM_VerboseWriter *createWriter(MM_EnvironmentBase *env, WriterType type, char *filename, UDATA fileCount, UDATA iterations);

	/**
	 * Create the output handler specific to the kind of collector currently running.
	 * @param env[in] The master GC thread
	 * @return An instance of a sub-class of the MM_VerboseHandlerOutput abstract class which can handle output of verbose data for the collector currently running
	 */
	virtual MM_VerboseHandlerOutput *createVerboseHandlerOutputObject(MM_EnvironmentBase *env);

public:

	static MM_VerboseManager *newInstance(MM_EnvironmentBase *env, OMR_VM* vm);

	virtual bool initialize(MM_EnvironmentBase *env);

	J9HookInterface** getHookInterface(){ return _mmHooks; }

	virtual void handleFileOpenError(MM_EnvironmentBase *env, char *fileName);

	MM_VerboseManagerJava(OMR_VM *omrVM)
		: MM_VerboseManager(omrVM)
		, _mmHooks(NULL)
	{
	}
};

#endif /* VERBOSEMANAGERJAVA_HPP_ */
