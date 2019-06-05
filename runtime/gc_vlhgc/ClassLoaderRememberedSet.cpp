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
 * @ingroup gc_vlhgc
 */

#include "ModronAssertions.h"

#include "ClassLoaderRememberedSet.hpp"

#include "AtomicOperations.hpp"
#include "ClassHeapIterator.hpp"
#include "ClassLoaderIterator.hpp"
#include "ClassLoaderSegmentIterator.hpp"
#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "GCExtensions.hpp"
#include "HeapRegionManager.hpp"

#define BITS_PER_UDATA (sizeof(UDATA) * 8)

MM_ClassLoaderRememberedSet::MM_ClassLoaderRememberedSet(MM_EnvironmentBase *env)
	: _extensions(MM_GCExtensions::getExtensions(env)) 
	, _regionManager(_extensions->heapRegionManager)
	, _bitVectorSize((_regionManager->getTableRegionCount() + BITS_PER_UDATA - 1) / BITS_PER_UDATA)
	, _bitVectorPool(NULL)
	, _bitsToClear(NULL)
{
	_typeId = __FUNCTION__;
}

MM_ClassLoaderRememberedSet*
MM_ClassLoaderRememberedSet::newInstance(MM_EnvironmentBase* env)
{
	MM_ClassLoaderRememberedSet* classLoaderRememberedSet = (MM_ClassLoaderRememberedSet*)env->getForge()->allocate(sizeof(MM_ClassLoaderRememberedSet), MM_AllocationCategory::REMEMBERED_SET, J9_GET_CALLSITE());
	if (classLoaderRememberedSet) {
		new(classLoaderRememberedSet) MM_ClassLoaderRememberedSet(env);
		if (!classLoaderRememberedSet->initialize(env)) {
			classLoaderRememberedSet->kill(env);
			classLoaderRememberedSet = NULL;
		}
	}
	return classLoaderRememberedSet;
}

bool
MM_ClassLoaderRememberedSet::initialize(MM_EnvironmentBase* env)
{
	if (!_lock.initialize(env, &_extensions->lnrlOptions, "MM_ClassLoaderRememberedSet:_lock")) {
		return false;
	}

	if (_extensions->tarokEnableIncrementalClassGC) {
		_bitVectorPool = pool_new(_bitVectorSize * sizeof(UDATA), 0, sizeof(UDATA), 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_MM, poolAllocateHelper, poolFreeHelper, this);
		if (NULL == _bitVectorPool) {
			return false;
		}
		_bitsToClear = (UDATA*)pool_newElement(_bitVectorPool);
		if (NULL == _bitsToClear) {
			return false;
		}
	} else {
		/* don't allocate a bit vector pool since we won't be consulting it for class unloading */
		_bitVectorPool = NULL;
	}
	
	return true;
}

void
MM_ClassLoaderRememberedSet::tearDown(MM_EnvironmentBase* env)
{
	if (NULL != _bitVectorPool) {
		pool_kill(_bitVectorPool);
		_bitVectorPool = NULL;
		_bitsToClear = NULL;
	}
	_lock.tearDown();
}

void
MM_ClassLoaderRememberedSet::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void * 
MM_ClassLoaderRememberedSet::poolAllocateHelper(void* userData, U_32 size, const char* callSite, U_32 memoryCategory, U_32 type, U_32* doInit)
{
	/* We ignore the memoryCategory, type and doInit arguments */
	MM_ClassLoaderRememberedSet *classLoaderRememberedSet = (MM_ClassLoaderRememberedSet*)userData;
	MM_Forge* forge = classLoaderRememberedSet->_extensions->getForge();
	return forge->allocate(size, MM_AllocationCategory::REMEMBERED_SET, callSite);
}

void 
MM_ClassLoaderRememberedSet::poolFreeHelper(void* userData, void* address, U_32 type)
{
	/* we ignore the type argument */
	MM_ClassLoaderRememberedSet *classLoaderRememberedSet = (MM_ClassLoaderRememberedSet*)userData;
	MM_Forge* forge = classLoaderRememberedSet->_extensions->getForge();
	forge->free(address);
}

