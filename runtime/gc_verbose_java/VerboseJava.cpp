/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include "j9.h"
#include "j9cfg.h"
#include "mmhook.h"

#include "AtomicOperations.hpp"
#include "Base.hpp"
#include "GCExtensions.hpp"
#include "VerboseEventStream.hpp"
#include "VerboseOutputAgent.hpp"
#include "VerboseWriter.hpp"
#include "VerboseManagerJava.hpp"
#include "VerboseManagerOld.hpp"

#include "gcutils.h"
#include "modronnls.h"

#include <string.h>

#include "EnvironmentBase.hpp"

extern "C" {

static UDATA gcDebugVerboseStartupLogging(J9JavaVM *javaVM, char* filename, UDATA numFiles, UDATA numCycles);
static void gcDebugVerboseShutdownLogging(J9JavaVM *javaVM, UDATA releaseVerboseStructures);
static void gcDumpMemorySizes(J9JavaVM *javaVM);
static UDATA configureVerbosegc(J9JavaVM *javaVM, int enable, char* filename, UDATA numFiles, UDATA numCycles);
static UDATA queryVerbosegc(J9JavaVM *javaVM);

/**
 * Verbose Function table.
 */
J9MemoryManagerVerboseInterface functionTable = {
	gcDebugVerboseStartupLogging,
	gcDebugVerboseShutdownLogging,
	gcDumpMemorySizes,
	configureVerbosegc,
	queryVerbosegc
};

/**
 * Initializes the verbose function table.
 */
void
initializeVerboseFunctionTable(J9JavaVM *javaVM)
{
	J9MemoryManagerVerboseInterface *mmFuncTable;
	J9MemoryManagerVerboseInterface *verboseTable = &functionTable;

	mmFuncTable = (J9MemoryManagerVerboseInterface *)javaVM->memoryManagerFunctions->getVerboseGCFunctionTable(javaVM);

	mmFuncTable->gcDebugVerboseStartupLogging = verboseTable->gcDebugVerboseStartupLogging;
	mmFuncTable->gcDebugVerboseShutdownLogging = verboseTable->gcDebugVerboseShutdownLogging;
	mmFuncTable->gcDumpMemorySizes = verboseTable->gcDumpMemorySizes;
	mmFuncTable->configureVerbosegc = verboseTable->configureVerbosegc;
	mmFuncTable->queryVerbosegc = verboseTable->queryVerbosegc;
}

static UDATA
configureVerbosegc(J9JavaVM *javaVM, int enable, char* filename, UDATA numFiles, UDATA numCycles)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	MM_VerboseManagerBase *manager = extensions->verboseGCManager;

	if (!manager && !enable) {
		/* they're turning verbosegc off, but it's never been started?! */
		return 1;
	}

	if (!manager) {
		/* startup the verbosegc manager */
		MM_EnvironmentBase env(javaVM->omrVM);
		if (extensions->verboseNewFormat) {
			manager = MM_VerboseManagerJava::newInstance(&env, javaVM->omrVM);
		} else {
			/* Used with -Xgc:verboseFormat:deprecated */
			manager = MM_VerboseManagerOld::newInstance(&env, javaVM->omrVM);
		}
		if (manager) {
			(MM_GCExtensions::getExtensions(javaVM))->verboseGCManager = manager;
		} else {
			return 0;
		}
		manager = (MM_GCExtensions::getExtensions(javaVM))->verboseGCManager;
	}

	if (!manager->configureVerboseGC(javaVM->omrVM, filename, numFiles, numCycles)) {
		return 0;
	}

	if (enable) {
		manager->enableVerboseGC();
	} else {
		manager->disableVerboseGC();
	}

	return 1;
}

/**
 * Query whether verbosegc is currently enabled.
 * Return the number of output agents that are currently enabled.
 */
static UDATA
queryVerbosegc(J9JavaVM *javaVM)
{
	MM_VerboseManagerBase *manager = (MM_GCExtensions::getExtensions(javaVM))->verboseGCManager;

	if (NULL != manager) {
		return manager->countActiveOutputHandlers();
	}

	return 0;
}

