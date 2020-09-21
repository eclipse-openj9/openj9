/*[INCLUDE-IF Sidecar19-SE & !OPENJDK_METHODHANDLES]*/
/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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

import java.lang.invoke.MethodHandle.PolymorphicSignature;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.ArrayList;
/*[IF Java12]*/
import java.util.Optional;
/*[ENDIF]*/
/*[IF Sidecar19-SE]
import jdk.internal.misc.Unsafe;
/*[ELSE]*/
import sun.misc.Unsafe;
/*[ENDIF]*/
/*[IF Java12]*/
import java.lang.constant.ClassDesc;
import java.lang.constant.Constable;
import java.lang.constant.ConstantDesc;
import java.lang.constant.ConstantDescs;
import java.lang.constant.DirectMethodHandleDesc;
import java.lang.constant.DynamicConstantDesc;
import java.util.Objects;
/*[ENDIF] Java12 */

import java.util.Map;
import java.util.HashMap;

/*[IF Java14]*/
import java.util.function.BiFunction;
import java.lang.reflect.Method;
/*[ENDIF] Java14 */

/**
 * Dynamically typed reference to a field, allowing read and write operations, 
 * both atomic and with/without memory barriers. See {@link AccessMode} for
 * supported operations.
 * 
 * VarHandle instances are created through the MethodHandles factory API.
 * 
 */
public abstract class VarHandle extends VarHandleInternal 
/*[IF Java12]*/
	implements Constable
