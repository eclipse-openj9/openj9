/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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
package java.lang.invoke;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;


/*
 * MethodHandle subclass that is able to return the value of
 * an instance field.
 * <p>
 * vmSlot will hold the field offset in the instance.
 * 
 */
final class FieldGetterHandle extends FieldHandle {
	
	FieldGetterHandle(Class<?> referenceClass, String fieldName, Class<?> fieldClass, Class<?> accessClass) throws IllegalAccessException, NoSuchFieldException {
		super(fieldMethodType(fieldClass, referenceClass), referenceClass, fieldName, fieldClass, KIND_GETFIELD, accessClass);
	}
	
	FieldGetterHandle(Field field) throws IllegalAccessException {
		super(fieldMethodType(field.getType(), field.getDeclaringClass()), field, KIND_GETFIELD, false);
	}

	FieldGetterHandle(FieldGetterHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	/* Create the MethodType to be passed to the constructor */
	private final static MethodType fieldMethodType(Class<?> returnType, Class<?> argument) {
		return MethodType.methodType(returnType, argument);
	}

	// {{{ JIT support
	@FrameIteratorSkip
	private final int    invokeExact_thunkArchetype_I(Object receiver, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getIntVolatile(receiver, vmSlot + HEADER_SIZE);
		else
			return UNSAFE.getInt        (receiver, vmSlot + HEADER_SIZE);
	}

	@FrameIteratorSkip
	private final long   invokeExact_thunkArchetype_J(Object receiver, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getLongVolatile(receiver, vmSlot + HEADER_SIZE);
		else
			return UNSAFE.getLong        (receiver, vmSlot + HEADER_SIZE);
	}

	@FrameIteratorSkip
	private final float  invokeExact_thunkArchetype_F(Object receiver, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getFloatVolatile(receiver, vmSlot + HEADER_SIZE);
		else
			return UNSAFE.getFloat        (receiver, vmSlot + HEADER_SIZE);
	}

	@FrameIteratorSkip
	private final double invokeExact_thunkArchetype_D(Object receiver, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getDoubleVolatile(receiver, vmSlot + HEADER_SIZE);
		else
			return UNSAFE.getDouble        (receiver, vmSlot + HEADER_SIZE);
	}

	@FrameIteratorSkip
	private final Object invokeExact_thunkArchetype_L(Object receiver, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getObjectVolatile(receiver, vmSlot + HEADER_SIZE);
		else
			return UNSAFE.getObject        (receiver, vmSlot + HEADER_SIZE);
	}

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }
	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new FieldGetterHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof FieldGetterHandle) {
			((FieldGetterHandle)right).compareWithFieldGetter(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithFieldGetter(FieldGetterHandle left, Comparator c) {
		compareWithField(left, c);
	}
}

