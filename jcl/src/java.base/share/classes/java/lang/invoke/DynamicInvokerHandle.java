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

@VMCONSTANTPOOL_CLASS
class DynamicInvokerHandle extends MethodHandle {
	@VMCONSTANTPOOL_FIELD
	final CallSite site;
	
	DynamicInvokerHandle(CallSite site) {
		super(site.type(), MethodHandle.KIND_DYNAMICINVOKER, null); //$NON-NLS-1$
		this.site = site;
	}

	DynamicInvokerHandle(DynamicInvokerHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.site = originalHandle.site;
	}

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected ThunkTable thunkTable(){ return _thunkTable; }

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(site.getTarget());
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(site.getTarget(), argPlaceholder);
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new DynamicInvokerHandle(this, newType);
	}

	// Final because any pair of invokers are equivalent if the point at the same site
	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof DynamicInvokerHandle) {
			((DynamicInvokerHandle)right).compareWithDynamicInvoker(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithDynamicInvoker(DynamicInvokerHandle left, Comparator c) {
		c.compareStructuralParameter(left.site, this.site);
	}
}

