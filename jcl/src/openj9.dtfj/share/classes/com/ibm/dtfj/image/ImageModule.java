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

import java.util.Iterator;
import java.util.Properties;

/**
 * Represents a shared library loaded into the image, or the executable module itself.
 */
public interface ImageModule {

    /**
     * Get the file name of the shared library.
     * @return the file name of the shared library.
     * 
     * @throws CorruptDataException If the module is corrupt and the 
     * original file cannot be determined.
     */
    public String getName() throws CorruptDataException;
    
    /**
     * Get the collection of sections that make up this library.
     * @return a collection of sections that make up this library.
     * 
     * @see ImageSection
     * @see CorruptData
     */
    public Iterator getSections();
    
    /**
     * Provides a collection of symbols defined by the library. This
     * list is likely incomplete as many symbols may be private,
     * symbols may have been stripped from the library, or symbols may
     * not by available in the image.
     * 
     * @return a collection of symbols which are defined by this library.
     * 
     * @see ImageSymbol
     * @see CorruptData
     */
    public Iterator getSymbols();
    
    /**
     * Get the table of properties associated with this module.
     * @return a table of properties associated with this module.
     * Values typically defined in this table include:
     * <ul>
     * <li>"version" -- version information about the module</li>
     * </ul>
     * @throws CorruptDataException 
     */
    public Properties getProperties() throws CorruptDataException;
    
    /**
     * Get the address at which the module or executable was loaded.
     * 
     * @return the address 
     * @throws CorruptDataException
     * @throws DataUnavailable
     */
    public long getLoadAddress() throws CorruptDataException, DataUnavailable;
}
