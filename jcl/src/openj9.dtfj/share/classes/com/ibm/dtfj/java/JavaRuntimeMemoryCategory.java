/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2010, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;

/**
 * <p>Represents a category of native memory allocated by the Java runtime.</p>
 * 
 * <p>A category is a high-level grouping of memory allocations
 * such as "Threads", "Classes" or "Java Heap".</p>
 *
 * <p>Some Java runtime implementations use categories to track native memory
 * use in the JRE.</p>
 *
 * <p>Categories can have child categories and form a hierarchy.</p>
 *
 * <p> The getShallow* methods return the allocation data for
 * just this category. The getDeep* methods return the allocation
 * data for this category and all its child categories. </p>
 * 
 * @since 1.5
 */
public interface JavaRuntimeMemoryCategory
{
	/**
	 * Gets the name of this category. E.g. "Classes".
	 * 
	 * @return Name string.
	 */
	public String getName() throws CorruptDataException;
	
	/**
	 * Gets number of allocations recorded against this category.
	 * 
	 * @return Allocation count.
	 */
	public long getShallowAllocations() throws CorruptDataException;
	
	/**
	 * Gets number of bytes allocated under this category.
	 * 
	 * @return Number of bytes.
	 */
	public long getShallowBytes() throws CorruptDataException;

	/**
	 * Gets number of allocations recorded against this category, and all children of this category.
	 * 
	 * @return Allocation count.
	 */	
	public long getDeepAllocations() throws CorruptDataException;
	
	/**
	 * Gets number of bytes recorded against this category, and all children of this category.
	 * 
	 * @return Number of bytes.
	 */	
	public long getDeepBytes() throws CorruptDataException;
	
	/**
	 * Gets iterator of child categories.
	 * 
	 * @return Iterator of JavaRuntimeMemoryCategory objects that are immediate children of this category.
	 */
	public Iterator getChildren() throws CorruptDataException;
	
	/**
	 * Gets iterator of memory sections allocated against this category.
	 * 
	 * @param includeFreed If true, iterator will iterate over blocks of memory that have been freed, but haven't been re-used yet.
	 * @see JavaRuntimeMemorySection CorruptData
	 * @return Iterator of memory sections
	 */
	public Iterator getMemorySections(boolean includeFreed) throws CorruptDataException, DataUnavailable;
}
