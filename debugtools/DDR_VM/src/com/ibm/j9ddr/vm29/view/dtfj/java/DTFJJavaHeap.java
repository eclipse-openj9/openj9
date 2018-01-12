/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import static com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils.corruptIterator;
import static com.ibm.j9ddr.vm29.events.EventManager.register;
import static com.ibm.j9ddr.vm29.events.EventManager.unregister;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.NoSuchElementException;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.util.IteratorHelpers;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageSection;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MemorySpacePointer;
import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionDescriptorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionManagerPointer;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;
import com.ibm.j9ddr.vm29.view.dtfj.java.j9.HeapObjectIterator;

public class DTFJJavaHeap implements JavaHeap {
	private final MM_MemorySpacePointer space;
	private String name;
	private String description;
	private ImagePointer id;
	List<GCHeapRegionDescriptor> regions;
	List<Object> objects = null;
	List<ImageSection> sections = null;
	
	/*
	 * A JavaHeap is essentially equivalent to a J9MemorySpace (in heapIterator code) 
	 */
	public DTFJJavaHeap(MM_MemorySpacePointer space, String name, ImagePointer id) throws com.ibm.j9ddr.CorruptDataException
	{
		this.space = space;
		this.name = name;
		this.id = id;
		initRegions(); 
	}
	
	@SuppressWarnings("unchecked")
	private void initRegions() throws com.ibm.j9ddr.CorruptDataException
	{
		MM_GCExtensionsPointer gcext = GCExtensions.getGCExtensionsPointer();
		MM_HeapRegionManagerPointer hrm = gcext.heapRegionManager();
		regions = IteratorHelpers.toList(GCHeapRegionIterator.fromMMHeapRegionManager(hrm, space, true, true));

	}
	
	public String getName() {
		return name + "@" + id;
	}

	@SuppressWarnings("rawtypes")
	public Iterator getObjects() 
	{			
		return new Iterator()
		{
			class CurrentRegionListener implements IEventListener {
				
				public void corruptData(String message, CorruptDataException e,
						boolean fatal) {
					if( fatal ) {
						currentRegionIterator = corruptIterator(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), e));
					}
					
				}
			};

			Iterator currentRegionIterator = null;
			Iterator<GCHeapRegionDescriptor> regionsIterator = regions.iterator();

			public boolean hasNext()
			{
				IEventListener currentRegionlistener = new CurrentRegionListener();
				try {
					register(currentRegionlistener);
					if(null == currentRegionIterator || !currentRegionIterator.hasNext()) {
						while(regionsIterator.hasNext()) {
							try {
								currentRegionIterator = new HeapObjectIterator(DTFJJavaHeap.this, regionsIterator.next());
							} catch (Throwable t) {
								CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
								currentRegionIterator = corruptIterator(cd);
							}
							if(currentRegionIterator.hasNext()) {
								return true;
							}
						}
					}

					if(null != currentRegionIterator) {
						return currentRegionIterator.hasNext();
					}
					return false;
				} finally {
					unregister(currentRegionlistener);
				}
			}
			
			public Object next()
			{
				if(hasNext()) {
					return currentRegionIterator.next();
				}
				
				throw new NoSuchElementException();
			}
			
			public void remove()
			{
				throw new UnsupportedOperationException("Remove not supported");
			}
			
		};
	}

	@SuppressWarnings({ "rawtypes", "unchecked" })
	public Iterator getSections() {
		try {
			if(null == sections) {
				List sectionList = new ArrayList<ImageSection>();
				
				Iterator<GCHeapRegionDescriptor> it = regions.iterator();
				while (it.hasNext()) {
					try {
						GCHeapRegionDescriptor region = it.next();
						long base = region.getLowAddress().getAddress();
						long size = region.getHighAddress().getAddress() - base;
						String name = String.format("Heap extent at 0x%x (0x%x bytes)", base, size);
						sectionList.add(new J9DDRImageSection(MM_HeapRegionDescriptorPointer.getProcess(), base, size, name));
					} catch (Throwable t) {
						CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
						sectionList.add(cd);
					}
				}
				sections = sectionList;
			}
			
			return sections.iterator();
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return corruptIterator(cd);
		} 
	}

	@Override
	public boolean equals(Object obj) {
		if((obj == null) || !(obj instanceof DTFJJavaHeap)) {
			return false;
		}
		DTFJJavaHeap compareTo = (DTFJJavaHeap) obj;
		return space.eq(compareTo.space);
	}

	@Override
	public int hashCode() {
		return space.hashCode();
	}

	@Override
	public String toString() {
		if(description == null) {
			try { 
				U8Pointer minBase = U8Pointer.cast(-1);
				U8Pointer maxTop = U8Pointer.cast(0);
				
				String name = space._name().getCStringAtOffset(0);
				Iterator<GCHeapRegionDescriptor> it = regions.iterator();
				while (it.hasNext()) {
					GCHeapRegionDescriptor region = it.next();
					U8Pointer base = U8Pointer.cast(region.getLowAddress());
					U8Pointer top = U8Pointer.cast(region.getHighAddress());
					if(base.lt(minBase)) {
						minBase = base;
					}
					if(top.gt(maxTop)) {
						maxTop = top;
					}
				}
				
				description = String.format("%s [%s, Span: 0x%08x->0x%08x, Regions: %d]", getName(), name, minBase.getAddress(), maxTop.getAddress(), regions.size());
			} catch (Throwable t) {
				J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
				description = super.toString();
			} 
		}
		return description;
	}
	
	
	
}
