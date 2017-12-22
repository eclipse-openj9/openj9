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
 * @ingroup gc_vlhgc
 */

#if !defined(CLASSLOADERREMEMBEREDSET_HPP)
#define CLASSLOADERREMEMBEREDSET_HPP

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"
#include "pool_api.h"
#include "ModronAssertions.h"

#include "BaseVirtual.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "GCExtensions.hpp"

class MM_EnvironmentBase;
class MM_HeapRegionDescriptor;
class MM_HeapRegionManager;
struct J9Pool;

class MM_ClassLoaderRememberedSet : public MM_BaseVirtual
{
private:
	MM_GCExtensions * const _extensions; /**< cached pointer to the extensions structure */
	MM_HeapRegionManager * const _regionManager; /** cached pointer to the region manager */
	UDATA const _bitVectorSize; /**< The size of each bit vector in UDATAs */
	J9Pool *_bitVectorPool; /**< A pool of bit vectors to associate with J9ClassLoaders */
	MM_LightweightNonReentrantLock _lock; /**< A lock to protect _bitVectorPool */
	UDATA * _bitsToClear; /**< A bit vector which is built up before a garbage collection */
	
private:
	MM_ClassLoaderRememberedSet(MM_EnvironmentBase* env);
	
	/**
	 * Allocation helper for J9Pool.
	 */
	static void * poolAllocateHelper(void* userData, U_32 size, const char* callSite, U_32 memoryCategory, U_32 type, U_32* doInit);

	/**
	 * Deallocation helper for J9Pool.
	 */
	static void poolFreeHelper(void* userData, void* address, U_32 type);
	
	/**
	 * Upgrade the specified class loader from a single remembered region to a full bit vector.
	 * If a bit vector can't be installed, set the loader to overflowed.
	 * @param env[in] the current thread
	 * @param gcRememberedSetAddress[in/out] an address of gcRememberedSet slot
	 */
	void installBitVector(MM_EnvironmentBase* env, volatile UDATA *gcRememberedSetAddress);
	
	/**
	 * Atomically set the specified bit in the specified bit vector.
	 * @param env[in] the current thread
	 * @param bitVector[in] the bitVector to modify
	 * @param bit the index of the bit to set
	 */
	void setBit(MM_EnvironmentBase* env, volatile UDATA* bitVector, UDATA bit);
	
	/**
	 * Determine if the specified gcRememberedSet field value represents an overflowed set.
	 * @param rememberedSet the value of a J9ClassLoader's gcRememberedSet field
	 * @return true if the remembered set is overflowed
	 */
	MMINLINE bool isOverflowedRemememberedSet(UDATA rememberedSet) { return UDATA_MAX == rememberedSet; } 

	/**
	 * Determine if the specified gcRememberedSet field value represents a tagged region index.
	 * Note that overflowed class loaders will return true!
	 * @param rememberedSet the value of a J9ClassLoader's gcRememberedSet field
	 * @return true if the remembered set is a tagged region index (or overflowed), false otherwise
	 */
	MMINLINE bool isTaggedRegionIndex(UDATA rememberedSet) { return 1 == (1 & rememberedSet); }
	
	/**
	 * Convert the specified region index into a tagged value suitable to be stored in the gcRememberedSet field.
	 * @param regionIndex the index to store
	 * @return the tagged version of the index
	 */
	MMINLINE UDATA asTaggedRegionIndex(UDATA regionIndex) { return (regionIndex << 1) | 1; }
	
	/**
	 * Convert a tagged region index back to the actual value
	 * @param rememberedSet the value of a J9ClassLoader's gcRememberedSet field
	 * @return the region index it represents
	 */
	MMINLINE UDATA asUntaggedRegionIndex(UDATA rememberedSet) { return rememberedSet >> 1; }
	
	/**
	 * Determine if the specified bit is set in the specified bit vector.
	 * @param env[in] the current thread
	 * @param bitVector[in] the bit vector to test
	 * @param bit the index of the bit to test
	 * @return true if the bit is set, false otherwise 
	 */
	bool isBitSet(MM_EnvironmentBase* env, volatile UDATA* bitVector, UDATA bit);
	
	/**
	 * Implementation helper for rememberInstance() based on region index.
	 *
	 * @param env[in] the current thread
	 * @param regionIndex[in] the region index to remember
	 * @param gcRememberedSetAddress[in/out] an address of gcRememberedSet slot
	 */
	void rememberRegionInternal(MM_EnvironmentBase* env, UDATA regionIndex, volatile UDATA *gcRememberedSetAddress);

