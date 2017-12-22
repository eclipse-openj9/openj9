/*[INCLUDE-IF Panama]*/
/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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

import java.nicl.LibrarySymbol;

import jdk.internal.nicl.types.PointerTokenImpl;

/**
 * A NativeMethodHandle is a reference to a native method.  It is typed with Java carrier types (Java equivalent of native types)
 * It can be invoked in the same ways as a MethodHandle
 *
 * A NativeMethodHandle can be created using the MethodHandles factory (see MethodHandles.findNative). 
 */
public class NativeMethodHandle extends PrimitiveHandle {

	private static final ThunkTable _thunkTable = new ThunkTable();

	private long J9NativeCalloutDataRef;

	private native void initJ9NativeCalloutDataRef(String[] argLayoutStrings);

	/* TODO Need to address cleanup of J9NativeCalloutDataRef */
	private native void freeJ9NativeCalloutDataRef();

	/* This constructor is used to bootstrap the Panama JCL (for sterror, dlopen, dlsym, etc.). It only handles primitive methods. */
	NativeMethodHandle(String methodName, MethodType type, long nativeAddress) throws IllegalAccessException {
		this(methodName, type, nativeAddress, null);
		checkIfPrimitiveType(type);
	}

	/* This constructor is used by generated 'Impl' classes via NativeInvoker. It handles non-primitive methods. */
	NativeMethodHandle(String methodName, MethodType type, long nativeAddress, String[] layoutStrings) throws IllegalAccessException {
		super(type, null, methodName, KIND_NATIVE, 0, null);
		this.vmSlot = nativeAddress;
		initJ9NativeCalloutDataRef(layoutStrings);
	}

	NativeMethodHandle(NativeMethodHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		initJ9NativeCalloutDataRef(null);
	}

	@Override
	protected ThunkTable thunkTable(){ return _thunkTable; }

	@Override
	protected final ThunkTuple computeThunks(Object arg) {
		/*
		 * Normalize everything, the only types we expect to see are primitives. The MH filters do
		 * the conversions for Pointers, Structs, Functional Interfaces, etc. TODO if this assumption
		 * changes we need to modify this code.
		 * 
		 *  TODO When we get JIT support we will have to re-asses how thunks are shared if subclasses
		 *  add final fields. If final fields are added to the compiled thunks that might limit the ability
		 *  to share.
		 */
		return thunkTable().get(new ThunkKey(ThunkKey.computeThunkableType(type())));
	}

	private void checkIfPrimitiveType(MethodType newType) {
		/* Throw InternalError if the newType contains non-primitives. */
		Class<?> retType = newType.returnType;
		if (!retType.isPrimitive()) {
			throw new InternalError("newType has non-primitive return type, only primtives are supported");  //$NON-NLS-1$
		}

		Class<?>[] argTypes = newType.parameterArray();
		for (int i = 0; i < argTypes.length; i++) {
			if (!argTypes[i].isPrimitive()) {
				throw new InternalError("newType has non-primitive argument type, only primitives are supported"); //$NON-NLS-1$
			}
		}
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		/* Only primitive method types are supported, need to ensure newType contains only primitives */
		checkIfPrimitiveType(newType);
		return new NativeMethodHandle(this, newType);
	}

	@Override
	void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof NativeMethodHandle) {
			((NativeMethodHandle)right).compareWithNative(this, c);
		} else {
			c.fail();
		}
	}
	void compareWithNative(NativeMethodHandle left, Comparator c) {
		c.compareStructuralParameter(left.vmSlot, this.vmSlot);
	}
}