/*[ENDIF]*/
{
	/**
	 * Access mode identifiers for VarHandle operations.
	 */
	public enum AccessMode {
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#get(Object...) get(Object...)}.
		 */
		GET("get", AccessType.GET, false), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#set(Object...) set(Object...)}.
		 */
		SET("set", AccessType.SET, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getVolatile(Object...) getVolatile(Object...)}.
		 */
		GET_VOLATILE("getVolatile", AccessType.GET, false), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#setVolatile(Object...) setVolatile(Object...)}.
		 */
		SET_VOLATILE("setVolatile", AccessType.SET, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getOpaque(Object...) getOpaque(Object...)}.
		 */
		GET_OPAQUE("getOpaque", AccessType.GET, false), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#setOpaque(Object...) setOpaque(Object...)}.
		 */
		SET_OPAQUE("setOpaque", AccessType.SET, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAcquire(Object...) getAcquire(Object...)}.
		 */
		GET_ACQUIRE("getAcquire", AccessType.GET, false), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#setRelease(Object...) setRelease(Object...)}.
		 */
		SET_RELEASE("setRelease", AccessType.SET, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#compareAndSet(Object...) compareAndSet(Object...)}.
		 */
		COMPARE_AND_SET("compareAndSet", AccessType.COMPARE_AND_SET, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#compareAndExchange(Object...) compareAndExchange(Object...)}.
		 */
		COMPARE_AND_EXCHANGE("compareAndExchange", AccessType.COMPARE_AND_EXCHANGE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#compareAndExchangeAcquire(Object...) compareAndExchangeAcquire(Object...)}.
		 */
		COMPARE_AND_EXCHANGE_ACQUIRE("compareAndExchangeAcquire", AccessType.COMPARE_AND_EXCHANGE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#compareAndExchangeRelease(Object...) compareAndExchangeRelease(Object...)}.
		 */
		COMPARE_AND_EXCHANGE_RELEASE("compareAndExchangeRelease", AccessType.COMPARE_AND_EXCHANGE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#weakCompareAndSet(Object...) weakCompareAndSet(Object...)}.
		 */
		WEAK_COMPARE_AND_SET("weakCompareAndSet", AccessType.COMPARE_AND_SET, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#weakCompareAndSetAcquire(Object...) weakCompareAndSetAcquire(Object...)}.
		 */
		WEAK_COMPARE_AND_SET_ACQUIRE("weakCompareAndSetAcquire", AccessType.COMPARE_AND_SET, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#weakCompareAndSetRelease(Object...) weakCompareAndSetRelease(Object...)}.
		 */
		WEAK_COMPARE_AND_SET_RELEASE("weakCompareAndSetRelease", AccessType.COMPARE_AND_SET, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#weakCompareAndSetPlain(Object...) weakCompareAndSetPlain(Object...)}.
		 */
		WEAK_COMPARE_AND_SET_PLAIN("weakCompareAndSetPlain", AccessType.COMPARE_AND_SET, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndSet(Object...) getAndSet(Object...)}.
		 */
		GET_AND_SET("getAndSet", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndSetAcquire(Object...) getAndSetAcquire(Object...)}.
		 */
		GET_AND_SET_ACQUIRE("getAndSetAcquire", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndSetRelease(Object...) getAndSetRelease(Object...)}.
		 */
		GET_AND_SET_RELEASE("getAndSetRelease", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndAdd(Object...) getAndAdd(Object...)}.
		 */
		GET_AND_ADD("getAndAdd", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndAddAcquire(Object...) getAndAddAcquire(Object...)}.
		 */
		GET_AND_ADD_ACQUIRE("getAndAddAcquire", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndAddRelease(Object...) getAndAddRelease(Object...)}.
		 */
		GET_AND_ADD_RELEASE("getAndAddRelease", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndBitwiseAnd(Object...) getAndBitwiseAnd(Object...)}.
		 */
		GET_AND_BITWISE_AND("getAndBitwiseAnd", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndBitwiseAndAcquire(Object...) getAndBitwiseAndAcquire(Object...)}.
		 */
		GET_AND_BITWISE_AND_ACQUIRE("getAndBitwiseAndAcquire", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndBitwiseAndRelease(Object...) getAndBitwiseAndRelease(Object...)}.
		 */
		GET_AND_BITWISE_AND_RELEASE("getAndBitwiseAndRelease", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndBitwiseOr(Object...) getAndBitwiseOr(Object...)}.
		 */
		GET_AND_BITWISE_OR("getAndBitwiseOr", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndBitwiseOrAcquire(Object...) getAndBitwiseOrAcquire(Object...)}.
		 */
		GET_AND_BITWISE_OR_ACQUIRE("getAndBitwiseOrAcquire", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndBitwiseOrRelease(Object...) getAndBitwiseOrRelease(Object...)}.
		 */
		GET_AND_BITWISE_OR_RELEASE("getAndBitwiseOrRelease", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndBitwiseXor(Object...) getAndBitwiseXor(Object...)}.
		 */
		GET_AND_BITWISE_XOR("getAndBitwiseXor", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndBitwiseXorAcquire(Object...) getAndBitwiseXorAcquire(Object...)}.
		 */
		GET_AND_BITWISE_XOR_ACQUIRE("getAndBitwiseXorAcquire", AccessType.GET_AND_UPDATE, true), //$NON-NLS-1$
		
		/**
		 * The {@link AccessMode} corresponding to {@link VarHandle#getAndBitwiseXorRelease(Object...) getAndBitwiseXorRelease(Object...)}.
		 */
		GET_AND_BITWISE_XOR_RELEASE("getAndBitwiseXorRelease", AccessType.GET_AND_UPDATE, true); //$NON-NLS-1$
		
		AccessType at;
		boolean isSetter;
		private String methodName;

		static final Map<String, AccessMode> methodNameToAccessMode;
		static {
			methodNameToAccessMode = new HashMap<>();
			for (AccessMode accessMode : values()) {
				methodNameToAccessMode.put(accessMode.methodName, accessMode);
			}
		}

		AccessMode(String methodName, AccessType signatureType, boolean isSetter) {
			this.methodName = methodName;
			this.at = signatureType;
			this.isSetter = isSetter;
		}
		
		/**
		 * @return The name of the method associated with this AccessMode.
		 */
		public String methodName() {
			return methodName;
		}

		/**
		 * Gets the AccessMode associated with the provided method name.
		 * 
		 * @param methodName The name of the method associated with the AccessMode being requested.
		 * @return The AccessMode associated with the provided method name.
		 */
		public static AccessMode valueFromMethodName(String methodName) {
			AccessMode accessMode = methodNameToAccessMode.get(methodName);
			if (accessMode != null) {
				return accessMode;
			}

			/*[MSG "K0633", "{0} is not a valid AccessMode."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0633", methodName)); //$NON-NLS-1$
		}
	}
	
	enum AccessType {
		GET(Object.class),
		SET(void.class),
		COMPARE_AND_SET(boolean.class),
		COMPARE_AND_EXCHANGE(Object.class),
		GET_AND_UPDATE(Object.class);

		final boolean isMonomorphicInReturnType;
		final Class<?> returnType;

		AccessType(Class<?> returnType) {
			this.returnType = returnType;
			this.isMonomorphicInReturnType = (returnType != Object.class);
		}

		/**
		 * Gets the MethodType associated with the AccessType.
		 * 
		 * This method gets invoked by the derived VarHandle classes through accessModeTypeUncached.
		 * 
		 * OpenJ9 only uses it to retrieve the receiver class, which is not available from VarForm.
		 * 
		 * @param receiver class of the derived VarHandle.
		 * @param type is the field type or value type.
		 * @param args is the list of intermediate argument classes in the derived VarHandle's
		 * AccessMode methods.
		 * @return the MethodType for the corresponding AccessType.
		 */
		MethodType accessModeType(Class<?> receiver, Class<?> type, Class<?>... args) {
			List<Class<?>> paramList = new ArrayList<>();
			Class<?> returnType = null;
			switch (this) {
			case GET:
				returnType = type;
				paramList.add(receiver);
				Collections.addAll(paramList, args);
				break;
			case SET:
				returnType = void.class;
				paramList.add(receiver);
				Collections.addAll(paramList, args);
				paramList.add(type);
				break;
			case COMPARE_AND_SET:
				returnType = boolean.class;
				paramList.add(receiver);
				Collections.addAll(paramList, args);
				Collections.addAll(paramList, type, type);
				break;
			case COMPARE_AND_EXCHANGE:
				returnType = type;
				paramList.add(receiver);
				Collections.addAll(paramList, args);
				Collections.addAll(paramList, type, type);
				break;
			case GET_AND_UPDATE:
				returnType = type;
				paramList.add(receiver);
				Collections.addAll(paramList, args);
				paramList.add(type);
				break;
			default:
				throw new InternalError("Invalid AccessType.");
			}
			return MethodType.methodType(returnType, paramList);
		}
	}
	
	static final Unsafe _unsafe = Unsafe.getUnsafe();
	static final Lookup _lookup = Lookup.IMPL_LOOKUP;

/*[IF Java14]*/
	static final BiFunction<String, List<Integer>, ArrayIndexOutOfBoundsException> AIOOBE_SUPPLIER = null;
	VarForm vform = null;
/*[ENDIF] Java14 */
	
/*[IF Java15]*/
	MethodHandle[] handleTable;
/*[ELSE]*/
	private final MethodHandle[] handleTable;
/*[ENDIF] Java15 */

	final Class<?> fieldType;
	final Class<?>[] coordinateTypes;
	final int modifiers;
/*[IF Java12]*/
	private int hashCode = 0;
/*[ENDIF] Java12 */
	
	/**
	 * Constructs a generic VarHandle instance. 
	 * 
	 * @param fieldType The type of the field referenced by this VarHandle.
	 * @param coordinateTypes An array of the argument types required to utilize the access modes on this VarHandle.
	 * @param handleTable An array of MethodHandles referencing the methods corresponding to this VarHandle's access modes.
	 * @param modifiers The field's modifiers.
	 */
	VarHandle(Class<?> fieldType, Class<?>[] coordinateTypes, MethodHandle[] handleTable, int modifiers) {
		this.fieldType = fieldType;
		this.coordinateTypes = coordinateTypes;
		this.handleTable = handleTable;
		this.modifiers = modifiers;
	}

/*[IF Java14]*/
	/**
	 * Constructs a generic VarHandle instance.
	 *
	 * @param varForm an instance of VarForm.
	 */
	VarHandle(VarForm varForm) {
		if (varForm.memberName_table == null) {
			/* Indirect VarHandle. */
			MethodType getter = varForm.methodType_table[VarHandle.AccessType.GET.ordinal()];
			this.fieldType = getter.returnType();
			this.coordinateTypes = getter.parameterArray();
			this.modifiers = 0;
			this.vform = varForm;
			return;
		} else {
			/* Direct VarHandle. */
			AccessMode[] accessModes = AccessMode.values();
			int numAccessModes = accessModes.length;
	
			/* The first argument in AccessType.GET MethodType is the receiver class. */
			Class<?> receiverActual = accessModeTypeUncached(AccessMode.GET).parameterType(0);
			Class<?> receiverVarForm = varForm.methodType_table[AccessType.GET.ordinal()].parameterType(0);
			
			/* Specify the exact operation method types if the actual receiver doesn't match the
			 * receiver derived from VarForm.
			 */
			MethodType[] operationMTsExact = null;
			if (receiverActual != receiverVarForm) {
				operationMTsExact = new MethodType[numAccessModes];
			}
			
			MethodType[] operationMTs = new MethodType[numAccessModes];
			Class<?> operationsClass = null;
	
			for (int i = 0; i < numAccessModes; i++) {
				MemberName memberName = varForm.memberName_table[i];
				if (memberName != null) {
					operationMTs[i] = memberName.getMethodType();
					if (operationMTsExact != null) {
						/* Replace with the actual receiver, which is expected when the operation method
						 * is invoked. The receiver is the second argument.
						 */
						operationMTsExact[i] = operationMTs[i].changeParameterType(1, receiverActual);
					}
					if (operationsClass == null) {
						operationsClass = memberName.getDeclaringClass();
					}
				}
			}
	
			MethodType getter = operationMTs[AccessMode.GET.ordinal()];
			MethodType setter = operationMTs[AccessMode.SET.ordinal()];
			
			if (operationMTsExact != null) {
				getter = operationMTsExact[AccessMode.GET.ordinal()];
				setter = operationMTsExact[AccessMode.SET.ordinal()];
			}
	
			this.fieldType = setter.parameterType(setter.parameterCount() - 1);
			
			/* The first VarHandle parameter type should be removed from the getter in order to derive
			 * the coordinate types. 
			 */
			Class<?>[] getterParams = getter.parameterArray();
			this.coordinateTypes = Arrays.copyOfRange(getterParams, 1, getterParams.length);
			
			if (operationMTsExact != null) {
				this.handleTable = populateMHsJEP370(operationsClass, operationMTs, operationMTsExact);
			} else {
				this.handleTable = populateMHsJEP370(operationsClass, operationMTs, operationMTs);
			}
			
			this.modifiers = 0;
			this.vform = varForm;
		}
	}

	/**
	 * This is derived from populateMHs in order to support the OpenJDK VarHandle operations classes, which
	 * don't extend the VarHandleOperations class. 
	 *
	 * @param operationsClass the class which has all AccessMode methods defined.
	 * @param lookupTypes the method types for the AccessMode methods in the operationsClass.
	 * @param exactType the exact method types to be expected when VarHandle AccessMode methods are invoked.
	 * @return a MethodHandle array which is used to initialize the handleTable.
	 */
	static MethodHandle[] populateMHsJEP370(Class<?> operationsClass, MethodType[] lookupTypes, MethodType[] exactTypes) {
		MethodHandle[] operationMHs = new MethodHandle[AccessMode.values().length];

		try {
			/* Lookup the MethodHandles corresponding to access modes. */
			for (AccessMode accessMode : AccessMode.values()) {
				int index = accessMode.ordinal();
				MethodType lookupType = lookupTypes[index];
				if (lookupType != null) {
					operationMHs[index] = _lookup.findStatic(operationsClass, accessMode.methodName(), lookupType);
					if (lookupTypes == exactTypes) {
						operationMHs[index] = permuteHandleJ9ToReference(operationMHs[index]);
					} else {
						/* Clone the MethodHandles with the exact types if different set of exactTypes are provided. */
						MethodType exactType = exactTypes[index];
						if (exactType != null) {
							/*[IF OPENJDK_METHODHANDLES]*/
							MethodHandle operationMH = operationMHs[index];
							operationMHs[index] = operationMH.copyWith(exactType, operationMH.form);
							/*[ELSE]*/
							operationMHs[index] = operationMHs[index].cloneWithNewType(exactType);
							/*[ENDIF] OPENJDK_METHODHANDLES */
						}
						operationMHs[index] = permuteHandleJ9ToReference(operationMHs[index]);
					}
				}
			}
		} catch (IllegalAccessException | NoSuchMethodException e) {
			/* [MSG "K0623", "Unable to create MethodHandle to VarHandle operation."] */
			InternalError error = new InternalError(com.ibm.oti.util.Msg.getString("K0623")); //$NON-NLS-1$
			error.initCause(e);
			throw error;
		}

		return operationMHs;
	}

	/**
	 * Generate a MethodHandle which translates:
	 *     FROM {Receiver, Intermediate ..., Value, VarHandle}ReturnType
	 *     TO   {VarHandle, Receiver, Intermediate ..., Value}ReturnType
	 *     
	 * @param methodHandle to be permuted.
	 * @return the adapter MethodHandle which performs the translation.
	 */
	static MethodHandle permuteHandleJ9ToReference(MethodHandle methodHandle) {
		/* HandleType = {VarHandle, Receiver, Intermediate ..., Value}
		 * PermuteType = {Receiver, Intermediate ..., Value, VarHandle}
		 */
		MethodType permuteMethodType = methodHandle.type();
		int parameterCount = permuteMethodType.parameterCount();
		Class<?> firstParameter = permuteMethodType.parameterType(0);
		permuteMethodType = permuteMethodType.dropParameterTypes(0, 1);
		permuteMethodType = permuteMethodType.appendParameterTypes(firstParameter);

		/* reorder specifies the mapping between PermuteType and HandleType
		 * reorder = {parameterCount - 1, 0, 1, 2, ..., parameterCount - 2}
		 */
		int[] reorder = new int[parameterCount];
		if (parameterCount > 0) {
			reorder[0] = parameterCount - 1;
			for (int i = 1; i < parameterCount; i++) {
				reorder[i] = i - 1;
			}
		}

		return MethodHandles.permuteArguments(methodHandle, permuteMethodType, reorder);
	}
/*[ENDIF] Java14 */

/*[IF Java15]*/
	/**
	 * Generate a MethodHandle which translates:
	 *     FROM {VarHandle, Receiver, Intermediate ..., Value}ReturnType
	 *     TO   {Receiver, Intermediate ..., Value, VarHandle}ReturnType
	 *
	 * @param methodHandle to be permuted.
	 * @return the adapter MethodHandle which performs the translation.
	 */
	static MethodHandle permuteHandleReferenceToJ9(MethodHandle methodHandle) {
		/* HandleType = {Receiver, Intermediate ..., Value, VarHandle}
		 * PermuteType = {VarHandle, Receiver, Intermediate ..., Value}
		 */
		MethodType permuteMethodType = methodHandle.type();
		int parameterCount = permuteMethodType.parameterCount();
		Class<?>[] params = permuteMethodType.parameterArray();
		int[] reorder = new int[parameterCount];
		
		if (parameterCount > 0) {
			permuteMethodType = permuteMethodType.changeParameterType(0, params[parameterCount - 1]);
			for (int i = 1; i < parameterCount; i++) {
				permuteMethodType = permuteMethodType.changeParameterType(i, params[i - 1]);
			}
			
			/**
			 * reorder specifies the mapping between PermuteType and HandleType.
			 * reorder = {1, 2, ..., parameterCount - 1, 0}
			 */
			reorder[parameterCount - 1] = 0;
			for (int i = 0; i < parameterCount - 1; i++) {
				reorder[i] = i + 1;
			}
		}
		
		return MethodHandles.permuteArguments(methodHandle, permuteMethodType, reorder);
	}

	/**
	 * Generates handleTable for Indirect VarHandles (supports JEP383).
	 *
	 * @param target the target VarHandle for the Indirect VarHandle.
	 * @param handleFactory used to transform an AccessMode MethodHandle accordingly.
	 * 
	 * @return a MethodHandle array which is used to initialize the handleTable.
	 */
	static MethodHandle[] populateMHsJEP383(VarHandle target, BiFunction<AccessMode, MethodHandle, MethodHandle> handleFactory) {
		MethodHandle[] operationMHs = new MethodHandle[AccessMode.values().length];

		try {
			MethodType replaceWithDirectType = MethodType.methodType(VarHandle.class, VarHandle.class);
			MethodHandle replaceWithDirect = MethodHandles.lookup().findStatic(VarHandle.class, "asDirect", replaceWithDirectType);

			for (AccessMode mode : AccessMode.values()) {
				int index = mode.ordinal();
				MethodHandle targetHandle = target.getMethodHandle(index);

				if (targetHandle != null) {
					MethodType targetType = targetHandle.type();
					if (targetType.parameterType(targetType.parameterCount() - 1) == VarHandle.class) {
						/*
						 * Permute a J9 VarHandle to an equivalent reference implementation VarHandle
						 * before applying handleFactory.
						 */
						targetHandle = permuteHandleReferenceToJ9(targetHandle);
					}
					operationMHs[index] = handleFactory.apply(mode, targetHandle);
					operationMHs[index] = permuteHandleJ9ToReference(operationMHs[index]);
					operationMHs[index] = MethodHandles.collectArguments(operationMHs[index], operationMHs[index].type().parameterCount() - 1, replaceWithDirect);
				}
			}
		} catch (IllegalAccessException | NoSuchMethodException e) {
			/* [MSG "K0623", "Unable to create MethodHandle to VarHandle operation."] */
			InternalError error = new InternalError(com.ibm.oti.util.Msg.getString("K0623")); //$NON-NLS-1$
			error.initCause(e);
			throw error;
		}

		return operationMHs;
	}
/*[ENDIF] Java15 */

	Class<?> getDefiningClass() {
		/*[MSG "K0627", "Expected override of this method."]*/
		throw new InternalError(com.ibm.oti.util.Msg.getString("K0627")); //$NON-NLS-1$
	}
	
	String getFieldName() {
		/*[MSG "K0627", "Expected override of this method."]*/
		throw new InternalError(com.ibm.oti.util.Msg.getString("K0627")); //$NON-NLS-1$
	}
	
	int getModifiers() {
		return modifiers;
	}
	
	/**
	 * Get the type of the field this {@link VarHandle} refers to.
	 * 
	 * @return The field type
	 */
	/*[IF Java15]*/
	public Class<?> varType() {
	/*[ELSE]*/
	public final Class<?> varType() {
	/*[ENDIF] Java15 */
		return this.fieldType;
	}
	
	/**
	 * Different parameters are required in order to access the field referenced
	 * by this {@link VarHandle} depending on the type of the {@link VarHandle}, 
	 * e.g. instance field, static field, or array element. This method provides
	 * a list of the access parameters for this {@link VarHandle} instance.
	 * 
	 * @return The parameters required to access the field.
	 */
	/*[IF Java15]*/
	public List<Class<?>> coordinateTypes() {
	/*[ELSE]*/
	public final List<Class<?>> coordinateTypes() {
	/*[ENDIF] Java15 */
		return Collections.unmodifiableList(Arrays.<Class<?>>asList(coordinateTypes));
	}

/*[IF Java12]*/
	/**
	 * Returns the nominal descriptor of this VarHandle instance, or an empty Optional 
	 * if construction is not possible.
	 * 
	 * @return Optional with a nominal descriptor of VarHandle instance
	 */
	public Optional<VarHandleDesc> describeConstable() {
		/* this method should be overridden by types that are supported */
		return Optional.empty();
	}

	/**
	 * Compares the specified object to this VarHandle and answer if they are equal.
	 *
	 * @param obj the object to compare
	 * @return true if the specified object is equal to this VarHandle, false otherwise
	 */
	@Override
	public boolean equals(Object obj) {
		if (this == obj) {
			return true;
		}

		if (!(obj instanceof VarHandle)) {
			return false;
		}

		/* argument comparison */
		VarHandle that = (VarHandle)obj;
		if (!(this.fieldType.equals(that.fieldType) 
			&& (this.modifiers == that.modifiers)
			&& Arrays.equals(this.coordinateTypes, that.coordinateTypes))
		) {
			return false;
		}

		/* Compare properties of FieldVarHandle that are not captured in the parent class */
		if (obj instanceof FieldVarHandle) {
			if (!(this instanceof FieldVarHandle)) {
				return false;
			}

			FieldVarHandle thisf = (FieldVarHandle)this;
			FieldVarHandle thatf = (FieldVarHandle)obj;
			if (!(thisf.definingClass.equals(thatf.definingClass)
				&& thisf.fieldName.equals(thatf.fieldName))
			) {
				return false;
			}
			
		}

		return true;
	}

	/**
	 * Answers an integer hash code for the VarHandle. VarHandle instances
	 * which are equal answer the same value for this method.
	 *
	 * @return a hash for this VarHandle
	 */
	@Override
	public int hashCode() {
		if (hashCode == 0) {
			hashCode = fieldType.hashCode();
			for (Class<?> c : coordinateTypes) {
				hashCode = 31 * hashCode + c.hashCode();
			}

			/* Add properties for FieldVarHandle */
			if (this instanceof FieldVarHandle) {
				hashCode = 31 * hashCode + ((FieldVarHandle)this).definingClass.hashCode();
			}
		}
		return hashCode;
	}

	/**
	 * Returns a text representation of the VarHandle instance.
	 * 
	 * @return String representation of VarHandle instance
	 */
	@Override
	public final String toString() {
		/* VarHandle[varType=x, coord=[x, x]] */
		String structure = "VarHandle[varType=%s, coord=%s]"; //$NON-NLS-1$
		String coordList = Arrays.toString(coordinateTypes);
		return String.format(structure, this.fieldType.getName(), coordList);
	}
/*[ENDIF]*/
	
	/**
	 * Each {@link AccessMode}, e.g. get and set, requires different parameters
	 * in addition to the {@link VarHandle#coordinateTypes() coordinateTypes()}. 
	 * This method provides the {@link MethodType} for a specific {@link AccessMode},
	 * including the {@link VarHandle#coordinateTypes() coordinateTypes()}.
	 * 
	 * @param accessMode The {@link AccessMode} to get the {@link MethodType} for. 
	 * @return The {@link MethodType} corresponding to the provided {@link AccessMode}.
	 */
	public final MethodType accessModeType(AccessMode accessMode) {
		MethodType modifiedType = null;
		MethodHandle internalHandle = handleTable[accessMode.ordinal()];
		if (internalHandle == null) {
			modifiedType = accessModeTypeUncached(accessMode);
		} else {
			MethodType internalType = internalHandle.type();
			int numOfArguments = internalType.parameterCount();

			/* Drop the internal VarHandle argument. */
			modifiedType = internalType.dropParameterTypes(numOfArguments - 1, numOfArguments);
		}
		return modifiedType;
	}
	
	/**
	 * This is a helper method to allow sub-class ViewVarHandle provide its own implementation
	 * while making isAccessModeSupported(accessMode) final to satisfy signature tests.
	 * 
	 * @param accessMode The {@link AccessMode} to check support for.
	 * @return A boolean value indicating whether the {@link AccessMode} is supported.
	 */
	boolean isAccessModeSupportedHelper(AccessMode accessMode) {
/*[IF Java14]*/
		if (vform != null) {
			return (handleTable[accessMode.ordinal()] != null);
		}
/*[ENDIF] Java14 */
		
		switch (accessMode) {
		case GET:
		case GET_VOLATILE:
		case GET_OPAQUE:
		case GET_ACQUIRE:
			return true;
		case SET:
		case SET_VOLATILE:
		case SET_OPAQUE:
		case SET_RELEASE:
		case COMPARE_AND_SET:
		case COMPARE_AND_EXCHANGE_ACQUIRE:
		case COMPARE_AND_EXCHANGE_RELEASE:
		case COMPARE_AND_EXCHANGE:
		case WEAK_COMPARE_AND_SET:
		case WEAK_COMPARE_AND_SET_ACQUIRE:
		case WEAK_COMPARE_AND_SET_RELEASE:
		case WEAK_COMPARE_AND_SET_PLAIN:
		case GET_AND_SET:
		case GET_AND_SET_ACQUIRE:
		case GET_AND_SET_RELEASE:
			return !Modifier.isFinal(modifiers);
		case GET_AND_ADD:
		case GET_AND_ADD_ACQUIRE:
		case GET_AND_ADD_RELEASE:
			return ((!Modifier.isFinal(modifiers)) && fieldType.isPrimitive() && (boolean.class != fieldType));
		case GET_AND_BITWISE_AND:
		case GET_AND_BITWISE_AND_ACQUIRE:
		case GET_AND_BITWISE_AND_RELEASE:
		case GET_AND_BITWISE_OR:
		case GET_AND_BITWISE_OR_ACQUIRE:
		case GET_AND_BITWISE_OR_RELEASE:
		case GET_AND_BITWISE_XOR:
		case GET_AND_BITWISE_XOR_ACQUIRE:
		case GET_AND_BITWISE_XOR_RELEASE:
			return ((!Modifier.isFinal(modifiers)) && (double.class != fieldType) && (float.class != fieldType) && fieldType.isPrimitive());
		default:
			throw new InternalError("Invalid AccessMode"); //$NON-NLS-1$
		}
	}
	
	/**
	 * Not all AccessMode are supported for all {@link VarHandle} instances, e.g. 
	 * because of the field type and/or field modifiers. This method indicates whether 
	 * a specific {@link AccessMode} is supported by by this {@link VarHandle} instance.
	 * 
	 * @param accessMode The {@link AccessMode} to check support for.
	 * @return A boolean value indicating whether the {@link AccessMode} is supported.
	 */
	public final boolean isAccessModeSupported(AccessMode accessMode) {
		return isAccessModeSupportedHelper(accessMode);
	}
	
	/**
	 * This method creates a {@link MethodHandle} for a specific {@link AccessMode}.
	 * The {@link MethodHandle} is bound to this {@link VarHandle} instance.
	 * 
	 * @param accessMode The {@link AccessMode} to create a {@link MethodHandle} for.
	 * @return A {@link MethodHandle} for the specified {@link AccessMode}, bound to
	 * 			this {@link VarHandle} instance.
	 */
	/*[IF Java15]*/
	public MethodHandle toMethodHandle(AccessMode accessMode) {
	/*[ELSE]*/
	public final MethodHandle toMethodHandle(AccessMode accessMode) {
	/*[ENDIF] Java15 */
		MethodHandle mh = handleTable[accessMode.ordinal()];

		if (mh != null) {
			mh = MethodHandles.insertArguments(mh, mh.type().parameterCount() - 1, this);
		}

		if ((mh == null) || !isAccessModeSupported(accessMode)) {
			MethodType mt = null;

			if (mh != null) {
				mt = mh.type();
			} else {
				mt = accessModeTypeUncached(accessMode);
				/* accessModeTypeUncached does not return null. It throws InternalError if the method type
				 * cannot be determined.
				 */
				mt = mt.appendParameterTypes(this.getClass());
			}

			try {
				mh = _lookup.findStatic(VarHandle.class, "throwUnsupportedOperationException", MethodType.methodType(void.class)); //$NON-NLS-1$
			} catch (Throwable e) {
				InternalError error = new InternalError("The method throwUnsupportedOperationException should be accessible and defined in this class."); //$NON-NLS-1$
				error.initCause(e);
				throw error;
			}

			/* The resulting method handle must come with the same signature as the requested access mode method
			 * so as to throw out UnsupportedOperationException from that method.
			 */
			mh = mh.asType(MethodType.methodType(mt.returnType));
			mh = MethodHandles.dropArguments(mh, 0, mt.arguments);
		}
		
		return mh;
	}
	
	private final static void throwUnsupportedOperationException() {
		/*[MSG "K0629", "Modification access modes are not allowed on final fields."]*/
		throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0629")); //$NON-NLS-1$
	}
	
	/**
	 * Inserts a complete memory fence, ensuring that no loads/stores before this
	 * fence are reordered with any loads/stores after the fence.
	 */
	public static void fullFence() {
		_unsafe.fullFence();
	}
	
	/**
	 * Inserts an acquire memory fence, ensuring that no loads before this fence
	 * are reordered with any loads/stores after the fence. 
	 */
	public static void acquireFence() {
		// TODO: loadLoad + loadStore
		_unsafe.loadFence();
	}
	
	/**
	 * Inserts a release memory fence, ensuring that no stores before this fence
	 * are reordered with any loads/stores after the fence. 
	 */
	public static void releaseFence() {
		// TODO: storeStore + loadStore
		_unsafe.storeFence();
	}
	
	/**
	 * Inserts a load memory fence, ensuring that no loads before the fence are
	 * reordered with any loads after the fence.
	 */
	public static void loadLoadFence() {
		_unsafe.loadLoadFence();
	}
	
	/**
	 * Inserts a store memory fence, ensuring that no stores before the fence are
	 * reordered with any stores after the fence.
	 */
	public static void storeStoreFence() {
		_unsafe.storeStoreFence();
	}
	
	/*[IF ]*/
	/* Methods with native send target */
	/*[ENDIF]*/
	/**
	 * Gets the value of the field referenced by this {@link VarHandle}. 
	 * This is a non-volatile operation.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field
	 */
	public final native @PolymorphicSignature Object get(Object... args);
	
	/**
	 * Sets the value of the field referenced by this {@link VarHandle}. 
	 * This is a non-volatile operation.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 */
	public final native @PolymorphicSignature void set(Object... args);
	
	/**
	 * Atomically gets the value of the field referenced by this {@link VarHandle}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field
	 */
	public final native @PolymorphicSignature Object getVolatile(Object... args);
	
	/**
	 * Atomically sets the value of the field referenced by this {@link VarHandle}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 */
	public final native @PolymorphicSignature void setVolatile(Object... args);
	
	/**
	 * Gets the value of the field referenced by this {@link VarHandle}.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field
	 */
	public final native @PolymorphicSignature Object getOpaque(Object... args);
	
	/**
	 * Sets the value of the field referenced by this {@link VarHandle}.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 */
	public final native @PolymorphicSignature void setOpaque(Object... args);
	
	/**
	 * Gets the value of the field referenced by this {@link VarHandle} using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field
	 */
	public final native @PolymorphicSignature Object getAcquire(Object... args);
		
	/**
	 * Sets the value of the field referenced by this {@link VarHandle} using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 */
	public final native @PolymorphicSignature void setRelease(Object... args);
	
	/**
	 * Atomically sets the value of the field referenced by this {@link VarHandle} if the
	 * field value before this operation equaled the expected value.
	 * The get operation has the memory semantics of {@link VarHandle#getVolatile(Object...) getVolatile(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setVolatile(Object...) setVolatile(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return A boolean value indicating whether the field was updated
	 */
	public final native @PolymorphicSignature boolean compareAndSet(Object... args);
	
	/**
	 * Atomically sets the value of the field referenced by this {@link VarHandle} if the
	 * field value before this operation equaled the expected value.
	 * The get operation has the memory semantics of {@link VarHandle#getVolatile(Object...) getVolatile(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setVolatile(Object...) setVolatile(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value of the field before this operation
	 */
	public final native @PolymorphicSignature Object compareAndExchange(Object... args);
	
	/**
	 * Atomically sets the value of the field referenced by this {@link VarHandle} if the
	 * field value before this operation equaled the expected value.
	 * The get operation has the memory semantics of {@link VarHandle#getAcquire(Object...) getAcquire(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#set(Object...) set(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value of the field before this operation
	 */
	public final native @PolymorphicSignature Object compareAndExchangeAcquire(Object... args);
	
	/**
	 * Atomically sets the value of the field referenced by this {@link VarHandle} if the
	 * field value before this operation equaled the expected value.
	 * The get operation has the memory semantics of {@link VarHandle#get(Object...) get(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setRelease(Object...) setRelease(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value of the field before this operation
	 */
	public final native @PolymorphicSignature Object compareAndExchangeRelease(Object... args);
	
	/**
	 * Sets the value of the field referenced by this {@link VarHandle} if the
	 * field value before this operation equaled the expected value.
	 * The get operation has the memory semantics of {@link VarHandle#get(Object...) get(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#set(Object...) set(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return A boolean value indicating whether the field was updated
	 */
	public final native @PolymorphicSignature boolean weakCompareAndSet(Object... args);
	
	/**
	 * Sets the value of the field referenced by this {@link VarHandle} if the
	 * field value before this operation equaled the expected value.
	 * The get operation has the memory semantics of {@link VarHandle#getAcquire(Object...) getAcquire(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#set(Object...) set(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return A boolean value indicating whether the field was updated
	 */
	public final native @PolymorphicSignature boolean weakCompareAndSetAcquire(Object... args);
	
	/**
	 * Sets the value of the field referenced by this {@link VarHandle} if the
	 * field value before this operation equaled the expected value.
	 * The get operation has the memory semantics of {@link VarHandle#get(Object...) get(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setRelease(Object...) setRelease(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return A boolean value indicating whether the field was updated
	 */
	public final native @PolymorphicSignature boolean weakCompareAndSetRelease(Object... args);
	
	/**
	 * Sets the value of the field referenced by this {@link VarHandle} if the
	 * field value before this operation equaled the expected value.
	 * The get operation has the memory semantics of {@link VarHandle#get(Object...) get(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#set(Object...) set(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return A boolean value indicating whether the field was updated
	 */
	public final native @PolymorphicSignature boolean weakCompareAndSetPlain(Object... args);
	
	/**
	 * Atomically sets the value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getVolatile(Object...) getVolatile(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setVolatile(Object...) setVolatile(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndSet(Object... args);
	
	/**
	 * Atomically sets the value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getAcquire(Object...) getAcquire(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#set(Object...) set(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndSetAcquire(Object... args);
	
	/**
	 * Atomically sets the value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#set(Object...) get(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setRelease(Object...) setRelease(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndSetRelease(Object... args);
	
	/**
	 * Atomically adds to the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getVolatile(Object...) getVolatile(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setVolatile(Object...) setVolatile(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndAdd(Object... args);
	
	/**
	 * Atomically adds to the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getAcquire(Object...) getAcquire(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#set(Object...) set(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndAddAcquire(Object... args);
	
	/**
	 * Atomically adds to the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#get(Object...) get(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setRelease(Object...) setRelease(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndAddRelease(Object... args);
	
	/**
	 * Atomically ANDs the provided value and the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getVolatile(Object...) getVolatile(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setVolatile(Object...) setVolatile(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndBitwiseAnd(Object... args);
	
	/**
	 * Atomically ANDs the provided value and the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getAcquire(Object...) getAcquire(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#set(Object...) set(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndBitwiseAndAcquire(Object... args);
	
	/**
	 * Atomically ANDs the provided value and the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#get(Object...) (Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setRelease(Object...) setRelease(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndBitwiseAndRelease(Object... args);
	
	/**
	 * Atomically ORs the provided value and the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getVolatile(Object...) getVolatile(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setVolatile(Object...) setVolatile(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndBitwiseOr(Object... args);
	
	/**
	 * Atomically ORs the provided value and the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getAcquire(Object...) getAcquire(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#set(Object...) set(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndBitwiseOrAcquire(Object... args);
	
	/**
	 * Atomically ORs the provided value and the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#get(Object...) get(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setRelease(Object...) setRelease(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndBitwiseOrRelease(Object... args);
	
	/**
	 * Atomically XORs the provided value and the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getVolatile(Object...) getVolatile(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setVolatile(Object...) setVolatile(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndBitwiseXor(Object... args);
	
	/**
	 * Atomically XORs the provided value and the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#getAcquire(Object...) getAcquire(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#set(Object...) set(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndBitwiseXorAcquire(Object... args);
	
	/**
	 * Atomically XORs the provided value and the current value of the field referenced by this {@link VarHandle}
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of {@link VarHandle#get(Object...) get(Object...)}.
	 * The set operation has the memory semantics of {@link VarHandle#setRelease(Object...) setRelease(Object...)}.
	 * 
	 * @param args The arguments for this operation are determined by the field type
	 * 				(see {@link VarHandle#accessModeType(AccessMode) accessModeType()}) 
	 * 				and the access parameters (see {@link VarHandle#coordinateTypes() coordinateTypes()}).
	 * @return The value stored in the field prior to the update
	 */
	public final native @PolymorphicSignature Object getAndBitwiseXorRelease(Object... args);
	
	static MethodHandle[] populateMHs(
			Class<? extends VarHandleOperations> operationsClass, MethodType[] lookupTypes, MethodType[] exactTypes) {
		MethodHandle[] operationMHs = new MethodHandle[AccessMode.values().length];
		
		try {
			/* Lookup the MethodHandles corresponding to access modes. */
			for (AccessMode accessMode : AccessMode.values()) {
				int index = accessMode.ordinal();
				operationMHs[index] = _lookup.findStatic(operationsClass, accessMode.methodName(), lookupTypes[index]);
			}
			
			/* If we provided a different set of exactTypes, clone the MethodHandles with the exact types. */
			if (lookupTypes != exactTypes) {
				for (AccessMode accessMode : AccessMode.values()) {
					int index = accessMode.ordinal();
					/*[IF OPENJDK_METHODHANDLES]*/
					MethodHandle operationMH = operationMHs[index];
					operationMHs[index] = operationMH.copyWith(exactTypes[index], operationMH.form);
					/*[ELSE]*/
					operationMHs[index] = operationMHs[index].cloneWithNewType(exactTypes[index]);
					/*[ENDIF] OPENJDK_METHODHANDLES */
				}
			}
		} catch (IllegalAccessException | NoSuchMethodException e) {
			/*[MSG "K0623", "Unable to create MethodHandle to VarHandle operation."]*/
			InternalError error = new InternalError(com.ibm.oti.util.Msg.getString("K0623")); //$NON-NLS-1$
			error.initCause(e);
			throw error;
		}
		
		return operationMHs;
	}
	
	static MethodType[] populateMTs(MethodType getter, MethodType setter, MethodType compareAndSet, MethodType compareAndExchange, MethodType getAndSet) {
		MethodType[] operationMTs = new MethodType[AccessMode.values().length];
		
		operationMTs[AccessMode.GET.ordinal()] = getter;
		operationMTs[AccessMode.SET.ordinal()] = setter;
		operationMTs[AccessMode.GET_VOLATILE.ordinal()] = getter;
		operationMTs[AccessMode.SET_VOLATILE.ordinal()] = setter;
		operationMTs[AccessMode.GET_OPAQUE.ordinal()] = getter;
		operationMTs[AccessMode.SET_OPAQUE.ordinal()] = setter;
		operationMTs[AccessMode.GET_ACQUIRE.ordinal()] = getter;
		operationMTs[AccessMode.SET_RELEASE.ordinal()] = setter;
		operationMTs[AccessMode.COMPARE_AND_SET.ordinal()] = compareAndSet;
		operationMTs[AccessMode.COMPARE_AND_EXCHANGE.ordinal()] = compareAndExchange;
		operationMTs[AccessMode.COMPARE_AND_EXCHANGE_ACQUIRE.ordinal()] = compareAndExchange;
		operationMTs[AccessMode.COMPARE_AND_EXCHANGE_RELEASE.ordinal()] = compareAndExchange;
		operationMTs[AccessMode.WEAK_COMPARE_AND_SET.ordinal()] = compareAndSet;
		operationMTs[AccessMode.WEAK_COMPARE_AND_SET_ACQUIRE.ordinal()] = compareAndSet;
		operationMTs[AccessMode.WEAK_COMPARE_AND_SET_RELEASE.ordinal()] = compareAndSet;
		operationMTs[AccessMode.WEAK_COMPARE_AND_SET_PLAIN.ordinal()] = compareAndSet;
		operationMTs[AccessMode.GET_AND_SET.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_SET_ACQUIRE.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_SET_RELEASE.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_ADD.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_ADD_ACQUIRE.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_ADD_RELEASE.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_BITWISE_AND.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_BITWISE_AND_ACQUIRE.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_BITWISE_AND_RELEASE.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_BITWISE_OR.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_BITWISE_OR_ACQUIRE.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_BITWISE_OR_RELEASE.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_BITWISE_XOR.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_BITWISE_XOR_ACQUIRE.ordinal()] = getAndSet;
		operationMTs[AccessMode.GET_AND_BITWISE_XOR_RELEASE.ordinal()] = getAndSet;
		
		return operationMTs;
	}
	
	static class VarHandleOperations {
		static UnsupportedOperationException operationNotSupported(VarHandle varHandle) {
			/*[MSG "K0620", "This VarHandle operation is not supported by type {0}."]*/
			return new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0620", varHandle.fieldType)); //$NON-NLS-1$
		}
	}

	MethodHandle getFromHandleTable(int operation) {
		return handleTable[operation];
	}

/*[IF Java12]*/
	/* nominal descriptor of a VarHandle constant */
	public static final class VarHandleDesc extends DynamicConstantDesc<VarHandle> implements ConstantDesc {
		private Kind type = null;

		private static enum Kind {
			ARRAY,
			STATIC_FIELD,
			INSTANCE_FIELD;

			DirectMethodHandleDesc getBootstrap() {
				DirectMethodHandleDesc result = null;
				switch (this) {
					case ARRAY:
						result = ConstantDescs.BSM_VARHANDLE_ARRAY;
						break;
					case STATIC_FIELD:
						result = ConstantDescs.BSM_VARHANDLE_STATIC_FIELD;
						break;
					case INSTANCE_FIELD:
						result = ConstantDescs.BSM_VARHANDLE_FIELD;
						break;
				}
				return result;
			}

			ConstantDesc[] getBootstrapArgs(ClassDesc declaringClassDesc, ClassDesc varHandleDesc) {
				ConstantDesc[] result = null;
				switch(this) {
					case ARRAY:
						result = new ConstantDesc[] { varHandleDesc };
						break;
					case STATIC_FIELD:
					case INSTANCE_FIELD:
						result = new ConstantDesc[] { declaringClassDesc, varHandleDesc };
						break;
				}
				return result;
			}

			boolean isField() {
				return !this.equals(ARRAY);
			}
		}

		/**
		 * Create a nominal descriptor for a VarHandle.
		 * 
		 * @param type kind of VarHandleDesc to be created
		 * @param declaringClassDesc ClassDesc describing the declaring class for the field (null for array)
		 * @param name unqualified String name of the field ("_" for array)
		 * @param varHandleType ClassDesc describing the field or array type
		 * @throws NullPointerException if name argument is null
		 */
		private VarHandleDesc(Kind type, ClassDesc declaringClassDesc, String name, ClassDesc varHandleDesc) throws NullPointerException {
			super(type.getBootstrap(), name, ConstantDescs.CD_VarHandle, type.getBootstrapArgs(declaringClassDesc, varHandleDesc));
			this.type = type;
		}

		/**
		 * Creates a VarHandleDesc describing a VarHandle for an array type.
		 * 
		 * @param arrayClassDesc ClassDesc describing the array type
		 * @return VarHandleDesc describing a VarHandle for an array type
		 * @throws NullPointerException if there is a null argument
		 */
		public static VarHandleDesc ofArray(ClassDesc arrayClassDesc) throws NullPointerException {
			/* Verify that arrayClass is an array. This also covers the null check. */
			if (!arrayClassDesc.isArray()) {
				/*[MSG "K0625", "{0} is not an array type."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0625", arrayClassDesc.descriptorString())); //$NON-NLS-1$
			}
			return new VarHandleDesc(Kind.ARRAY, null, "_", arrayClassDesc); //$NON-NLS-1$
		}

		/**
		 * Creates a VarHandleDesc describing an instance field.
		 * 
		 * @param declaringClassDesc ClassDesc describing the declaring class for the field
		 * @param name unqualified String name of the field
		 * @param fieldTypeDesc ClassDesc describing the field type
		 * @return VarHandleDesc describing the instance field
		 * @throws NullPointerException if there is a null argument
		 */
		public static VarHandleDesc ofField(ClassDesc declaringClassDesc, String name, ClassDesc fieldTypeDesc) throws NullPointerException {
			/* name null check is in the constructor */
			Objects.requireNonNull(declaringClassDesc);
			Objects.requireNonNull(fieldTypeDesc);
			return new VarHandleDesc(Kind.INSTANCE_FIELD, declaringClassDesc, name, fieldTypeDesc);
		}

		/**
		 * Creates a VarHandleDesc describing a static field.
		 * 
		 * @param declaringClassDesc ClassDesc describing the declaring class for the field
		 * @param name unqualified String name of the field
		 * @param fieldTypeDesc ClassDesc describing the field type
		 * @return VarHandleDesc describing the static field
		 * @throws NullPointerException if there is a null argument
		 */
		public static VarHandleDesc ofStaticField(ClassDesc declaringClassDesc, String name, ClassDesc fieldTypeDesc) throws NullPointerException {
			/* name null check is in the constructor */
			Objects.requireNonNull(declaringClassDesc);
			Objects.requireNonNull(fieldTypeDesc);
			return new VarHandleDesc(Kind.STATIC_FIELD, declaringClassDesc, name, fieldTypeDesc);
		}

		/**
		 * Resolve nominal VarHandle descriptor reflectively.
		 * 
		 * @param lookup provides name resolution and access control context
		 * @return resolved VarHandle
		 * @throws ReflectiveOperationException if field, class, or method could not be resolved
		 */
		@Override
		public VarHandle resolveConstantDesc(MethodHandles.Lookup lookup) throws ReflectiveOperationException {
			VarHandle result;

			switch(type) {
				case ARRAY:
					Class<?> arrayClass = (Class<?>)getArrayTypeClassDesc().resolveConstantDesc(lookup);
					result = MethodHandles.arrayElementVarHandle(arrayClass);
					break;
				case STATIC_FIELD:
				case INSTANCE_FIELD:
					/* resolve declaring class, field descriptor and field name */
					Class<?> declaringClass = (Class<?>)getFieldDeclaringClassDesc().resolveConstantDesc(lookup);
					Class<?> fieldClass = (Class<?>)getFieldTypeClassDesc().resolveConstantDesc(lookup);
					String name = constantName();

					try {
						if (type.equals(Kind.STATIC_FIELD)) {
							result = lookup.findStaticVarHandle(declaringClass, name, fieldClass);
						} else {
							result = lookup.findVarHandle(declaringClass, name, fieldClass);
						}
					} catch(Exception e) {
						throw new ReflectiveOperationException(e);
					}
					break;
				default:
					/*[MSG "K0619", "Malformated VarHandleDesc, {0} could not be retrieved."]*/
					throw new InternalError(com.ibm.oti.util.Msg.getString("K0619", "field type")); //$NON-NLS-1$ //$NON-NLS-2$
			}
			
			return result;
		}

		/**
		 * Returns a string containing a concise, human-readable representation of the VarHandleDesc. 
		 * Description includes the bootstrap method, name, type, and bootstrap arguments.
		 *
		 * @return String that textually represents the Object instance
		 */
		@Override
		public String toString() {
			/* VarHandleDesc[(static) (declaring class name).(constant field):(var type)] */
			String structure = "VarHandleDesc[%s]"; //$NON-NLS-1$
			String content = ""; //$NON-NLS-1$

			if (type.equals(Kind.ARRAY)) {
				/* array type is declaring class, varType is the component type of the array */
				content = String.format("%s[]", getArrayTypeClassDesc().displayName()); //$NON-NLS-1$
			} else {
				if (type.equals(Kind.STATIC_FIELD)) {
					content += "static ";  //$NON-NLS-1$
				}
				content = String.format(content + "%s.%s:%s", getFieldDeclaringClassDesc().displayName(), //$NON-NLS-1$
					constantName(), getFieldTypeClassDesc().displayName());
			}

			return String.format(structure, content);
		}

		/**
		 * Returns a ClassDesc describing the variable represented by this VarHandleDesc.
		 * 
		 * @return ClassDesc of variable type
		 */
		public ClassDesc varType() {
			return type.isField() ? getFieldTypeClassDesc() : getArrayTypeClassDesc().componentType();
		}

		/**
		 * Helper to retrieve declaring class descriptor from superclass bootstrap arguments.
		 * 
		 * @return ClassDesc declaring class descriptor
		 * @throws InternalError if VarHandleDesc was created improperly or instance is not a field
		 */
		private ClassDesc getFieldDeclaringClassDesc() {
			ConstantDesc[] args = bootstrapArgs();
			/* declaring class is always the first bootstrap argument */
			if (!type.isField() || (args.length < 2) || !(args[0] instanceof ClassDesc)) {
				/*[MSG "K0619", "Malformated VarHandleDesc, {0} could not be retrieved."]*/
				throw new InternalError(com.ibm.oti.util.Msg.getString("K0619", "field declaring class descriptor")); //$NON-NLS-1$ //$NON-NLS-2$
			}
			return (ClassDesc)args[0];
		}

		/**
		 * Helper to retrieve field type descriptor from superclass bootstrap arguments.
		 * 
		 * @return ClassDesc field type descriptor
		 * @throws InternalError if VarHandleDesc was created improperly or instance is not a field
		 */
		private ClassDesc getFieldTypeClassDesc() {
			ConstantDesc[] args = bootstrapArgs();
			/* field type is always the second bootstrap argument */
			if (!type.isField() || (args.length < 2) || !(args[1] instanceof ClassDesc)) {
				/*[MSG "K0619", "Malformated VarHandleDesc, {0} could not be retrieved."]*/
				throw new InternalError(com.ibm.oti.util.Msg.getString("K0619", "field class descriptor")); //$NON-NLS-1$ //$NON-NLS-2$
			}
			return (ClassDesc)args[1];
		}

		/**
		 * Helper to retrieve array type descriptor from superclass bootstrap arguments.
		 * 
		 * @return ClassDesc array type descriptor
		 * @throws InternalError if VarHandleDesc was created improperly or instance is not an array type
		 */
		private ClassDesc getArrayTypeClassDesc() {
			ConstantDesc[] args = bootstrapArgs();
			/* array type is always the first bootstrap argument for an array type */
			if (type.isField() || (args.length == 0) || !(args[0] instanceof ClassDesc)) {
				/*[MSG "K0619", "Malformated VarHandleDesc, {0} could not be retrieved."]*/
				throw new InternalError(com.ibm.oti.util.Msg.getString("K0619", "array type descriptor")); //$NON-NLS-1$ //$NON-NLS-2$
			}
			return (ClassDesc)args[0];
		}		
	}
/*[ENDIF] Java12 */ 

/*[IF Java15]*/
	/**
	 * Return the target VarHandle. For a direct VarHandle, the target
	 * VarHandle is null. An indirect VarHandle will override this method to
	 * return a non-null target VarHandle.
	 * 
	 * @return null for the target VarHandle.
	 */
	VarHandle target() {
		return null;
	}

	/**
	 * Return the direct-target VarHandle. For a direct VarHandle, the
	 * direct-target is itself. An indirect VarHandle will override this method 
	 * to return a direct-target, which should be a direct VarHandle.
	 * 
	 * @return "this" for a direct VarHandle.
	 */
	VarHandle asDirect() {
		return this;
	}

	/**
	 * Return true for a direct VarHandle and false for an indirect VarHandle.
	 * An indirect VarHandle will override this method to return false. By
	 * default, all VarHandles are direct VarHandles.
	 * 
	 * @return true for a direct VarHandle and false for an indirect VarHandle.
	 */
	boolean isDirect() {
		return true;
	}
	
	/**
	 * Return the direct-target VarHandle for the input varHandle.
	 * 
	 * @param varHandle the input VarHandle.
	 * 
	 * @return the direct-target VarHandle for the input VarHandle.
	 */
	private static VarHandle asDirect(VarHandle varHandle) {
		return varHandle.asDirect();
	}
/*[ENDIF] Java15 */

/*[IF Java15 | OPENJDK_METHODHANDLES]*/
	/**
	 * Return the MethodHandle corresponding to the integer-value of the AccessMode.
	 * 
	 * @param i integer value of the AccessMode.
	 * 
	 * @return the MethodHandle corresponding to the integer-value of the AccessMode.
	 */
	MethodHandle getMethodHandle(int i) {
		return handleTable[i];
	}
/*[ENDIF] Java15 | OPENJDK_METHODHANDLES */

	abstract MethodType accessModeTypeUncached(AccessMode accessMode);
}
