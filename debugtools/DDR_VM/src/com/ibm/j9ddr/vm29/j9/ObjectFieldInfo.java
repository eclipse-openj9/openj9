/*******************************************************************************
 * Copyright (c) 2015, 2020 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagObject;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldSizeDouble;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.J9AccStatic;

import java.util.ArrayList;
import java.util.LinkedList;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMFieldShapeHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.pointer.helper.ValueTypeHelper;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.UDATA;

public class ObjectFieldInfo {
	ValueTypeHelper valueTypeHelper = ValueTypeHelper.getValueTypeHelper();
	J9ROMClassPointer romClass;
	int superclassFieldsSize;
	boolean objectCanUseBackfill; /* true if an object reference is the same size as a backfill slot */
	// Sizeof constants
	public static final int fj9object_t_SizeOf =
			(J9ObjectHelper.compressObjectReferences ? U32.SIZEOF : UDATA.SIZEOF);
	public static final int j9objectmonitor_t_SizeOf =
			(J9ObjectHelper.compressObjectReferences ? U32.SIZEOF : UDATA.SIZEOF);

	int instanceObjectCount;
	int instanceSingleCount;
	int instanceDoubleCount;
	int totalObjectCount;
	int totalSingleCount;
	int totalDoubleCount;
	boolean instanceFieldBackfillEligible; /* use this to give instance fields priority over the hidden fields for backfill slots */
	int hiddenFieldCount;
	int superclassBackfillOffset; /* inherited backfill */
	int myBackfillOffset; /* backfill available for this class's fields */
	int subclassBackfillOffset; /* backfill available to subclasses */
	boolean isValue = false;
	J9ClassPointer containerClazz = J9ClassPointer.NULL;
	int totalFlatFieldDoubleBytes = 0;
	int totalFlatFieldRefBytes = 0;
	int totalFlatFieldSingleBytes = 0;
	int flatAlignedObjectInstanceBackfill = 0;
	int flatAlignedSingleInstanceBackfill = 0;
	int flatUnAlignedObjectInstanceBackfill = 0;
	int flatUnAlignedSingleInstanceBackfill = 0;
	boolean classRequiresPrePadding = false;
	boolean isBackFillPostPadded = false;

	public static final int NO_BACKFILL_AVAILABLE = -1;
	public static final int BACKFILL_SIZE = U32.SIZEOF;
	public static final int LOCKWORD_SIZE = j9objectmonitor_t_SizeOf;
	public static final int FINALIZE_LINK_SIZE = fj9object_t_SizeOf;
	private static final int OBJECT_SIZE_INCREMENT_IN_BYTES = U64.SIZEOF;

	ObjectFieldInfo(J9ROMClassPointer romClass) {
		instanceObjectCount = 0;
		instanceSingleCount = 0;
		instanceDoubleCount = 0;
		totalObjectCount = 0;
		totalSingleCount = 0;
		totalDoubleCount = 0;
		hiddenFieldCount = 0;
		this.romClass = romClass;
		superclassFieldsSize = -1;
		superclassBackfillOffset = NO_BACKFILL_AVAILABLE;
		myBackfillOffset = NO_BACKFILL_AVAILABLE;
		subclassBackfillOffset = NO_BACKFILL_AVAILABLE;
		objectCanUseBackfill = (fj9object_t_SizeOf == BACKFILL_SIZE);
		instanceFieldBackfillEligible = false;
	}

	ObjectFieldInfo(J9ROMClassPointer romClass, J9ClassPointer clazz) throws CorruptDataException {
		this(romClass);
		containerClazz = clazz;
		isValue = valueTypeHelper.isRomClassAValueType(romClass);
		totalFlatFieldDoubleBytes = 0;
		totalFlatFieldRefBytes = 0;
		totalFlatFieldSingleBytes = 0;
	}

	int getTotalDoubleCount() {
		return totalDoubleCount;
	}

	int getTotalObjectCount() {
		return totalObjectCount;
	}

	int getTotalSingleCount() {
		return totalSingleCount;
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
	 * @return number of object fields (instance and hidden) which do not go  in a backfill slot.
	 */
	int getNonBackfilledObjectCount() {
		int nonBackfilledObjects = totalObjectCount;
		if (isBackfillSuitableObjectAvailable()  
			&& !isBackfillSuitableInstanceSingleAvailable()
			&& isMyBackfillSlotAvailable()
			&& (0 != nonBackfilledObjects)
		) {
			nonBackfilledObjects -= 1;
		}
		return nonBackfilledObjects;
	}

	int getNonBackfilledSingleCount() {
		int nonBackfilledSingle = totalSingleCount;
		if (isBackfillSuitableSingleAvailable() 
			&& isMyBackfillSlotAvailable()
			&& (0 != nonBackfilledSingle)
		) {
			nonBackfilledSingle -= 1;
		}
		return nonBackfilledSingle;
	}

	int getNonBackfilledInstanceObjectCount() {
		int nonBackfilledObjects = instanceObjectCount;
		if (isBackfillSuitableInstanceObjectAvailable()
			&& !isBackfillSuitableInstanceSingleAvailable()
			&& isMyBackfillSlotAvailable()
			&& (0 != nonBackfilledObjects)
		) {
			nonBackfilledObjects -= 1;
		}
		return nonBackfilledObjects;
	}

	int getNonBackfilledInstanceSingleCount() {
		int nonBackfilledSingle = instanceSingleCount;
		if (isBackfillSuitableInstanceSingleAvailable() 
			&& isMyBackfillSlotAvailable()
			&& (0 != nonBackfilledSingle)
		) {
			nonBackfilledSingle -= 1;
		}
		return nonBackfilledSingle;
	}

	int getInstanceDoubleCount() {
		return instanceDoubleCount;
	}

	int getInstanceObjectCount() {
		return instanceObjectCount;
	}

	int getInstanceSingleCount() {
		return instanceSingleCount;
	}

	int getHiddenFieldCount() {
		return hiddenFieldCount;
	}

	boolean
	isBackfillSuitableSingleAvailable()
	{
		return ((0 != getTotalSingleCount())
				|| (0 != getFlatAlignedSingleInstanceBackfillSize())
				|| (0 != getFlatUnAlignedSingleInstanceBackfillSize())
		);
	}

	boolean
	isBackfillSuitableObjectAvailable()
	{
		return ((objectCanUseBackfill && (0 != getTotalObjectCount()))
				|| (0 != getFlatAlignedObjectInstanceBackfillSize())
				|| (0 != getFlatUnAlignedObjectInstanceBackfillSize()));
	}

	boolean
	isBackfillSuitableInstanceSingleAvailable()
	{
		return ((0 != getInstanceSingleCount())
				|| (0 != getFlatAlignedSingleInstanceBackfillSize())
				|| (0 != getFlatUnAlignedSingleInstanceBackfillSize()));
	}

	boolean
	isBackfillSuitableInstanceObjectAvailable()
	{
		return ((objectCanUseBackfill && (0 != getInstanceObjectCount()))
				|| (0 != getFlatAlignedObjectInstanceBackfillSize())
				|| (0 != getFlatUnAlignedObjectInstanceBackfillSize()));
	}
	
	boolean isBackfillSuitableFieldAvailable() {
		return isBackfillSuitableSingleAvailable() || isBackfillSuitableObjectAvailable();
	}
	
	/**
	 * @note This is used for hidden fields which use an offset from the header
	 * @return offset of backfill slot from the start of the object
	 */
	UDATA getMyBackfillOffsetForHiddenField() {
		return new UDATA(myBackfillOffset + J9ObjectHelper.headerSize());
	}

	int getSuperclassFieldsSize() {
		return superclassFieldsSize;
	}

	long getSuperclassObjectSize() {
		return superclassFieldsSize + J9ObjectHelper.headerSize();
	}

	void
	setSuperclassFieldsSize(int i)
	{
		this.superclassFieldsSize = i;
	}

	boolean isMyBackfillSlotAvailable() {
		return (myBackfillOffset >= 0);
	}

	boolean isSuperclassBackfillSlotAvailable() {
		return (superclassBackfillOffset >= 0);
	}


	/**
	 * Fields can start on the first 8-byte boundary after the end of the superclass.
	 * If the superclass is not end-aligned, the padding bytes become the new backfill.
	 * Since the superclass is not end-aligned it must have no embedded backfill.
	 * @return offset to the first (non-backfilled) field.
	 */
	int calculateFieldDataStart() {
		int fieldDataStart = getSuperclassFieldsSize();
		
		/* If the type has double field the doubles need to start on an 8-byte boundary. In the case of valuetypes
		 * there is no lockword so the super class field size will be zero. This means that valueTypes in compressedrefs
		 * mode with double alignment fields need to be pre-padded.
		 */
		boolean doubleAlignment = (totalDoubleCount > 0) || (totalFlatFieldDoubleBytes > 0);

		if (
			((getSuperclassObjectSize() % OBJECT_SIZE_INCREMENT_IN_BYTES) != 0) && /* superclass is not end-aligned */
			(doubleAlignment || (!objectCanUseBackfill && (totalObjectCount > 0)))
		) { /* our fields start on a 8-byte boundary */
			fieldDataStart += getBackfillSize();
			/* if a flat type is used up as backfill and its size is not 4, 12, 20, ... this
			 * means that the starting point for doubles will not be on an 8byte boundary. To
			 * fix this the field start position will be incremented by 4bytes so doubles start
			 * at the correct position.
			 */
			if (!isBackfillTypeEndAligned(fieldDataStart)) {
				fieldDataStart += BACKFILL_SIZE;
				isBackFillPostPadded = true;
			}
		}
		return fieldDataStart;
	}

	/**
	 * @note Always uses the number of doubles
	 * @param start end of previous field area
	 * @return offset to end of the doubles area
	 */
	int addDoublesArea(int start) {
		return start + (totalDoubleCount * U64.SIZEOF);
	}

	/**
	 * @param start end of previous field area
	 * @return offset to end of the objects area
	 * @note takes into account objects which will go in the backfill
	 */
	int addObjectsArea(int start) {
		int nonBackfilledObjects = getNonBackfilledObjectCount();
		return start + (nonBackfilledObjects * fj9object_t_SizeOf);
	}

	/**
	 * @note This is used for instance fields.
	 * The backfill slot can either be embedded within the superclass,
	 * or it can be the result of padding the superclass.
	 * If there is no embedded backfile (i.e. backfillOffset == -1, then see if there is space available
	 * at the end of the superclass.
	 * @return backfill offset from the first field
	 */
	int getMyBackfillOffset() {
		return myBackfillOffset;
	}
	
	/**
	 *
	 * Returns the size of backfill. The order or precedence is:
	 * 1. instance singles
	 * 2. instance objects
	 * 3. flat end-aligned singles
	 * 4. flat end-aligned objects
	 * 5. flat end-unaligned singles
	 * 6. flat end-unaligned objects
	 * 7. hidden singles
	 * 8. hidden objects
	 *
	 * @return size of backfill
	 */
	int
	getBackfillSize()
	{
		int backFillSize = BACKFILL_SIZE;

		if ((0 != getInstanceSingleCount()) || (0 != getInstanceObjectCount())) {
			/* BACKFILL_SIZE */
		} else if (0 != getFlatAlignedSingleInstanceBackfillSize()) {
			backFillSize = getFlatAlignedSingleInstanceBackfillSize();
		} else if (0 != getFlatAlignedObjectInstanceBackfillSize()) {
			backFillSize = getFlatAlignedObjectInstanceBackfillSize();
		} else if (0 != getFlatUnAlignedSingleInstanceBackfillSize()) {
			backFillSize = getFlatUnAlignedSingleInstanceBackfillSize();
		} else if (0 != getFlatUnAlignedObjectInstanceBackfillSize()) {
			backFillSize = getFlatUnAlignedObjectInstanceBackfillSize();
		}
		
		return backFillSize;
	}

	int getSubclassBackfillOffset() {
		return subclassBackfillOffset;
	}

	void
	setSuperclassBackfillOffset(int superclassBackfillOffset)
	{
		this.superclassBackfillOffset = superclassBackfillOffset;
	}

	int getSuperclassBackfillOffset() {
		return superclassBackfillOffset;
	}

	boolean isInstanceFieldBackfillEligible() {
		return instanceFieldBackfillEligible;
	}

	void
	countInstanceFields() throws CorruptDataException
	{
		/* iterate over fields to count instance fields by size */

		Iterable <J9ROMFieldShapePointer>  fields = new J9ROMFieldShapeIterator(romClass.romFields(), romClass.romFieldCount());
		for  (J9ROMFieldShapePointer f: fields) {
			UDATA modifiers = f.modifiers();
			if (!modifiers.anyBitsIn(J9AccStatic) ) {
				if (modifiers.anyBitsIn(J9FieldFlagObject)) {
					if (valueTypeHelper.isFlattenableFieldSignature(J9ROMFieldShapeHelper.getSignature(f))) {
						int size = 0;
						J9ClassPointer fieldClass = valueTypeHelper.findJ9ClassInFlattenedClassCacheWithFieldName(containerClazz, J9ROMFieldShapeHelper.getName(f));
						if (!valueTypeHelper.isJ9ClassIsFlattened(fieldClass)) {
							instanceObjectCount += 1;
							totalObjectCount += 1;
						} else if (valueTypeHelper.isJ9ClassLargestAlignmentConstraintDouble(fieldClass)) {
							UDATA doubleSize = fieldClass.totalInstanceSize();
							if (valueTypeHelper.classRequires4BytePrePadding(fieldClass)) {
								doubleSize = doubleSize.sub(U32.SIZEOF);
							}
							size = Scalar.roundToSizeofU64(doubleSize).intValue();
							totalFlatFieldDoubleBytes += size;
						} else if (valueTypeHelper.isJ9ClassLargestAlignmentConstraintReference(fieldClass)) {
							size = Scalar.roundToSizeToFJ9object(fieldClass.totalInstanceSize()).intValue();
							totalFlatFieldRefBytes += size;
							setPotentialFlatObjectInstanceBackfill(size);
						} else {
							size = fieldClass.totalInstanceSize().intValue();
							totalFlatFieldSingleBytes += size;
							setPotentialFlatSingleInstanceBackfill(size);
						}
					} else {
						instanceObjectCount += 1;
						totalObjectCount += 1;
					}
				} else if (modifiers.anyBitsIn(J9FieldSizeDouble)) {
					instanceDoubleCount += 1;
					totalDoubleCount += 1;
				} else {
					instanceSingleCount += 1;
					totalSingleCount += 1;
				}
			}
		}
		instanceFieldBackfillEligible = (instanceSingleCount > 0) || (objectCanUseBackfill && (instanceSingleCount > 0));
	}

	int countAndCopyHiddenFields(LinkedList<HiddenInstanceField> hiddenFieldList, ArrayList<HiddenInstanceField>  hiddenFieldArray) throws CorruptDataException
	{

		String className = J9UTF8Helper.stringValue(romClass.className());
		hiddenFieldCount = 0;
		for (HiddenInstanceField hiddenField : hiddenFieldList) {
			if (hiddenField.className() == null || className.equals(hiddenField.className())) {
				UDATA modifiers = hiddenField.shape().modifiers();
				if (modifiers.anyBitsIn(J9FieldFlagObject)) {
					totalObjectCount += 1;
				} else if (modifiers.anyBitsIn(J9FieldSizeDouble)) {
					totalDoubleCount += 1;
				} else {
					totalSingleCount += 1;
				}
				hiddenFieldArray.add(hiddenField);
				hiddenFieldCount += 1;
			}
		}
		return hiddenFieldCount;
	}

	int calculateTotalFieldsSizeAndBackfill() { /* TODO update to handle contended fields */
		long accumulator = superclassFieldsSize + (totalObjectCount * J9ObjectHelper.headerSize()) + (totalSingleCount * U32.SIZEOF)
				+ (totalDoubleCount * U64.SIZEOF);

		accumulator += totalFlatFieldDoubleBytes + totalFlatFieldRefBytes + totalFlatFieldSingleBytes;

		/* ValueTypes cannot be subtyped and their superClass contains no fields */
		if (isValue) {
			/* If the first field does not start at zero, this means we added padding to satisfy
			 * alignment requirements of double slot fields. We will use the J9ClassHasPrePadding flag
			 * to denote this.
			 */
			int firstFieldOffset = calculateFieldDataStart();
			if (0 < firstFieldOffset) {
				if (isBackfillSuitableFieldAvailable()) {
					/*
					 * If the class is a valuetype and it has at least one single or object aligned field, instead of pre-padding
					 * the field with the smaller alignment constraint will be placed first.
					 */
					myBackfillOffset = 0;
					
					/*
					 * Backfill may not be end-aligned, ie 8, 16, 24btye types. In these cases the types
					 * are post-padded so doubles can start on a 8byte boundary.
					 */
					if (isBackFillPostPadded()) {
						accumulator += BACKFILL_SIZE;
					}
				} else {
					/* add pre-padding */
					accumulator += firstFieldOffset;
					setClassRequiresPrePadding();
				}
			}
		} else {
			if (((getSuperclassObjectSize() % OBJECT_SIZE_INCREMENT_IN_BYTES) != 0)
					&& /* superclass is not end-aligned */
					((totalDoubleCount > 0) || (!objectCanUseBackfill
							&& (totalObjectCount > 0)))) { /* our fields start on a 8-byte boundary */
				superclassBackfillOffset = getSuperclassFieldsSize();
				accumulator += BACKFILL_SIZE;
			}
			if (isSuperclassBackfillSlotAvailable() && isBackfillSuitableFieldAvailable()) {
				accumulator -= BACKFILL_SIZE;
				myBackfillOffset = superclassBackfillOffset;
				superclassBackfillOffset = NO_BACKFILL_AVAILABLE;
			}
			if (((accumulator + J9ObjectHelper.headerSize()) % OBJECT_SIZE_INCREMENT_IN_BYTES) != 0) {
				/* we have consumed the superclass's backfill (if any), so let our subclass use the residue at the end of this class. */
				subclassBackfillOffset = (int)accumulator;
				accumulator += BACKFILL_SIZE;
			} else {
				subclassBackfillOffset = superclassBackfillOffset;
			}
		}
		return (int) accumulator;
	}

	/**
	 * @param start end of previous field area, which should be the first field area
	 * @return offset to end of the flat doubles area
	 */
	int
	addFlatDoublesArea(int start)
	{
		return start + totalFlatFieldDoubleBytes;
	}

	/**
	 * @param start end of previous field area, which should be after doubles
	 * @return offset to end of the flat doubles area
	 */
	int
	addFlatObjectsArea(int start)
	{
		return start + getNonBackfilledFlatInstanceObjectSize();
	}

	/**
	 * @param start end of previous field area, which should be after objects
	 * @return offset to end of the flat doubles area
	 */
	int
	addFlatSinglesArea(int start)
	{
		return start + getNonBackfilledFlatInstanceSingleSize();
	}
	
	int
	getFlatAlignedSingleInstanceBackfillSize()
	{
		return flatAlignedSingleInstanceBackfill;
	}

	int
	getFlatUnAlignedSingleInstanceBackfillSize()
	{
		return flatUnAlignedSingleInstanceBackfill;
	}

	int
	getFlatAlignedObjectInstanceBackfillSize()
	{
		return flatAlignedObjectInstanceBackfill;
	}

	int
	getFlatUnAlignedObjectInstanceBackfillSize()
	{
		return flatUnAlignedObjectInstanceBackfill;
	}

	void
	setFlatAlignedSingleInstanceBackfill(int size)
	{
		flatAlignedSingleInstanceBackfill = size;
	}

	void
	setFlatUnAlignedSingleInstanceBackfill(int size)
	{
		flatUnAlignedSingleInstanceBackfill = size;
	}

	void
	setFlatAlignedObjectInstanceBackfill(int size)
	{
		flatAlignedObjectInstanceBackfill = size;
	}

	void
	setFlatUnAlignedObjectInstanceBackfill(int size)
	{
		flatUnAlignedObjectInstanceBackfill = size;
	}

	void
	setPotentialFlatObjectInstanceBackfill(int size)
	{
		if (isValue) {
			if (U32.SIZEOF == (size % U64.SIZEOF)) {
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

	void
	setPotentialFlatSingleInstanceBackfill(int size)
	{
		if (isValue) {
			if (U32.SIZEOF == (size % U64.SIZEOF)) {
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

	int
	getNonBackfilledFlatInstanceObjectSize()
	{
		int nonBackfilledFlatObjectsSize = totalFlatFieldRefBytes;
		if (isBackfillSuitableInstanceObjectAvailable()
			&& isMyBackfillSlotAvailable()
			&& (0 == getInstanceSingleCount())
			&& (objectCanUseBackfill && (0 == getInstanceObjectCount()))
			&& (0 == getFlatAlignedSingleInstanceBackfillSize())
		) {
			int backfillSize = getFlatAlignedObjectInstanceBackfillSize();
			if (0 == backfillSize) {
				if (0 == getFlatUnAlignedSingleInstanceBackfillSize()) {
					backfillSize = getFlatUnAlignedObjectInstanceBackfillSize();
				}
			}
			nonBackfilledFlatObjectsSize -= backfillSize;
		}
		return nonBackfilledFlatObjectsSize;
	}

	int
	getNonBackfilledFlatInstanceSingleSize()
	{
		int nonBackfilledFlatSinglesSize = totalFlatFieldSingleBytes;
		if (isBackfillSuitableInstanceSingleAvailable()
			&& isMyBackfillSlotAvailable()
			&& (0 == getInstanceSingleCount())
			&& (objectCanUseBackfill && (0 == getInstanceObjectCount()))
		) {
			int backfillSize = getFlatAlignedSingleInstanceBackfillSize();
			if (0 == backfillSize) {
				if (0 == getFlatAlignedObjectInstanceBackfillSize()) {
					backfillSize = getFlatUnAlignedSingleInstanceBackfillSize();
				}
			}
			nonBackfilledFlatSinglesSize -= backfillSize;
		}
		return nonBackfilledFlatSinglesSize;
	}

	boolean
	doesClassRequiresPrePadding()
	{
		return classRequiresPrePadding;
	}

	void
	setClassRequiresPrePadding()
	{
		classRequiresPrePadding = true;
	}
	
	boolean
	isBackfillTypeEndAligned(int fieldDataStart) {
		return BACKFILL_SIZE == (fieldDataStart % U64.SIZEOF);
	}
	
	boolean
	isBackFillPostPadded()
	{
		return isBackFillPostPadded;
	}
}