void
MM_ClassLoaderRememberedSet::rememberInstance(MM_EnvironmentBase* env, J9Object* object) 
{
	Assert_MM_true(NULL != object);
	UDATA regionIndex = _regionManager->physicalTableDescriptorIndexForAddress(object);

	J9Class *clazz = J9GC_J9OBJECT_CLAZZ(object);
	Assert_MM_mustBeClass(clazz);

	if (J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassIsAnonymous)) {
		/* this is anonymous class - it should be remembered on class level */
		/* class should not be unloaded otherwise gcLink is used to form list of unloaded classes */
		Assert_MM_true(!J9_ARE_ANY_BITS_SET(clazz->classDepthAndFlags, J9AccClassDying));
		rememberRegionInternal(env, regionIndex, (volatile UDATA *)&clazz->gcLink);
	} else {
		/* non-anonymous classloader */
		J9ClassLoader *classLoader = clazz->classLoader;
		Assert_MM_true(NULL != classLoader);

		/* check for overflow first */
		if (UDATA_MAX != classLoader->gcRememberedSet) {
			rememberRegionInternal(env, regionIndex, &classLoader->gcRememberedSet);
		}
	}
}

void
MM_ClassLoaderRememberedSet::rememberRegionInternal(MM_EnvironmentBase* env, UDATA regionIndex, volatile UDATA *gcRememberedSetAddress)
{
	UDATA taggedRegionIndex = asTaggedRegionIndex(regionIndex);

	bool success = false;
	while (!success) {
		UDATA gcRememberedSet = *gcRememberedSetAddress;
		if (taggedRegionIndex == gcRememberedSet) {
			/* this region is already remembered */
			success = true;
		} else if (isOverflowedRemememberedSet(gcRememberedSet)) {
			/* this class loader is overflowed */
			success = true;
		} else if (0 == gcRememberedSet) {
			success = (0 == MM_AtomicOperations::lockCompareExchange(gcRememberedSetAddress, 0, taggedRegionIndex));
		} else if (isTaggedRegionIndex(gcRememberedSet)) {
			/* another region is remembered -- inflate the remembered set to a bit vector */
			installBitVector(env, gcRememberedSetAddress);
		} else {
			/* this remembered set must be an inflated bit vector */
			setBit(env, (volatile UDATA*)gcRememberedSet, regionIndex);
			success = true;
		}
	}
}

void
MM_ClassLoaderRememberedSet::installBitVector(MM_EnvironmentBase* env, volatile UDATA *gcRememberedSetAddress)
{
	_lock.acquire();
	UDATA gcRememberedSet = *gcRememberedSetAddress;
	if (isOverflowedRemememberedSet(gcRememberedSet)) {
		/* this class loader overflowed in another thread - nothing to do */
	} else if (!isTaggedRegionIndex(gcRememberedSet)) {
		/* already inflated - nothing to do */
		Assert_MM_true(0 != gcRememberedSet);
	} else {
		if (NULL == _bitVectorPool) {
			Assert_MM_false(_extensions->tarokEnableIncrementalClassGC);
			*gcRememberedSetAddress = UDATA_MAX;
		} else {
			volatile UDATA* bitVector = (volatile UDATA*)pool_newElement(_bitVectorPool);
			if (NULL == bitVector) {
				*gcRememberedSetAddress = UDATA_MAX;
			} else {
				*gcRememberedSetAddress = (UDATA)bitVector;
				UDATA rememberedRegion = asUntaggedRegionIndex(gcRememberedSet);
				setBit(env, bitVector, rememberedRegion);
			}
		}
	}	
	_lock.release();
}

void
MM_ClassLoaderRememberedSet::setBit(MM_EnvironmentBase* env, volatile UDATA* bitVector, UDATA bit)
{
	UDATA wordIndex = bit / BITS_PER_UDATA;
	UDATA bitIndex = bit % BITS_PER_UDATA;
	UDATA bitMask = ((UDATA)1) << bitIndex;

	Assert_MM_true(wordIndex < _bitVectorSize);
	
	UDATA oldValue = bitVector[wordIndex];
	while (0 == (oldValue & bitMask)) {
		oldValue = MM_AtomicOperations::lockCompareExchange(&bitVector[wordIndex], oldValue, oldValue | bitMask);
	}
}

bool
MM_ClassLoaderRememberedSet::isBitSet(MM_EnvironmentBase* env, volatile UDATA* bitVector, UDATA bit)
{
	UDATA wordIndex = bit / BITS_PER_UDATA;
	UDATA bitIndex = bit % BITS_PER_UDATA;
	UDATA bitMask = ((UDATA)1) << bitIndex;

	Assert_MM_true(wordIndex < _bitVectorSize);
	
	return bitMask == (bitVector[wordIndex] & bitMask);
}


