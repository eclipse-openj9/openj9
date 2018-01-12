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
package com.ibm.dtfj.phd;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;

/** 
 * @author ajohnson
 */
class PHDJavaReference implements JavaReference {

	private final Object target;
	private final Object source;
	private final int reach;
	private final int ref;
	private final int root;
	private final String desc;
	
	PHDJavaReference(Object target, Object source, int reach, int ref, int root, String desc) {
		this.target = target;
		this.source = source;
		this.reach = reach;
		this.ref = ref;
		this.root = root;
		switch (ref) {
	     /** Unknown reference type */
		case REFERENCE_UNKNOWN:
			desc = "Unknown reference";
			break;
	    /** Reference from an object to its class */
		case REFERENCE_CLASS:
			desc = "Class reference";
			break;
	    /** Reference from an object to the value of one of its instance fields */
		case REFERENCE_FIELD:
			desc = "Field reference";
			break;
	    /** Reference from an array to one of its elements */
		case REFERENCE_ARRAY_ELEMENT:
			desc = "Array element reference";
			break;
	    /** Reference from a class to its class loader */
		case REFERENCE_CLASS_LOADER:
			desc = "Class loader reference";
			break;
	    /** Reference from a class to its signers array */
		case REFERENCE_SIGNERS:
			desc = "Signers reference";
			break;
	    /** Reference from a class to its protection domain */
		case REFERENCE_PROTECTION_DOMAIN:
			desc = "Protection domain reference";
			break;
	    /** Reference from a class to one of its interfaces */
		case REFERENCE_INTERFACE:
			desc = "Interface reference";
			break;
	    /** Reference from a class to the value of one of its static fields */
		case REFERENCE_STATIC_FIELD:
			desc = "Static field reference";
			break;
	    /** Reference from a class to a resolved entry in the constant pool */
		case REFERENCE_CONSTANT_POOL:
			desc = "Constant pool reference";
			break;
	    /** Reference from a class to its superclass */
		case REFERENCE_SUPERCLASS:
			desc = "Superclass reference";
			break;
	    /** Reference from a classloader object to its loaded classes */
		case REFERENCE_LOADED_CLASS:
			desc = "Loaded class reference";
			break;
	    /** Reference from a class to its java.lang.Class instance */
		case REFERENCE_CLASS_OBJECT:
			desc = "Class object reference";
			break;
		/** Reference from a JavaObject representing a Class to the associated JavaClass **/
		case REFERENCE_ASSOCIATED_CLASS:
			desc = "Associated class reference";
			break;
		}
		this.desc = desc;
	}
	
	public String getDescription() {
		return desc;
	}

	public int getReachability() throws CorruptDataException {
		return reach;
	}

	public int getReferenceType() throws CorruptDataException {
		return ref;
	}

	public int getRootType() throws CorruptDataException {
		return root;
	}

	public Object getSource() throws DataUnavailable, CorruptDataException {
		return source;
	}

	public Object getTarget() throws DataUnavailable, CorruptDataException {
		return target;
	}

	public boolean isClassReference() throws DataUnavailable,
			CorruptDataException {
		return target instanceof JavaClass;
	}

	public boolean isObjectReference() throws DataUnavailable,
			CorruptDataException {
		return target instanceof JavaObject;
	}

	public boolean equals(Object o) {
		if (o instanceof PHDJavaReference) {
			PHDJavaReference to = (PHDJavaReference)o;
			if (target == to.target &&
				source == to.source &&
				reach == to.reach &&
				ref == to.ref &&
				root == to.root &&
				(getDescription() == null ? to.getDescription() == null : getDescription().equals(to.getDescription())) ) {
				return true;
			}
		}
		return false;
	}
	
	public int hashCode() {
		return (target != null ? target.hashCode() : 0) ^
			(source != null && source != target ? source.hashCode() : 0) ^
			reach ^ ref ^ root ^ (getDescription() != null ? getDescription().hashCode() : 0);  		
	}
	
	public String toString() {
		return getClass().getName()+"["+target+","+source+","+desc+"]";
	}
}
