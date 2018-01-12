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
 * @ingroup GC_Modron_VLHGC
 */

#if !defined(MARKMAPMANAGER_HPP_)
#define MARKMAPMANAGER_HPP_

#include "j9.h"
#include "j9cfg.h"

#include "BaseNonVirtual.hpp"

class MM_EnvironmentVLHGC;
class MM_GCExtensions;
class MM_MarkMap;
class MM_MemorySubSpace;

/**
 * Manage resources associated to mark maps.
 */
class MM_MarkMapManager : public MM_BaseNonVirtual
{
private:
	MM_GCExtensions * const _extensions; /**< The extensions for this JVM */
	MM_MarkMap *_nextMarkMap;	/**< The mark map waiting to be finished by a GMP */
	MM_MarkMap *_previousMarkMap;	/**< The mark map which is used by PGC */
	MM_MarkMap *_deleteEventShadowMarkMap;	/**< Used only when J9HOOK_MM_OMR_OBJECT_DELETE is hooked or reserved.  Used to represent the mark map state prior to a PGC mark or a GMP completion so that the difference in map states can be used to report deleted objects */

protected:
public:

private:
	bool initialize(MM_EnvironmentVLHGC *env);
	void tearDown(MM_EnvironmentVLHGC *env);

protected:
public:
	static MM_MarkMapManager *newInstance(MM_EnvironmentVLHGC *env);
	void kill(MM_EnvironmentVLHGC *env);

	/**
 	 * Perform any initialization required by the delegate once the VM is up and ready to start.
 	 * @return TRUE if startup completes OK, FALSE otherwise 
 	 */
	bool collectorStartup(MM_GCExtensions *extensions);

	/**
	 * Adjust internal structures to reflect the change in heap size.
	 *
	 * @param size The amount of memory added to the heap
	 * @param lowAddress The base address of the memory added to the heap
	 * @param highAddress The top address (non-inclusive) of the memory added to the heap
	 * @return true if operation completes with success
	 */
	bool heapAddRange(MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress);

	/**
	 * Adjust internal structures to reflect the change in heap size.
	 *
	 * @param size The amount of memory added to the heap
	 * @param lowAddress The base address of the memory added to the heap
	 * @param highAddress The top address (non-inclusive) of the memory added to the heap
	 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
	 * @param highValidAddress The first valid address following the highest in the heap range being removed
	 * @return true if operation completes with success
	 */
	bool heapRemoveRange(
			MM_EnvironmentVLHGC *env, MM_MemorySubSpace *subspace, UDATA size, void *lowAddress, void *highAddress,
		void *lowValidAddress, void *highValidAddress);

	/**
	 * Return the mark map used for the global mark phase.
	 * @return an MM_MarkMap
	 */
	MM_MarkMap *getGlobalMarkPhaseMap() {
		return _nextMarkMap;
	}

	/**
	 * Return the mark map used for the partial garbage collection phase.
	 * @return an MM_MarkMap
	 */
	MM_MarkMap *getPartialGCMap() {
		return _previousMarkMap;
	}
	
	/**
	 * Swaps the _inProgressMarkMap and the _completeMarkMap.  Called at the end of a GMP when it has completed a new map.
	 */
	void swapMarkMaps();

	/**
	 * Over-writes the contents of _deleteEventShadowMarkMap with that of _previousMarkMap and then
	 * returns a pointer to _deleteEventShadowMarkMap.
	 * @note can only be called if J9HOOK_MM_OMR_OBJECT_DELETE was reserved or hooked at startup (asserts otherwise)
	 * @param env[in] The master GC thread
	 * @return _deleteEventShadowMarkMap
	 */
	MM_MarkMap *savePreviousMarkMapForDeleteEvents(MM_EnvironmentVLHGC *env);

	/**
	 * Checks all regions in the heap for differences between oldMap and newMap.  If a region has a valid mark map in both
	 * versions, objects which were marked in oldMap but not in newMap are reported.  If the region only has a valid mark
	 * map in newMap, the region is walked address-ordered and objects which are not marked in newMap are reported.
	 * @note can only be called if J9HOOK_MM_OMR_OBJECT_DELETE was reserved or hooked at startup (asserts otherwise)
	 * @param env[in] The master GC thread
	 * @param oldMap[in] The mark map prior to a mark operation which could have killed objects
	 * @param newMap[in] The mark map following a mark operation which could have killed objects
	 */
	void reportDeletedObjects(MM_EnvironmentVLHGC *env, MM_MarkMap *oldMap, MM_MarkMap *newMap);

	/**
	 * Called when _nextMarkMap has been completed but before it is swapped with _previousMarkMap.  Ensures that _nextMarkMap
	 * is a subset of _previousMarkMap.  Asserts on failure.
	 * Note that this must be called before _nextMarkMap is used to move objects on the heap since there is no way to fixup
	 * _previousMarkMap such that this method will pass, once that has been done (since it refers to objects which are dead
	 * in the map used for the object movement)
	 * @param env[in] The master GC thread
	 */
	void verifyNextMarkMapSubsetOfPrevious(MM_EnvironmentVLHGC *env);

	MM_MarkMapManager(MM_EnvironmentVLHGC *env);
};

#endif /* MARKMAPMANAGER_HPP_ */
