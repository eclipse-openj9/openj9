/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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

final class FinallyHandle extends PassThroughHandle {
	private final MethodHandle tryTarget;
	private final MethodHandle finallyTarget;

	protected FinallyHandle(MethodHandle tryTarget, MethodHandle finallyTarget, MethodHandle equivalent) {
		super(equivalent, infoAffectingThunks(finallyTarget.type().parameterCount())); //$NON-NLS-1$
		this.tryTarget = tryTarget;
		this.finallyTarget = finallyTarget;
	}

	FinallyHandle(FinallyHandle finallyHandle, MethodType newType) {
		super(finallyHandle, newType);
		this.tryTarget = finallyHandle.tryTarget;
		this.finallyTarget = finallyHandle.finallyTarget;
	}

	public static FinallyHandle get(MethodHandle tryTarget, MethodHandle finallyTarget, MethodHandle equivalent) { 
		return new FinallyHandle(tryTarget, finallyTarget, equivalent);
	}

	// {{{ JIT support
	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	private static native int numFinallyTargetArgsToPassThrough();

	private static Object infoAffectingThunks(int numFinallyTargetArgs) {
		// The number of arguments passed to the finally target affects the code generated in the thunks
		return numFinallyTargetArgs;
	}

	protected final ThunkTuple computeThunks(Object arg) {
		int numFinallyTargetArgs = (Integer)arg;
		return thunkTable().get(new ThunkKeyWithInt(ThunkKey.computeThunkableType(type()), numFinallyTargetArgs));
	}
	// }}} JIT support

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof FinallyHandle) {
			((FinallyHandle)right).compareWithFinally(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithFinally(FinallyHandle left, Comparator c) {
		c.compareChildHandle(left.tryTarget, this.tryTarget);
		c.compareChildHandle(left.finallyTarget, this.finallyTarget);
	}
	
	MethodHandle cloneWithNewType(MethodType newType) {
		return new FinallyHandle(this, newType);
	}
	
	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, finallyTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		
		Throwable finallyThrowable = null;
		try {
			ILGenMacros.invokeExact_V(tryTarget, argPlaceholder);
		} catch (Throwable tryThrowable) {
			finallyThrowable = tryThrowable;
			throw tryThrowable;
		} finally {
			ILGenMacros.invokeExact_V(finallyTarget, 
									ILGenMacros.placeholder(finallyThrowable, 
									ILGenMacros.firstN(numFinallyTargetArgsToPassThrough(), 
									argPlaceholder)));
		}
	}

	@FrameIteratorSkip
	@SuppressWarnings("finally")
	private final boolean invokeExact_thunkArchetype_Z(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, finallyTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		
		Throwable finallyThrowable = null;
		boolean result = false;
		try {
			result = ILGenMacros.invokeExact_Z(tryTarget, argPlaceholder);
		} catch (Throwable tryThrowable) {
			finallyThrowable = tryThrowable;
			throw tryThrowable;
		} finally {
			return ILGenMacros.invokeExact_Z(finallyTarget, 
											ILGenMacros.placeholder(finallyThrowable, result, 
											ILGenMacros.firstN(numFinallyTargetArgsToPassThrough(), 
											argPlaceholder)));
		}
	}

	@FrameIteratorSkip
	@SuppressWarnings("finally")
	private final byte invokeExact_thunkArchetype_B(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, finallyTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		
		Throwable finallyThrowable = null;
		byte result = 0x0;
		try {
			result = ILGenMacros.invokeExact_B(tryTarget, argPlaceholder);
		} catch (Throwable tryThrowable) {
			finallyThrowable = tryThrowable;
			throw tryThrowable;
		} finally {
			return ILGenMacros.invokeExact_B(finallyTarget, 
											ILGenMacros.placeholder(finallyThrowable, result, 
											ILGenMacros.firstN(numFinallyTargetArgsToPassThrough(), 
											argPlaceholder)));
		}
	}

	@FrameIteratorSkip
	@SuppressWarnings("finally")
	private final short invokeExact_thunkArchetype_S(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, finallyTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		
		Throwable finallyThrowable = null;
		short result = 0;
		try {
			result = ILGenMacros.invokeExact_S(tryTarget, argPlaceholder);
		} catch (Throwable tryThrowable) {
			finallyThrowable = tryThrowable;
			throw tryThrowable;
		} finally {
			return ILGenMacros.invokeExact_S(finallyTarget, 
											ILGenMacros.placeholder(finallyThrowable, result, 
											ILGenMacros.firstN(numFinallyTargetArgsToPassThrough(), 
											argPlaceholder)));
		}
	}

	@FrameIteratorSkip
	@SuppressWarnings("finally")
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, finallyTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		
		Throwable finallyThrowable = null;
		int result = 0;
		try {
			result = ILGenMacros.invokeExact_X(tryTarget, argPlaceholder);
		} catch (Throwable tryThrowable) {
			finallyThrowable = tryThrowable;
			throw tryThrowable;
		} finally {
			return ILGenMacros.invokeExact_X(finallyTarget, 
											ILGenMacros.placeholder(finallyThrowable, result, 
											ILGenMacros.firstN(numFinallyTargetArgsToPassThrough(), 
											argPlaceholder)));
		}
	}

	@FrameIteratorSkip
	@SuppressWarnings("finally")
	private final long invokeExact_thunkArchetype_J(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, finallyTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		
		Throwable finallyThrowable = null;
		long result = 0L;
		try {
			result = ILGenMacros.invokeExact_J(tryTarget, argPlaceholder);
		} catch (Throwable tryThrowable) {
			finallyThrowable = tryThrowable;
			throw tryThrowable;
		} finally {
			return ILGenMacros.invokeExact_J(finallyTarget, 
											ILGenMacros.placeholder(finallyThrowable, result, 
											ILGenMacros.firstN(numFinallyTargetArgsToPassThrough(), 
											argPlaceholder)));
		}
	}
	
	@FrameIteratorSkip
	@SuppressWarnings("finally")
	private final float invokeExact_thunkArchetype_F(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, finallyTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		
		Throwable finallyThrowable = null;
		float result = 0.0F;
		try {
			result = ILGenMacros.invokeExact_F(tryTarget, argPlaceholder);
		} catch (Throwable tryThrowable) {
			finallyThrowable = tryThrowable;
			throw tryThrowable;
		} finally {
			return ILGenMacros.invokeExact_F(finallyTarget, 
											ILGenMacros.placeholder(finallyThrowable, result, 
											ILGenMacros.firstN(numFinallyTargetArgsToPassThrough(), 
											argPlaceholder)));
		}
	}
	
	@FrameIteratorSkip
	@SuppressWarnings("finally")
	private final double invokeExact_thunkArchetype_D(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, finallyTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		
		Throwable finallyThrowable = null;
		double result = 0.0D;
		try {
			result = ILGenMacros.invokeExact_D(tryTarget, argPlaceholder);
		} catch (Throwable tryThrowable) {
			finallyThrowable = tryThrowable;
			throw tryThrowable;
		} finally {
			return ILGenMacros.invokeExact_D(finallyTarget, 
											ILGenMacros.placeholder(finallyThrowable, result, 
											ILGenMacros.firstN(numFinallyTargetArgsToPassThrough(), 
											argPlaceholder)));
		}
	}
	
	@FrameIteratorSkip
	@SuppressWarnings("finally")
	private final Object invokeExact_thunkArchetype_L(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, finallyTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		
		Throwable finallyThrowable = null;
		Object result = null;
		try {
			result = ILGenMacros.invokeExact_L(tryTarget, argPlaceholder);
		} catch (Throwable tryThrowable) {
			finallyThrowable = tryThrowable;
			throw tryThrowable;
		} finally {
			return ILGenMacros.invokeExact_L(finallyTarget, 
											ILGenMacros.placeholder(finallyThrowable, result, 
											ILGenMacros.firstN(numFinallyTargetArgsToPassThrough(), 
											argPlaceholder)));
		}
	}
}
