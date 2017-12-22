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
package com.ibm.dtfj.image;

import java.util.Properties;

/**
 * Represents an address in memory. To create an ImagePointer using an absolute address use
 * <code>com.ibm.dtfj.image.ImageAddressSpace.getPointer()</code>
 * 
 * @see com.ibm.dtfj.image.ImageAddressSpace#getPointer(long)
 */
public interface ImagePointer {

    /**
     * <p>Get the unwrapped address, represented as a Java long.</p>
     * <p> Use caution when comparing addresses, as some addresses may be
     * negative. </p>
     * <p>Note that, on segmented memory architectures, it may not be 
     * possible to represent all addresses accurately as integers.</p>
     * @return the unwrapped address, represented as a 64-bit integer.
     */
    public long getAddress();
    
    /**
     * Get the address space to which this pointer belongs.
     * @return the address space to which this pointer belongs.
     */
    public ImageAddressSpace getAddressSpace();
    
    /**
     * Build a new image pointer offset from this one by the given amount.
     * @param offset Offset in bytes.
     * @return a new ImagePointer based at getAddress() + offset
     */
    public ImagePointer add(long offset);
    
    /**
     * Tests memory execute permission.
     * @return true if this memory address is within an executable page.
     * @throws DataUnavailable 
     */
    public boolean isExecutable() throws DataUnavailable;
    
    /**
     * Tests memory read/write permission.
     * @return true if this memory address is read-only. False otherwise.
     * @throws DataUnavailable 
     */
    public boolean isReadOnly() throws DataUnavailable;
    
    /**
     * Tests memory shared permission.
     * @return true if this memory address is shared between processes.
     * @throws DataUnavailable 
     */
    public boolean isShared() throws DataUnavailable;
    
    /**
     * Get the value at the given offset from this pointer. To determine the number of bytes to skip after this
     * call to read the next value, use <code>ImageProcess.getPointerSize()</code>. Note: to create an 
     * ImagePointer using an absolute address use <code>com.ibm.dtfj.image.ImageAddressSpace.getPointer()</code>
     * @param index  an offset (in bytes) from the current position
     * @return the 32 or 64-bit pointer stored at getAddress() + index 
     * @throws MemoryAccessException if the memory cannot be read
     * @throws CorruptDataException if the memory should be in the image, but is missing or corrupted
     * @see    ImageProcess#getPointerSize()
     * @see    ImageAddressSpace#getPointer(long)
     */
    public ImagePointer getPointerAt(long index) throws MemoryAccessException, CorruptDataException;
    
    /**
     * Get the value at the given offset from this pointer.
     * @param index  an offset (in bytes) from the current position
     * @return the 64-bit long stored at getAddress() + index 
     * @throws MemoryAccessException if the memory cannot be read
     * @throws CorruptDataException if the memory should be in the image, but is missing or corrupted
     */
    public long getLongAt(long index)  throws MemoryAccessException, CorruptDataException;
    
    /**
     * Get the value at the given offset from this pointer.
     * @param index  an offset (in bytes) from the current position
     * @return the 32-bit int stored at getAddress() + index 
     * @throws MemoryAccessException if the memory cannot be read
     * @throws CorruptDataException if the memory should be in the image, but is missing or corrupted
     */
    public int getIntAt(long index) throws MemoryAccessException, CorruptDataException;
    
    /**
     * Get the value at the given offset from this pointer.
     * @param index  an offset (in bytes) from the current position
     * @return the 16-bit short stored at getAddress() + index 
     * @throws MemoryAccessException if the memory cannot be read
     * @throws CorruptDataException if the memory should be in the image, but is missing or corrupted
     */
    public short getShortAt(long index) throws MemoryAccessException, CorruptDataException;
    
    /**
     * Get the value at the given offset from this pointer.
     * @param index  an offset (in bytes) from the current position
     * @return the 8-bit byte stored at getAddress() + index 
     * @throws MemoryAccessException if the memory cannot be read
     * @throws CorruptDataException if the memory should be in the image, but is missing or corrupted
     */
    public byte getByteAt(long index) throws MemoryAccessException, CorruptDataException;
    
    /**
     * Get the value at the given offset from this pointer.
     * @param index  an offset (in bytes) from the current position
     * @return the 32-bit float stored at getAddress() + index
     * @throws MemoryAccessException if the memory cannot be read
     * @throws CorruptDataException if the memory should be in the image, but is missing or corrupted
     */
    public float getFloatAt(long index) throws MemoryAccessException, CorruptDataException;

    /**
     * Get the value at the given offset from this pointer.
     * @param index  an offset (in bytes) from the current position
     * @return the 64-bit double stored at getAddress() + index 
     * @throws MemoryAccessException if the memory cannot be read
     * @throws CorruptDataException if the memory should be in the image, but is missing or corrupted
     */
    public double getDoubleAt(long index) throws MemoryAccessException, CorruptDataException;
    
	/**
	 * @param obj
	 * @return True obj refers to the same Image Pointer in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();

    /**
     * Get the OS-specific properties for this address
     * @return a table of OS-specific properties for this address.
     * Values which are commonly available include
     * <ul>
     * <li>"readable" -- whether the memory address can be read from</li>
     * <li>"writable" -- whether the memory address can be written to</li>
     * <li>"executable" -- whether data in the memory address can be executed</li>
     * </ul> 
     * @since 1.11
     */
	public Properties getProperties();
}
