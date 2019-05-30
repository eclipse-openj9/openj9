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

#ifndef OBJECTFIELDINFO_HPP_
#define OBJECTFIELDINFO_HPP_
#include "j9comp.h"
#include "j9port.h"
#include "ut_j9vm.h"

/**
 * represent information about number of various field types, including
 * single and double word primitives, object references, and hidden fields.
 * Hidden fields are held in a linked list, instance fields are a contiguous array of J9ROMFieldShape
 */

#define SINGLES_PRIORITY_FOR_BACKFILL

class ObjectFieldInfo {
private:
	U_32 _cacheLineSize;
	bool _useContendedClassLayout; /* check this in constructor. Forced to false if we can't get the line size */
	J9ROMClass *_romClass;
	U_32 _superclassFieldsSize;
	bool _objectCanUseBackfill; /* true if an object reference is the same size as a backfill slot */

	/* These count uncontended instance and hidden fields */
	U_32 _instanceObjectCount;
	U_32 _instanceSingleCount;
	U_32 _instanceDoubleCount;
	U_32 _totalObjectCount;
	U_32 _totalSingleCount;
	U_32 _totalDoubleCount;

	/* the following count fields annotated with @Contended, including regular fields in classes annotated with @Contended. */
	U_32 _contendedObjectCount;
	U_32 _contendedSingleCount;
	U_32 _contendedDoubleCount;

#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
	bool _isValue;
	J9FlattenedClassCache *_flattenedClassCache;
	U_32 _totalFlatFieldDoubleBytes;
	U_32 _totalFlatFieldRefBytes;
	U_32 _totalFlatFieldSingleBytes;
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */

	bool _hiddenFieldOffsetResolutionRequired;
	bool _instanceFieldBackfillEligible; /* use this to give instance fields priority over the hidden fields for backfill slots */
	U_32 _hiddenFieldCount;
	IDATA _superclassBackfillOffset; /* inherited backfill */
	IDATA _myBackfillOffset; /* backfill available for this class's fields */
	IDATA _subclassBackfillOffset; /* backfill available to subclasses */

	/**
	 * Calculate a position guaranteed to be on a different cache line from the header, superclass fields, and hidden fields.
	 * @return offset from the start of the header
	 */
	VMINLINE U_32
	getPaddedTrueNonContendedSize(void) const
	{
		U_32 accumulator = _superclassFieldsSize +  sizeof(J9Object); /* get the true size with header */
		accumulator += (_totalObjectCount * sizeof(J9Object)) + (_totalSingleCount * sizeof(U_32)) + (_totalDoubleCount * sizeof(U_64)); /* add the non-contended and hidden fields */
		accumulator = ROUND_DOWN_TO_POWEROF2(accumulator, OBJECT_SIZE_INCREMENT_IN_BYTES) + _cacheLineSize; /* get the worst-case cache line boundary and add a cache size */
		return accumulator;
	}


public:
	enum {
		NO_BACKFILL_AVAILABLE = -1,
		BACKFILL_SIZE = sizeof(U_32),
		LOCKWORD_SIZE = sizeof(j9objectmonitor_t),
		FINALIZE_LINK_SIZE = sizeof(fj9object_t),
		OBJECT_SIZE_INCREMENT_IN_BYTES = 8
	};

#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
	ObjectFieldInfo(J9JavaVM *vm, J9ROMClass *romClass, J9FlattenedClassCache *flattenedClassCache):
#else
	ObjectFieldInfo(J9JavaVM *vm, J9ROMClass *romClass):
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
		_cacheLineSize(0),
		_useContendedClassLayout(false),
		_romClass(romClass),
		_superclassFieldsSize((U_32) -1),
		_objectCanUseBackfill((sizeof(fj9object_t) == BACKFILL_SIZE)),
		_instanceObjectCount(0),
		_instanceSingleCount(0),
		_instanceDoubleCount(0),
		_totalObjectCount(0),
		_totalSingleCount(0),
		_totalDoubleCount(0),
		_contendedObjectCount(0),
		_contendedSingleCount(0),
		_contendedDoubleCount(0),
#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
		_isValue(J9ROMCLASS_IS_VALUE(romClass)),
		_flattenedClassCache(flattenedClassCache),
		_totalFlatFieldDoubleBytes(0),
		_totalFlatFieldRefBytes(0),
		_totalFlatFieldSingleBytes(0),
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
		_hiddenFieldOffsetResolutionRequired(false),
		_instanceFieldBackfillEligible(false),
		_hiddenFieldCount(0),
		_superclassBackfillOffset(NO_BACKFILL_AVAILABLE),
		_myBackfillOffset(NO_BACKFILL_AVAILABLE),
		_subclassBackfillOffset(NO_BACKFILL_AVAILABLE)
	{
		if (J9ROMCLASS_IS_CONTENDED(romClass)) {
			PORT_ACCESS_FROM_VMC(vm);
			J9CacheInfoQuery cQuery;
			memset(&cQuery, 0, sizeof(cQuery));
			cQuery.cmd = J9PORT_CACHEINFO_QUERY_LINESIZE;
			cQuery.level = 1;
			cQuery.cacheType = J9PORT_CACHEINFO_DCACHE;
			IDATA queryResult = j9sysinfo_get_cache_info(&cQuery);
			if (queryResult > 0) {
				_cacheLineSize = (U_32) queryResult;
				_useContendedClassLayout = true;
			} else {
				Trc_VM_contendedLinesizeFailed(queryResult);
			}
		}
	}

