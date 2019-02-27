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
package com.ibm.j9ddr.vm29.j9.gc;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ObjectHash;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.GC_ObjectModelPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

import static com.ibm.j9ddr.vm29.structure.J9Consts.*;
import static com.ibm.j9ddr.vm29.structure.J9Object.*;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.*;
import static com.ibm.j9ddr.vm29.structure.GC_ObjectModel$ScanType.*;

class GCObjectModel_V1 extends GCObjectModel
{
	private J9JavaVMPointer vm;
	private J9ClassPointer classClass;
	private J9ClassPointer classLoaderClass;
	private J9ClassPointer atomicMarkableReferenceClass;
	private VoidPointer tenureBase;
	private UDATA tenureSize;
	
	// TODO : these will be real soon
	private static final long OBJECT_HEADER_REMEMBERED_BITS;
//	private static final long OBJECT_HEADER_REMEMBERED_BITS_SHIFT;
//	private static final long STATE_NOT_REMEMBERED;
	private static final long STATE_REMEMBERED;
	
	/*
	 * Object alignment in the heap was 8 for years but
	 * now we are supporting run-time value for it to have selection between 8 and 16
	 * to support 4-bit compressed references.
	 * However DDR should have ability to handle old cores
	 * so we need hard coded value for object alignment 
	 */
	private static final long OBSOLETE_OBJECT_ALIGNMENT_IN_BYTES = 8;
	
	static 
	{
		if (J9BuildFlags.gc_arraylets) {
			OBJECT_HEADER_REMEMBERED_BITS = OBJECT_HEADER_AGE_MASK & ~J9_GC_ARRAYLET_LAYOUT_MASK;
		} else {
			OBJECT_HEADER_REMEMBERED_BITS = OBJECT_HEADER_AGE_MASK;
		}
//		OBJECT_HEADER_REMEMBERED_BITS_SHIFT = OBJECT_HEADER_AGE_SHIFT;
//		STATE_NOT_REMEMBERED = 0;
		STATE_REMEMBERED = J9_OBJECT_HEADER_REMEMBERED_BITS_TO_SET & OBJECT_HEADER_REMEMBERED_BITS;
	}
	
	/* Do not instantiate. Use the factory */
	protected GCObjectModel_V1() throws CorruptDataException 
	{
		GC_ObjectModelPointer imageObjectModel = getExtensions().objectModel();
		vm = GCBase.getJavaVM();
		classClass = imageObjectModel._classClass();
		classLoaderClass = imageObjectModel._classLoaderClass();
		atomicMarkableReferenceClass = imageObjectModel._atomicMarkableReferenceClass();
		tenureBase = getExtensions()._tenureBase();
		tenureSize = getExtensions()._tenureSize();
	}

	@Override
	public U32 getAge(J9ObjectPointer object) throws CorruptDataException 
	{
		return J9ObjectHelper.flags(object).bitAnd(OBJECT_HEADER_AGE_MASK).rightShift((int)OBJECT_HEADER_AGE_SHIFT);
	}

	@Override
	public UDATA getConsumedSizeInBytesWithHeader(J9ObjectPointer object) throws CorruptDataException 
	{
		UDATA size = getSizeInBytesWithHeader(object);
		if (hasBeenMoved(object)) {
			if (getHashcodeOffset(object).eq(size)) {
				size = size.add(UDATA.SIZEOF);
			}
		}
		return adjustSizeInBytes(size);
	}

	@Override
	public UDATA getClassShape(J9ObjectPointer object) throws CorruptDataException 
	{
		return getClassShape(J9ObjectHelper.clazz(object));
	}

	@Override
	public UDATA getSizeInBytesDeadObject(J9ObjectPointer object) throws CorruptDataException 
	{
		if (isSingleSlotDeadObject(object)) {
			return getSizeInBytesSingleSlotDeadObject(object);
		}
		return getSizeInBytesMultiSlotDeadObject(object);
	}

	@Override
	public UDATA getSizeInBytesMultiSlotDeadObject(J9ObjectPointer object) throws CorruptDataException 
	{
		return GCHeapLinkedFreeHeader.fromJ9Object(object).getSize();
	}

	@Override
	public UDATA getSizeInBytesSingleSlotDeadObject(J9ObjectPointer object) 
	{
		// TODO : this is sort of bogus
		return new UDATA(UDATA.SIZEOF);
	}

