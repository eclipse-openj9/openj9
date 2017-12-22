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

/* DirectHandle is a MethodHandle subclass used to call methods that have already
 * been resolved down to an exact method address.
 * <p>
 * The exact method address is known in the following cases:
 * <ul>
 * <li> MethodHandles.lookup().findStatic </li>
 * <li> MethodHandles.lookup().findSpecial </li>
 * </ul>
 * <p>
 * The vmSlot will hold a J9Method address.
 */
class DirectHandle extends PrimitiveHandle {
	final int final_modifiers;
	final boolean originIsFindVirtual;
	
	DirectHandle(Class<?> referenceClass, String methodName, MethodType type, byte kind, Class<?> specialCaller) throws NoSuchMethodException, IllegalAccessException {
		this(referenceClass, methodName, type, kind, specialCaller, false);
	}
	
	DirectHandle(Class<?> referenceClass, String methodName, MethodType type, byte kind, Class<?> specialCaller, boolean originIsFindVirtual) throws NoSuchMethodException, IllegalAccessException {
		super(directMethodType(type, kind, specialCaller), referenceClass, methodName, kind, null);
		assert (kind != KIND_SPECIAL) || (specialCaller != null);
		this.specialCaller = specialCaller;
		addHandleToClassCache();
		this.defc = finishMethodInitialization(specialCaller, type);
		/* Kind should have been changed from KIND_VIRTUAL in finishMethodInitialization */
		assert (this.kind != KIND_VIRTUAL);
		final_modifiers = rawModifiers;
		this.originIsFindVirtual = originIsFindVirtual;
	}
	
	public DirectHandle(Method method, byte kind, Class<?> specialCaller) throws IllegalAccessException {
		this(method, kind, specialCaller, false);
	}
	
	public DirectHandle(Method method, byte kind, Class<?> specialCaller, boolean originIsFindVirtual) throws IllegalAccessException {
		super(directMethodType(MethodType.methodType(method.getReturnType(), method.getParameterTypes()), kind, specialCaller), method.getDeclaringClass(), method.getName(), kind, method.getModifiers(), null);
		assert (kind != KIND_SPECIAL) || (specialCaller != null);
		this.specialCaller = specialCaller;
		addHandleToClassCache();
		boolean succeed = setVMSlotAndRawModifiersFromMethod(this, referenceClass, method, this.kind, specialCaller);
		if (!succeed) {
			throw new IllegalAccessException();
		}
		final_modifiers = rawModifiers;
		this.originIsFindVirtual = originIsFindVirtual;
	}
	
	/*
	 * Create a new DirectHandle from another DirectHandle.  
	 * This is used by ReceiverBoundHandle
	 */
	DirectHandle(PrimitiveHandle other, byte kind) {
		super(ReceiverBoundHandle.receiverBoundMethodType(other.type), other.referenceClass, other.name, kind, other.rawModifiers, null);
		if (!(other instanceof DirectHandle)) {
			throw new IllegalArgumentException();
		}
		this.specialCaller = other.specialCaller;
		addHandleToClassCache();
		this.vmSlot = other.vmSlot;
		this.defc = other.defc;
		final_modifiers = rawModifiers;
		this.originIsFindVirtual = other.directHandleOriginatedInFindVirtual();
	}
	
	DirectHandle(DirectHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		final_modifiers = rawModifiers;
		this.originIsFindVirtual = originalHandle.originIsFindVirtual;
		addHandleToClassCache();
		// Reassigning the vmSlot because an HCR may have occurred since it was assigned in super()
		this.vmSlot = originalHandle.vmSlot;
	}

	/*
	 * Determine the correct MethodType for the DirectHandle
	 * 		KIND_STATIC		- unmodified
	 * 		KIND_SPECIAL	- insert specialCaller as first parameter
	 */
	private static final MethodType directMethodType(MethodType existingType, byte kind, Class<?> specialCaller) {
		if (kind == KIND_STATIC) {
			return existingType;
		}
		return existingType.insertParameterTypes(0, specialCaller);
	}
	
	private void addHandleToClassCache() {
		MethodHandleCache cache = MethodHandleCache.getCache(defc);
		cache.addDirectHandle(this);
	}

	final void nullCheckIfRequired(Object receiver) throws NullPointerException {
		if ((receiver == null) && !Modifier.isStatic(final_modifiers)) {
			receiver.getClass(); // Deliberate NPE
		}
	}

	@Override
	boolean canRevealDirect() {
		return true;
	}
	
	@Override
	boolean directHandleOriginatedInFindVirtual() {
		return originIsFindVirtual;
	}
	
