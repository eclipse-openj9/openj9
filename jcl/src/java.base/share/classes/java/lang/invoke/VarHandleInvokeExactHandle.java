/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

import java.lang.invoke.VarHandle.AccessMode;

/**
 * Implementation for the invokeExact varhandle combinator.
 */
final class VarHandleInvokeExactHandle extends VarHandleInvokeHandle {
	VarHandleInvokeExactHandle(AccessMode accessMode, MethodType accessModeType) {
		super(accessMode, accessModeType, KIND_VARHANDLEINVOKEEXACT);
	}

	VarHandleInvokeExactHandle(VarHandleInvokeExactHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}
	
	@Override
	boolean canRevealDirect() {
		/* This is invokevirtual of VarHandle.invokeExact() */
		return true;
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
