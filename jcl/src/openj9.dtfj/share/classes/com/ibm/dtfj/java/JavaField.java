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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.MemoryAccessException;

/**
 * <p>Represents a Java field declaration.</p>
 * 
 * <p>This interface is modeled on java.lang.reflect.Field.</p>
 */
public interface JavaField extends JavaMember {

    /**
     * Get the contents of an Object field
     * @param object to fetch the field from. Ignored for static
     * fields.
     * 
     * @return a JavaObject instance for reference type fields,
     * an instance of a subclass of Number, Boolean, or Character 
     * for primitive fields, or null for null reference fields.
     * <p>
     * This field must be declared in the object's class or in a superclass
     * <p>
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * @throws NullPointerException if the field is an instance field, and object is null
     * @throws IllegalArgumentException if the specified object is not appropriate for
     * this field
     *
     * @see JavaObject
     * @see Byte
     * @see Double
     * @see Float
     * @see Integer
     * @see Long
     * @see Short
     * @see Character
     * @see Boolean
     */
    public Object get(JavaObject object) throws CorruptDataException, MemoryAccessException;

    /**
     * Get the contents of a boolean field
     * @param  object to fetch the field from. Ignored for static fields.
     * @return the field contents
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * @throws NullPointerException if the field is an instance field, and object is null
     * @throws IllegalArgumentException if the specified object is not appropriate for
     * this field, or if the type of the field cannot be converted to boolean
     */
    public boolean getBoolean(JavaObject object) throws CorruptDataException, MemoryAccessException;

    /**
     * Get the contents of a byte field
     * @param  object to fetch the field from. Ignored for static fields.
     * @return the field contents
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * @throws NullPointerException if the field is an instance field, and object is null
     * @throws IllegalArgumentException if the specified object is not appropriate for
     * this field, or if the type of the field cannot be converted to byte
     */
    public byte getByte(JavaObject object) throws CorruptDataException, MemoryAccessException;

    /**
     * Get the contents of a char field
     * @param  object to fetch the field from. Ignored for static fields.
     * @return the field contents
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * @throws NullPointerException if the field is an instance field, and object is null
     * @throws IllegalArgumentException if the specified object is not appropriate for
     * this field, or if the type of the field cannot be converted to char
     */
    public char getChar(JavaObject object) throws CorruptDataException, MemoryAccessException;

    /**
     * Get the contents of a double field
     * @param  object to fetch the field from. Ignored for static fields.
     * @return the field contents
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * @throws NullPointerException if the field is an instance field, and object is null
     * @throws IllegalArgumentException if the specified object is not appropriate for
     * this field, or if the type of the field cannot be converted to double
     */
    public double getDouble(JavaObject object) throws CorruptDataException, MemoryAccessException;

    /**
     * Get the contents of a float field
     * @param  object to fetch the field from. Ignored for static fields.
     * @return the field contents 
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * @throws NullPointerException if the field is an instance field, and object is null
     * @throws IllegalArgumentException if the specified object is not appropriate for
     * this field, or if the type of the field cannot be converted to float
     */
    public float getFloat(JavaObject object) throws CorruptDataException, MemoryAccessException;

    /**
     * Get the contents of an int field
     * @return the field contents
     * @param  object to fetch the field from. Ignored for static fields.
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * @throws NullPointerException if the field is an instance field, and object is null
     * @throws IllegalArgumentException if the specified object is not appropriate for
     * this field, or if the type of the field cannot be converted to int
     */
    public int getInt(JavaObject object) throws CorruptDataException, MemoryAccessException;

    /**
     * Get the contents of a long field
     * @param  object to fetch the field from. Ignored for static fields.
     * @return the field contents
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * @throws NullPointerException if the field is an instance field, and object is null
     * @throws IllegalArgumentException if the specified object is not appropriate for
     * this field, or if the type of the field cannot be converted to long
     */
    public long getLong(JavaObject object) throws CorruptDataException, MemoryAccessException;

    /**
     * Get the contents of a short field
     * @param  object to fetch the field from. Ignored for static fields.
     * @return the field contents 
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * @throws NullPointerException if the field is an instance field, and object is null
     * @throws IllegalArgumentException if the specified object is not appropriate for
     * this field, or if the type of the field cannot be converted to short
     */
    public short getShort(JavaObject object) throws CorruptDataException, MemoryAccessException;
    
    /**
     * Get the contents of a string field
     * @param object to fetch the field from. Ignored for static fields.
     * 
     * @return a String representing the value of the String field.  Note that the instance
     * returned can be null if the field was null in object.
     * @throws CorruptDataException 
     * @throws MemoryAccessException 
     * 
     * @throws IllegalArgumentException if the specified field is not a String
     * @throws NullPointerException if the field is an instance field, and object is null
     */
    public String getString(JavaObject object) throws CorruptDataException, MemoryAccessException;
    
	/**
	 * @param obj
	 * @return True if the given object refers to the same Java Field in the image
	 */
	public boolean equals(Object obj);
	public int hashCode();

	@Deprecated
	public default boolean isNestedPacked() throws CorruptDataException, MemoryAccessException {
		return false;
	}
	
	@Deprecated
	public default boolean isNestedPackedArray() throws CorruptDataException, MemoryAccessException {
		return false;
	}
}
