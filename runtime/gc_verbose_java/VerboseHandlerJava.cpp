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

/* Temporary file to make compilers happy */

#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "VerboseHandlerJava.hpp"
#include "EnvironmentBase.hpp"
#include "VerboseManager.hpp"
#include "VerboseHandlerOutput.hpp"
#include "VerboseWriterChain.hpp"
#include "GCExtensions.hpp"
#include "FinalizeListManager.hpp"

void
MM_VerboseHandlerJava::outputFinalizableInfo(MM_VerboseManager *manager, MM_EnvironmentBase *env, UDATA indent)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(env);
	GC_FinalizeListManager *finalizeListManager = extensions->finalizeListManager;

	UDATA systemCount = finalizeListManager->getSystemCount();
	UDATA defaultCount = finalizeListManager->getDefaultCount();
	UDATA referenceCount = finalizeListManager->getReferenceCount();
	UDATA classloaderCount = finalizeListManager->getClassloaderCount();

	if((0 != systemCount) || (0 != defaultCount) || (0 != referenceCount) || (0 != classloaderCount)) {
		manager->getWriterChain()->formatAndOutput(env, indent, "<pending-finalizers system=\"%zu\" default=\"%zu\" reference=\"%zu\" classloader=\"%zu\" />", systemCount, defaultCount, referenceCount, classloaderCount);
	}
}

bool
MM_VerboseHandlerJava::getThreadName(char *buf, UDATA bufLen, OMR_VMThread *omrThread)
{
	PORT_ACCESS_FROM_JAVAVM(((J9VMThread*)omrThread->_language_vmthread)->javaVM);
	char* threadName = getOMRVMThreadName(omrThread);
	UDATA threadNameLength = strlen(threadName);
	UDATA escapeConsumed = escapeXMLString(OMRPORT_FROM_J9PORT(PORTLIB), buf, bufLen, threadName, strlen(threadName));
	bool consumedEntireString = (escapeConsumed >= threadNameLength);
	releaseOMRVMThreadName(omrThread);
	return consumedEntireString;
}

void
MM_VerboseHandlerJava::writeVmArgs(MM_VerboseManager *manager, MM_EnvironmentBase* env, J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	JavaVMInitArgs* vmArgs = vm->vmArgsArray->actualVMArgs;
	MM_VerboseWriterChain* writer = manager->getWriterChain();
	writer->formatAndOutput(env, 1, "<vmargs>");
	for (jint i = 0; i < vmArgs->nOptions; ++i) {
		char escapedXMLString[128];
		UDATA optLen = strlen(vmArgs->options[i].optionString);
		UDATA escapeConsumed = escapeXMLString(OMRPORT_FROM_J9PORT(PORTLIB), escapedXMLString, sizeof(escapedXMLString), vmArgs->options[i].optionString, optLen);
		const char* dots = (escapeConsumed < optLen) ? "..." : "";
		if (NULL == vmArgs->options[i].extraInfo) {
			writer->formatAndOutput(env, 2, "<vmarg name=\"%s%s\" />", escapedXMLString, dots);
		} else {
			writer->formatAndOutput(env, 2, "<vmarg name=\"%s%s\" value=\"%p\" />", escapedXMLString, dots, vmArgs->options[i].extraInfo);
		}
	}
	writer->formatAndOutput(env, 1, "</vmargs>");
}
