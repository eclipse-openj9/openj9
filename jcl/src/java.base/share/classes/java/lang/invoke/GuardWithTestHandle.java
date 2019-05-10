/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2011, 2019 IBM Corp. and others
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

final class GuardWithTestHandle extends MethodHandle {

	final MethodHandle guard;
	final MethodHandle trueTarget;
	final MethodHandle falseTarget;

	protected GuardWithTestHandle(MethodHandle guard, MethodHandle trueTarget, MethodHandle falseTarget) {
		super(trueTarget.type(), KIND_GUARDWITHTEST, guard.type());  //$NON-NLS-1$
		this.guard       = guard;
		this.trueTarget  = trueTarget;
		this.falseTarget = falseTarget;
	}

	GuardWithTestHandle(GuardWithTestHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.guard = originalHandle.guard;
		this.trueTarget = originalHandle.trueTarget;
		this.falseTarget = originalHandle.falseTarget;
	}

	public static GuardWithTestHandle get(MethodHandle guard, MethodHandle trueTarget, MethodHandle falseTarget) {
		return new GuardWithTestHandle(guard, trueTarget, falseTarget);
	}

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

 	protected final ThunkTuple computeThunks(Object guardType) {
 		// Different thunks accommodate guards with different numbers of parameters
 		return thunkTable().get(new ThunkKeyWithObject(ThunkKey.computeThunkableType(type()), ThunkKey.computeThunkableType((MethodType)guardType)));
 	}
 
 	private static native int numGuardArgs();
 
	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(guard, trueTarget, falseTarget);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		if (ILGenMacros.invokeExact_Z(guard, ILGenMacros.firstN(numGuardArgs(), argPlaceholder))) {
			return ILGenMacros.invokeExact_X(trueTarget, argPlaceholder);
		} else {
			return ILGenMacros.invokeExact_X(falseTarget, argPlaceholder);
		}
	}
 
	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new GuardWithTestHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof GuardWithTestHandle) {
			((GuardWithTestHandle)right).compareWithGuardWithTest(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithGuardWithTest(GuardWithTestHandle left, Comparator c) {
		c.compareChildHandle(left.guard, this.guard);
		c.compareChildHandle(left.trueTarget, this.trueTarget);
		c.compareChildHandle(left.falseTarget, this.falseTarget);
	}
}

