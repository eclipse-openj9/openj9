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

import java.lang.reflect.Constructor;

/* ConstructorHandle is a MethodHandle subclass used to call <init> methods.  This
 * class is similar to DirectHandle in that the method to call has already
 * been resolved down to an exact method address.
 * <p>
 * The constructor must be called with the type of the <init> method. This means
 * it must have a void return type.
 * <p>
 * This is the equivalent of calling newInstance except with a known constructor.
 * <p>
 * The vmSlot will hold a J9Method address of the <init> method.
 */
final class ConstructorHandle extends PrimitiveHandle {
	
	static {
		//Making sure DirectHandle is loaded before the ConstructorHandle is loaded. Therefore, to secure a correct thunk.
		DirectHandle.load();
	}
		
	public ConstructorHandle(Class<?> referenceClass, MethodType type) throws NoSuchMethodException, IllegalAccessException {
		super(constructorMethodType(type, referenceClass), referenceClass, "<init>", KIND_CONSTRUCTOR, null); //$NON-NLS-1$
		/* Pass referenceClass as SpecialToken as KIND_SPECIAL & KIND_CONSTRUCTOR share lookup code */
		this.defc = finishMethodInitialization(referenceClass, type);
	}
	
	public ConstructorHandle(Constructor<?> ctor) throws IllegalAccessException {
		super(constructorMethodType(ctor), ctor.getDeclaringClass(), "<init>", KIND_CONSTRUCTOR, ctor.getModifiers(), ctor.getDeclaringClass()); //$NON-NLS-1$
		
		boolean succeed = setVMSlotAndRawModifiersFromConstructor(this, ctor);
		if (!succeed) {
			throw new IllegalAccessException();
		}
	}
	
	ConstructorHandle(ConstructorHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	/*
	 * Constructors have type (args of passed in type)referenceClass.
	 */
	private static final MethodType constructorMethodType(MethodType type, Class<?> referenceClazz) {
		return type.changeReturnType(referenceClazz);
	}
	
	/*
	 * Constructors have type (constructor.getParameterType)constructor.getDeclaringCLass.
	 */
	private static final MethodType constructorMethodType(Constructor<?> constructor) {
		Class<?> declaringClass = constructor.getDeclaringClass();
		return MethodType.methodType(declaringClass, constructor.getParameterTypes());
	}
	
	@Override
	boolean canRevealDirect() {
		return true;
	}

	// {{{ JIT support
	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	@FrameIteratorSkip
	private final Object invokeExact_thunkArchetype_L(int argPlaceholder) {
		if (ILGenMacros.isCustomThunk()) {
			DirectHandle.directCall_V(ILGenMacros.push(ILGenMacros.rawNew(referenceClass)), argPlaceholder);
		} else if (DirectHandle.isAlreadyCompiled(vmSlot)) {
			ComputedCalls.dispatchDirect_V(DirectHandle.compiledEntryPoint(vmSlot), ILGenMacros.push(ILGenMacros.rawNew(referenceClass)), argPlaceholder);
		} else {
			// Calling rawNew on referenceClass will cause <clinit> to be called, so we don't need an explicit call to initializeClassIfRequired.
			ComputedCalls.dispatchJ9Method_V(vmSlot, ILGenMacros.push(ILGenMacros.rawNew(referenceClass)), argPlaceholder);
		}
		return ILGenMacros.pop_L();
	}
	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new ConstructorHandle(this, newType);
	}
	
	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof ConstructorHandle) {
			((ConstructorHandle)right).compareWithConstructor(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithConstructor(ConstructorHandle left, Comparator c) {
		c.compareStructuralParameter(left.referenceClass, this.referenceClass);
	}
}

