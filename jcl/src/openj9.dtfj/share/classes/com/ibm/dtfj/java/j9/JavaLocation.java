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
package com.ibm.dtfj.java.j9;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DTFJException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.j9.CorruptData;
import com.ibm.dtfj.java.JavaMethod;

/**
 * @author jmdisher
 *
 */
public class JavaLocation implements com.ibm.dtfj.java.JavaLocation
{
	private JavaMethod _method;
	private ImagePointer _methodID;
	private ImagePointer _pc;
	private int _lineNumber;

	public JavaLocation(JavaMethod method, ImagePointer pc, int lineNumber)
	{
		if (null == method) {
			throw new IllegalArgumentException("A Java Location cannot have a null method");
		}
		if (null == pc) {
			throw new IllegalArgumentException("A Java Location cannot have a null program counter");
		}
		_method = method;
		_pc = pc;
		_lineNumber = lineNumber;
	}

	public JavaLocation(ImagePointer methodid, ImagePointer pc, int lineNumber) {
		if (null == pc) {
			throw new IllegalArgumentException("A Java Location cannot have a null program counter");
		}
		_methodID = methodid;
		_pc = pc;
		_lineNumber = lineNumber;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaLocation#getAddress()
	 */
	public ImagePointer getAddress() throws CorruptDataException
	{
		return _pc;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaLocation#getLineNumber()
	 */
	public int getLineNumber() throws DataUnavailable, CorruptDataException
	{
		if (_lineNumber > 0) {
			return _lineNumber;
		}
		throw new DataUnavailable();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaLocation#getFilename()
	 */
	public String getFilename() throws DataUnavailable, CorruptDataException
	{
		JavaClass j9Class = (JavaClass)getMethod().getDeclaringClass();
		return j9Class.getFilename();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaLocation#getCompilationLevel()
	 */
	public int getCompilationLevel() throws CorruptDataException
	{
		/* we can't get accurate information from the JIT, so we will
		 * consider any JIT section to be 1, and any non-JIT section to
		 * be 0. 
		 */
		long pointer = getAddress().getAddress();
		
		Iterator iter = getMethod().getBytecodeSections();
		while (iter.hasNext()) {
			ImageSection section = (ImageSection)iter.next(); 
			if ( pointer >= section.getBaseAddress().getAddress() &&
				 pointer < section.getBaseAddress().getAddress() + section.getSize()) 
			{
				/* the PC is within an interpreted section */
				return 0;
			} 
		}

		iter = getMethod().getCompiledSections();
		while (iter.hasNext()) {
			ImageSection section = (ImageSection)iter.next(); 
			if ( pointer >= section.getBaseAddress().getAddress() &&
				 pointer < section.getBaseAddress().getAddress() + section.getSize()) 
			{
				/* the PC is within a compiled section */
				return 1;
			} 
		}
		
		//we couldn't find the PC. This should probably be an error:
		//throw new CorruptDataException(new CorruptData("PC is not within a known method section", getAddress()));
		//but let's play it safe, since J9 uses some magic PCs which jextract doesn't hide
		return 0;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaLocation#getMethod()
	 */
	public JavaMethod getMethod() throws CorruptDataException
	{
		if (_method == null) throw new CorruptDataException(new CorruptData("Bad method", _methodID));
		return _method;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaLocation#toString()
	 */
	public String toString()
	{
		String output = null;
		
		try {
			//TODO: it would be nice if we could get the currently receiving class type but that will require some help from JExtract
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

	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof JavaLocation) {
			JavaLocation local = (JavaLocation) obj;
			isEqual = equals(_method, local._method) && _pc.equals(local._pc) && equals(_methodID, local._methodID);
		}
		return isEqual;
	}
	
	private static boolean equals(Object o1, Object o2)
	{
		return o1 == o2 || o1 != null && o1.equals(o2);
	}

	public int hashCode()
	{
		return hashCode(_method) ^ _pc.hashCode() ^ hashCode(_methodID);
	}
	
	private static int hashCode(Object o1)
	{
		return o1 == null ? 0 : o1.hashCode();
	}
}
