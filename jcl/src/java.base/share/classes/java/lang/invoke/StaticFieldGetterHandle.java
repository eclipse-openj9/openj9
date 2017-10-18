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
 * MethodHandle subclass that is able to return the value of
 * a static field.
 * <p>
 * vmSlot will hold the Unsafe field offset + low tag.
 * 
 */
final class StaticFieldGetterHandle extends FieldHandle {
	
	StaticFieldGetterHandle(Class<?> referenceClass, String fieldName, Class<?> fieldClass, Class<?> accessClass) throws IllegalAccessException, NoSuchFieldException {
		super(fieldMethodType(fieldClass), referenceClass, fieldName, fieldClass, KIND_GETSTATICFIELD, accessClass);
	}
	
	StaticFieldGetterHandle(Field field) throws IllegalAccessException {
		super(fieldMethodType(field.getType()), field, KIND_GETSTATICFIELD, true);
	}
	
	StaticFieldGetterHandle(StaticFieldGetterHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	/* Create the MethodType to be passed to the constructor */
	private final static MethodType fieldMethodType(Class<?> fieldClass) {
		return MethodType.methodType(fieldClass);
	}

	// {{{ JIT support
	@FrameIteratorSkip
	private final int    invokeExact_thunkArchetype_I(int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getIntVolatile(defc, vmSlot);
		else
			return UNSAFE.getInt        (defc, vmSlot);
	}

	@FrameIteratorSkip
	private final long   invokeExact_thunkArchetype_J(int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getLongVolatile(defc, vmSlot);
		else
			return UNSAFE.getLong        (defc, vmSlot);
	}

	@FrameIteratorSkip
	private final float  invokeExact_thunkArchetype_F(int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getFloatVolatile(defc, vmSlot);
		else
			return UNSAFE.getFloat        (defc, vmSlot);
	}

	@FrameIteratorSkip
	private final double invokeExact_thunkArchetype_D(int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getDoubleVolatile(defc, vmSlot);
		else
			return UNSAFE.getDouble        (defc, vmSlot);
	}

	@FrameIteratorSkip
	private final Object invokeExact_thunkArchetype_L(int argPlaceholder) {
		initializeClassIfRequired();
		if (Modifier.isVolatile(final_modifiers))
			return UNSAFE.getObjectVolatile(defc, vmSlot);
		else
			return UNSAFE.getObject        (defc, vmSlot);
	}

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }
	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new StaticFieldGetterHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof StaticFieldGetterHandle) {
			((StaticFieldGetterHandle)right).compareWithStaticFieldGetter(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithStaticFieldGetter(StaticFieldGetterHandle left, Comparator c) {
		compareWithField(left, c);
	}
}

