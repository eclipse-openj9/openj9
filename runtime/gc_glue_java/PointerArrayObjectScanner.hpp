/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

#if !defined(POINTERARRAYOBJECTSCANNER_HPP_)
#define POINTERARRAYOBJECTSCANNER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "modron.h"

#include "ArrayObjectModel.hpp"
#include "GCExtensionsBase.hpp"
#include "IndexableObjectScanner.hpp"

/**
 * This class is used to iterate over the slots of a Java pointer array.
 */
class GC_PointerArrayObjectScanner : public GC_IndexableObjectScanner
{
	/* Data Members */
private:
	fomrobject_t *_mapPtr;	/**< pointer to first array element in current scan segment */

protected:

public:

	/* Methods */
private:

protected:
	/**
	 * @param env The scanning thread environment
	 * @param[in] arrayPtr pointer to the array to be processed
	 * @param[in] basePtr pointer to the first contiguous array cell
	 * @param[in] limitPtr pointer to end of last contiguous array cell
	 * @param[in] scanPtr pointer to the array cell where scanning will start
	 * @param[in] endPtr pointer to the array cell where scanning will stop
	 * @param[in] flags Scanning context flags
	 */
	MMINLINE GC_PointerArrayObjectScanner(
		MM_EnvironmentBase *env
		, omrobjectptr_t arrayPtr
		, fomrobject_t *basePtr
		, fomrobject_t *limitPtr
		, fomrobject_t *scanPtr
		, fomrobject_t *endPtr
		, uintptr_t flags
	)
		: GC_IndexableObjectScanner(env, arrayPtr, basePtr, limitPtr, scanPtr, endPtr, flags)
		, _mapPtr(_scanPtr)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * @param env The scanning thread environment
	 * @param objectScanner The scanner to split from
	 * @param splitAmount The maximum number of array elements to include
	 */
	MMINLINE GC_PointerArrayObjectScanner(MM_EnvironmentBase *env, GC_PointerArrayObjectScanner *objectScanner, uintptr_t splitAmount)
		: GC_IndexableObjectScanner(env, objectScanner, splitAmount)
		, _mapPtr(_scanPtr)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Set up the scanner.
	 * @param[in] env Current environment
	 */
	MMINLINE void
	initialize(MM_EnvironmentBase *env)
	{
		GC_IndexableObjectScanner::initialize(env);
	}

public:
	/**
	 * @param[in] env The scanning thread environment
	 * @param[in] objectPtr pointer to the array to be processed
	 * @param[in] allocSpace pointer to space within which the scanner should be instantiated (in-place)
	 * @param[in] flags Scanning context flags
	 * @param[in] splitAmount If >0, the number of elements to include for this scanner instance; if 0, include all elements
	 * @param[in] startIndex The index of the first element to scan
	 */
	MMINLINE static GC_PointerArrayObjectScanner *
	newInstance(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, void *allocSpace, uintptr_t flags, uintptr_t splitAmount, uintptr_t startIndex = 0)
	{
		GC_PointerArrayObjectScanner *objectScanner = (GC_PointerArrayObjectScanner *)allocSpace;
		if (NULL != objectScanner) {
			GC_ArrayObjectModel *arrayObjectModel = &(env->getExtensions()->indexableObjectModel);
			omrarrayptr_t arrayPtr = (omrarrayptr_t)objectPtr;
			uintptr_t sizeInElements = arrayObjectModel->getSizeInElements(arrayPtr);
			fomrobject_t *basePtr = (fj9object_t *)arrayObjectModel->getDataPointerForContiguous(arrayPtr);
			fomrobject_t *scanPtr = basePtr + startIndex;
			fomrobject_t *limitPtr = basePtr + sizeInElements;
			fomrobject_t *endPtr = limitPtr;

			if (!GC_ObjectScanner::isIndexableObjectNoSplit(flags)) {
				fomrobject_t *splitPtr = scanPtr + splitAmount;
				if ((splitPtr > scanPtr) && (splitPtr < endPtr)) {
					endPtr = splitPtr;
				}
			}

			new(objectScanner) GC_PointerArrayObjectScanner(env, objectPtr, basePtr, limitPtr, scanPtr, endPtr, flags);
			objectScanner->initialize(env);
			if (0 != startIndex) {
				objectScanner->clearHeadObjectScanner();
			}
		}
		return objectScanner;
	}

	MMINLINE uintptr_t getBytesRemaining() { return sizeof(fomrobject_t) * (_endPtr - _scanPtr); }

	/**
	 * @param env The scanning thread environment
	 * @param[in] allocSpace pointer to space within which the scanner should be instantiated (in-place)
	 * @param splitAmount The maximum number of array elements to include
	 * @return Pointer to split scanner in allocSpace
	 */
	virtual GC_IndexableObjectScanner *
	splitTo(MM_EnvironmentBase *env, void *allocSpace, uintptr_t splitAmount)
	{
		GC_PointerArrayObjectScanner *splitScanner = NULL;

		Assert_MM_true(_limitPtr >= _endPtr);
		/* Downsize splitAmount if larger than the tail of this scanner */
		uintptr_t remainder = (uintptr_t)(_limitPtr - _endPtr);
		if (splitAmount > remainder) {
			splitAmount = remainder;
		}

		Assert_MM_true(NULL != allocSpace);
		/* If splitAmount is 0 the new scanner will return NULL on the first call to getNextSlot(). */
		splitScanner = (GC_PointerArrayObjectScanner *)allocSpace;
		new(splitScanner) GC_PointerArrayObjectScanner(env, this, splitAmount);
		splitScanner->initialize(env);

		return splitScanner;
	}

	/**
	 * @see GC_ObjectScanner::getNextSlotMap()
	 */
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t &slotMap, bool &hasNextSlotMap)
	{
		_mapPtr += _bitsPerScanMap;
		if (_endPtr > _mapPtr) {
			intptr_t remainder = _endPtr - _mapPtr;
			if (remainder >= _bitsPerScanMap) {
				slotMap = UDATA_MAX;
			} else {
				slotMap = ((uintptr_t)1 << remainder) - 1;
			}
			hasNextSlotMap = remainder > _bitsPerScanMap;
			return _mapPtr;
		} else {
			slotMap = 0;
			hasNextSlotMap = false;
			return NULL;
		}
	}

#if defined(OMR_GC_LEAF_BITS)
	virtual fomrobject_t *
	getNextSlotMap(uintptr_t &scanMap, uintptr_t &leafMap, bool &hasNextSlotMap)
	{
		leafMap = 0;
		return getNextSlotMap(scanMap, hasNextSlotMap);
	}
#endif /* defined(OMR_GC_LEAF_BITS) */
};

#endif /* POINTERARRAYOBJECTSCANNER_HPP_ */
