/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
package com.ibm.dtfj.java.j9;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.image.j9.CorruptData;
import com.ibm.dtfj.image.j9.ImageSection;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaThread;

/**
 * @author jmdisher
 * 
 * TODO:  there are more conditions where this is a null pointer that need to be handled
 */
public class JavaObject implements com.ibm.dtfj.java.JavaObject
{
	private JavaRuntime _javaVM;
	private ImagePointer _basePointer;
	private JavaHeap _containingHeap;
	private long _arrayletSpineSize;
	private long _arrayletLeafSize;
	private boolean _isArraylet = false;
	private int _objectAlignment;
	
	private byte _associatedObjectFlags = 0;
	private JavaClass _associatedClass = null;
	private JavaMonitor _associatedMonitor = null;
	private JavaThread _associatedThread = null;
	private JavaClassLoader _associatedClassLoader = null;
	
	private static final byte CLASS_FLAG = 1;
	private static final byte THREAD_FLAG = 2;
	private static final byte MONITOR_FLAG = 4;
	private static final byte CLASSLOADER_FLAG = 8;
	
	protected static final String BOOLEAN_SIGNATURE = "Z";
	protected static final String BYTE_SIGNATURE = "B";
	protected static final String CHAR_SIGNATURE = "C";
	protected static final String SHORT_SIGNATURE = "S";
	protected static final String INTEGER_SIGNATURE = "I";
	protected static final String LONG_SIGNATURE = "J";
	protected static final String FLOAT_SIGNATURE = "F";
	protected static final String DOUBLE_SIGNATURE = "D";

	protected static final String ARRAY_PREFIX_SIGNATURE = "[";
	protected static final String OBJECT_PREFIX_SIGNATURE = "L";
	
	private Vector _references = null;
	
	JavaObject(JavaRuntime vm, ImagePointer address, JavaHeap containingHeap, long arrayletSpineSize, long arrayletLeafSize, boolean isArraylet, int objectAlignment) throws CorruptDataException
	{
		//this case will not be handled as quietly as the other cases in the factory method since it would imply a more fundamental problem
		if (null == vm) {
			throw new IllegalArgumentException("A Java Object cannot exist in a null VM");
		}
		_basePointer = address;
		_javaVM = vm;
		_containingHeap = containingHeap;
		_arrayletSpineSize = arrayletSpineSize;
		_arrayletLeafSize = arrayletLeafSize;
		_isArraylet = isArraylet;
		_objectAlignment = objectAlignment;
	}
	
	/**
	 * @deprecated Use {@link JavaRuntime#getObjectAtAddress(ImagePointer)} instead
	 */
	@Deprecated
	public static JavaObject createJavaObject(JavaRuntime vm, ImagePointer address) throws CorruptDataException
	{
		try {
			return (JavaObject) vm.getObjectAtAddress(address);
		} catch (IllegalArgumentException e) {
			return null;
		}
	}
	
