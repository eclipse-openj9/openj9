/*[INCLUDE-IF Valhalla-MVT]*/
/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package jdk.experimental.value;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.MethodHandles.Lookup;

/**
 * Support class for Minimal Value Types
 *
 * @param <T> The type of the Value Capable Class (VCC)
 */
public class ValueType<T> {
	private Class<T> sourceClass;

	private ValueType(Class<T> clazz) {
		this.sourceClass = clazz;
	}

	/**
	 * Check whether a class is value capable.
	 * 
	 * @param clazz The class to check
	 * @return A boolean indicating whether the provided class is value capable
	 */
	public static <T> boolean classHasValueType(Class<T> clazz) {
		return clazz.isAnnotationPresent(jvm.internal.value.DeriveValueType.class);
	}

	/**
	 * Create a ValueType instance for the provided class.
	 * 
	 * @param clazz The value capable class for which to create a ValueType
	 * @return A ValueType instance for the provided class
	 * @throws IllegalArgumentException If the provided class is not value capable
	 */
	public static <T> ValueType<T> forClass(Class<T> clazz) throws IllegalArgumentException {
		if (!classHasValueType(clazz)) {
			throw new IllegalArgumentException(clazz.getCanonicalName());
		}
		return new ValueType<>(clazz);
	}

	/**
	 * Get the Derived Value Type (DVT) Class instance
	 * 
	 * @return The derived value type Class instance
	 */
	public Class<T> valueClass() {
		return valueClassImpl(sourceClass);
	}
	
	private static native <T> Class<T> valueClassImpl(Class<T> sourceClass);

	/**
	 * Get the Value Capable Class (VCC) Class instance
	 * 
	 * @return The value capable Class instance
	 */
	public Class<T> boxClass() {
		return sourceClass;
	}
	
	/**
	 * Get a MethodHandle that returns a default value
	 * 
	 * @return A MethodHandle that returns a default value
	 */
	public MethodHandle defaultValueConstant() {
		return MethodHandles.zero(valueClass());
	}
	
	/**
	 * Get a MethodHandle that performs a field-by-field comparison of two value
	 * 
	 * @return A MethodHandle that compares two values
	 */
	public MethodHandle substitutabilityTest() {
		throw new UnsupportedOperationException();
	}
	
	/**
	 * Get a MethodHandle that returns a hashcode for a value.
	 * 
	 * @return A MethodHandle that returns a hashcode for a value
	 */
	public MethodHandle substitutabilityHashCode() {
		throw new UnsupportedOperationException();
	}
	
	/**
	 * Get a MethodHandle that returns a new value given 
	 * <br> - a value of the same type and 
	 * <br> - a new value for a given value field.
	 * 
	 * @param lookup A Lookup object for accessing fields of the DVT
	 * @param refc The DVT
	 * @param name The name of the field to update
	 * @param type The type of the field to update
	 * @return A MethodHandle that can generate a modified value from another value
	 */
	public MethodHandle findWither(Lookup lookup, Class<?> refc, String name, Class<?> type) {
		throw new UnsupportedOperationException();
	}
}
