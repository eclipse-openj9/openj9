/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2020 IBM Corp. and others
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

import java.nio.ByteOrder;
import java.util.Iterator;
import java.util.Properties;

/**
 * <p>Represents a single address space within the image.</p>
 *  
 * <p>On some operating systems (e.g. z/OS), there can be more than 
 * one address space per core file and more than one process per address space.</p> 
 */
public interface ImageAddressSpace {

    /**
     * Get the process within this address space that caused the image to be created.
     * @return the process within this address space which caused 
     * the image to be created, if any. Return null if no individual 
     * process triggered the creation of the image.
     */
    public ImageProcess getCurrentProcess();
    
    /**
     * Get the set of processes within the address space.
     * @return an iterator which provides all of the processes
     * within a given address space.
     * 
     * @see ImageProcess
     * @see CorruptData
     */
    public Iterator getProcesses();

	/**
	 * Return the byte order of this address space.
	 *
	 * @return the byte order of this address space
	 */
	public ByteOrder getByteOrder();

    /**
     * A factory method for creating pointers into this address space.
     * @param address the address to point to.
     * @return an ImagePointer for the specified address.
     */
    public ImagePointer getPointer(long address);
    
    /**
     * Get the raw memory in the address space.
     * 
     * @return An iterator of all the ImageSections in the address.  Their union will be the total process address space.
     * @see ImageSection
     */
    public Iterator getImageSections();
    
    /**
     * Gets the system wide identifier for the address space
     * 
     * @return address space ID
     * @since 1.7
     */
    public String getID() throws DataUnavailable, CorruptDataException;
    
    /**
     * Gets the OS specific properties for this address space.
     * 
     * @return a set of OS specific properties
     * @since 1.7
     */
    public Properties getProperties();
}
