/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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

import java.lang.invoke.VarHandle.AccessMode;

@VMCONSTANTPOOL_CLASS
abstract class VarHandleInvokeHandle extends PrimitiveHandle {
	@VMCONSTANTPOOL_FIELD
	final int operation;
	@VMCONSTANTPOOL_FIELD
	final MethodType accessModeType;

	VarHandleInvokeHandle(AccessMode accessMode, MethodType accessModeType, byte kind) {
		// Prepend a VarHandle receiver argument to match the how this MethodHandle will be invoked
		super(accessModeType.insertParameterTypes(0, VarHandle.class), null, null, kind, null);
		this.operation = accessMode.ordinal();
		// Append a VarHandle argument to match the VarHandle's internal method signature
		this.accessModeType = accessModeType.appendParameterTypes(VarHandle.class);
	}

	VarHandleInvokeHandle(VarHandleInvokeHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		operation = originalHandle.operation;
		accessModeType = originalHandle.accessModeType;
	}

	final void compareWithVarHandleInvoke(VarHandleInvokeHandle left, Comparator c) {
		c.compareStructuralParameter(left.operation, this.operation);
		c.compareStructuralParameter(left.accessModeType, this.accessModeType);
	}

	// {{{ JIT support
	@Override
	protected final ThunkTuple computeThunks(Object arg) {
		return thunkTable().get(new ThunkKey(ThunkKey.computeThunkableType(type(), 0, 1)));
	}
	// }}} JIT support
}

final class VarHandleInvokeExactHandle extends VarHandleInvokeHandle {
	VarHandleInvokeExactHandle(AccessMode accessMode, MethodType accessModeType) {
		super(accessMode, accessModeType, KIND_VARHANDLEINVOKEEXACT);
	}

	VarHandleInvokeExactHandle(VarHandleInvokeExactHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new VarHandleInvokeExactHandle(this, newType);
	}

	@Override
	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof VarHandleInvokeExactHandle) {
			((VarHandleInvokeExactHandle)right).compareWithVarHandleInvoke(this, c);
		} else {
			c.fail();
		}
	}

	// {{{ JIT support
	private static final ThunkTable _thunkTable = new ThunkTable();

	@Override
	protected final ThunkTable thunkTable() {
		return _thunkTable;
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(VarHandle varHandle, int argPlaceholder) {
		MethodHandle next = varHandle.getFromHandleTable(operation);
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}

		ILGenMacros.typeCheck(next, accessModeType);
		return ILGenMacros.invokeExact_X(next, ILGenMacros.placeholder(argPlaceholder, varHandle));
	}
	// }}} JIT support
}

final class VarHandleInvokeGenericHandle extends VarHandleInvokeHandle {
	VarHandleInvokeGenericHandle(AccessMode accessMode, MethodType accessModeType) {
		super(accessMode, accessModeType, KIND_VARHANDLEINVOKEGENERIC);
	}

	VarHandleInvokeGenericHandle(VarHandleInvokeGenericHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new VarHandleInvokeGenericHandle(this, newType);
	}

	@Override
	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof VarHandleInvokeGenericHandle) {
			((VarHandleInvokeGenericHandle)right).compareWithVarHandleInvoke(this, c);
		} else {
			c.fail();
		}
	}

	// {{{ JIT support
	private static final ThunkTable _thunkTable = new ThunkTable();

	@Override
	protected final ThunkTable thunkTable() {
		return _thunkTable;
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(VarHandle varHandle, int argPlaceholder) {
		MethodHandle next = varHandle.getFromHandleTable(operation);
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(next.asType(accessModeType), ILGenMacros.placeholder(argPlaceholder, varHandle));
	}
	// }}} JIT support
}
