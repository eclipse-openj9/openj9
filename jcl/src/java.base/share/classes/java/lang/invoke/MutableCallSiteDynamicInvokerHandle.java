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
final class MutableCallSiteDynamicInvokerHandle extends DynamicInvokerHandle {
	/* mutableSite and the parent's site fields will always be sync.  This is 
	 * a redefinition as a MCS to enable the thunkArchetype to inline without
	 * a guard
	 */
	final MutableCallSite mutableSite;
	
	MutableCallSiteDynamicInvokerHandle(MutableCallSite site) {
		super(site);
		this.mutableSite = site;
	}
	
	MutableCallSiteDynamicInvokerHandle(MutableCallSiteDynamicInvokerHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.mutableSite = originalHandle.mutableSite;
	}
	
	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new MutableCallSiteDynamicInvokerHandle(this, newType);
	}

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(mutableSite.getTarget());
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}

		// MutableCallSite.getTarget is final, so using mutableSite here allows
		// us to inline getTarget without a guard.
		//
		return ILGenMacros.invokeExact_X(mutableSite.getTarget(), argPlaceholder);
	}

	// }}} JIT support
}

