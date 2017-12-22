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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageSection;

/**
 * Represents a native memory range allocated by the Java Runtime.
 * 
 * @since 1.5
 */
public interface JavaRuntimeMemorySection extends ImageSection
{

	/**
	 * Type code for memory allocated on the native heap via an API such as malloc().
	 *
	 * Covers malloc'd memory that hasn't been freed
	 * @see #getAllocationType
	 */
	public static final int ALLOCATION_TYPE_MALLOC_LIVE = 1;
	
	/**
	 * Type code for memory that was allocated, and freed, by the Java runtime on the native heap using malloc() or similar.
	 * @see #getAllocationType
	 */
	public static final int ALLOCATION_TYPE_MALLOC_FREED = 2;
	
	/**
	 * Type code for memory allocated for memory mapping files through an API like mmap().
	 * @see #getAllocationType
	 */
	public static final int ALLOCATION_TYPE_MEMORY_MAPPED_FILE = 3;
	
	/**
	 * Type code for anonymous memory mappings / virtual allocations.
	 * @see #getAllocationType
	 */
	public static final int ALLOCATION_TYPE_VIRTUAL_ALLOC = 4;
	
	/**
	 * Type code for shared memory sections.
	 * @see #getAllocationType
	 */
	public static final int ALLOCATION_TYPE_SHARED_MEMORY = 5;

	/**
	 * Returns string describing the code that allocated this memory section.
	 * 
	 * @return Allocator string.
	 */
	public String getAllocator() throws CorruptDataException, DataUnavailable;
	
	/**
	 * Returns memory category this section was allocated under.
	 * 
	 * @return Memory category.
	 */
	public JavaRuntimeMemoryCategory getMemoryCategory() throws CorruptDataException, DataUnavailable;
	
	/**
	 * Get memory allocation type code.
	 * 
	 * @return Type code.
	 */
	public int getAllocationType();
}
