/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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

/*
 * InterfaceHandle is a MethodHandle that does interface dispatch 
 * on the receiver.  
 * <p>
 * The vmSlot holds the itable index for the correct method.
 * The type is the same as the method's except with the interface class prepended
 */
final class InterfaceHandle extends IndirectHandle {

	/*[IF ]*/
	// Support natives defined in the JIT dll
	/*[ENDIF]*/
	static native void registerNatives();
	static {
		registerNatives();
	}

	InterfaceHandle(Class<?> referenceClass, String methodName, MethodType type) throws NoSuchMethodException, IllegalAccessException {
		super(indirectMethodType(type, referenceClass), referenceClass, methodName, KIND_INTERFACE);
		assert referenceClass.isInterface();
		this.defc = finishMethodInitialization(null, type);
	}
	
	
	InterfaceHandle(Method method) throws IllegalAccessException {
		super(indirectMethodType(method), method.getDeclaringClass(), method.getName(), KIND_INTERFACE, method.getModifiers());
			
		if (!referenceClass.isInterface()) {
			throw new IllegalArgumentException();
		}
		
		boolean succeed = setVMSlotAndRawModifiersFromMethod(this, referenceClass, method, this.kind, specialCaller);
		if (!succeed) {
			throw new IllegalAccessException();
		}
	}
	
	public InterfaceHandle(InterfaceHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	/// {{{ JIT support
	protected final long vtableOffset(Object receiver) {
		/*[IF]*/
		/* Must be 'referenceClass' rather than 'type().parameterType(0)' or
		 * 'defc' so that the itable index matches the defining interface at
		 * handle creation time, otherwise handles on interfaces methods defined
		 * in parent interfaces will crash
		 */
		/*[ENDIF]*/
		Class<?> interfaceClass = referenceClass;
		if (interfaceClass.isInstance(receiver)) {
			long interfaceJ9Class = getJ9ClassFromClass(interfaceClass);
			long receiverJ9Class = getJ9ClassFromClass(receiver.getClass());
			return convertITableIndexToVTableIndex(interfaceJ9Class, (int)vmSlot, receiverJ9Class) << VTABLE_ENTRY_SHIFT;
		} else {
			throw new IncompatibleClassChangeError();
		}
	}

	protected static native int convertITableIndexToVTableIndex(long interfaceClass, int itableIndex, long receiverClass);

	// Thunks

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	protected static native void interfaceCall_V(Object receiver, int argPlaceholder);
	protected static native int interfaceCall_I(Object receiver, int argPlaceholder);
	protected static native long interfaceCall_J(Object receiver, int argPlaceholder);
	protected static native float interfaceCall_F(Object receiver, int argPlaceholder);
	protected static native double interfaceCall_D(Object receiver, int argPlaceholder);
	protected static native Object interfaceCall_L(Object receiver, int argPlaceholder);

	@FrameIteratorSkip
	private final void invokeExact_thunkArchetype_V(Object receiver, int argPlaceholder) {
		/*[IF]*/
		if (ILGenMacros.isCustomThunk()) {
			interfaceCall_V(receiver, argPlaceholder);
		} else 
		/*[ENDIF]*/
		{
			ComputedCalls.dispatchVirtual_V(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_I(Object receiver, int argPlaceholder) {
		/*[IF]*/
		if (ILGenMacros.isCustomThunk()) {
			return interfaceCall_I(receiver, argPlaceholder);
		} else 
		/*[ENDIF]*/
		{
			return ComputedCalls.dispatchVirtual_I(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final long invokeExact_thunkArchetype_J(Object receiver, int argPlaceholder) {
		/*[IF]*/
		if (ILGenMacros.isCustomThunk()) {
			return interfaceCall_J(receiver, argPlaceholder);
		} else 
		/*[ENDIF]*/
		{
			return ComputedCalls.dispatchVirtual_J(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final float invokeExact_thunkArchetype_F(Object receiver, int argPlaceholder) {
		/*[IF]*/
		if (ILGenMacros.isCustomThunk()) {
			return interfaceCall_F(receiver, argPlaceholder);
		} else 
		/*[ENDIF]*/
		{
			return ComputedCalls.dispatchVirtual_F(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final double invokeExact_thunkArchetype_D(Object receiver, int argPlaceholder) {
		/*[IF]*/
		if (ILGenMacros.isCustomThunk()) {
			return interfaceCall_D(receiver, argPlaceholder);
		} else 
		/*[ENDIF]*/
		{
			return ComputedCalls.dispatchVirtual_D(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	@FrameIteratorSkip
	private final Object invokeExact_thunkArchetype_L(Object receiver, int argPlaceholder) {
		/*[IF]*/
		if (ILGenMacros.isCustomThunk()) {
			return interfaceCall_L(receiver, argPlaceholder);
		} else 
		/*[ENDIF]*/
		{
			return ComputedCalls.dispatchVirtual_L(jittedMethodAddress(receiver), vtableIndexArgument(receiver), receiver, argPlaceholder);
		}
	}

	/// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new InterfaceHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof InterfaceHandle) {
			((InterfaceHandle)right).compareWithInterface(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithInterface(InterfaceHandle left, Comparator c) {
		super.compareWithIndirect(left, c);
	}
}

