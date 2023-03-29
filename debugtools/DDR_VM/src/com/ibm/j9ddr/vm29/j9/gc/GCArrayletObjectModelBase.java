/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
package com.ibm.j9ddr.vm29.j9.gc;

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.GC_ArrayletObjectModelPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectContiguousPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9IndexableObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.structure.GC_ArrayletObjectModelBase$ArrayLayout;
import com.ibm.j9ddr.vm29.structure.J9IndexableObjectContiguous;
import com.ibm.j9ddr.vm29.structure.J9IndexableObjectDiscontiguous;
import com.ibm.j9ddr.vm29.structure.J9Object;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

public abstract class GCArrayletObjectModelBase extends GCArrayObjectModel
{
	protected GC_ArrayletObjectModelPointer arrayletObjectModel;
	protected VoidPointer arrayletRangeBase;
	protected VoidPointer arrayletRangeTop;
	protected UDATA largestDesirableArraySpineSize;
	protected UDATA arrayletLeafSize;
	protected UDATA arrayletLeafLogSize;
	protected UDATA arrayletLeafSizeMask;
	protected boolean enableDoubleMapping;
	protected boolean enableVirtualLargeObjectHeap;

	public GCArrayletObjectModelBase() throws CorruptDataException
	{
		arrayletObjectModel = GC_ArrayletObjectModelPointer.cast(GCBase.getExtensions().indexableObjectModel());
		arrayletRangeBase = arrayletObjectModel._arrayletRangeBase();
		arrayletRangeTop = arrayletObjectModel._arrayletRangeTop();
		largestDesirableArraySpineSize = arrayletObjectModel._largestDesirableArraySpineSize();
		J9JavaVMPointer vm = GCBase.getJavaVM();
		arrayletLeafSize = vm.arrayletLeafSize();
		arrayletLeafLogSize = vm.arrayletLeafLogSize();
		arrayletLeafSizeMask = arrayletLeafSize.sub(1);
		try {
			enableDoubleMapping = arrayletObjectModel._enableDoubleMapping();
		} catch (NoSuchFieldException e) {
			enableDoubleMapping = false;
		}
		try {
			enableVirtualLargeObjectHeap = arrayletObjectModel._enableVirtualLargeObjectHeap();
		} catch (NoSuchFieldException e) {
			enableVirtualLargeObjectHeap = false;
		}
	}

	/**
	 * Get the spine size for an arraylet with these properties
	 * @param J9Class The class of the array.
	 * @param layout The layout of the indexable object
	 * @param numberArraylets Number of arraylets for this indexable object
	 * @param dataSize How many elements are in the indexable object
	 * @param alignData Should the data section be aligned
	 * @return spineSize The actual size in byte of the spine
	 */
	protected UDATA getSpineSize(long layout, UDATA numberArraylets, UDATA dataSize, boolean alignData) throws CorruptDataException
	{
		UDATA headerSize = getHeaderSize(layout);
		UDATA spineSizeWithoutHeader = getSpineSizeWithoutHeader(layout, numberArraylets, dataSize, alignData);
		return spineSizeWithoutHeader.add(headerSize);
	}

	/**
	 * Returns the size of an indexable object in elements.
	 * @param array Pointer to the indexable object whose size is required
	 * @return Size of object in elements
	 * @throws CorruptDataException
	 */
	public UDATA getSizeInElements(J9IndexableObjectPointer array) throws CorruptDataException
	{
		U32 size = J9IndexableObjectHelper.size(array);
		if (size.anyBitsIn(0x80000000)) {
			throw new CorruptDataException("java array size with sign bit set");
		}
		return new UDATA(size);
	}

	/**
	 * Get the spine size without header for an arraylet with these properties
	 * @param layout The layout of the indexable object
	 * @param numberArraylets Number of arraylets for this indexable object
	 * @param dataSize How many elements are in the indexable object
	 * @param alignData Should the data section be aligned
	 * @return spineSize The actual size in byte of the spine
	 */
	protected UDATA getHeaderSize(long layout)
	{
		long headerSize;
		if (GC_ArrayletObjectModelBase$ArrayLayout.InlineContiguous != layout) {
			headerSize = J9IndexableObjectHelper.discontiguousHeaderSize();
		} else {
			headerSize = J9IndexableObjectHelper.contiguousHeaderSize();
		}
		return new UDATA(headerSize);
	}

