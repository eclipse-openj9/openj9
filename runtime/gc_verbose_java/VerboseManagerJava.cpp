
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

#include "gcutils.h"
#include "modronnls.h"
#include "omr.h"

#include <string.h>

#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"
#include "VerboseManagerJava.hpp"

#include "VerboseHandlerOutput.hpp"
#if defined(J9VM_GC_REALTIME)
#include "VerboseHandlerOutputRealtime.hpp"
#endif /* defined(J9VM_GC_REALTIME) */
#if defined(J9VM_GC_MODRON_STANDARD)
#include "VerboseHandlerOutputStandardJava.hpp"
#endif /* defined(J9VM_GC_MODRON_STANDARD) */
#if defined(J9VM_GC_VLHGC)
#include "VerboseHandlerOutputVLHGC.hpp"
#endif /* defined(J9VM_GC_VLHGC) */
#include "VerboseWriter.hpp"
#include "VerboseWriterChain.hpp"
#include "VerboseWriterFileLoggingBuffered.hpp"
#include "VerboseWriterFileLoggingSynchronous.hpp"
#include "VerboseWriterHook.hpp"
#include "VerboseWriterStreamOutput.hpp"
#include "VerboseWriterTrace.hpp"

/**
 * Create a new MM_VerboseManagerJava instance.
 * @return Pointer to the new MM_VerboseManagerJava.
 */
MM_VerboseManager *
MM_VerboseManagerJava::newInstance(MM_EnvironmentBase *env, OMR_VM* vm)
{
	MM_GCExtensions* extensions = MM_GCExtensions::getExtensions(vm);
	
	MM_VerboseManagerJava *verboseManager = (MM_VerboseManagerJava *)extensions->getForge()->allocate(sizeof(MM_VerboseManagerJava), MM_AllocationCategory::FIXED, J9_GET_CALLSITE());
	if (verboseManager) {
		new(verboseManager) MM_VerboseManagerJava(vm);
		if(!verboseManager->initialize(env)) {
			verboseManager->kill(env);
			verboseManager = NULL;
		}
	}
	return verboseManager;
}

/**
 * Initializes the MM_VerboseManager instance.
 */
bool
MM_VerboseManagerJava::initialize(MM_EnvironmentBase *env)
{
	PORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env->getOmrVM());
	_mmHooks = J9_HOOK_INTERFACE(extensions->hookInterface);
	_mmPrivateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);
	_omrHooks = J9_HOOK_INTERFACE(extensions->omrHookInterface);

	_writerChain = MM_VerboseWriterChain::newInstance(env);
	if (NULL == _writerChain) {
		return false;
	}

	if(NULL == (_verboseHandlerOutput = createVerboseHandlerOutputObject(env))) {
		return false;
	}

	_lastOutputTime = j9time_hires_clock();

	return true;
}

MM_VerboseHandlerOutput *
MM_VerboseManagerJava::createVerboseHandlerOutputObject(MM_EnvironmentBase *env)
{
	MM_VerboseHandlerOutput *handler = NULL;
	MM_GCExtensionsBase *extensions = env->getExtensions();
	
	if (extensions->isMetronomeGC()) {
#if defined(J9VM_GC_REALTIME)
		handler = MM_VerboseHandlerOutputRealtime::newInstance(env, this);
#endif /* defined(J9VM_GC_REALTIME) */
	} else if (extensions->isVLHGC()) {
#if defined(J9VM_GC_VLHGC)
		handler = MM_VerboseHandlerOutputVLHGC::newInstance(env, this);
#endif /* defined(J9VM_GC_VLHGC) */
	} else if (extensions->isStandardGC()) {
#if defined(J9VM_GC_MODRON_STANDARD)
		handler = MM_VerboseHandlerOutputStandardJava::newInstance(env, this);
#endif /* defined(J9VM_GC_MODRON_STANDARD) */
	} else {
		/* unknown collector type but verbose doesn't have assertions so just return NULL so that a caller will observe that we failed */
	}
	return handler;
}

MM_VerboseWriter *
MM_VerboseManagerJava::createWriter(MM_EnvironmentBase *env, WriterType type, char *filename, UDATA fileCount, UDATA iterations)
{
	MM_VerboseWriter *writer = NULL;

	switch(type) {
	case VERBOSE_WRITER_STANDARD_STREAM:
		writer = MM_VerboseWriterStreamOutput::newInstance(env, filename);
		break;

	case VERBOSE_WRITER_TRACE:
		writer = MM_VerboseWriterTrace::newInstance(env);
		break;

	case VERBOSE_WRITER_HOOK:
		writer = MM_VerboseWriterHook::newInstance(env);
		break;

	case VERBOSE_WRITER_FILE_LOGGING_SYNCHRONOUS:
		writer = MM_VerboseWriterFileLoggingSynchronous::newInstance(env, this, filename, fileCount, iterations);
		if (NULL == writer) {
			writer = findWriterInChain(VERBOSE_WRITER_STANDARD_STREAM);
			if (NULL != writer) {
				writer->isActive(true);
				return writer;
			}
			/* if we failed to create a file stream and there is no stderr stream try to create a stderr stream */
			writer = MM_VerboseWriterStreamOutput::newInstance(env, NULL);
		}
		break;

	case VERBOSE_WRITER_FILE_LOGGING_BUFFERED:
		writer = MM_VerboseWriterFileLoggingBuffered::newInstance(env, this, filename, fileCount, iterations);
		if (NULL == writer) {
			writer = findWriterInChain(VERBOSE_WRITER_STANDARD_STREAM);
			if (NULL != writer) {
				writer->isActive(true);
				return writer;
			}
			/* if we failed to create a file stream and there is no stderr stream try to create a stderr stream */
			writer = MM_VerboseWriterStreamOutput::newInstance(env, NULL);
		}
		break;

	default:
		return NULL;
	}

	return writer;
}

void
MM_VerboseManagerJava::handleFileOpenError(MM_EnvironmentBase *env, char *fileName)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	omrnls_printf(J9NLS_ERROR, J9NLS_GC_UNABLE_TO_OPEN_FILE, fileName);
}

