
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

#include "ScavengerJavaStats.hpp"

MM_ScavengerJavaStats::MM_ScavengerJavaStats() :
	_unfinalizedCandidates(0)
	,_unfinalizedEnqueued(0)
	,_ownableSynchronizerCandidates(0)
	,_ownableSynchronizerTotalSurvived(0)
	,_ownableSynchronizerNurserySurvived(0)
	,_continuationCandidates(0)
	,_continuationCleared(0)
	,_weakReferenceStats()
	,_softReferenceStats()
	,_phantomReferenceStats()
	,_monitorReferenceCleared(0)
	,_monitorReferenceCandidates(0)
{
}

void 
MM_ScavengerJavaStats::clear()
{
	_unfinalizedCandidates = 0;
	_unfinalizedEnqueued = 0;

	_ownableSynchronizerCandidates = 0;
	_ownableSynchronizerTotalSurvived = 0;
	_ownableSynchronizerNurserySurvived = 0;

	_continuationCandidates = 0;
	_continuationCleared = 0;

	_weakReferenceStats.clear();
	_softReferenceStats.clear();
	_phantomReferenceStats.clear();

	_monitorReferenceCleared = 0;
	_monitorReferenceCandidates = 0;
};


void
MM_ScavengerJavaStats::clearOwnableSynchronizerCounts()
{
	_ownableSynchronizerCandidates = 0;
	_ownableSynchronizerTotalSurvived = 0;
	_ownableSynchronizerNurserySurvived = 0;
}

void
MM_ScavengerJavaStats::mergeOwnableSynchronizerCounts(MM_ScavengerJavaStats *statsToMerge)
{
	_ownableSynchronizerCandidates += statsToMerge->_ownableSynchronizerCandidates;
	_ownableSynchronizerTotalSurvived += statsToMerge->_ownableSynchronizerTotalSurvived;
	_ownableSynchronizerNurserySurvived += statsToMerge->_ownableSynchronizerNurserySurvived;
}

void
MM_ScavengerJavaStats::clearContinuationCounts()
{
	_continuationCandidates = 0;
	_continuationCleared = 0;
}

void
MM_ScavengerJavaStats::mergeContinuationCounts(MM_ScavengerJavaStats *statsToMerge)
{
	_continuationCandidates += statsToMerge->_continuationCandidates;
	_continuationCleared += statsToMerge->_continuationCleared;
}
