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

/**
 * {@link VarHandle} subclass for array element {@link VarHandle} instances.
 */
final class ArrayVarHandle extends VarHandle {
	/**
	 * Populates the static MethodHandle[] corresponding to the provided type. 
	 * 
	 * @param arrayType The receiver type to create MethodHandles for. Must be an array type.
	 * @return The populated MethodHandle[].
	 * 
	 * @throws NullPointerException if arrayType is null or not an array type.
	 */
	static final MethodHandle[] populateMHs(Class<?> arrayType) {
		Class<? extends ArrayVarHandleOperations> operationsClass = null;
		final Class<?> initType = arrayType.getComponentType();
		Class<?> type = initType;
		
		if (!type.isPrimitive()) {
			type = Object.class;
			operationsClass = ArrayVarHandleOperations.OpObject.class;
		} else if (int.class == type) {
			operationsClass = ArrayVarHandleOperations.OpInt.class;
		} else if (long.class == type) {
			operationsClass = ArrayVarHandleOperations.OpLong.class;
		} else if (boolean.class == type) {
			operationsClass = ArrayVarHandleOperations.OpBoolean.class;
		} else if (byte.class == type) {
			operationsClass = ArrayVarHandleOperations.OpByte.class;
		} else if (char.class == type) {
			operationsClass = ArrayVarHandleOperations.OpChar.class;
		} else if (double.class == type) {
			operationsClass = ArrayVarHandleOperations.OpDouble.class;
		} else if (float.class == type) {
			operationsClass = ArrayVarHandleOperations.OpFloat.class;
		} else if (short.class == type) {
			operationsClass = ArrayVarHandleOperations.OpShort.class;
		} else {
			/*[MSG "K0626", "Unable to handle type {0}."]*/
			throw new InternalError(com.ibm.oti.util.Msg.getString("K0626", type)); //$NON-NLS-1$
		}
		
		// Populate the MethodHandle[] using MethodHandles to methods in ArrayVarHandleOperations
		MethodType getter = methodType(type, Object.class, int.class, ArrayVarHandle.class);
		MethodType setter = methodType(void.class, Object.class, int.class, type, ArrayVarHandle.class);
		MethodType compareAndSet = methodType(boolean.class, Object.class, int.class, type, type, ArrayVarHandle.class);
		MethodType compareAndExchange = compareAndSet.changeReturnType(type);
		MethodType getAndSet = setter.changeReturnType(type);
		
		/* Construct the lookup MethodTypes. References use the type Object, so these MethodTypes need to be specialized to the initial type. */
		MethodType[] lookupTypes = populateMTs(getter, setter, compareAndSet, compareAndExchange, getAndSet);
		MethodType[] exactTypes = lookupTypes;
		if (initType == type) {
			MethodType exactGetter = methodType(initType, arrayType, int.class, ArrayVarHandle.class);
			MethodType exactSetter = methodType(void.class, arrayType, int.class, initType, ArrayVarHandle.class);
			MethodType exactCompareAndSet = methodType(boolean.class, arrayType, int.class, initType, initType, ArrayVarHandle.class);
			MethodType exactCompareAndExchange = compareAndSet.changeReturnType(initType);
			MethodType exactGetAndSet = setter.changeReturnType(initType);
			exactTypes = populateMTs(exactGetter, exactSetter, exactCompareAndSet, exactCompareAndExchange, exactGetAndSet);
		}
		
		return populateMHs(operationsClass, lookupTypes, exactTypes);
	}
	
	/**
	 * Populates the static MethodType[] corresponding to the provided type. 
	 * 
	 * @param type The type to create MethodType for.
	 * @return The populated MethodType[].
	 */
	static final MethodType[] populateMTs(Class<?> arrayType) {
		Class<?> type = arrayType.getComponentType();
		
		// Create generic MethodTypes
		MethodType getter = methodType(type, arrayType, int.class);
		MethodType setter = methodType(void.class, arrayType, int.class, type);
		MethodType compareAndSet = methodType(boolean.class, arrayType, int.class, type, type);
		MethodType compareAndExchange = compareAndSet.changeReturnType(type);
		MethodType getAndSet = setter.changeReturnType(type);
		
		return populateMTs(getter, setter, compareAndSet, compareAndExchange, getAndSet);
	}
	
	/**
	 * Constructs a VarHandle that can access elements of an array
	 * 
	 * @param arrayType The array type (i.e. not the componentType) this VarHandle will access.
	 */
	ArrayVarHandle(Class<?> arrayType) {
		super(arrayType.getComponentType(), new Class<?>[] {arrayType, int.class}, populateMHs(arrayType), populateMTs(arrayType), 0);
	}

