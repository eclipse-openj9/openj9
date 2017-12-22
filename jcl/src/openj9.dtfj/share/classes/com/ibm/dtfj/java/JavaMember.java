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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;

/**
 * <p>Base interface inherited by JavaField and JavaMethod.</p>
 * 
 * <p>This interface is modeled on java.lang.reflect.Member.</p>
 */
public interface JavaMember {

	/**
	 * Get the set of modifiers for this field or method - a set of bits
	 * @return the modifiers for this field or method. The values for the constants representing
     * the modifiers can be obtained from java.lang.reflect.Modifier.
	 * @throws CorruptDataException 
     *
     */
    public int getModifiers()  throws CorruptDataException;

    /**
     * Get the class which declares this field or method
     * @return the JavaClass which declared this field or method
     * @throws CorruptDataException 
     * @throws DataUnavailable 
     */
    public JavaClass getDeclaringClass() throws CorruptDataException, DataUnavailable;
    
    /**
     * Get the name of the field or method
     * @return the name of the field or method
     * @throws CorruptDataException 
     */
    public String getName()  throws CorruptDataException;
    
    /**
     * Get the signature of the field or method
     * @return the signature of the field or method.
     * e.g. "(Ljava/lang/String;)V"
     * @throws CorruptDataException 
     */
    public String getSignature()  throws CorruptDataException;
    
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Member in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
}
