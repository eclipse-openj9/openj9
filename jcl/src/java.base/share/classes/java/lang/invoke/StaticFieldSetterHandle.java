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
 * MethodHandle subclass that is able to set the value of
 * a static field.
 * <p>
 * vmSlot will hold the Unsafe field offset  + low tag.
 * 
 */
final class StaticFieldSetterHandle extends FieldHandle {
	
	StaticFieldSetterHandle(Class<?> referenceClass, String fieldName, Class<?> fieldClass, Class<?> accessClass) throws IllegalAccessException, NoSuchFieldException {
		super(fieldMethodType(fieldClass), referenceClass, fieldName, fieldClass, KIND_PUTSTATICFIELD, accessClass);
	}
	
	StaticFieldSetterHandle(Field field) throws IllegalAccessException {
		super(fieldMethodType(field.getType()), field, KIND_PUTSTATICFIELD, true);
	}

	StaticFieldSetterHandle(StaticFieldSetterHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	/* Create the MethodType to be passed to the constructor */
	private final static MethodType fieldMethodType(Class<?> fieldClass) {
		return MethodType.methodType(void.class, fieldClass);
	}

	// {{{ JIT support
	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(int    newValue, int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putIntVolatile(defc, vmSlot, newValue);
		else
			UNSAFE.putInt        (defc, vmSlot, newValue);
	}

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(long   newValue, int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putLongVolatile(defc, vmSlot, newValue);
		else
			UNSAFE.putLong        (defc, vmSlot, newValue);
	}

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(float  newValue, int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putFloatVolatile(defc, vmSlot, newValue);
		else
			UNSAFE.putFloat        (defc, vmSlot, newValue);
	}

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(double newValue, int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putDoubleVolatile(defc, vmSlot, newValue);
		else
			UNSAFE.putDouble        (defc, vmSlot, newValue);
	}

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(Object newValue, int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			UNSAFE.putObjectVolatile(defc, vmSlot, newValue);
		else
			UNSAFE.putObject        (defc, vmSlot, newValue);
	}

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }
	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new StaticFieldSetterHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof StaticFieldSetterHandle) {
			((StaticFieldSetterHandle)right).compareWithStaticFieldSetter(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithStaticFieldSetter(StaticFieldSetterHandle left, Comparator c) {
		compareWithField(left, c);
	}
}

