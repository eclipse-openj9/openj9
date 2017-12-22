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
 * Represents a native operating system thread.
 * 
 */
public interface ImageThread {

    /**
     * Fetch a unique identifier for the thread.
     * In many operating systems, threads have more than one identifier (e.g.
     * a thread id, a handle, a pointer to VM structures associated with the thread).
     * In this case, one of these identifiers will be chosen as the canonical
     * one. The other identifiers would be returned by getProperties()
     * 
     * @return a process-wide identifier for the thread (e.g. a tid number)
     * @throws CorruptDataException 
     */
    public String getID() throws CorruptDataException;
 
    /**
     * Get the set of stack frames on this thread.
     * @return an iterator to walk the native stack frames in order from 
     * top-of-stack (that is, the most recent frame) to bottom-of-stack. Throws
     * DataUnavailable if native stack frames are not available on this platform.
     * 
     * @throws DataUnavailable If native stack frames are not available on this platform
     * @see ImageStackFrame
     * @see CorruptData
     * 
     */
    public Iterator getStackFrames() throws DataUnavailable;
    
    /**
     * Get the set of image sections which make up the stack.
     * @return a collection of ImageSections which make up the stack. On
     * most platforms this consists of a single entry, but on some platforms
     * the thread's stack may consist of non-contiguous sections
     * 
     * @see ImageSection
     * @see CorruptData
     */
    public Iterator getStackSections();
    
    /**
     * Get the register contents.
     * @return an iterator to iterate over the state of the CPU registers
     * when the image was created. The collection may be empty if the register 
     * state is not available for this thread.
     * 
     * If the CPU supports partial registers (e.g. AH, AL, AX, EAX, RAX on
     * AMD64), only the largest version of the register will be included
     * 
     * @see ImageRegister
     */
    public Iterator getRegisters();
    
    /**
     * Get the OS-specific properties for this thread.
     * @return a table of OS-specific properties for this thread.
     * Values which are commonly available include
     * <ul>
     * <li>"priority" -- the priority of the thread</li>
     * <li>"policy" -- the scheduling policy of the thread</li>
     * </ul> 
     */
    public Properties getProperties();
}