	/**
	 * Type specific methods used by array element VarHandle methods.
	 */
	@SuppressWarnings("unused")
	static class ArrayVarHandleOperations extends VarHandleOperations {
		static final void boundsCheck(int arrayLength, long index) {
			if ((index < 0) || (index >= arrayLength)) {
				throw newArrayIndexOutOfBoundsException(arrayLength, index);
			}
		}
		
		static final long computeOffset(int index, int baseOffset, int indexScale) {
			return baseOffset + ((long)index * indexScale);
		}
		
		private static final ArrayIndexOutOfBoundsException newArrayIndexOutOfBoundsException(int arrayLength, long index) {
			/*[MSG "K0622", "Index {0}, array length {1}."]*/
			return new ArrayIndexOutOfBoundsException(com.ibm.oti.util.Msg.getString("K0622", java.lang.Long.toString(index), Integer.toString(arrayLength))); //$NON-NLS-1$
		}
		
		static final void storeCheck(Object value, Class<?> fieldType) {
			if (null != value) {
				Class<?> actualType = value.getClass();
				if (!fieldType.isAssignableFrom(actualType)) {
					throw newIllegalArgumentException(fieldType, actualType);
				}
			}
		}
		
		private static final IllegalArgumentException newIllegalArgumentException(Class<?> actualType, Class<?> fieldType) {
			/*[MSG "K0630", "Incorrect type - expected {0}, was {1}."]*/
			return new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0630", fieldType, actualType)); //$NON-NLS-1$
		}

