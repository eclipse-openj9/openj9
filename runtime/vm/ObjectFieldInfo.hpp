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
	U_32 _objectHeaderSize;
	U_32 _referenceSize;
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
	U_32 _flatAlignedObjectInstanceBackfill;
	U_32 _flatAlignedSingleInstanceBackfill;
	U_32 _flatUnAlignedObjectInstanceBackfill;
	U_32 _flatUnAlignedSingleInstanceBackfill;
	bool _classRequiresPrePadding;
	bool _isBackFillPostPadded;
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
		U_32 accumulator = _superclassFieldsSize +  _objectHeaderSize; /* get the true size with header */
		accumulator += (_totalObjectCount * _objectHeaderSize) + (_totalSingleCount * sizeof(U_32)) + (_totalDoubleCount * sizeof(U_64)); /* add the non-contended and hidden fields */
		accumulator = ROUND_DOWN_TO_POWEROF2(accumulator, OBJECT_SIZE_INCREMENT_IN_BYTES) + _cacheLineSize; /* get the worst-case cache line boundary and add a cache size */
		return accumulator;
	}


public:
	enum {
		NO_BACKFILL_AVAILABLE = -1,
		BACKFILL_SIZE = sizeof(U_32),
		OBJECT_SIZE_INCREMENT_IN_BYTES = 8
	};

#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
	ObjectFieldInfo(J9JavaVM *vm, J9ROMClass *romClass, J9FlattenedClassCache *flattenedClassCache):
