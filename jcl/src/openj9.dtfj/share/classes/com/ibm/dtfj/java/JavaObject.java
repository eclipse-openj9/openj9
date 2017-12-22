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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.MemoryAccessException;

/**
 * Represents a Java Object.
 */
public interface JavaObject {
    
    /**
     * Get the JavaClass instance which represents the class of this object.
     * @return the JavaClass instance which represents the class of this object
     * @throws CorruptDataException 
     */
    public JavaClass getJavaClass() throws CorruptDataException;
    
    /**
     * Is this object an array ?
     * @return true if the receiver represents an instance of an array, or
     * false otherwise
     * @throws CorruptDataException 
     */
    public boolean isArray()  throws CorruptDataException;
    
    /**
     * Get the number of elements in this array.
     * @return the number of elements in this array
     * <p>
     * @throws CorruptDataException 
     * @exception IllegalArgumentException if the receiver is not an array
     */
    public int getArraySize() throws CorruptDataException;
    
    /**
     * Copies data from the image array into a local Java array.
     * The dst object must be an array of the appropriate type --
     * a base type array for base types, or a JavaObject array
     * for reference arrays. 
     *  
     * @param srcStart index in the receiver to start copying from
     * @param dst the destination array
     * @param dstStart index in the destination array to start copying into
     * @param length the number of elements to be copied
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * 
     * @exception NullPointerException if dst is null
     * @exception IllegalArgumentException if the receiver is not an array,
     * or if dst is not an array of the appropriate type
     * @exception IndexOutOfBoundsException if srcStart, dstStart, or length
     * are out of bounds in either the receiver or the destination array
     */
    public void arraycopy(int srcStart, Object dst, int dstStart, int length)  throws CorruptDataException, MemoryAccessException;
    
    /**
     * Get the number of bytes of memory occupied by this object on the heap.
     * The memory may not necessarily be contiguous
     * @throws CorruptDataException 
     */
    public long getSize()  throws CorruptDataException;
    
    /**
     * Fetch the basic hash code for the object. This is the hash code which
     * would be returned if a Java thread had requested it. Typically the hash 
     * code is based on the address of an object, and may change if the object
     * is moved by a garbage collect cycle.
     * 
     * @return the basic hash code of the object in the image.
     * 
     * @see #getPersistentHashcode()
     * 
     * @exception DataUnavailable if the hash code cannot be determined
     * @throws CorruptDataException 
     */
    public long getHashcode() throws DataUnavailable, CorruptDataException;
    
    /**
     * Fetch the basic hash code of the object in the image. This hash code
     * is guaranteed to be persistent between multiple snapshots of the same Image.
     * If the hash code cannot be determined, or if the hash code for this object
     * could change between snapshots, an exception is thrown.
     * <p>
     * <i>If the VM uses a 'hasBeenHashed' bit, the value of this bit can be
     * inferred by calling getPersistentHashcode(). If the persistent hash code
     * is not available, then the 'hasBeenHashed' bit has not been set, and the
     * hash of the object could change if the object moves between snapshots</i>
     * 
     * @return the basic hash code of the object in the image
     * 
     * @see #getHashcode()
     * 
     * @exception DataUnavailable if a hash code cannot be determined, or if the
     * hash code could change between successive snapshots
     * @throws CorruptDataException 
     */
    public long getPersistentHashcode() throws DataUnavailable, CorruptDataException;
    
    /**
     * The ID of an object is a unique address is memory which identifies the object.
     * The data at this memory is implementation defined. The object may be 
     * non-contiguous. Portions of the object may appear below or above this address.
     * 
     * @return the runtime-wide unique identifier for the object
     */
	public ImagePointer getID();
	
    /**
     * An object is represented in the Java runtime by one or more regions of memory.
     * These include the object's header and the data in the object.
     *
     * In certain allocation strategies, an object's header and data may be allocated 
     * contiguously. In this case, this method may return an iterator for a single
     * section.
     * 
     * In other schemes, the header may be separate from the data or the data may be
     * broken up into multiple regions. Additionally, this function does not guarantee
     * that the memory used by this object is not also shared by one or more other
     * objects.
     * 
     * Callers should not make any assumptions about the contents of the memory. 
     * 
     * @return a collection of sections that make up this object
     * 
     * @see ImageSection
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getSections();
    
	/**
	 * Get the set of references from this object.
	 * <p>
	 * Corrupt references will be returned as CorruptData objects by the Iterator
	 *
	 * @return an iterator of JavaReference and CorruptData objects
	 * 
	 * @see com.ibm.dtfj.java.JavaReference
	 * @see com.ibm.dtfj.image.CorruptData
	 */
	public Iterator getReferences();
    
    /**
     * Gets the heap where this object is located.
     * @return	the <code>JavaHeap</code> instance representing the heap where this object is stored in memory.
     * @throws 	CorruptDataException	if the heap information for this object is corrupt.
     * @throws 	DataUnavailable			if the heap information for this object is not available.
     * @see 	com.ibm.dtfj.java.JavaHeap
     */
    public JavaHeap getHeap() throws CorruptDataException, DataUnavailable;
	
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Object in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
	

}