	/**
	 * @deprecated Use {@link JavaRuntime#getObjectInHeapRegion(ImagePointer,JavaHeap,JavaHeapRegion)} instead
	 */
	@Deprecated
	public static JavaObject createJavaObject(JavaRuntime vm, ImagePointer address, JavaHeap containingHeap, JavaHeapRegion containingRegion) throws CorruptDataException
	{
		try {
			return (JavaObject) containingRegion.getObjectAtAddress(address);
		} catch (IllegalArgumentException e) {
			return null;
		}

	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#getJavaClass()
	 */
	public JavaClass getJavaClass() throws CorruptDataException
	{
		if (0 != _basePointer.getAddress()) {
			ImagePointer classPointer;
			if (_containingHeap == null) throw new CorruptDataException(new CorruptData("unable to access class pointer as containing heap is null", _basePointer));
			try {
				classPointer = _containingHeap.readClassPointerRelativeTo(_basePointer);
			} catch (MemoryAccessException e) {
				throw new CorruptDataException(new CorruptData("unable to access class pointer", _basePointer));
			}
			long classID = classPointer.getAddress();
			/* CMVC 167379: Lowest few bits of the class id are used as flags, and should be 
			 * masked with ~(J9_REQUIRED_CLASS_ALIGNMENT - 1) to find real class id. 
			 */
			long classAlignment = _containingHeap.getClassAlignment();
			long alignedClassID = classID;
			if (classAlignment > 0) {
				 alignedClassID &= (~(classAlignment - 1L));
			}
			JavaClass ret = _javaVM.getClassForID(alignedClassID);
			if (ret == null) {
				throw new CorruptDataException(new CorruptData("Unknown class ID " + Long.toHexString(alignedClassID) + " for object " + Long.toHexString(_basePointer.getAddress()) + " (read class ID from "+Long.toHexString(classPointer.getAddress())+", in memory value was "+Long.toHexString(classID)+")", _basePointer));
			}
			return ret;
		} else {
			throw new NullPointerException();
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#isArray()
	 */
	public boolean isArray() throws CorruptDataException
	{
		return getJavaClass().isArray();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#getArraySize()
	 */
	public int getArraySize() throws CorruptDataException
	{
		JavaClass isa = getJavaClass();
		if (isa instanceof JavaArrayClass) {
			JavaArrayClass blueprint = (JavaArrayClass) isa;
			int offset = blueprint.getSizeOffset();
			int numberOfSizeBytes = blueprint.getNumberOfSizeBytes();
			
			try {
				int numberOfElements = 0;
				
				if (4 == numberOfSizeBytes) {
					//read an int
					numberOfElements = _basePointer.getIntAt(offset);
				} else if (8 == numberOfSizeBytes) {
					//read a long
					long longCount = _basePointer.getLongAt(offset);
					//the spec says to return an int here so we just truncate, for now
					numberOfElements = (int)longCount;
					if (((long)numberOfElements) != longCount) {
						System.err.println("Error:  Array element count overflow or underflow.");
					}
				} else {
					//not sure what this is (this could be done generically but that would require exposing endian-conversion differently.  Besides, stopping this at a strange number is probably a good idea since it is more likely an error)
					System.err.println("Error:  unable to read array size as we weren't expecting to read " + numberOfSizeBytes + " bytes.");
				}
				return numberOfElements;
			} catch (MemoryAccessException e) {
				throw new CorruptDataException(new CorruptData("unable to read the number of elements", _basePointer.add(offset)));
			}
		} else {
			throw new IllegalArgumentException();
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#arraycopy(int, java.lang.Object, int, int)
	 */
	public void arraycopy(int srcStart, Object dst, int dstStart, int length)
			throws CorruptDataException, MemoryAccessException
	{
		JavaClass javaClass = getJavaClass();
		if (javaClass instanceof JavaArrayClass) {
			JavaArrayClass isa = (JavaArrayClass)javaClass;
			String type = javaClass.getName();
			int elementCount = getArraySize();
			
			// do some rudimentary sanity checking before attempting the copy.
			if (null == dst) {
				// cannot copy to a null object, so raise an exception.
				throw new NullPointerException("dst is null");
			}
			
			if (length < 0) {
				// length cannot be negative, so raise an exception.
				throw new ArrayIndexOutOfBoundsException("length out of range: " + length);
			}
			
			if (dstStart < 0) {
				// array index cannot be negative, so raise an exception.
				throw new ArrayIndexOutOfBoundsException("dstStart out of range: " + dstStart);
			}
			
			if (srcStart < 0) {
				// array index cannot be negative, so raise an exception.
				throw new ArrayIndexOutOfBoundsException("srcStart out of range: " + srcStart);
			}
			
			if (srcStart + length > elementCount) {
				throw new ArrayIndexOutOfBoundsException("source array index out of range: " + (int)(srcStart + length));
			}
			
			// Note that type name is dependent on implementation in JavaArrayClass.getName()
			// boolean
			if (type.equals(ARRAY_PREFIX_SIGNATURE + BYTE_SIGNATURE)) {
				if (dst instanceof byte[]) {
					byte target[] = (byte[])dst;
					
					if (dstStart + length > target.length) {
						throw new ArrayIndexOutOfBoundsException("destination array index out of range: " + (int)(dstStart + length));
					}

					for (int x = 0; x < length; x++) {
						ImagePointer leafBase = leafBaseForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 1));
						long leafIndex = leafIndexForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 1));
						target[x+dstStart] = leafBase.getByteAt(leafIndex);
					}
				} else {
					throw new IllegalArgumentException("destination array type must be byte");
				}
			// boolean
			} else if (type.equals(ARRAY_PREFIX_SIGNATURE + BOOLEAN_SIGNATURE)) {
				if (dst instanceof boolean[]) {
					boolean target[] = (boolean[])dst;

					if (dstStart + length > target.length) {
						throw new ArrayIndexOutOfBoundsException("destination array index out of range: " + (int)(dstStart + length));
					}

					for (int x = 0; x < length; x++) {
						ImagePointer leafBase = leafBaseForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 1));
						long leafIndex = leafIndexForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 1));
						target[x+dstStart] = (0 != leafBase.getByteAt(leafIndex));
					}
				} else {
					throw new IllegalArgumentException("destination array type must be boolean");
				}
			// char
			} else if (type.equals(ARRAY_PREFIX_SIGNATURE + CHAR_SIGNATURE)) {
				if (dst instanceof char[]) {
					char target[] = (char[])dst;

					if (dstStart + length > target.length) {
						throw new ArrayIndexOutOfBoundsException("destination array index out of range: " + (int)(dstStart + length));
					}

					for (int x = 0; x < length; x++) {
						ImagePointer leafBase = leafBaseForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 2));
						long leafIndex = leafIndexForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 2));
						target[x+dstStart] = (char)(leafBase.getShortAt(leafIndex));
					}
				} else {
					throw new IllegalArgumentException("destination array type must be char");
				}
			// short
			} else if (type.equals(ARRAY_PREFIX_SIGNATURE + SHORT_SIGNATURE)) {
				if (dst instanceof short[]) {
					short target[] = (short[])dst;

					if (dstStart + length > target.length) {
						throw new ArrayIndexOutOfBoundsException("destination array index out of range: " + (int)(dstStart + length));
					}

					for (int x = 0; x < length; x++) {
						ImagePointer leafBase = leafBaseForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 2));
						long leafIndex = leafIndexForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 2));
						target[x+dstStart] = leafBase.getShortAt(leafIndex);
					}
				} else {
					throw new IllegalArgumentException("destination array type must be short");
				}
			// int
			} else if (type.equals(ARRAY_PREFIX_SIGNATURE + INTEGER_SIGNATURE)) {
				if (dst instanceof int[]) {
					int target[] = (int[])dst;

					if (dstStart + length > target.length) {
						throw new ArrayIndexOutOfBoundsException("destination array index out of range: " + (int)(dstStart + length));
					}

					for (int x = 0; x < length; x++) {
						ImagePointer leafBase = leafBaseForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 4));
						long leafIndex = leafIndexForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 4));
						target[x+dstStart] = leafBase.getIntAt(leafIndex);
					}
				} else {
					throw new IllegalArgumentException("destination array type must be int");
				}
			// long
			} else if (type.equals(ARRAY_PREFIX_SIGNATURE + LONG_SIGNATURE)) {
				if (dst instanceof long[]) {
					long target[] = (long[])dst;

					if (dstStart + length > target.length) {
						throw new ArrayIndexOutOfBoundsException("destination array index out of range: " + (int)(dstStart + length));
					}

					for (int x = 0; x < length; x++) {
						ImagePointer leafBase = leafBaseForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 8));
						long leafIndex = leafIndexForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 8));
						target[x+dstStart] = leafBase.getLongAt(leafIndex);
					}
				} else {
					throw new IllegalArgumentException("destination array type must be long");
				}
			// float
			} else if (type.equals(ARRAY_PREFIX_SIGNATURE + FLOAT_SIGNATURE)) {
				if (dst instanceof float[]) {
					float target[] = (float[])dst;

					if (dstStart + length > target.length) {
						throw new ArrayIndexOutOfBoundsException("destination array index out of range: " + (int)(dstStart + length));
					}

					for (int x = 0; x < length; x++) {
						ImagePointer leafBase = leafBaseForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 4));
						long leafIndex = leafIndexForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 4));
						target[x+dstStart] = leafBase.getFloatAt(leafIndex);
					}
				} else {
					throw new IllegalArgumentException("destination array type must be float");
				}
			// double
			} else if (type.equals(ARRAY_PREFIX_SIGNATURE + DOUBLE_SIGNATURE)) {
				if (dst instanceof double[]) {
					double target[] = (double[])dst;

					if (dstStart + length > target.length) {
						throw new ArrayIndexOutOfBoundsException("destination array index out of range: " + (int)(dstStart + length));
					}

					for (int x = 0; x < length; x++) {
						ImagePointer leafBase = leafBaseForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 8));
						long leafIndex = leafIndexForIndex(isa.getFirstElementOffset(), ((srcStart + x) * 8));
						target[x+dstStart] = leafBase.getDoubleAt(leafIndex);
					}
				} else {
					throw new IllegalArgumentException("destination array type must be double");
				}
			// object
			} else { // type must be Object 
				if (!(dst instanceof Object[])) {
					throw new IllegalArgumentException("destination array type must be Object");
				}

				Object target[] = (Object[])dst;		

				if (dstStart + length > target.length) {
					throw new ArrayIndexOutOfBoundsException("destination array index out of range: " + (int)(dstStart + length));
				}

				// gather all the JavaObject refs into an intermediate array, throwing exceptions if we encounter an error
				Object[] intermediateArray = new Object[length];
				for (int x = 0; x < length; x++) {
					int fobjectSize = _containingHeap.getFObjectSize();
					ImagePointer leafBase = leafBaseForIndex(isa.getFirstElementOffset(), ((srcStart + x) * fobjectSize));
					long leafIndex = leafIndexForIndex(isa.getFirstElementOffset(), ((srcStart + x) * fobjectSize));
					ImagePointer pointer = _containingHeap.readFObjectAt(leafBase, leafIndex);
					try {
						//CMVC 150173 : it is possible that the pointer used to get the object is in some way invalid. If this is the case
						//				then getObjectAtAddress may return null, so check for this and throw CorruptDataException
						//9th Sept 2009 : reinstated original code. There are two reasons why getObjectAtAddress may return a null, 
						//				when it is given an invalid address and when it is copying array slots that do not yet contain
						//				an object.
						if(pointer.getAddress() == 0) {
							//CMVC 175864 : the getObjectAtAddress function enforces stricter address checking, so need to exclude uninitialized slots up front
							intermediateArray[x] = null;
						} else {
							try {
								intermediateArray[x] = _javaVM.getObjectAtAddress(pointer);
							} catch (IllegalArgumentException e) {
								// an IllegalArgumentException might be thrown if the address is not aligned
								throw new CorruptDataException(new CorruptData(e.getMessage(),pointer));
							}
						}
					} catch (ArrayStoreException e) {
						throw new IllegalArgumentException(e.getMessage());
					}
				}
				// if no exceptions were thrown, update the caller's array all in one go 
				for (int i=0; i < length; i++) {
					try {
						target[dstStart + i] = intermediateArray[i];
					} catch (ArrayStoreException e) {
						throw new IllegalArgumentException(e.getMessage());
					}
				}
			}
		} else {
			throw new IllegalArgumentException("this JavaObject instance is not an array");
		}
	}

	private long leafIndexForIndex(int firstElementOffset, long index)
	{
		long ret = 0;
		
		if (isArraylet()) {
			ret = index % _arrayletLeafSize;
		} else {
			ret = firstElementOffset + index;
		}
		return ret;
	}

	private ImagePointer leafBaseForIndex(int firstElementOffset, long index) throws CorruptDataException, MemoryAccessException
	{
		ImagePointer ret = null;
		
		if (isArraylet()) {
			int fobjectSize = _containingHeap.getFObjectSize();
			long pointerIndex = index / _arrayletLeafSize;
			long pointerAddress = firstElementOffset + (fobjectSize * pointerIndex);
			ret = _containingHeap.readFObjectAt(_basePointer, pointerAddress);
		} else {
			ret = _basePointer;
		}
		return ret;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#getSize()
	 */
	public long getSize() throws CorruptDataException
	{
		//this is the total number of bytes consumed by the object so we will total up the section sizes
		Iterator sections = getSections();
		long size = 0;
		
		// Produce a fix for a class cast in getSize.  
		// Sadly I can no longer get this one to fail but there's no point in just leaving it unfixed.  
		// Originally came from Dump analyzer defect 74504.
		while (sections.hasNext()) {
			Object qsect = sections.next();
			if (qsect instanceof ImageSection) {
				ImageSection section = (ImageSection) qsect;
				size += section.getSize();
			} else {
				if (qsect instanceof CorruptData) {
					throw new CorruptDataException((CorruptData)qsect);
				} else {
					throw new RuntimeException("Found unexpected object " + qsect.getClass().getName());
				}
			}
		}
		return size;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#getHashcode()
	 */
	public long getHashcode() throws DataUnavailable, CorruptDataException
	{
		//currently, we know no difference between these two kinds of hashcodes (may be revisited in the future)
		return getPersistentHashcode();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#getPersistentHashcode()
	 */
	public long getPersistentHashcode() throws DataUnavailable,
			CorruptDataException
	{
		//note that we have no abstract way of defining the hash strategy, at this time, so we will have to ask the runtime if we can guess at our internal implementation
		//this is a terrible way to do this since we know that it _will_ _break_ in the future.  It is, however, the best that we can do
		if (_javaVM.objectShouldInferHash()) {
			try {
				int flags = ((com.ibm.dtfj.java.j9.JavaAbstractClass)getJavaClass()).readFlagsFromInstance(this);
				//now mask out the non-hash bits, shift and return
				int twoBytes = flags & 0x7FFF0000;	//high 15 bits, omitting most significant
				long hash = (twoBytes >> 16) | twoBytes;
				return hash;
			} catch (MemoryAccessException e) {
				//if we can't access the memory, the core must be corrupt
				throw new CorruptDataException(new CorruptData("Address in object header but unreadable", _basePointer));
			}
		} else {
			throw new DataUnavailable("Unknown hash strategy for this VM version");
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#getID()
	 */
	public ImagePointer getID()
	{
		return _basePointer;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#getSections()
	 */
	public Iterator getSections()
	{
		//(not initialized so that code paths will be compiler-validated)
		List sections;

		//arraylets have a more complicated scheme so handle them differently
		if (isArraylet()) {
			try
			{
				JavaArrayClass arrayForm = (JavaArrayClass) getJavaClass();
				//the first element comes immediately after the header so the offset to it is the size of the header
				// NOTE:  this header size does NOT count arraylet leaves
				int objectHeaderSize = arrayForm.getFirstElementOffset();
				//we require the pointer size in order to walk the leaf pointers in the spine
				int bytesPerPointer = _javaVM.bytesPerPointer();
				try
				{
					int instanceSize = arrayForm.getInstanceSize(this);
					//the instance size will include the header and the actual data inside the array so separate them
					long contentDataSize = (long)(instanceSize - objectHeaderSize);
					//get the number of leaves, excluding the tail leaf (the tail leaf is the final leaf which points back into the spine).  There won't be one if there is isn't a remainder in this calculation since it would be empty
					int fullSizeLeaves = (int)(contentDataSize / _arrayletLeafSize);
					//find out how big the tail leaf would be
					long tailLeafSize = contentDataSize % _arrayletLeafSize;
					//if it is non-zero, we know that there must be one (bear in mind the fact that all arraylets have at least 1 leaf pointer - consider empty arrays)
					int totalLeafCount = (0 == tailLeafSize) ? fullSizeLeaves : (fullSizeLeaves + 1);
					//CMVC 153943 : DTFJ fix for zero-length arraylets - remove code to add 1 to the leaf count in the event that it is 0.
					//by always assuming there is a leaf means that when the image sections are determined it will cause an error as there
					//is no space allocated in this instance beyond the size of the spine.
					String nestedType = arrayForm.getLeafClass().getName();
					//4-byte object alignment in realtime requires the long and double arraylets have padding which may need to be placed before the array data or after, depending on if the alignment succeeded at a natural boundary or not
					boolean alignmentCandidate = (4 == _objectAlignment) && ("double".equals(nestedType) || "long".equals(nestedType));
					//we will need a size for the section which includes the spine (and potentially the tail leaf or even all the leaves (in immortal))
					//start with the object header and the leaves
					long headerAndLeafPointers = objectHeaderSize + (totalLeafCount * bytesPerPointer);
					long spineSectionSize = headerAndLeafPointers;
					//we will now walk the leaves to see if this is an inline arraylet
					//first off, see if we would need padding to align the first inline data element
					long nextExpectedInteriorLeafAddress = _basePointer.getAddress() + headerAndLeafPointers;
					boolean doesHaveTailPadding = false;
					if (alignmentCandidate && (totalLeafCount > 0)) {			//alignment candidates need to have at least 1 leaf otherwise there is nothing to align
						if (0 == (nextExpectedInteriorLeafAddress % 8)) {
							//no need to add extra space here so the extra slot will be at the tail
							doesHaveTailPadding = true;
						} else {
							//we need to bump up our expected location for alignment
							nextExpectedInteriorLeafAddress += 4;
							spineSectionSize += 4;
							if (0 != (nextExpectedInteriorLeafAddress % 8)) {
								//this can't happen so the core is corrupt
								throw new CorruptDataException(new CorruptData("Arraylet leaf pointer misaligned for object", _basePointer));
							}
						}
					}
					Vector externalSections = null;
					for (int i = 0; i < totalLeafCount; i++) {
						ImagePointer leafPointer = _basePointer.getPointerAt(objectHeaderSize + (i * bytesPerPointer));
						if (leafPointer.getAddress() == nextExpectedInteriorLeafAddress) {
							//this pointer is interior so add it to the spine section
							long internalLeafSize = _arrayletLeafSize;
							if (fullSizeLeaves == i) {
								//this is the last leaf so get the tail leaf size
								internalLeafSize = tailLeafSize;
							}
							spineSectionSize += internalLeafSize;
							nextExpectedInteriorLeafAddress += internalLeafSize;
						} else {
							//this pointer is exterior so make it its own section
							if (null == externalSections) {
								externalSections = new Vector();
							}
							externalSections.add(new JavaObjectImageSection(leafPointer, _arrayletLeafSize));
						}
					}
					if (doesHaveTailPadding) {
						//now, add the extra 4 bytes to the end
						spineSectionSize += 4;
					}
					//ensure that we are at least the minimum object size
					spineSectionSize = Math.max(spineSectionSize, _arrayletSpineSize);
					
					JavaObjectImageSection spineSection = new JavaObjectImageSection(_basePointer, spineSectionSize);
					if (null == externalSections) {
						//create the section list, with the spine first (other parts of our implementation use the knowledge that the spine is first to reduce logic duplication)
						sections = Collections.singletonList(spineSection);
					} else {
						sections = new Vector();
						sections.add(spineSection);
						sections.addAll(externalSections);
					}
				}
				catch (MemoryAccessException e)
				{
					//if we had a memory access exception, the spine must be corrupt, or something
					sections = Collections.singletonList(new CorruptData("failed to walk arraylet spine", e.getPointer()));
				}
			}
			catch (CorruptDataException e)
			{
				sections = Collections.singletonList(e.getCorruptData());
			}
		} else {
			//currently J9 objects are atomic extents of memory but that changes with metronome and that will probably extend to other VM configurations, as well
			long size = 0;
			try
			{
				size = ((com.ibm.dtfj.java.j9.JavaAbstractClass)getJavaClass()).getInstanceSize(this);
				JavaObjectImageSection section = new JavaObjectImageSection(_basePointer, size);
				sections = Collections.singletonList(section);
			}
			catch (CorruptDataException e)
			{
				sections = Collections.singletonList(e.getCorruptData());
			}
			// XXX - handle the case of this corrupt data better (may require API change)
		}
		return sections.iterator();
	}

	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof JavaObject) {
			JavaObject local = (JavaObject) obj;
			isEqual = (_javaVM.equals(local._javaVM) && _basePointer.equals(local._basePointer));
		}
		return isEqual;
	}

	public int hashCode()
	{
		return _javaVM.hashCode() ^ _basePointer.hashCode();
	}
	
	class JavaObjectImageSection extends com.ibm.dtfj.image.j9.ImageSection
	{
		public JavaObjectImageSection(ImagePointer base, long size)
		{
			super(base, size);
		}
		
		public String getName()
		{
			return "In-memory Object section at 0x" + Long.toHexString(getBaseAddress().getAddress()) + " (0x" + Long.toHexString(getSize()) + " bytes)";
		}
		
	}

	public boolean isArraylet()
	{
		return _isArraylet;
	}

	public ImagePointer getFObjectAtOffset(int offset) throws MemoryAccessException, CorruptDataException
	{
		return _containingHeap.readFObjectAt(_basePointer, offset);
	}

	/**
	 * A method required for the JavaArrayClass so it can ask the instance it is trying to size how big reference fields are in its heap
	 * 
	 * @return The size of fj9object_t in the heap containing this instance
	 */
	public int getFObjectSize()
	{
		return _containingHeap.getFObjectSize();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaObject#getReferences()
	 */
	public Iterator getReferences()
	{
		if (null == _references) {

			_references = new Vector();
			try {
				// find this object's class
				JavaClass jClass = getJavaClass();
				//add a reference to the object's class
				if (null != jClass) {
					JavaReference ref = new JavaReference(_javaVM, this, jClass, "Class", JavaReference.REFERENCE_CLASS, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
					_references.add(ref);
				}
	
				// walk through the super classes to this object.
				while (null != jClass) {
					List refs = null;
					if (jClass instanceof JavaArrayClass) {
						JavaArrayClass arrayClass = (JavaArrayClass)jClass;
						refs = getArrayReferences(arrayClass);
					} else if (jClass instanceof com.ibm.dtfj.java.j9.JavaClass) {
						refs = getFieldReferences((com.ibm.dtfj.java.j9.JavaClass)jClass);
					}
					if (null != refs) {
						_references.addAll(refs);
					}
					jClass = jClass.getSuperclass();
				}
				
			} catch (CorruptDataException e) {
				// Corrupt data, so add it to the container.
				_references.add(e.getCorruptData());
			}
			
			// Now add association-specific references
			if (isClassLoader()) {
				JavaClassLoader associatedClassLoader = getAssociatedClassLoader();
				for (Iterator classes = associatedClassLoader.getDefinedClasses(); classes.hasNext();) {
					Object potentialClass = classes.next();
					if (potentialClass instanceof JavaClass) {
						JavaClass currentClass = (JavaClass)potentialClass;
						JavaReference ref = new JavaReference(_javaVM, this, currentClass, "Loaded class", JavaReference.REFERENCE_LOADED_CLASS,JavaReference.HEAP_ROOT_UNKNOWN,JavaReference.REACHABILITY_STRONG);
						_references.add(ref);
					}
				}
			} 
			if (isMonitor()) {
				//need to figure out whether we need additional references here (for example to the owning thread)
			}
			if (isThread()) {
				//need to figure out whether we need additional references here
			}
			if (isClass()) {
				JavaClass associatedClass = getAssociatedClass();
				JavaReference ref = new JavaReference(_javaVM, this, associatedClass, "Associated class", JavaReference.REFERENCE_ASSOCIATED_CLASS,JavaReference.HEAP_ROOT_UNKNOWN,JavaReference.REACHABILITY_STRONG);
				_references.add(ref);
			}
			
		}
		return _references.iterator(); 
	}
	
	/*
	 * @returns true if this object represents a class, false otherwise
	 * J9 specific
	 */
	public boolean isClass() {
		return (_associatedObjectFlags & CLASS_FLAG) > 0;
	}

	/*
	 * @returns true if this object represents a monitor, false otherwise
	 * J9 specific
	 */
	public boolean isMonitor() {
		return (_associatedObjectFlags & MONITOR_FLAG) > 0;
	}
	/*
	 * @returns true if this object represents a thread, false otherwise
	 * J9 specific
	 */
	public boolean isThread() {
		return (_associatedObjectFlags & THREAD_FLAG) > 0;
	}

	/*
	 * @returns true if this object represents a classloader, false otherwise
	 * J9 specific
	 */
	public boolean isClassLoader() {
		return (_associatedObjectFlags & CLASSLOADER_FLAG) > 0;
	}
	
	
	
	private List getFieldReferences(com.ibm.dtfj.java.j9.JavaClass jClass)
	{
		List fieldRefs = new ArrayList();
		
		// handle an Object
		Iterator fieldIt = jClass.getDeclaredFields();
		
		while (fieldIt.hasNext()) {
			// iterate through all the instance fields.
			Object fObject = fieldIt.next();
			if (fObject instanceof JavaInstanceField) {
				JavaField jField = (JavaField)fObject;
				JavaReference jRef = getFieldReference(jField);
				if (null != jRef) {
					fieldRefs.add(jRef);
				}
			}
		}

		return fieldRefs;
	}

	private JavaReference getFieldReference(JavaField jField) {
		JavaReference jRef = null;
		try {
			String sigPrefix = jField.getSignature();
			JavaClass jClass = getJavaClass();
			if (sigPrefix.startsWith(JavaField.OBJECT_PREFIX_SIGNATURE) || sigPrefix.startsWith(JavaField.ARRAY_PREFIX_SIGNATURE)) {
				// this is an object reference.
				try {
					JavaObject jObject = (JavaObject)jField.getReferenceType(this);
					
					if (null != jObject) {
						// build a JavaReference type and add the reference to the container.
						String fieldName = jField.getName();
						String description = "Object Reference";
						if (null != fieldName) {
							description = description + " [field name:" + fieldName + "]";
						}
						
						//Now figure out the reachability of the new reference.
						//This will normally be "strong", except for the referent field of a reference, 
						//for which reachability will be weak, soft or phantom, depending on the concrete reference type.
						int reachability = JavaReference.REACHABILITY_STRONG;
						if ("referent".equals(fieldName) && "java/lang/ref/Reference".equals(jField.getDeclaringClass().getName())) {
							if (_javaVM._weakReferenceClass != null && _javaVM._weakReferenceClass.isAncestorOf(jClass)) {
								reachability = JavaReference.REACHABILITY_WEAK;
							} else if (_javaVM._softReferenceClass != null && _javaVM._softReferenceClass.isAncestorOf(jClass)) {
								reachability = JavaReference.REACHABILITY_SOFT;
							} else if (_javaVM._phantomReferenceClass != null && _javaVM._phantomReferenceClass.isAncestorOf(jClass)) {
								reachability = JavaReference.REACHABILITY_PHANTOM;
							}
						}
						
						jRef = new JavaReference(_javaVM, this, jObject, description, JavaReference.REFERENCE_FIELD, JavaReference.HEAP_ROOT_UNKNOWN, reachability);
					}
				} catch (CorruptDataException e) {
				} 
			}
		} catch (CorruptDataException e) {
		} catch (MemoryAccessException e) {
		}
		return jRef;
	}

	private List getArrayReferences(JavaArrayClass arrayClass) {
		
		List references = new ArrayList();
		try {
			String type = arrayClass.getComponentType().getName();
	
			if (type.equals("byte")) {
				// ignore byte arrays.
			} else if (type.equals("boolean")) {
				// ignore boolean arrays.
			} else if (type.equals("char")) {
				// ignore char arrays.
			} else if (type.equals("short")) {
				// ignore short arrays.
			} else if (type.equals("int")) {
				// ignore int arrays.
			} else if (type.equals("long")) {
				// ignore long arrays.
			} else if (type.equals("float")) {
				// ignore float arrays.
			} else if (type.equals("double")) {
				// ignore double arrays.
			} else { 
				// must be Object array so handle it. 
				Object dst[] = null;
				int arraySize = getArraySize();
				
				if (arraySize > 0) {
					// only deal with arrays that have space for Object references.
					dst = new Object[arraySize];
					try {
						// copy the objects into our own array and then build the JavaReference
						// objects from them.
						arraycopy(0, dst, 0, arraySize);
	
						for (int i = 0; i < dst.length; i++) {
							if (null != dst[i]) {
								String description = "Array Reference";
								description  = description + " [index:" + i + "]";
	
								JavaReference jRef = new JavaReference(_javaVM, this, dst[i], description, JavaReference.REFERENCE_ARRAY_ELEMENT, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
								references.add(jRef);
							}
						}
					} catch (MemoryAccessException e) {
						// Memory access problems, so create a CorruptData object 
						// to describe the problem and add it to the container.
						ImagePointer ptrInError = e.getPointer();
						String message = e.getMessage();
						references.add(new CorruptData(message, ptrInError));
					}
				}
			}
		} catch (CorruptDataException e) {
			// Corrupt data, so add it to the container.
			references.add(e.getCorruptData());
		}

		return references;
	}

	public com.ibm.dtfj.java.JavaHeap getHeap() throws DataUnavailable {
		if (null == _containingHeap) {
			throw new DataUnavailable("Containing heap not available for this object.");
		} else {
			return _containingHeap;
		}
	}

	/**
	 * @return the associatedClass
	 */
	public JavaClass getAssociatedClass() {
		return _associatedClass;
	}

	/**
	 * @param associatedClass the associatedClass to set
	 */
	public void setAssociatedClass(JavaClass associatedClass) {
		_associatedObjectFlags |= CLASS_FLAG;
		_associatedClass = associatedClass;
	}

	/**
	 * @return the associatedMonitor
	 */
	public JavaMonitor getAssociatedMonitor() {
		return _associatedMonitor;
	}

	/**
	 * @param associatedMonitor the associatedMonitor to set
	 */
	public void setAssociatedMonitor(JavaMonitor associatedMonitor) {
		_associatedObjectFlags |= MONITOR_FLAG;
		_associatedMonitor = associatedMonitor;
	}

	/**
	 * @return the associatedThread
	 */
	public JavaThread getAssociatedThread() {
		return _associatedThread;
	}

	/**
	 * @param associatedThread the associatedThread to set
	 */
	public void setAssociatedThread(JavaThread associatedThread) {
		_associatedObjectFlags |= THREAD_FLAG;
		_associatedThread = associatedThread;
	}

	/**
	 * @return the associatedClassLoader
	 */
	public JavaClassLoader getAssociatedClassLoader() {
		return _associatedClassLoader;
	}

	/**
	 * @param associatedClassLoader the associatedClassLoader to set
	 */
	public void setAssociatedClassLoader(JavaClassLoader associatedClassLoader) {
		_associatedObjectFlags |= CLASSLOADER_FLAG;
		_associatedClassLoader = associatedClassLoader;
	}
	
	public String toString() {
		try {
			String className = getJavaClass().getName();
			return "Instance of " + className + " @ "+ _basePointer;
		} catch (CorruptDataException e) {
			return super.toString();
		}
	}
}
