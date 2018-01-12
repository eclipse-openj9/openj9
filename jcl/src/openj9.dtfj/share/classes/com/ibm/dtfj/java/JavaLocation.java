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
import com.ibm.dtfj.image.ImagePointer;

/**
 * Represents a point of execution within a Java method.
 */
public interface JavaLocation {
	
	/**
	 * Fetches the absolute address of the code which this location represents.
	 * This pointer will be contained within one of the segments returned by
	 * getBytecodeSections() or getCompiledSections() of the method returned by
	 * getMethod().
	 * <p>
	 * null may be returned, particularly for methods with no bytecode or 
	 * compiled sections (e.g. some native methods)
	 * <p>
	 * Although an offset into the method may be calculated using this 
	 * pointer, caution should be exercised in attempting to map this offset to
	 * an offset within the original class file. Various transformations may
	 * have been applied to the bytecodes by the VM or other agents which may
	 * make the offset difficult to interpret.
	 * <p>
	 * For native methods, the address may be meaningless.
	 * 
	 * @return the address in memory of the managed code
	 * @throws CorruptDataException 
	 */
	public ImagePointer getAddress() throws CorruptDataException;
	
	/**
	 * Get the line number.
	 * @return the line number, if available, or throws DataUnavailable if it is not available
	 * Line numbers are counted from 1  
	 * 
	 * @throws DataUnavailable if the line number data is not available for this location
	 * @throws CorruptDataException 
	 */
	public int getLineNumber() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Get the source file name.
	 * @return the name of the source file, if available, or throws DataUnavailable if it is
	 * not available
	 * 
	 * @throws DataUnavailable if the source file name is unavailable in the core
	 * @throws CorruptDataException 
	 */
	public String getFilename() throws DataUnavailable, CorruptDataException;
	
	/**
	 * Get the compilation level for this location. This is an implementation 
	 * defined number indicating the level at which the current location was 
	 * compiled. 0 indicates interpreted. Any positive number indicates some 
	 * level of JIT compilation. Typically, higher numbers indicate more 
	 * aggressive compilation strategies
	 * <p>
	 * For native methods, a non-zero compilation level indicates that some
	 * level of JIT compilation has been applied to the native call (e.g. a
	 * custom native call stub). To determine if the method is native, use
	 * getMethod().getModifiers().
	 * 
	 * @return the compilation level 
	 * @throws CorruptDataException 
	 */
	public int getCompilationLevel() throws CorruptDataException;
	
	/**
	 * Get the method which contains the point of execution.
	 * @return the method which contains the point of execution
	 * @throws CorruptDataException 
	 */
	public JavaMethod getMethod() throws CorruptDataException;
	
	/**
	 * @return A string representing the location as it would be seen in a Java stack trace
	 */
	public String toString();
	
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Location in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();
}
