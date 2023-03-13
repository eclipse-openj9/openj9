/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.j9ddr.vm29.j9;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

public final class J9ConstantHelper {

	private static Field getStaticFinalField(Class<?> clazz, String name, Class<?> expectedFieldType) throws NoSuchFieldException {
		try {
			Field field = clazz.getField(name);
			int modifiers = field.getModifiers();

			// check that the field has the expected modifiers and type
			if (!Modifier.isPublic(modifiers) || !Modifier.isStatic(modifiers) || !Modifier.isFinal(modifiers)) {
				String message = String.format("%s.%s is not public static final", clazz.getName(), name);
				throw new IllegalArgumentException(message);
			}

			if (field.getType() != expectedFieldType) {
				String message = String.format("%s.%s is not type %s", clazz.getName(), name, expectedFieldType.getName());
				throw new IllegalArgumentException(message);
			}
			return field;
		} catch (SecurityException e) {
			throw new InternalError("unexpected exception", e);
		}
	}

	/**
	 * Using reflection, read the value of a public static final long field from the given
	 * class or, if the field is not present, return the default value provided.
	 *
	 * @param clazz the class which owns the field of interest
	 * @param name the name of the field
	 * @param defaultValue the value to be returned if the field is not present
	 * @return the value of the field, or the default value
	 */
	public static long getLong(Class<?> clazz, String name, long defaultValue) {
		try {
			return getStaticFinalField(clazz, name, long.class).getLong(null);
		} catch (IllegalAccessException e) {
			// this should not happen - the field is public static
			throw new InternalError("unexpected exception", e);
		} catch (NoSuchFieldException e) {
			return defaultValue;
		}
	}

	/**
	 * Using reflection, read the value of a public static final boolean field from the given
	 * class or, if the field is not present, return the default value provided.
	 *
	 * @param clazz the class which owns the field of interest
	 * @param name the name of the field
	 * @param defaultValue the value to be returned if the field is not present
	 * @return the value of the boolean field, or the default value
	 */
	public static boolean getBoolean(Class<?> clazz, String name, boolean defaultValue) {
		try {
			return getStaticFinalField(clazz, name, boolean.class).getBoolean(null);
		} catch (IllegalAccessException e) {
			// this should not happen - the field is public static
			throw new InternalError("unexpected exception", e);
		} catch (NoSuchFieldException e) {
			return defaultValue;
		}
	}

	/**
	 * Using reflection, read the value of a public static final int field from the given
	 * class or, if the field is not present, return the default value provided.
	 *
	 * @param clazz the class which owns the field of interest
	 * @param name the name of the field
	 * @param defaultValue the value to be returned if the field is not present
	 * @return the value of the field, or the default value
	 */
	public static int getInt(Class<?> clazz, String name, int defaultValue) {
		try {
			return getStaticFinalField(clazz, name, int.class).getInt(null);
		} catch (IllegalAccessException e) {
			// this should not happen - the field is public static
			throw new InternalError("unexpected exception", e);
		} catch (NoSuchFieldException e) {
			return defaultValue;
		}
	}

}
