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

/* ReceiverBoundHandle is a DirectHandle subclass used to call methods 
 * that have an exact known address and a bound first parameter.
 * <b>
 * The bound first parameter will be inserted into the stack prior to
 * executing.  We have a "free" stack slot that contains either the MH
 * receiver or null and the receiver parameter will be used to hammer
 * that slot.
 * <p>
 * This is use-able by both static and special methods as all stack 
 * shapes will have a free slot as their first slot.
 * <p>
 * It may be necessary to convert the "receiver" object into the right type.
 * If this is a call to a static method, it may be necessary to convert the
 * object to a primitive.
 * <p>
 * The vmSlot will hold a J9Method address.
 */
@VMCONSTANTPOOL_CLASS
final class ReceiverBoundHandle extends DirectHandle {
	final Object receiver;
	final MethodHandle combinableVersion;
	
	public ReceiverBoundHandle(PrimitiveHandle toBind, Object receiver, MethodHandle combinableVersion) {
		super(toBind, KIND_BOUND);
		if (toBind instanceof DirectHandle) {
			vmSlot = toBind.vmSlot;
			this.receiver = receiver;
			this.defc = toBind.defc;
		} else {
			throw new IllegalArgumentException();
		}
		this.combinableVersion = combinableVersion;
	}
	
	private ReceiverBoundHandle(ReceiverBoundHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.receiver = originalHandle.receiver;
		this.combinableVersion = originalHandle.combinableVersion.cloneWithNewType(newType);
	}
	
	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new ReceiverBoundHandle(this, newType);
	}
	
	/*
	 * MethodType is same as incoming handle minus the first
	 * argument.
	 */
	static final MethodType receiverBoundMethodType(MethodType type) {
		return type.dropFirstParameterType();
	}

	@Override
	boolean canRevealDirect() {
		return false;
	}

	/*[IF ]*/
	/* We don't want to have the receiver object buried deep in the
	 * MethodHandle chain, necessitating a long dereference chain to load it.
	 * Thus, we want permuteArguments and insertArguments to return
	 * BruteArgumentMoverHandles that have the receiver in them.
	 */
	/*[ENDIF]*/
	MethodHandle permuteArguments(MethodType permuteType, int... permute) throws NullPointerException, IllegalArgumentException {
		MethodHandle result = new PermuteHandle(permuteType, this, permute);
		if (true) {
			result = combinableVersion.permuteArguments(permuteType, permute);
		}
		return result;
	}

	MethodHandle insertArguments(MethodHandle equivalent, MethodHandle unboxingHandle, int location, Object... values) {
		MethodHandle result = equivalent;
		if (true) {
			result = combinableVersion.insertArguments(equivalent, unboxingHandle, location, values);
		}
		return result;
	}


	// {{{ JIT support
	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }
	
	/*[IF ]*/
	/* Null check the receiver if the handle isn't for a static method.
	 * This is the lowest risk change for this failure.
	 * TODO: Investigate introducing a new handle subclass: 
	 * NullReceiverBoundHandle as a subclass of RBH that only
	 * throws NPE.  We should always know at creation time
	 * which kind of handle it will be - NRBH or RBH.
	 * 
	 */
	/*[ENDIF]*/
	final void nullCheckReceiverIfNonStatic(){
		if ((receiver == null) && !Modifier.isStatic(final_modifiers)) {
			receiver.getClass(); // Deliberate NPE
		}
	}
	
	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(int argPlaceholder) {
		nullCheckReceiverIfNonStatic();
		if (ILGenMacros.isCustomThunk()) {
			directCall_V(receiver, argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot))
			ComputedCalls.dispatchDirect_V(compiledEntryPoint(vmSlot), receiver, argPlaceholder); 
		else
			ComputedCalls.dispatchJ9Method_V(vmSlot, receiver, argPlaceholder); 
	}

	
	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_I(int argPlaceholder) {
		nullCheckReceiverIfNonStatic();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_I(receiver, argPlaceholder);
		} else if (isAlreadyCompiled(vmSlot))
			return ComputedCalls.dispatchDirect_I(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		else
			return ComputedCalls.dispatchJ9Method_I(vmSlot, receiver, argPlaceholder);
	}

	
	@FrameIteratorSkip
	private final long invokeExact_thunkArchetype_J(int argPlaceholder) {
		nullCheckReceiverIfNonStatic();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_J(receiver, argPlaceholder);
		} else if (isAlreadyCompiled(vmSlot))
			return ComputedCalls.dispatchDirect_J(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		else
			return ComputedCalls.dispatchJ9Method_J(vmSlot, receiver, argPlaceholder);
	}

	
	@FrameIteratorSkip
	private final float invokeExact_thunkArchetype_F(int argPlaceholder) {
		nullCheckReceiverIfNonStatic();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_F(receiver, argPlaceholder);
		} else if (isAlreadyCompiled(vmSlot))
			return ComputedCalls.dispatchDirect_F(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		else
			return ComputedCalls.dispatchJ9Method_F(vmSlot, receiver, argPlaceholder);
	}

	
	@FrameIteratorSkip
	private final double invokeExact_thunkArchetype_D(int argPlaceholder) {
		nullCheckReceiverIfNonStatic();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_D(receiver, argPlaceholder);
		} else if (isAlreadyCompiled(vmSlot))
			return ComputedCalls.dispatchDirect_D(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		else
			return ComputedCalls.dispatchJ9Method_D(vmSlot, receiver, argPlaceholder);
	}

	
	@FrameIteratorSkip
	private final Object invokeExact_thunkArchetype_L(int argPlaceholder) { 
		nullCheckReceiverIfNonStatic();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_L(receiver, argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot))
			return ComputedCalls.dispatchDirect_L(compiledEntryPoint(vmSlot), receiver, argPlaceholder); 
		else
			return ComputedCalls.dispatchJ9Method_L(vmSlot, receiver, argPlaceholder); 
	}

	// }}} JIT support

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof ReceiverBoundHandle) {
			((ReceiverBoundHandle)right).compareWithReceiverBound(this, c);
		} else {
			c.fail();
		}
	}

	void compareWithDirect(DirectHandle left, Comparator c) {
		c.fail();
	}

	final void compareWithReceiverBound(ReceiverBoundHandle left, Comparator c) {
		c.compareUserSuppliedParameter(left.receiver, this.receiver);
		super.compareWithDirect(left, c);
	}
}