bool
MM_ClassLoaderRememberedSet::isRemembered(MM_EnvironmentBase *env, J9ClassLoader *classLoader)
{
	/* This call is for non-anonymous classloaders only. Anonymous classloader should be handled on classes level */
	Assert_MM_true(!J9_ARE_ANY_BITS_SET(classLoader->flags, J9CLASSLOADER_ANON_CLASS_LOADER));

	return isRememberedInternal(env, classLoader->gcRememberedSet);
}

bool
MM_ClassLoaderRememberedSet::isClassRemembered(MM_EnvironmentBase *env, J9Class *clazz)
{
	/* remembering on class level is supported for anonymous classes only */
	Assert_MM_true(J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassIsAnonymous));
	/* class should not be unloaded otherwise gcLink is used to form list of unloaded classes */
	Assert_MM_true(!J9_ARE_ANY_BITS_SET(clazz->classDepthAndFlags, J9AccClassDying));

	return isRememberedInternal(env, (UDATA)clazz->gcLink);
}

bool
MM_ClassLoaderRememberedSet::isRememberedInternal(MM_EnvironmentBase *env, UDATA gcRememberedSet)
{
	bool isRemembered = false;
	if (0 == gcRememberedSet) {
		/* this class loader is not remembered */
	} else if (isOverflowedRemememberedSet(gcRememberedSet)) {
		/* this class loader is overflowed */
		isRemembered = true;
	} else if (isTaggedRegionIndex(gcRememberedSet)) {
		/* some region is remembered using the the immediate encoding */
		isRemembered = true;
	} else {
		/* a bit vector is installed. Check to see if it's all zero */
		volatile UDATA* bitVector = (volatile UDATA*)gcRememberedSet;
		for (UDATA i = 0; i < _bitVectorSize; i++) {
			if (0 != bitVector[i]) {
				isRemembered = true;
				break;
			}
		}
	}
	return isRemembered;
}

bool
MM_ClassLoaderRememberedSet::isInstanceRemembered(MM_EnvironmentBase *env, J9Object* object)
{
	bool isRemembered = false;
	Assert_MM_true(NULL != object);

	J9Class *clazz = J9GC_J9OBJECT_CLAZZ(object);
	Assert_MM_mustBeClass(clazz);
	
	UDATA regionIndex = _regionManager->physicalTableDescriptorIndexForAddress(object);
	if (J9_ARE_ANY_BITS_SET(J9CLASS_EXTENDED_FLAGS(clazz), J9ClassIsAnonymous)) {
		/* class should not be unloaded otherwise gcLink is used to form list of unloaded classes */
		Assert_MM_true(!J9_ARE_ANY_BITS_SET(clazz->classDepthAndFlags, J9AccClassDying));
		isRemembered = isRegionRemembered(env, regionIndex, (UDATA)clazz->gcLink);
	} else {
		J9ClassLoader *classLoader = clazz->classLoader;
		Assert_MM_true(NULL != classLoader);
		isRemembered = isRegionRemembered(env, regionIndex, classLoader->gcRememberedSet);
	}
	return isRemembered;
}

bool
MM_ClassLoaderRememberedSet::isRegionRemembered(MM_EnvironmentBase *env, UDATA regionIndex, UDATA gcRememberedSet)
{
	bool isRemembered = false;
	UDATA taggedRegionIndex = asTaggedRegionIndex(regionIndex); 
	
	if (taggedRegionIndex == gcRememberedSet) {
		/* this region is the only remembered region */
		isRemembered = true;
	} else if (isOverflowedRemememberedSet(gcRememberedSet)) {
		/* this class loader is overflowed */
		isRemembered = true;
	} else if (0 == gcRememberedSet) {
		/* nothing is remembered */
		isRemembered = false;
	} else if (isTaggedRegionIndex(gcRememberedSet)) {
		/* another region is remembered  */
		isRemembered = false;
	} else {
		/* this remembered set must be an inflated bit vector */
		isRemembered = isBitSet(env, (volatile UDATA*)gcRememberedSet, regionIndex);
	}
	
	return isRemembered;
}

void
MM_ClassLoaderRememberedSet::killRememberedSet(MM_EnvironmentBase *env, J9ClassLoader *classLoader)
{
	Assert_MM_true(!J9_ARE_ANY_BITS_SET(classLoader->flags, J9CLASSLOADER_ANON_CLASS_LOADER));

	killRememberedSetInternal(env, classLoader->gcRememberedSet);
	classLoader->gcRememberedSet = 0;
}

