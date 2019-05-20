/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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
package com.ibm.dtfj.java;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;

/**
 * Represents a Java reference.
 * 
 */
public interface JavaReference
{
    
	/** Unknown heap root type */
    int HEAP_ROOT_UNKNOWN      = 0;
    /** JNI global reference heap root */
    int HEAP_ROOT_JNI_GLOBAL   = 1;
    /** System class heap root */
    int HEAP_ROOT_SYSTEM_CLASS = 2;
    /** Monitor heap root */
    int HEAP_ROOT_MONITOR      = 3;
    /** Stack local heap root */
    int HEAP_ROOT_STACK_LOCAL  = 4;
    /** JNI local reference heap root */
    int HEAP_ROOT_JNI_LOCAL    = 5;
    /** Thread heap root */
    int HEAP_ROOT_THREAD       = 6;
    /** Other heap root type */
    int HEAP_ROOT_OTHER        = 7;
    /** Finalizable object heap root */
    int HEAP_ROOT_FINALIZABLE_OBJ = 8;
    /** Unfinalized object heap root */
    int HEAP_ROOT_UNFINALIZED_OBJ = 9;
    /** Classloader heap root */
    int HEAP_ROOT_CLASSLOADER = 10;
    /** Stringtable heap root */
    int HEAP_ROOT_STRINGTABLE = 11;
    

     /** Unknown reference type */
    int REFERENCE_UNKNOWN           = 0;
    /** Reference from an object to its class */ 
    int REFERENCE_CLASS             = 1;  // 
    /** Reference from an object to the value of one of its instance fields */
    int REFERENCE_FIELD             = 2;  
    /** Reference from an array to one of its elements */
    int REFERENCE_ARRAY_ELEMENT     = 3;
    /** Reference from a class to its class loader */
    int REFERENCE_CLASS_LOADER      = 4;
    /** Reference from a class to its signers array */
    int REFERENCE_SIGNERS           = 5;
    /** Reference from a class to its protection domain */
    int REFERENCE_PROTECTION_DOMAIN = 6;
    /** Reference from a class to one of its interfaces */
    int REFERENCE_INTERFACE         = 7;
    /** Reference from a class to the value of one of its static fields */
    int REFERENCE_STATIC_FIELD      = 8; 
    /** Reference from a class to a resolved entry in the constant pool */
    int REFERENCE_CONSTANT_POOL     = 9;
    /** Reference from a class to its superclass */
    int REFERENCE_SUPERCLASS     	= 10;
    /** Reference from a classloader object to its loaded classes */
    int REFERENCE_LOADED_CLASS     	= 11;
    /** Reference from a class to its java.lang.Class instance */
    int REFERENCE_CLASS_OBJECT      = 12;
	/** Reference from a JavaObject representing a Class to the associated JavaClass **/
    int REFERENCE_ASSOCIATED_CLASS 	= 13;
    int REFERENCE_UNUSED_14 = 14;


    /** Reachability of target object via this reference is unknown */
    int REACHABILITY_UNKNOWN =  0;
    /** Reachability of target object via this reference is Strong */
    int REACHABILITY_STRONG  =  1;
    /** Reachability of target object via this reference is Soft */
    int REACHABILITY_SOFT    =  2;
    /** Reachability of target object via this reference is Weak */
    int REACHABILITY_WEAK    =  3;
    /** Reachability of target object via this reference is Phantom */
    int REACHABILITY_PHANTOM =  4;

    /**
     * Get the root type, as defined in the JVMTI specification.
     *
     * @return an integer representing the root type, see HEAP_ROOT_ statics above.
     */
    public int getRootType() throws CorruptDataException;

    /**
     * Get the reference type, as defined in the JVMTI specification.
     * @return an integer representing the reference type, see REFERENCE_ statics above.
     */
    public int getReferenceType() throws CorruptDataException;

    /**
     * Get the reachability of the target object via this specific reference.
     * @return an integer representing the reachability, see REACHABILITY_ statics above.
     */
    public int getReachability() throws CorruptDataException;

    /**
     * Get a string describing the reference type.
     * Implementers should not depend on the contents or identity of this string.
     * e.g. "JNI Weak global reference", "Instance field 'MyClass.value'", "Constant pool string constant"
     * @return a String describing the reference type
     */
    public String getDescription();

    /**
     * Does this reference point to an object in the heap?
     * @return true if the target of this root is an object
     */
    public boolean isObjectReference() throws DataUnavailable, CorruptDataException;

    /**
     * Does this reference point to a class?
     * @return true if the target of this root is a class
     */
    public boolean isClassReference() throws DataUnavailable, CorruptDataException;

    /**
     * Get the object referred to by this reference.
     * @return a JavaObject or a JavaClass
     */
    public Object getTarget() throws DataUnavailable, CorruptDataException;

    /**
     * Get the source of this reference if available.
     * @return a JavaClass, JavaObject, JavaStackFrame or null if unknown
     */
    public Object getSource() throws DataUnavailable, CorruptDataException;
}
