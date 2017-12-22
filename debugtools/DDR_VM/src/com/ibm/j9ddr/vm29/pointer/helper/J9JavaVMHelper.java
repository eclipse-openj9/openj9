/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.pointer.helper;

import java.util.Iterator;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ClassIterator;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMSystemPropertyPointer;

public class J9JavaVMHelper {
	public static Properties getSystemProperties(J9JavaVMPointer vm) throws CorruptDataException 
	{
		Properties result = new Properties();
		Pool<J9VMSystemPropertyPointer> sysprops = Pool.fromJ9Pool(vm.systemProperties(), J9VMSystemPropertyPointer.class);
		Iterator<J9VMSystemPropertyPointer> syspropsIterator = sysprops.iterator();
		int count = 0;
		while (syspropsIterator.hasNext()) {
			J9VMSystemPropertyPointer prop = syspropsIterator.next();
			// Iterator may return null if corrupt data was found.
			if( prop != null ) {
				String name = null;
				try {
					name = prop.name().getCStringAtOffset(0);
				} catch (CorruptDataException e) {
					name = "Corrupt System Property[" + count + "]";
				}
				String value = null;
				try {
					value = prop.value().getCStringAtOffset(0);
				} catch (CorruptDataException e) {
					value = "Corrupt Value";
				}
				result.setProperty(name, value);
			}
			count++;
		}
		return result;
	}
	
	/*
	 * Returns a program space pointer to the matching J9Method for the
	 * specified PC.
	 */
	public static J9MethodPointer getMethodFromPC(J9JavaVMPointer vmPtr, U8Pointer pc) throws CorruptDataException 
	{
		GCClassLoaderIterator it = GCClassLoaderIterator.from();
		while (it.hasNext()) {
			J9ClassLoaderPointer loader = it.next();

			Iterator<J9ClassPointer> classIt = ClassIterator.fromJ9Classloader(loader);

			while (classIt.hasNext()) {
				J9ClassPointer clazz = classIt.next();
				J9MethodPointer result = J9ClassHelper.getMethodFromPCAndClass(clazz, pc);
				if (!result.isNull()) {
					return result;
				}
			}
		}
		return J9MethodPointer.NULL;
	}
	
	public static boolean extendedRuntimeFlagIsSet(J9JavaVMPointer javaVM, long flag) throws CorruptDataException {
		return javaVM.extendedRuntimeFlags().allBitsIn(flag);
	}
}
