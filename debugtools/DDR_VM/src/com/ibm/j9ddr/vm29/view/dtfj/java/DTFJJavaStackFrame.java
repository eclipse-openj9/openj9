/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import java.lang.reflect.Modifier;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaLocation;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

import static com.ibm.j9ddr.vm29.j9.OptInfo.getLineNumberForROMClass;

/**
 * Combined JavaStackFrame & JavaLocation
 * 
 * @author andhall
 *
 */
public class DTFJJavaStackFrame implements JavaStackFrame, JavaLocation
{

	private final ImagePointer basePointer;
	private final DTFJJavaMethod dtfjMethod;
	private final ImagePointer pc;
	private final boolean jitMethod;
	private final U8Pointer bytecodePCOffset;
	private final J9MethodPointer j9method;
	@SuppressWarnings("unused")
	private final DTFJJavaThread thread;
	
	private List<Object> roots = new LinkedList<Object>();
	
	public DTFJJavaStackFrame(DTFJJavaThread dtfjJavaThread, DTFJJavaMethod dtfjMethod, J9MethodPointer method, ImagePointer pc, ImagePointer basePointer, U8Pointer bytecodePCOffset, boolean jitted) {
		this.dtfjMethod = dtfjMethod;
		this.basePointer = basePointer;
		this.bytecodePCOffset = bytecodePCOffset;
		this.pc = pc;
		this.jitMethod = jitted;
		this.j9method = method;
		this.thread = dtfjJavaThread;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaStackFrame#getBasePointer()
	 */
	public ImagePointer getBasePointer() throws CorruptDataException
	{
		return basePointer;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaStackFrame#getHeapRoots()
	 */
	@SuppressWarnings("rawtypes")
	public Iterator getHeapRoots()
	{
		return roots.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaStackFrame#getLocation()
	 */
	public JavaLocation getLocation() throws CorruptDataException
	{
		return this;
	}

	void addReference(Object object)
	{
		roots.add(object);
	}
	
	boolean isJitMethod()
	{
		return jitMethod;
	}

	public ImagePointer getAddress() throws CorruptDataException
	{
		return pc;
	}

	public int getCompilationLevel() throws CorruptDataException
	{
		//Mimics behaviour for J9 DTFJ. 1 = JIT compiled, 0 = interpreted.
		return jitMethod ? 1 : 0;
	}

	public String getFilename() throws DataUnavailable, CorruptDataException
	{
		return dtfjMethod.getFilename();
	}
	
	public int getLineNumber() throws DataUnavailable, CorruptDataException
	{
		if (J9BuildFlags.opt_debugInfoServer) {
			int toReturn = 0;
			try {
				 toReturn = getLineNumberForROMClass(j9method, UDATA.cast(bytecodePCOffset));
				
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
			if (toReturn < 0) {
				throw new DataUnavailable();
			}
			
			return toReturn;
		} else {
			throw new DataUnavailable();
		}
	}

	public JavaMethod getMethod() throws CorruptDataException
	{
		return dtfjMethod;
	}

	@Override
	public boolean equals(Object obj)
	{
		if (obj instanceof DTFJJavaStackFrame) {
			DTFJJavaStackFrame other = (DTFJJavaStackFrame)obj;
			
			if (! other.basePointer.equals(this.basePointer)) {
				return false;
			}
			
			if (! other.bytecodePCOffset.equals(this.bytecodePCOffset)) {
				return false;
			}
			
			if (! other.j9method.equals(this.j9method)) {
				return false;
			}
			
			if (! other.pc.equals(this.pc)) {
				return false;
			}
			
			if (other.jitMethod != this.jitMethod) {
				return false;
			}
			
			return true;
			
		} else {
			return false;
		}
	}

	@Override
	public int hashCode()
	{
		return basePointer.hashCode();
	}

	@Override
	public String toString()
	{
		try {
			String className = "";
			
			try {
				className = dtfjMethod.getDeclaringClass().getName();
				className = className.replace("/", ".");
			} catch (DataUnavailable e) {
				className = "<class name unavailable>";
			}
			
			String methodName = dtfjMethod.getName();
			
			if(Modifier.isNative(dtfjMethod.getModifiers())) {
				return className + "." + methodName + "()";
			} else {
				String fileName = dtfjMethod.getFilename();
				int lineNumber = -1;
				
				try {
					lineNumber = getLineNumber();
				} catch (DataUnavailable e) {
					//Do nothing
				}
				
				if (lineNumber != -1) {
					return className + "." + methodName + "(" + fileName + ":" + lineNumber + ")";
				} else {
					return className + "." + methodName + "(" + fileName + ")";
				}
			}
			
		} catch (Throwable t) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			return super.toString();
		}
	}

}
