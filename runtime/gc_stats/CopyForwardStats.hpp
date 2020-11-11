
/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
#include "JavaStats.hpp"

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
	/* The below stats include both marked and copied cases */
	MM_JavaStats _javaStats;
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
	UDATA _doubleMappedArrayletsCleared; /**< The number of double mapped arraylets that have been cleared durign marking */
	UDATA _doubleMappedArrayletsCandidates; /**< The number of double mapped arraylets that have been visited during marking */
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */

private:
	
	/* 
	 * Function members 
	 */
public:

	MMINLINE void clear() {

		MM_CopyForwardStatsCore::clear();
		_javaStats.clear();

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
		_doubleMappedArrayletsCleared = 0;
		_doubleMappedArrayletsCandidates = 0;
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */
	}
	
	/**
	 * Merge the given stat structures values into the receiver.
	 * @note This method is NOT thread safe.
	 */
	void merge(MM_CopyForwardStats *stats) {
		MM_CopyForwardStatsCore::merge(stats);
		_javaStats.merge(&stats->_javaStats);

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
		_doubleMappedArrayletsCleared += stats->_doubleMappedArrayletsCleared;
		_doubleMappedArrayletsCandidates += stats->_doubleMappedArrayletsCandidates;
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */
	}

	MM_CopyForwardStats() :
		MM_CopyForwardStatsCore()
		, _javaStats()
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
		, _doubleMappedArrayletsCleared(0)
		, _doubleMappedArrayletsCandidates(0)
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */
	{}
};

#endif /* J9VM_GC_VLHGC */
#endif /* COPYFORWARDSTATS_HPP_ */
