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
package com.ibm.j9ddr.vm29.pointer.helper;

import java.util.HashMap;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ClassIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;

public class J9ClassLoaderHelper
{
	
	private static HashMap<Character, J9ClassPointer> PRIMITIVE_TO_CLASS;
	static {
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			PRIMITIVE_TO_CLASS = new HashMap<Character, J9ClassPointer>();
			PRIMITIVE_TO_CLASS.put('V', vm.voidReflectClass());		
			PRIMITIVE_TO_CLASS.put('Z', vm.booleanReflectClass());		
			PRIMITIVE_TO_CLASS.put('B', vm.byteReflectClass());
			PRIMITIVE_TO_CLASS.put('C', vm.charReflectClass());
			PRIMITIVE_TO_CLASS.put('S', vm.shortReflectClass());
			PRIMITIVE_TO_CLASS.put('I', vm.intReflectClass());
			PRIMITIVE_TO_CLASS.put('J', vm.longReflectClass());
			PRIMITIVE_TO_CLASS.put('F', vm.floatReflectClass());
			PRIMITIVE_TO_CLASS.put('D', vm.doubleReflectClass());
		} catch (CorruptDataException e) {
			EventManager.raiseCorruptDataEvent("Corrupt VM pointer", e, true);
		}		
	}
	/* TODO: this is slightly bogus, if a class is not found in current classloader, we should use it's parent classloader */
	
	/**
	 * Return class for MixedObject or component class type for array classes.
	 * @param classLoader 
	 * @param signature JNI Class signature
	 * @return J9ClassPointer of a class or null if class is not found.
	 * @throws CorruptDataException
	 */
	public static J9ClassPointer findClass(J9ClassLoaderPointer classLoader, String signature) throws CorruptDataException 
	{
		J9ClassPointer result = null;
		Iterator<J9ClassPointer> classIterator = ClassIterator.fromJ9Classloader(classLoader);
		
		int arity = calculateClassArity(signature);
		
		if (arity > 0 && signature.charAt(arity) != 'L') {
			return PRIMITIVE_TO_CLASS.get(signature.charAt(arity));			
		} else {		
			while (classIterator.hasNext()) {
				J9ClassPointer clazz = classIterator.next();
				result = match(signature, arity, clazz);
				if (null != result) {
					break;
				}			
			}
		}
		return result;
	}

	private static J9ClassPointer match(String searchSignature, int arity, J9ClassPointer clazz) throws CorruptDataException
	{
		String classSignature = J9ClassHelper.getSignature(clazz);
		J9ClassPointer result = null;		
		if (matchLeafComponentType(searchSignature, arity, classSignature)) {		
			result = getArrayClassWithArity(clazz, arity);
		}	
		return result;
	}

	private static J9ClassPointer getArrayClassWithArity(J9ClassPointer clazz, int arity) throws CorruptDataException
	{
		J9ClassPointer result = clazz;
		for (int i = 0; i < arity; i++) {				
			result = result.arrayClass();
			if (result.isNull()) {
				return null;
			}
		}
		return result;
	}

	private static boolean matchLeafComponentType(String searchSignature, int arity, String classSignature)
	{		
		/* Don't need to check for trailing characters because ; is a terminator */
		return classSignature.regionMatches(0, searchSignature, arity, searchSignature.length() - arity);
	}

	private static int calculateClassArity(String signature)
	{
		int arity = signature.lastIndexOf('[');
		return arity + 1;
	}
}
