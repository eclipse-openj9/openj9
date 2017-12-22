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
 * Represents a method or constructor in a class.
 */
public interface JavaMethod extends JavaMember {

    /**
     * Get the set of ImageSections containing the bytecode of this method.
     * @return an iterator over a collection of ImageSections.
     * Each ImageSection contains data (usually bytecodes) used
     * in executing this method in interpreted mode. 
     * <p>
     * The collection may be empty for native methods, or
     * pre-compiled methods.
     * <p>
     * Typically, the collection will contain no more than one
     * section, but this is not guaranteed. 
     * 
     * @see com.ibm.dtfj.image.ImageSection
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getBytecodeSections();

    /**
     * Get the set of ImageSections containing the compiled code of this method.
     * @return an iterator over a collection of ImageSections.
     * Each ImageSection contains data (usually executable code) used
     * in executing this method in compiled mode.
     * 
     * @see com.ibm.dtfj.image.ImageSection
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getCompiledSections();
    
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Method in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
}
