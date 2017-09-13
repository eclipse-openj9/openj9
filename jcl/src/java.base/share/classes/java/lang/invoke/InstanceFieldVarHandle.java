/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package java.lang.invoke;

import static java.lang.invoke.MethodType.methodType;

import java.lang.reflect.Field;

/**
 * {@link VarHandle} subclass for {@link VarHandle} instances for non-static field.
 */
final class InstanceFieldVarHandle extends FieldVarHandle {
	/**
	 * Populates a MethodHandle[] corresponding to the provided type. 
	 * 
	 * @param type The type to create MethodHandles for.
	 * @return The populated MethodHandle[].
	 */
	static final MethodHandle[] populateMHs(Class<?> receiverType, final Class<?> initFieldType) {
		Class<? extends InstanceFieldVarHandleOperations> operationsClass = null;
		Class<?> fieldType = initFieldType;
		
		if (!initFieldType.isPrimitive()) {
			fieldType = Object.class;
			operationsClass = InstanceFieldVarHandleOperations.OpObject.class;
		} else if (int.class == initFieldType) {
			operationsClass = InstanceFieldVarHandleOperations.OpInt.class;
		} else if (long.class == initFieldType) {
			operationsClass = InstanceFieldVarHandleOperations.OpLong.class;
		} else if (boolean.class == initFieldType) {
			operationsClass = InstanceFieldVarHandleOperations.OpBoolean.class;
		} else if (byte.class == initFieldType) {
			operationsClass = InstanceFieldVarHandleOperations.OpByte.class;
		} else if (char.class == initFieldType) {
			operationsClass = InstanceFieldVarHandleOperations.OpChar.class;
		} else if (double.class == initFieldType) {
			operationsClass = InstanceFieldVarHandleOperations.OpDouble.class;
		} else if (float.class == initFieldType) {
			operationsClass = InstanceFieldVarHandleOperations.OpFloat.class;
		} else if (short.class == initFieldType) {
			operationsClass = InstanceFieldVarHandleOperations.OpShort.class;
		} else if (void.class == initFieldType) {
			throw new NoSuchFieldError();
		} else {
			/*[MSG "K0626", "Unable to handle type {0}."]*/
			throw new InternalError(com.ibm.oti.util.Msg.getString("K0626", initFieldType)); //$NON-NLS-1$
		}

		MethodType getter = methodType(fieldType, Object.class, InstanceFieldVarHandle.class);
		MethodType setter = methodType(void.class, Object.class, fieldType, InstanceFieldVarHandle.class);
		MethodType compareAndSet = methodType(boolean.class, Object.class, fieldType, fieldType, InstanceFieldVarHandle.class);
		MethodType compareAndExchange = compareAndSet.changeReturnType(fieldType);
		MethodType getAndSet = setter.changeReturnType(fieldType);
		MethodType[] lookupTypes = populateMTs(getter, setter, compareAndSet, compareAndExchange, getAndSet);
		
		MethodType exactGetter = methodType(initFieldType, receiverType, InstanceFieldVarHandle.class);
		MethodType exactSetter = methodType(void.class, receiverType, initFieldType, InstanceFieldVarHandle.class);
		MethodType exactCompareAndSet = methodType(boolean.class, receiverType, initFieldType, initFieldType, InstanceFieldVarHandle.class);
		MethodType exactCompareAndExchange = compareAndSet.changeReturnType(initFieldType);
		MethodType exactGetAndSet = setter.changeReturnType(initFieldType);
		MethodType[]exactTypes = populateMTs(exactGetter, exactSetter, exactCompareAndSet, exactCompareAndExchange, exactGetAndSet);
				
		return populateMHs(operationsClass, lookupTypes, exactTypes);
	}

	/**
	 * Populates a MethodType[] corresponding to the provided type. 
	 * 
	 * @param type The type to create MethodType for.
	 * @return The populated MethodType[].
	 */
	static final MethodType[] populateMTs(Class<?> type, Class<?> receiverType) {
		MethodType getter = methodType(type, receiverType);
		MethodType setter = methodType(void.class, receiverType, type);
		MethodType compareAndSet = methodType(boolean.class, receiverType, type, type);
		MethodType compareAndExchange = compareAndSet.changeReturnType(type);
		MethodType getAndSet = setter.changeReturnType(type);
		
		return populateMTs(getter, setter, compareAndSet, compareAndExchange, getAndSet);
	}

