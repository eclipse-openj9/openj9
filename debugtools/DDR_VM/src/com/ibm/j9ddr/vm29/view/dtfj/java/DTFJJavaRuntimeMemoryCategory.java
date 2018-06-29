/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaRuntimeMemoryCategory;
import com.ibm.dtfj.java.JavaRuntimeMemorySection;
import com.ibm.j9ddr.util.IteratorHelpers;
import com.ibm.j9ddr.util.IteratorHelpers.IteratorFilter;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.vm29.pointer.generated.OMRMemCategoryPointer;
import com.ibm.j9ddr.vm29.pointer.helper.OMRMemCategoryHelper;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

import static com.ibm.j9ddr.vm29.pointer.helper.OMRMemCategoryHelper.IOMRMemCategoryVisitor;

public class DTFJJavaRuntimeMemoryCategory implements JavaRuntimeMemoryCategory
{

	private final OMRMemCategoryPointer category;
	private final JavaRuntime runtime;
	
	public DTFJJavaRuntimeMemoryCategory(JavaRuntime runtime, OMRMemCategoryPointer category)
	{
		this.category = category;
		this.runtime = runtime;
	}
	
	class ChildIterator implements Iterator<Object>
	{
		private final OMRMemCategoryPointer category;
		private Object next = null;
		private int index = 0;
		
		public ChildIterator(OMRMemCategoryPointer c)
		{
			this.category = c;
		}
		
		private Object internalNext()
		{
			int numberOfChildren = 0;
			try {
				numberOfChildren =  category.numberOfChildren().intValue();
			} catch (com.ibm.j9ddr.CorruptDataException e) {
				return null;
			}
			
			if (index >= numberOfChildren) {
				return null;
			}
			
			try {
				UDATA childCode = category.children().at(index++);
				OMRMemCategoryPointer childCategory = OMRMemCategoryHelper.getMemoryCategory(childCode);
				
				if (childCategory.categoryCode().eq(childCode)) {
					return new DTFJJavaRuntimeMemoryCategory(runtime,childCategory);
				} else {
					/* getMemoryCategory will map any code it doesn't understand to the unknown category. If that happens
					 * we view that as corruption
					 */
					return J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Unknown child category code " + childCode.getHexValue());
				}
			} catch (Throwable e) {
				CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), e);
				return cd;
			}
		}
		
		public boolean hasNext()
		{
			if (next != null) {
				return true;
			} else {
				next = internalNext();
				return next != null;
			}
		}

		public Object next()
		{
			if (hasNext()) {
				Object toReturn = next;
				next = null;
				return toReturn;
			} else {
				throw new NoSuchElementException();
			}
		}

		public void remove()
		{ 
			throw new UnsupportedOperationException();
		}
		
	}
	
	public Iterator<?> getChildren() throws CorruptDataException
	{
		return new ChildIterator(category);
	}

	private static class DeepAllocationVisitor implements IOMRMemCategoryVisitor
	{
		long deepAllocations = 0;
		
		public void visit(OMRMemCategoryPointer category)
				throws com.ibm.j9ddr.CorruptDataException
		{
			deepAllocations += category.liveAllocations().longValue();
		}
	}
	
	public long getDeepAllocations() throws CorruptDataException
	{
		DeepAllocationVisitor v = new DeepAllocationVisitor();
		
		try {
			OMRMemCategoryHelper.visitMemoryCategoryChildren(category, v);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		
		return v.deepAllocations;
	}

	private static class DeepBytesVisitor implements IOMRMemCategoryVisitor
	{
		long deepBytes = 0;
		
		public void visit(OMRMemCategoryPointer category)
				throws com.ibm.j9ddr.CorruptDataException
		{
			deepBytes += category.liveBytes().longValue();
		}
	}
	
	public long getDeepBytes() throws CorruptDataException
	{
		DeepBytesVisitor v = new DeepBytesVisitor();
		
		try {
			OMRMemCategoryHelper.visitMemoryCategoryChildren(category, v);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		
		return v.deepBytes;
	}

	@SuppressWarnings("unchecked")
	public Iterator<?> getMemorySections(boolean includeFreed) throws CorruptDataException, DataUnavailable
	{
		return IteratorHelpers.filterIterator(runtime.getMemorySections(includeFreed), new IteratorFilter<Object>() {

			public boolean accept(Object obj)
			{
				if (obj instanceof JavaRuntimeMemorySection) {
					try {
						if (DTFJJavaRuntimeMemoryCategory.this.equals(((JavaRuntimeMemorySection)obj).getMemoryCategory())) {
							return true;
						}
					} catch (Throwable t) {
						J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
						//Fall through
					}
					return false;
				} else {
					/* Any other objects will be CorruptData. Pass these through. */
					return true;
				}
			}
			
		});
	}

	public String getName() throws CorruptDataException
	{
		try {
			return category.name().getCStringAtOffset(0);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public long getShallowAllocations() throws CorruptDataException
	{
		try {
			return category.liveAllocations().longValue();
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public long getShallowBytes() throws CorruptDataException
	{
		try {
			return category.liveBytes().longValue();
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result
				+ ((category == null) ? 0 : category.hashCode());
		result = prime * result + ((runtime == null) ? 0 : runtime.hashCode());
		return result;
	}

	@Override
	public boolean equals(Object obj)
	{
		if (this == obj) {
			return true;
		}
		if (obj == null) {
			return false;
		}
		if (!(obj instanceof DTFJJavaRuntimeMemoryCategory)) {
			return false;
		}
		DTFJJavaRuntimeMemoryCategory other = (DTFJJavaRuntimeMemoryCategory) obj;
		if (category == null) {
			if (other.category != null) {
				return false;
			}
		} else if (!category.equals(other.category)) {
			return false;
		}
		if (runtime == null) {
			if (other.runtime != null) {
				return false;
			}
		} else if (!runtime.equals(other.runtime)) {
			return false;
		}
		return true;
	}
}