void
MM_ClassLoaderRememberedSet::killRememberedSetInternal(MM_EnvironmentBase *env, UDATA gcRememberedSet)
{
	if (0 == gcRememberedSet) {
		/* nothing to do */
	} else {
		if (!isTaggedRegionIndex(gcRememberedSet)) {
			/* inflated remembered set */
			_lock.acquire();
			Assert_MM_true(NULL != _bitVectorPool);
			pool_removeElement(_bitVectorPool, (void*)gcRememberedSet);
			_lock.release();
		}
	}
}

void 
MM_ClassLoaderRememberedSet::resetRegionsToClear(MM_EnvironmentBase *env)
{
	Assert_MM_true(NULL != _bitsToClear);
	memset(_bitsToClear, 0, _bitVectorSize * sizeof(UDATA));
}

void
MM_ClassLoaderRememberedSet::prepareToClearRememberedSetForRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region)
{
	Assert_MM_true(NULL != _bitsToClear);
	UDATA bitIndex = _regionManager->mapDescriptorToRegionTableIndex(region);
	setBit(env, _bitsToClear, bitIndex);
}

void
MM_ClassLoaderRememberedSet::clearRememberedSets(MM_EnvironmentBase *env)
{
	J9JavaVM *javaVM = (J9JavaVM *)env->getLanguageVM();
	Assert_MM_true(NULL != _bitsToClear);
	GC_ClassLoaderIterator classLoaderIterator(javaVM->classLoaderBlocks);
	J9ClassLoader *classLoader = NULL;
	while(NULL != (classLoader = classLoaderIterator.nextSlot())) {
		if(J9_ARE_ANY_BITS_SET(classLoader->flags, J9CLASSLOADER_ANON_CLASS_LOADER)) {
			/* Anonymous classloader should be scanned on level of classes every time */
			GC_ClassLoaderSegmentIterator segmentIterator(classLoader, MEMORY_TYPE_RAM_CLASS);
			J9MemorySegment *segment = NULL;
			while(NULL != (segment = segmentIterator.nextSegment())) {
				GC_ClassHeapIterator classHeapIterator(javaVM, segment);
				J9Class *clazz = NULL;
				while(NULL != (clazz = classHeapIterator.nextClass())) {
					/* class should not be unloaded otherwise gcLink is used to form list of unloaded classes */
					Assert_MM_true(!J9_ARE_ANY_BITS_SET(clazz->classDepthAndFlags, J9AccClassDying));
					clearRememberedSetsInternal(env, (volatile UDATA *)&clazz->gcLink);
				}
			}
		} else {
			clearRememberedSetsInternal(env, &classLoader->gcRememberedSet);
		}
	}
}

void
MM_ClassLoaderRememberedSet::clearRememberedSetsInternal(MM_EnvironmentBase *env, volatile UDATA *gcRememberedSetAddress)
{
	UDATA gcRememberedSet = *gcRememberedSetAddress;
	if (0 == gcRememberedSet) {
		/* this class loader is not remembered - do nothing */
	} else if (isOverflowedRemememberedSet(gcRememberedSet)) {
		/* this class loader is overflowed  - do nothing */
	} else if (isTaggedRegionIndex(gcRememberedSet)) {
		/* some region is remembered using the the immediate encoding */
		UDATA regionIndex = asUntaggedRegionIndex(gcRememberedSet);
		if (isBitSet(env, _bitsToClear, regionIndex)) {
			/* the region is not preserved - clear it */
			*gcRememberedSetAddress = 0;
		}
	} else {
		/* a bit vector is installed */
		volatile UDATA* bitVector = (volatile UDATA *)gcRememberedSet;
		for (UDATA i = 0; i < _bitVectorSize; i++) {
			if ((0 != _bitsToClear[i]) && (0 != bitVector[i])) {
				bitVector[i] &= ~_bitsToClear[i];
			}
		}
	}
}

void 
MM_ClassLoaderRememberedSet::setupBeforeGC(MM_EnvironmentBase *env)
{
	/* mark the permanent class loaders as overflowed so that we can quickly short circuit remembering their instances */
	J9JavaVM *javaVM = (J9JavaVM *)env->getLanguageVM();
	
	if (NULL != javaVM->systemClassLoader) {
		killRememberedSet(env, javaVM->systemClassLoader);
		javaVM->systemClassLoader->gcRememberedSet = UDATA_MAX;
	}
	
	if (NULL != javaVM->applicationClassLoader) {
		killRememberedSet(env, javaVM->applicationClassLoader);
		javaVM->applicationClassLoader->gcRememberedSet = UDATA_MAX;
	}
}
