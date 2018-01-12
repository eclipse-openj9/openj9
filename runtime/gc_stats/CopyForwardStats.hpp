
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

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(COPYFORWARDSTATS_HPP_)
#define COPYFORWARDSTATS_HPP_

#include "CopyForwardStatsCore.hpp"
#include "j9cfg.h"
#include "j9comp.h"
#include "j9port.h"
#include "modronbase.h"
#include "modronopt.h"

#if defined(J9VM_GC_VLHGC)

#include "ReferenceStats.hpp"

/**
 * Storage for statistics relevant to a copy forward collector.
 * @ingroup GC_Stats
 */
class MM_CopyForwardStats : public MM_CopyForwardStatsCore
{
	/* 
	 * Data members 
	 */
public:

	UDATA _unfinalizedCandidates;  /**< unfinalized objects that are candidates to be finalized visited this cycle */
	UDATA _unfinalizedEnqueued;  /**< unfinalized objects that are enqueued during this cycle (MUST be less than or equal _unfinalizedCandidates) */

	UDATA _ownableSynchronizerCandidates;  /**< number of ownable synchronizer objects visited this cycle */
	UDATA _ownableSynchronizerSurvived;	/**< number of ownable synchronizer objects survived this cycle */

	MM_ReferenceStats _weakReferenceStats;  /**< Weak reference stats for the cycle */
	MM_ReferenceStats _softReferenceStats;  /**< Soft reference stats for the cycle */
	MM_ReferenceStats _phantomReferenceStats;  /**< Phantom reference stats for the cycle */

	UDATA _stringConstantsCleared;  /**< The number of string constants that have been cleared during marking */
	UDATA _stringConstantsCandidates; /**< The number of string constants that have been visited in string table during marking */

private:
	
	/* 
	 * Function members 
	 */
public:

	MMINLINE void clear() {

		MM_CopyForwardStatsCore::clear();

		_unfinalizedCandidates = 0;
		_unfinalizedEnqueued = 0;

		_ownableSynchronizerCandidates = 0;
		_ownableSynchronizerSurvived = 0;

		_weakReferenceStats.clear();
		_softReferenceStats.clear();
		_phantomReferenceStats.clear();

		_stringConstantsCleared = 0;
		_stringConstantsCandidates = 0;
	}
	
	/**
	 * Merge the given stat structures values into the receiver.
	 * @note This method is NOT thread safe.
	 */
	void merge(MM_CopyForwardStats *stats) {
		MM_CopyForwardStatsCore::merge(stats);
		_unfinalizedCandidates += stats->_unfinalizedCandidates;
		_unfinalizedEnqueued += stats->_unfinalizedEnqueued;

		_ownableSynchronizerSurvived += stats->_ownableSynchronizerSurvived;

		_weakReferenceStats.merge(&stats->_weakReferenceStats);
		_softReferenceStats.merge(&stats->_softReferenceStats);
		_phantomReferenceStats.merge(&stats->_phantomReferenceStats);

		_stringConstantsCleared += stats->_stringConstantsCleared;
		_stringConstantsCandidates += stats->_stringConstantsCandidates;
	}

	MM_CopyForwardStats() :
		MM_CopyForwardStatsCore()
		,_unfinalizedCandidates(0)
		,_unfinalizedEnqueued(0)
		,_ownableSynchronizerCandidates(0)
		,_ownableSynchronizerSurvived(0)
		,_weakReferenceStats()
		,_softReferenceStats()
		,_phantomReferenceStats()
		,_stringConstantsCleared(0)
		,_stringConstantsCandidates(0)
	{}
};

#endif /* J9VM_GC_VLHGC */
#endif /* COPYFORWARDSTATS_HPP_ */
