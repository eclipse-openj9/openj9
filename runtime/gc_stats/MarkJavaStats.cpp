
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

#include "j9port.h"
#include "modronopt.h"

#include "MarkJavaStats.hpp"

void
MM_MarkJavaStats::clear()
{
	_unfinalizedCandidates = 0;
	_unfinalizedEnqueued = 0;

	_ownableSynchronizerCandidates = 0;
	_ownableSynchronizerCleared = 0;

	_weakReferenceStats.clear();
	_softReferenceStats.clear();
	_phantomReferenceStats.clear();

	_stringConstantsCleared = 0;
	_stringConstantsCandidates = 0;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	splitArraysProcessed = 0;
	splitArraysAmount = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
};

void
MM_MarkJavaStats::merge(MM_MarkJavaStats* statsToMerge)
{
	_unfinalizedCandidates += statsToMerge->_unfinalizedCandidates;
	_unfinalizedEnqueued += statsToMerge->_unfinalizedEnqueued;

	_ownableSynchronizerCandidates += statsToMerge->_ownableSynchronizerCandidates;
	_ownableSynchronizerCleared += statsToMerge->_ownableSynchronizerCleared;

	_weakReferenceStats.merge(&statsToMerge->_weakReferenceStats);
	_softReferenceStats.merge(&statsToMerge->_softReferenceStats);
	_phantomReferenceStats.merge(&statsToMerge->_phantomReferenceStats);

	_stringConstantsCleared += statsToMerge->_stringConstantsCleared;
	_stringConstantsCandidates += statsToMerge->_stringConstantsCandidates;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	/* It may not ever be useful to merge these stats, but do it anyways */
	splitArraysProcessed += statsToMerge->splitArraysProcessed;
	splitArraysAmount += statsToMerge->splitArraysAmount;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
};