	// {{{ JIT support
	private static final ThunkTable _thunkTable = new ThunkTable();
	protected ThunkTable thunkTable(){ return _thunkTable; }

 	// ILGen macros
 	protected static native boolean isAlreadyCompiled(long j9method);
 	protected static native long compiledEntryPoint(long j9method);

	protected static native void     directCall_V(int argPlaceholder);
	protected static native int      directCall_I(int argPlaceholder);
	protected static native long     directCall_J(int argPlaceholder);
	protected static native float    directCall_F(int argPlaceholder);
	protected static native double   directCall_D(int argPlaceholder);
	protected static native Object   directCall_L(int argPlaceholder);

	protected static native void     directCall_V(Object receiver, int argPlaceholder);
	protected static native int      directCall_I(Object receiver, int argPlaceholder);
	protected static native long     directCall_J(Object receiver, int argPlaceholder);
	protected static native float    directCall_F(Object receiver, int argPlaceholder);
	protected static native double   directCall_D(Object receiver, int argPlaceholder);
	protected static native Object   directCall_L(Object receiver, int argPlaceholder);

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(int argPlaceholder) {
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			directCall_V(argPlaceholder);
		} else if (isAlreadyCompiled(vmSlot)) {
			ComputedCalls.dispatchDirect_V(compiledEntryPoint(vmSlot), argPlaceholder); 
		} else {
			ComputedCalls.dispatchJ9Method_V(vmSlot, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(Object receiver, int argPlaceholder) {
		nullCheckIfRequired(receiver);
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			directCall_V(receiver, argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			ComputedCalls.dispatchDirect_V(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		} else {
			ComputedCalls.dispatchJ9Method_V(vmSlot, receiver, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_I(int argPlaceholder) {
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_I(argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_I(compiledEntryPoint(vmSlot), argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_I(vmSlot, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_I(Object receiver, int argPlaceholder) {
		nullCheckIfRequired(receiver);
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_I(receiver, argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_I(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_I(vmSlot, receiver, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final long invokeExact_thunkArchetype_J(int argPlaceholder) {
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_J(argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_J(compiledEntryPoint(vmSlot), argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_J(vmSlot, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final long invokeExact_thunkArchetype_J(Object receiver, int argPlaceholder) {
		nullCheckIfRequired(receiver);
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_J(receiver, argPlaceholder);	 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_J(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_J(vmSlot, receiver, argPlaceholder);	 
		}
	}

	@FrameIteratorSkip
	private final float invokeExact_thunkArchetype_F(int argPlaceholder) {
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_F(argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_F(compiledEntryPoint(vmSlot), argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_F(vmSlot, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final float invokeExact_thunkArchetype_F(Object receiver, int argPlaceholder) {
		nullCheckIfRequired(receiver);
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_F(receiver, argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_F(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_F(vmSlot, receiver, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final double invokeExact_thunkArchetype_D(int argPlaceholder) {
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_D(argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_D(compiledEntryPoint(vmSlot), argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_D(vmSlot, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final double invokeExact_thunkArchetype_D(Object receiver, int argPlaceholder) {
		nullCheckIfRequired(receiver);
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_D(receiver, argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_D(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_D(vmSlot, receiver, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final Object invokeExact_thunkArchetype_L(int argPlaceholder) {
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_L(argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_L(compiledEntryPoint(vmSlot), argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_L(vmSlot, argPlaceholder); 
		}
	}

	@FrameIteratorSkip
	private final Object invokeExact_thunkArchetype_L(Object receiver, int argPlaceholder) {
		nullCheckIfRequired(receiver);
		initializeClassIfRequired();
		if (ILGenMacros.isCustomThunk()) {
			return directCall_L(receiver, argPlaceholder); 
		} else if (isAlreadyCompiled(vmSlot)) {
			return ComputedCalls.dispatchDirect_L(compiledEntryPoint(vmSlot), receiver, argPlaceholder);
		} else {
			return ComputedCalls.dispatchJ9Method_L(vmSlot, receiver, argPlaceholder); 
		}
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new DirectHandle(this, newType);
	}

	// Not final because it's overridden in ReceiverBoundHandle
	void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof DirectHandle) {
			((DirectHandle)right).compareWithDirect(this, c);
		} else {
			c.fail();
		}
	}

	void compareWithDirect(DirectHandle left, Comparator c) {
		c.compareStructuralParameter(left.vmSlot, this.vmSlot);
	}
	
	//Used by ConstructorHandle
	//Making sure the DirectHandle class is loaded before ConstructorHandle is loaded. Therefore, to secure a correct thunk.
	public static void load() {}
}

