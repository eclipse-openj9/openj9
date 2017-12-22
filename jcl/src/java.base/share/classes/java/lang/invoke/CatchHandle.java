/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2011, 2011 IBM Corp. and others
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

final class CatchHandle extends PassThroughHandle {
	private final MethodHandle tryTarget;
	private final Class<? extends Throwable> exceptionClass;
	private final MethodHandle catchTarget;

	protected CatchHandle(MethodHandle tryTarget, Class<? extends Throwable> exceptionClass, MethodHandle catchTarget, MethodHandle equivalent) {
		super(equivalent, infoAffectingThunks(catchTarget.type().parameterCount())); //$NON-NLS-1$
		this.tryTarget = tryTarget;
		this.exceptionClass = exceptionClass;
		this.catchTarget = catchTarget;
	}

	CatchHandle(CatchHandle catchHandle, MethodType newType) {
		super(catchHandle, newType);
		this.tryTarget = catchHandle.tryTarget;
		this.exceptionClass = catchHandle.exceptionClass;
		this.catchTarget = catchHandle.catchTarget;
	}

	MethodHandle cloneWithNewType(MethodType newType) {
		return new CatchHandle(this, newType);
	}

	public static CatchHandle get(MethodHandle tryTarget, Class<? extends Throwable> exceptionClass, MethodHandle catchTarget, MethodHandle equivalent) { 
		return new CatchHandle(tryTarget, exceptionClass, catchTarget, equivalent);
	}

	// {{{ JIT support
	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	private static native int numCatchTargetArgsToPassThrough();

	private static Object infoAffectingThunks(int numCatchTargetArgs) {
		// The number of arguments passed to the catch target affects the code generated in the thunks
		return numCatchTargetArgs;
	}

	protected final ThunkTuple computeThunks(Object arg) {
		int numCatchTargetArgs = (Integer)arg;
		return thunkTable().get(new ThunkKeyWithInt(ThunkKey.computeThunkableType(type()), numCatchTargetArgs));
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(tryTarget, catchTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		try {
			return ILGenMacros.invokeExact_X(tryTarget, argPlaceholder);
		} catch (Throwable t) {
			if (exceptionClass.isInstance(t)) {
				return ILGenMacros.invokeExact_X(catchTarget, ILGenMacros.placeholder(t, ILGenMacros.firstN(numCatchTargetArgsToPassThrough(), argPlaceholder)));
			} else {
				throw t;
			}
		}
	}

	// }}} JIT support

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof CatchHandle) {
			((CatchHandle)right).compareWithCatch(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithCatch(CatchHandle left, Comparator c) {
		c.compareStructuralParameter(left.exceptionClass, this.exceptionClass);
		c.compareChildHandle(left.tryTarget, this.tryTarget);
		c.compareChildHandle(left.catchTarget, this.catchTarget);
	}
}

