/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2009 IBM Corp. and others
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

import static java.lang.invoke.MethodType.*;

/*
 * MethodHandle subclass responsible for dealing with constant values.
 * The dispatch targets pop the MethodHandle and then push the constant value.
 */
abstract class ConstantHandle extends MethodHandle {

	ConstantHandle(MethodType type, byte kind) {
		super(type, kind, null);
	}

	ConstantHandle(ConstantHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	private static final ThunkTable _thunkTable = new ThunkTable();
	private static final ConstantHandle constantHandleInt_0 = new ConstantIntHandle(methodType(int.class), 0);
	private static final ConstantHandle constantHandleInt_1 = new ConstantIntHandle(methodType(int.class), 1);
	private static final ConstantHandle constantHandleDouble_0 = new ConstantDoubleHandle(methodType(double.class), 0.0);
	private static final ConstantHandle constantHandleDouble_1 = new ConstantDoubleHandle(methodType(double.class), 1.0);
	private static final ConstantHandle constantHandleLong_0 = new ConstantLongHandle(methodType(long.class), 0L);
	private static final ConstantHandle constantHandleLong_1 = new ConstantLongHandle(methodType(long.class), 1L);

	/*
	 * This is a single shared thunkTable across all ConstantHandle subclasses
	 * as each subclass only deals with a single signature.
	 */
	@Override
	protected final ThunkTable thunkTable() {
		return _thunkTable;
	}

	public static ConstantHandle get(Class<?> constantType, Object constantValue) {
		MethodType methodType = methodType(constantType);
		if (!constantType.isPrimitive()) {
			if (null != constantValue) {
				return new ConstantObjectHandle(methodType, constantValue);
			}
			MethodHandleCache methodHandleCache = MethodHandleCache.getCache(constantType);
			return (ConstantHandle)methodHandleCache.getNullConstantObjectHandle();
		}

		char charValue = 0;
		if (constantValue instanceof Character) {
			charValue = ((Character)constantValue).charValue();
		}

		if (constantType == double.class) {
			double value;
			if (constantValue instanceof Number) {
				value = ((Number)constantValue).doubleValue();
			} else {
				value = charValue;
			}
			if (0.0 == value) {
				return constantHandleDouble_0;
			} else if (1.0 == value) {
				return constantHandleDouble_1;
			}
			return new ConstantDoubleHandle(methodType, value);
		} else if (constantType == long.class) {
			long value;
			if (constantValue instanceof Number) {
				value = ((Number)constantValue).longValue();
			} else {
				value = charValue;
			}
			if (0L == value) {
				return constantHandleLong_0;
			} else if (1L == value) {
				return constantHandleLong_1;
			}
			return new ConstantLongHandle(methodType, value);
		} else if (constantType == float.class) {
			float value;
			if (constantValue instanceof Number) {
				value = ((Number)constantValue).floatValue();
			} else {
				value = charValue;
			}
			return new ConstantFloatHandle(methodType, value);
		} else {
			// int, short, byte, char, boolean
			int value;
			if (constantValue instanceof Number) {
				value = ((Number)constantValue).intValue();
			} else if (constantValue instanceof Boolean) {
				value = ((Boolean)constantValue).booleanValue() ? 1 : 0;
			} else {
				value = charValue;
			}
			if (int.class == constantType) {
				if (0 == value) {
					return constantHandleInt_0;
				} else if (1 == value) {
					return constantHandleInt_1;
				}
			}
			return new ConstantIntHandle(methodType, value);
		}
	}
}

final class ConstantObjectHandle extends ConstantHandle {

	final Object value;

	ConstantObjectHandle(MethodType type, Object value) {
		super(type, KIND_CONSTANTOBJECT); //$NON-NLS-1$
		this.value = value;
	}

	ConstantObjectHandle(ConstantObjectHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.value = originalHandle.value;
	}

	@FrameIteratorSkip
	private final Object invokeExact_thunkArchetype_L(int placeholder) {
		return value;
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new ConstantObjectHandle(this, newType);
	}

	@Override
	MethodHandle permuteArguments(MethodType permuteType, int... permute) {
		/* Do not use cloneWithNewType() as incoming signatures may be different */
		return new ConstantObjectHandle(permuteType, this.value);
	}

