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

import static java.lang.invoke.ByteArrayViewVarHandle.ByteArrayViewVarHandleOperations.*;
import static java.lang.invoke.MethodType.methodType;

import java.nio.ByteOrder;
import jdk.internal.misc.Unsafe;

final class ByteArrayViewVarHandle extends ViewVarHandle {
	private final static Class<?>[] COORDINATE_TYPES = new Class<?>[] {byte[].class, int.class};
	
	/**
	 * Populates the static MethodHandle[] corresponding to the provided type. 
	 * 
	 * @param type The type to create MethodHandles for.
	 * @return The populated MethodHandle[].
	 */
	static final MethodHandle[] populateMHs(Class<?> type, ByteOrder byteOrder) {
		Class<? extends ByteArrayViewVarHandleOperations> operationsClass = null;
		boolean convertEndian = (byteOrder != ByteOrder.nativeOrder());

		if (int.class == type) {
			operationsClass = convertEndian ? OpIntConvertEndian.class : OpInt.class;
		} else if (long.class == type) {
			operationsClass = convertEndian ? OpLongConvertEndian.class : OpLong.class;
		} else if (char.class == type) {
			operationsClass = convertEndian ? OpCharConvertEndian.class : OpChar.class;
		} else if (double.class == type) {
			operationsClass = convertEndian ? OpDoubleConvertEndian.class : OpDouble.class;
		} else if (float.class == type) {
			operationsClass = convertEndian ? OpFloatConvertEndian.class : OpFloat.class;
		} else if (short.class == type) {
			operationsClass = convertEndian ? OpShortConvertEndian.class : OpShort.class;
		} else {
			/*[MSG "K0624", "{0} is not a supported view type."]*/
			throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0624", type)); //$NON-NLS-1$
		}
		
		MethodType getter = methodType(type, byte[].class, int.class, VarHandle.class);
		MethodType setter = methodType(void.class, byte[].class, int.class, type, VarHandle.class);
		MethodType compareAndSet = methodType(boolean.class, byte[].class, int.class, type, type, VarHandle.class);
		MethodType compareAndExchange = compareAndSet.changeReturnType(type);
		MethodType getAndSet = setter.changeReturnType(type);
		MethodType[] lookupTypes = populateMTs(getter, setter, compareAndSet, compareAndExchange, getAndSet);
		
		return populateMHs(operationsClass, lookupTypes, lookupTypes);
	}
	
	/**
	 * Constructs a VarHandle that can access elements of a byte array as wider types.
	 * 
	 * @param viewArrayType The component type used when viewing elements of the array. 
	 */
	ByteArrayViewVarHandle(Class<?> viewArrayType, ByteOrder byteOrder) {
		super(viewArrayType, COORDINATE_TYPES, populateMHs(viewArrayType, byteOrder), 0);
	}

	/**
	 * Type specific methods used by byte array view VarHandle methods.
	 */
	@SuppressWarnings("unused")
	static class ByteArrayViewVarHandleOperations extends ViewVarHandle.ViewVarHandleOperations {
		private static final int INDEX_OFFSET = Unsafe.ARRAY_BYTE_BASE_OFFSET;
		
		private static final long computeOffset(int index) {
			return (long)INDEX_OFFSET + index;
		}

		static long checkAndComputeOffset(byte[] receiver, int viewTypeSize, int index, boolean allowUnaligned) {
			long offset = computeOffset(index);
			
			receiver.getClass();
			boundsCheck(receiver.length, viewTypeSize, index);
			alignmentCheck(offset, viewTypeSize, allowUnaligned);
			
			return offset;
		}
		
		static final class OpChar extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Character.BYTES;
			
			private static final char get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				return _unsafe.getChar(receiver, offset);
			}

			private static final void set(byte[] receiver, int index, char value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putChar(receiver, offset, value);
			}

