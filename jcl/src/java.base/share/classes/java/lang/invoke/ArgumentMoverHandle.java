/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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

abstract class ArgumentMoverHandle extends PassThroughHandle {

	/** Base class for handles that can alter how arguments are passed to another handle.
	 *  The "permute" array indicates what arguments to pass to the next handle,
	 *  in what order.  If this handle takes N arguments, then integers from
	 *  0..N-1 indicate that the corresponding argument to this handle should be
	 *  passed on to the next handle.  Any other integer results in a call to
	 *  the "extra_" method of the right return type.
	 *
	 *  For example, suppose you have an ArgumentMoverHandle "amh" taking 3
	 *  arguments and having a permute array of [2,25,1,1,-4,0].  That would mean
	 *  that calling this:
	 *
	 *    amh.invokeExact(x,y,z);
	 *
	 *  is equivalent to this:
	 *
	 *    amh.next.invokeExact(z, amh.extra_L(25), y, y, amd.extra_L(-4), x);
	 *
	 *  ...assuming that all arguments are objects.  (If they're not, a
	 *  different extra_ method would be called.)
	 *
	 *  Subclasses can implement the extra_ method in any way they want.
	 */

	final MethodHandle next;
	final int[] permute;

	protected ArgumentMoverHandle(MethodType type, MethodHandle next, int[] permute, Object infoAffectingThunks, MethodHandle equivalent) {
		super(equivalent, infoAffectingThunks);
		this.next = next;
		this.permute = permute;
	}

	protected ArgumentMoverHandle(ArgumentMoverHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.next = originalHandle.next;
		this.permute = originalHandle.permute;
	}

	static int[] identityPermute(MethodType type) {
		int[] result = new int[type.parameterCount()];
		for (int i = 0; i < result.length; i++) {
			result[i] = i;
		}
		return result;
	}

	static int[] composePermute(int[] inner, int[] outer, int outerExtraIndexOffset) {
		int innerLength = inner.length;
		int[] result = new int[innerLength];
		for (int i = 0; i < innerLength; i++) {
			int index = inner[i];
			if ((0 <= index) && (index < outer.length)) {
				result[i] = outer[inner[i]];
			} else {
				result[i] = index + outerExtraIndexOffset;
			}
		}
		return result;
	}

	static int[] insertPermute(int[] originalPermute, int insertLocation, int numValues, int startingIndex) {
		// TODO: A variant that takes a MethodType instead of originalPermute so we don't have to bother with identityPermute
		int[] result = new int[originalPermute.length + numValues];
		for (int i = 0; i < insertLocation; i++) {
			result[i] = originalPermute[i];
		}
		for (int i = 0; i < numValues; i++) {
			result[insertLocation + i] = startingIndex - i;
		}
		for (int i = insertLocation; i < originalPermute.length; i++) {
			result[i + numValues] = originalPermute[i];
		}
		return result;
	}

	// {{{ JIT support

	protected ThunkTuple computeThunks(Object arg) {
		return thunkTable().get(new ThunkKeyWithObjectArray(ThunkKey.computeThunkableType(type()), (Object[])arg));
	}

	// This implements the argument permutation protocol.
	// Ints 0..N-1 refer to the incoming arguments to the thunk; all other
	// numbers are passed to the extra_X function returning the appropriate
	// type, and subclasses can decide what they mean.
	static native int permuteArgs(int argPlaceholder);

	// Thunks can only fabricate static calls, but we want virtual calls,
	// so here are a bunch of wrappers.
	private static boolean    extra_Z(ArgumentMoverHandle handle, int index){ return handle.extra_Z(index); }
	private static byte       extra_B(ArgumentMoverHandle handle, int index){ return handle.extra_B(index); }
	private static char       extra_C(ArgumentMoverHandle handle, int index){ return handle.extra_C(index); }
	private static short      extra_S(ArgumentMoverHandle handle, int index){ return handle.extra_S(index); }
	private static int        extra_I(ArgumentMoverHandle handle, int index){ return handle.extra_I(index); }
	private static long       extra_J(ArgumentMoverHandle handle, int index){ return handle.extra_J(index); }
	private static float      extra_F(ArgumentMoverHandle handle, int index){ return handle.extra_F(index); }
	private static double     extra_D(ArgumentMoverHandle handle, int index){ return handle.extra_D(index); }
	private static Object     extra_L(ArgumentMoverHandle handle, int index){ return handle.extra_L(index); }

	// Subclasses can implement whichever of these they need
	native boolean    extra_Z(int index);
	native byte       extra_B(int index);
	native char       extra_C(int index);
	native short      extra_S(int index);
	native int        extra_I(int index);
	native long       extra_J(int index);
	native float      extra_F(int index);
	native double     extra_D(int index);
	native Object     extra_L(int index);

	// }}} JIT support

	void compareWithArgumentMover(ArgumentMoverHandle left, Comparator c) {
		c.compareStructuralParameter(left.permute.length, this.permute.length);
		for (int i = 0; (i < left.permute.length) && (i < this.permute.length); i++) {
			c.compareStructuralParameter(left.permute[i], this.permute[i]);
		}
		c.compareChildHandle(left.next, this.next);
	}

}

