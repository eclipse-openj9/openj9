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
 * @ingroup GC_Modron_Tarok
 */

#if !defined(COPYFORWARDDELEGATE_HPP_)
#define COPYFORWARDDELEGATE_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "CopyForwardScheme.hpp"

class MM_EnvironmentVLHGC;
class MM_GCExtensions;
class MM_HeapRegionManager;

class MM_CopyForwardDelegate : public MM_BaseNonVirtual
{
	/* Data Members */
private:
	J9JavaVM *_javaVM;  /**< Cached JVM reference */
	MM_GCExtensions *_extensions;  /**< Cached GC global variable container */
	MM_CopyForwardScheme *_breadthFirstCopyForwardScheme;  /**< Underlying mechanics for breadth-first copyForward garbage collection */

protected:
public:

	/* Member Functions */
private:
	/**
	 * Updates the contents of _extensions->compactGroupPersistentStats before a PGC copy-forward operation.
	 * @param env[in] The master GC thread
	 */
	void updatePersistentStatsBeforeCopyForward(MM_EnvironmentVLHGC *env);

	/**
	 * Updates the contents of _extensions->compactGroupPersistentStats before a PGC copy-forward operation.
	 * @param env[in] The master GC thread
	 */
	void updatePersistentStatsAfterCopyForward(MM_EnvironmentVLHGC *env);

protected:
public:
	MM_CopyForwardDelegate(MM_EnvironmentVLHGC *env);

	/**
	 * Initialize the receivers internal support structures and state.
	 * @param env[in] Main thread.
	 */
	bool initialize(MM_EnvironmentVLHGC *env);

	/**
	 * Clean up the receivers internal support structures and state.
	 * @param env[in] Main thread.
	 */
	void tearDown(MM_EnvironmentVLHGC *env);

	/**
	 * Runs a PGC collect using the copy forward mechanism. This call does report events, before and after the collection, but does not collect statistics.
	 * @param env[in] The master GC thread
	 * @param allocDescription[in] The allocation request which triggered the collect
	 * @return flag indicating if the copy forward collection was successful or not.
	 */
	bool performCopyForwardForPartialGC(MM_EnvironmentVLHGC *env);

	/**
	 * Infrastructure and state setup pre-copyForward.
	 * @param env[in] Master GC thread.
	 */
	void preCopyForwardSetup(MM_EnvironmentVLHGC *env);

	/**
	 * Infrastructure and state cleanup post-copyForward.
	 * @param env[in] Master GC thread.
	 */
	void postCopyForwardCleanup(MM_EnvironmentVLHGC *env);
	
	/**
	 * Estimate the number of bytes required for survivor space given the set of _shouldMark regions and their historical survival rates.
	 * @param env[in] the current thread
	 * @return the estimated number of bytes which will survive 
	 */
	UDATA estimateRequiredSurvivorBytes(MM_EnvironmentVLHGC *env);

	/**
	 * Return true if the copyForward is running under Hybrid mode
	 */
	bool isHybrid(MM_EnvironmentVLHGC *env)
	{
		bool ret = false;
		if (NULL != _breadthFirstCopyForwardScheme) {
			ret = _breadthFirstCopyForwardScheme->isHybrid(env);
		}
		return ret;
	}

	/**
	 * Set the number of regions, which are need to be reserved for Mark/Compact only in CollectionSet due to short of survivor regions for CopyForward
	 */
	void setReservedNonEvacuatedRegions(UDATA regionCount)
	{
		if (NULL != _breadthFirstCopyForwardScheme) {
			_breadthFirstCopyForwardScheme->setReservedNonEvacuatedRegions(regionCount);
		}
	}
};


#endif /* COPYFORWARDDELEGATE_HPP_ */
