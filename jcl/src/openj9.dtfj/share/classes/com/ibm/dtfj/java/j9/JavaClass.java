/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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

import java.lang.reflect.Modifier;
import java.util.Iterator;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.image.j9.CorruptData;
import com.ibm.dtfj.java.JavaObject;

/**
 * @author jmdisher
 *
 */
public class JavaClass extends JavaAbstractClass
{
	private long _superClassID;
	private String _className;
	private Vector _methods = new Vector();
	private Vector _fields = new Vector();
	private Vector _constantPoolClassRefs = new Vector();
	private Vector _constantPoolObjects = new Vector();
	private int _instanceSize;
	private String _fileName;
	
	public JavaClass(JavaRuntime vm, ImagePointer classPointer, long superClassID, String name, int instanceSize, long classLoaderID, int modifiers, int flagOffset, String fileName, ImagePointer objectID, int hashcodeSlot)
	{
		super(vm,classPointer, modifiers, classLoaderID, objectID, flagOffset, hashcodeSlot);
		_superClassID = superClassID;
		_className = name;
		_instanceSize = instanceSize;
		_fileName = fileName;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getName()
	 */
	public String getName() throws CorruptDataException
	{
		return _className;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getSuperclass()
	 */
	public com.ibm.dtfj.java.JavaClass getSuperclass()
			throws CorruptDataException
	{
		if (_superClassID == 0) return null;
		if (Modifier.isInterface(this.getModifiers())) {
			return null;
		}
		com.ibm.dtfj.java.JavaClass ret = _javaVM.getClassForID(_superClassID);
		if (ret == null) {
			throw new CorruptDataException(new CorruptData("Unknown superclass ID " + _superClassID, null));
		}
		return ret;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#isArray()
	 */
	public boolean isArray() throws CorruptDataException
	{
		return false;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getComponentType()
	 */
	public com.ibm.dtfj.java.JavaClass getComponentType() throws CorruptDataException {
			//we should probably document this special case and throw something appropriate
			throw new IllegalArgumentException("Only array types have component types");
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getDeclaredFields()
	 */
	public Iterator getDeclaredFields()
	{
		return _fields.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getDeclaredMethods()
	 */
	public Iterator getDeclaredMethods()
	{
		return _methods.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getConstantPoolReferences()
	 */
	public Iterator getConstantPoolReferences()
	{
		//first look up all the class IDs and translate them into classes then add the objects
		Iterator ids = _constantPoolClassRefs.iterator();
		Vector allRefs = new Vector();
		
		while (ids.hasNext()) {
			long oneID = ((Long)ids.next()).longValue();
			Object toBeAdded = null;
			com.ibm.dtfj.java.JavaClass oneClass = _javaVM.getClassForID(oneID);
			if (oneClass == null) {
				toBeAdded = new CorruptData("Unknown class in constant pool " + oneID, null);
			} else {
				try {
					toBeAdded = oneClass.getObject();
				} catch (CorruptDataException e) {
					toBeAdded = e.getCorruptData();
				} catch (Exception e) {
					toBeAdded = new CorruptData(e.getMessage());
				}
			}
			allRefs.add(toBeAdded);
		}
		
		// Loop through the list of constant pool objects, instantiating them and adding them to the list
		for (int i = 0; i < _constantPoolObjects.size(); i++) {
			try {
				long objectId = ((Long)(_constantPoolObjects.get(i))).longValue();
				if (objectId != 0) {				
					ImagePointer pointer = _javaVM.pointerInAddressSpace(objectId); 
					try {
						JavaObject instance = _javaVM.getObjectAtAddress(pointer);
						allRefs.add(instance);
					} catch (IllegalArgumentException e) {
						// getObjectAtAddress may throw an IllegalArgumentException if the address is not aligned
						allRefs.add(new CorruptData(e.getMessage(),pointer));
					}					
				}
			} catch(CorruptDataException e) {
				allRefs.add(e.getCorruptData());
			}
		}

		return allRefs.iterator();
	}
	
	/**
	 * The constant pool consists of class IDs and object instances.  This is how the class IDs are added
	 * 
	 * @param id
	 */
	public void addConstantPoolClassRef(long id)
	{
		//TODO:  What does a null class in the constant pool represent?  (It happens frequently)
		if (0L != id) {
			_constantPoolClassRefs.add(Long.valueOf(id));
		}
	}

	public int getInstanceSize(com.ibm.dtfj.java.JavaObject instance)
	{
		return _instanceSize;
	}
	
	public String getFilename() throws DataUnavailable, CorruptDataException
	{
		if (_fileName != null) {
			return _fileName;
		}
		throw new DataUnavailable(); 
	}

	public void createNewField(String name, String sig, int modifiers, int offset, long classID)
	{
		//only create and add this field if it belongs to this class
		if (getID().getAddress() == classID) {
			JavaInstanceField field = new JavaInstanceField(_javaVM, name, sig, modifiers, offset, classID);
			_fields.add(field);
		}
	}

	public JavaMethod createNewMethod(long id, String name, String signature, int modifiers)
	{
		JavaMethod method = new JavaMethod(_javaVM.pointerInAddressSpace(id), name, signature, modifiers, this);
		_javaVM.addMethodForID(method, id);
		_methods.add(method);
		return method;
	}

	public void createConstantPoolObjectRef(long id)
	{
		// Add the id to the list
		_constantPoolObjects.add(Long.valueOf(id));	
	}

	public void createNewStaticField(String name, String sig, int modifiers, String value)
	{
		JavaStaticField newStatic = new JavaStaticField(_javaVM, name, sig, modifiers, value, getID().getAddress());
		_fields.add(newStatic);
	}
	
	public String toString()
	{
		return _className+ "@" + Long.toHexString(_classPointer.getAddress());
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getReferences()
	 */
	public Iterator getReferences()
	{
		// need to build a list of references from this class.
		Vector references = new Vector();
		JavaReference jRef = null;
		
		// get the Constant Pool references from this class.
		Iterator constantPoolIt = getConstantPoolReferences();
		while (constantPoolIt.hasNext()) {
			// get each reference in turn, note that the iterator can return JavaClass
			// JavaObject and CorruptData. The CorruptData objects are ignored.
			Object cpObject = constantPoolIt.next();
			if (cpObject instanceof JavaObject) {
				// add the reference to the container.
				jRef = new JavaReference(_javaVM, this, cpObject, "Constant Pool Object", JavaReference.REFERENCE_CONSTANT_POOL, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
			} else if (cpObject instanceof JavaClass) {
				// got a JavaClass
				JavaClass jClass = (JavaClass)cpObject;

				// add the reference to the container.
				jRef = new JavaReference(_javaVM, this, jClass, "Constant Pool Class", JavaReference.REFERENCE_CONSTANT_POOL, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
			}
			
			if (null != jRef) {
				references.add(jRef);
			}
		}
		
		// get the static field references from this class.
		Iterator declaredFieldIt = getDeclaredFields();
		while (declaredFieldIt.hasNext()) {
			JavaField jField = (JavaField)declaredFieldIt.next();
			// got a field, now test it to see if it is a static reference.
			if (jField instanceof JavaStaticField) {
				JavaStaticField sField = (JavaStaticField)jField;
				try {
					Object obj = sField.getReferenceType(null);
					if (null != obj) {
						if (obj instanceof JavaObject) {
							
							// build a JavaReference type and add the reference to the container.
							String fieldName = sField.getName();
							String description = "Static field";
							if (null != fieldName) {
								description  = description + " [field name:" + fieldName + "]";
							}
							
							JavaObject jObject = (JavaObject)obj;
							jRef = new JavaReference(_javaVM, this, jObject, description, JavaReference.REFERENCE_STATIC_FIELD, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG); 
							references.add(jRef);
						}
					}
				} catch (CorruptDataException e) {
					// Corrupt data, so add it to the container.
					references.add(e.getCorruptData());
				} catch (MemoryAccessException e) {
					// Memory access problems, so create a CorruptData object 
					// to describe the problem and add it to the container.
					ImagePointer ptrInError = e.getPointer();
					String message = e.getMessage();
					references.add(new CorruptData(message, ptrInError));
				} catch (IllegalArgumentException e) {
					// No static data, so ignore.
				}
			}
		}
		
		addSuperclassReference(references);
		addClassLoaderReference(references);
		addClassObjectReference(references);
		
		return references.iterator();
	}

	public boolean isAncestorOf(com.ibm.dtfj.java.JavaClass theClass) {
		if (null == theClass) {
			return false;
		} else if (this.equals(theClass)) {
			return true;
		} else {
			try {
				return this.isAncestorOf(theClass.getSuperclass());
			} catch (CorruptDataException cde) {
				return false;
			}
		}
	}

	public long getInstanceSize() throws CorruptDataException {
		return (long)_instanceSize;
	}
}
