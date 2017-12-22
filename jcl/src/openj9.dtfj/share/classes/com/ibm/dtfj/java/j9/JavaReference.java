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
package com.ibm.dtfj.java.j9;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.j9.CorruptData;

/**
 * @author nhardman
 * 
 * JavaReference is intended to represent either a standard reference within a java heap,
 * for example a reference from one object to another, or a root. A root is a reference 
 * that is held outside of the heap, in the Java stack or within the JVM itself.
 *
 */
public class JavaReference implements com.ibm.dtfj.java.JavaReference
{
	private static final int ResolutionType_UNRESOLVED = 0;
	private static final int ResolutionType_CLASS = 1;
	private static final int ResolutionType_OBJECT = 2;
	private static final int ResolutionType_BROKEN = 3;

	private JavaRuntime _javaVM = null;
	private String _description = null;
	private int    _reachability = REACHABILITY_UNKNOWN;
	private int    _referencetype = REFERENCE_UNKNOWN;
	private int    _roottype = HEAP_ROOT_UNKNOWN;
	private Object _source = null;
	private Object _target = null;
	private long   _address = 0;
	private int _resolution = ResolutionType_UNRESOLVED;
	
	/**
	 * Constructor
	 * 
	 * @param javaVM JavaRuntime
	 * @param source Object The source of this reference/root, for example the JVM or an object on the heap
	 * @param address long Address of the target object of this reference/root.
	 * @param description String  
	 * @param referencetype int Mutually exclusive with respect to roottype. 
	 * @param roottype int Mutually exclusive with respect to referencetype.
	 * @param reachability int the strength of the reference (this helps the GC in selection of objects for collection).  
	 */
	public JavaReference(JavaRuntime javaVM, Object source, long address, String description, int referencetype, int roottype, int reachability) {
		_javaVM = javaVM;
		_source = source;
		_address = address;
		_description = description;
		_referencetype = referencetype;
		_roottype = roottype;
		_reachability = reachability;
	}

