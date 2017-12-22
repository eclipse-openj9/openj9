/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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

abstract class FoldHandle extends MethodHandle {
	protected final MethodHandle next;
	protected final MethodHandle combiner;
	@VMCONSTANTPOOL_FIELD
	private final int foldPosition;  /* The starting position of fold arguments */
	@VMCONSTANTPOOL_FIELD
	private final int[] argumentIndices;  /* An array of argument indexes of fold handle */
	
	protected FoldHandle(MethodHandle next, MethodHandle combiner, MethodType type, int foldPosition, int... argumentIndices) {
		super(type, KIND_FOLDHANDLE, infoAffectingThunks(combiner.type(), foldPosition, argumentIndices));
		this.next     = next;
		this.combiner = combiner;
		this.foldPosition = foldPosition;
		this.argumentIndices = argumentIndices;
	}

	FoldHandle(FoldHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.next = originalHandle.next;
		this.combiner = originalHandle.combiner;
		this.foldPosition = originalHandle.foldPosition;
		this.argumentIndices = originalHandle.argumentIndices;
	}

	public static FoldHandle get(MethodHandle next, MethodHandle combiner, MethodType type, int foldPosition, int... argumentIndices) {
		if (combiner.type().returnType() == void.class) {
			/* The foldPosition is used to determine the starting position of combiner,
			 * so it must be kept unchanged even though the combiner returns void.
			 */
			return new FoldVoidHandle(next, combiner, type, foldPosition, argumentIndices);
		} else {
			return new FoldNonvoidHandle(next, combiner, type, foldPosition, argumentIndices);
		}
	}
	
	private static Object[] infoAffectingThunks(MethodType combinerType, int foldPosition, int... argumentIndices) {
		// The number and types of values to fold affect jitted thunks
		Object[] result = { combinerType, foldPosition, argumentIndices};
		return result;
	}

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected ThunkTable thunkTable(){ return _thunkTable; }


	protected final ThunkTuple computeThunks(Object info) {
		return thunkTable().get(new ThunkKeyWithObjectArray(ThunkKey.computeThunkableType(type()), (Object[])info));
 	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof FoldHandle) {
			((FoldHandle)right).compareWithFold(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithFold(FoldHandle left, Comparator c) {
		c.compareChildHandle(left.next, this.next);
		c.compareChildHandle(left.combiner, this.combiner);
		c.compareStructuralParameter(left.foldPosition, this.foldPosition);
		/* The comparator does not address the case where two FoldHandles, 
		 * one with empty indices array and the other with non-empty indices array
		 * but the argument indices for them are the same, should share the same thunkArchetype.
		 * This is because that case will not happen due to the way we create the FoldHandle: 
		 * if the argument indices in the array are exactly the same to the argument indices 
		 * starting from the fold position, the FoldHandle will be created as if no array is passed in.
		 */
		c.compareStructuralParameter(left.argumentIndices, this.argumentIndices);
	}

	// {{{ JIT support
	protected static native int foldPosition();
	protected static native int argIndices();
	protected static native int argumentsForCombiner(int indice, int argPlaceholder);
	// }}} JIT support
}

final class FoldNonvoidHandle extends FoldHandle {

	FoldNonvoidHandle(MethodHandle next, MethodHandle combiner, MethodType type, int foldPosition, int... argumentIndices) {
		super(next, combiner, type, foldPosition, argumentIndices); //$NON-NLS-1$
	}

	// {{{ JIT support

	FoldNonvoidHandle(FoldNonvoidHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(combiner, next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(next, ILGenMacros.placeholder(
				ILGenMacros.firstN(foldPosition(), argPlaceholder),
				ILGenMacros.invokeExact(combiner, argumentsForCombiner(argIndices(), argPlaceholder)),
				ILGenMacros.dropFirstN(foldPosition(), argPlaceholder)));
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new FoldNonvoidHandle(this, newType);
	}
}

final class FoldVoidHandle extends FoldHandle {

	FoldVoidHandle(MethodHandle next, MethodHandle combiner, MethodType type, int foldPosition, int... argumentIndices) {
		super(next, combiner, type, foldPosition, argumentIndices); //$NON-NLS-1$
	}

	FoldVoidHandle(FoldVoidHandle foldVoidHandle, MethodType newType) {
		super(foldVoidHandle, newType);
	}

	// {{{ JIT support


	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(combiner, next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		ILGenMacros.invokeExact(combiner, argumentsForCombiner(argIndices(), argPlaceholder));
		return ILGenMacros.invokeExact_X(next, argPlaceholder);
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new FoldVoidHandle(this,  newType);
	}
}