#else
	ObjectFieldInfo(J9JavaVM *vm, J9ROMClass *romClass):
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
		_objectHeaderSize(J9JAVAVM_OBJECT_HEADER_SIZE(vm)),
		_referenceSize(J9JAVAVM_REFERENCE_SIZE(vm)),
		_cacheLineSize(0),
		_useContendedClassLayout(false),
		_romClass(romClass),
		_superclassFieldsSize((U_32) -1),
		_objectCanUseBackfill(J9JAVAVM_REFERENCE_SIZE(vm) == BACKFILL_SIZE),
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
		_flatAlignedObjectInstanceBackfill(0),
		_flatAlignedSingleInstanceBackfill(0),
		_flatUnAlignedObjectInstanceBackfill(0),
		_flatUnAlignedSingleInstanceBackfill(0),
		_classRequiresPrePadding(false),
		_isBackFillPostPadded(false),
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
		_hiddenFieldOffsetResolutionRequired(false),
		_instanceFieldBackfillEligible(false),
		_hiddenFieldCount(0),
		_superclassBackfillOffset(NO_BACKFILL_AVAILABLE),
		_myBackfillOffset(NO_BACKFILL_AVAILABLE),
		_subclassBackfillOffset(NO_BACKFILL_AVAILABLE)
	{
		if (J9ROMCLASS_IS_CONTENDED(romClass)) {
			UDATA dCacheLineSize = vm->dCacheLineSize;
			if (dCacheLineSize > 0) {
				_cacheLineSize = (U_32) dCacheLineSize;
				_useContendedClassLayout = true;
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
	 * 3. flat end-aligned singles
	 * 4. flat end-aligned objects
	 * 5. flat end-unaligned singles
	 * 6. flat end-unaligned objects
	 * 7. hidden singles
	 * 8. hidden objects
	 *
	 * @return number of object fields (instance and hidden) which do not go  in a backfill slot.
	 */
	VMINLINE U_32
	getNonBackfilledObjectCount(void) const
	{
		U_32 nonBackfilledObjects = _totalObjectCount;
		if (isBackfillSuitableObjectAvailable()
			&& !isBackfillSuitableInstanceSingleAvailable()
			&& isMyBackfillSlotAvailable()
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
			&& (0 != nonBackfilledObjects)
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		) {
			nonBackfilledObjects -= 1;
		}
		return nonBackfilledObjects;
	}

	VMINLINE U_32
	getNonBackfilledSingleCount(void) const
	{
		U_32 nonBackfilledSingle = _totalSingleCount;
		if (isBackfillSuitableSingleAvailable()
			&& isMyBackfillSlotAvailable()
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
			&& (0 != nonBackfilledSingle)
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		) {
			nonBackfilledSingle -= 1;
		}
		return nonBackfilledSingle;
	}

	VMINLINE U_32
	getNonBackfilledInstanceObjectCount(void) const
	{
		U_32 nonBackfilledObjects = _instanceObjectCount;
		if (isBackfillSuitableInstanceObjectAvailable()
			&& !isBackfillSuitableInstanceSingleAvailable()
			&& isMyBackfillSlotAvailable()
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
			&& (0 != nonBackfilledObjects)
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		) {
			nonBackfilledObjects -= 1;
		}
		return nonBackfilledObjects;
	}

	VMINLINE U_32
	getNonBackfilledInstanceSingleCount(void) const
	{
		U_32 nonBackfilledSingle = _instanceSingleCount;
		if (isBackfillSuitableInstanceSingleAvailable()
		&& isMyBackfillSlotAvailable()
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		&& (0 != nonBackfilledSingle)
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		) {
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
		return ((0 != getTotalSingleCount())
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				|| (0 != getFlatAlignedSingleInstanceBackfillSize())
				|| (0 != getFlatUnAlignedSingleInstanceBackfillSize())
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		);
	}

	VMINLINE bool
	isBackfillSuitableObjectAvailable(void) const
	{
		return ((_objectCanUseBackfill && (0 != getTotalObjectCount()))
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				|| (0 != getFlatAlignedObjectInstanceBackfillSize())
				|| (0 != getFlatUnAlignedObjectInstanceBackfillSize())
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		);
	}

	VMINLINE bool
	isBackfillSuitableInstanceSingleAvailable(void) const
	{
		return ((0 != getInstanceSingleCount())
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				|| (0 != getFlatAlignedSingleInstanceBackfillSize())
				|| (0 != getFlatUnAlignedSingleInstanceBackfillSize())
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		);
	}

	VMINLINE bool
	isBackfillSuitableInstanceObjectAvailable(void) const
	{
		return ((_objectCanUseBackfill && (0 != getInstanceObjectCount()))
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				|| (0 != getFlatAlignedObjectInstanceBackfillSize())
				|| (0 != getFlatUnAlignedObjectInstanceBackfillSize())
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
				);
	}

	VMINLINE bool
	isBackfillSuitableFieldAvailable(void) const
	{
		return isBackfillSuitableSingleAvailable() || isBackfillSuitableObjectAvailable();
	}

	/**
	 *
	 * Returns the size of backfill. The order or precedence is:
	 * 1. Singles
	 * 2. Objects
	 * 3. Flat end-aligned singles
	 * 4. Flat end-aligned objects
	 * 5. Flat end-unaligned singles
	 * 6. Flat end un-aligned objects
	 *
	 * @return size of backfill
	 */
	VMINLINE U_32
	getBackfillSize()
	{
		U_32 backFillSize = BACKFILL_SIZE;
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		if ((0 != getInstanceSingleCount()) || (_objectCanUseBackfill && (0 != getInstanceObjectCount()))) {
			/* BACKFILL_SIZE */
		} else if (0 != getFlatAlignedSingleInstanceBackfillSize()) {
			backFillSize = getFlatAlignedSingleInstanceBackfillSize();
		} else if (_objectCanUseBackfill && (0 != getFlatAlignedObjectInstanceBackfillSize())) {
			backFillSize = getFlatAlignedObjectInstanceBackfillSize();
		} else if (0 != getFlatUnAlignedSingleInstanceBackfillSize()) {
			backFillSize = getFlatUnAlignedSingleInstanceBackfillSize();
		} else if (_objectCanUseBackfill && (0 != getFlatUnAlignedObjectInstanceBackfillSize())) {
			backFillSize = getFlatUnAlignedObjectInstanceBackfillSize();
		}
		return backFillSize;
#else /* if defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		return backFillSize;
#endif /* if defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	}

	/**
	 * @note This is used for hidden fields which use an offset from the header
	 * @return offset of backfill slot from the start of the object
	 */
	VMINLINE IDATA
	getMyBackfillOffsetForHiddenField(void) const
	{
		return _myBackfillOffset + _objectHeaderSize;
	}

	VMINLINE U_32
	getSuperclassFieldsSize(void) const
	{
		return _superclassFieldsSize;
	}

	VMINLINE U_32
	getSuperclassObjectSize(void) const
	{
		return _superclassFieldsSize + _objectHeaderSize;
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
	calculateFieldDataStart(void)
	{
		U_32 fieldDataStart = 0;
		if (!isContendedClassLayout()) {
			bool doubleAlignment = (_totalDoubleCount > 0);
			bool hasObjects = _totalObjectCount > 0;
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)

			/* If the type has double field the doubles need to start on an 8-byte boundary. In the case of valuetypes
			 * there is no lockword so the super class field size will be zero. This means that valueTypes in compressedrefs
			 * mode with double alignment fields need to be pre-padded.
			 */
			doubleAlignment = (doubleAlignment || (_totalFlatFieldDoubleBytes > 0));
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

			fieldDataStart = getSuperclassFieldsSize();
			if (((getSuperclassObjectSize() % OBJECT_SIZE_INCREMENT_IN_BYTES) != 0) /* superclass is not end-aligned */
				&& (doubleAlignment || (!_objectCanUseBackfill && hasObjects))
			) {
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				fieldDataStart += getBackfillSize();
				/* If a flat type is used up as backfill and its size is not 4, 12, 20, ... this
				 * means that the starting point for doubles will not be on an 8byte boundary. To
				 * fix this the field start position will be incremented by 4bytes so doubles start
				 * at the correct position.
				 */
				if (!isBackfillTypeEndAligned(fieldDataStart)) {
					fieldDataStart += BACKFILL_SIZE;
					_isBackFillPostPadded = true;
				}
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
				fieldDataStart += BACKFILL_SIZE;
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
			}
		} else {
			if (TrcEnabled_Trc_VM_contendedClass) {
				J9UTF8 *className = J9ROMCLASS_CLASSNAME(_romClass);
				U_32	utfLength = J9UTF8_LENGTH(className);
				U_8 *utfData = J9UTF8_DATA(className);
				Trc_VM_contendedClass(utfLength, utfData);
			}
			fieldDataStart = getPaddedTrueNonContendedSize() - _objectHeaderSize;
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
		return start + (isContendedClassLayout() ? _contendedObjectCount: getNonBackfilledObjectCount()) * _referenceSize;
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
		return start + getNonBackfilledFlatInstanceObjectSize();
	}

	/**
	 * @param start end of previous field area, which should be after objects
	 * @return offset to end of the flat doubles area
	 */
	VMINLINE UDATA
	addFlatSinglesArea(UDATA start) const
	{
		return start + getNonBackfilledFlatInstanceSingleSize();
	}

	/**
	 * Determines if a double field can be placed contiguously after the
	 * backfill. If this is the case then the back fill is considered
	 * "end-aligned"
	 *
	 * @param fieldDataStart the address immediately following the backfill if any,
	 * 			otherwise the address following the object header
	 *
	 * @return true if double field can be placed immediately after, false otherwise
	 */
	VMINLINE bool
	isBackfillTypeEndAligned(UDATA fieldDataStart) {
		return BACKFILL_SIZE == (fieldDataStart % sizeof(U_64));
	}

	/**
	 * Returns if type is a valuetype
	 *
	 * @return true if valuetype, false otherwise
	 */
	VMINLINE bool
	isValue() const
	{
		return _isValue;
	}

	/**
	 * Returns the size of the end-aligned single backfill.
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @return size of flat single slot end-aligned backfill
	 */
	VMINLINE U_32
	getFlatAlignedSingleInstanceBackfillSize() const
	{
		return _flatAlignedSingleInstanceBackfill;
	}

	/**
	 * Returns the size of the non end-aligned single backfill.
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @return size of flat single slot non end-aligned backfill
	 */
	VMINLINE U_32
	getFlatUnAlignedSingleInstanceBackfillSize() const
	{
		return _flatUnAlignedSingleInstanceBackfill;
	}

	/**
	 * Returns the size of the end-aligned object backfill.
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @return size of flat object slot end-aligned backfill
	 */
	VMINLINE U_32
	getFlatAlignedObjectInstanceBackfillSize() const
	{
		return _flatAlignedObjectInstanceBackfill;
	}

	/**
	 * Returns the size of the non end-aligned single backfill.
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @return size of flat object slot end-aligned backfill
	 */
	VMINLINE U_32
	getFlatUnAlignedObjectInstanceBackfillSize() const
	{
		return _flatUnAlignedObjectInstanceBackfill;
	}

	/**
	 * Sets the size of the end-aligned single backfill.
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @param size of flat single slot end-aligned backfill
	 */
	VMINLINE void
	setFlatAlignedSingleInstanceBackfill(U_32 size)
	{
		_flatAlignedSingleInstanceBackfill = size;
	}

	/**
	 * Sets the size of the non end-aligned single backfill.
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @param size of flat single slot non end-aligned backfill
	 */
	VMINLINE void
	setFlatUnAlignedSingleInstanceBackfill(U_32 size)
	{
		_flatUnAlignedSingleInstanceBackfill = size;
	}

	/**
	 * Sets the size of the end-aligned object backfill.
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @param size of flat object slot end-aligned backfill
	 */
	VMINLINE void
	setFlatAlignedObjectInstanceBackfill(U_32 size)
	{
		_flatAlignedObjectInstanceBackfill = size;
	}

	/**
	 * Sets the size of the non end-aligned object backfill.
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @param size of flat object slot non end-aligned backfill
	 */
	VMINLINE void
	setFlatUnAlignedObjectInstanceBackfill(U_32 size)
	{
		_flatUnAlignedObjectInstanceBackfill = size;
	}

	/**
	 * Sets the size of the object backfill. Determines if the backfill
	 * is end-aligned or not. End-aligned backfill gets precedence, once
	 * the first end-aligned backfill is set not other object backfill
	 * can be set. If not non end-aligned backfill will be set (which can
	 * only be set once).
	 *
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @param size of flat object slot end-aligned backfill
	 */
	VMINLINE void
	setPotentialFlatObjectInstanceBackfill(U_32 size)
	{
		if (_isValue) {
			if (isBackfillTypeEndAligned(size)) {
				if (0 == getFlatAlignedObjectInstanceBackfillSize()) {
					setFlatAlignedObjectInstanceBackfill(size);
				}
			} else {
				if (0 == getFlatUnAlignedObjectInstanceBackfillSize()) {
					setFlatUnAlignedObjectInstanceBackfill(size);
				}
			}
		}
	}

	/**
	 * Sets the size of the single backfill. Determines if the backfill
	 * is end-aligned or not. End-aligned backfill gets precedence, once
	 * the first end-aligned backfill is set not other single backfill
	 * can be set. If not non end-aligned backfill will be set (which can
	 * only be set once).
	 *
	 * See 'isBackfillTypeEndAligned'
	 *
	 * @param size of flat single slot end-aligned backfill
	 */
	VMINLINE void
	setPotentialFlatSingleInstanceBackfill(U_32 size)
	{
		if (_isValue) {
			if (isBackfillTypeEndAligned(size)) {
				if (0 == getFlatAlignedSingleInstanceBackfillSize()) {
					setFlatAlignedSingleInstanceBackfill(size);
				}
			} else {
				if (0 == getFlatUnAlignedSingleInstanceBackfillSize()) {
					setFlatUnAlignedSingleInstanceBackfill(size);
				}
			}
		}
	}

	/**
	 * Returns the size of the flat object instance bytes minus backfill
	 *
	 * @return size of flat object instance bytes
	 */
	VMINLINE U_32
	getNonBackfilledFlatInstanceObjectSize(void) const
	{
		U_32 nonBackfilledFlatObjectsSize = _totalFlatFieldRefBytes;
		if (isBackfillSuitableInstanceObjectAvailable()
			&& isMyBackfillSlotAvailable()
			&& (0 == getInstanceSingleCount())
			&& (_objectCanUseBackfill && (0 == getInstanceObjectCount()))
			&& (0 == getFlatAlignedSingleInstanceBackfillSize())
		) {
			U_32 backfillSize = getFlatAlignedObjectInstanceBackfillSize();
			if (0 == backfillSize) {
				if (0 == getFlatUnAlignedSingleInstanceBackfillSize()) {
					backfillSize = getFlatUnAlignedObjectInstanceBackfillSize();
				}
			}
			nonBackfilledFlatObjectsSize -= backfillSize;
		}
		return nonBackfilledFlatObjectsSize;
	}

	/**
	 * Returns the size of the flat single instance bytes minus backfill
	 *
	 * @return size of flat single instance bytes
	 */
	VMINLINE U_32
	getNonBackfilledFlatInstanceSingleSize(void) const
	{
		U_32 nonBackfilledFlatSinglesSize = _totalFlatFieldSingleBytes;
		if (isBackfillSuitableInstanceSingleAvailable()
			&& isMyBackfillSlotAvailable()
			&& (0 == getInstanceSingleCount())
			&& (_objectCanUseBackfill && (0 == getInstanceObjectCount()))
		) {
			U_32 backfillSize = getFlatAlignedSingleInstanceBackfillSize();
			if (0 == backfillSize) {
				if (0 == getFlatAlignedObjectInstanceBackfillSize()) {
					backfillSize = getFlatUnAlignedSingleInstanceBackfillSize();
				}
			}
			nonBackfilledFlatSinglesSize -= backfillSize;
		}
		return nonBackfilledFlatSinglesSize;
	}

	/**
	 * Returns true if the type requires pre-padding. Pre-padding is
	 * added on -XcompressedRefs and 32bit modes on valuetypes where there
	 * is a field with double (64bit) alignment. These types need pre-padding as
	 * valuetypes do not have lockwords and the double cannot be placed immediately
	 * after a 32bit header. When a type is flattened the pre-padding is ommitted.
	 *
	 * @return true, if the type requires pre-padding
	 */
	VMINLINE bool
	doesClassRequiresPrePadding() const
	{
		return _classRequiresPrePadding;
	}

	/**
	 * Sets the flag indicating that the type requires pre-padding
	 */
	VMINLINE void
	setClassRequiresPrePadding()
	{
		_classRequiresPrePadding = true;
	}

	/**
	 * In cases where the backfill is not end-aligned (see isBackfillTypeEndAligned)
	 * padding bytes need to be added after the backfill so a double can be placed immediately after.
	 *
	 * @return if type contains backfill that requires post-padding
	 */
	VMINLINE bool
	isBackFillPostPadded()
	{
		return _isBackFillPostPadded;
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