/**
 * Allocates and initializes the verbose gc infrastructure.
 * @param filename The name of the file or output stream to log to.
 * @param numFiles The number of files to log to.
 * @param numCycles The number of gc cycles to log to each file.
 * @return 1 if successful, 0 otherwise.
 */
static UDATA
gcDebugVerboseStartupLogging(J9JavaVM *javaVM, char* filename, UDATA numFiles, UDATA numCycles)
{
	return configureVerbosegc(javaVM, 1, filename, numFiles, numCycles);
}

/**
 * Bring down the verbose gc infrastructure.
 * Closes the verbose gc output streams and if safe releases the infrastructure.
 * @param releaseVerboseStructures Indicator of whether it is safe to free the infrastructure.
 * @return Pointer to the allocated memory.
 */
static void
gcDebugVerboseShutdownLogging(J9JavaVM *javaVM, UDATA releaseVerboseStructures)
{
	MM_EnvironmentBase env(javaVM->omrVM);
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);

	if (NULL == extensions) {
		return;
	}

	MM_VerboseManagerBase *manager = extensions->verboseGCManager;

	if (NULL == manager) {
		return;
	}

	manager->closeStreams(&env);

	if (releaseVerboseStructures) {
		manager->kill(&env);
		extensions->verboseGCManager = NULL;
	}
}

/**
 * Dump sizes with the correct qualifier.
 * @param byteSize The size in bytes.
 * @param optionName The name of the option.
 * @param module_name The NLS module to look in
 * @param message_num The message number to get from the module
 */
void
gcDumpQualifiedSize(J9PortLibrary* portLib, UDATA byteSize, const char* optionName, U_32 module_name, U_32 message_num)
{
	char buffer[16] = {'\0'};
	UDATA size = 0;
	UDATA paramSize = 0;
	const char* qualifier = NULL;
	const char* optionDescription = NULL;
	PORT_ACCESS_FROM_PORT(portLib);

	/* Obtain the qualified size (e.g. 2k) */
	size = byteSize;
	qualifiedSize(&size, &qualifier);

	/* look up the appropriate translation */
	optionDescription = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(
		OMRPORT_FROM_J9PORT(PORTLIB),
		J9NLS_DO_NOT_APPEND_NEWLINE | J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
		module_name,
		message_num,
		NULL);

	/* Format output String */
	paramSize = j9str_printf(PORTLIB, buffer, 16, "%zu%s", size, qualifier);
	paramSize = 15 - paramSize;
	paramSize += strlen(optionDescription);
	paramSize -= strlen(optionName);
	j9tty_printf(PORTLIB, "  %s%s %*s\n", optionName, buffer, paramSize, optionDescription);

	return;
}

/**
 * Dump gc memory sizes.
 * Output is intentionally ordered the same as "j9 -X"
 */