	/**
	 * Returns the header size of a given indexable object. The arraylet layout is determined base on "small" size.
	 * @param array Ptr to an array for which header size will be returned
	 * @return Size of header in bytes
	 */
	@Override
	public UDATA getHeaderSize(J9IndexableObjectPointer array) throws CorruptDataException
	{
		long headerSize;
		UDATA size = J9IndexableObjectHelper.rawSize(array);
		if (size.eq(0)) {
			headerSize = J9IndexableObjectHelper.discontiguousHeaderSize();
		} else {
			headerSize = J9IndexableObjectHelper.contiguousHeaderSize();
		}
		return new UDATA(headerSize);
	}

	/**
	 * Get the spine size without header for an arraylet with these properties
	 * @param layout The layout of the indexable object
	 * @param numberArraylets Number of arraylets for this indexable object
	 * @param dataSize How many elements are in the indexable object
	 * @param alignData Should the data section be aligned
	 * @return spineSize The actual size in byte of the spine
	 */
	protected UDATA getSpineSizeWithoutHeader(long layout, UDATA numberArraylets, UDATA dataSize, boolean alignData) throws CorruptDataException
	{
		UDATA spineArrayoidSize = new UDATA(0);
		UDATA spinePaddingSize = new UDATA(0);

		/* The spine consists of three or four (possibly empty) sections, not including the header:
		 * 1. the alignment word - padding between arrayoid and inline-data
		 * 2. the arrayoid - an array of pointers to the leaves
		 * 3. in-line data
		 * 4. A secondary size field, for hybrid arrays only.
		 */
		if (GC_ArrayletObjectModelBase$ArrayLayout.InlineContiguous != layout) {
			if (!dataSize.eq(0)) {
				/* not in-line, so there in an arrayoid */
				if (alignData) {
					spinePaddingSize = new UDATA(ObjectModel.getObjectAlignmentInBytes() - ObjectReferencePointer.SIZEOF);
				}
				spineArrayoidSize = numberArraylets.mult(ObjectReferencePointer.SIZEOF);
			}
		}

		boolean isAllIndexableDataContiguousEnabled = enableDoubleMapping || enableVirtualLargeObjectHeap;
		
		UDATA spineDataSize = new UDATA(0);
		if (GC_ArrayletObjectModelBase$ArrayLayout.InlineContiguous == layout) {
			if (!isAllIndexableDataContiguousEnabled || isArrayletDataAdjacentToHeader(dataSize)) {
				spineDataSize = dataSize; // All data in spine
			}
		} else if (GC_ArrayletObjectModelBase$ArrayLayout.Hybrid == layout) {
			//TODO: lpnguyen put this pattern that appears everywhere into UDATA and think up a name, or just use mod?00
			spineDataSize = dataSize.bitAnd(arrayletLeafSizeMask); // Last arraylet in spine.
		}

		return spinePaddingSize.add(spineArrayoidSize).add(spineDataSize);
	}

	/**
	 * Get the spine size for the given indexable object
	 * @param objPtr Pointer to an array object
	 * @return The total size in bytes of objPtr's array spine;
	 *         includes header, arraylet ptrs, and (if present) padding & inline data
	 */
	protected UDATA getSpineSize(J9IndexableObjectPointer array) throws CorruptDataException
	{
		long layout = getArrayLayout(array);
		boolean alignData = shouldAlignSpineDataSection(J9IndexableObjectHelper.clazz(array));
		UDATA dataSize = getDataSizeInBytes(array);
		UDATA numberArraylets = numArraylets(dataSize);

		return getSpineSize(layout, numberArraylets, dataSize, alignData);
	}

