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
package com.ibm.dtfj.java;

import java.util.Iterator;

/**
 * <p>Represents a heap of managed objects.</p>
 * 
 * <p>There may be multiple heaps within a JVM, for instance a generational 
 * heap and a class heap. Additionally, heaps may consist of non-contiguous 
 * memory regions. For instance, an object heap may be divided into a hot 
 * and cold section.</p>
 * 
 */
public interface JavaHeap {
    
    /**
     * Get the set of contiguous memory regions which form this heap. 
     * @return an iterator over the collection of contiguous memory regions
     * which form this heap
     * 
     * @see com.ibm.dtfj.image.ImageSection
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getSections();
    
    /**
     * Get a brief textual description of this heap.
     * @return a brief textual description of this heap
     */
    public String getName();
    
    /**
     * Get the set of objects which are stored in this heap.
     * @return an iterator over the collection of objects which
     * are stored in this heap
     * 
     * @see JavaObject
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getObjects();
    
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Heap in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
}
