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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;
import static com.ibm.j9ddr.vm29.j9.ObjectFieldInfo.BACKFILL_SIZE;
import static com.ibm.j9ddr.vm29.j9.ObjectFieldInfo.LOCKWORD_SIZE;
import static com.ibm.j9ddr.vm29.j9.ObjectFieldInfo.NO_BACKFILL_AVAILABLE;
import static com.ibm.j9ddr.vm29.j9.ObjectFieldInfo.fj9object_t_SizeOf;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagObject;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldSizeDouble;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.J9AccStatic;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC;
import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.AddressedCorruptDataException;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HiddenInstanceFieldPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.structure.J9Object;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Class that calculates offsets into an Object of fields.
 * Based on:
 * resolvefield.cpp
 * description.c
 */
public class J9ObjectFieldOffsetIterator_V1 extends J9ObjectFieldOffsetIterator {

	private J9JavaVMPointer vm;
	private J9ROMClassPointer romClass;
	private J9ClassPointer superClazz;
	private Iterator romFieldsShapeIterator;
	private J9ClassPointer instanceClass;

	private U32 doubleSeen = new U32(0);
	private U32 doubleStaticsSeen = new U32(0);
	private UDATA firstDoubleOffset = new UDATA(0);
	private UDATA firstSingleOffset = new UDATA(0);
	private UDATA firstObjectOffset = new UDATA(0);
	private U32 objectsSeen = new U32(0);
	private U32 objectStaticsSeen = new U32(0);
	private U32 singlesSeen = new U32(0);
	private U32 singleStaticsSeen = new U32(0);
	private U32 walkFlags = new U32(0);

	private J9ROMFieldShapePointer field;
	private UDATA index = new UDATA(0);
	private UDATA offset = new UDATA(0);
	private IDATA backfillOffsetToUse = new IDATA(-1);
	private UDATA lockwordNeeded = new UDATA(0);
	private UDATA lockOffset = new UDATA(0);

	private boolean isHidden;

	@SuppressWarnings("unused")
	private UDATA finalizeLinkOffset = new UDATA(0);
	private int hiddenInstanceFieldWalkIndex = -1;
	private ArrayList<HiddenInstanceField> hiddenInstanceFieldList = new ArrayList<HiddenInstanceField>();

	private static final UDATA NO_LOCKWORD_NEEDED = new UDATA(-1);
	private static final UDATA LOCKWORD_NEEDED = new UDATA(-2);

	private J9ObjectFieldOffset next;

	protected J9ObjectFieldOffsetIterator_V1(J9JavaVMPointer vm, J9ROMClassPointer romClass, J9ClassPointer clazz, J9ClassPointer superClazz, U32 flags) {
		this.vm = vm;
		this.romClass = romClass;
		this.instanceClass = clazz;
		this.superClazz = superClazz;
		this.walkFlags = flags;
		// Can't call init or set romClass in the constructor because they can throw a CorruptDataException
	}

	private void init() throws CorruptDataException {
		calculateInstanceSize(romClass, superClazz); //initInstanceSize(romClass, superClazz);
		romFieldsShapeIterator = new J9ROMFieldShapeIterator(romClass.romFields(), romClass.romFieldCount());
	}

	private void nextField() throws CorruptDataException {
		boolean walkHiddenFields = false;
		isHidden = false;
		field = null;

		/*
		 * Walk regular ROM fields until we run out of them. Then switch
		 * to hidden instance fields if the caller wants them.
		 */
		if (hiddenInstanceFieldWalkIndex == -1) {
			fieldOffsetsFindNext();
			if (field == null && walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN)) {
				walkHiddenFields = true;
				/* Start the walk over the hidden fields array in reverse order. */
				hiddenInstanceFieldWalkIndex = hiddenInstanceFieldList.size();
			}
		} else {
			walkHiddenFields = true;
		}

