
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

/**
 * @file
 * @ingroup GC_Trace
 */

#if !defined(TGCEXTENSIONS_HPP_)
#define TGCEXTENSIONS_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "BaseNonVirtual.hpp"
#include "GCExtensions.hpp"
#include "TgcBacktrace.hpp"
#include "TgcDump.hpp"
#include "TgcExclusiveaccess.hpp"
#include "TgcExcessivegc.hpp"
#include "TgcFreelist.hpp"
#include "TgcHeap.hpp"
#include "TgcParallel.hpp"
#include "TgcAllocation.hpp"
#include "TgcTerse.hpp"
#include "TgcNuma.hpp"

#if defined(J9VM_GC_MODRON_STANDARD)
#include "TgcConcurrent.hpp"
#include "TgcConcurrentcardcleaning.hpp"
#include "TgcScavenger.hpp"
#endif /* defined(J9VM_GC_MODRON_STANDARD) */

#if defined(J9VM_GC_MODRON_STANDARD)
/**
 * Structure holding information relating to tgc tracing for compaction.
 */
typedef struct TgcCompactionExtensions {
	UDATA unused;
} TgcCompactionExtensions;
#endif /* defined(J9VM_GC_MODRON_STANDARD) */

#if defined(J9VM_GC_VLHGC)
/**
 * Structure holding information relating to tgc tracing for compaction.
 */
typedef struct TgcWriteOnceCompactionExtensions {
	UDATA unused;
} TgcWriteOnceCompactionExtensions;

/**
 * Structure holding information relating to tgc tracing for inter-region remembered set demographics.
 */
typedef struct TgcInterRegionRememberedSetDemographicsExtension {
	UDATA incrementCount; /**< A persistent counter of how many times the demographic data has been printed */
	J9HashTable *classHashTable; /**< A shared hash table of class data, repopulated in each iteration */
	UDATA errorCount; /**< A shared count of errors, reset in each increment */
	UDATA totalRememberedObjects; /**< A shared count of total remembered objects, reset in each increment */
	omrthread_monitor_t mutex; /**< A monitor used to protect the shared resources when parallel threads merge their results */
} TgcInterRegionRememberedSetDemographicsExtension;
#endif /* J9VM_GC_VLHGC */

/**
 * Class for handling trace information.
 * This class stores state information and variables needed by the
 * functions associated with the trace output.
 */
class MM_TgcExtensions : public MM_BaseNonVirtual
{
/*
 * Data members
 */
private:
	J9PortLibrary* _portLibrary; /**< The port library to use for TGC */
	IDATA _outputFile; /**< The file handle TGC output should be written to */
protected:
public:
	/* data used to save parsed requests whatever are compatible with GC policy or not */
	bool _allocationRequested; /**< true if "allocation" option is parsed */
	bool _largeAllocationRequested; /**< true if "largeAllocation" option is parsed */
	bool _largeAllocationVerboseRequested; /**< true if "_largeAllocationVerboseRequested" option is parsed */
	bool _backtraceRequested; /**< true if "backtrace" option is parsed */
	bool _compactionRequested; /**< true if "compaction" option is parsed */
	bool _concurrentRequested; /**< true if "concurrent" option is parsed */
	bool _cardCleaningRequested; /**< true if "cardcleaning" option is parsed */
	bool _dumpRequested; /**< true if "dump" option is parsed */
	bool _exclusiveAccessRequested; /**< true if "exclusiveaccess" option is parsed */
	bool _excessiveGCRequested; /**< true if "excessivegc" option is parsed */
	bool _freeListSummaryRequested; /**< true if "freeListSummary" option is parsed */
	bool _freeListRequested; /**< true if "freeList" option is parsed */
	bool _heapRequested; /**< true if "heap" option is parsed */
	bool _parallelRequested; /**< true if "parallel" option is parsed */
	bool _rootScannerRequested; /**< true if "rootscantime" option is parsed */
	bool _scavengerRequested; /**< true if "scavenger" option is parsed */
	bool _scavengerSurvivalStatsRequested; /**< true if "scavengerSurvivalStats" option is parsed */
	bool _scavengerMemoryStatsRequested; /**< true if "scavengerMemoryStats" option is parsed */
	bool _terseRequested; /**< true if "terse" option is parsed */
	bool _interRegionRememberedSetRequested; /**< true if "rememberedSetCardList" option is parsed */
	bool _interRegionRememberedSetDemographicsRequested; /**< true if "rememberedSetDemographics" option is parsed */
	bool _numaRequested; /**< true if "numa" option is parsed */
	bool _allocationContextRequested; /**< true if "allocationContext" option is parsed */
	bool _intelligentCompactRequested; /**< true if "intelligentCompact" option is parsed */
	bool _dynamicCollectionSetRequested; /**< true if "dynamicCollectionSet" option is parsed */
	bool _projectedStatsRequested; /**< true if "projectedStats" option is parsed */
	bool _writeOnceCompactTimingRequested; /**< true if "writeOnceCompactTiming" option is parsed */
	bool _copyForwardRequested; /**< true if "copyForward" option is parsed */
	bool _interRegionReferencesRequested; /**< true if "interRegionReferences" option is parsed */
	bool _sizeClassesRequested; /**< true if "sizeClasses" option is parsed */

