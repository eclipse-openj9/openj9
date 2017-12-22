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

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageSection;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITExceptionTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMMethodHelper;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

public class DTFJJavaMethod implements JavaMethod {

	protected final DTFJJavaClass clazz;
	protected final J9ROMMethodPointer j9romMethod;
	protected final J9MethodPointer j9ramMethod;
	
	public DTFJJavaMethod(DTFJJavaClass dtfjJavaClass, J9MethodPointer j9Method) throws com.ibm.j9ddr.CorruptDataException {
		this.clazz = dtfjJavaClass;
		this.j9ramMethod = j9Method;
		this.j9romMethod = J9MethodHelper.romMethod(j9Method);
	}

	private List<Object> byteCodeSections; 
	@SuppressWarnings("rawtypes")
	public Iterator getBytecodeSections() {
		if (byteCodeSections == null) {
			byteCodeSections = new ArrayList<Object>();
			try {
				J9ROMMethodPointer originalRomMethod = ROMHelp.getOriginalROMMethod(j9ramMethod);
				if (!originalRomMethod.modifiers().anyBitsIn(J9JavaAccessFlags.J9AccNative)) {
					U8Pointer bcStart = ROMHelp.J9_BYTECODE_START_FROM_ROM_METHOD(originalRomMethod);
					UDATA bcSize = ROMHelp.J9_BYTECODE_SIZE_FROM_ROM_METHOD(originalRomMethod);
					J9DDRImageSection is = DTFJContext.getImageSection(bcStart.getAddress(), "bytecode section at " + j9ramMethod.bytecodes().getAddress());
					is.setSize(bcSize.longValue());
					byteCodeSections.add(is);
					
					if (!j9romMethod.equals(originalRomMethod)) {
						is = DTFJContext.getImageSection(j9ramMethod.bytecodes().getAddress(), "bytecode section at " + j9ramMethod.bytecodes().getAddress());
						is.setSize(bcSize.longValue());
						byteCodeSections.add(is);
					}
				}
			} catch (Throwable t) {
				CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
				byteCodeSections.add(cd);
			}
		}
		
		return byteCodeSections.iterator();
	}

	List<Object> compiledSections;
	@SuppressWarnings("rawtypes")
	public Iterator getCompiledSections() {
		if (compiledSections == null) {
			compiledSections = new ArrayList<Object>();
			List<J9JITExceptionTablePointer> metaDatas = DTFJContext.getJITMetaData(j9ramMethod);
			if (metaDatas != null) {
				for (J9JITExceptionTablePointer metaData : metaDatas) {
					// There is always a warm region
					try {
						long start = metaData.startPC().longValue();
						long size = metaData.endWarmPC().longValue() - start;
						String name = String.format("jit section (%s) at %s", metaData.getAddress(), start);
						J9DDRImageSection is = DTFJContext.getImageSection(start, name);
						is.setSize(size);
						compiledSections.add(is);
					} catch (Throwable t) {
						CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
						compiledSections.add(cd);
					}
					
					// There may be a cold region
					// JEXTRACT never considered the cold region.  Leading to results where JEXTRACT could report 1 section and DTFJ will report 2.
					try {
						long start = metaData.startColdPC().longValue();
						if (start != 0) {
							long size = metaData.endPC().longValue() - start;
							String name = String.format("cold jit section (%s) at %s", metaData.getAddress(), start);
							J9DDRImageSection is = DTFJContext.getImageSection(start, name);
							is.setSize(size);
							compiledSections.add(is);
						}
					} catch (Throwable t) {
						CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
						compiledSections.add(cd);
					}
				}
			}
		}
		return compiledSections.iterator();
	}

	public JavaClass getDeclaringClass() throws CorruptDataException,
			DataUnavailable {
		return clazz;
	}

	public int getModifiers() throws CorruptDataException {
		try {
			return J9ROMMethodHelper.getReflectModifiers(j9romMethod);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public String getName() throws CorruptDataException {
		try {
			return J9ROMMethodHelper.getName(j9romMethod);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public String getSignature() throws CorruptDataException {
		try {
			return J9ROMMethodHelper.getSignature(j9romMethod);
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}

	public String getFilename() throws CorruptDataException
	{
		return clazz.getFilename();
	}
	
	public String toString() {
		try {
			return clazz.toString() + "." + getName() + getSignature();
		} catch (Throwable t) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			return super.toString();
		}
	}

	public boolean equals(Object obj) {
		try {
			if (obj == null || !(obj instanceof DTFJJavaMethod)) {
				return false;
			}
			DTFJJavaMethod method = (DTFJJavaMethod) obj;
			return (j9ramMethod.getAddress() == method.j9ramMethod.getAddress());
		} catch (Throwable t) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			return false;
		}
	}

	public int hashCode() {
		return j9ramMethod.hashCode();
	}
}
