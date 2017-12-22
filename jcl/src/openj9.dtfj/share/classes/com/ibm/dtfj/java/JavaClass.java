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
package com.ibm.dtfj.java;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;

/**
 * Represents a Java class.
 * 
 */
public interface JavaClass {

	/**
	 * MODIFIERS_UNAVAILABLE is returned by getModifiers() when the modifiers are unavailable. 
     * The value 0xff000000 is chosen as it is a value that can never return true to any of the java.reflect.Modifier methods 
     * that test the modifier: isNative(), isPublic(), etc. while at the same time not being 0 which is a possible value
     * when no modifier at all is present i.e. "default". 
     * 
     * All other values for the modifier returned are defined in java.lang.reflect.Modifiers
     * @see java.lang.reflect.Modifier
	 */
	static int MODIFIERS_UNAVAILABLE = 0xff000000;

    /**
     * Fetch the java.lang.Class object associated with this class.
     * <p>
     * In some implementations this may be null if no object has been
     * created to represent this class, or if the class is synthetic.
     * 
     * @return the java.lang.Class object associated with this class 
     * @throws CorruptDataException 
     * 
     * @see #getID()
     */
    public JavaObject getObject() throws CorruptDataException; 

    /**
     * Fetch the class loader associated with this class. Classes defined in
     * the bootstrap class loader (including classes representing primitive 
     * types or void) will always return a JavaClassLoader representing the
     * bootstrap class loader. This asymmetry with 
     * java.lang.Class#getClassLoader() is intentional.
     * 
     * @throws CorruptDataException if the class loader for this class cannot
     * be found (a class cannot exist without a loader so this implies corruption)
     * 
     * @return the JavaClassLoader in which this class was defined
     */
    public JavaClassLoader getClassLoader() throws CorruptDataException;
    
    /**
     * Get the name of the class.
     * @return the name of the class in the form: "full/package/class$innerclass"
     * @throws CorruptDataException 
     */
    public String getName() throws CorruptDataException;
    
    /**
     * Get the immediate superclass of this class.
     * @return the immediate superclass of this class, or null if this
     * class has no superclass. For interfaces, Object, primitive types 
     * and void null is always returned.
     * @throws CorruptDataException 
     */
    public JavaClass getSuperclass() throws CorruptDataException;
    
    
    /**
     * Get the set of names of interfaces directly implemented by this class.
     * @return an iterator over the collection of the names of interfaces 
     * directly implemented by this class. Some JVM implementations may
     * choose to load interfaces lazily, so only the names are returned.
     * The JavaClass objects may be found through the defining class loader.
     * 
     * @see java.lang.String
     * @see JavaClassLoader#findClass(String)
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getInterfaces();
    
    /**
     * Return the Java language modifiers for this class. 
     * <p>
     * The modifiers are defined by the JVM Specification.
     * <p>
     * Return MODIFIERS_UNAVAILABLE if the modifiers are unavailable.
     * This might be the case if DTFJ is operating against an artefact such as a portable heap dump that does not contain
     * information about a class's modifiers.  
     * <p>
     * Note that, for inner classes, the actual modifiers are returned, not 
     * the synthetic modifiers. For instance, a class will never have its
     * 'protected' modifier set, even if the inner class was a protected
     * member, since 'protected' is not a legal modifier for a class file.
     * 
     * @return the modifiers for this class
     * @throws CorruptDataException
     */
    public int getModifiers() throws CorruptDataException;
    
    /**
     * Is this an array class ?
     * @return true if this class is an array class
     * @throws CorruptDataException 
     */
    public boolean isArray() throws CorruptDataException;
    
    /**
     * For array classes, returns a JavaClass representing the component type of this array class.
     *
     * @return a JavaClass representing the component type of this array class
     * @throws CorruptDataException 
     * @exception java.lang.IllegalArgumentException if this JavaClass does not represent an array class
     */
    public JavaClass getComponentType() throws CorruptDataException;
    
    /**
     * Get the set of fields declared in this class. 
     * @return an iterator over the collection of fields declared
     * in this class. 
     * 
     * @see JavaField
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getDeclaredFields();
    
    /**
     * Get the set of methods declared in this class. 
     * @return an iterator over the collection of methods declared
     * in this class.
     * 
     * @see JavaMethod
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getDeclaredMethods();
    
    /**
     * Java classes may refer to other classes and to String objects via 
     * the class's constant pool. These references are followed by the 
     * garbage collector, forming edges on the graph of reachable objects.
     * This getConstantPoolReferences() may be used to determine which
     * objects are referred to by the receiver's constant pool.
     * <p>
     * Although Java VMs typically permit only Class and String 
     * objects in the constant pool, some esoteric or future virtual 
     * machines may permit other types of objects to occur in the constant 
     * pool. This API imposes no restrictions on the types of JavaObjects 
     * which might be included in the Iterator.
     * <p>
     * No assumption should be made about the order in which constant 
     * pool references are returned.
     * <p>
     * Classes may also refer to objects through static variables.
     * These may be found with the getDeclaredFields() API. Objects
     * referenced by static variables are not returned by 
     * getConstantPoolReferences() unless the object is also referenced
     * by the constant pool.
     * 
     * @return an iterator over the collection of JavaObjects which
     * are referred to by the constant pool of this class
     * 
     * @see JavaObject
     * @see com.ibm.dtfj.image.CorruptData
     */
    public Iterator getConstantPoolReferences();
    
    /**
     * The ID of a class is a pointer to a section of memory which identifies
     * the class. The contents of this memory are implementation defined.
     * <p>
     * In some implementations getID() and getObject().getID() may return the
     * same value. This implies that the class object is also the primary
     * internal representation of the class. DTFJ users should not rely on this
     * behaviour.
     * <p>
     * In some implementations, getID() may return null for some classes.
     * This indicates that the class is a synthetic class which has been 
     * constructed for DTFJ purposes only. The class has no physical 
     * representation in the VM.
     * 
     * @return a pointer to the class
     */
	public ImagePointer getID();
	
	/**
	 * Get the set of references from this class.
	 *
	 * @return an iterator of JavaReferences
	 *
	 * @see com.ibm.dtfj.java.JavaReference
	 * @see com.ibm.dtfj.image.CorruptData
	 */
	public Iterator getReferences();
    
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Class in the image
	 */
	public boolean equals(Object obj);
	
	/**
	* Returns the size in bytes of an instance of this class on the heap.
	* <p>
	* The call is only meaningful for a non-array JavaClass, where all instances of the 
	* class are of the same size. If this method is called on a JavaArrayClass, where instances
	* can be of different sizes, an UnsupportedOperationException will be thrown.
	* DataUnavailable can be thrown if no value is available: for example when 
	* DTFJ is examining a javacore, where the instance size for a class is not recorded. 
	*  
	* @return size in bytes of an instance
	* @throws DataUnavailable
	* @throws CorruptDataException
	* @exception java.lang.UnsupportedOperationException if the JavaClass is an instance of JavaArrayClass
	* @since 1.6
	*/
	public long getInstanceSize() throws DataUnavailable, CorruptDataException; 

	public int hashCode();
	
	/**
	 * Returns the protection domain for this class.
	 * 
	 * @return the protection domain or null for the default protection domain
	 * @throws DataUnavailable
	 * @throws CorruptDataException
	 * @since 1.9
	 */
	public JavaObject getProtectionDomain() throws DataUnavailable, CorruptDataException;
	
}
