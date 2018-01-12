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

import java.util.Collections;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.javacore.JCCorruptData;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.javacore.builder.IBuilderData;

public class JCJavaClass implements JavaClass{
	
	private final JCJavaRuntime fJavaRuntime;
	
	private JavaClassLoader fJavaClassLoader;
	private JavaObject fJavaObject;
	private long fClassID;
	private long fSuperClassID;
	private final String fClassName;
	private ImagePointer fClassPointer;
	private int fModifiers;
	
	private LinkedHashSet fMethods;
	private Vector fConstantPoolReferences;
	private Vector fDeclaredFields;
	private Vector fInterfaces;
	
	/**
	 * 
	 * @param javaRuntime
	 * @param className
	 */
	public JCJavaClass(JCJavaRuntime javaRuntime, String className) throws JCInvalidArgumentsException {
		if (javaRuntime == null) {
			throw new JCInvalidArgumentsException("Must pass a valid java runtime and class name");
		}
		if (className == null) {
			throw new JCInvalidArgumentsException("Must pass either a valid className or ID at construction time");
		}
		// final
		fClassName = className;
		
		// all other
		fJavaRuntime = javaRuntime;
		fClassPointer = null;
		fJavaClassLoader = null;
		fJavaObject = null;
		fClassID = IBuilderData.NOT_AVAILABLE;
		fSuperClassID = IBuilderData.NOT_AVAILABLE;
		fModifiers = IBuilderData.NOT_AVAILABLE;
		
		// containers
		fMethods = new LinkedHashSet();
		fConstantPoolReferences = new Vector();
		fDeclaredFields = new Vector();
		fInterfaces = new Vector();
		
		// must associate with a runtime.
		fJavaRuntime.addJavaClass(this);
	}
	
	/**
	 * 
	 */
	public JavaClassLoader getClassLoader() throws CorruptDataException {
		if (fJavaClassLoader == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fJavaClassLoader;
	}

	/**
	 * component type only for arrays
	 */
	public JavaClass getComponentType() throws CorruptDataException {
		throw new CorruptDataException(new JCCorruptData(null));
	}
	
	/**
	 * 
	 */
	public Iterator getConstantPoolReferences() {
		return fConstantPoolReferences.iterator();
	}

	/**
	 * 
	 */
	public Iterator getDeclaredFields() {
		return fDeclaredFields.iterator();
	}

	/**
	 * 
	 */
	public Iterator getDeclaredMethods() {
		return fMethods.iterator();
	}
	
	/**
	 * 
	 */
	public ImagePointer getID() {
		if (fClassPointer == null && fClassID != IBuilderData.NOT_AVAILABLE) {
			fClassPointer = fJavaRuntime.getImageProcess().getImageAddressSpace().getPointer(fClassID);
		}
		return fClassPointer;
	}
	
	/**
	 * 
	 */
	public Iterator getInterfaces() {
		return fInterfaces.iterator();
	}
	
	/**
	 * @see com.ibm.dtfj.java.JavaClass#getModifiers()
	 * @throws CorruptDataException if modifiers not set
	 */
	public int getModifiers() throws CorruptDataException {
		if (fModifiers == IBuilderData.NOT_AVAILABLE) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fModifiers;
	}

	/**
	 * @see com.ibm.dtfj.java.JavaClass#getName()
	 * @throws CorruptDataException if class name is not set
	 */
	public String getName() throws CorruptDataException {
		if (fClassName == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fClassName;
	}
	
	/**
	 * Instance object of this class
	 * @see com.ibm.dtfj.java.JavaClass#getObject()
	 * @throws CorruptDataException if no instance of this class is set
	 */
	public JavaObject getObject() throws CorruptDataException {
		if (fJavaObject == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fJavaObject;
	}

	/**
	 * Returning null is permissable for cases like Object, primitives, interfaces, etc..
	 * @return JavaClass of this class's superclass.
	 * @see com.ibm.dtfj.java.JavaClass#getSuperclass()
	 * @throws CorruptDataException if super class not found.
	 */
	public JavaClass getSuperclass() throws CorruptDataException {
		JavaClass superClass = fJavaRuntime.findJavaClass(fSuperClassID);
		if (superClass == null) {
			throw new CorruptDataException(new JCCorruptData(null));
		}
		return fJavaRuntime.findJavaClass(fSuperClassID);
	}

	/**
	 * 
	 */
	public long getInstanceSize() throws DataUnavailable, CorruptDataException {
		throw new DataUnavailable("Instance size is not available in a javacore");
	}
	
	/**
	 * 
	 * Same as the J9 System Core implementation for DTFJ.
	 * @see com.ibm.dtfj.java.j9.JavaClass
	 * @return true if an array
	 */
	public boolean isArray() throws CorruptDataException {
		String name = getName();
		return name.startsWith("[");
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param classID which is a valid hexadecimal address
	 * @throws JCInvalidArgumentsException if invalid address is passed as an ID
	 */
	public void setID(long classID) throws JCInvalidArgumentsException {
		if (!fJavaRuntime.getImageProcess().getImageAddressSpace().isValidAddressID(classID)) {
			throw new JCInvalidArgumentsException("Must pass a valid class ID (non zero value)");
		}
		fClassID = classID;
		// Add the new ID to the list of all class IDs
		fJavaRuntime.addJavaClass(this);
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @return class Name 
	 */
	public String internalGetName() {
		return fClassName;
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param classLoader
	 */
	public void setClassLoader(JavaClassLoader classLoader) {
		fJavaClassLoader = classLoader;
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param superClassID
	 */
	public void setJavaSuperClass(long superClassID) {
		fSuperClassID = superClassID;
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param javaObject
	 */
	public void setJavaObject(JavaObject javaObject) {
		fJavaObject = javaObject;
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param java class modifiers
	 */
	public void setModifiers(int modifiers) {
		fModifiers = modifiers;
	}

	public Iterator getReferences() {
		return Collections.EMPTY_LIST.iterator();
	}
	
	/**
	 * NON-DTFJ
	 * <br>
	 * <b>For internal building purposes only</b>. Do not call outside the DTFJ implementation.
	 * <br><br>
	 * @param java method - a method declared by this class
	 */
	public void addMethod(JavaMethod method) {
		fMethods.add(method);
	}

	public JavaObject getProtectionDomain() throws DataUnavailable,	CorruptDataException {
		throw new DataUnavailable("This implementation of DTFJ does not support getProtectionDomain");
	}

}
