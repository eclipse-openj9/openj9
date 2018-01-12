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

import java.lang.reflect.Modifier;
import java.util.Collection;
import java.util.Iterator;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaObject;

/**
 * @author jmdisher
 *
 */
public abstract class JavaAbstractClass implements JavaClass
{
	protected ImagePointer _classPointer;
	protected JavaRuntime _javaVM;
	private int _modifiers;
	private Vector _interfaceNames = new Vector();
	private long _classLoaderID;
	private ImagePointer _objectID;
	private int _flagOffset;
	private int _hashcodeSlot;
	
	private static final String PROTECTION_DOMAIN_FIELD_NAME = "protectionDomain";

	protected JavaAbstractClass(JavaRuntime vm, ImagePointer id, int modifiers, long loaderID, ImagePointer objectID, int flagOffset, int hashcodeSlot)
	{
		if (null == id) {
			throw new IllegalArgumentException("Java class pointer must be non-null");
		}
		if (null == vm) {
			throw new IllegalArgumentException("Java VM for a class must not be null");
		}
		_javaVM = vm;
		_classPointer = id;
		_modifiers = modifiers;
		_classLoaderID = loaderID;
		_objectID = objectID;
		_flagOffset = flagOffset;
		_hashcodeSlot = hashcodeSlot;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getClassLoader()
	 */
	public JavaClassLoader getClassLoader() throws CorruptDataException
	{
		return _javaVM.getClassLoaderForID(_classLoaderID);
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getObject()
	 */
	public JavaObject getObject() throws CorruptDataException
	{
		/* 
		 * In new VMs the objectID is distinct from the class pointer and both will be specified.
		 * In older VMs the class ID is the object ID
		 */
		ImagePointer pointer;
		if (_objectID == null) {
			pointer = _classPointer;
		} else {
			pointer = _objectID;
		}

		try {
			return _javaVM.getObjectAtAddress(pointer);
		} catch (IllegalArgumentException e) {
			// getObjectAtAddress can throw an IllegalArgumentException if the address is not aligned
			throw new CorruptDataException(new com.ibm.dtfj.image.j9.CorruptData(e.getMessage(),pointer));
		}
	}

	private static final int SYNTHETIC = 0x1000;
	private static final int ANNOTATION = 0x2000;
	private static final int ENUM = 0x4000;
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getModifiers()
	 */
	public int getModifiers() throws CorruptDataException
	{
		int rawModifiers = _modifiers;
		if (isArray()) {
			rawModifiers &= (Modifier.PUBLIC | Modifier.PRIVATE | Modifier.PROTECTED |
				Modifier.ABSTRACT | Modifier.FINAL);
		} else {
			rawModifiers &= (Modifier.PUBLIC | Modifier.PRIVATE | Modifier.PROTECTED |
							Modifier.STATIC | Modifier.FINAL | Modifier.INTERFACE |
							Modifier.ABSTRACT | SYNTHETIC | ENUM | ANNOTATION);
		}
		
		return rawModifiers;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getID()
	 */
	public ImagePointer getID()
	{
		return _classPointer;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaClass#getInterfaces()
	 */
	public Iterator getInterfaces()
	{
		return _interfaceNames.iterator();
	}

	public void addInterfaceName(String interfaceName)
	{
		_interfaceNames.add(interfaceName);
	}

	/**
	 * @param instance The instance is needed for array types to calculate the size of a per-instance basis
	 * @return The size, in bytes, of an instance of this class (required for iterating the heap)
	 */
	public abstract int getInstanceSize(com.ibm.dtfj.java.JavaObject instance);
	
	public boolean equals(Object obj)
	{
		boolean isEqual = false;
		
		if (obj instanceof JavaAbstractClass) {
			JavaAbstractClass clazz = (JavaAbstractClass) obj;
			
			isEqual = (getClass().equals(clazz.getClass())) && (_javaVM.equals(clazz._javaVM) && _classPointer.equals(clazz._classPointer));
		}
		return isEqual;
	}
	
	/**
	 * Returns the size of the extra slot needed for stored hashcode - if the object was moved - in JVMs built with 
	 * J9VM_OPT_NEW_OBJECT_HASH. If the hashcode could fit in spare space in the object header, this will return 0.
	. *
	 * @return object instance size delta, in bytes
	 */
	public int getHashcodeSlotSize()
	{
		return _hashcodeSlot;
	}
	
	public int hashCode()
	{
		return _javaVM.hashCode() ^ _classPointer.hashCode();
	}
	
	public int readFlagsFromInstance(JavaObject instance) throws MemoryAccessException, CorruptDataException
	{
		try {		
			Object ersatzHeap = instance.getHeap();
			if ( ! (ersatzHeap == null || ersatzHeap instanceof CorruptData)) {
				com.ibm.dtfj.java.j9.JavaHeap heap = (com.ibm.dtfj.java.j9.JavaHeap) ersatzHeap;
				if (heap.isSWH()) {
					return (int)(instance.getID().getPointerAt(_flagOffset).getAddress() & 0xFFL);
				}
			}

		} catch (CorruptDataException e) {
			
		} catch (DataUnavailable e) {
	
		}
		// Problems looking at the heap, so do what we always did...
		return instance.getID().getIntAt(_flagOffset);
	}
	
	protected void addClassLoaderReference (Collection coll) {
		JavaReference jRef = null;

		try {
			JavaClassLoader classLoader = this.getClassLoader();
			if (null != classLoader) {
				JavaObject classLoaderObject = classLoader.getObject();
				if (null != classLoaderObject) {
					jRef = new JavaReference(_javaVM, this, classLoaderObject, "Classloader", JavaReference.REFERENCE_CLASS_LOADER, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
					coll.add(jRef);
				}
			}
		} catch (CorruptDataException e) {
			coll.add(e.getCorruptData());
		}
	}

	protected void addSuperclassReference (Collection coll) {
		JavaReference jRef = null;

		try {
			com.ibm.dtfj.java.JavaClass superClass = this.getSuperclass();
			if (null != superClass) {
				jRef = new JavaReference(_javaVM, this, superClass, "Superclass", JavaReference.REFERENCE_SUPERCLASS, JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
				coll.add(jRef);
			}
		} catch (CorruptDataException e) {
			coll.add(e.getCorruptData());
		}
	}

	protected void addClassObjectReference (Collection coll) {
	    JavaReference jRef = null;

	    try {
	        com.ibm.dtfj.java.JavaObject classObject = this.getObject();
	    
	        if(null != classObject) {
	            jRef = new JavaReference(_javaVM,this,classObject,"Class object",JavaReference.REFERENCE_CLASS_OBJECT,JavaReference.HEAP_ROOT_UNKNOWN, JavaReference.REACHABILITY_STRONG);
	            coll.add(jRef);
	        }
	    } catch (CorruptDataException e) {
	        coll.add(e.getCorruptData());
	    }
	}

	private JavaField getProtectionDomainField(JavaClass clazz) throws CorruptDataException {
		Iterator<?> fieldsIt = clazz.getDeclaredFields();
		while(fieldsIt.hasNext()) {
			Object potential = fieldsIt.next();
			
			if(potential instanceof JavaField) {
				JavaField field = (JavaField) potential;
				
				if(field.getName().equals(PROTECTION_DOMAIN_FIELD_NAME)) {
					return field;
				}
			}
		}
		return null;
	}
	
	//define the search order for the protection domains
	private enum DomainSearchOrder {
		UNDEFINED,
		J9CLASS_FIRST,
		JAVA_LANG_FIRST
	}
	
	private static DomainSearchOrder pdSearchOrder = DomainSearchOrder.UNDEFINED;
	
	public JavaObject getProtectionDomain() throws DataUnavailable,	CorruptDataException {
		JavaField _protectionDomainField = null;
		switch(pdSearchOrder) {
			case UNDEFINED :
				_protectionDomainField = getProtectionDomainField(this);
				if(_protectionDomainField == null) {
					_protectionDomainField = getProtectionDomainField(getObject().getJavaClass());
					if(_protectionDomainField != null) {
						pdSearchOrder = DomainSearchOrder.JAVA_LANG_FIRST;
					}
				} else {
					pdSearchOrder = DomainSearchOrder.J9CLASS_FIRST;
				}
			break;
			case J9CLASS_FIRST :
				_protectionDomainField = getProtectionDomainField(this);
				if(_protectionDomainField == null) {
					_protectionDomainField = getProtectionDomainField(getObject().getJavaClass());
					if(_protectionDomainField != null) {
						pdSearchOrder = DomainSearchOrder.JAVA_LANG_FIRST;
					}
				}
			break;
			case JAVA_LANG_FIRST :
				_protectionDomainField = getProtectionDomainField(getObject().getJavaClass());
				if(_protectionDomainField == null) {
					_protectionDomainField = getProtectionDomainField(this);
					if(_protectionDomainField != null) {
						pdSearchOrder = DomainSearchOrder.J9CLASS_FIRST;
					}
				}
			break;
		}
		
		if(_protectionDomainField == null) {
			throw new DataUnavailable("The protection domain is not available");
		} else {
			Object potential;
			try {
				potential = _protectionDomainField.get(this.getObject());
			} catch (MemoryAccessException e) {
				if(e.getPointer() == null) {
					CorruptData cd = new com.ibm.dtfj.image.j9.CorruptData("Invalid memory access : " + e.getMessage(), _classPointer);
					throw new CorruptDataException(cd);
				} else {
					CorruptData cd = new com.ibm.dtfj.image.j9.CorruptData("Invalid memory access : " + e.getPointer().toString(), _classPointer);
					throw new CorruptDataException(cd);
				}
			}
			
			if(potential == null) {
				return null;		//this indicates that this class is in the default protection domain
			}
			
			if(potential instanceof JavaObject) {
				return (JavaObject)potential;
			} else {
				CorruptData cd = new com.ibm.dtfj.image.j9.CorruptData("Unable to get protection domain", _classPointer);
				throw new CorruptDataException(cd);
			}
		}
	}
	
}
