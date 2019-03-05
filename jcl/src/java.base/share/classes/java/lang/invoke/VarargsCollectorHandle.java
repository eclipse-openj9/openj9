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

import java.lang.invoke.MethodHandles.Lookup;
/*[IF Java11]*/
import java.lang.reflect.Array;
import java.util.Arrays;
import java.util.Objects;
/*[ENDIF]*/

import com.ibm.oti.util.Msg;

/*
 * VarargsCollectorHandle is a MethodHandle subclass used to implement
 * MethodHandle.asVarargsCollector(Class<?> arrayType)
 * 
 * Type of VarargsCollectorHandle and its 'next' handle will always match
 */
final class VarargsCollectorHandle extends MethodHandle {
	final MethodHandle next;
	final Class<?> arrayType;
	final boolean isPrimitiveVarargs;
	
	VarargsCollectorHandle(MethodHandle next, Class<?> arrayType, boolean isPrimitiveVarargs) {
		super(varargsCollectorType(next.type, arrayType), KIND_VARARGSCOLLECT, null);
		this.next = next;
		if (arrayType == null) {
			throw new IllegalArgumentException();
		}
		this.arrayType = arrayType;
		this.isPrimitiveVarargs = isPrimitiveVarargs;
	}
	
	VarargsCollectorHandle(VarargsCollectorHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.next = originalHandle.next;
		this.arrayType = originalHandle.arrayType;
		this.isPrimitiveVarargs = originalHandle.isPrimitiveVarargs;
	}
	
	@Override
	Class<?> getDefc() throws InternalError {
		if (isPrimitiveVarargs) {
			return next.getDefc();
		}
		return super.getDefc();
	}
	
	@Override
	Class<?> getReferenceClass() throws InternalError {
		if (isPrimitiveVarargs) {
			return next.getReferenceClass();
		}
		return super.getReferenceClass();
	}
	
	@Override
	Class<?> getSpecialCaller() throws InternalError {
		if (isPrimitiveVarargs) {
			return next.getSpecialCaller();
		}
		return super.getSpecialCaller();
	}
	
	@Override
	String getMethodName() throws InternalError {
		if (isPrimitiveVarargs) {
			return next.getMethodName();
		}
		return super.getMethodName();
	}
	
	@Override
	int getModifiers() throws InternalError {
		if (isPrimitiveVarargs) {
			return next.getModifiers();
		}
		return super.getModifiers();
	}
	
	static MethodType varargsCollectorType(MethodType nextType, Class<?> arrayType) {
		return nextType.changeParameterType(nextType.parameterCount() - 1, arrayType);
	}
	
	public Object invokeWithArguments(Object... args) throws Throwable, WrongMethodTypeException, ClassCastException {
		int argsLength = 0;
		if (args != null) {
			argsLength = args.length;
		}
		/*[IF Java11]*/
		/*
		 * If argument count exceeds the parameter count of the MethodHandle, special handling is required to
		 * store the additional arguments in the trailing array.
		 */
		int mhLength = this.type.parameterCount();
		if (argsLength > mhLength) {
			int numTrailingArgs = argsLength - mhLength + 1;
			Class<?> trailingArrayType = this.arrayType.getComponentType();
			Object trailingArgs = Array.newInstance(trailingArrayType, numTrailingArgs);
			MethodHandle arraySetter = MethodHandles.arrayElementSetter(this.arrayType);
			for (int i = 0; i < numTrailingArgs; i++) {
				arraySetter.invoke(trailingArgs, i, args[mhLength - 1 + i]);
			}
			args = Arrays.copyOf(args, mhLength);
			args[mhLength - 1] = trailingArgs;
			return this.asFixedArity().invokeWithArguments(args);
		} else
		/*[ENDIF]*/
		{
			if (argsLength < 253) {
				MethodHandle mh = IWAContainer.getMH(argsLength);
				return mh.invokeExact((MethodHandle) this, args); 
			}
			MethodHandle mh = this.asType(MethodType.genericMethodType(argsLength));
			mh = mh.asSpreader(Object[].class, argsLength);
			return mh.invokeExact(args);
		}
	}
	
	private CollectHandle previousCollector = null;
	
	private WrongMethodTypeException throwNewWMTE(IllegalArgumentException iae) {
		/*[MSG "K0681", "Failed to build collector"]*/
		throw new WrongMethodTypeException(com.ibm.oti.util.Msg.getString("K0681"), iae); //$NON-NLS-1$
	}
	
	@Override
	public MethodHandle asType(MethodType newType) throws ClassCastException {
		if (type == newType)  {
			return this;
		}
		int parameterCount = type.parameterCount();
		int newTypeParameterCount = newType.parameterCount();
		if (parameterCount == newTypeParameterCount) {
			if (type.lastParameterType().isAssignableFrom(newType.lastParameterType())) {
				return next.asType(newType);
			}
		}
		/*[IF ]*/
		/*
		 * 	MH Type		newType		newType - MH type	num args to collect
		 *  (I[])		()				-1					0
		 *  (II[])		(I)				-1					0
		 *  (II[])		(II)			0					1
		 *  (II[])		(III)			1					2
		 */
		/*[ENDIF]*/
		int collectCount = newTypeParameterCount - parameterCount + 1;
		if (collectCount < 0) {
			/*[IF ]*/
			/* Expected by the JCK - should be allowed to just let asCollector throw the IAE */
			/*[ENDIF]*/
			throw new WrongMethodTypeException();
		}
		CollectHandle collector = previousCollector;
		if ((collector == null) || (collector.collectArraySize != collectCount)) {
			try {
				collector = (CollectHandle) next.asCollector(arrayType, collectCount);
			} catch (IllegalArgumentException iae) {
				throw throwNewWMTE(iae);
			}
			// update cached collector handle
			previousCollector = collector;
		}
		return collector.asType(newType);
	}
	
	@Override
	public MethodHandle	asVarargsCollector(Class<?> arrayParameter) throws IllegalArgumentException {
		if (arrayType == arrayParameter) {
			return this;
		}
		if (!arrayType.isAssignableFrom(arrayParameter)) {
			/*[MSG "K05cc", "Cannot assign '{0}' to methodtype '{1}'"]*/
			throw new IllegalArgumentException(Msg.getString("K05cc", arrayParameter, type)); //$NON-NLS-1$
		}
		return next.asVarargsCollector(arrayParameter);
	}

	@Override
	public MethodHandle asFixedArity() {
		MethodHandle fixedArity = next;
		while (fixedArity.isVarargsCollector()) {
			// cover varargsCollector on a varargsCollector
			fixedArity = ((VarargsCollectorHandle)fixedArity).next;
		}
		// asType will return 'this' if type is the same
		return fixedArity.asType(type());
	}
	
	@Override
	boolean canRevealDirect() {
		return isPrimitiveVarargs;
	}
	
	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }
	
	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) throws Throwable {
		/*[IF ]*/
		/* Disable custom thunks as they are currently a perf regression */
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		/*[ENDIF]*/
		return ILGenMacros.invokeExact_X(next, argPlaceholder);
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new VarargsCollectorHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof VarargsCollectorHandle) {
			((VarargsCollectorHandle)right).compareWithVarargsCollector(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithVarargsCollector(VarargsCollectorHandle left, Comparator c) {
		c.compareStructuralParameter(left.arrayType, this.arrayType);
		c.compareChildHandle(left.next, this.next);
	}
}

