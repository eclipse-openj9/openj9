
/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
 * @ingroup GC_Modron_Standard
 */

#if !defined(GLOBALMARKCARDSCRUBBER_HPP_)
#define GLOBALMARKCARDSCRUBBER_HPP_


#include "j9.h"
#include "j9cfg.h"
#include "j9modron.h"

#include "CardCleaner.hpp"
#include "ParallelTask.hpp"

class MM_CycleState;
class MM_EnvironmentVLHGC;
class MM_HeapMap;
class MM_InterRegionRememberedSet;

/**
 * This is the card scrubber. It is distinct from the GlobalMarkCardCleaner in that it does not produce any new work.
 * It simple examines cards to determine if they must be processed by the GlobalMarkCardCleaner. If they can be ignored
 * it changes the card state, otherwise it leaves the card unchanged.
 * The scrubber honours shouldYieldFromTask and calls it according to the _yieldCheckFrequency to avoid the expense of
 * checking the time.
 */
class MM_GlobalMarkCardScrubber : public MM_CardCleaner 
{
	/* Data Members */
private:
	MM_HeapMap * const _markMap; /**< The mark map to use when looking up live objects in dirty cards */
	MM_InterRegionRememberedSet * const _interRegionRememberedSet; /** A cached pointer to the remembered set */
	const UDATA _yieldCheckFrequency; /**< The number of references to scan before checking to yield */
	UDATA _countBeforeYieldCheck; /**< The number of references which will be checked before the next check for yielding */
	struct {
		UDATA _dirtyCards; /**< A count of all the DIRTY cards examined by this scrubber instance */
		UDATA _gmpMustScanCards; /**< A count of all the GMP_MUST_SCAN cards examined by this scrubber instance */
		UDATA _scrubbedCards; /**< A count of the cards successfully scrubbed (changed state) by this scrubber instance */
		UDATA _scrubbedObjects; /**<  A count of the objects in cards scrubbed by this scrubber instance */
	} _statistics;
protected:
public:

	/* Member Functions */
private:
	/**
	 * Scan the specified object to determine if it needs to be examined during card cleaning.
	 * @param env[in] the current thread
	 * @param objectPtr[in] the object to scan. Must be a non-NULL object on the heap.
	 */
	bool scrubObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr);

	/**
	 * Scrub a SCAN_REFERENCE_MIXED_OBJECT.
	 * @param env[in] the current thread
	 * @param objectPtr[in] the object to scan. Must be a non-NULL object on the heap.
	 */
	bool scrubMixedObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr);

	/**
	 * Scrub a SCAN_POINTER_ARRAY_OBJECT.
	 * @param env[in] the current thread
	 * @param objectPtr[in] the object to scan. Must be a non-NULL object array on the heap.
	 */
	bool scrubPointerArrayObject(MM_EnvironmentVLHGC *env, J9Object *objectPtr);

	/**
	 * Scrub a SCAN_CLASS_OBJECT.
	 * @param env[in] the current thread
	 * @param objectPtr[in] the object to scan. Must be a non-NULL class object on the heap.
	 */
	bool scrubClassObject(MM_EnvironmentVLHGC *env, J9Object *classObject);
	
	/**
	 * Scrub a SCAN_CLASSLOADER_OBJECT.
	 * @param env[in] the current thread
	 * @param objectPtr[in] the object to scan. Must be a non-NULL class loader object on the heap.
	 */
	bool scrubClassLoaderObject(MM_EnvironmentVLHGC *env, J9Object *classLoaderObject);

	/**
	 * Given the specified reference from fromObject to toObject, determine if the card can
	 * be scrubbed (marked clean).
	 * @param env[in] the current thread
	 * @param fromObject[in] the object being scanned
	 * @param toObject[in] an object referenced from fromObject
	 * @return true if the card may be scrubbed, false if cleaning is required
	 */
	bool mayScrubReference(MM_EnvironmentVLHGC *env, J9Object *fromObject, J9Object* toObject);
	
	/**
	 * Scans the marked objects in the [lowAddress..highAddress) range to determine if these objects need to be scanned during
	 * card cleaning. The receiver uses its _markMap to determine which objects in the range are marked.
	 * 
	 * @param env[in] A GC thread (note that this method could be called by multiple threads, in parallel, but on disjoint address ranges)
	 * @param lowAddress[in] The heap address where the receiver will begin walking objects
	 * @param highAddress[in] The heap address after the last address we will potentially find a live object
	 * @return true if the objects are all clean, or false if card cleaning is required
	 */
	bool scrubObjectsInRange(MM_EnvironmentVLHGC *env, void *lowAddress, void *highAddress);
	
