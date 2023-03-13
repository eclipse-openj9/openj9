/*[INCLUDE-IF Sidecar17 & !OPENJDK_METHODHANDLES]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2011
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package java.lang.invoke;

import java.lang.invoke.MethodHandle.FrameIteratorSkip;
/*[IF JAVA_SPEC_VERSION >= 15]*/
import java.util.List;
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */

@VMCONSTANTPOOL_CLASS
final class FilterReturnHandle extends ConvertHandle {
	@VMCONSTANTPOOL_FIELD
	final MethodHandle filter;
	
	FilterReturnHandle(MethodHandle next, MethodHandle filter) {
		super(next, next.type.changeReturnType(filter.type.returnType()), KIND_FILTERRETURN, filter.type()); //$NON-NLS-1$
		this.filter = filter;
	}

	FilterReturnHandle(FilterReturnHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.filter = originalHandle.filter;
	}

	/*[IF ]*/
	protected void dumpDetailsTo(java.io.PrintStream out, String prefix, java.util.Set<MethodHandle> alreadyDumped) {
		super.dumpDetailsTo(out, prefix, alreadyDumped);
		filter.dumpTo(out, prefix, "filter: ", alreadyDumped); //$NON-NLS-1$
	}
	/*[ENDIF]*/

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	protected final ThunkTuple computeThunks(Object filterHandleType) {
		// We include the full type of filter in order to get the type cast right.
		return thunkTable().get(new ThunkKeyWithObject(ThunkKey.computeThunkableType(type()), filterHandleType));
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(filter, next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(filter, ILGenMacros.invokeExact(next, argPlaceholder));
	}

/*[IF JAVA_SPEC_VERSION >= 15]*/
	@Override
	boolean addRelatedMHs(List<MethodHandle> relatedMHs) {
		relatedMHs.add(next);
		relatedMHs.add(filter);
		return true;
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new FilterReturnHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof FilterReturnHandle) {
			((FilterReturnHandle)right).compareWithFilterReturn(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithFilterReturn(FilterReturnHandle left, Comparator c) {
		compareWithConvert(left, c);
		c.compareChildHandle(left.filter, this.filter);
	}
}
