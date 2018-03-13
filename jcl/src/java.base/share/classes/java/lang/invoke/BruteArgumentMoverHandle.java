/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

final class BruteArgumentMoverHandle extends ArgumentMoverHandle {

	/** An ArgumentMoverHandle that can hold additional values to pass to the next handle.
	 *  The "extra" array contains the values to pass along.  In addition, for
	 *  performance reasons, the first few arguments are also redundantly stored
	 *  in fields of the BruteArgumentMoverHandle object, which allows us to
	 *  avoid unboxing primitives, and also reduces the number of levels of
	 *  indirection needed to access objects.
	 *
	 *  The extra_ methods take an integer i from -1 to -N, where N is the
	 *  length of the extra array.  They return extra[-1-i], or a faster equivalent.
	 *
	 *  This functionality is enough to compose any number and sequence of
	 *  insertArguments, permuteArguments, and dropArguments operations, as well
	 *  as asType operations that do only boxing / unboxing and integer widening.
	 *  This is the main "value add" of ArgumentMoverHandle: it collapses whole
	 *  chains of these handles into a single handle, most of whose functionality
	 *  is performed at creation time instead of invocation time.
	 *
	 *  (Generally, other asType operations need AsType handles because they are
	 *  not composable.  For example, casting an argument to class A and then to
	 *  B actually does necessitate two checkcasts if A and B are unrelated types.)
	 */

	final Object[] extra;

	// Save a level of indirection for the first few objects.
	Object   extra_L0;
	Object   extra_L1;
	Object   extra_L2;
	Object   extra_L3;
	Object   extra_L4;

	// Save a couple of levels of indirection for inserted ints.
	// Also, mark as non-final so we leave the loads in the residual code.
	// They can be treated as honourary finals in full-custom thunks.
	int extra_I0;
	int extra_I1;
	int extra_I2;
	int extra_I3;
	int extra_I4;

	// Longs too.
	long extra_J0;
	long extra_J1;
	long extra_J2;
	long extra_J3;
	long extra_J4;

	protected BruteArgumentMoverHandle(MethodType type, MethodHandle next, int[] permute, Object[] extra, MethodHandle equivalent) {
		super(type, next, permute, infoAffectingThunks(next, permute, extra), equivalent);
		this.extra = extra;
		// This code is a bit contorted just to satisfy javac
		if (extra.length >= 1) {
			extra_L0 = extra[0];
			if (extra_L0 instanceof Integer) {
				extra_I0 = (Integer)extra_L0;
			} else if (extra_L0 instanceof Long) {
				extra_J0 = (Long)extra_L0;
			}
		} else {
			extra_L0 = null;
		}
		if (extra.length >= 2) {
			extra_L1 = extra[1];
			if (extra_L1 instanceof Integer) {
				extra_I1 = (Integer)extra_L1;
			} else if (extra_L1 instanceof Long) {
				extra_J1 = (Long)extra_L1;
			}
		} else {
			extra_L1 = null;
		}
		if (extra.length >= 3) {
			extra_L2 = extra[2];
			if (extra_L2 instanceof Integer) {
				extra_I2 = (Integer)extra_L2;
			} else if (extra_L2 instanceof Long) {
				extra_J2 = (Long)extra_L2;
			}
		} else {
			extra_L2 = null;
		}
		if (extra.length >= 4) {
			extra_L3 = extra[3];
			if (extra_L3 instanceof Integer) {
				extra_I3 = (Integer)extra_L3;
			} else if (extra_L3 instanceof Long) {
				extra_J3 = (Long)extra_L3;
			}
		} else {
			extra_L3 = null;
		}
		if (extra.length >= 5) {
			extra_L4 = extra[4];
			if (extra_L4 instanceof Integer) {
				extra_I4 = (Integer)extra_L4;
			} else if (extra_L4 instanceof Long) {
				extra_J4 = (Long)extra_L4;
			}
		} else {
			extra_L4 = null;
		}
	}

	protected BruteArgumentMoverHandle(BruteArgumentMoverHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		extra = originalHandle.extra;
		extra_L0 = originalHandle.extra_L0;
		extra_L1 = originalHandle.extra_L1;
		extra_L2 = originalHandle.extra_L2;
		extra_L3 = originalHandle.extra_L3;
		extra_L4 = originalHandle.extra_L4;
		extra_I0 = originalHandle.extra_I0;
		extra_I1 = originalHandle.extra_I1;
		extra_I2 = originalHandle.extra_I2;
		extra_I3 = originalHandle.extra_I3;
		extra_I4 = originalHandle.extra_I4;
		extra_J0 = originalHandle.extra_J0;
		extra_J1 = originalHandle.extra_J1;
		extra_J2 = originalHandle.extra_J2;
		extra_J3 = originalHandle.extra_J3;
		extra_J4 = originalHandle.extra_J4;
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new BruteArgumentMoverHandle(this, newType);
	}

