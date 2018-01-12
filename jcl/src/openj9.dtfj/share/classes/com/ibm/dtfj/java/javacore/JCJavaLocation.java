/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java.javacore;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DTFJException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.javacore.JCCorruptData;
import com.ibm.dtfj.java.JavaLocation;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.javacore.builder.IBuilderData;

public class JCJavaLocation implements JavaLocation {
	
	private final JavaMethod fMethod;
	private ImagePointer fAddress;
	private int fCompilationLevel;
	private String fFileName;
	private int fLineNumber;
	
	public JCJavaLocation(JavaMethod javaMethod) throws JCInvalidArgumentsException{
		if (javaMethod == null) {
			throw new JCInvalidArgumentsException("A java location must be associated with a java method");
		}
		fMethod = javaMethod;
		
		fAddress = null;
		fCompilationLevel = IBuilderData.NOT_AVAILABLE;
		fFileName = null;
		fLineNumber = IBuilderData.NOT_AVAILABLE;
	}

	/**
	 * 
	 */
	public ImagePointer getAddress() throws CorruptDataException {
		if (fAddress == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fAddress;
	}
	
	/**
	 * NON-DTFJ
	 * @param address
	 */
	public void setAddress(ImagePointer address) {
		fAddress = address;
	}

	/**
	 * 
	 */
	public int getCompilationLevel() throws CorruptDataException {
		if (fCompilationLevel == IBuilderData.NOT_AVAILABLE) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fCompilationLevel;
	}

	/**
	 * NON-DTFJ
	 * @param compilationLevel
	 */
	public void setCompilation(String compilationLevel) {
		if ("compiled".equals(compilationLevel)) {
			fCompilationLevel = 1;
		} else {
			fCompilationLevel = 0;			
		}
	}
	
	/**
	 * 
	 */
	public String getFilename() throws DataUnavailable, CorruptDataException {
		if (fFileName == null) {
			throw new DataUnavailable();
		}
		return fFileName;
	}
	
	/**
	 * NON-DTFJ
	 * @param fileName
	 */
	public void setFilename(String fileName) {
		fFileName = fileName;
	}

	/**
	 * 
	 */
	public int getLineNumber() throws DataUnavailable, CorruptDataException {
		if (fLineNumber == IBuilderData.NOT_AVAILABLE) {
			throw new DataUnavailable();
		}
		return fLineNumber;
	}
	
	/**
	 * NON-DTFJ
	 * @param lineNumber
	 */
	public void setLineNumber(int lineNumber) {
		fLineNumber = lineNumber;
	}
	
	/**
	 * 
	 */
	public JavaMethod getMethod() throws CorruptDataException {
		if (fMethod == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fMethod;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaLocation#toString()
	 * copied directly from:
	 * com.ibm.dtfj.java.j9.JavaLocation
	 * to ensure the output matches.
	 */
	public String toString()
	{
		String output = null;
		
		try {
			String className = getMethod().getDeclaringClass().getName();
			className = className.replace('/', '.');
			String methodName = getMethod().getName();
			String suffix = null;
			try {
				String filename = getFilename();
				int line = getLineNumber();
				suffix = "(" + filename + ":" + line + ")";
			} catch (DTFJException e) {
				suffix = "()";
			}
			output = className + "." + methodName + suffix;
		} catch (CorruptDataException e) {
			output = "(corrupt)";
		} catch (DataUnavailable e) {
			output = "(data unavailable)";
		}
		return output;
	}
}