	/**
	 * Constructs a VarHandle to an instance field.
	 * 
	 * @param lookupClass The class where we start the lookup of the field
	 * @param fieldName The field name
	 * @param fieldType The exact type of the field
	 * @param accessClass The class being used to look up the field
	 */
	InstanceFieldVarHandle(Class<?> lookupClass, String fieldName, Class<?> fieldType, Class<?> accessClass) {
		super(lookupClass, fieldName, fieldType, accessClass, false, new Class<?>[] {lookupClass}, populateMHs(lookupClass, fieldType), populateMTs(fieldType, lookupClass));
	}
	
	/**
	 * Construct a VarHandle to the instance field represented by the provided {@link java.lang.reflect.Field Field}.
	 * 
	 * @param field The {@link java.lang.reflect.Field} to create a VarHandle for.
	 */
	InstanceFieldVarHandle(Field field, Class<?> lookupClass, Class<?> fieldType) {
		super(field, false, new Class<?>[] {lookupClass}, populateMHs(lookupClass, fieldType), populateMTs(lookupClass, fieldType));
	}
	
	/**
	 * Type specific methods used by VarHandle methods for non-static field.
	 */
	@SuppressWarnings("unused")
	static class InstanceFieldVarHandleOperations extends VarHandleOperations {
		static final class OpObject extends InstanceFieldVarHandleOperations {
			private static final Object get(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getObject(receiver, varHandle.vmslot);
			}

			private static final void set(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putObject(receiver, varHandle.vmslot, value);
			}

			private static final Object getVolatile(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getObjectVolatile(receiver, varHandle.vmslot);
			}

			private static final void setVolatile(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putObjectVolatile(receiver, varHandle.vmslot, value);
			}

			private static final Object getOpaque(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getObjectOpaque(receiver, varHandle.vmslot);
			}

			private static final void setOpaque(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putObjectOpaque(receiver, varHandle.vmslot, value);
			}

			private static final Object getAcquire(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getObjectAcquire(receiver, varHandle.vmslot);
			}

			private static final void setRelease(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putObjectRelease(receiver, varHandle.vmslot, value);
			}