	@Override
	MethodHandle insertArguments(MethodHandle equivalent, MethodHandle unboxingHandle, int location, Object... values) {
		return new ConstantObjectHandle(equivalent.type, this.value);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof ConstantObjectHandle) {
			((ConstantObjectHandle)right).compareWithConstantObject(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithConstantObject(ConstantObjectHandle left, Comparator c) {
		c.compareUserSuppliedParameter(left.value, this.value);
	}
}

final class ConstantIntHandle extends ConstantHandle {

	final int value;

	ConstantIntHandle(MethodType type, int value) {
		super(type, KIND_CONSTANTINT); //$NON-NLS-1$
		this.value = value;
	}

	ConstantIntHandle(ConstantIntHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.value = originalHandle.value;
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_I(int placeholder) {
		return value;
	}

	@FrameIteratorSkip
	private final byte invokeExact_thunkArchetype_B(int placeholder) {
		return (byte)value;
	}

	@FrameIteratorSkip
	private final char invokeExact_thunkArchetype_C(int placeholder) {
		return (char)value;
	}

	@FrameIteratorSkip
	private final short invokeExact_thunkArchetype_S(int placeholder) {
		return (short)value;
	}

	@FrameIteratorSkip
	private final boolean invokeExact_thunkArchetype_Z(int placeholder) {
		return value != 0;
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new ConstantIntHandle(this, newType);
	}

	@Override
	MethodHandle permuteArguments(MethodType permuteType, int... permute) {
		/* Do not use cloneWithNewType() as incoming signatures may be different */
		return new ConstantIntHandle(permuteType, this.value);
	}

	@Override
	MethodHandle insertArguments(MethodHandle equivalent, MethodHandle unboxingHandle, int location, Object... values) {
		return new ConstantIntHandle(equivalent.type, this.value);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof ConstantIntHandle) {
			((ConstantIntHandle)right).compareWithConstantInt(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithConstantInt(ConstantIntHandle left, Comparator c) {
		c.compareStructuralParameter(left.value, this.value);
	}
}

final class ConstantFloatHandle extends ConstantHandle {

	final float value;

	ConstantFloatHandle(MethodType type, float value) {
		super(type, KIND_CONSTANTFLOAT); //$NON-NLS-1$
		this.value = value;
	}

	ConstantFloatHandle(ConstantFloatHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.value = originalHandle.value;
	}

	@FrameIteratorSkip
	private final float invokeExact_thunkArchetype_F(int placeholder) {
		return value;
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new ConstantFloatHandle(this, newType);
	}

	@Override
	MethodHandle permuteArguments(MethodType permuteType, int... permute) {
		/* Do not use cloneWithNewType() as incoming signatures may be different */
		return new ConstantFloatHandle(permuteType, this.value);
	}

	@Override
	MethodHandle insertArguments(MethodHandle equivalent, MethodHandle unboxingHandle, int location, Object... values) {
		return new ConstantFloatHandle(equivalent.type, this.value);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof ConstantFloatHandle) {
			((ConstantFloatHandle)right).compareWithConstantFloat(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithConstantFloat(ConstantFloatHandle left, Comparator c) {
		c.compareStructuralParameter(left.value, this.value);
	}
}

final class ConstantLongHandle extends ConstantHandle {

	final long value;

	ConstantLongHandle(MethodType type, long value) {
		super(type, KIND_CONSTANTLONG); //$NON-NLS-1$
		this.value = value;
	}

	ConstantLongHandle(ConstantLongHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.value = originalHandle.value;
	}

	@FrameIteratorSkip
	private final long invokeExact_thunkArchetype_J(int placeholder) {
		return value;
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new ConstantLongHandle(this, newType);
	}

	@Override
	MethodHandle permuteArguments(MethodType permuteType, int... permute) {
		/* Do not use cloneWithNewType() as incoming signatures may be different */
		return new ConstantLongHandle(permuteType, this.value);
	}

	@Override
	MethodHandle insertArguments(MethodHandle equivalent, MethodHandle unboxingHandle, int location, Object... values) {
		return new ConstantLongHandle(equivalent.type, this.value);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof ConstantLongHandle) {
			((ConstantLongHandle)right).compareWithConstantLong(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithConstantLong(ConstantLongHandle left, Comparator c) {
		c.compareStructuralParameter(left.value, this.value);
	}
}

final class ConstantDoubleHandle extends ConstantHandle {

	final double value;

	ConstantDoubleHandle(MethodType type, double value) {
		super(type, KIND_CONSTANTDOUBLE); //$NON-NLS-1$
		this.value = value;
	}

	ConstantDoubleHandle(ConstantDoubleHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.value = originalHandle.value;
	}

	@FrameIteratorSkip
	private final double invokeExact_thunkArchetype_D(int placeholder) {
		return value;
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new ConstantDoubleHandle(this, newType);
	}

	@Override
	MethodHandle permuteArguments(MethodType permuteType, int... permute) {
		/* Do not use cloneWithNewType() as incoming signatures may be different */
		return new ConstantDoubleHandle(permuteType, this.value);
	}

	@Override
	MethodHandle insertArguments(MethodHandle equivalent, MethodHandle unboxingHandle, int location, Object... values) {
		return new ConstantDoubleHandle(equivalent.type, this.value);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof ConstantDoubleHandle) {
			((ConstantDoubleHandle)right).compareWithConstantDouble(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithConstantDouble(ConstantDoubleHandle left, Comparator c) {
		c.compareStructuralParameter(left.value, this.value);
	}
}