	/**
	 * Get the layout for the given indexable object
	 * @param objPtr Pointer to a array object
	 * @return the ArrayLayout for objectPtr
	 */
	protected long getArrayLayout(J9IndexableObjectPointer array) throws CorruptDataException
	{
		/* Trivial check for InlineContiguous. */
		if (!J9IndexableObjectHelper.rawSize(array).eq(0)) {
			return GC_ArrayletObjectModelBase$ArrayLayout.InlineContiguous;
		}

		/* Check if the objPtr is in the allowed arraylet range. */
		if (array.gte(arrayletRangeBase) && array.lt(arrayletRangeTop)) {
			UDATA dataSizeInBytes = getDataSizeInBytes(array);
			long layout = getArrayLayout(J9IndexableObjectHelper.clazz(array), dataSizeInBytes);
			return layout;
		}
		return GC_ArrayletObjectModelBase$ArrayLayout.InlineContiguous;
	}

	/**
	 * Get the layout of an indexable object given it's class, data size in bytes and the subspace's largestDesirableSpine.
	 * @param clazz The class of the object stored in the array.
	 * @param dataSizeInBytes the size in bytes of the data of the array.
	 * @param largestDesirableSpine The largest desirable spine of the arraylet.
	 */
	protected long getArrayLayout(J9ArrayClassPointer clazz, UDATA dataSizeInBytes) throws CorruptDataException
	{
		long layout = GC_ArrayletObjectModelBase$ArrayLayout.Illegal;
		UDATA minimumSpineSize = new UDATA(0);
		UDATA minimumSpineSizeAfterGrowing = minimumSpineSize;

		if (GCExtensions.isVLHGC()) {
			/* CMVC 170688: Ensure that we don't try to allocate an inline contiguous array of a size which will overflow the region if it ever grows
			 * (easier to handle this case in the allocator than to special-case the collectors to know how to avoid this case)
			 * (currently, we only grow by a hashcode slot which is 4-bytes but will increase our size by the granule of alignment)
			 */
			minimumSpineSize = minimumSpineSize.add(ObjectModel.getObjectAlignmentInBytes());
		}

		/* CMVC 135307 : when checking for InlineContiguous layout, perform subtraction as adding to dataSizeInBytes could trigger overflow. */
		if (largestDesirableArraySpineSize.eq(UDATA.MAX)
		|| dataSizeInBytes.lte(largestDesirableArraySpineSize.sub(minimumSpineSizeAfterGrowing).sub(J9IndexableObjectHelper.contiguousHeaderSize()))
		) {
			layout = GC_ArrayletObjectModelBase$ArrayLayout.InlineContiguous;
			if (dataSizeInBytes.eq(0)) {
				/* Zero sized NUA uses the discontiguous shape */
				layout = GC_ArrayletObjectModelBase$ArrayLayout.Discontiguous;
			}
		} else {
			UDATA lastArrayletBytes = dataSizeInBytes.bitAnd(arrayletLeafSizeMask);
			boolean isAllIndexableDataContiguousEnabled = enableVirtualLargeObjectHeap || enableDoubleMapping;

			if (isAllIndexableDataContiguousEnabled && dataSizeInBytes.gt(0)) {
				layout = GC_ArrayletObjectModelBase$ArrayLayout.InlineContiguous;
			} else if (lastArrayletBytes.gt(0)) {
				/* determine how large the spine would be if this were a hybrid array */
				UDATA numberArraylets = numArraylets(dataSizeInBytes);
				boolean align = shouldAlignSpineDataSection(clazz);
				UDATA hybridSpineBytes = getSpineSize(GC_ArrayletObjectModelBase$ArrayLayout.Hybrid, numberArraylets, dataSizeInBytes, align);
				UDATA adjustedHybridSpineBytes = ObjectModel.adjustSizeInBytes(hybridSpineBytes);
				UDATA adjustedHybridSpineBytesAfterMove = adjustedHybridSpineBytes;
				if (GCExtensions.isVLHGC()) {
					adjustedHybridSpineBytesAfterMove.add(ObjectModel.getObjectAlignmentInBytes());
				}

				if (adjustedHybridSpineBytesAfterMove.lte(largestDesirableArraySpineSize)) {
					layout = GC_ArrayletObjectModelBase$ArrayLayout.Hybrid;
				} else {
					layout = GC_ArrayletObjectModelBase$ArrayLayout.Discontiguous;
				}
			} else {
				/* remainder is empty, so no arraylet allocated; last arrayoid pointer is set to MULL */
				layout = GC_ArrayletObjectModelBase$ArrayLayout.Discontiguous;
			}
		}

		return layout;
	}