		static final class OpObject extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(Object[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(Object[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final Object get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObject(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObject(receiver, computeOffset(index), value);
			}

			private static final Object getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObjectVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObjectVolatile(receiver, computeOffset(index), value);
			}

			private static final Object getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObjectOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObjectOpaque(receiver, computeOffset(index), value);
			}

			private static final Object getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObjectAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObjectRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetObject(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapObject(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final Object compareAndExchange(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeObject(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeObjectVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final Object compareAndExchangeAcquire(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
				return _unsafe.compareAndExchangeObjectAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final Object compareAndExchangeRelease(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
				return _unsafe.compareAndExchangeObjectRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetObjectAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final Object getAndSet(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				return _unsafe.getAndSetObject(receiver, computeOffset(index), value);
			}

			private static final Object getAndSetAcquire(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				return _unsafe.getAndSetObjectAcquire(receiver, computeOffset(index), value);
			}

			private static final Object getAndSetRelease(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				return _unsafe.getAndSetObjectRelease(receiver, computeOffset(index), value);
			}

			private static final Object getAndAdd(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndAddAcquire(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndAddRelease(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseAnd(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseAndAcquire(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseAndRelease(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseOr(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseOrAcquire(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseOrRelease(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseXor(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseXorAcquire(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseXorRelease(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpByte extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(byte[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(byte[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final byte get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getByte(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				_unsafe.putByte(receiver, computeOffset(index), value);
			}

			private static final byte getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getByteVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				_unsafe.putByteVolatile(receiver, computeOffset(index), value);
			}

			private static final byte getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getByteOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				_unsafe.putByteOpaque(receiver, computeOffset(index), value);
			}

			private static final byte getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getByteAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				_unsafe.putByteRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetByte(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapByte(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final byte compareAndExchange(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeByte(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeByteVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final byte compareAndExchangeAcquire(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.compareAndExchangeByteAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final byte compareAndExchangeRelease(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.compareAndExchangeByteRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBytePlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByte(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetByteAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByteAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetByteRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByteRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBytePlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByte(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final byte getAndSet(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndSetByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndSetAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndSetByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndSetRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndSetByteRelease(receiver, computeOffset(index), value);
			}

			private static final byte getAndAdd(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndAddByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndAddAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndAddByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndAddRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndAddByteRelease(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseAnd(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseAndByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseAndAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseAndByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseAndRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseAndByteRelease(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseOr(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseOrByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseOrAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseOrByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseOrRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseOrByteRelease(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseXor(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseXorByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseXorAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseXorByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseXorRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseXorByteRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpChar extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(char[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(char[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final char get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getChar(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				_unsafe.putChar(receiver, computeOffset(index), value);
			}

			private static final char getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getCharVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				_unsafe.putCharVolatile(receiver, computeOffset(index), value);
			}

			private static final char getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getCharOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				_unsafe.putCharOpaque(receiver, computeOffset(index), value);
			}

			private static final char getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getCharAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				_unsafe.putCharRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetChar(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapChar(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final char compareAndExchange(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeChar(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeCharVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final char compareAndExchangeAcquire(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.compareAndExchangeCharAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final char compareAndExchangeRelease(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.compareAndExchangeCharRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetCharPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapChar(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetCharAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapCharAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetCharRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapCharRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetCharPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapChar(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final char getAndSet(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndSetChar(receiver, computeOffset(index), value);
			}

			private static final char getAndSetAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndSetCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndSetRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndSetCharRelease(receiver, computeOffset(index), value);
			}

			private static final char getAndAdd(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndAddChar(receiver, computeOffset(index), value);
			}

			private static final char getAndAddAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndAddCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndAddRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndAddCharRelease(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseAnd(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseAndChar(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseAndAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseAndCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseAndRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseAndCharRelease(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseOr(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseOrChar(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseOrAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseOrCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseOrRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseOrCharRelease(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseXor(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseXorChar(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseXorAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseXorCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseXorRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseXorCharRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpDouble extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(double[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(double[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final double get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getDouble(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				_unsafe.putDouble(receiver, computeOffset(index), value);
			}

			private static final double getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getDoubleVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				_unsafe.putDoubleVolatile(receiver, computeOffset(index), value);
			}

			private static final double getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getDoubleOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				_unsafe.putDoubleOpaque(receiver, computeOffset(index), value);
			}

			private static final double getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getDoubleAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				_unsafe.putDoubleRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchange(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.compareAndExchangeDouble(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeDoubleVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchangeAcquire(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.compareAndExchangeDoubleAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final double compareAndExchangeRelease(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.compareAndExchangeDoubleRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetDoublePlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoublePlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double getAndSet(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndSetDouble(receiver, computeOffset(index), value);
			}

			private static final double getAndSetAcquire(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndSetDoubleAcquire(receiver, computeOffset(index), value);
			}

			private static final double getAndSetRelease(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndSetDoubleRelease(receiver, computeOffset(index), value);
			}

			private static final double getAndAdd(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndAddDouble(receiver, computeOffset(index), value);
			}

			private static final double getAndAddAcquire(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndAddDoubleAcquire(receiver, computeOffset(index), value);
			}

			private static final double getAndAddRelease(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndAddDoubleRelease(receiver, computeOffset(index), value);
			}

			private static final double getAndBitwiseAnd(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndAcquire(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndRelease(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOr(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrAcquire(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrRelease(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXor(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorAcquire(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorRelease(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpFloat extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(float[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(float[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final float get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getFloat(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				_unsafe.putFloat(receiver, computeOffset(index), value);
			}

			private static final float getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getFloatVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				_unsafe.putFloatVolatile(receiver, computeOffset(index), value);
			}

			private static final float getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getFloatOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				_unsafe.putFloatOpaque(receiver, computeOffset(index), value);
			}

			private static final float getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getFloatAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				_unsafe.putFloatRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(receiver, computeOffset(index), testValue, newValue);				
/*[ELSE]
				return _unsafe.compareAndSwapFloat(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchange(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeFloat(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeFloatVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchangeAcquire(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.compareAndExchangeFloatAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final float compareAndExchangeRelease(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.compareAndExchangeFloatRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float getAndSet(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndSetFloat(receiver, computeOffset(index), value);
			}

			private static final float getAndSetAcquire(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndSetFloatAcquire(receiver, computeOffset(index), value);
			}

			private static final float getAndSetRelease(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndSetFloatRelease(receiver, computeOffset(index), value);
			}

			private static final float getAndAdd(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndAddFloat(receiver, computeOffset(index), value);
			}

			private static final float getAndAddAcquire(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndAddFloatAcquire(receiver, computeOffset(index), value);
			}

			private static final float getAndAddRelease(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndAddFloatRelease(receiver, computeOffset(index), value);
			}

			private static final float getAndBitwiseAnd(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndAcquire(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndRelease(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOr(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrAcquire(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrRelease(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXor(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorAcquire(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorRelease(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpInt extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(int[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(int[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final int get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getInt(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				_unsafe.putInt(receiver, computeOffset(index), value);
			}

			private static final int getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getIntVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				_unsafe.putIntVolatile(receiver, computeOffset(index), value);
			}

			private static final int getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getIntOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				_unsafe.putIntOpaque(receiver, computeOffset(index), value);
			}

			private static final int getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getIntAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				_unsafe.putIntRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchange(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeInt(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeIntVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchangeAcquire(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.compareAndExchangeIntAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final int compareAndExchangeRelease(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.compareAndExchangeIntRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int getAndSet(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndSetInt(receiver, computeOffset(index), value);
			}

			private static final int getAndSetAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndSetIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndSetRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndSetIntRelease(receiver, computeOffset(index), value);
			}

			private static final int getAndAdd(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndAddInt(receiver, computeOffset(index), value);
			}

			private static final int getAndAddAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndAddIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndAddRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndAddIntRelease(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseAnd(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseAndInt(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseAndAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseAndIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseAndRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseAndIntRelease(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseOr(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseOrInt(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseOrAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseOrIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseOrRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseOrIntRelease(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseXor(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseXorInt(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseXorAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseXorIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseXorRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseXorIntRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpLong extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(long[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(long[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final long get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getLong(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				_unsafe.putLong(receiver, computeOffset(index), value);
			}

			private static final long getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getLongVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				_unsafe.putLongVolatile(receiver, computeOffset(index), value);
			}

			private static final long getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getLongOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				_unsafe.putLongOpaque(receiver, computeOffset(index), value);
			}

			private static final long getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getLongAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				_unsafe.putLongRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapLong(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final long compareAndExchange(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeLong(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeLongVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long compareAndExchangeAcquire(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.compareAndExchangeLongAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final long compareAndExchangeRelease(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.compareAndExchangeLongRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long getAndSet(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndSetLong(receiver, computeOffset(index), value);
			}

			private static final long getAndSetAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndSetLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndSetRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndSetLongRelease(receiver, computeOffset(index), value);
			}

			private static final long getAndAdd(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndAddLong(receiver, computeOffset(index), value);
			}

			private static final long getAndAddAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndAddLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndAddRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndAddLongRelease(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseAnd(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseAndLong(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseAndAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseAndLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseAndRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseAndLongRelease(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseOr(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseOrLong(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseOrAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseOrLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseOrRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseOrLongRelease(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseXor(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseXorLong(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseXorAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseXorLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseXorRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseXorLongRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpShort extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(short[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(short[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final short get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getShort(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				_unsafe.putShort(receiver, computeOffset(index), value);
			}

			private static final short getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getShortVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				_unsafe.putShortVolatile(receiver, computeOffset(index), value);
			}

			private static final short getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getShortOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				_unsafe.putShortOpaque(receiver, computeOffset(index), value);
			}

			private static final short getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getShortAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				_unsafe.putShortRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetShort(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapShort(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short compareAndExchange(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeShort(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeShortVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short compareAndExchangeAcquire(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.compareAndExchangeShortAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final short compareAndExchangeRelease(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.compareAndExchangeShortRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetShortPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShort(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetShortAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShortAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetShortRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShortRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetShortPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShort(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short getAndSet(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndSetShort(receiver, computeOffset(index), value);
			}

			private static final short getAndSetAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndSetShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndSetRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndSetShortRelease(receiver, computeOffset(index), value);
			}

			private static final short getAndAdd(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndAddShort(receiver, computeOffset(index), value);
			}

			private static final short getAndAddAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndAddShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndAddRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndAddShortRelease(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseAnd(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseAndShort(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseAndAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseAndShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseAndRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseAndShortRelease(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseOr(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseOrShort(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseOrAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseOrShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseOrRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseOrShortRelease(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseXor(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseXorShort(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseXorAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseXorShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseXorRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseXorShortRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpBoolean extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(boolean[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(boolean[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final boolean get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getBoolean(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				_unsafe.putBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getBooleanVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				_unsafe.putBooleanVolatile(receiver, computeOffset(index), value);
			}

			private static final boolean getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getBooleanOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				_unsafe.putBooleanOpaque(receiver, computeOffset(index), value);
			}

			private static final boolean getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getBooleanAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				_unsafe.putBooleanRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchange(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeBooleanVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchangeAcquire(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.compareAndExchangeBooleanAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean compareAndExchangeRelease(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.compareAndExchangeBooleanRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBooleanPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBooleanAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBooleanAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBooleanRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBooleanRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBooleanPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean getAndSet(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndSetBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getAndSetAcquire(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndSetBooleanAcquire(receiver, computeOffset(index), value);
			}

			private static final boolean getAndSetRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndSetBooleanRelease(receiver, computeOffset(index), value);
			}

			private static final boolean getAndAdd(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddAcquire(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndBitwiseAnd(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseAndBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseAndAcquire(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseAndBooleanAcquire(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseAndRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseAndBooleanRelease(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseOr(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseOrBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseOrAcquire(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseOrBooleanAcquire(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseOrRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseOrBooleanRelease(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseXor(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseXorBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseXorAcquire(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseXorBooleanAcquire(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseXorRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseXorBooleanRelease(receiver, computeOffset(index), value);
			}
		}
	}
}