	TgcBacktraceExtensions _backtrace;
	TgcDumpExtensions _dump;
	TgcExclusiveAccessExtensions _exclusiveAccess;
	TgcExcessiveGCExtensions _excessiveGC;
	TgcFreeListExtensions _freeList;
	TgcParallelExtensions _parallel;
	TgcTerseExtensions _terse;

#if defined(J9VM_GC_MODRON_STANDARD)
	TgcCompactionExtensions _compaction;
	TgcConcurrentExtensions _concurrent;
	TgcConcurrentCardCleaningExtensions _cardCleaning;
	TgcScavengerExtensions _scavenger;
#endif /* defined(J9VM_GC_MODRON_STANDARD) */

#if defined(J9VM_GC_VLHGC)
	TgcNumaExtensions _numa;
	TgcWriteOnceCompactionExtensions _writeOnceCompaction;
	U_8 *_rsclDistinctFlagArray;  /* hash table used for distinct card list detection */
	void *_dynamicCollectionSetData;  /**< Private data used by dynamic collection set TGC extensions */
	TgcInterRegionRememberedSetDemographicsExtension _interRegionRememberedSetDemographics;
#endif /* J9VM_GC_VLHGC */

/*
 * Function members
 */
private:
protected:
	/**
	 * Construct a new TGC extensions object
	 * @param extensions[in] the GC extensions
	 */
	MM_TgcExtensions(MM_GCExtensions *extensions);
	
	/**
	 * Tear down a TGC extensions object
	 * @param extensions[in] the GC extensions
	 */
	void tearDown(MM_GCExtensions *extensions);

public:
	/**
	 * Fetch the cached port library pointer
	 * @return the cached port library pointer
	 */
	J9PortLibrary* getPortLibrary() { return _portLibrary; }
	
	/**
	 * Print TGC output to stderr or the TGC output file
	 * @param format[in] a format string, see j9tty_printf
	 * @param args[in] arguments to be formatted
	 */
	void vprintf(const char *format, va_list args);
	
	/**
	 * Print TGC output to stderr or the TGC output file
	 * @param format[in] a format string, see j9tty_printf
	 * @param ...[in] arguments to be formatted
	 */
	void printf(const char *format, ...);
	
	/**
	 * Set the output file for TGC output.
	 * @param filename[in] the name of the file to open
	 * @return true on success, false on failure
	 */
	bool setOutputFile(const char* filename);
	
	/**
	 * Allocate and initialize a new TGC extensions object
	 * @param extensions[in] the GC extensions
	 * @return an initialized instance, or NULL on failure
	 */
	static MM_TgcExtensions * newInstance(MM_GCExtensions *extensions);
	
	/**
	 * Tear down and free the TGC extensions object
 	 * @param extensions[in] the GC extensions
	 */
	void kill(MM_GCExtensions *extensions);
	
	/**
	 * Get the TGC extensions object from the GC Extensions object
	 * @param extensions[in] the GC extensions object
	 * @return the TGC extensions
	 */
	static MM_TgcExtensions* getExtensions(MM_GCExtensions* extensions) { return (MM_TgcExtensions*)extensions->tgcExtensions; }

	/**
	 * Get the TGC extensions object from the J9JavaVM struct
	 * @param javaVM[in] the J9JavaVM struct
	 * @return the TGC extensions
	 */
	static MM_TgcExtensions* getExtensions(J9JavaVM* javaVM) { return getExtensions(MM_GCExtensions::getExtensions(javaVM)); }

	/**
	 * Get the TGC extensions object from the J9JavaVM struct
	 * @param javaVM[in] the J9JavaVM struct
	 * @return the TGC extensions
	 */
	static MM_TgcExtensions* getExtensions(OMR_VMThread* omrVMThread) { return getExtensions(MM_GCExtensions::getExtensions(omrVMThread)); }

	/**
	 * Get the TGC extensions object from a J9VMThread struct
	 * @param vmThread[in] a J9VMThread struct
	 * @return the TGC extensions
	 */
	static MM_TgcExtensions* getExtensions(J9VMThread* vmThread) { return getExtensions(vmThread->javaVM); }
};

#endif /* TGCEXTENSIONS_HPP_ */

