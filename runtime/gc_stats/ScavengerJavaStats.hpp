
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

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(SCAVENGERSTATSJAVA_HPP_)
#define SCAVENGERSTATSJAVA_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "j9consts.h"
#include "modron.h"
#include "modronopt.h"

#include "Base.hpp"
#include "ReferenceStats.hpp"

/**
 * Storage for statistics relevant to a scavenging (semi-space copying) collector.
 * @ingroup GC_Stats
 */
class MM_ScavengerJavaStats
{
public:

	uintptr_t _unfinalizedCandidates;  /**< unfinalized objects that are candidates to be finalized visited this cycle */
	uintptr_t _unfinalizedEnqueued;  /**< unfinalized objects that are enqueued during this cycle (MUST be less than or equal _unfinalizedCandidates) */

	uintptr_t _ownableSynchronizerCandidates;  /**< number of ownable synchronizer objects visited this cycle */
	uintptr_t _ownableSynchronizerTotalSurvived;	/**< number of ownable synchronizer objects survived this cycle */
	uintptr_t _ownableSynchronizerNurserySurvived; /**< number of ownable synchronizer objects survived this cycle in Nursery Space */

	uintptr_t _continuationCandidates;  /**< number of continuation objects visited this cycle */
	uintptr_t _continuationCleared;	/**< number of continuation objects survived this cycle */

	MM_ReferenceStats _weakReferenceStats;  /**< Weak reference stats for the cycle */
	MM_ReferenceStats _softReferenceStats;  /**< Soft reference stats for the cycle */
	MM_ReferenceStats _phantomReferenceStats;  /**< Phantom reference stats for the cycle */

	uintptr_t _monitorReferenceCleared; /**< The number of monitor references that have been cleared during scavenge */
	uintptr_t _monitorReferenceCandidates; /**< The number of monitor references that have been visited in monitor table during scavenge */

protected:

private:

public:

	void clear();
	/* clear only OwnableSynchronizerObject related data */
	void clearOwnableSynchronizerCounts();
	/* merge only OwnableSynchronizerObject related data */
	void mergeOwnableSynchronizerCounts(MM_ScavengerJavaStats *statsToMerge);
	
	MMINLINE void 
	updateOwnableSynchronizerNurseryCounts(uintptr_t survivedCount)
	{
		_ownableSynchronizerNurserySurvived += survivedCount;
	}
	/* clear only ContinuationObject related data */
	void clearContinuationCounts();
	/* merge only ContinuationObject related data */
	void mergeContinuationCounts(MM_ScavengerJavaStats *statsToMerge);

	MM_ScavengerJavaStats();


};

#endif /* SCAVENGERSTATSJAVA_HPP_ */