static void
gcDumpMemorySizes(J9JavaVM *javaVM)
{
	MM_GCExtensions *extensions = MM_GCExtensions::getExtensions(javaVM);
	UDATA *pageSizes = NULL;
	PORT_ACCESS_FROM_JAVAVM(javaVM);

	gcDumpQualifiedSize(PORTLIB, javaVM->ramClassAllocationIncrement, "-Xmca", J9NLS_GC_VERB_SIZES_XMCA);
	gcDumpQualifiedSize(PORTLIB, javaVM->romClassAllocationIncrement, "-Xmco", J9NLS_GC_VERB_SIZES_XMCO);

#if defined(OMR_GC_COMPRESSED_POINTERS)
	if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(javaVM)) {
		gcDumpQualifiedSize(PORTLIB, extensions->suballocatorInitialSize, "-Xmcrs", J9NLS_GC_VERB_SIZES_XMCRS);
	} else
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	{
		gcDumpQualifiedSize(PORTLIB, 0, "-Xmcrs", J9NLS_GC_VERB_SIZES_XMCRS);
	}

	if (extensions->isVLHGC()) {
#if defined (J9VM_GC_VLHGC)
		gcDumpQualifiedSize(PORTLIB, extensions->tarokIdealEdenMinimumBytes, "-Xmns", J9NLS_GC_VERB_SIZES_XMNS);
		gcDumpQualifiedSize(PORTLIB, extensions->tarokIdealEdenMaximumBytes, "-Xmnx", J9NLS_GC_VERB_SIZES_XMNX);
#endif /* J9VM_GC_VLHGC */
	} else if (!extensions->isMetronomeGC()) {
		gcDumpQualifiedSize(PORTLIB, extensions->newSpaceSize, "-Xmns", J9NLS_GC_VERB_SIZES_XMNS);
		gcDumpQualifiedSize(PORTLIB, extensions->maxNewSpaceSize, "-Xmnx", J9NLS_GC_VERB_SIZES_XMNX);
	}
	gcDumpQualifiedSize(PORTLIB, extensions->initialMemorySize, "-Xms", J9NLS_GC_VERB_SIZES_XMS);
	if (!(extensions->isMetronomeGC())) {
		gcDumpQualifiedSize(PORTLIB, extensions->oldSpaceSize, "-Xmos", J9NLS_GC_VERB_SIZES_XMOS);
		gcDumpQualifiedSize(PORTLIB, extensions->maxOldSpaceSize, "-Xmox", J9NLS_GC_VERB_SIZES_XMOX);
	}

	if (extensions->allocationIncrementSetByUser) {
		gcDumpQualifiedSize(PORTLIB, extensions->allocationIncrement, "-Xmoi", J9NLS_GC_VERB_SIZES_XMOI);
	}
	gcDumpQualifiedSize(PORTLIB, extensions->memoryMax, "-Xmx", J9NLS_GC_VERB_SIZES_XMX);

	if (extensions->isStandardGC()) {
#if defined(J9VM_GC_MODRON_SCAVENGER)
		gcDumpQualifiedSize(PORTLIB, extensions->rememberedSet.getGrowSize(), "-Xmr", J9NLS_GC_VERB_SIZES_XMR);
#endif /* J9VM_GC_GENERATIONAL */
	}

	if (0 != extensions->softMx) {
		gcDumpQualifiedSize(PORTLIB, extensions->softMx, "-Xsoftmx", J9NLS_GC_VERB_SIZES_XSOFTMX);
	}


	pageSizes = j9vmem_supported_page_sizes();
	/* If entry at index 1 of supported page size array is non zero, then large pages are available */
	{
		const char* optionDescription = NULL;
		UDATA *pageFlags = NULL;
		UDATA pageIndex = 0;
		UDATA size = 0;
		const char *qualifier = NULL;
		const char* optionName = "-Xlp:objectheap:pagesize=";
		char postOption[16] = {'\0'};

		/* Obtain the qualified size (e.g. 4K) */
		size = extensions->requestedPageSize;
		qualifiedSize(&size, &qualifier);

		/* look up the appropriate translation */
		optionDescription = j9nls_lookup_message(
			J9NLS_DO_NOT_APPEND_NEWLINE | J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
			J9NLS_GC_VERB_SIZES_XLP,
			NULL);

		if (J9PORT_VMEM_PAGE_FLAG_NOT_USED != extensions->requestedPageFlags) {
			j9str_printf(PORTLIB, postOption, 16, ",%s", getPageTypeString(extensions->requestedPageFlags));
		}

		j9tty_printf(PORTLIB, "  %s%zu%s%s\t %s\n", optionName, size, qualifier, postOption, optionDescription);

		pageFlags = j9vmem_supported_page_flags();

		optionDescription = j9nls_lookup_message(J9NLS_DO_NOT_APPEND_NEWLINE | J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
			J9NLS_GC_VERB_SIZES_AVAILABLE_XLP,
			NULL);

		j9tty_printf(PORTLIB, "  %*s %s", 15, " ", optionDescription);

		for(pageIndex = 0; 0 != pageSizes[pageIndex]; pageIndex++) {
			const char *pageTypeString = NULL;
			size = pageSizes[pageIndex];
			qualifiedSize(&size, &qualifier);
			j9tty_printf(PORTLIB, "\n  %*s %zu%s", 15, " ", size, qualifier);
			if (J9PORT_VMEM_PAGE_FLAG_NOT_USED != pageFlags[pageIndex]) {
				pageTypeString = getPageTypeString(pageFlags[pageIndex]);
			}
			if (NULL != pageTypeString) {
				j9tty_printf(PORTLIB, " %s", pageTypeString);
			}
		}
		j9tty_printf(PORTLIB, "\n");
	}

	return;
}

} /* extern "C" */