	/**
	 * Implementation helper for isRemembered().
	 *
	 * @param env[in] the current thread
	 * @param gcRememberedSet[in] gcRememberedSet slot
	 */
	bool isRememberedInternal(MM_EnvironmentBase *env, UDATA gcRememberedSet);

	/**
	 * Implementation helper for isRemembered() based on region index.
	 *
	 * @param env[in] the current thread
	 * @param regionIndex[in] the region index to remember
	 * @param gcRememberedSet[in] gcRememberedSet slot
	 */
	bool isRegionRemembered(MM_EnvironmentBase *env, UDATA regionIndex, UDATA gcRememberedSet);

	/**
	 * Implementation helper for killRememberedSet().
	 *
	 * @param env[in] the current thread
	 * @param gcRememberedSet[in] gcRememberedSet slot
	 */
	void killRememberedSetInternal(MM_EnvironmentBase *env, UDATA gcRememberedSet);

	/**
	 * Implementation helper for clearRememberedSets().
	 *
	 * @param env[in] the current thread
	 * @param gcRememberedSetAddress[in/out] an address of gcRememberedSet slot
	 */
	void clearRememberedSetsInternal(MM_EnvironmentBase *env, volatile UDATA *gcRememberedSetAddress);

protected:
public:
	static MM_ClassLoaderRememberedSet *newInstance(MM_EnvironmentBase* env);
	virtual void kill(MM_EnvironmentBase *env);

	/*
	 * Initialize ClassLoaderRememberedSet
	 */
	bool initialize(MM_EnvironmentBase* env);

	/*
	 * Teardown ClassLoaderRememberedSet
	 */	
	virtual void tearDown(MM_EnvironmentBase* env);

	/**
	 * Update the remembered set data for the specified object's class loader to record the object.
	 * 
	 * The object's region and class loader are identified, and the relationship between them is recorded in the class loader. 
	 * 
	 * @param env[in] the current thread
	 * @param object[in] the object to remember
	 */
	void rememberInstance(MM_EnvironmentBase* env, J9Object* object);

	/**
	 * Determine if there are any instances of classes defined by specified class loader.
	 * @param env[in] the current thread
	 * @param classLoader[in] the loader to examine
	 * @return true if there are instances (or the loader is overflowed), false otherwise  
	 */
	bool isRemembered(MM_EnvironmentBase *env, J9ClassLoader *classLoader);

	/**
	 * Determine if there are instances of anonymous class defined.
	 * @param env[in] the current thread
	 * @param clazz[in] the anonymous class to examine
	 * @return true if there are instances, false otherwise
	 */
	bool isClassRemembered(MM_EnvironmentBase *env, J9Class *clazz);

	/**
	 * Determine if the specified object is remembered in its class loader.
	 * @note This is intended primarily for verification 
	 * @param env[in] the current thread
	 * @param object[in] the object to test
	 * @return true if the object is properly remembered  
	 */
	bool isInstanceRemembered(MM_EnvironmentBase *env, J9Object* object);
	
	/**
	 * Delete any resources associated with the remembered set for the specified class loader.
	 * @param env[in] the current thread
	 * @param classLoader[in] the loader to modify
	 */
	void killRememberedSet(MM_EnvironmentBase *env, J9ClassLoader *classLoader);
	
	/**
	 * Called before a partial GC to initialize the preserved regions bit vector.
	 * All bits are 1 (preserved) once this call completes.
	 * See also #prepareToClearRememberedSetForRegion() and #clearRememberedSets()
	 * @param env[in] the current thread
	 */
	void resetRegionsToClear(MM_EnvironmentBase *env);
	
	/**
	 * Ensure that the remembered bits for the specified region will be cleared when #clearRememberedSets() is called.
	 * This may safely be called from multiple threads.
	 * @param env[in] the current thread
	 * @param region[in] a region which is part of the collection set and should be cleared
	 */
	void prepareToClearRememberedSetForRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region);
	
	/**
	 * Called before a partial GC to clear remembered sets in all class loaders for regions 
	 * which have been marked using #prepareToClearRememberedSetForRegion()
	 * @param env[in] the current thread
	 */
	void clearRememberedSets(MM_EnvironmentBase *env);
	
	/**
 	 * Called by the master thread before a GC cycle to perform any setup required before a collection.
 	 * @param env[in] the current thread
 	 */
	void setupBeforeGC(MM_EnvironmentBase *env); 
};

#endif /* CLASSLOADERREMEMBEREDSET_HPP */
