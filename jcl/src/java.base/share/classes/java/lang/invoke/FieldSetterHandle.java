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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package java.lang.invoke;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

/*
 * MethodHandle subclass that is able to set the value of
 * an instance field.
 * <p>
 * vmSlot will hold the field offset in the instance.
 * 
 */
final class FieldSetterHandle extends FieldHandle {
	
	FieldSetterHandle(Class<?> referenceClass, String fieldName, Class<?> fieldClass, Class<?> accessClass) throws IllegalAccessException, NoSuchFieldException {
		super(fieldMethodType(referenceClass, fieldClass), referenceClass, fieldName, fieldClass, KIND_PUTFIELD, accessClass);
	}
	
	FieldSetterHandle(Field field) throws IllegalAccessException {
		super(fieldMethodType(field.getDeclaringClass(), field.getType()), field, KIND_PUTFIELD, false);
	}
	
	FieldSetterHandle(FieldSetterHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	/*
	 * Create the MethodType to be passed to the constructor
	 * MethodType of a field setter is (instanceType, fieldType)V. 
	 */
	private final static MethodType fieldMethodType(Class<?> referenceClass, Class<?> fieldClass) {
		return MethodType.methodType(void.class, referenceClass, fieldClass);
	}

	// {{{ JIT support
	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(Object receiver, int    newValue, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putIntVolatile(receiver, vmSlot + HEADER_SIZE, newValue);
		else
			UNSAFE.putInt        (receiver, vmSlot + HEADER_SIZE, newValue);
	}
	
	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(Object receiver, long   newValue, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putLongVolatile(receiver, vmSlot + HEADER_SIZE, newValue);
		else
			UNSAFE.putLong        (receiver, vmSlot + HEADER_SIZE, newValue);
	}

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(Object receiver, float  newValue, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putFloatVolatile(receiver, vmSlot + HEADER_SIZE, newValue);
		else
			UNSAFE.putFloat        (receiver, vmSlot + HEADER_SIZE, newValue);
	}

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(Object receiver, double newValue, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putDoubleVolatile(receiver, vmSlot + HEADER_SIZE, newValue);
		else
			UNSAFE.putDouble        (receiver, vmSlot + HEADER_SIZE, newValue);
	}

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(Object receiver, Object newValue, int argPlaceholder) {
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putObjectVolatile(receiver, vmSlot + HEADER_SIZE, newValue);
		else
			UNSAFE.putObject        (receiver, vmSlot + HEADER_SIZE, newValue);
	}

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }
	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new FieldSetterHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof FieldSetterHandle) {
			((FieldSetterHandle)right).compareWithFieldSetter(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithFieldSetter(FieldSetterHandle left, Comparator c) {
		compareWithField(left, c);
	}
}

