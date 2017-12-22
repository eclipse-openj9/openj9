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
 * Represents a contiguous range of memory in an address space. 
 */
public interface ImageSection 
{
    
    /**
     * Gets the base address (the lowest) of memory in this section.
     * 
     * @return Base address pointer.
     */
    public ImagePointer getBaseAddress();
    
    /**
     * Gets the size of the memory section.
     * 
     * @return Size of section in bytes.
     */
    public long getSize();
    
    /**
     * Gets the name of this section.
     * 
     * Some memory sections are named. For example, the executable data in a module is typically called ".text".
     * 
     * For memory sections without a specific name, a placeholder string will be returned. This method will never
     * return null.
     * 
     * @return non-null name String.
     */
    public String getName();
    
    /**
     * Tests executable permission on memory section.
     * 
     * @return true if the memory pages in this section are marked executable. False otherwise. 
     * @throws DataUnavailable 
     */
    public boolean isExecutable() throws DataUnavailable;
    
    /**
     * Tests read permission on memory section.
     * 
     * @return true if the memory pages in this section are marked read-only. False otherwise.
     * @throws DataUnavailable 
     */
    public boolean isReadOnly() throws DataUnavailable;
    
    /**
     * Tests shared permission on memory section.
     * 
     * @return true if this section is shared between processes. False otherwise.
     * @throws DataUnavailable 
     */
    public boolean isShared() throws DataUnavailable;

    /**
     * Get the OS-specific properties for this section.
     * @return a table of OS-specific properties for this section.
     * Values which are commonly available include
     * <ul>
     * <li>"readable" -- whether the memory section can be read from</li>
     * <li>"writable" -- whether the memory section can be written to</li>
     * <li>"executable" -- whether data in the memory section can be executed</li>
     * </ul> 
     * @since 1.11
     */
    public Properties getProperties();
    
}