	final MethodHandle permuteArguments(MethodType permuteType, int... outerPermute) throws NullPointerException, IllegalArgumentException {
		if (true) {
			return new BruteArgumentMoverHandle(
				permuteType,
				next,
				composePermute(this.permute, outerPermute, 0),
				extra,
				new PermuteHandle(permuteType, this.equivalent, outerPermute)
				);
		} else {
			return super.permuteArguments(permuteType, outerPermute);
		}
	}

	final MethodHandle insertArguments(MethodHandle equivalent, MethodHandle unboxingHandle, int location, Object... outerValues) {
		MethodHandle result;
		if (true) {
			int[] insertPermute = ArgumentMoverHandle.insertPermute(ArgumentMoverHandle.identityPermute(equivalent.type), location, outerValues.length, -1);
			int[] combinedPermute = composePermute(this.permute, insertPermute, -outerValues.length);
			Object[] combinedExtra;
			if (this.extra.length >= 1) {
				combinedExtra = java.util.Arrays.copyOf(outerValues, outerValues.length + this.extra.length);
				System.arraycopy(this.extra, 0, combinedExtra, outerValues.length, this.extra.length);
			} else {
				combinedExtra = outerValues;
			}
			result = new BruteArgumentMoverHandle(
				equivalent.type(),
				next,
				combinedPermute,
				combinedExtra,
				equivalent
				);
		} else {
			result = super.insertArguments(equivalent, unboxingHandle, location, outerValues);
		}
		return result;
	}

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected ThunkTable thunkTable(){ return _thunkTable; }

	final Object extra_L(int index) {
		if (index == -1) {
			return extra_L0;
		} else if (index == -2) {
			return extra_L1;
		} else if (index == -3) {
			return extra_L2;
		} else if (index == -4) {
			return extra_L3;
		} else if (index == -5) {
			return extra_L4;
		} else {
			return extra[-1 - index];
		}
	}

	final int extra_I(int index) {
		if (index == -1) {
			return extra_I0;
		} else if (index == -2) {
			return extra_I1;
		} else if (index == -3) {
			return extra_I2;
		} else if (index == -4) {
			return extra_I3;
		} else if (index == -5) {
			return extra_I4;
		} else {
			return (Integer)extra[-1 - index];
		}
	}

	final long extra_J(int index) {
		if (index == -1) {
			return extra_J0;
		} else if (index == -2) {
			return extra_J1;
		} else if (index == -3) {
			return extra_J2;
		} else if (index == -4) {
			return extra_J3;
		} else if (index == -5) {
			return extra_J4;
		} else {
			return (Long)extra[-1 - index];
		}
	}

	// Unbox if needed.  These allow us to skip an AsTypeHandle just for unboxing,
	// though it does impose a checkcast unless the jit can eliminate it.
	//
	final boolean extra_Z(int index) { 
		return (Boolean)extra_L(index); 
	}
	final byte extra_B(int index) {
		return (Byte)extra_L(index); }
	final short extra_S(int index) {
		return (Short)extra_L(index); }
	final char extra_C(int index) {
		return (Character)extra_L(index); 
	}
	final float extra_F(int index) { 
		return (Float)extra_L(index); 
	}
	final double extra_D(int index) { 
		return (Double)extra_L(index); 
	}

	private static Object[] infoAffectingThunks(MethodHandle next, int[] permute, Object[] extra) {
		// The location and number of values to insert affects the code generated in shareable thunks,
		// as does the thunkableType of the next handle.
		// The actual inserted values don't affect shareable thunks.
		Object[] result = {ThunkKey.computeThunkableType(next.type()), permute};
		return result;
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(
			next,
			permuteArgs(argPlaceholder));
	}

	// }}} JIT support
 	final void compareWith(MethodHandle right, Comparator c) {
 		if (right instanceof BruteArgumentMoverHandle) {
 			((BruteArgumentMoverHandle)right).compareWithBruteArgumentMover(this, c);
 		} else {
 			c.fail();
 		}
 	}
 
 	final void compareWithArgumentMover(ArgumentMoverHandle left, Comparator c) {
 		// If left were an BruteArgumentMoverHandle, we'd be in
 		// compareWithBruteArgumentMover, so it doesn't match.
 		c.fail();
 	}

 	final void compareWithBruteArgumentMover(BruteArgumentMoverHandle left, Comparator c) {
 		c.compareStructuralParameter(left.extra.length, this.extra.length);
 		for (int i = 0; (i < left.extra.length) && (i < this.extra.length); i++) {
 			c.compareUserSuppliedParameter(left.extra[i], this.extra[i]);
 		}
 		super.compareWithArgumentMover(left, c);
 	}

}