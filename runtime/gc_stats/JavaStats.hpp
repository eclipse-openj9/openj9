
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef JAVASTATS_HPP_
#define JAVASTATS_HPP_

#include "j9port.h"
#include "modronopt.h"

#include "Base.hpp"
#include "ReferenceStats.hpp"

class MM_JavaStats : public MM_Base {
	/* data members */
private:
protected:
public:
    UDATA _unfinalizedCandidates; /**< unfinalized objects that are candidates to be finalized visited this cycle */
    UDATA _unfinalizedEnqueued; /**< unfinalized objects that are enqueued during this cycle (MUST be less than or equal _unfinalizedCandidates) */

    UDATA _ownableSynchronizerCandidates; /**< number of ownable synchronizer objects visited this cycle */

    MM_ReferenceStats _weakReferenceStats; /**< Weak reference stats for the cycle */
	MM_ReferenceStats _softReferenceStats; /**< Soft reference stats for the cycle */
	MM_ReferenceStats _phantomReferenceStats; /**< Phantom reference stats for the cycle */

    UDATA _monitorReferenceCleared; /**< The number of monitor references that have been cleared during marking */
	UDATA _monitorReferenceCandidates; /**< The number of monitor references that have been visited in monitor table during marking */
	/* function members */
private:
protected:
public:
    virtual void clear()
    {
        _unfinalizedCandidates = 0;
        _unfinalizedEnqueued = 0;

        _ownableSynchronizerCandidates = 0;

        _weakReferenceStats.clear();
        _softReferenceStats.clear();
        _phantomReferenceStats.clear();

        _monitorReferenceCleared = 0;
        _monitorReferenceCandidates = 0;
    }

    virtual void merge(MM_JavaStats *statsToMerge)
    {
        _unfinalizedCandidates += statsToMerge->_unfinalizedCandidates;
        _unfinalizedEnqueued += statsToMerge->_unfinalizedEnqueued;

        _ownableSynchronizerCandidates += statsToMerge->_ownableSynchronizerCandidates;

        _weakReferenceStats.merge(&statsToMerge->_weakReferenceStats);
		_softReferenceStats.merge(&statsToMerge->_softReferenceStats);
		_phantomReferenceStats.merge(&statsToMerge->_phantomReferenceStats);
        
        _monitorReferenceCleared += statsToMerge->_monitorReferenceCleared;
        _monitorReferenceCandidates += statsToMerge->_monitorReferenceCandidates;
    }

	MM_JavaStats() :
		MM_Base()
        , _unfinalizedCandidates(0)
        , _unfinalizedEnqueued(0)
        , _ownableSynchronizerCandidates(0)
        , _weakReferenceStats()
        , _softReferenceStats()
		, _phantomReferenceStats()
        , _monitorReferenceCleared(0)
		, _monitorReferenceCandidates(0)
	{
        clear();
	}
};

#endif /* JAVASTATS_HPP_ */
