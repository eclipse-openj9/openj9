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

import java.lang.invoke.MethodHandle.FrameIteratorSkip;

/* ExplicitCastHandle is a MethodHandle subclass used to implement
 * MethodHandles.explicitCastArgument()
 * 
 */
final class ExplicitCastHandle extends ArgumentConversionHandle {

	ExplicitCastHandle(MethodHandle handle, MethodType type) {
		super(handle, type, KIND_EXPLICITCAST);
	}

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	protected static native int convertArgs(int argPlaceholder);

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(next, convertArgs(argPlaceholder));
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new ExplicitCastHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof ExplicitCastHandle) {
			((ExplicitCastHandle)right).compareWithExplicitCast(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithExplicitCast(ExplicitCastHandle left, Comparator c) {
		compareWithConvert(left, c);
	}
}

