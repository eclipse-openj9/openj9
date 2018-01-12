/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaLocation;
import com.ibm.dtfj.java.JavaMethod;

/** 
 * @author ajohnson
 */
public class PHDJavaLocation implements JavaLocation {
	
	private ImagePointer address;
	private CorruptData address_cd;
	private int compilationLevel;
	private CorruptData compilationLevel_cd;
	private String filename;
	private CorruptData filename_cd;
	private int lineNumber;
	private CorruptData lineNumber_cd;
	private JavaMethod method;
	private CorruptData method_cd;

	/**
	 * Build Java program location information from a JavaLocation from another dump type.
	 * Extract all the information on object construction.
	 * @param space
	 * @param runtime
	 * @param shadow
	 */
	PHDJavaLocation(ImageAddressSpace space, PHDJavaRuntime runtime, JavaLocation shadow) {
		try {
			address = space.getPointer(shadow.getAddress().getAddress());
		} catch (CorruptDataException e) {
			address_cd = new PHDCorruptData(space, e);
		}
		try {
			compilationLevel = shadow.getCompilationLevel();
		} catch (CorruptDataException e) {
			compilationLevel_cd = new PHDCorruptData(space, e);
		}
		try {
			filename = shadow.getFilename();
		} catch (CorruptDataException e) {
			filename_cd = new PHDCorruptData(space, e);
		} catch (DataUnavailable e) {			
		}
		try {
			lineNumber = shadow.getLineNumber();
		} catch (CorruptDataException e) {
			lineNumber_cd = new PHDCorruptData(space, e);
		} catch (DataUnavailable e) {
			lineNumber = -1;
		}
		try {
			method = new PHDJavaMethod(space, runtime, shadow.getMethod());
		} catch (CorruptDataException e) {
			method_cd = new PHDCorruptData(space, e);
		}
	}

	public ImagePointer getAddress() throws CorruptDataException {
		checkCD(address_cd);
		return address;
	}

	public int getCompilationLevel() throws CorruptDataException {
		checkCD(compilationLevel_cd);
		return compilationLevel;
	}

	public String getFilename() throws DataUnavailable, CorruptDataException {
		checkCD(filename_cd);
		if (filename == null) throw new DataUnavailable();
		return filename;
	}

	public int getLineNumber() throws DataUnavailable, CorruptDataException {
		checkCD(lineNumber_cd);
		if (lineNumber == -1) throw new DataUnavailable();
		return lineNumber;
	}

	public JavaMethod getMethod() throws CorruptDataException {
		checkCD(method_cd);
		return method;
	}

	private void checkCD(CorruptData cd) throws CorruptDataException {
		if (cd != null) throw new CorruptDataException(cd);
	}
	
	public boolean equals(Object o) {
		if (o == null) return false;
		if (getClass() != o.getClass()) return false;
		PHDJavaLocation l = (PHDJavaLocation)o;
		return equals(method, l.method) && equals(lineNumber, l.lineNumber);
	}
	
	public int hashCode() {
		return hashCode(method) ^ hashCode(lineNumber);
	}
	
	private boolean equals(Object o1, Object o2) {
		return (o1 == null ? o2 == null : o1.equals(o2));
	}
	
	private int hashCode(Object o) {
		return o == null ? 0 : o.hashCode();
	}
	
	/*
	 * Get a description of the frame in the same format as a stack trace 
	 */
	public String toString() {
		StringBuilder sb = new StringBuilder();
		try {
			JavaMethod m = getMethod();
			sb.append(m.getDeclaringClass().getName().replace('/', '.'));
			sb.append('.');
			sb.append(m.getName());
		} catch (CorruptDataException e) {
			sb.append("corrupt");
		} catch (DataUnavailable e) {
			sb.append("data unavailable");
		}
		sb.append('(');
		boolean d = false;
		try {
			sb.append(getFilename());
			d = true;
			int line = getLineNumber();
			sb.append(':');
			sb.append(line);
		} catch (CorruptDataException e) {
			try {
				if (getCompilationLevel() > 0) {
					if (d)
						sb.append('(');
					sb.append("Compiled Code");
					if (d)
						sb.append(')');
				}
			} catch (CorruptDataException e2) {
			}
		} catch (DataUnavailable e) {
			try {
				if (getCompilationLevel() > 0) {
					if (d)
						sb.append('(');
					sb.append("Compiled Code");
					if (d)
						sb.append(')');
				}
			} catch (CorruptDataException e2) {
			}
		} finally {
			sb.append(')');
		}
		return sb.toString();
	}
}
