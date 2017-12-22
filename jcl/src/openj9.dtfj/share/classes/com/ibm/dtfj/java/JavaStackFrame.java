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
import com.ibm.dtfj.image.ImagePointer;

/**
 * Represents a frame on a Java thread stack.
 */
public interface JavaStackFrame {
    /**
     * Get a pointer to the base of this stack frame
     * @return the base pointer of the stack frame
     * @throws CorruptDataException 
     */
    public ImagePointer getBasePointer() throws CorruptDataException;
    
	/**
     * Get the location at which the method owning this frame is currently executing
     * @return a location object describing where the frame
     * is executing
	 * @throws CorruptDataException 
     * 
     * @see JavaLocation
     * 
     */
    public JavaLocation getLocation() throws CorruptDataException;
    
    /**
     * Get the set of object roots from this stack frame.
	 *
     * @return an iterator of JavaReferences
     */
	public Iterator getHeapRoots();                      

	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Stack Frame in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
}
