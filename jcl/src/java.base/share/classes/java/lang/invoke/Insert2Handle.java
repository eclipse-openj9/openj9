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

final class Insert2Handle extends InsertHandle {
	final Object value_1;
	final Object value_2;
	
	Insert2Handle(MethodType type, MethodHandle next, int insertionIndex, Object values[]) {
		super(type, next, insertionIndex, values);
		this.value_1 = values[0];
		this.value_2 = values[1];
	}
	
	Insert2Handle(Insert2Handle originalHandle, MethodType nextType) {
		super(originalHandle, nextType);
		this.value_1 = originalHandle.value_1;
		this.value_2 = originalHandle.value_2;
	}
	
	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new Insert2Handle(this, newType);
	}

	// {{{ JIT support
	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	protected final ThunkTuple computeThunks(Object arg) {
		int[] info = (int[])arg;
		return thunkTable().get(new ThunkKeyWithIntArray(ThunkKey.computeThunkableType(type()), info));
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(next, ILGenMacros.placeholder(
				ILGenMacros.firstN(numPrefixArgs(), argPlaceholder),
				value_1, 
				value_2,
				ILGenMacros.lastN(numSuffixArgs(), argPlaceholder)));
	}

	// }}} JIT support
}