	/**
	 * Constructor
	 * 
	 * @param javaVM JavaRuntime
	 * @param source Object The source of this reference/root, for example the JVM or an object on the heap
	 * @param target Object The target object of this reference/root.
	 * @param description String  
	 * @param referencetype int Mutually exclusive with respect to roottype. 
	 * @param roottype int Mutually exclusive with respect to referencetype.
	 * @param reachability int The strength of the reference (this helps the GC in selection of objects for collection).  
	 */
	public JavaReference(JavaRuntime javaVM, Object source, Object target, String description, int referencetype, int roottype, int reachability) {
		_javaVM = javaVM;
		_source = source;
		_target = target;
		_description = description;
		_referencetype = referencetype;
		_roottype = roottype;
		_reachability = reachability;

		if (null != _target) {
			if (JavaReference.HEAP_ROOT_SYSTEM_CLASS == _roottype || 
				JavaReference.REFERENCE_CLASS == _referencetype || 
				JavaReference.REFERENCE_SUPERCLASS == _referencetype || 
				JavaReference.REFERENCE_LOADED_CLASS == _referencetype ||
				JavaReference.REFERENCE_ASSOCIATED_CLASS == _referencetype) {
				/* target is a class. */
				_resolution = ResolutionType_CLASS;
			} else if ((JavaReference.HEAP_ROOT_JNI_GLOBAL == _roottype) ||
			           (JavaReference.HEAP_ROOT_JNI_LOCAL == _roottype) ||
			           (JavaReference.HEAP_ROOT_MONITOR == _roottype) ||
			           (JavaReference.HEAP_ROOT_OTHER == _roottype) ||
			           (JavaReference.HEAP_ROOT_STACK_LOCAL == _roottype) ||
			           (JavaReference.HEAP_ROOT_THREAD == _roottype) ||
			           (JavaReference.HEAP_ROOT_FINALIZABLE_OBJ == _roottype) ||
			           (JavaReference.HEAP_ROOT_UNFINALIZED_OBJ == _roottype) ||
			           (JavaReference.HEAP_ROOT_CLASSLOADER == _roottype) ||
			           (JavaReference.HEAP_ROOT_STRINGTABLE == _roottype) ||
			           (JavaReference.REFERENCE_ARRAY_ELEMENT == _referencetype) ||
			           (JavaReference.REFERENCE_CLASS_LOADER == _referencetype) ||
			           (JavaReference.REFERENCE_CONSTANT_POOL == _referencetype) ||
			           (JavaReference.REFERENCE_FIELD == _referencetype) ||
			           (JavaReference.REFERENCE_INTERFACE == _referencetype) ||
			           (JavaReference.REFERENCE_PROTECTION_DOMAIN == _referencetype) ||
			           (JavaReference.REFERENCE_SIGNERS == _referencetype) ||
			           (JavaReference.REFERENCE_STATIC_FIELD == _referencetype) ||
			           (JavaReference.REFERENCE_CLASS_OBJECT == _referencetype)) {
				/* target is an object. */
				_resolution = ResolutionType_OBJECT;
			} else {
				/* target has a value but we cannot determine what it is. */
				_resolution = ResolutionType_BROKEN;
			}
		} else {
			// the target passed is null and as we know address is currently
			// set to zero, then this reference can never be resolved.
			_resolution = ResolutionType_UNRESOLVED;
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#getDescription()
	 */
	public String getDescription() {
		/* return the description of this reference. */
		return _description;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#getReachability()
	 */
	public int getReachability() throws CorruptDataException {
		return _reachability;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#getReferenceType()
	 */
	public int getReferenceType() throws CorruptDataException {
		return _referencetype;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#getRootType()
	 */
	public int getRootType() throws CorruptDataException {
		return _roottype;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#getSource()
	 */
	public Object getSource() throws DataUnavailable, CorruptDataException {
		/* source object for this reference (i.e. the object that contains this reference). */
		return _source;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#getTarget()
	 */
	public Object getTarget() throws DataUnavailable, CorruptDataException {
		if (null == _target) {
			if (0 == _address) {
				// we have no way of determining the target so it is unresolved.
				_resolution = ResolutionType_UNRESOLVED;
				return null;
			}

			if (JavaReference.HEAP_ROOT_SYSTEM_CLASS == _roottype || 
				JavaReference.REFERENCE_CLASS == _referencetype || 
				JavaReference.REFERENCE_SUPERCLASS == _referencetype || 
				JavaReference.REFERENCE_LOADED_CLASS == _referencetype ||
				JavaReference.REFERENCE_ASSOCIATED_CLASS == _referencetype) {
				
				// this is a class reference, so create a class to represent the target.
				_target = _javaVM.getClassForID(_address);
				if (null == _target) {
					ImagePointer pointer = _javaVM.pointerInAddressSpace(_address);
					_resolution = ResolutionType_BROKEN;
					throw new CorruptDataException(new CorruptData("Unknown class ID", pointer));
				}
				_resolution = ResolutionType_CLASS;
			} else if ((JavaReference.HEAP_ROOT_JNI_GLOBAL == _roottype) ||
			           (JavaReference.HEAP_ROOT_JNI_LOCAL == _roottype) ||
			           (JavaReference.HEAP_ROOT_MONITOR == _roottype) ||
			           (JavaReference.HEAP_ROOT_OTHER == _roottype) ||
			           (JavaReference.HEAP_ROOT_STACK_LOCAL == _roottype) ||
			           (JavaReference.HEAP_ROOT_THREAD == _roottype) ||
			           (JavaReference.HEAP_ROOT_FINALIZABLE_OBJ == _roottype) ||
			           (JavaReference.HEAP_ROOT_UNFINALIZED_OBJ == _roottype) ||
			           (JavaReference.HEAP_ROOT_CLASSLOADER == _roottype) ||
			           (JavaReference.HEAP_ROOT_STRINGTABLE == _roottype) ||
			           (JavaReference.REFERENCE_ARRAY_ELEMENT == _referencetype) ||
			           (JavaReference.REFERENCE_CLASS_LOADER == _referencetype) ||
			           (JavaReference.REFERENCE_CONSTANT_POOL == _referencetype) ||
			           (JavaReference.REFERENCE_FIELD == _referencetype) ||
			           (JavaReference.REFERENCE_INTERFACE == _referencetype) ||
			           (JavaReference.REFERENCE_PROTECTION_DOMAIN == _referencetype) ||
			           (JavaReference.REFERENCE_SIGNERS == _referencetype) ||
			           (JavaReference.REFERENCE_STATIC_FIELD == _referencetype) ||
			           (JavaReference.REFERENCE_CLASS_OBJECT == _referencetype)) {
				// this is an object reference, so create a object to represent the target.
				ImagePointer pointer = _javaVM.pointerInAddressSpace(_address);
				try {
					_target = _javaVM.getObjectAtAddress(pointer);
				} catch (IllegalArgumentException e) {
					// getObjectAtAddress can throw an IllegalArgumentException if the address is not aligned
					_resolution = ResolutionType_BROKEN;
					throw new CorruptDataException(new com.ibm.dtfj.image.j9.CorruptData(e.getMessage(),pointer));
				}
				if (_target == null) {
					_resolution = ResolutionType_BROKEN;
					throw new CorruptDataException(new CorruptData("Unknown object ID", pointer));
				}
				_resolution = ResolutionType_OBJECT;
			} else {
				// we have no way of determining the target so it is unresolved.
				_resolution = ResolutionType_UNRESOLVED;
				return null;
			}
		}
		
		return _target;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#isClassReference()
	 */
	public boolean isClassReference() throws DataUnavailable, CorruptDataException {
		if (ResolutionType_UNRESOLVED == _resolution) {
			// the target is unresolved, so we need to get it.
			_target = getTarget();
		}
		
		if (ResolutionType_BROKEN == _resolution) {
			throw new DataUnavailable();
		}

		return ResolutionType_CLASS == _resolution;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#isObjectReference()
	 */
	public boolean isObjectReference() throws DataUnavailable, CorruptDataException {
		if (ResolutionType_UNRESOLVED == _resolution) {
			// the target is unresolved, so we need to get it.
			_target = getTarget();
		}
		
		if (ResolutionType_BROKEN == _resolution) {
			throw new DataUnavailable();
		}

		return ResolutionType_OBJECT == _resolution;
	}
}