	/**
	 * Returns the size of data in an indexable object, in bytes, including leaves, excluding the header.
	 * @param array Pointer to the indexable object whose size is required
	 * @return Size of object in bytes excluding the header
	 */
	public UDATA getDataSizeInBytes(J9IndexableObjectPointer array) throws CorruptDataException
	{
		J9ArrayClassPointer clazz = J9IndexableObjectHelper.clazz(array);
		UDATA arrayShape = J9ROMArrayClassPointer.cast(clazz.romClass()).arrayShape();
		UDATA numberOfElements = getSizeInElements(array);
		UDATA size = numberOfElements.leftShift(arrayShape.bitAnd(0x0000FFFF).intValue());
		return UDATA.roundToSizeofUDATA(size);
	}

	@Override
	public ObjectReferencePointer getArrayoidPointer(J9IndexableObjectPointer array) throws CorruptDataException
	{
		return ObjectReferencePointer.cast(array.addOffset(J9IndexableObjectHelper.discontiguousHeaderSize()));
	}

	@Override
	public VoidPointer getDataPointerForContiguous(J9IndexableObjectPointer array) throws CorruptDataException
	{
		return VoidPointer.cast(array.addOffset(J9IndexableObjectHelper.contiguousHeaderSize()));
	}

	/**
	 * Determine the validity of the data address belonging to arrayPtr.
	 *
	 * @param arrayPtr array object who's data address validity we are checking
	 * @throws CorruptDataException if there's a problem accessing the indexable object dataAddr field
	 * @throws NoSuchFieldException if the indexable object dataAddr field does not exist on the build that generated the core file
	 * @return true if the data address of arrayPtr is valid, false otherwise
	 */
	public abstract boolean isCorrectDataAddrPointer(J9IndexableObjectPointer arrayPtr) throws CorruptDataException, NoSuchFieldException;

	@Override
	public UDATA getHashcodeOffset(J9IndexableObjectPointer array) throws CorruptDataException
	{
		// Don't call getDataSizeInBytes() since that rounds up to UDATA.
		long layout = getArrayLayout(array);
		J9ArrayClassPointer clazz = J9IndexableObjectHelper.clazz(array);
		UDATA arrayShape = J9ROMArrayClassPointer.cast(clazz.romClass()).arrayShape();
		UDATA numberOfElements = getSizeInElements(array);
		UDATA dataSize = numberOfElements.leftShift(arrayShape.bitAnd(0x0000FFFF).intValue());
		UDATA numberArraylets = numArraylets(dataSize);
		boolean alignData = shouldAlignSpineDataSection(clazz);
		UDATA spineSize = getSpineSize(layout, numberArraylets, dataSize, alignData);
		return U32.roundToSizeofU32(spineSize);
	}

	/**
	 * Check the given indexable object is inline contiguous
	 * @param objPtr Pointer to an array object
	 * @return true of array is inline contiguous
	 */
	@Override
	public boolean isInlineContiguousArraylet(J9IndexableObjectPointer array) throws CorruptDataException
	{
		return getArrayLayout(array) == GC_ArrayletObjectModelBase$ArrayLayout.InlineContiguous;
	}

	/**
	 * Determine if the arraylet data of arrayPtr is adjacent to its header.
	 *
	 * @param arrayPtr array object
	 * @throws CorruptDataException If there's a problem accessing the indexable object dataAddr field
	 * @return true if the arrayPtr has its data adjacent to its header
	 */
	public boolean isArrayletDataAdjacentToHeader(J9IndexableObjectPointer array) throws CorruptDataException
	{
		UDATA dataSizeInBytes = getDataSizeInBytes(array);
		return isArrayletDataAdjacentToHeader(dataSizeInBytes);
	}

