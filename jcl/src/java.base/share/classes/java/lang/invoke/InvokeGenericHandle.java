/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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

import java.lang.reflect.Modifier;

/* InvokeGenericHandle is a MethodHandle subclass used to MethodHandle.invokeGeneric
 * with a specific signature on a MethodHandle.
 * <p>
 * The vmSlot will hold 0 as there is no actual method for it.
 * <p>
 * Can be thought of as a special case of VirtualHandle.
 */
final class InvokeGenericHandle extends PrimitiveHandle {
 	/* MethodType that the first argument MethodHandle will be cast to using asType */
	@VMCONSTANTPOOL_FIELD
 	final MethodType castType;
 
	
	InvokeGenericHandle(MethodType type) {
		super(invokeGenericMethodType(type), MethodHandle.class, "invoke", KIND_INVOKEGENERIC, PUBLIC_FINAL_NATIVE, null); //$NON-NLS-1$
		if (type == null) {
			throw new IllegalArgumentException();
		}
		this.vmSlot = 0;
		this.defc = MethodHandle.class;
		this.castType = type;
	}

	InvokeGenericHandle(InvokeGenericHandle originalHandle,	MethodType newType) {
		super(originalHandle, newType);
		this.castType = originalHandle.castType;
	}

	/*
	 * Insert MethodHandle as first argument to existing type.
	 * (LMethodHandle;otherargs)returntype
	 */
	private static final MethodType invokeGenericMethodType(MethodType type) {
		return type.insertParameterTypes(0, MethodHandle.class);
	}

	@Override
	boolean canRevealDirect() {
		/* This is invokevirtual of MethodHandle.invoke() */
		return true;
	}
	
	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	protected ThunkTuple computeThunks(Object arg) {
		// The first argument is always a MethodHandle.
		// We don't upcast that to Object to avoid a downcast in the thunks.
		//
		return thunkTable().get(new ThunkKey(ThunkKey.computeThunkableType(type(), 0, 1)));
	}

	@FrameIteratorSkip
 	private final int invokeExact_thunkArchetype_X(MethodHandle next, int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		return ILGenMacros.invokeExact_X(next.asType(castType), argPlaceholder);
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new InvokeGenericHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof InvokeGenericHandle) {
			((InvokeGenericHandle)right).compareWithInvokeGeneric(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithInvokeGeneric(InvokeGenericHandle left, Comparator c) {
		// Nothing distinguishes InvokeGenericHandles except their type, which Comparator already deals with
	}
}

