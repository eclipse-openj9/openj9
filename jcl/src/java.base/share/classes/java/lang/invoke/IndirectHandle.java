/*[INCLUDE-IF Sidecar17]*/
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

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

abstract class IndirectHandle extends PrimitiveHandle {
	IndirectHandle(MethodType type, Class<?> referenceClass, String name, byte kind, int modifiers) {
		super(type, referenceClass, name, kind, modifiers, null);
	}
	
	IndirectHandle(MethodType type, Class<?> referenceClass, String name, byte kind) {
		super(type, referenceClass, name, kind, null);
	}

	IndirectHandle(IndirectHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
	}

	// {{{ JIT support
	protected abstract long vtableOffset(Object receiver);
	protected final long vtableIndexArgument(Object receiver){ return - vtableOffset(receiver); }

	protected final long jittedMethodAddress(Object receiver) {
		long receiverClass = getJ9ClassFromClass(receiver.getClass());
		long result; 
		if (VTABLE_ENTRY_SIZE == 4) {
			result = UNSAFE.getInt(receiverClass - vtableOffset(receiver));
		} else { 
			result = UNSAFE.getLong(receiverClass - vtableOffset(receiver));
		}
		return result;
	}

	@Override
	boolean canRevealDirect() {
		return true;
	}

	// }}} JIT support

	protected static final MethodType indirectMethodType(Method method) {
		MethodType originalType = MethodType.methodType(method.getReturnType(), method.getParameterTypes());
		return indirectMethodType(originalType, method.getDeclaringClass());
	}
	
	/* Indirect MethodHandles have the receiver type inserted as 
	 * first argument of the MH's type.
	 */
	protected static final MethodType indirectMethodType(MethodType type, Class<?> referenceClazz) {
		return type.insertParameterTypes(0, referenceClazz);
	}
	
	@Override
	public MethodHandle bindTo(Object value) throws IllegalArgumentException, ClassCastException {
		if (null == value) {
			return super.bindTo(value);
		}
		
		/*
		 * Check whether the first parameter has a reference type assignable from value. Note that MethodType.parameterType(0) will
		 * throw an IllegalArgumentException if type has no parameters.
		 */
		Class<?> firstParameterType = type().parameterType(0);
		if (firstParameterType.isPrimitive()) {
			throw new IllegalArgumentException();
		}

		/*
		 * Ensure type compatibility.
		 */
		value = firstParameterType.cast(value);

		/*
		 * Devirtualize virtual/interface handles.
		 */
		try {
			MethodHandle result = MethodHandles.Lookup.internalPrivilegedLookup.bind(value, name, type().dropFirstParameterType());

			/*
			 * An interface method must devirtualize to a public method. If the devirtualized method is not public,
			 * an IllegalAccessError will be thrown when the MethodHandle is invoked. In order to enforce this behaviour,
			 * we must preserve the original MethodHandle, i.e. not optimize to a ReceiverBoundHandle. 
			 */
			if (this instanceof InterfaceHandle) { 
				if ((result.getModifiers() & Modifier.PUBLIC) == 0) {
					throw new IllegalAccessException();
				}
			}
			
			return result;
		} catch (IllegalAccessException e) {
			/*
			 * Create a receiver bound MethodHandle by inserting the receiver object as an argument. This is done to 
			 * ensure that invocation is done using the original MethodHandle, which will result in an IllegalAccessError.
			 */
			return MethodHandles.insertArguments(this, 0, value);
		} catch (NoSuchMethodException e) {
			throw new Error(e);
		}
	}
	
	final void compareWithIndirect(IndirectHandle left, Comparator c) {
		c.compareStructuralParameter(left.referenceClass, this.referenceClass);
		c.compareStructuralParameter(left.vmSlot, this.vmSlot);
	}
}

