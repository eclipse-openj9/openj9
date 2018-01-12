/*******************************************************************************
 * Copyright (c) 2008, 2014 IBM Corp. and others
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


import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaReference;

/**
 * JavaReference is intended to represent either a standard reference within a java heap,
 * for example a reference from one object to another, or a root. A root is a reference 
 * that is held outside of the heap, in the Java stack or within the JVM itself.
 */
public class DTFJJavaReference implements com.ibm.dtfj.java.JavaReference
{
	private static final int ResolutionType_UNRESOLVED = 0;
	private static final int ResolutionType_CLASS = 1;
	private static final int ResolutionType_OBJECT = 2;
	private static final int ResolutionType_BROKEN = 3;

	private String _description = null;
	private int    _reachability = REACHABILITY_UNKNOWN;
	private int    _referencetype = REFERENCE_UNKNOWN;
	private int    _roottype = HEAP_ROOT_UNKNOWN;
	private Object _source = null;
	private Object _target = null;
	private int _resolution = ResolutionType_UNRESOLVED;
	
	@Override
	public String toString()
	{
		StringBuilder toReturn = new StringBuilder();
		
		toReturn.append("Reference from \"");
		toReturn.append(_source);
		toReturn.append("\" to \"");
		toReturn.append(_target);
		toReturn.append("\". Description: ");
		toReturn.append(_description);
		toReturn.append(". Reachability: ");
		
		switch (_reachability) {
		case JavaReference.REACHABILITY_STRONG:
			toReturn.append("STRONG");
			break;
		case JavaReference.REACHABILITY_WEAK:
			toReturn.append("WEAK");
			break;
		case JavaReference.REACHABILITY_PHANTOM:
			toReturn.append("PHANTOM");
			break;
		case JavaReference.REACHABILITY_SOFT:
			toReturn.append("SOFT");
			break;
		case JavaReference.REACHABILITY_UNKNOWN:
			toReturn.append("UNKNOWN");
			break;
		default:
			toReturn.append("Unrecognised reachability code: " + _reachability);
			break;
		}
		
		toReturn.append(". Resolution: ");
		toReturn.append(_resolution);
		toReturn.append(". Roottype: ");
		toReturn.append(_roottype);
		toReturn.append(". Reference type: ");
		toReturn.append(_referencetype);
		toReturn.append(".");
		
		
		return toReturn.toString();
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
	public DTFJJavaReference(Object source, Object target, String description, int referencetype, int roottype, int reachability) {
		_source = source;
		_target = target;
		_description = description;
		_referencetype = referencetype;
		_roottype = roottype;
		_reachability = reachability;

		if (target == null) {
			throw new IllegalArgumentException("Target may not be null");
		}
		
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
		return _target;
	}

	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#isClassReference()
	 */
	public boolean isClassReference() throws DataUnavailable, CorruptDataException {
		if (ResolutionType_BROKEN == _resolution) {
			throw new DataUnavailable();
		}

		return ResolutionType_CLASS == _resolution;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.java.JavaReference#isObjectReference()
	 */
	public boolean isObjectReference() throws DataUnavailable, CorruptDataException {
		if (ResolutionType_BROKEN == _resolution) {
			throw new DataUnavailable();
		}

		return ResolutionType_OBJECT == _resolution;
	}
}