protected:
	/**
	 * Clean a range of addresses (typically within a span of a card).
	 * This class is used for updating the RSM based on the previous mark map and scanning modified objects in the next mark map
	 * (note that next is always a subset of previous so anything not marked in next but marked in previous only requires RSM
	 * inter-region reference updating while objects marked in both require RSM updates and object scanning to update the next
	 * mark map).
	 *
	 * @param[in] env A thread (typically the thread initializing the GC)
	 * @param[in] lowAddress low address of the range to be cleaned
	 * @param[in] highAddress high address of the range to be cleaned 
	 * @param cardToClean[in/out] The card which we are cleaning
	 */		
	virtual void clean(MM_EnvironmentBase *env, void *lowAddress, void *highAddress, Card *cardToClean);

	/**
	 * @see MM_CardCleaner::getVMStateID()
	 */
	virtual UDATA getVMStateID() { return OMRVMSTATE_GC_GLOBAL_MARK_CARD_SCRUBBER; }

public:	
	
	/**
	 * Create a CardScrubber instance.
	 * @param env[in] current thread
	 * @param map[in] the next mark map
	 * @param yieldFrequency how many references to scan before checking the interval manager 
	 */
	MM_GlobalMarkCardScrubber(MM_EnvironmentVLHGC *env, MM_HeapMap *map, UDATA yieldCheckFrequency);
	
	/**
	 * Returns the number of DIRTY cards examined by this scrubber instance.
	 * @return the number of cards examined
	 */
	UDATA getDirtyCards() { return _statistics._dirtyCards; }

	/**
	 * Returns the number of GMP_MUST_SCAN cards examined by this scrubber instance.
	 * @return the number of cards examined
	 */
	UDATA getGMPMustScanCards() { return _statistics._gmpMustScanCards; }

	/**
	 * Returns the number of cards scrubbed by this scrubber instance.
	 * This is the number of cards which were modified so that the cleaner can skip them.
	 * @return the number of cards scrubbed
	 */
	UDATA getScrubbedCards() { return _statistics._scrubbedCards; }
	
	/**
	 * Returns the number of objects found in cards scrubbed by this scrubber instance.
	 * @return the number of objects in scrubbed cards
	 */
	UDATA getScrubbedObjects() { return _statistics._scrubbedObjects; }

};

class MM_ParallelScrubCardTableTask : public MM_ParallelTask
{
private:
	MM_CycleState * const _cycleState; /**< Current cycle state information */
	bool _timeLimitWasHit;	/**< true if any requests to check the time came in after we had hit our time threshold */
	const I_64 _timeThreshold;	/**< The millisecond which represents the end of this increment */
	
public:

	virtual UDATA getVMStateID() { return OMRVMSTATE_GC_SCRUB_CARD_TABLE; };
	
	virtual void run(MM_EnvironmentBase *env);
	virtual void setup(MM_EnvironmentBase *env);
	virtual void cleanup(MM_EnvironmentBase *env);

	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id);

	virtual bool shouldYieldFromTask(MM_EnvironmentBase *env);

	/**
	 * @return True if this task hit its timeout limit (does not check if NOW is after the time limit, though)
	 */
	MMINLINE bool didTimeout() const { return _timeLimitWasHit; }

	/**
	 * Create a ParallelMarkTask object.
	 */
	MM_ParallelScrubCardTableTask(MM_EnvironmentBase *env,
			MM_Dispatcher *dispatcher, 
			I_64 timeThreshold,
			MM_CycleState *cycleState) :
		MM_ParallelTask(env, dispatcher)
		,_cycleState(cycleState)
		,_timeLimitWasHit(false)
		,_timeThreshold(timeThreshold)
	{
		_typeId = __FUNCTION__;
	};
};


#endif /* GLOBALMARKCARDSCRUBBER_HPP_ */
