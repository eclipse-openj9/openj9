/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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

import static java.lang.invoke.MethodType.methodType;
import static java.lang.invoke.ArrayVarHandle.ArrayVarHandleOperations.*;

/*[IF Java12]*/
import java.lang.constant.ClassDesc;
import java.util.Optional;
/*[ENDIF] Java12 */

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
		boolean isPrimitiveType = initType.isPrimitive();
		Class<?> type = initType;
		
		if (!isPrimitiveType) {
			type = Object.class;
			operationsClass = OpObject.class;
		} else if (int.class == type) {
			operationsClass = OpInt.class;
		} else if (long.class == type) {
			operationsClass = OpLong.class;
		} else if (boolean.class == type) {
			operationsClass = OpBoolean.class;
		} else if (byte.class == type) {
			operationsClass = OpByte.class;
		} else if (char.class == type) {
			operationsClass = OpChar.class;
		} else if (double.class == type) {
			operationsClass = OpDouble.class;
		} else if (float.class == type) {
			operationsClass = OpFloat.class;
		} else if (short.class == type) {
			operationsClass = OpShort.class;
		} else {
			/*[MSG "K0626", "Unable to handle type {0}."]*/
			throw new InternalError(com.ibm.oti.util.Msg.getString("K0626", type)); //$NON-NLS-1$
		}
		
		/* Populate the MethodHandle[] using MethodHandles to methods in ArrayVarHandleOperations.
		 * Note: For primitive types, there is no need to clone the MethodHandles with the exact types later
		 * in populateMHs() as we already know their types here.
		 */
		Class<?> actualArrayType = (isPrimitiveType) ? arrayType : Object.class;
		MethodType getter = methodType(type, actualArrayType, int.class, VarHandle.class);
		MethodType setter = methodType(void.class, actualArrayType, int.class, type, VarHandle.class);
		MethodType compareAndSet = methodType(boolean.class, actualArrayType, int.class, type, type, VarHandle.class);
		MethodType compareAndExchange = compareAndSet.changeReturnType(type);
		MethodType getAndSet = setter.changeReturnType(type);
		MethodType[] lookupTypes = populateMTs(getter, setter, compareAndSet, compareAndExchange, getAndSet);
		
		/* References use the type Object, so these MethodTypes need to be specialized to the initial type. */
		MethodType[] exactTypes = lookupTypes;
		if (!isPrimitiveType) {
			MethodType exactGetter = methodType(initType, arrayType, int.class, VarHandle.class);
			MethodType exactSetter = methodType(void.class, arrayType, int.class, initType, VarHandle.class);
			MethodType exactCompareAndSet = methodType(boolean.class, arrayType, int.class, initType, initType, VarHandle.class);
			MethodType exactCompareAndExchange = exactCompareAndSet.changeReturnType(initType);
			MethodType exactGetAndSet = exactSetter.changeReturnType(initType);
			exactTypes = populateMTs(exactGetter, exactSetter, exactCompareAndSet, exactCompareAndExchange, exactGetAndSet);
		}
		return populateMHs(operationsClass, lookupTypes, exactTypes);
	}
	
	/**
	 * Constructs a VarHandle that can access elements of an array
	 * 
	 * @param arrayType The array type (i.e. not the componentType) this VarHandle will access.
	 */
	ArrayVarHandle(Class<?> arrayType) {
		super(arrayType.getComponentType(), new Class<?>[] {arrayType, int.class}, populateMHs(arrayType), 0);
	}

/*[IF Java12]*/
	@Override
	public Optional<VarHandleDesc> describeConstable() {
		VarHandleDesc result = null;
		/* coordinateTypes[0] contains array type */
		Optional<ClassDesc> descOp = coordinateTypes[0].describeConstable();
		if (descOp.isPresent()) {
			result = VarHandleDesc.ofArray(descOp.get());
		}
		return Optional.ofNullable(result);
	}