	@Override
	public long getObjectAlignmentInBytes() 
	{
		long result = 0;
		try {
			result = vm.omrVM()._objectAlignmentInBytes().longValue();
			/*
			 * TODO: Currently zero means transitional case (slot has been added but not used)
			 * However there is another possible case that core is generated very early at Java
			 * startup time and slot has not been set properly yet
			 * To handle this we might need to initialize this slot to UDATA_MAX as soon as J9JavaVM structure is created
			 * to recognize as special case here 
			 */
			if (0 == result) {
				/* slot in J9JavaVM exist, but it is 0 - assume this is a transitional core */
				result = OBSOLETE_OBJECT_ALIGNMENT_IN_BYTES;
			}
		} catch (NoSuchFieldError e) {
			/* slot in J9JavaVM does not exist, assume this is old core */
			result = OBSOLETE_OBJECT_ALIGNMENT_IN_BYTES;
		} catch (CorruptDataException e) {
			/* slot in J9JavaVM does not exist, assume this is old core */
			result = OBSOLETE_OBJECT_ALIGNMENT_IN_BYTES;
		}
		return result;
	}

@Override
	public UDATA getSizeInBytesWithHeader(J9ObjectPointer object) throws CorruptDataException 
	{
		if (isIndexable(object)) {
			return indexableObjectModel.getSizeInBytesWithHeader(J9IndexableObjectPointer.cast(object));
		} else {
			return mixedObjectModel.getSizeInBytesWithHeader(object);
		}
	}

@Override
public UDATA getTotalFootprintInBytesWithHeader(J9ObjectPointer object) throws CorruptDataException {
	if (isIndexable(object)) {
		return adjustSizeInBytes(indexableObjectModel.getTotalFootprintInBytesWithHeader(J9IndexableObjectPointer.cast(object)));
	} else {
		return adjustSizeInBytes(mixedObjectModel.getSizeInBytesWithHeader(object));
	}
}

@Override
	public boolean isDeadObject(J9ObjectPointer object) throws CorruptDataException 
	{
		return J9ObjectHelper.flags(object).allBitsIn(J9_GC_OBJ_HEAP_HOLE);
	}

	@Override
	public boolean isIndexable(J9ObjectPointer object) throws CorruptDataException 
	{
		return isIndexable(J9ObjectHelper.clazz(object));
	}
	
	@Override
	public boolean isIndexable(J9ClassPointer clazz) throws CorruptDataException
	{
		return J9ClassHelper.isArrayClass(clazz);
	}

	@Override
	public boolean isOld(J9ObjectPointer object) throws CorruptDataException 
	{
		VoidPointer topOfTenure = tenureBase.addOffset(tenureSize);
		return object.gte(tenureBase) && object.lt(topOfTenure); 
	}

	@Override
	public boolean isRemembered(J9ObjectPointer object) throws CorruptDataException
	{
		//return object.flags().allBitsIn(OBJECT_HEADER_REMEMBERED);
		return getRememberedBits(object).gte(new UDATA(STATE_REMEMBERED));
	}

	@Override
	public boolean isSingleSlotDeadObject(J9ObjectPointer object) throws CorruptDataException 
	{
		return J9ObjectHelper.flags(object).bitAnd(J9_GC_OBJ_HEAP_HOLE_MASK).eq(J9_GC_SINGLE_SLOT_HOLE);
	}

	@Override
	public UDATA adjustSizeInBytes(UDATA sizeInBytes)
	{
		long bytes = sizeInBytes.longValue();
		if (!J9BuildFlags.env_data64 || (J9BuildFlags.gc_compressedPointers && J9BuildFlags.gc_arraylets)) {		
			bytes = (bytes + (ObjectModel.getObjectAlignmentInBytes() - 1)) & ~(ObjectModel.getObjectAlignmentInBytes() - 1);
		}

		if (J9BuildFlags.gc_minimumObjectSize) {
			if (bytes < J9_GC_MINIMUM_OBJECT_SIZE) {
				bytes = J9_GC_MINIMUM_OBJECT_SIZE;
			}
		}

		return new UDATA(bytes);
	}

	@Override
	public UDATA getSizeInElements(J9ObjectPointer object) throws IllegalArgumentException, CorruptDataException
	{
		if (isIndexable(object)) {
			return indexableObjectModel.getSizeInElements(J9IndexableObjectPointer.cast(object));
		} else {
			throw new IllegalArgumentException("this API is only valid on indexable objects");
		}
	}

	@Override
	public UDATA getRememberedBits(J9ObjectPointer object) throws CorruptDataException
	{
		return J9ObjectHelper.flags(object).bitAnd(new UDATA(OBJECT_HEADER_REMEMBERED_BITS));
	}

	@Override
	public long getScanType(J9ObjectPointer object) throws CorruptDataException
	{
		long result;
		J9ClassPointer clazz = J9ObjectHelper.clazz(object);
		return getScanType(clazz);
	}

