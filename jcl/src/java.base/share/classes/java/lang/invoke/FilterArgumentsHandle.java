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

final class FilterArgumentsHandle extends MethodHandle {
	private final MethodHandle   next;
	private final int            startPos;
	private final MethodHandle[] filters;

	protected FilterArgumentsHandle(MethodHandle next, int startPos, MethodHandle[] filters, MethodType newType) {
		super(newType, KIND_FILTERARGUMENTS, infoAffectingThunks(startPos, filters)); //$NON-NLS-1$
		this.next     = next;
		this.startPos = startPos;
		this.filters  = filters;
	}

	FilterArgumentsHandle(FilterArgumentsHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.next = originalHandle.next;
		this.startPos = originalHandle.startPos;
		this.filters = originalHandle.filters;
	}

	public static FilterArgumentsHandle get(MethodHandle next, int startPos, MethodHandle[] filters, MethodType newType) {
		return new FilterArgumentsHandle(next, startPos, filters, newType);
	}

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	private static native int numPrefixArgs();
	private static native int numSuffixArgs();
	private static native int numArgsToFilter();
	private static native int filterArguments(MethodHandle[] filters, int filteredArgumentPlaceholder);

	private static Object[] infoAffectingThunks(int startPos, MethodHandle[] filters) {
		// The position of the null filter and filter type will affect jitted code
		Object[] result = new Object[1 + filters.length];
		result[0] = Integer.valueOf(startPos);
		for (int i = 0; i < filters.length; i++) {
			if (filters[i] == null) {
				// Argument i will be passed through unfiltered. We represent
				// this by leaving a null in filterTypes.
			} else {
				result[i + 1] = ThunkKey.computeThunkableType(filters[i].type);
			}
		}
		return result;
	}

	protected final ThunkTuple computeThunks(Object arg) {
		return thunkTable().get(new ThunkKeyWithObjectArray(ThunkKey.computeThunkableType(type()), (Object[])arg));
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(next);
			undoCustomizationLogic(filters);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(next, ILGenMacros.placeholder(
			ILGenMacros.firstN(numPrefixArgs(), argPlaceholder),
			filterArguments(filters, ILGenMacros.middleN(numPrefixArgs(), numArgsToFilter(), argPlaceholder)),
			ILGenMacros.lastN(numSuffixArgs(), argPlaceholder)));
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new FilterArgumentsHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof FilterArgumentsHandle) {
			((FilterArgumentsHandle)right).compareWithFilterArguments(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithFilterArguments(FilterArgumentsHandle left, Comparator c) {
		c.compareStructuralParameter(left.startPos, this.startPos);
		c.compareStructuralParameter(left.filters.length, this.filters.length);
		c.compareChildHandle(left.next, this.next);
		for (int i = 0; (i < left.filters.length) && (i < this.filters.length); i++) {
			c.compareChildHandle(left.filters[i], this.filters[i]);
		}
	}
}