		if (walkHiddenFields && hiddenInstanceFieldWalkIndex != 0) {
			/* Note: hiddenInstanceFieldWalkIndex is the index of the last hidden instance field that was returned. */
			HiddenInstanceField hiddenField = hiddenInstanceFieldList.get(--hiddenInstanceFieldWalkIndex);

			field = hiddenField.shape();
			isHidden = true;
			/*
			 * This function returns offsets relative to the end of the object header,
			 * whereas fieldOffset is relative to the start of the header.
			 */
			offset = new UDATA(hiddenField.fieldOffset().intValue() - J9Object.SIZEOF);
			/* Hidden fields do not have a valid JVMTI index. */
			index = new UDATA(-1);
		}
	}

	// Based on fieldOffsetsFindNext from resolvefield.c
	private void fieldOffsetsFindNext() throws CorruptDataException {
		/* start walking the fields to find the first one to return */
		/* loop in case we have been told to only consider instance or static fields - we may have to skip past a few of the wrong kind */

		while (romFieldsShapeIterator.hasNext()) {
			J9ROMFieldShapePointer localField = (J9ROMFieldShapePointer) romFieldsShapeIterator.next();
			UDATA modifiers = localField.modifiers();

			/* count the index for jvmti */
			index = index.add(1);

			if (modifiers.anyBitsIn(J9AccStatic)) {
				if (walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC)) {
					/* found a static, the walk wants statics */
					if (modifiers.anyBitsIn(J9FieldFlagObject)) {
						offset = new UDATA(objectStaticsSeen.mult(UDATA.SIZEOF));
						objectStaticsSeen = objectStaticsSeen.add(1);
						field = localField;
						break;
					} else if (!(walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS))) {
						if (modifiers.anyBitsIn(J9FieldSizeDouble)) {
							/* Add single scalar and object counts together, round up to 2 and divide by 2 to get number of doubles used by singles */
							UDATA doubleSlots;
							if (J9BuildFlags.env_data64) {
								doubleSlots = new UDATA(romClass.objectStaticCount().add(romClass.singleScalarStaticCount()));
							} else {
								doubleSlots = new UDATA(romClass.objectStaticCount().add(romClass.singleScalarStaticCount()).add(1)).rightShift(1);
							}
							offset = doubleSlots.mult(U64.SIZEOF).add(doubleStaticsSeen.mult(U64.SIZEOF));
							doubleStaticsSeen = doubleStaticsSeen.add(1);
						} else {
							offset = new UDATA(romClass.objectStaticCount().mult(UDATA.SIZEOF).add(singleStaticsSeen.mult(UDATA.SIZEOF)));
							singleStaticsSeen = singleStaticsSeen.add(1);
						}
						field = localField;
						break;
					}
				}
			} else {
				if (walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE)) {
					if (modifiers.anyBitsIn(J9FieldFlagObject)) {
						if (walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD)) {
							// Assert_VM_true(state->backfillOffsetToUse >= 0);
							offset = new UDATA(backfillOffsetToUse);
							walkFlags = walkFlags.bitAnd(new U32(new UDATA(J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD).bitNot()));
						} else {
							offset = firstObjectOffset.add(objectsSeen.mult(fj9object_t_SizeOf));
							objectsSeen = objectsSeen.add(1);
						}
						field = localField;
						break;
					} else if (!walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS)) {
						if (modifiers.anyBitsIn(J9FieldSizeDouble)) {
							offset = firstDoubleOffset.add(doubleSeen.mult(U64.SIZEOF));
							doubleSeen = doubleSeen.add(1);
						} else {
							if (walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD)) {
								// Assert_VM_true(state->backfillOffsetToUse >= 0);
								offset = new UDATA(backfillOffsetToUse);
								walkFlags = walkFlags.bitAnd(new U32(new UDATA(J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD).bitNot()));
							} else {
								offset = firstSingleOffset.add(singlesSeen.mult(U32.SIZEOF));
								singlesSeen = singlesSeen.add(1);
							}
						}
						field = localField;
						break;
					}
				}
			}
		}
		// At this point all the info we need is stashed away in private state variables
		return;
	}

	// Based on fieldOffsetsStartDo in resolvefield.cpp

	private LinkedList<HiddenInstanceField> copyHiddenInstanceFieldsList(J9JavaVMPointer vm) throws CorruptDataException {
		LinkedList<HiddenInstanceField> list = new LinkedList<HiddenInstanceField>();
		J9HiddenInstanceFieldPointer fieldPointer = vm.hiddenInstanceFields();
		while (!fieldPointer.isNull()) {
			list.add(new HiddenInstanceField(fieldPointer));
			fieldPointer = fieldPointer.next();
		}
		return list;
	}

	private void calculateInstanceSize(J9ROMClassPointer romClass, J9ClassPointer superClazz) throws CorruptDataException {
		lockwordNeeded = NO_LOCKWORD_NEEDED;

		/* if we only care about statics we can skip all work related to instance size calculations */
		if (!walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE)) {
			return;
		}
		ObjectFieldInfo fieldInfo = new ObjectFieldInfo(romClass);

		/*
		 * Step 1: Calculate the size of the superclass and backfill offset.
		 * Inherit the instance size and backfillOffset from the superclass.
		 */
		if (superClazz.notNull()) {
			/*
			 * Note that in the J9Class, we do not store -1 to indicate no back fill,
			 * we store the total instance size (including the header) instead.
			 */
			fieldInfo.setSuperclassFieldsSize( superClazz.totalInstanceSize().intValue());
			if (!superClazz.backfillOffset().eq(superClazz.totalInstanceSize().add(J9Object.SIZEOF))) {
				fieldInfo.setSuperclassBackfillOffset(superClazz.backfillOffset().sub(J9Object.SIZEOF).intValue());
			}
		} else {
			fieldInfo.setSuperclassFieldsSize(0);
		}
		lockwordNeeded = checkLockwordNeeded(romClass, superClazz, instanceClass);
		/*
		 * remove the lockword from Object (if there is one) only if we don't need a lockword or we do need one
		 * and we are not re-using the one from Object which we can tell because lockwordNeeded is LOCKWORD_NEEDED as
		 * opposed to the value of the existing offset.
		 */
		if ((LOCKWORD_NEEDED.equals(lockwordNeeded))||(NO_LOCKWORD_NEEDED .equals(lockwordNeeded))) {
			if (superClazz.notNull() && !superClazz.lockOffset().eq(new UDATA(-1)) && J9ClassHelper.classDepth(superClazz).isZero()) {
				int newSuperSize = fieldInfo.getSuperclassFieldsSize() - LOCKWORD_SIZE;
				/*
				 * superClazz is java.lang.Object: subtract off non-inherited monitor field.
				 * Note that java.lang.Object's backfill slot can be only at the end.
				 */
				/* this may have been rounded to 8 bytes so also get rid of the padding */
				if (fieldInfo.isSuperclassBackfillSlotAvailable()) { /* j.l.Object was not end aligned */
					newSuperSize -= BACKFILL_SIZE;
					fieldInfo.setSuperclassBackfillOffset(NO_BACKFILL_AVAILABLE);
				}
				fieldInfo.setSuperclassFieldsSize(newSuperSize);
			}
		}

		/*
		 * Step 2: Determine which extra hidden fields we need and prepend them to the list of hidden fields.
		 */
		LinkedList<HiddenInstanceField> extraHiddenFields = copyHiddenInstanceFieldsList(vm);

		finalizeLinkOffset = new UDATA(0);
		if (!superClazz.isNull() && !superClazz.finalizeLinkOffset().isZero()) {
			/* Superclass is finalizeable */
			finalizeLinkOffset = superClazz.finalizeLinkOffset();
		} else {
			/*
			 * Superclass is not finalizable.
			 * We add the field if the class marked as needing finalization. In addition when HCR is enabled we also add the field if the
			 * the class is not the class for java.lang.Object (superclass name in java.lang.object isnull) and the class has a finalize
			 * method regardless of whether this method is empty or not. We need to do this because when HCR is enabled it is possible
			 * that the method will be re-transformed such that there finalization is needed and in that case
			 * we need the field to be in the shape of the existing objects
			 */

			/* Superclass is not finalizeable */
			if (J9ROMClassHelper.finalizeNeeded(romClass)) {
				extraHiddenFields.addFirst(new HiddenInstanceField(vm.hiddenFinalizeLinkFieldShape()));
			}
		}

		lockOffset = new UDATA(lockwordNeeded);
		if (lockOffset.eq(LOCKWORD_NEEDED)) {
			extraHiddenFields.addFirst(new HiddenInstanceField(vm.hiddenLockwordFieldShape()));
		}

		/*
		 * Step 3: Calculate the number of various categories of fields: single word primitive, double word primitive, and object references.
		 * Iterate over fields to count instance fields by size.
		 */

		fieldInfo.countInstanceFields();

		fieldInfo.countAndCopyHiddenFields(extraHiddenFields, hiddenInstanceFieldList);

		new UDATA(fieldInfo.calculateTotalFieldsSizeAndBackfill());
		firstDoubleOffset = new UDATA(fieldInfo.calculateFieldDataStart());
		firstObjectOffset = new UDATA(fieldInfo.addDoublesArea(firstDoubleOffset.intValue()));
		firstSingleOffset = new UDATA(fieldInfo.addObjectsArea(firstObjectOffset.intValue()));

		if (fieldInfo.isMyBackfillSlotAvailable() && fieldInfo.isBackfillSuitableFieldAvailable() ) {
			if (fieldInfo.isBackfillSuitableInstanceSingleAvailable()) {
				walkFlags = walkFlags.bitOr(J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD);
			} else if (fieldInfo.isBackfillSuitableInstanceObjectAvailable()) {
				walkFlags = walkFlags.bitOr(J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD);
			}
		}

		/*
		 * Calculate offsets (from the object header) for hidden fields. Hidden fields follow immediately the instance fields of the same type.
		 * Give instance fields priority for backfill slots.
		 * Note that the hidden fields remember their offsets, so this need be done once only.
		 */
		if (!hiddenInstanceFieldList.isEmpty()) {
			UDATA hiddenSingleOffset = firstSingleOffset.add(J9Object.SIZEOF + (fieldInfo.getNonBackfilledInstanceSingleCount() * U32.SIZEOF));
			UDATA hiddenDoubleOffset = firstDoubleOffset.add(J9Object.SIZEOF + (fieldInfo.getInstanceDoubleCount() * U64.SIZEOF));
			UDATA hiddenObjectOffset = firstObjectOffset.add(J9Object.SIZEOF + (fieldInfo.getNonBackfilledInstanceObjectCount() * fj9object_t_SizeOf));
			boolean useBackfillForObject = false;
			boolean useBackfillForSingle = false;

			if (fieldInfo.isMyBackfillSlotAvailable() && !walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD | J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD)) {
				/* There are no backfill-suitable instance fields, so let the hidden fields use the backfill slot */
				if (fieldInfo.isBackfillSuitableSingleAvailable()) {
					useBackfillForSingle = true;
				} else if (fieldInfo.isBackfillSuitableObjectAvailable()) {
					useBackfillForObject = true;
				}
			}

			for (HiddenInstanceField hiddenField: hiddenInstanceFieldList) {
				UDATA modifiers = hiddenField.shape().modifiers();

				if (modifiers.allBitsIn(J9FieldFlagObject)) {
					if (useBackfillForObject) {
						hiddenField.setFieldOffset(fieldInfo.getMyBackfillOffsetForHiddenField());
						useBackfillForObject = false;
					} else {
						hiddenField.setFieldOffset(hiddenObjectOffset);
						hiddenObjectOffset = hiddenObjectOffset.add(fj9object_t_SizeOf);
					}
				} else if (modifiers.allBitsIn(J9FieldSizeDouble)) {
					hiddenField.setFieldOffset(hiddenDoubleOffset);
					hiddenDoubleOffset = hiddenDoubleOffset.add(U64.SIZEOF);
				} else {
					if (useBackfillForSingle) {
						hiddenField.setFieldOffset(fieldInfo.getMyBackfillOffsetForHiddenField());
						useBackfillForSingle = false;
					} else {
						hiddenField.setFieldOffset(hiddenSingleOffset);
						hiddenSingleOffset = hiddenSingleOffset.add(U32.SIZEOF);
					}
				}

			}

		}
		backfillOffsetToUse = new IDATA(fieldInfo.getMyBackfillOffset()); /* backfill offset for this class's fields */
	}

	static PrintStream err = System.err;

	/**
	 * Determine if this class adds a lockword.
	 * If superclass does not need a lockword, then it re-use the super-superclass's lockword.
	 * This means the instance class allocates a new slot for its lockword.
	 * @param romClass
	 * @param ramSuperClass
	 * @param instanceClass
	 * @return NO_LOCKWORD_NEEDED if no lockword, lockword offset if it inherits a lockword,
	 * LOCKWORD_NEEDED if lockword needed and superclass does not have a lockword
	 * @throws CorruptDataException
	 */
	private UDATA checkLockwordNeeded(J9ROMClassPointer romClass, J9ClassPointer ramSuperClass,J9ClassPointer instanceClass) throws CorruptDataException {
		if (!J9BuildFlags.thr_lockNursery) {
			return NO_LOCKWORD_NEEDED;
		}

		J9ClassPointer ramClassForRomClass = instanceClass;
		while (!ramClassForRomClass.isNull() && (!romClass.equals(ramClassForRomClass.romClass()))) {
			ramClassForRomClass = J9ClassHelper.superclass(ramClassForRomClass);
		}
		// if the class does not have a lock offset then one was not needed
		if (ramClassForRomClass.lockOffset().eq(NO_LOCKWORD_NEEDED)) {
			return NO_LOCKWORD_NEEDED;
		}

		// if class inherited its lockword from its parent then we return the offset
		if ((ramSuperClass != null) && (!ramSuperClass.isNull()) && ramClassForRomClass.lockOffset().eq(ramSuperClass.lockOffset())) {
			return ramSuperClass.lockOffset();
		}

		// class has lockword and did not get it from its parent so a lockword was needed
		return LOCKWORD_NEEDED;
	}

	public boolean hasNext() {
		if (next != null) {
			return true;
		}

		next = getNext();

		return next != null;

//
//		if (romFieldsShapeIterator.hasNext()) {
//			return true;
//		}
//		if (walkFlags.anyBitsIn(J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN)) {
//			return (hiddenInstanceFieldWalkIndex != 0 && hiddenInstanceFields.size() != 0);
//		}
//		return false;
	}

	private J9ObjectFieldOffset getNext()
	{
		if (romFieldsShapeIterator == null) {
			try {
				init();
			} catch (CorruptDataException e) {
				// Tell anyone listening there was corrupt data.
				raiseCorruptDataEvent("CorruptDataException in com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator_V1.init()", e, false);
				return null;
			}
		}

		try {
			nextField();
			if (field == null) {
				return null;
			} else {
				return new J9ObjectFieldOffset(field, offset, isHidden);
			}
		} catch (AddressedCorruptDataException e) {
			// Returning null will stop the iteration.
			raiseCorruptDataEvent("AddressedCorruptDataException getting next field offset.", e, false);
			return null;
		} catch (CorruptDataException e) {
			// Returning null will stop the iteration.
			raiseCorruptDataEvent("CorruptDataException getting next field offset.", e, false);
			return null;
		}
	}

	public J9ObjectFieldOffset next() {
		if (!hasNext()) {
			throw new NoSuchElementException();
		}

		J9ObjectFieldOffset toReturn = next;
		next = null;
		return toReturn;
	}
}
