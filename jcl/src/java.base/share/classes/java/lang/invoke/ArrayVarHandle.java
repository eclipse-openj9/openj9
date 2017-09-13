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

import java.lang.invoke.ArrayVarHandle.ArrayVarHandleOperations;

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

	final int indexOffset;
	final int indexScale;
	
	/**
	 * Constructs a VarHandle that can access elements of an array
	 * 
	 * @param arrayType The array type (i.e. not the componentType) this VarHandle will access.
	 */
	ArrayVarHandle(Class<?> arrayType) {
		super(arrayType.getComponentType(), new Class<?>[] {arrayType, int.class}, populateMHs(arrayType), populateMTs(arrayType), 0);
		this.indexOffset = _unsafe.arrayBaseOffset(arrayType);
		this.indexScale = _unsafe.arrayIndexScale(arrayType);
	}
	
	/**
	 * Calculates the offset for the given array index.
	 * 
	 * @param index The array index to be accessed
	 * @return The computed offset
	 */
	final long computeIndex(int index) {
		return indexOffset + ((long)index * indexScale);
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
			private static final Object get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObject(receiver, varHandle.computeIndex(index));
			}

			private static final void set(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObject(receiver, varHandle.computeIndex(index), value);
			}

			private static final Object getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObjectVolatile(receiver, varHandle.computeIndex(index));
			}

			private static final void setVolatile(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObjectVolatile(receiver, varHandle.computeIndex(index), value);
			}

			private static final Object getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObjectOpaque(receiver, varHandle.computeIndex(index));
			}

			private static final void setOpaque(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObjectOpaque(receiver, varHandle.computeIndex(index), value);
			}

			private static final Object getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObjectAcquire(receiver, varHandle.computeIndex(index));
			}

			private static final void setRelease(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObjectRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetObject(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapObject(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final Object compareAndExchange(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeObject(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeObjectVolatile(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final Object compareAndExchangeAcquire(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
				return _unsafe.compareAndExchangeObjectAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final Object compareAndExchangeRelease(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
				return _unsafe.compareAndExchangeObjectRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetObjectAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, Object testValue, Object newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final Object getAndSet(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				return _unsafe.getAndSetObject(receiver, varHandle.computeIndex(index), value);
			}

			private static final Object getAndSetAcquire(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				return _unsafe.getAndSetObjectAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final Object getAndSetRelease(Object receiver, int index, Object value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				return _unsafe.getAndSetObjectRelease(receiver, varHandle.computeIndex(index), value);
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
			private static final byte get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getByte(receiver, varHandle.computeIndex(index));
			}

			private static final void set(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				_unsafe.putByte(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getByteVolatile(receiver, varHandle.computeIndex(index));
			}

			private static final void setVolatile(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				_unsafe.putByteVolatile(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getByteOpaque(receiver, varHandle.computeIndex(index));
			}

			private static final void setOpaque(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				_unsafe.putByteOpaque(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getByteAcquire(receiver, varHandle.computeIndex(index));
			}

			private static final void setRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				_unsafe.putByteRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetByte(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapByte(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final byte compareAndExchange(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeByte(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeByteVolatile(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final byte compareAndExchangeAcquire(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.compareAndExchangeByteAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final byte compareAndExchangeRelease(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.compareAndExchangeByteRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBytePlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByte(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetByteAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByteAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetByteRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByteRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, byte testValue, byte newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBytePlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByte(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final byte getAndSet(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndSetByte(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndSetAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndSetByteAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndSetRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndSetByteRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndAdd(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndAddByte(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndAddAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndAddByteAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndAddRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndAddByteRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndBitwiseAnd(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseAndByte(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndBitwiseAndAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseAndByteAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndBitwiseAndRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseAndByteRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndBitwiseOr(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseOrByte(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndBitwiseOrAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseOrByteAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndBitwiseOrRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseOrByteRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndBitwiseXor(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseXorByte(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndBitwiseXorAcquire(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseXorByteAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final byte getAndBitwiseXorRelease(Object receiver, int index, byte value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((byte[])receiver).length, index);
				return _unsafe.getAndBitwiseXorByteRelease(receiver, varHandle.computeIndex(index), value);
			}
		}

		static final class OpChar extends ArrayVarHandleOperations {
			private static final char get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getChar(receiver, varHandle.computeIndex(index));
			}

			private static final void set(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				_unsafe.putChar(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getCharVolatile(receiver, varHandle.computeIndex(index));
			}

			private static final void setVolatile(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				_unsafe.putCharVolatile(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getCharOpaque(receiver, varHandle.computeIndex(index));
			}

			private static final void setOpaque(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				_unsafe.putCharOpaque(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getCharAcquire(receiver, varHandle.computeIndex(index));
			}

			private static final void setRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				_unsafe.putCharRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetChar(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapChar(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final char compareAndExchange(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeChar(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeCharVolatile(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final char compareAndExchangeAcquire(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.compareAndExchangeCharAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final char compareAndExchangeRelease(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.compareAndExchangeCharRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetCharPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapChar(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetCharAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapCharAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetCharRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapCharRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, char testValue, char newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetCharPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapChar(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final char getAndSet(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndSetChar(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndSetAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndSetCharAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndSetRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndSetCharRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndAdd(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndAddChar(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndAddAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndAddCharAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndAddRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndAddCharRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndBitwiseAnd(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseAndChar(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndBitwiseAndAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseAndCharAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndBitwiseAndRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseAndCharRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndBitwiseOr(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseOrChar(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndBitwiseOrAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseOrCharAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndBitwiseOrRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseOrCharRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndBitwiseXor(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseXorChar(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndBitwiseXorAcquire(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseXorCharAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final char getAndBitwiseXorRelease(Object receiver, int index, char value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((char[])receiver).length, index);
				return _unsafe.getAndBitwiseXorCharRelease(receiver, varHandle.computeIndex(index), value);
			}
		}

		static final class OpDouble extends ArrayVarHandleOperations {
			private static final double get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getDouble(receiver, varHandle.computeIndex(index));
			}

			private static final void set(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				_unsafe.putDouble(receiver, varHandle.computeIndex(index), value);
			}

			private static final double getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getDoubleVolatile(receiver, varHandle.computeIndex(index));
			}

			private static final void setVolatile(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				_unsafe.putDoubleVolatile(receiver, varHandle.computeIndex(index), value);
			}

			private static final double getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getDoubleOpaque(receiver, varHandle.computeIndex(index));
			}

			private static final void setOpaque(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				_unsafe.putDoubleOpaque(receiver, varHandle.computeIndex(index), value);
			}

			private static final double getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getDoubleAcquire(receiver, varHandle.computeIndex(index));
			}

			private static final void setRelease(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				_unsafe.putDoubleRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchange(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.compareAndExchangeDouble(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeDoubleVolatile(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchangeAcquire(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.compareAndExchangeDoubleAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final double compareAndExchangeRelease(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.compareAndExchangeDoubleRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetDoublePlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, double testValue, double newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoublePlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double getAndSet(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndSetDouble(receiver, varHandle.computeIndex(index), value);
			}

			private static final double getAndSetAcquire(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndSetDoubleAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final double getAndSetRelease(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndSetDoubleRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final double getAndAdd(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndAddDouble(receiver, varHandle.computeIndex(index), value);
			}

			private static final double getAndAddAcquire(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndAddDoubleAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final double getAndAddRelease(Object receiver, int index, double value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((double[])receiver).length, index);
				return _unsafe.getAndAddDoubleRelease(receiver, varHandle.computeIndex(index), value);
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
			private static final float get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getFloat(receiver, varHandle.computeIndex(index));
			}

			private static final void set(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				_unsafe.putFloat(receiver, varHandle.computeIndex(index), value);
			}

			private static final float getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getFloatVolatile(receiver, varHandle.computeIndex(index));
			}

			private static final void setVolatile(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				_unsafe.putFloatVolatile(receiver, varHandle.computeIndex(index), value);
			}

			private static final float getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getFloatOpaque(receiver, varHandle.computeIndex(index));
			}

			private static final void setOpaque(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				_unsafe.putFloatOpaque(receiver, varHandle.computeIndex(index), value);
			}

			private static final float getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getFloatAcquire(receiver, varHandle.computeIndex(index));
			}

			private static final void setRelease(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				_unsafe.putFloatRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(receiver, varHandle.computeIndex(index), testValue, newValue);				
/*[ELSE]
				return _unsafe.compareAndSwapFloat(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchange(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeFloat(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeFloatVolatile(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchangeAcquire(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.compareAndExchangeFloatAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final float compareAndExchangeRelease(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.compareAndExchangeFloatRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, float testValue, float newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float getAndSet(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndSetFloat(receiver, varHandle.computeIndex(index), value);
			}

			private static final float getAndSetAcquire(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndSetFloatAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final float getAndSetRelease(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndSetFloatRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final float getAndAdd(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndAddFloat(receiver, varHandle.computeIndex(index), value);
			}

			private static final float getAndAddAcquire(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndAddFloatAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final float getAndAddRelease(Object receiver, int index, float value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((float[])receiver).length, index);
				return _unsafe.getAndAddFloatRelease(receiver, varHandle.computeIndex(index), value);
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
			private static final int get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getInt(receiver, varHandle.computeIndex(index));
			}

			private static final void set(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				_unsafe.putInt(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getIntVolatile(receiver, varHandle.computeIndex(index));
			}

			private static final void setVolatile(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				_unsafe.putIntVolatile(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getIntOpaque(receiver, varHandle.computeIndex(index));
			}

			private static final void setOpaque(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				_unsafe.putIntOpaque(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getIntAcquire(receiver, varHandle.computeIndex(index));
			}

			private static final void setRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				_unsafe.putIntRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchange(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeInt(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeIntVolatile(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchangeAcquire(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.compareAndExchangeIntAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final int compareAndExchangeRelease(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.compareAndExchangeIntRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, int testValue, int newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int getAndSet(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndSetInt(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndSetAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndSetIntAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndSetRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndSetIntRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndAdd(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndAddInt(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndAddAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndAddIntAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndAddRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndAddIntRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndBitwiseAnd(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseAndInt(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndBitwiseAndAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseAndIntAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndBitwiseAndRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseAndIntRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndBitwiseOr(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseOrInt(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndBitwiseOrAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseOrIntAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndBitwiseOrRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseOrIntRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndBitwiseXor(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseXorInt(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndBitwiseXorAcquire(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseXorIntAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final int getAndBitwiseXorRelease(Object receiver, int index, int value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((int[])receiver).length, index);
				return _unsafe.getAndBitwiseXorIntRelease(receiver, varHandle.computeIndex(index), value);
			}
		}

		static final class OpLong extends ArrayVarHandleOperations {
			private static final long get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getLong(receiver, varHandle.computeIndex(index));
			}

			private static final void set(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				_unsafe.putLong(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getLongVolatile(receiver, varHandle.computeIndex(index));
			}

			private static final void setVolatile(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				_unsafe.putLongVolatile(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getLongOpaque(receiver, varHandle.computeIndex(index));
			}

			private static final void setOpaque(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				_unsafe.putLongOpaque(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getLongAcquire(receiver, varHandle.computeIndex(index));
			}

			private static final void setRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				_unsafe.putLongRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapLong(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final long compareAndExchange(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeLong(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeLongVolatile(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long compareAndExchangeAcquire(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.compareAndExchangeLongAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final long compareAndExchangeRelease(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.compareAndExchangeLongRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, long testValue, long newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long getAndSet(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndSetLong(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndSetAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndSetLongAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndSetRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndSetLongRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndAdd(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndAddLong(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndAddAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndAddLongAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndAddRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndAddLongRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndBitwiseAnd(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseAndLong(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndBitwiseAndAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseAndLongAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndBitwiseAndRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseAndLongRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndBitwiseOr(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseOrLong(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndBitwiseOrAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseOrLongAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndBitwiseOrRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseOrLongRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndBitwiseXor(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseXorLong(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndBitwiseXorAcquire(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseXorLongAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final long getAndBitwiseXorRelease(Object receiver, int index, long value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((long[])receiver).length, index);
				return _unsafe.getAndBitwiseXorLongRelease(receiver, varHandle.computeIndex(index), value);
			}
		}

		static final class OpShort extends ArrayVarHandleOperations {
			private static final short get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getShort(receiver, varHandle.computeIndex(index));
			}

			private static final void set(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				_unsafe.putShort(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getShortVolatile(receiver, varHandle.computeIndex(index));
			}

			private static final void setVolatile(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				_unsafe.putShortVolatile(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getShortOpaque(receiver, varHandle.computeIndex(index));
			}

			private static final void setOpaque(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				_unsafe.putShortOpaque(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getShortAcquire(receiver, varHandle.computeIndex(index));
			}

			private static final void setRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				_unsafe.putShortRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetShort(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapShort(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short compareAndExchange(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeShort(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeShortVolatile(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short compareAndExchangeAcquire(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.compareAndExchangeShortAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final short compareAndExchangeRelease(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.compareAndExchangeShortRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetShortPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShort(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetShortAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShortAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetShortRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShortRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, short testValue, short newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetShortPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShort(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short getAndSet(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndSetShort(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndSetAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndSetShortAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndSetRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndSetShortRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndAdd(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndAddShort(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndAddAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndAddShortAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndAddRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndAddShortRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndBitwiseAnd(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseAndShort(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndBitwiseAndAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseAndShortAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndBitwiseAndRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseAndShortRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndBitwiseOr(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseOrShort(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndBitwiseOrAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseOrShortAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndBitwiseOrRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseOrShortRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndBitwiseXor(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseXorShort(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndBitwiseXorAcquire(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseXorShortAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final short getAndBitwiseXorRelease(Object receiver, int index, short value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((short[])receiver).length, index);
				return _unsafe.getAndBitwiseXorShortRelease(receiver, varHandle.computeIndex(index), value);
			}
		}

		static final class OpBoolean extends ArrayVarHandleOperations {
			private static final boolean get(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getBoolean(receiver, varHandle.computeIndex(index));
			}

			private static final void set(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				_unsafe.putBoolean(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getVolatile(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getBooleanVolatile(receiver, varHandle.computeIndex(index));
			}

			private static final void setVolatile(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				_unsafe.putBooleanVolatile(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getOpaque(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getBooleanOpaque(receiver, varHandle.computeIndex(index));
			}

			private static final void setOpaque(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				_unsafe.putBooleanOpaque(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAcquire(Object receiver, int index, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getBooleanAcquire(receiver, varHandle.computeIndex(index));
			}

			private static final void setRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				_unsafe.putBooleanRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetBoolean(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapBoolean(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchange(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeBoolean(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeBooleanVolatile(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchangeAcquire(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.compareAndExchangeBooleanAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean compareAndExchangeRelease(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.compareAndExchangeBooleanRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBooleanPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBoolean(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBooleanAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBooleanAcquire(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBooleanRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBooleanRelease(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, boolean testValue, boolean newValue, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetBooleanPlain(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBoolean(receiver, varHandle.computeIndex(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean getAndSet(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndSetBoolean(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndSetAcquire(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndSetBooleanAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndSetRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndSetBooleanRelease(receiver, varHandle.computeIndex(index), value);
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
				return _unsafe.getAndBitwiseAndBoolean(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndBitwiseAndAcquire(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseAndBooleanAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndBitwiseAndRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseAndBooleanRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndBitwiseOr(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseOrBoolean(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndBitwiseOrAcquire(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseOrBooleanAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndBitwiseOrRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseOrBooleanRelease(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndBitwiseXor(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseXorBoolean(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndBitwiseXorAcquire(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseXorBooleanAcquire(receiver, varHandle.computeIndex(index), value);
			}

			private static final boolean getAndBitwiseXorRelease(Object receiver, int index, boolean value, ArrayVarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((boolean[])receiver).length, index);
				return _unsafe.getAndBitwiseXorBooleanRelease(receiver, varHandle.computeIndex(index), value);
			}
		}
	}
}
