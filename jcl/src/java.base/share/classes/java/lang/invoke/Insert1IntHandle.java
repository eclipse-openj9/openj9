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

final class Insert1IntHandle extends InsertHandle {
	final int value;
	final MethodHandle nextNoUnbox;  // Next handle without the astype from Object for the inserted value

	Insert1IntHandle(MethodType type, MethodHandle next, int insertionIndex, Object values[], MethodHandle nextNoUnbox) {
		super(type, next, insertionIndex, values);
		/* Value must unbox and widen to int.  Only byte, short, char, or int are possible */
		if (values[0] instanceof Character) {
			this.value = ((Character)values[0]).charValue();
		} else {
			this.value = ((Number)values[0]).intValue();	
		}
		this.nextNoUnbox = nextNoUnbox;
	}

	Insert1IntHandle(Insert1IntHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.value = originalHandle.value;
		this.nextNoUnbox = originalHandle.nextNoUnbox;
	}

	
	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new Insert1IntHandle(this, newType);
	}
	
	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(nextNoUnbox);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(nextNoUnbox, ILGenMacros.placeholder(
			ILGenMacros.firstN(numPrefixArgs(), argPlaceholder),
			value,
			ILGenMacros.lastN(numSuffixArgs(), argPlaceholder)));
	}
 
	// }}} JIT support
}

