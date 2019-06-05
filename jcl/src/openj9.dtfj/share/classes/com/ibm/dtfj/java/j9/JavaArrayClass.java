/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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

import java.util.Collections;
import java.util.Iterator;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.j9.CorruptData;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;

/**
 * @author jmdisher
 *
 */
public class JavaArrayClass extends JavaAbstractClass
{
	private int _sizeOffset;	//the offset into the header where the size field occurs
	private int _bytesForSize;	//the number of bytes to read at _sizeOffset to interpret as the size
	private int _firstElementOffset;
	private long _leafClassID;
	private int _dimension;
	
	
	public JavaArrayClass(JavaRuntime runtime, ImagePointer pointer, int modifiers, int flagOffset, int sizeOffset, int bytesForSize, int firstElementOffset, long leafClassID, int dimension, long loaderID, ImagePointer objectID, int hashcodeSlot)
	{
		super(runtime, pointer, modifiers, loaderID, objectID, flagOffset, hashcodeSlot);
		_sizeOffset = sizeOffset;
		_bytesForSize = bytesForSize;
		_firstElementOffset = firstElementOffset;
		_leafClassID = leafClassID;
		_dimension = dimension;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getName()
	 */
	public String getName() throws CorruptDataException
	{
		String name = "";
		// Note that validation in JavaObject.arraycopy is dependent on type name constructed here
		for (int x = 0; x < _dimension; x++) {
			name += JavaObject.ARRAY_PREFIX_SIGNATURE;
		}
		
		JavaClass leafClass=getLeafClass();
		if(null == leafClass) {
			CorruptData data=new CorruptData("unable to retrieve leaf class",null);
			throw new CorruptDataException(data);
		}
		
		String elementClassName = leafClass.getName();

		if (elementClassName.equals("boolean")) {
			name += JavaObject.BOOLEAN_SIGNATURE;
		} else if (elementClassName.equals("byte")) {
			name += JavaObject.BYTE_SIGNATURE;
		} else if (elementClassName.equals("char")) { 
			name += JavaObject.CHAR_SIGNATURE;
		} else if (elementClassName.equals("short")) {
			name += JavaObject.SHORT_SIGNATURE;
		} else if (elementClassName.equals("int"))  {
			name += JavaObject.INTEGER_SIGNATURE;
		} else if (elementClassName.equals("long")) {
			name += JavaObject.LONG_SIGNATURE;
		} else if (elementClassName.equals("float")) {
			name += JavaObject.FLOAT_SIGNATURE;
		} else if (elementClassName.equals("double")) {
			name += JavaObject.DOUBLE_SIGNATURE;
		} else {
			// reference type
			name += JavaObject.OBJECT_PREFIX_SIGNATURE;
			name += elementClassName;
			name += ';';
		}

		return name;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getSuperclass()
	 */
	public JavaClass getSuperclass() throws CorruptDataException
	{
		//the superclass for every array class is j/l/O
		return _javaVM._javaLangObjectClass;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#isArray()
	 */
	public boolean isArray() throws CorruptDataException
	{
		return true;
	}

	/* J9-specific method (not in API).
	 * Returns the leaf class of this ArrayClass. 
	 * For example, the leaf class for String[][] is java.lang.String.
	 * This is different from getComponentType for multi-dimensional arrays.
	 */
	public JavaClass getLeafClass() {
		return _javaVM.getClassForID(_leafClassID);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getComponentType()
	 */
	public JavaClass getComponentType() throws CorruptDataException
	{
		JavaClass componentType = null;
		if (1 == _dimension) {
			 componentType = getLeafClass();
		} else {
			componentType = _javaVM.getComponentTypeForClass(this);
			
		}
		if (componentType == null) {
			throw new CorruptDataException(new CorruptData("Unable to retrieve component type for array: " + getName(), null));
		}
		return componentType;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getClassLoader()
	 */
	public JavaClassLoader getClassLoader() throws CorruptDataException
	{
		JavaClassLoader cl = super.getClassLoader();
		JavaClass leafClass = getLeafClass();
		if (null == cl && null != leafClass) {
			cl = getLeafClass().getClassLoader();
		}
		return cl;
	}

	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getDeclaredFields()
	 */
	public Iterator getDeclaredFields()
	{
		//there aren't declared fields on array types
		return Collections.EMPTY_LIST.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getDeclaredMethods()
	 */
	public Iterator getDeclaredMethods()
	{
		//there aren't declared methods on array types
		return Collections.EMPTY_LIST.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getConstantPoolReferences()
	 */
	public Iterator getConstantPoolReferences()
	{
		//array classes don't have constant pools
		return Collections.EMPTY_LIST.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.j9.JavaAbstractClass#getInstanceSize(com.ibm.dtfj.java.JavaObject)
	 */
	public int getInstanceSize(com.ibm.dtfj.java.JavaObject instance)
	{
		try {
			int numberOfElements = instance.getArraySize();
			int instanceOverhead = getFirstElementOffset();
			int bytesPerElement = getBytesPerElement(((JavaObject)instance).getFObjectSize());
			
			return instanceOverhead + (numberOfElements * bytesPerElement);
		} catch (CorruptDataException e) {
			//we get here if we couldn't read the class name so this should probably throw
			return -1;
		}
	}
	
	/**
	 * Looks up the class name of the child elements to determine the size of one element
	 * 
	 * @param refFieldSize The size of reference fields (fj9object_t) in the heap of the object instance we are sizing
	 * @return The size of one element, in bytes
	 * @throws CorruptDataException 
	 */
	public int getBytesPerElement(int refFieldSize) throws CorruptDataException
	{
		String elementClassName = getName().substring(1);
		int size = 0;
		
		if ((elementClassName.equals(JavaObject.BYTE_SIGNATURE)) || 
			(elementClassName.equals(JavaObject.BOOLEAN_SIGNATURE))) {
			size = 1;
		} else if ((elementClassName.equals(JavaObject.CHAR_SIGNATURE)) || 
			(elementClassName.equals(JavaObject.SHORT_SIGNATURE))) {
			size = 2;
		} else if ((elementClassName.equals(JavaObject.FLOAT_SIGNATURE)) || 
			(elementClassName.equals(JavaObject.INTEGER_SIGNATURE))) {
			size = 4;
		} else if ((elementClassName.equals(JavaObject.DOUBLE_SIGNATURE)) || 
			(elementClassName.equals(JavaObject.LONG_SIGNATURE))) {
			size = 8;
		} else {
			//this must be a ref type so return the fobject_t size
			size = refFieldSize;
		}
		return size;
	}

	/**
	 * @return
	 */
	public int getFirstElementOffset()
	{
		return _firstElementOffset;
	}
	
	/**
	 * @return The offset into an array where the length (in elements) is found
	 */
	public int getSizeOffset()
	{
		return _sizeOffset;
	}
	
	/**
	 * @return The number of bytes to read from the object header as the size field
	 */
	public int getNumberOfSizeBytes()
	{
		return _bytesForSize;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaArrayClass#getReferences()
	 */
	public Iterator getReferences()
	{
		// need to build a list of references from this class.
		Vector references = new Vector();

		addSuperclassReference(references);
		addClassLoaderReference(references);
		addClassObjectReference(references);
		
		return references.iterator();
	}

	public long getInstanceSize() throws CorruptDataException {
		throw new java.lang.UnsupportedOperationException("JavaArrayClass does not support getInstanceSize");
	}
	
}
