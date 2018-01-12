/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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

import java.util.Arrays;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.MemoryAccessException;

/**
 * @author jmdisher
 *
 */
public class JavaHeap implements com.ibm.dtfj.java.JavaHeap
{
	private JavaRuntime _javaVM;
	private String _name;
	private ImagePointer _id;
	private long _size;
	
	/**
	 * Contains an ordered list of all the pairings of regions and image sections in the heap in address order so we can search through them faster
	 */
	private HeapSubRegionSection[] _allSortedRegionSections;
	
	/**
	 * All the JavaHeapRegions which make up the heap (note that this is always one in a legacy heap)
	 */
	private List _heapRegions;
	private int _arrayletIdOffset;
	private int _arrayletIdWidth;
	private long _arrayletIdMask;
	private long _arrayletIdResult;
	private int _fobjectSize;
	private int _fobjectPointerScale;
	private long _fobjectPointerDisplacement;
	private int _classOffset;
	private int _classSize;
	private long _classAlignment;
	private boolean _isSWH;
	
	//required for binary search and generally looking for ImagePointers inside JavaHeapRegions
	private static RegionMatcher _matcher = new RegionMatcher();
	
	
	public JavaHeap(JavaRuntime vm, String name, ImagePointer id, ImagePointer start, long size, int arrayletIdOffset, int arrayletIdWidth, long arrayletIdMask, long arrayletIdResult, int fobjectSize, int fobjectPointerScale, long fobjectPointerDisplacement, int classOffset, int classSize, long classAlignment, boolean isSWH)
	{
		if (null == vm) {
			throw new IllegalArgumentException("Java VM for a heap cannot be null");
		}
		if (null == name) {
			throw new IllegalArgumentException("JavaHeap name cannot be null");
		}
		_javaVM = vm;
		_id = id;
		_name = name;
		_size = size;
		
		_arrayletIdOffset = arrayletIdOffset;
		_arrayletIdWidth = arrayletIdWidth;
		_arrayletIdMask = arrayletIdMask;
		_arrayletIdResult = arrayletIdResult;
		
		_fobjectSize = fobjectSize;
		_fobjectPointerScale = fobjectPointerScale;
		_fobjectPointerDisplacement = fobjectPointerDisplacement;
		_classOffset = classOffset;
		_classSize = classSize;
		_classAlignment = classAlignment;
		_isSWH = isSWH;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaHeap#getSections()
	 */
	public Iterator getSections()
	{
		return new MultiLevelSectionIterator(_heapRegions);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaHeap#getName()
	 */
	public String getName()
	{
		//TODO:  In the future, we will need a getID method on the JavaHeap interface and, at that time, this can be changed to just return _name
		return _name + "@" + _id;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaHeap#getObjects()
	 */
	public Iterator getObjects()
	{
		return new MultiLevelExtentWalker(_heapRegions);
	}
	
	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if ((null != _name) && (obj instanceof JavaHeap)) {
			JavaHeap local = (JavaHeap) obj;
			
			isEqual = (_javaVM.equals(local._javaVM))
				&& (_name.equals(local._name))
				&& (_id.equals(local._id)
				&& (_size == local._size));
		}
		return isEqual;
	}

	public int hashCode()
	{
		return 	(_javaVM.hashCode() ^ _name.hashCode() ^ _id.hashCode() ^ ((int) _size));
	}
	
	private class MultiLevelSectionIterator implements Iterator
	{
		private Iterator _outer;
		private Iterator _inner;
		
		public MultiLevelSectionIterator(List list)
		{
			_outer = list.iterator();
		}
		
		public boolean hasNext()
		{
			_refreshInner();
			return _inner.hasNext();
		}

		public Object next()
		{
			_refreshInner();
			return _inner.next();
		}

		public void remove()
		{
			throw new UnsupportedOperationException("The core-resident Java heap is immutable");
		}
		
		private void _refreshInner()
		{
			if ((null == _inner) || (!_inner.hasNext())) {
				if (_outer.hasNext()) {
					_inner = ((JavaHeapRegion)(_outer.next())).getSections();
				} else {
					_inner = _outer;
				}
			}
		}
	}
	
	private class MultiLevelExtentWalker implements Iterator
	{
		private Iterator _outer;
		private Iterator _inner;

		public MultiLevelExtentWalker(List list)
		{
			_outer = list.iterator();
		}
		
		public boolean hasNext()
		{
			_refreshInner();
			return _inner.hasNext();
		}

		public Object next()
		{
			_refreshInner();
			return _inner.next();
		}

		public void remove()
		{
			throw new UnsupportedOperationException("The core-resident Java heap is immutable");
		}
		
		private void _refreshInner()
		{
			if ((null == _inner) || (!_inner.hasNext())) {
				if (_outer.hasNext()) {
					do {
						_inner = ((JavaHeapRegion)(_outer.next())).getObjects();
					} while ((!_inner.hasNext()) && (_outer.hasNext()));
				} else {
					_inner = _outer;
				}
			}
		}
	}
	
	private static class RegionMatcher implements Comparator
	{
		public int compare(Object arg0, Object arg1)
		{
			HeapSubRegionSection region = (HeapSubRegionSection) arg0;
			ImagePointer pointer = (ImagePointer) arg1;
			int result = -1;
			ImageSection section = region.getSection();
			long base = section.getBaseAddress().getAddress();
			long offset = pointer.getAddress() - base;
			
			if (offset < 0) { 
				//the address was less than the base so it is a region before this one
				result = 1;
			} else if (offset < section.getSize()) {
				//the address is inside the range to return a match
				result = 0;
			} else {
				//the address was outside of the region and not below the base so it is in a region after this one
				result = -1;
			}
			return result;
		}
	}
	
	public JavaHeapRegion regionForPointer(ImagePointer address)
	{
		JavaHeapRegion match = null;
		int foundIndex = Arrays.binarySearch(_allSortedRegionSections, address, _matcher);
		
		if ((foundIndex >= 0) && (foundIndex < _allSortedRegionSections.length)) {
			match = _allSortedRegionSections[foundIndex].getRegion();
		}
		return match;
	}
	
	public int getArrayletIdentificationWidth()
	{
		return _arrayletIdWidth;
	}

	public int getArrayletIdentificationOffset()
	{
		return _arrayletIdOffset;
	}
	
	public long getArrayletIdentificationBitmask()
	{
		return _arrayletIdMask;
	}

	public long getArrayletIdentificationResult()
	{
		return _arrayletIdResult;
	}

	public void setRegions(Vector regions)
	{
		//store the regions
		_heapRegions = regions;
		//now make the sorted index of the subsections within all the regions for faster look-up, later on
		Iterator outer = regions.iterator();
		Vector heapSubRegionSections = new Vector();
		while (outer.hasNext()) {
			JavaHeapRegion region = (JavaHeapRegion) outer.next();
			Iterator sections = region.getSections();
			while (sections.hasNext()) {
				ImageSection section = (ImageSection)sections.next();
				heapSubRegionSections.add(new HeapSubRegionSection(region, section));
			}
		}
		//now sort the sub-regions
		_allSortedRegionSections = (HeapSubRegionSection[]) heapSubRegionSections.toArray(new HeapSubRegionSection[heapSubRegionSections.size()]);
		Arrays.sort(_allSortedRegionSections, new Comparator(){
			public int compare(Object arg0, Object arg1)
			{
				HeapSubRegionSection one = (HeapSubRegionSection)arg0;
				HeapSubRegionSection two = (HeapSubRegionSection)arg1;
				long delta =  one.base() - two.base();
				int result = 0;
				if (delta < 0) {
					result = -1;
				} else if (delta > 0) {
					result = 1;
				}
				return result;
			}
		});
	}
	
	private class HeapSubRegionSection
	{
		private JavaHeapRegion _region;
		private ImageSection _section;
		
		private HeapSubRegionSection(JavaHeapRegion region, ImageSection section)
		{
			_region = region;
			_section = section;
		}
		
		public ImageSection getSection()
		{
			return _section;
		}

		public JavaHeapRegion getRegion()
		{
			return _region;
		}

		private long base()
		{
			return _section.getBaseAddress().getAddress();
		}
	}

	public int getFObjectSize()
	{
		return _fobjectSize;
	}
	
	public long tokenToPointer(long fobject)
	{
		return (fobject * _fobjectPointerScale) + _fobjectPointerDisplacement;
	}

	public ImagePointer readClassPointerRelativeTo(ImagePointer pointer) throws MemoryAccessException, CorruptDataException
	{
		long classPointer;
		
		if (8 == _classSize)
		{
			//8-byte fj9object_t
			classPointer = pointer.getLongAt(_classOffset);
		}
		else if (4 == _classSize)
		{
			//4-byte fj9object_t
			classPointer = 0xFFFFFFFFL & pointer.getIntAt(_classOffset);
		}
		else
		{
			throw new IllegalArgumentException("Heap has unexpected class pointer size: " + _classSize);
		}
		return _id.getAddressSpace().getPointer(classPointer);
	}

	public ImagePointer readFObjectAt(ImagePointer basePointer, long offset) throws MemoryAccessException, CorruptDataException
	{
		long pointer;
		
		if (8 == _fobjectSize)
		{
			//8-byte fj9object_t
			pointer = tokenToPointer(basePointer.getLongAt(offset));
		}
		else if (4 == _fobjectSize)
		{
			//4-byte fj9object_t
			pointer = tokenToPointer(0xFFFFFFFFL & basePointer.getIntAt(offset));
		}
		else
		{
			throw new IllegalArgumentException("Heap has unexpected size reference fields: " + _fobjectSize);
		}
		return _id.getAddressSpace().getPointer(pointer);
	}

	public long getClassAlignment() {
		return _classAlignment;
	}

	public boolean isSWH() {
		return _isSWH;
	}
}