	private long getScanType(J9ClassPointer clazz) throws CorruptDataException 
	{
		long result;
		long shape = getClassShape(clazz).longValue();
		
		if (shape == OBJECT_HEADER_SHAPE_MIXED) {
			if (J9ClassHelper.classFlags(clazz).anyBitsIn(J9AccClassReferenceMask)) {
				result = SCAN_REFERENCE_MIXED_OBJECT;
			} else if (J9ClassHelper.classFlags(clazz).anyBitsIn(J9AccClassGCSpecial)) {
				result = getSpecialClassScanType(clazz);
			} else {
				result = SCAN_MIXED_OBJECT;
			}
		} else if (shape == OBJECT_HEADER_SHAPE_POINTERS) {
			result = SCAN_POINTER_ARRAY_OBJECT;
		} else if (
			(shape == OBJECT_HEADER_SHAPE_DOUBLES)
			|| (shape == OBJECT_HEADER_SHAPE_BYTES)
			|| (shape == OBJECT_HEADER_SHAPE_WORDS)
			|| (shape == OBJECT_HEADER_SHAPE_LONGS)) 
		{
			/* Must be a primitive array*/
			result = SCAN_PRIMITIVE_ARRAY_OBJECT;
		} else {
			result = SCAN_INVALID_OBJECT;
		}
		return result;
	}

	private long getSpecialClassScanType(J9ClassPointer clazz) throws CorruptDataException
	{
		long result = SCAN_MIXED_OBJECT;
		
		if (clazz.eq(classClass)) {
			result = SCAN_CLASS_OBJECT;
		} else if (classLoaderClass.notNull() && J9ClassHelper.isSameOrSuperClassOf(classLoaderClass, clazz)) {
			result = SCAN_CLASSLOADER_OBJECT;
		} else if (atomicMarkableReferenceClass.notNull() && J9ClassHelper.isSameOrSuperClassOf(atomicMarkableReferenceClass, clazz)) {
			result = SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT;
		} else {
			result = SCAN_INVALID_OBJECT;
		}
		return result;
	}

	@Override
	public I32 getObjectHashCode(J9ObjectPointer object) throws CorruptDataException
	{
		I32 result = new I32(0);
		
		if (J9BuildFlags.gc_modronCompaction || J9BuildFlags.gc_generational) {
			if (hasBeenMoved(object)) {
				result = I32Pointer.cast(object).addOffset(getHashcodeOffset(object).longValue()).at(0);
			} else {
				result = ObjectHash.convertObjectAddressToHash(vm, object);
			}
		} else {
			result = ObjectHash.convertObjectAddressToHash(vm, object);
		}
		return result;
	}


	@Override
	public boolean hasBeenHashed(J9ObjectPointer object) throws CorruptDataException
	{
		if (J9BuildFlags.interp_flagsInClassSlot) {
			return J9ObjectHelper.flags(object).allBitsIn(OBJECT_HEADER_HAS_BEEN_HASHED_MASK_IN_CLASS);
		} else {
			return J9ObjectHelper.flags(object).allBitsIn(OBJECT_HEADER_HAS_BEEN_HASHED);
		}
	}

	@Override
	public boolean hasBeenMoved(J9ObjectPointer object) throws CorruptDataException
	{
		if (J9BuildFlags.interp_flagsInClassSlot) {
			return J9ObjectHelper.flags(object).allBitsIn(OBJECT_HEADER_HAS_BEEN_MOVED_IN_CLASS);
		} else {
			return J9ObjectHelper.flags(object).allBitsIn(OBJECT_HEADER_HAS_BEEN_MOVED);
		}
	}

	@Override
	public UDATA getHashcodeOffset(J9ObjectPointer object) throws CorruptDataException
	{
		UDATA offset = new UDATA(0);
		if (isIndexable(object)) {
			offset = indexableObjectModel.getHashcodeOffset(J9IndexableObjectPointer.cast(object));
		} else {
			offset = mixedObjectModel.getHashcodeOffset(object);
		}
		return offset;
	}

	@Override
	public UDATA getHeaderSize(J9ObjectPointer object) throws CorruptDataException
	{
		if (isIndexable(object)) {
			return indexableObjectModel.getHeaderSize(J9IndexableObjectPointer.cast(object));
		} else {
			return mixedObjectModel.getHeaderSize(object);
		}
	}

	@Override
	public UDATA getClassShape(J9ClassPointer clazz) throws CorruptDataException
	{
		return new UDATA((J9ClassHelper.classFlags(clazz).longValue() >> J9AccClassRAMShapeShift) & OBJECT_HEADER_SHAPE_MASK);		
	}

	@Override
	public VoidPointer getElementAddress(J9IndexableObjectPointer indexableObjectPointer, int elementIndex, int elementSize) throws CorruptDataException
	{
		return indexableObjectModel.getElementAddress(indexableObjectPointer, elementIndex, elementSize);
	}

}