			private static final boolean compareAndSet(Object receiver, Object testValue, Object newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetObject(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapObject(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final Object compareAndExchange(Object receiver, Object testValue, Object newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeObject(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeObjectVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final Object compareAndExchangeAcquire(Object receiver, Object testValue, Object newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeObjectAcquire(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final Object compareAndExchangeRelease(Object receiver, Object testValue, Object newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeObjectRelease(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, Object testValue, Object newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectPlain(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, Object testValue, Object newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, Object testValue, Object newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, Object testValue, Object newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectPlain(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final Object getAndSet(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetObject(receiver, varHandle.vmslot, value);
			}

			private static final Object getAndSetAcquire(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetObjectAcquire(receiver, varHandle.vmslot, value);
			}

			private static final Object getAndSetRelease(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetObjectRelease(receiver, varHandle.vmslot, value);
			}

			private static final Object getAndAdd(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndAddAcquire(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndAddRelease(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseAnd(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseAndAcquire(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseAndRelease(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseOr(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseOrAcquire(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseOrRelease(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseXor(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseXorAcquire(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseXorRelease(Object receiver, Object value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpByte extends InstanceFieldVarHandleOperations {
			private static final byte get(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getByte(receiver, varHandle.vmslot);
			}

			private static final void set(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putByte(receiver, varHandle.vmslot, value);
			}

			private static final byte getVolatile(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getByteVolatile(receiver, varHandle.vmslot);
			}

			private static final void setVolatile(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putByteVolatile(receiver, varHandle.vmslot, value);
			}

			private static final byte getOpaque(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getByteOpaque(receiver, varHandle.vmslot);
			}

			private static final void setOpaque(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putByteOpaque(receiver, varHandle.vmslot, value);
			}

			private static final byte getAcquire(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getByteAcquire(receiver, varHandle.vmslot);
			}

			private static final void setRelease(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putByteRelease(receiver, varHandle.vmslot, value);
			}

			private static final boolean compareAndSet(Object receiver, byte testValue, byte newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final byte compareAndExchange(Object receiver, byte testValue, byte newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return (byte)_unsafe.compareAndExchangeInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return (byte)_unsafe.compareAndExchangeIntVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final byte compareAndExchangeAcquire(Object receiver, byte testValue, byte newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.compareAndExchangeIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final byte compareAndExchangeRelease(Object receiver, byte testValue, byte newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.compareAndExchangeIntRelease(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, byte testValue, byte newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, byte testValue, byte newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, byte testValue, byte newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, byte testValue, byte newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final byte getAndSet(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndSetInt(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndSetAcquire(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndSetIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndSetRelease(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndSetIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndAdd(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndAddInt(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndAddAcquire(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndAddIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndAddRelease(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndAddIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndBitwiseAnd(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndBitwiseAndInt(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndBitwiseAndAcquire(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndBitwiseAndIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndBitwiseAndRelease(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndBitwiseAndIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndBitwiseOr(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndBitwiseOrInt(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndBitwiseOrAcquire(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndBitwiseOrIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndBitwiseOrRelease(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndBitwiseOrIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndBitwiseXor(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndBitwiseXorInt(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndBitwiseXorAcquire(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndBitwiseXorIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final byte getAndBitwiseXorRelease(Object receiver, byte value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (byte)_unsafe.getAndBitwiseXorIntRelease(receiver, varHandle.vmslot, value);
			}
		}

		static final class OpChar extends InstanceFieldVarHandleOperations {
			private static final char get(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getChar(receiver, varHandle.vmslot);
			}

			private static final void set(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putChar(receiver, varHandle.vmslot, value);
			}

			private static final char getVolatile(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getCharVolatile(receiver, varHandle.vmslot);
			}

			private static final void setVolatile(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putCharVolatile(receiver, varHandle.vmslot, value);
			}

			private static final char getOpaque(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getCharOpaque(receiver, varHandle.vmslot);
			}

			private static final void setOpaque(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putCharOpaque(receiver, varHandle.vmslot, value);
			}

			private static final char getAcquire(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getCharAcquire(receiver, varHandle.vmslot);
			}

			private static final void setRelease(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putCharRelease(receiver, varHandle.vmslot, value);
			}

			private static final boolean compareAndSet(Object receiver, char testValue, char newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final char compareAndExchange(Object receiver, char testValue, char newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/
				return (char)_unsafe.compareAndExchangeInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return (char)_unsafe.compareAndExchangeIntVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final char compareAndExchangeAcquire(Object receiver, char testValue, char newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.compareAndExchangeIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final char compareAndExchangeRelease(Object receiver, char testValue, char newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.compareAndExchangeIntRelease(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, char testValue, char newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, char testValue, char newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, char testValue, char newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, char testValue, char newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final char getAndSet(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndSetInt(receiver, varHandle.vmslot, value);
			}

			private static final char getAndSetAcquire(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndSetIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final char getAndSetRelease(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndSetIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final char getAndAdd(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndAddInt(receiver, varHandle.vmslot, value);
			}

			private static final char getAndAddAcquire(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndAddIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final char getAndAddRelease(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndAddIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final char getAndBitwiseAnd(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndBitwiseAndInt(receiver, varHandle.vmslot, value);
			}

			private static final char getAndBitwiseAndAcquire(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndBitwiseAndIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final char getAndBitwiseAndRelease(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndBitwiseAndIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final char getAndBitwiseOr(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndBitwiseOrInt(receiver, varHandle.vmslot, value);
			}

			private static final char getAndBitwiseOrAcquire(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndBitwiseOrIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final char getAndBitwiseOrRelease(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndBitwiseOrIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final char getAndBitwiseXor(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndBitwiseXorInt(receiver, varHandle.vmslot, value);
			}

			private static final char getAndBitwiseXorAcquire(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndBitwiseXorIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final char getAndBitwiseXorRelease(Object receiver, char value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (char)_unsafe.getAndBitwiseXorIntRelease(receiver, varHandle.vmslot, value);
			}
		}

		static final class OpDouble extends InstanceFieldVarHandleOperations {
			private static final double get(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getDouble(receiver, varHandle.vmslot);
			}

			private static final void set(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putDouble(receiver, varHandle.vmslot, value);
			}

			private static final double getVolatile(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getDoubleVolatile(receiver, varHandle.vmslot);
			}

			private static final void setVolatile(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putDoubleVolatile(receiver, varHandle.vmslot, value);
			}

			private static final double getOpaque(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getDoubleOpaque(receiver, varHandle.vmslot);
			}

			private static final void setOpaque(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putDoubleOpaque(receiver, varHandle.vmslot, value);
			}

			private static final double getAcquire(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getDoubleAcquire(receiver, varHandle.vmslot);
			}

			private static final void setRelease(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putDoubleRelease(receiver, varHandle.vmslot, value);
			}

			private static final boolean compareAndSet(Object receiver, double testValue, double newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchange(Object receiver, double testValue, double newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeDouble(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeDoubleVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchangeAcquire(Object receiver, double testValue, double newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeDoubleAcquire(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final double compareAndExchangeRelease(Object receiver, double testValue, double newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeDoubleRelease(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, double testValue, double newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDouble(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, double testValue, double newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, double testValue, double newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetDoubleRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, double testValue, double newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoublePlain(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double getAndSet(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetDouble(receiver, varHandle.vmslot, value);
			}

			private static final double getAndSetAcquire(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetDoubleAcquire(receiver, varHandle.vmslot, value);
			}

			private static final double getAndSetRelease(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetDoubleRelease(receiver, varHandle.vmslot, value);
			}

			private static final double getAndAdd(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddDouble(receiver, varHandle.vmslot, value);
			}

			private static final double getAndAddAcquire(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddDoubleAcquire(receiver, varHandle.vmslot, value);
			}

			private static final double getAndAddRelease(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddDoubleRelease(receiver, varHandle.vmslot, value);
			}

			private static final double getAndBitwiseAnd(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndAcquire(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndRelease(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOr(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrAcquire(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrRelease(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXor(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorAcquire(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorRelease(Object receiver, double value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpFloat extends InstanceFieldVarHandleOperations {
			private static final float get(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getFloat(receiver, varHandle.vmslot);
			}

			private static final void set(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putFloat(receiver, varHandle.vmslot, value);
			}

			private static final float getVolatile(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getFloatVolatile(receiver, varHandle.vmslot);
			}

			private static final void setVolatile(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putFloatVolatile(receiver, varHandle.vmslot, value);
			}

			private static final float getOpaque(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getFloatOpaque(receiver, varHandle.vmslot);
			}

			private static final void setOpaque(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putFloatOpaque(receiver, varHandle.vmslot, value);
			}

			private static final float getAcquire(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getFloatAcquire(receiver, varHandle.vmslot);
			}

			private static final void setRelease(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putFloatRelease(receiver, varHandle.vmslot, value);
			}

			private static final boolean compareAndSet(Object receiver, float testValue, float newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapFloat(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchange(Object receiver, float testValue, float newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeFloat(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeFloatVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchangeAcquire(Object receiver, float testValue, float newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeFloatAcquire(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final float compareAndExchangeRelease(Object receiver, float testValue, float newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeFloatRelease(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, float testValue, float newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloat(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, float testValue, float newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, float testValue, float newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, float testValue, float newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float getAndSet(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetFloat(receiver, varHandle.vmslot, value);
			}

			private static final float getAndSetAcquire(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetFloatAcquire(receiver, varHandle.vmslot, value);
			}

			private static final float getAndSetRelease(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetFloatRelease(receiver, varHandle.vmslot, value);
			}

			private static final float getAndAdd(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddFloat(receiver, varHandle.vmslot, value);
			}

			private static final float getAndAddAcquire(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddFloatAcquire(receiver, varHandle.vmslot, value);
			}

			private static final float getAndAddRelease(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddFloatRelease(receiver, varHandle.vmslot, value);
			}

			private static final float getAndBitwiseAnd(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndAcquire(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndRelease(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOr(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrAcquire(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrRelease(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXor(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorAcquire(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorRelease(Object receiver, float value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpInt extends InstanceFieldVarHandleOperations {
			private static final int get(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getInt(receiver, varHandle.vmslot);
			}

			private static final void set(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putInt(receiver, varHandle.vmslot, value);
			}

			private static final int getVolatile(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getIntVolatile(receiver, varHandle.vmslot);
			}

			private static final void setVolatile(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putIntVolatile(receiver, varHandle.vmslot, value);
			}

			private static final int getOpaque(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getIntOpaque(receiver, varHandle.vmslot);
			}

			private static final void setOpaque(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putIntOpaque(receiver, varHandle.vmslot, value);
			}

			private static final int getAcquire(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getIntAcquire(receiver, varHandle.vmslot);
			}

			private static final void setRelease(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final boolean compareAndSet(Object receiver, int testValue, int newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchange(Object receiver, int testValue, int newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeIntVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchangeAcquire(Object receiver, int testValue, int newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final int compareAndExchangeRelease(Object receiver, int testValue, int newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeIntRelease(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int testValue, int newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int testValue, int newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int testValue, int newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int testValue, int newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int getAndSet(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetInt(receiver, varHandle.vmslot, value);
			}

			private static final int getAndSetAcquire(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final int getAndSetRelease(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final int getAndAdd(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddInt(receiver, varHandle.vmslot, value);
			}

			private static final int getAndAddAcquire(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final int getAndAddRelease(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final int getAndBitwiseAnd(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseAndInt(receiver, varHandle.vmslot, value);
			}

			private static final int getAndBitwiseAndAcquire(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseAndIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final int getAndBitwiseAndRelease(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseAndIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final int getAndBitwiseOr(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseOrInt(receiver, varHandle.vmslot, value);
			}

			private static final int getAndBitwiseOrAcquire(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseOrIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final int getAndBitwiseOrRelease(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseOrIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final int getAndBitwiseXor(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseXorInt(receiver, varHandle.vmslot, value);
			}

			private static final int getAndBitwiseXorAcquire(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseXorIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final int getAndBitwiseXorRelease(Object receiver, int value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseXorIntRelease(receiver, varHandle.vmslot, value);
			}
		}
		
		static final class OpLong extends InstanceFieldVarHandleOperations {
			private static final long get(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getLong(receiver, varHandle.vmslot);
			}

			private static final void set(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putLong(receiver, varHandle.vmslot, value);
			}

			private static final long getVolatile(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getLongVolatile(receiver, varHandle.vmslot);
			}

			private static final void setVolatile(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putLongVolatile(receiver, varHandle.vmslot, value);
			}

			private static final long getOpaque(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getLongOpaque(receiver, varHandle.vmslot);
			}

			private static final void setOpaque(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putLongOpaque(receiver, varHandle.vmslot, value);
			}

			private static final long getAcquire(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getLongAcquire(receiver, varHandle.vmslot);
			}

			private static final void setRelease(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putLongRelease(receiver, varHandle.vmslot, value);
			}

			private static final boolean compareAndSet(Object receiver, long testValue, long newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapLong(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long compareAndExchange(Object receiver, long testValue, long newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeLong(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeLongVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long compareAndExchangeAcquire(Object receiver, long testValue, long newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeLongAcquire(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final long compareAndExchangeRelease(Object receiver, long testValue, long newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.compareAndExchangeLongRelease(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, long testValue, long newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLong(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, long testValue, long newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, long testValue, long newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, long testValue, long newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long getAndSet(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetLong(receiver, varHandle.vmslot, value);
			}

			private static final long getAndSetAcquire(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetLongAcquire(receiver, varHandle.vmslot, value);
			}

			private static final long getAndSetRelease(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndSetLongRelease(receiver, varHandle.vmslot, value);
			}

			private static final long getAndAdd(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddLong(receiver, varHandle.vmslot, value);
			}

			private static final long getAndAddAcquire(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddLongAcquire(receiver, varHandle.vmslot, value);
			}

			private static final long getAndAddRelease(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndAddLongRelease(receiver, varHandle.vmslot, value);
			}

			private static final long getAndBitwiseAnd(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseAndLong(receiver, varHandle.vmslot, value);
			}

			private static final long getAndBitwiseAndAcquire(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseAndLongAcquire(receiver, varHandle.vmslot, value);
			}

			private static final long getAndBitwiseAndRelease(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseAndLongRelease(receiver, varHandle.vmslot, value);
			}

			private static final long getAndBitwiseOr(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseOrLong(receiver, varHandle.vmslot, value);
			}

			private static final long getAndBitwiseOrAcquire(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseOrLongAcquire(receiver, varHandle.vmslot, value);
			}

			private static final long getAndBitwiseOrRelease(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseOrLongRelease(receiver, varHandle.vmslot, value);
			}

			private static final long getAndBitwiseXor(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseXorLong(receiver, varHandle.vmslot, value);
			}

			private static final long getAndBitwiseXorAcquire(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseXorLongAcquire(receiver, varHandle.vmslot, value);
			}

			private static final long getAndBitwiseXorRelease(Object receiver, long value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getAndBitwiseXorLongRelease(receiver, varHandle.vmslot, value);
			}
		}

		static final class OpShort extends InstanceFieldVarHandleOperations {
			private static final short get(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getShort(receiver, varHandle.vmslot);
			}

			private static final void set(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putShort(receiver, varHandle.vmslot, value);
			}

			private static final short getVolatile(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getShortVolatile(receiver, varHandle.vmslot);
			}

			private static final void setVolatile(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putShortVolatile(receiver, varHandle.vmslot, value);
			}

			private static final short getOpaque(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getShortOpaque(receiver, varHandle.vmslot);
			}

			private static final void setOpaque(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putShortOpaque(receiver, varHandle.vmslot, value);
			}

			private static final short getAcquire(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getShortAcquire(receiver, varHandle.vmslot);
			}

			private static final void setRelease(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putShortRelease(receiver, varHandle.vmslot, value);
			}

			private static final boolean compareAndSet(Object receiver, short testValue, short newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short compareAndExchange(Object receiver, short testValue, short newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return (short)_unsafe.compareAndExchangeInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return (short)_unsafe.compareAndExchangeIntVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short compareAndExchangeAcquire(Object receiver, short testValue, short newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.compareAndExchangeIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final short compareAndExchangeRelease(Object receiver, short testValue, short newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.compareAndExchangeIntRelease(receiver, varHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, short testValue, short newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntVolatile(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, short testValue, short newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, short testValue, short newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, short testValue, short newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short getAndSet(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndSetInt(receiver, varHandle.vmslot, value);
			}

			private static final short getAndSetAcquire(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndSetIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final short getAndSetRelease(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndSetIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final short getAndAdd(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndAddInt(receiver, varHandle.vmslot, value);
			}

			private static final short getAndAddAcquire(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndAddIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final short getAndAddRelease(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndAddIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseAnd(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndBitwiseAndInt(receiver, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseAndAcquire(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndBitwiseAndIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseAndRelease(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndBitwiseAndIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseOr(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndBitwiseOrInt(receiver, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseOrAcquire(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndBitwiseOrIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseOrRelease(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndBitwiseOrIntRelease(receiver, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseXor(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndBitwiseXorInt(receiver, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseXorAcquire(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndBitwiseXorIntAcquire(receiver, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseXorRelease(Object receiver, short value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (short)_unsafe.getAndBitwiseXorIntRelease(receiver, varHandle.vmslot, value);
			}
		}
		
		static final class OpBoolean extends InstanceFieldVarHandleOperations {
			private static final boolean get(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getBoolean(receiver, varHandle.vmslot);
			}

			private static final void set(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putBoolean(receiver, varHandle.vmslot, value);
			}

			private static final boolean getVolatile(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getBooleanVolatile(receiver, varHandle.vmslot);
			}

			private static final void setVolatile(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putBooleanVolatile(receiver, varHandle.vmslot, value);
			}

			private static final boolean getOpaque(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getBooleanOpaque(receiver, varHandle.vmslot);
			}

			private static final void setOpaque(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putBooleanOpaque(receiver, varHandle.vmslot, value);
			}

			private static final boolean getAcquire(Object receiver, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return _unsafe.getBooleanAcquire(receiver, varHandle.vmslot);
			}

			private static final void setRelease(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				_unsafe.putBooleanRelease(receiver, varHandle.vmslot, value);
			}

			private static final boolean compareAndSet(Object receiver, boolean testValue, boolean newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchange(Object receiver, boolean testValue, boolean newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return (0 != _unsafe.compareAndExchangeInt(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0));
/*[ELSE]
				return (0 != _unsafe.compareAndExchangeIntVolatile(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0));
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchangeAcquire(Object receiver, boolean testValue, boolean newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.compareAndExchangeIntAcquire(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0));
			}

			private static final boolean compareAndExchangeRelease(Object receiver, boolean testValue, boolean newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.compareAndExchangeIntRelease(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0));
			}

			private static final boolean weakCompareAndSet(Object receiver, boolean testValue, boolean newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetInt(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntVolatile(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, boolean testValue, boolean newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, boolean testValue, boolean newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, boolean testValue, boolean newValue, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, varHandle.vmslot, testValue ? 1: 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean getAndSet(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndSetInt(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndSetAcquire(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndSetIntAcquire(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndSetRelease(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndSetIntRelease(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndAdd(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddAcquire(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddRelease(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndBitwiseAnd(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndBitwiseAndInt(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseAndAcquire(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndBitwiseAndIntAcquire(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseAndRelease(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndBitwiseAndIntRelease(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOr(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndBitwiseOrInt(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOrAcquire(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndBitwiseOrIntAcquire(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOrRelease(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndBitwiseOrIntRelease(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXor(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndBitwiseXorInt(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXorAcquire(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndBitwiseXorIntAcquire(receiver, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXorRelease(Object receiver, boolean value, InstanceFieldVarHandle varHandle) {
				receiver.getClass();
				return (0 != _unsafe.getAndBitwiseXorIntRelease(receiver, varHandle.vmslot, value ? 1 : 0));
			}
		}
	}
}
