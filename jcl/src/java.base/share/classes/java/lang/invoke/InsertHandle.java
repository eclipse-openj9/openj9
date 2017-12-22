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

class InsertHandle extends MethodHandle {
	final MethodHandle  next;
	final int           insertionIndex;
	private final Object[]      values;

	/* 
	 * next must be an appropriately typed handle to ensure that the bound parameters are typechecked
	 * correctly.
	 */
	InsertHandle(MethodType type, MethodHandle next, int insertionIndex, Object values[]) {
		super(type, KIND_INSERT,infoAffectingThunks(insertionIndex, values.length)); //$NON-NLS-1$
		this.next			= next;
		this.insertionIndex = insertionIndex;
		this.values         = values;
	}

	InsertHandle(InsertHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.next 			= originalHandle.next;
		this.insertionIndex = originalHandle.insertionIndex;
		this.values 		= originalHandle.values;
	}

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected ThunkTable thunkTable(){ return _thunkTable; }

	private static int[] infoAffectingThunks(int insertionIndex, int numberOfValues) {
		// The location and number of values to insert affects the code generated in the thunks; see ILGen macros below
		int[] result = { insertionIndex, numberOfValues };
		return result;
	}

	protected ThunkTuple computeThunks(Object arg) {
		int[] info = (int[])arg;
		return thunkTable().get(new ThunkKeyWithIntArray(ThunkKey.computeThunkableType(type()), info));
	}

	static native int numPrefixArgs();
	static native int numSuffixArgs();
	private static native int numValuesToInsert();

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
			ILGenMacros.arrayElements(values, 0, numValuesToInsert()),
			ILGenMacros.lastN(numSuffixArgs(), argPlaceholder)));
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new InsertHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof InsertHandle) {
			((InsertHandle)right).compareWithInsert(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithInsert(InsertHandle left, Comparator c) {
		c.compareStructuralParameter(left.insertionIndex, this.insertionIndex);
		c.compareStructuralParameter(left.values.length, this.values.length);
		for (int i = 0; (i < left.values.length) && (i < this.values.length); i++) {
			c.compareUserSuppliedParameter(left.values[i], this.values[i]);
		}
		c.compareChildHandle(left.next, this.next);
	}
}

