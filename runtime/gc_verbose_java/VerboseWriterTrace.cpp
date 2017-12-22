
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

/* TODO (stefanbu) Tracing story unavailable for OMR, this file should be moved back to verbose */

#include "j9cfg.h"

#include "VerboseWriterTrace.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"

#define _UTE_STATIC_                  
#include "ut_j9vgc.h"

MM_VerboseWriterTrace::MM_VerboseWriterTrace(MM_EnvironmentBase *env) :
	MM_VerboseWriter(VERBOSE_WRITER_TRACE)
	 ,_componentLoaded(false)
{
	/* no implementation */
}

/**
 * Create a new MM_VerboseWriterTrace instance.
 * @return Pointer to the new MM_VerboseWriterTrace.
 */
MM_VerboseWriterTrace *
MM_VerboseWriterTrace::newInstance(MM_EnvironmentBase *env)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	
	MM_VerboseWriterTrace *agent = (MM_VerboseWriterTrace *)extensions->getForge()->allocate(sizeof(MM_VerboseWriterTrace), MM_AllocationCategory::DIAGNOSTIC, J9_GET_CALLSITE());
	if (agent) {
		new(agent) MM_VerboseWriterTrace(env);
		if(!agent->initialize(env)){
			agent->kill(env);
			agent = NULL;
		}
	}
	return agent;
}

/**
 * Initializes the MM_VerboseWriterTrace instance.
 */
bool
MM_VerboseWriterTrace::initialize(MM_EnvironmentBase *env)
{
	return MM_VerboseWriter::initialize(env);
}

/**
 */
void
MM_VerboseWriterTrace::endOfCycle(MM_EnvironmentBase *env)
{
}

/**
 * Closes the agents output stream.
 */
void
MM_VerboseWriterTrace::closeStream(MM_EnvironmentBase *env)
{
}

void
MM_VerboseWriterTrace::outputString(MM_EnvironmentBase *env, const char* string)
{
	if(!_componentLoaded) {
		/* If this is the first time in, we have to load the j9vgc trace component.
		 * Can't do it at startup because the trace engine initializes too late */
		UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM((J9JavaVM *)env->getLanguageVM()));
		_componentLoaded = true;
	}

	/* Call the tracepoint that outputs the line of verbosegc */
	Trc_VGC_Verbosegc(env->getLanguageVMThread(), string);
}
