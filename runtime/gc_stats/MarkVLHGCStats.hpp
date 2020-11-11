
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

#if !defined(MARKVLHGCSTATS_HPP_)
#define MARKVLHGCSTATS_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9port.h"
#include "j9consts.h"
#include "modron.h"
#include "modronopt.h"

#if defined(J9VM_GC_VLHGC)

#include "MarkVLHGCStatsCore.hpp"
#include "Base.hpp"
#include "AtomicOperations.hpp" 
#include "ReferenceStats.hpp"
#include "JavaStats.hpp"

/**
 * Storage for statistics relevant to the mark phase of a global collection.
 * @ingroup GC_Stats
 */
class MM_MarkVLHGCStats
{
/* data members */
private:
protected:
public:
	MM_JavaStats _javaStats;
	uintptr_t _objectsCardClean;	/**< Objects scanned through card cleaning */
	uintptr_t _bytesCardClean;	/**< Bytes scanned through card cleaning */

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
	UDATA _doubleMappedArrayletsCleared; /**< The number of double mapped arraylets that have been cleared durign marking */
	UDATA _doubleMappedArrayletsCandidates; /**< The number of double mapped arraylets that have been visited during marking */
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */	

/* function members */
private:
protected:
public:

	void clear()
	{
		_javaStats.clear();
		_objectsCardClean = 0;
		_bytesCardClean = 0;

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
		_doubleMappedArrayletsCleared = 0;
		_doubleMappedArrayletsCandidates = 0;
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */	

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		// _splitArraysProcessed = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	}

	void merge(MM_MarkVLHGCStats *statsToMerge)
	{
		_javaStats.merge(&statsToMerge->_javaStats);
		_objectsCardClean += statsToMerge->_objectsCardClean;
		_bytesCardClean += statsToMerge->_bytesCardClean;

#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
		_doubleMappedArrayletsCleared += statsToMerge->_doubleMappedArrayletsCleared;
		_doubleMappedArrayletsCandidates += statsToMerge->_doubleMappedArrayletsCandidates;
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */	


#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		/* It may not ever be useful to merge these stats, but do it anyways */
		// _splitArraysProcessed += statsToMerge->_splitArraysProcessed;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	}
	
	MM_MarkVLHGCStats() :
		_javaStats()
		,_objectsCardClean(0)
		,_bytesCardClean(0)
#if defined(J9VM_GC_ENABLE_DOUBLE_MAP)
		,_doubleMappedArrayletsCleared(0)
		,_doubleMappedArrayletsCandidates(0)
#endif /* J9VM_GC_ENABLE_DOUBLE_MAP */
	{
	}
	
}; 

#endif /* J9VM_GC_VLHGC */
#endif /* MARKVLHGCSTATS_HPP_ */
