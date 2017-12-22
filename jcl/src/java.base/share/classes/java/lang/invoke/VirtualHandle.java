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

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

/*
 * VirtualHandle is a MethodHandle that does virtual dispatch 
 * on the receiver.  
 * <p>
 * The vmSlot holds the vtable index for the correct method.
 * The type is the same as the method's except with the receiver's class prepended
 */
final class VirtualHandle extends IndirectHandle {

	VirtualHandle(Method method) throws IllegalAccessException {
		super(indirectMethodType(method), method.getDeclaringClass(), method.getName(), KIND_VIRTUAL, method.getModifiers());
		if (Modifier.isStatic(method.getModifiers())) {
			throw new IllegalArgumentException();
		}

		// Set the vmSlot to an vtable index
		boolean succeed = setVMSlotAndRawModifiersFromMethod(this, referenceClass, method, this.kind, null);
		if (!succeed) {
			throw new IllegalAccessException();
		}
	}
	
	VirtualHandle(DirectHandle nonVirtualHandle) throws IllegalAccessException {
		super(nonVirtualHandle.type(), nonVirtualHandle.referenceClass, nonVirtualHandle.name, KIND_VIRTUAL, nonVirtualHandle.rawModifiers);
		this.defc = nonVirtualHandle.defc;
		
		// Set the vmSlot to an vtable index
		boolean succeed = setVMSlotAndRawModifiersFromSpecialHandle(this, nonVirtualHandle);
		if (!succeed) {
			throw new IllegalAccessException();
		}
	}
	
	VirtualHandle(VirtualHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	/// {{{ JIT support
	protected final long vtableOffset(Object receiver) {
		return vmSlot - INTRP_VTABLE_OFFSET;
	}

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

 	// ILGen macros
	protected static native void     virtualCall_V(Object receiver, int argPlaceholder);
	protected static native int      virtualCall_I(Object receiver, int argPlaceholder);
	protected static native long     virtualCall_J(Object receiver, int argPlaceholder);
	protected static native float    virtualCall_F(Object receiver, int argPlaceholder);
	protected static native double   virtualCall_D(Object receiver, int argPlaceholder);
	protected static native Object   virtualCall_L(Object receiver, int argPlaceholder);

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(Object receiver, int argPlaceholder) {
		if (ILGenMacros.isCustomThunk()) {
			virtualCall_V(receiver, argPlaceholder);
		} else {
			ComputedCalls.dispatchVirtual_V(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_I(Object receiver, int argPlaceholder) {
		if (ILGenMacros.isCustomThunk()) {
			return virtualCall_I(receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchVirtual_I(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final long invokeExact_thunkArchetype_J(Object receiver, int argPlaceholder) {
		if (ILGenMacros.isCustomThunk()) {
			return virtualCall_J(receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchVirtual_J(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final float invokeExact_thunkArchetype_F(Object receiver, int argPlaceholder) {
		if (ILGenMacros.isCustomThunk()) {
			return virtualCall_F(receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchVirtual_F(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final double invokeExact_thunkArchetype_D(Object receiver, int argPlaceholder) {
		if (ILGenMacros.isCustomThunk()) {
			return virtualCall_D(receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchVirtual_D(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final Object invokeExact_thunkArchetype_L(Object receiver, int argPlaceholder) {
		if (ILGenMacros.isCustomThunk()) {
			return virtualCall_L(receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchVirtual_L(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	/// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new VirtualHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof VirtualHandle) {
			((VirtualHandle)right).compareWithVirtual(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithVirtual(VirtualHandle left, Comparator c) {
		super.compareWithIndirect(left, c);
	}
}

