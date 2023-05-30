
/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#if !defined(MARKJAVASTATS_HPP_)
#define MARKJAVASTATS_HPP_

#include "j9port.h"
#include "modronopt.h"

#include "Base.hpp"
#include "AtomicOperations.hpp"
#include "ReferenceStats.hpp"

/**
 * Storage for statistics relevant to the mark phase of a global collection.
 * @ingroup GC_Stats
 */
class MM_MarkJavaStats : public MM_Base {
	/* data members */
private:
protected:
public:
	uintptr_t _unfinalizedCandidates; /**< unfinalized objects that are candidates to be finalized visited this cycle */
	uintptr_t _unfinalizedEnqueued; /**< unfinalized objects that are enqueued during this cycle (MUST be less than or equal _unfinalizedCandidates) */

	uintptr_t _ownableSynchronizerCandidates; /**< number of ownable synchronizer objects visited this cycle */
	uintptr_t _ownableSynchronizerCleared; /**< number of ownable synchronizer objects cleared this cycle */

	uintptr_t _continuationCandidates; /**< number of continuation objects visited this cycle */
	uintptr_t _continuationCleared; /**< number of continuation objects cleared this cycle */

	MM_ReferenceStats _weakReferenceStats; /**< Weak reference stats for the cycle */
	MM_ReferenceStats _softReferenceStats; /**< Soft reference stats for the cycle */
	MM_ReferenceStats _phantomReferenceStats; /**< Phantom reference stats for the cycle */

	uintptr_t _stringConstantsCleared; /**< The number of string constants that have been cleared during marking */
	uintptr_t _stringConstantsCandidates; /**< The number of string constants that have been visited in string table during marking */

	uintptr_t _monitorReferenceCleared; /**< The number of monitor references that have been cleared during marking */
	uintptr_t _monitorReferenceCandidates; /**< The number of monitor references that have been visited in monitor table during marking */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	uintptr_t splitArraysProcessed; /**< The number of array chunks (not counting parts smaller than the split size) processed by this thread */
	uintptr_t splitArraysAmount;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	/* function members */
private:
protected:
public:
	void clear();
	void merge(MM_MarkJavaStats* statsToMerge);

	MM_MarkJavaStats() :
		MM_Base()
		, _unfinalizedCandidates(0)
		, _unfinalizedEnqueued(0)
		, _ownableSynchronizerCandidates(0)
		, _ownableSynchronizerCleared(0)
		, _continuationCandidates(0)
		, _continuationCleared(0)
		, _weakReferenceStats()
		, _softReferenceStats()
		, _phantomReferenceStats()
		, _stringConstantsCleared(0)
		, _stringConstantsCandidates(0)
		, _monitorReferenceCleared(0)
		, _monitorReferenceCandidates(0)
	{
		clear();
	}
};

#endif /* MARKJAVASTATS_HPP_ */
