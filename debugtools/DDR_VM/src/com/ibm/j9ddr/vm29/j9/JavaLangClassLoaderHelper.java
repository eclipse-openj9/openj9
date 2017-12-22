/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassLoaderHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.J9VmconstantpoolConstants;

public class JavaLangClassLoaderHelper
{

	private static J9ObjectFieldOffset vmRefOffset;
	private static J9ObjectFieldOffset parentOffset;

	public static J9ObjectPointer getParent(J9ObjectPointer loader) throws CorruptDataException
	{
		if(loader.isNull()) {
			return J9ObjectPointer.NULL;
		}
		
		if( parentOffset == null ) {
			
			// Iterate through all the fields of Ljava/lang/ClassLoader; until you find "parent"
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9ClassPointer javaLangClassLoader = J9ClassLoaderHelper.findClass( vm.systemClassLoader(), "Ljava/lang/ClassLoader;" );
			Iterator<J9ObjectFieldOffset> fieldIterator = J9ClassHelper.getFieldOffsets(javaLangClassLoader);
			
			while( fieldIterator.hasNext() ) {
				Object nextField = fieldIterator.next();
			
				if (nextField instanceof J9ObjectFieldOffset) {
					
					J9ObjectFieldOffset offset = (J9ObjectFieldOffset) nextField;
					
					if( "parent".equals(offset.getName())) {
						parentOffset = offset;
						break;
					}
				}
			}
		}
		
		if( parentOffset != null ) {
			return J9ObjectHelper.getObjectField(loader, parentOffset);
		} else {
			throw new CorruptDataException("Unable to find field offset for the 'parent' field");
		}
	}

	public static J9ClassLoaderPointer getClassLoader(J9ObjectPointer loader) throws CorruptDataException
	{
		if (loader.isNull()) {
			return J9ClassLoaderPointer.NULL;
		}

		if (vmRefOffset == null) {
			vmRefOffset = VMConstantPool.getFieldOffset(J9VmconstantpoolConstants.J9VMCONSTANTPOOL_JAVALANGCLASSLOADER_VMREF);
		}

		if (vmRefOffset != null) {
			long address = J9ObjectHelper.getLongField(loader, vmRefOffset);
			return J9ClassLoaderPointer.cast(address);
		} else {
			throw new CorruptDataException("Unable to find field offset for the 'vmRef' field");
		}
	}
}