	/**
	 * Determine if the arraylet data of arrayPtr is adjacent to its header.
	 *
	 * @param arrayPtr array object
	 * @throws CorruptDataException if there's a problem accessing the indexable object dataAddr field
	 * @return true if the arrayPtr has its data adjacent to its header
	 */
	public boolean isArrayletDataAdjacentToHeader(UDATA dataSizeInBytes)  throws CorruptDataException
	{
		MM_GCExtensionsPointer extensions = GCBase.getExtensions();
		UDATA minimumSpineSizeAfterGrowing = new UDATA(ObjectModel.getObjectAlignmentInBytes());

		return (largestDesirableArraySpineSize.eq(UDATA.MAX) || dataSizeInBytes.lte(largestDesirableArraySpineSize.sub(minimumSpineSizeAfterGrowing).sub(J9IndexableObjectHelper.contiguousHeaderSize())));
	}

	/**
	 * Check if the given indexable object is double mapped.
	 *
	 * @param address indexable object to check if it is double mapped
	 * @throws CorruptDataException if there's a problem accessing card table heap fields
	 * @return true if the given indexable object is double mapped
	 */
	public boolean isIndexableObjectDoubleMapped(VoidPointer indexableDataAddr, UDATA dataSizeInBytes) throws CorruptDataException
	{
		boolean isObjectWithinHeap = isAddressWithinHeap(indexableDataAddr);
		//return (!isObjectWithinHeap && !indexableDataAddr.equals(null));
		return (!isObjectWithinHeap && dataSizeInBytes.gte(arrayletLeafSize));
	}

	/**
	 * Check if the given address is within the heap.
	 *
	 * @param address Address to check if it is within the heap
	 * @throws CorruptDataException if there's a problem accessing card table heap fields
	 * @return true if the given address is within the heap
	 */
	public boolean isAddressWithinHeap(VoidPointer address) throws CorruptDataException
	{
		MM_GCExtensionsPointer extensions = GCBase.getExtensions();

		UDATA heapAddr = UDATA.cast(address);
		UDATA heapBase = UDATA.cast(extensions.cardTable()._heapBase());
		UDATA heapTop = UDATA.cast(extensions.cardTable()._heapAlloc());

		return (heapAddr.gte(heapBase) && heapAddr.lte(heapTop));
	}

	/**
	 * Determines whether or not a spine that represents an object of this
	 * class should have its data section aligned.
	 * @param clazz The class to check alignment for
	 * @return needAlignment Should the data section be aligned
	 */
	protected boolean shouldAlignSpineDataSection(J9ArrayClassPointer clazz) throws CorruptDataException
	{
		return shouldAlignSpineDataSection(J9ClassPointer.cast(clazz));
	}

	/**
	 * Determines whether or not a spine that represents an object of this
	 * class should have its data section aligned.
	 * @param clazz The class to check alignment for
	 * @return needAlignment Should the data section be aligned
	 */
	protected boolean shouldAlignSpineDataSection(J9ClassPointer clazz) throws CorruptDataException
	{
		boolean needAlignment = false;

		if (J9ObjectHelper.compressObjectReferences) {
			/* Compressed pointers require that each leaf starts at 8 aligned address.
			 * Otherwise compressed leaf pointers will not work with shift value of 3.
			 */
			needAlignment = true;
		} else {
			/* The alignment padding is only required when the size of the spine pointers are
			 * not 8 byte aligned. For example, on a 64-bit non-compressed platform, a single
			 * spine pointer would be 8 byte aligned, whereas on a 32-bit platform, a single
			 * spine pointer would not be 8 byte aligned, and would require additional padding.
			 */
			if (!J9BuildFlags.env_data64) {
				/* this cast is ugly.. TODO: lpnguyen. Implement getClassShape(J9ArrayClassPointer clazz) and other similar methods for J9ArrayClassPointers in ObjectModel.
				 */
				UDATA classShape = ObjectModel.getClassShape(clazz);
				if (classShape.eq(J9Object.OBJECT_HEADER_SHAPE_DOUBLES)) {
					needAlignment = true;
				}
			}
		}
		return needAlignment;
	}