	/**
	 * Count the various types of instance fields from the ROM class.
	 * Instance fields are represented as a contiguous array of variable size structures.
	 */
	void
	countInstanceFields(void);

	/**
	 * Count the various types of hidden fields, filtering out those not applicable to this ROM class,
	 * and copy them to a list.
	 * @param hiddenFieldList linked list of hidden fields
	 * @param hiddenFieldArray buffer into which to copy the applicable fields
	 * @return number of hidden fields
	 */
	U_32
	countAndCopyHiddenFields(J9HiddenInstanceField *hiddenFieldList, J9HiddenInstanceField *hiddenFieldArray[]);

	VMINLINE U_32
	getTotalDoubleCount(void) const
	{
		return _totalDoubleCount;
	}

	VMINLINE U_32
	getTotalObjectCount(void) const
	{
		return _totalObjectCount;
	}

	VMINLINE U_32
	getTotalSingleCount(void) const
	{
		return _totalSingleCount;
	}

	/**
	 * Priorities for slots are:
	 * 1. instance singles
	 * 2. instance objects
	 * 3. hidden singles
	 * 4. hidden objects
	 * @return number of object fields (instance and hidden) which do not go  in a backfill slot.
	 */
	VMINLINE U_32
	getNonBackfilledObjectCount(void) const
	{
		U_32 nonBackfilledObjects = _totalObjectCount;
		if (isBackfillSuitableObjectAvailable()  && !isBackfillSuitableInstanceSingleAvailable()
				&& isMyBackfillSlotAvailable()) {
			nonBackfilledObjects -= 1;
		}
		return nonBackfilledObjects;
	}

	VMINLINE U_32
	getNonBackfilledSingleCount(void) const
	{
		U_32 nonBackfilledSingle = _totalSingleCount;
		if (isBackfillSuitableSingleAvailable() && isMyBackfillSlotAvailable()) {
			nonBackfilledSingle -= 1;
		}
		return nonBackfilledSingle;
	}

	VMINLINE U_32
	getNonBackfilledInstanceObjectCount(void) const
 {
		U_32 nonBackfilledObjects = _instanceObjectCount;
		if (isBackfillSuitableInstanceObjectAvailable()  && !isBackfillSuitableInstanceSingleAvailable()
				&& isMyBackfillSlotAvailable()) {
			nonBackfilledObjects -= 1;
		}
		return nonBackfilledObjects;
	}

	VMINLINE U_32
	getNonBackfilledInstanceSingleCount(void) const
	{
		U_32 nonBackfilledSingle = _instanceSingleCount;
		if (isBackfillSuitableInstanceSingleAvailable() && isMyBackfillSlotAvailable()) {
			nonBackfilledSingle -= 1;
		}
		return nonBackfilledSingle;
	}

	VMINLINE U_32
	getInstanceDoubleCount(void) const
	{
		return _instanceDoubleCount;
	}

	VMINLINE U_32
	getInstanceObjectCount(void) const
	{
		return _instanceObjectCount;
	}

	VMINLINE U_32
	getInstanceSingleCount(void) const
	{
		return _instanceSingleCount;
	}

	VMINLINE U_32
	getHiddenFieldCount(void) const
	{
		return _hiddenFieldCount;
	}

	VMINLINE bool
	isBackfillSuitableSingleAvailable(void) const
	{
		return (0 != getTotalSingleCount());
	}

	VMINLINE bool
	isBackfillSuitableObjectAvailable(void) const
	{
		return (_objectCanUseBackfill && (0 != getTotalObjectCount()));
	}

	VMINLINE bool
	isBackfillSuitableInstanceSingleAvailable(void) const
	{
		return (0 != getInstanceSingleCount());
	}

	VMINLINE bool
	isBackfillSuitableInstanceObjectAvailable(void) const
	{
		return (_objectCanUseBackfill && (0 != getInstanceObjectCount()));
	}

	VMINLINE bool
	isBackfillSuitableFieldAvailable(void) const
	{
		return isBackfillSuitableSingleAvailable() || isBackfillSuitableObjectAvailable();
	}


	/**
	 * @note This is used for hidden fields which use an offset from the header
	 * @return offset of backfill slot from the start of the object
	 */
	VMINLINE IDATA
	getMyBackfillOffsetForHiddenField(void) const
	{
		return _myBackfillOffset + sizeof(J9Object);
	}

	VMINLINE U_32
	getSuperclassFieldsSize(void) const
	{
		return _superclassFieldsSize;
	}

	VMINLINE U_32
	getSuperclassObjectSize(void) const
	{
		return _superclassFieldsSize + sizeof(J9Object);
	}

	VMINLINE void
	setSuperclassFieldsSize(U_32 superTotalSize)
	{
		this->_superclassFieldsSize = superTotalSize;
	}