/*[ENDIF] Java12 */

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
					throw newArrayStoreException(fieldType, actualType);
				}
			}
		}
		
		private static final ArrayStoreException newArrayStoreException(Class<?> fieldType, Class<?> actualType) {
			/*[MSG "K0630", "Incorrect type - expected {0}, was {1}."]*/
			return new ArrayStoreException(com.ibm.oti.util.Msg.getString("K0630", fieldType, actualType)); //$NON-NLS-1$
		}

		static final class OpObject extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(Object[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(Object[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final Object get(Object receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObject(receiver, computeOffset(index));
			}

			private static final void set(Object receiver, int index, Object value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObject(receiver, computeOffset(index), value);
			}

			private static final Object getVolatile(Object receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObjectVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(Object receiver, int index, Object value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObjectVolatile(receiver, computeOffset(index), value);
			}

			private static final Object getOpaque(Object receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObjectOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(Object receiver, int index, Object value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObjectOpaque(receiver, computeOffset(index), value);
			}

			private static final Object getAcquire(Object receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				return _unsafe.getObjectAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(Object receiver, int index, Object value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				_unsafe.putObjectRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(Object receiver, int index, Object testValue, Object newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetObject(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapObject(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final Object compareAndExchange(Object receiver, int index, Object testValue, Object newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeObject(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeObjectVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final Object compareAndExchangeAcquire(Object receiver, int index, Object testValue, Object newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
				return _unsafe.compareAndExchangeObjectAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final Object compareAndExchangeRelease(Object receiver, int index, Object testValue, Object newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
				return _unsafe.compareAndExchangeObjectRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(Object receiver, int index, Object testValue, Object newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetObjectPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(Object receiver, int index, Object testValue, Object newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.weakCompareAndSetObjectAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(Object receiver, int index, Object testValue, Object newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetObjectRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(Object receiver, int index, Object testValue, Object newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(newValue, receiver.getClass().getComponentType());
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetObjectPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final Object getAndSet(Object receiver, int index, Object value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				return _unsafe.getAndSetObject(receiver, computeOffset(index), value);
			}

			private static final Object getAndSetAcquire(Object receiver, int index, Object value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				return _unsafe.getAndSetObjectAcquire(receiver, computeOffset(index), value);
			}

			private static final Object getAndSetRelease(Object receiver, int index, Object value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(((Object[])receiver).length, index);
				storeCheck(value, receiver.getClass().getComponentType());
				return _unsafe.getAndSetObjectRelease(receiver, computeOffset(index), value);
			}

			private static final Object getAndAdd(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndAddAcquire(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndAddRelease(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseAnd(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseAndAcquire(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseAndRelease(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseOr(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseOrAcquire(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseOrRelease(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseXor(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseXorAcquire(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final Object getAndBitwiseXorRelease(Object receiver, int index, Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpByte extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(byte[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(byte[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final byte get(byte[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getByte(receiver, computeOffset(index));
			}

			private static final void set(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putByte(receiver, computeOffset(index), value);
			}

			private static final byte getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getByteVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putByteVolatile(receiver, computeOffset(index), value);
			}

			private static final byte getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getByteOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putByteOpaque(receiver, computeOffset(index), value);
			}

			private static final byte getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getByteAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putByteRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(byte[] receiver, int index, byte testValue, byte newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetByte(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapByte(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final byte compareAndExchange(byte[] receiver, int index, byte testValue, byte newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeByte(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeByteVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final byte compareAndExchangeAcquire(byte[] receiver, int index, byte testValue, byte newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeByteAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final byte compareAndExchangeRelease(byte[] receiver, int index, byte testValue, byte newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeByteRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(byte[] receiver, int index, byte testValue, byte newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetBytePlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByte(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, byte testValue, byte newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetByteAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByteAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, byte testValue, byte newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetByteRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByteRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, byte testValue, byte newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetBytePlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapByte(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final byte getAndSet(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndSetAcquire(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndSetRelease(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetByteRelease(receiver, computeOffset(index), value);
			}

			private static final byte getAndAdd(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndAddAcquire(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndAddRelease(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddByteRelease(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseAnd(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseAndAcquire(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseAndRelease(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndByteRelease(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseOr(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseOrAcquire(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseOrRelease(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrByteRelease(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseXor(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorByte(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseXorAcquire(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorByteAcquire(receiver, computeOffset(index), value);
			}

			private static final byte getAndBitwiseXorRelease(byte[] receiver, int index, byte value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorByteRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpChar extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(char[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(char[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final char get(char[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getChar(receiver, computeOffset(index));
			}

			private static final void set(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putChar(receiver, computeOffset(index), value);
			}

			private static final char getVolatile(char[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getCharVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putCharVolatile(receiver, computeOffset(index), value);
			}

			private static final char getOpaque(char[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getCharOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putCharOpaque(receiver, computeOffset(index), value);
			}

			private static final char getAcquire(char[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getCharAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putCharRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(char[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetChar(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapChar(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final char compareAndExchange(char[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeChar(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeCharVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final char compareAndExchangeAcquire(char[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeCharAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final char compareAndExchangeRelease(char[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeCharRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(char[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetCharPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapChar(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(char[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetCharAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapCharAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(char[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetCharRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapCharRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(char[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetCharPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapChar(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final char getAndSet(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetChar(receiver, computeOffset(index), value);
			}

			private static final char getAndSetAcquire(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndSetRelease(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetCharRelease(receiver, computeOffset(index), value);
			}

			private static final char getAndAdd(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddChar(receiver, computeOffset(index), value);
			}

			private static final char getAndAddAcquire(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndAddRelease(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddCharRelease(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseAnd(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndChar(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseAndAcquire(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseAndRelease(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndCharRelease(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseOr(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrChar(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseOrAcquire(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseOrRelease(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrCharRelease(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseXor(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorChar(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseXorAcquire(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorCharAcquire(receiver, computeOffset(index), value);
			}

			private static final char getAndBitwiseXorRelease(char[] receiver, int index, char value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorCharRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpDouble extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(double[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(double[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final double get(double[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getDouble(receiver, computeOffset(index));
			}

			private static final void set(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putDouble(receiver, computeOffset(index), value);
			}

			private static final double getVolatile(double[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getDoubleVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putDoubleVolatile(receiver, computeOffset(index), value);
			}

			private static final double getOpaque(double[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getDoubleOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putDoubleOpaque(receiver, computeOffset(index), value);
			}

			private static final double getAcquire(double[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getDoubleAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putDoubleRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(double[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetDouble(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchange(double[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.compareAndExchangeDouble(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeDoubleVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchangeAcquire(double[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeDoubleAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final double compareAndExchangeRelease(double[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeDoubleRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(double[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.weakCompareAndSetDoublePlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(double[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(double[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(double[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoublePlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double getAndSet(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetDouble(receiver, computeOffset(index), value);
			}

			private static final double getAndSetAcquire(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetDoubleAcquire(receiver, computeOffset(index), value);
			}

			private static final double getAndSetRelease(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetDoubleRelease(receiver, computeOffset(index), value);
			}

			private static final double getAndAdd(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddDouble(receiver, computeOffset(index), value);
			}

			private static final double getAndAddAcquire(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddDoubleAcquire(receiver, computeOffset(index), value);
			}

			private static final double getAndAddRelease(double[] receiver, int index, double value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddDoubleRelease(receiver, computeOffset(index), value);
			}

			private static final double getAndBitwiseAnd(double[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndAcquire(double[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndRelease(double[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOr(double[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrAcquire(double[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrRelease(double[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXor(double[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorAcquire(double[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorRelease(double[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpFloat extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(float[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(float[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final float get(float[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getFloat(receiver, computeOffset(index));
			}

			private static final void set(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putFloat(receiver, computeOffset(index), value);
			}

			private static final float getVolatile(float[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getFloatVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putFloatVolatile(receiver, computeOffset(index), value);
			}

			private static final float getOpaque(float[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getFloatOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putFloatOpaque(receiver, computeOffset(index), value);
			}

			private static final float getAcquire(float[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getFloatAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putFloatRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(float[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetFloat(receiver, computeOffset(index), testValue, newValue);				
/*[ELSE]
				return _unsafe.compareAndSwapFloat(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchange(float[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeFloat(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeFloatVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchangeAcquire(float[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeFloatAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final float compareAndExchangeRelease(float[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeFloatRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(float[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(float[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(float[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(float[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float getAndSet(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetFloat(receiver, computeOffset(index), value);
			}

			private static final float getAndSetAcquire(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetFloatAcquire(receiver, computeOffset(index), value);
			}

			private static final float getAndSetRelease(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetFloatRelease(receiver, computeOffset(index), value);
			}

			private static final float getAndAdd(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddFloat(receiver, computeOffset(index), value);
			}

			private static final float getAndAddAcquire(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddFloatAcquire(receiver, computeOffset(index), value);
			}

			private static final float getAndAddRelease(float[] receiver, int index, float value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddFloatRelease(receiver, computeOffset(index), value);
			}

			private static final float getAndBitwiseAnd(float[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndAcquire(float[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndRelease(float[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOr(float[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrAcquire(float[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrRelease(float[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXor(float[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorAcquire(float[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorRelease(float[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpInt extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(int[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(int[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final int get(int[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getInt(receiver, computeOffset(index));
			}

			private static final void set(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putInt(receiver, computeOffset(index), value);
			}

			private static final int getVolatile(int[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getIntVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putIntVolatile(receiver, computeOffset(index), value);
			}

			private static final int getOpaque(int[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getIntOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putIntOpaque(receiver, computeOffset(index), value);
			}

			private static final int getAcquire(int[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getIntAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putIntRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(int[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetInt(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchange(int[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeInt(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeIntVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchangeAcquire(int[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeIntAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final int compareAndExchangeRelease(int[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeIntRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(int[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final boolean weakCompareAndSetAcquire(int[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(int[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(int[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int getAndSet(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetInt(receiver, computeOffset(index), value);
			}

			private static final int getAndSetAcquire(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndSetRelease(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetIntRelease(receiver, computeOffset(index), value);
			}

			private static final int getAndAdd(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddInt(receiver, computeOffset(index), value);
			}

			private static final int getAndAddAcquire(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndAddRelease(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddIntRelease(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseAnd(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndInt(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseAndAcquire(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseAndRelease(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndIntRelease(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseOr(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrInt(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseOrAcquire(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseOrRelease(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrIntRelease(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseXor(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorInt(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseXorAcquire(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorIntAcquire(receiver, computeOffset(index), value);
			}

			private static final int getAndBitwiseXorRelease(int[] receiver, int index, int value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorIntRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpLong extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(long[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(long[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final long get(long[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getLong(receiver, computeOffset(index));
			}

			private static final void set(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putLong(receiver, computeOffset(index), value);
			}

			private static final long getVolatile(long[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getLongVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putLongVolatile(receiver, computeOffset(index), value);
			}

			private static final long getOpaque(long[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getLongOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putLongOpaque(receiver, computeOffset(index), value);
			}

			private static final long getAcquire(long[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getLongAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putLongRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(long[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetLong(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapLong(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final long compareAndExchange(long[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeLong(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeLongVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long compareAndExchangeAcquire(long[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeLongAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final long compareAndExchangeRelease(long[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeLongRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(long[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(long[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(long[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(long[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long getAndSet(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetLong(receiver, computeOffset(index), value);
			}

			private static final long getAndSetAcquire(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndSetRelease(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetLongRelease(receiver, computeOffset(index), value);
			}

			private static final long getAndAdd(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddLong(receiver, computeOffset(index), value);
			}

			private static final long getAndAddAcquire(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndAddRelease(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddLongRelease(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseAnd(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndLong(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseAndAcquire(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseAndRelease(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndLongRelease(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseOr(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrLong(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseOrAcquire(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseOrRelease(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrLongRelease(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseXor(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorLong(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseXorAcquire(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorLongAcquire(receiver, computeOffset(index), value);
			}

			private static final long getAndBitwiseXorRelease(long[] receiver, int index, long value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorLongRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpShort extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(short[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(short[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final short get(short[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getShort(receiver, computeOffset(index));
			}

			private static final void set(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putShort(receiver, computeOffset(index), value);
			}

			private static final short getVolatile(short[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getShortVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putShortVolatile(receiver, computeOffset(index), value);
			}

			private static final short getOpaque(short[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getShortOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putShortOpaque(receiver, computeOffset(index), value);
			}

			private static final short getAcquire(short[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getShortAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putShortRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(short[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetShort(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapShort(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short compareAndExchange(short[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeShort(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeShortVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short compareAndExchangeAcquire(short[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeShortAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final short compareAndExchangeRelease(short[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeShortRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(short[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.weakCompareAndSetShortPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShort(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(short[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetShortAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShortAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(short[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetShortRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShortRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(short[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetShortPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapShort(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short getAndSet(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetShort(receiver, computeOffset(index), value);
			}

			private static final short getAndSetAcquire(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndSetRelease(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetShortRelease(receiver, computeOffset(index), value);
			}

			private static final short getAndAdd(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddShort(receiver, computeOffset(index), value);
			}

			private static final short getAndAddAcquire(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndAddRelease(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndAddShortRelease(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseAnd(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndShort(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseAndAcquire(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseAndRelease(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndShortRelease(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseOr(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrShort(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseOrAcquire(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseOrRelease(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrShortRelease(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseXor(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorShort(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseXorAcquire(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorShortAcquire(receiver, computeOffset(index), value);
			}

			private static final short getAndBitwiseXorRelease(short[] receiver, int index, short value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorShortRelease(receiver, computeOffset(index), value);
			}
		}

		static final class OpBoolean extends ArrayVarHandleOperations {
			private static final int BASE_OFFSET = _unsafe.arrayBaseOffset(boolean[].class);
			private static final int INDEX_SCALE = _unsafe.arrayIndexScale(boolean[].class);
			
			private static final long computeOffset(int index) {
				return computeOffset(index, BASE_OFFSET, INDEX_SCALE);
			}
			
			private static final boolean get(boolean[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getBoolean(receiver, computeOffset(index));
			}

			private static final void set(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getVolatile(boolean[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getBooleanVolatile(receiver, computeOffset(index));
			}

			private static final void setVolatile(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putBooleanVolatile(receiver, computeOffset(index), value);
			}

			private static final boolean getOpaque(boolean[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getBooleanOpaque(receiver, computeOffset(index));
			}

			private static final void setOpaque(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putBooleanOpaque(receiver, computeOffset(index), value);
			}

			private static final boolean getAcquire(boolean[] receiver, int index, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getBooleanAcquire(receiver, computeOffset(index));
			}

			private static final void setRelease(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				_unsafe.putBooleanRelease(receiver, computeOffset(index), value);
			}

			private static final boolean compareAndSet(boolean[] receiver, int index, boolean testValue, boolean newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.compareAndSetBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/
			}

			private static final boolean compareAndExchange(boolean[] receiver, int index, boolean testValue, boolean newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeBooleanVolatile(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchangeAcquire(boolean[] receiver, int index, boolean testValue, boolean newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeBooleanAcquire(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean compareAndExchangeRelease(boolean[] receiver, int index, boolean testValue, boolean newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.compareAndExchangeBooleanRelease(receiver, computeOffset(index), testValue, newValue);
			}

			private static final boolean weakCompareAndSet(boolean[] receiver, int index, boolean testValue, boolean newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetBooleanPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(boolean[] receiver, int index, boolean testValue, boolean newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetBooleanAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBooleanAcquire(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(boolean[] receiver, int index, boolean testValue, boolean newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetBooleanRelease(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBooleanRelease(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(boolean[] receiver, int index, boolean testValue, boolean newValue, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetBooleanPlain(receiver, computeOffset(index), testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapBoolean(receiver, computeOffset(index), testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean getAndSet(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getAndSetAcquire(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetBooleanAcquire(receiver, computeOffset(index), value);
			}

			private static final boolean getAndSetRelease(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndSetBooleanRelease(receiver, computeOffset(index), value);
			}

			private static final boolean getAndAdd(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddAcquire(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddRelease(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndBitwiseAnd(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseAndAcquire(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndBooleanAcquire(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseAndRelease(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseAndBooleanRelease(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseOr(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseOrAcquire(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrBooleanAcquire(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseOrRelease(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseOrBooleanRelease(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseXor(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorBoolean(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseXorAcquire(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorBooleanAcquire(receiver, computeOffset(index), value);
			}

			private static final boolean getAndBitwiseXorRelease(boolean[] receiver, int index, boolean value, VarHandle varHandle) {
				receiver.getClass();
				boundsCheck(receiver.length, index);
				return _unsafe.getAndBitwiseXorBooleanRelease(receiver, computeOffset(index), value);
			}
		}
	}
}