	@Override
	public VoidPointer getElementAddress(J9IndexableObjectPointer array, int elementIndex, int elementSize) throws CorruptDataException
	{
		boolean isInlineContiguous = isInlineContiguousArraylet(array);
		UDATA byteOffsetIntoData = new UDATA(elementIndex * (long) elementSize);

		/* TODO: lpnguyen bounds check would be nice here */

		if (isInlineContiguous) {
			return VoidPointer.cast(getDataPointerForContiguous(array).addOffset(byteOffsetIntoData));
		} else {
			ObjectReferencePointer arrayoid = getArrayoidPointer(array);
			UDATA arrayletIndex = byteOffsetIntoData.rightShift(arrayletLeafLogSize);
			VoidPointer arrayletLeafBaseAddress = VoidPointer.cast(arrayoid.at(arrayletIndex));

			if (arrayletLeafBaseAddress.isNull()) {
				/* this can happen if the arraylet wasn't fully initialized */
				throw new NoSuchElementException("Arraylet leaf not yet initialized");
			}
			UDATA indexIntoArraylet = byteOffsetIntoData.bitAnd(arrayletLeafSizeMask);
			return arrayletLeafBaseAddress.addOffset(indexIntoArraylet);
		}
	}

	/**
	 * Return the total number of arraylets for an indexable object with a size of dataInSizeByte, including a (possibly empty) leaf in the spine.
	 * @param dataSizeInBytes size of an array in bytes (not elements)
	 * @return the number of arraylets used for an array of dataSizeInBytes bytes
	 */
	protected UDATA numArraylets(UDATA unadjustedDataSizeInBytes) throws CorruptDataException
	{
		UDATA numberOfArraylets = new UDATA(1);
		if (!UDATA.MAX.eq(arrayletLeafSize)) {
			/* We add one to unadjustedDataSizeInBytes to ensure that it's always possible to determine the address
			 * of the after-last element without crashing. Handle the case of UDATA_MAX specially, since we use that
			 * for any object whose size overflows the address space.
			 */
			UDATA dataSizeInBytes = UDATA.MAX.eq(unadjustedDataSizeInBytes) ? UDATA.MAX : unadjustedDataSizeInBytes.add(1);

			/* CMVC 135307 : following logic for calculating the leaf count would not overflow dataSizeInBytes.
			 * the assumption is leaf size is order of 2. It's identical to:
			 * if (dataSizeInBytes % leafSize) is 0
			 *   leaf count = dataSizeInBytes >> leafLogSize
			 * else
			 *   leaf count = (dataSizeInBytes >> leafLogSize) + 1
			 */
			numberOfArraylets = dataSizeInBytes.rightShift(arrayletLeafLogSize).add(dataSizeInBytes.bitAnd(arrayletLeafSizeMask).add(arrayletLeafSizeMask).rightShift(arrayletLeafLogSize));
		}
		return numberOfArraylets;
	}

	/**
	 * Return the total number of arraylets for an indexable object, not including the arraylet in the spine.
	 * Note that discontiguous arrays always have an empty leaf contained in the spine.
	 * @param array pointer to array
	 * @return the number of leaf arraylets
	 */
	protected UDATA numExternalArraylets(J9IndexableObjectPointer array) throws CorruptDataException
	{
		UDATA numberOfArraylets = new UDATA(0);
		if (getArrayLayout(array) != GC_ArrayletObjectModelBase$ArrayLayout.InlineContiguous) {
			numberOfArraylets = numArraylets(getDataSizeInBytes(array));
			numberOfArraylets = numberOfArraylets.sub(1);
		}

		return numberOfArraylets;
	}

	/**
	 * Get the total number of bytes consumed by arraylets external to the
	 * given indexable object.
	 * @param array Pointer to an array object
	 * @return the number of bytes consumed external to the spine
	 * @throws CorruptDataException
	 */
	protected UDATA externalArrayletsSize(J9IndexableObjectPointer array) throws CorruptDataException
	{
		UDATA numberArraylets = numExternalArraylets(array);

		if (numberArraylets.isZero()) {
			return numberArraylets;
		} else {
			return numberArraylets.mult(arrayletLeafSize);
		}
	}

}