			private static final char getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getCharVolatile(receiver, offset);
			}

			private static final void setVolatile(byte[] receiver, int index, char value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putCharVolatile(receiver, offset, value);
			}

			private static final char getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getCharOpaque(receiver, offset);
			}

			private static final void setOpaque(byte[] receiver, int index, char value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putCharOpaque(receiver, offset, value);
			}

			private static final char getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getCharAcquire(receiver, offset);
			}

			private static final void setRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putCharRelease(receiver, offset, value);
			}

			private static final boolean compareAndSet(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchange(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchangeAcquire(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchangeRelease(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSet(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSet(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSetAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSetRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAdd(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAddAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAddRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAnd(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAndAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAndRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOr(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOrAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOrRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXor(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXorAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXorRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpDouble extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Double.BYTES;
			
			private static final double get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				return _unsafe.getDouble(receiver, offset);
			}

			private static final void set(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putDouble(receiver, offset, value);
			}

			private static final double getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getDoubleVolatile(receiver, offset);
			}

			private static final void setVolatile(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putDoubleVolatile(receiver, offset, value);
			}

			private static final double getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getDoubleOpaque(receiver, offset);
			}

			private static final void setOpaque(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putDoubleOpaque(receiver, offset, value);
			}

			private static final double getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getDoubleAcquire(receiver, offset);
			}

			private static final void setRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putDoubleRelease(receiver, offset, value);
			}

			private static final boolean compareAndSet(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchange(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeDouble(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeDoubleVolatile(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchangeAcquire(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.compareAndExchangeDoubleAcquire(receiver, offset, testValue, newValue);
			}

			private static final double compareAndExchangeRelease(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.compareAndExchangeDoubleRelease(receiver, offset, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoublePlain(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoublePlain(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double getAndSet(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetDouble(receiver, offset, value);
			}

			private static final double getAndSetAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetDoubleAcquire(receiver, offset, value);
			}

			private static final double getAndSetRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetDoubleRelease(receiver, offset, value);
			}

			private static final double getAndAdd(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndAddAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndAddRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAnd(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOr(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXor(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpFloat extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Float.BYTES;
			
			private static final float get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				return _unsafe.getFloat(receiver, offset);
			}

			private static final void set(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putFloat(receiver, offset, value);
			}

			private static final float getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getFloatVolatile(receiver, offset);
			}

			private static final void setVolatile(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putFloatVolatile(receiver, offset, value);
			}

			private static final float getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getFloatOpaque(receiver, offset);
			}

			private static final void setOpaque(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putFloatOpaque(receiver, offset, value);
			}

			private static final float getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getFloatAcquire(receiver, offset);
			}

			private static final void setRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putFloatRelease(receiver, offset, value);
			}

			private static final boolean compareAndSet(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapFloat(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchange(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeFloat(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeFloatVolatile(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchangeAcquire(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.compareAndExchangeFloatAcquire(receiver, offset, testValue, newValue);
			}

			private static final float compareAndExchangeRelease(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.compareAndExchangeFloatRelease(receiver, offset, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetFloatRelease(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float getAndSet(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetFloat(receiver, offset, value);
			}

			private static final float getAndSetAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetFloatAcquire(receiver, offset, value);
			}

			private static final float getAndSetRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetFloatRelease(receiver, offset, value);
			}
		
			private static final float getAndAdd(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndAddAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndAddRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAnd(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOr(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXor(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpInt extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Integer.BYTES;
			
			private static final int get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				return _unsafe.getInt(receiver, offset);
			}

			private static final void set(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putInt(receiver, offset, value);
			}

			private static final int getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getIntVolatile(receiver, offset);
			}

			private static final void setVolatile(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putIntVolatile(receiver, offset, value);
			}

			private static final int getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getIntOpaque(receiver, offset);
			}

			private static final void setOpaque(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putIntOpaque(receiver, offset, value);
			}

			private static final int getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getIntAcquire(receiver, offset);
			}

			private static final void setRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putIntRelease(receiver, offset, value);
			}

			private static final boolean compareAndSet(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, offset, testValue, newValue);
/*[ENDIF]*/
			}

			private static final int compareAndExchange(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeInt(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeIntVolatile(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchangeAcquire(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.compareAndExchangeIntAcquire(receiver, offset, testValue, newValue);
			}

			private static final int compareAndExchangeRelease(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.compareAndExchangeIntRelease(receiver, offset, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int getAndSet(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetInt(receiver, offset, value);
			}

			private static final int getAndSetAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetIntAcquire(receiver, offset, value);
			}

			private static final int getAndSetRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetIntRelease(receiver, offset, value);
			}

			private static final int getAndAdd(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndAddInt(receiver, offset, value);
			}

			private static final int getAndAddAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndAddIntAcquire(receiver, offset, value);
			}

			private static final int getAndAddRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndAddIntRelease(receiver, offset, value);
			}

			private static final int getAndBitwiseAnd(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseAndInt(receiver, offset, value);
			}

			private static final int getAndBitwiseAndAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseAndIntAcquire(receiver, offset, value);
			}

			private static final int getAndBitwiseAndRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseAndIntRelease(receiver, offset, value);
			}

			private static final int getAndBitwiseOr(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseOrInt(receiver, offset, value);
			}

			private static final int getAndBitwiseOrAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseOrIntAcquire(receiver, offset, value);
			}

			private static final int getAndBitwiseOrRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseOrIntRelease(receiver, offset, value);
			}

			private static final int getAndBitwiseXor(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseXorInt(receiver, offset, value);
			}

			private static final int getAndBitwiseXorAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseXorIntAcquire(receiver, offset, value);
			}

			private static final int getAndBitwiseXorRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseXorIntRelease(receiver, offset, value);
			}
		}
		
		static final class OpLong extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Long.BYTES;
			
			private static final long get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				return _unsafe.getLong(receiver, offset);
			}

			private static final void set(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putLong(receiver, offset, value);
			}

			private static final long getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getLongVolatile(receiver, offset);
			}

			private static final void setVolatile(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putLongVolatile(receiver, offset, value);
			}

			private static final long getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getLongOpaque(receiver, offset);
			}

			private static final void setOpaque(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putLongOpaque(receiver, offset, value);
			}

			private static final long getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getLongAcquire(receiver, offset);
			}

			private static final void setRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putLongRelease(receiver, offset, value);
			}

			private static final boolean compareAndSet(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapLong(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long compareAndExchange(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeLong(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeLongVolatile(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long compareAndExchangeAcquire(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.compareAndExchangeLongAcquire(receiver, offset, testValue, newValue);
			}

			private static final long compareAndExchangeRelease(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.compareAndExchangeLongRelease(receiver, offset, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongAcquire(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongRelease(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long getAndSet(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetLong(receiver, offset, value);
			}

			private static final long getAndSetAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetLongAcquire(receiver, offset, value);
			}

			private static final long getAndSetRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndSetLongRelease(receiver, offset, value);
			}

			private static final long getAndAdd(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndAddLong(receiver, offset, value);
			}

			private static final long getAndAddAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndAddLongAcquire(receiver, offset, value);
			}

			private static final long getAndAddRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndAddLongRelease(receiver, offset, value);
			}

			private static final long getAndBitwiseAnd(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseAndLong(receiver, offset, value);
			}

			private static final long getAndBitwiseAndAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseAndLongAcquire(receiver, offset, value);
			}

			private static final long getAndBitwiseAndRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseAndLongRelease(receiver, offset, value);
			}

			private static final long getAndBitwiseOr(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseOrLong(receiver, offset, value);
			}

			private static final long getAndBitwiseOrAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseOrLongAcquire(receiver, offset, value);
			}

			private static final long getAndBitwiseOrRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseOrLongRelease(receiver, offset, value);
			}

			private static final long getAndBitwiseXor(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseXorLong(receiver, offset, value);
			}

			private static final long getAndBitwiseXorAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseXorLongAcquire(receiver, offset, value);
			}

			private static final long getAndBitwiseXorRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getAndBitwiseXorLongRelease(receiver, offset, value);
			}
		}
		
		static final class OpShort extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Short.BYTES;
			
			private static final short get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				return _unsafe.getShort(receiver, offset);
			}

			private static final void set(byte[] receiver, int index, short value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putShort(receiver, offset, value);
			}

			private static final short getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getShortVolatile(receiver, offset);
			}

			private static final void setVolatile(byte[] receiver, int index, short value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putShortVolatile(receiver, offset, value);
			}

			private static final short getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getShortOpaque(receiver, offset);
			}

			private static final void setOpaque(byte[] receiver, int index, short value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putShortOpaque(receiver, offset, value);
			}

			private static final short getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				return _unsafe.getShortAcquire(receiver, offset);
			}

			private static final void setRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putShortRelease(receiver, offset, value);
			}

			private static final boolean compareAndSet(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchange(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchangeAcquire(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchangeRelease(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSet(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSet(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSetAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSetRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAdd(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAddAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAddRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAnd(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAndAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAndRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOr(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOrAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOrRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXor(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXorAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXorRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpCharConvertEndian extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Character.BYTES;
			
			private static final char get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				char result = _unsafe.getChar(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void set(byte[] receiver, int index, char value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putChar(receiver, offset, convertEndian(value));
			}
		
			private static final char getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				char result = _unsafe.getCharVolatile(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setVolatile(byte[] receiver, int index, char value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putCharVolatile(receiver, offset, convertEndian(value));
			}
		
			private static final char getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				char result = _unsafe.getCharOpaque(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setOpaque(byte[] receiver, int index, char value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putCharOpaque(receiver, offset, convertEndian(value));
			}
		
			private static final char getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				char result = _unsafe.getCharAcquire(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putCharRelease(receiver, offset, convertEndian(value));
			}
		
			private static final boolean compareAndSet(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchange(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchangeAcquire(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchangeRelease(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSet(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSet(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSetAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSetRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAdd(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAddAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAddRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAnd(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAndAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAndRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOr(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOrAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOrRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXor(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXorAcquire(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXorRelease(byte[] receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpDoubleConvertEndian extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Double.BYTES;
			
			private static final double get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				double result = _unsafe.getDouble(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void set(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putDouble(receiver, offset, convertEndian(value));
			}
		
			private static final double getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				double result = _unsafe.getDoubleVolatile(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setVolatile(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putDoubleVolatile(receiver, offset, convertEndian(value));
			}
		
			private static final double getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				double result = _unsafe.getDoubleOpaque(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setOpaque(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putDoubleOpaque(receiver, offset, convertEndian(value));
			}
		
			private static final double getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				double result = _unsafe.getDoubleAcquire(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putDoubleRelease(receiver, offset, convertEndian(value));
			}
		
			private static final boolean compareAndSet(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.compareAndSwapDouble(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final double compareAndExchange(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				double result = _unsafe.compareAndExchangeDouble(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				double result = _unsafe.compareAndExchangeDoubleVolatile(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
				return convertEndian(result);
			}
		
			private static final double compareAndExchangeAcquire(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				double result = _unsafe.compareAndExchangeDoubleAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}
		
			private static final double compareAndExchangeRelease(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				double result = _unsafe.compareAndExchangeDoubleRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}
		
			private static final boolean weakCompareAndSet(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoublePlain(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetDoublePlain(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final double getAndSet(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				double result = _unsafe.getAndSetDouble(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final double getAndSetAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				double result = _unsafe.getAndSetDoubleAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final double getAndSetRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				double result = _unsafe.getAndSetDoubleRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final double getAndAdd(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndAddAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndAddRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndBitwiseAnd(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndBitwiseAndAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndBitwiseAndRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndBitwiseOr(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndBitwiseOrAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndBitwiseOrRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndBitwiseXor(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndBitwiseXorAcquire(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final double getAndBitwiseXorRelease(byte[] receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpFloatConvertEndian extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Float.BYTES;
			
			private static final float get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				float result = _unsafe.getFloat(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void set(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putFloat(receiver, offset, convertEndian(value));
			}
		
			private static final float getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				float result = _unsafe.getFloatVolatile(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setVolatile(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putFloatVolatile(receiver, offset, convertEndian(value));
			}
		
			private static final float getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				float result = _unsafe.getFloatOpaque(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setOpaque(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putFloatOpaque(receiver, offset, convertEndian(value));
			}
		
			private static final float getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				float result = _unsafe.getFloatAcquire(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putFloatRelease(receiver, offset, convertEndian(value));
			}
		
			private static final boolean compareAndSet(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.compareAndSwapFloat(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final float compareAndExchange(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/
				float result = _unsafe.compareAndExchangeFloat(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				float result = _unsafe.compareAndExchangeFloatVolatile(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/
				return convertEndian(result);
			}
		
			private static final float compareAndExchangeAcquire(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				float result = _unsafe.compareAndExchangeFloatAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}
		
			private static final float compareAndExchangeRelease(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				float result = _unsafe.compareAndExchangeFloatRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}
		
			private static final boolean weakCompareAndSet(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetFloatPlain(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetFloatRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final float getAndSet(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				float result = _unsafe.getAndSetFloat(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final float getAndSetAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				float result = _unsafe.getAndSetFloatAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final float getAndSetRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				float result = _unsafe.getAndSetFloatRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final float getAndAdd(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndAddAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndAddRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndBitwiseAnd(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndBitwiseAndAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndBitwiseAndRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndBitwiseOr(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndBitwiseOrAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndBitwiseOrRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndBitwiseXor(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndBitwiseXorAcquire(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		
			private static final float getAndBitwiseXorRelease(byte[] receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}

		static final class OpIntConvertEndian extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Integer.BYTES;
			
			private static final int get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				int result = _unsafe.getInt(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void set(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putInt(receiver, offset, convertEndian(value));
			}
		
			private static final int getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getIntVolatile(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setVolatile(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putIntVolatile(receiver, offset, convertEndian(value));
			}
		
			private static final int getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getIntOpaque(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setOpaque(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putIntOpaque(receiver, offset, convertEndian(value));
			}
		
			private static final int getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getIntAcquire(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putIntRelease(receiver, offset, convertEndian(value));
			}
		
			private static final boolean compareAndSet(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.compareAndSwapInt(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final int compareAndExchange(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				int result = _unsafe.compareAndExchangeInt(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				int result = _unsafe.compareAndExchangeIntVolatile(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
				return convertEndian(result);
			}
		
			private static final int compareAndExchangeAcquire(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.compareAndExchangeIntAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}
		
			private static final int compareAndExchangeRelease(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.compareAndExchangeIntRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}
		
			private static final boolean weakCompareAndSet(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetIntPlain(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetIntPlain(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final int getAndSet(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndSetInt(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndSetAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndSetIntAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndSetRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndSetIntRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndAdd(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndAddInt(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndAddAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndAddIntAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndAddRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndAddIntRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndBitwiseAnd(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndBitwiseAndInt(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndBitwiseAndAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndBitwiseAndIntAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndBitwiseAndRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndBitwiseAndIntRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndBitwiseOr(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndBitwiseOrInt(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndBitwiseOrAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndBitwiseOrIntAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndBitwiseOrRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndBitwiseOrIntRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndBitwiseXor(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndBitwiseXorInt(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndBitwiseXorAcquire(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndBitwiseXorIntAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final int getAndBitwiseXorRelease(byte[] receiver, int index, int value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				int result = _unsafe.getAndBitwiseXorIntRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		}

		static final class OpLongConvertEndian extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Long.BYTES;
			
			private static final long get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				long result = _unsafe.getLong(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void set(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putLong(receiver, offset, convertEndian(value));
			}
		
			private static final long getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getLongVolatile(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setVolatile(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putLongVolatile(receiver, offset, convertEndian(value));
			}
		
			private static final long getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getLongOpaque(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setOpaque(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putLongOpaque(receiver, offset, convertEndian(value));
			}
		
			private static final long getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getLongAcquire(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putLongRelease(receiver, offset, convertEndian(value));
			}
		
			private static final boolean compareAndSet(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.compareAndSwapLong(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final long compareAndExchange(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				long result = _unsafe.compareAndExchangeLong(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				long result = _unsafe.compareAndExchangeLongVolatile(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
				return convertEndian(result);
			}
		
			private static final long compareAndExchangeAcquire(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.compareAndExchangeLongAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}
		
			private static final long compareAndExchangeRelease(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.compareAndExchangeLongRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}
		
			private static final boolean weakCompareAndSet(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetLongPlain(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongPlain(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(receiver, offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}
		
			private static final long getAndSet(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndSetLong(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndSetAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndSetLongAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndSetRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndSetLongRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndAdd(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndAddLong(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndAddAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndAddLongAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndAddRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndAddLongRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndBitwiseAnd(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndBitwiseAndLong(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndBitwiseAndAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndBitwiseAndLongAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndBitwiseAndRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndBitwiseAndLongRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndBitwiseOr(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndBitwiseOrLong(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndBitwiseOrAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndBitwiseOrLongAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndBitwiseOrRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndBitwiseOrLongRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndBitwiseXor(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndBitwiseXorLong(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndBitwiseXorAcquire(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndBitwiseXorLongAcquire(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		
			private static final long getAndBitwiseXorRelease(byte[] receiver, int index, long value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				long result = _unsafe.getAndBitwiseXorLongRelease(receiver, offset, convertEndian(value));
				return convertEndian(result);
			}
		}

		static final class OpShortConvertEndian extends ByteArrayViewVarHandleOperations {
			private static final int BYTES = Short.BYTES;
			
			private static final short get(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				short result = _unsafe.getShort(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void set(byte[] receiver, int index, short value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, true);
				_unsafe.putShort(receiver, offset, convertEndian(value));
			}
		
			private static final short getVolatile(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				short result = _unsafe.getShortVolatile(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setVolatile(byte[] receiver, int index, short value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putShortVolatile(receiver, offset, convertEndian(value));
			}
		
			private static final short getOpaque(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				short result = _unsafe.getShortOpaque(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setOpaque(byte[] receiver, int index, short value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putShortOpaque(receiver, offset, convertEndian(value));
			}
		
			private static final short getAcquire(byte[] receiver, int index, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				short result = _unsafe.getShortAcquire(receiver, offset);
				return convertEndian(result);
			}
		
			private static final void setRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				long offset = checkAndComputeOffset(receiver, BYTES, index, false);
				_unsafe.putShortRelease(receiver, offset, convertEndian(value));
			}
			
			private static final boolean compareAndSet(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchange(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchangeAcquire(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchangeRelease(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSet(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetAcquire(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetRelease(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetPlain(byte[] receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSet(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSetAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSetRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAdd(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAddAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAddRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAnd(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAndAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAndRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOr(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOrAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOrRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXor(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXorAcquire(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXorRelease(byte[] receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
	}
}