	VMINLINE bool
	const isMyBackfillSlotAvailable(void) const
	{
		return (_myBackfillOffset >= 0);
	}

	VMINLINE bool
	const isSuperclassBackfillSlotAvailable(void) const
	{
		return (_superclassBackfillOffset >= 0);
	}

	/**
	 * Compute the total size of the object including padding to end on 8-byte boundary.
	 * Update the backfill values to use for this class's fields and those of subclasses
	 * @return size of the object in bytes, not including the header
	 */
	U_32
	calculateTotalFieldsSizeAndBackfill(void);

	/**
	 * Fields can start on the first 8-byte boundary after the end of the superclass.
	 * If the superclass is not end-aligned, the padding bytes become the new backfill.
	 * Since the superclass is not end-aligned it must have no embedded backfill.
	 * @return offset to the first (non-backfilled) field.
	 */
	VMINLINE U_32
	calculateFieldDataStart(void) const
	{
		U_32 fieldDataStart = 0;
		if (!isContendedClassLayout()) {
			bool doubleAlignment = (_totalDoubleCount > 0);
#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
			doubleAlignment = doubleAlignment || (_totalFlatFieldDoubleBytes > 0);
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */

			fieldDataStart = getSuperclassFieldsSize();
			if (
					((getSuperclassObjectSize() % OBJECT_SIZE_INCREMENT_IN_BYTES) != 0) && /* superclass is not end-aligned */
					(doubleAlignment || (!_objectCanUseBackfill && (_totalObjectCount > 0)))
			){ /* our fields start on a 8-byte boundary */
				fieldDataStart += BACKFILL_SIZE;
			}
		} else {
			if (TrcEnabled_Trc_VM_contendedClass) {
				J9UTF8 *className = J9ROMCLASS_CLASSNAME(_romClass);
				U_32	utfLength = J9UTF8_LENGTH(className);
				U_8 *utfData = J9UTF8_DATA(className);
				Trc_VM_contendedClass(utfLength, utfData);
			}
			fieldDataStart = getPaddedTrueNonContendedSize() - sizeof(J9Object);
		}
		return fieldDataStart;
	}

	/**
	 * @note Always uses the number of doubles
	 * @param start end of previous field area
	 * @return offset to end of the doubles area
	 */
	VMINLINE UDATA
	addDoublesArea(UDATA start) const
	{
		return start + ((isContendedClassLayout() ? _contendedDoubleCount: _totalDoubleCount) * sizeof(U_64));
	}

	/**
	 * @param start end of previous field area
	 * @return offset to end of the objects area
	 * @note takes into account objects which will go in the backfill
	 */
	VMINLINE UDATA
	addObjectsArea(UDATA start) const
	{
		return start + (isContendedClassLayout() ? _contendedObjectCount: getNonBackfilledObjectCount()) * sizeof(fj9object_t);
	}

#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
	/**
	 * @param start end of previous field area, which should be the first field area
	 * @return offset to end of the flat doubles area
	 */
	VMINLINE UDATA
	addFlatDoublesArea(UDATA start) const
	{
		return start + _totalFlatFieldDoubleBytes;
	}

	/**
	 * @param start end of previous field area, which should be after doubles
	 * @return offset to end of the flat doubles area
	 */
	VMINLINE UDATA
	addFlatObjectsArea(UDATA start) const
	{
		return start + _totalFlatFieldRefBytes;
	}

	/**
	 * @param start end of previous field area, which should be after objects
	 * @return offset to end of the flat doubles area
	 */
	VMINLINE UDATA
	addFlatSinglesArea(UDATA start) const
	{
		return start + _totalFlatFieldSingleBytes;
	}

	VMINLINE bool
	isValue() const
	{
		return _isValue;
	}
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */

	VMINLINE bool
	isHiddenFieldOffsetResolutionRequired(void) const
	{
		return _hiddenFieldOffsetResolutionRequired;
	}

	/**
	 * @note This is used for instance fields.
	 * The backfill slot can either be embedded within the superclass,
	 * or it can be the result of padding the superclass.
	 * If there is no embedded backfile (i.e. backfillOffset == -1, then see if there is space available
	 * at the end of the superclass.
	 * @return backfill offset from the first field
	 */

	VMINLINE IDATA
	getMyBackfillOffset(void) const
	{
		return _myBackfillOffset;
	}

	VMINLINE IDATA
	getSubclassBackfillOffset(void) const
	{
		return _subclassBackfillOffset;
	}

	VMINLINE void
	setSuperclassBackfillOffset(IDATA _superclassBackfillOffset)
	{
		this->_superclassBackfillOffset = _superclassBackfillOffset;
	}

	VMINLINE IDATA
	getSuperclassBackfillOffset(void) const
	{
		return _superclassBackfillOffset;
	}

	VMINLINE bool
	isInstanceFieldBackfillEligible(void) const
	{
		return _instanceFieldBackfillEligible;
	}

	VMINLINE bool
	isContendedClassLayout(void) const {
		return _useContendedClassLayout;
	}
};
#endif /* OBJECTFIELDINFO_HPP_ */
