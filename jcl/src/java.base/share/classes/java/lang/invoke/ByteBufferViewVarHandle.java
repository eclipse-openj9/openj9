/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

import static java.lang.invoke.ByteBufferViewVarHandle.ByteBufferViewVarHandleOperations.*;
import static java.lang.invoke.MethodHandles.Lookup.internalPrivilegedLookup;
import static java.lang.invoke.MethodType.methodType;

import com.ibm.oti.util.Msg;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.ReadOnlyBufferException;
import jdk.internal.misc.Unsafe;

final class ByteBufferViewVarHandle extends ViewVarHandle {
	private final static Class<?>[] COORDINATE_TYPES = new Class<?>[] {ByteBuffer.class, int.class};
	
	/**
	 * Populates the static MethodHandle[] corresponding to the provided type. 
	 * 
	 * @param type The type to create MethodHandles for.
	 * @param byteOrder The byteOrder of the ByteBuffer(s) that will be used.
	 * @return The populated MethodHandle[].
	 */
	static final MethodHandle[] populateMHs(Class<?> type, ByteOrder byteOrder) {
		Class<? extends ByteBufferViewVarHandleOperations> operationsClass = null;
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
		
		MethodType getter = methodType(type, ByteBuffer.class, int.class, VarHandle.class);
		MethodType setter = methodType(void.class, ByteBuffer.class, int.class, type, VarHandle.class);
		MethodType compareAndSet = methodType(boolean.class, ByteBuffer.class, int.class, type, type, VarHandle.class);
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
	ByteBufferViewVarHandle(Class<?> viewArrayType, ByteOrder byteOrder) {
		super(viewArrayType, COORDINATE_TYPES, populateMHs(viewArrayType, byteOrder), 0);
	}

	/**
	 * Type specific methods used by ByteBufferViewVarHandle methods.
	 */
	@SuppressWarnings("unused")
	static class ByteBufferViewVarHandleOperations extends ViewVarHandle.ViewVarHandleOperations {
		
		/**
		 * A ByteBuffer may be on-heap or off-heap. On-heap buffers are backed by a byte[], 
		 * and off-heap buffers have a base memory address. This class abstracts away the
		 * difference so that a buffer element can simply be referenced by base and offset.
		 */
		private static class BufferElement {
			private static final int INDEX_OFFSET = Unsafe.ARRAY_BYTE_BASE_OFFSET;
			private static final MethodHandle directBufferAddressMH;
			private static final MethodHandle onHeapBufferArrayMH;
			private static final MethodHandle bufferOffsetMH;
			
			final Object base;
			final long offset;
			
			static {
				MethodHandle addressMH = null;
				MethodHandle arrayMH = null;
				MethodHandle offsetMH = null;
				try {
					addressMH = internalPrivilegedLookup.findGetter(ByteBuffer.class, "address", long.class);
					arrayMH = internalPrivilegedLookup.findGetter(ByteBuffer.class, "hb", byte[].class);
					offsetMH = internalPrivilegedLookup.findGetter(ByteBuffer.class, "offset", int.class);
				} catch (Throwable t) {
					throw new InternalError("Could not create MethodHandles for ByteBuffer fields", t);
				}
				directBufferAddressMH = addressMH;
				onHeapBufferArrayMH = arrayMH;
				bufferOffsetMH = offsetMH;
			}
			
			BufferElement(ByteBuffer buffer, int viewTypeSize, int index) {
				if (buffer.isDirect()) {
					this.base = null;
					this.offset = computeAddress(buffer, index, viewTypeSize);
				} else {
					this.base = getArray(buffer);
					this.offset = computeOffset(buffer, index, viewTypeSize);
				}
			}
			
			private final byte[] getArray(ByteBuffer buffer) {
				try {
					return (byte[])onHeapBufferArrayMH.invokeExact(buffer);
				} catch (Throwable e) {
					throw new InternalError("Could not get ByteBuffer backing array", e);
				}
			}
			
			private static final long computeOffset(ByteBuffer buffer, int index, int viewTypeSize) {
				int arrayOffset = 0;
				try {
					arrayOffset = (int)bufferOffsetMH.invokeExact(buffer);
				} catch (Throwable e) {
					throw new InternalError("Could not get ByteBuffer offset", e);
				}
				return (long)INDEX_OFFSET + arrayOffset + index;
			}
			
			private static final long computeAddress(ByteBuffer buffer, int index, int viewTypeSize) {
				long base = 0;
				try {
					base = (long)directBufferAddressMH.invokeExact(buffer);
				} catch (Throwable e) {
					throw new InternalError("Could not get ByteBuffer address", e);
				}
				return (long)base + index;
			}
		}

		static BufferElement checkAndGetBufferElement(ByteBuffer receiver, int viewTypeSize, int index, boolean readOnlyOperation, boolean allowUnaligned) {
			receiver.getClass();
			boundsCheck(receiver.limit(), viewTypeSize, index);
			if ((!readOnlyOperation) && receiver.isReadOnly()) {
				throw new ReadOnlyBufferException();
			}
			BufferElement be = new BufferElement(receiver, viewTypeSize, index);
			alignmentCheck(be.offset, viewTypeSize, allowUnaligned);
			return be;
		}

		static final class OpChar extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Character.BYTES;
			
			private static final char get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				return _unsafe.getChar(be.base, be.offset);

			}

			private static final void set(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putChar(be.base, be.offset, value);
			}

			private static final char getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getCharVolatile(be.base, be.offset);

			}

			private static final void setVolatile(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putCharVolatile(be.base, be.offset, value);
			}

			private static final char getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getCharOpaque(be.base, be.offset);

			}

			private static final void setOpaque(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putCharOpaque(be.base, be.offset, value);
			}

			private static final char getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getCharAcquire(be.base, be.offset);

			}

			private static final void setRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putCharRelease(be.base, be.offset, value);
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchange(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchangeAcquire(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchangeRelease(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSet(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSetAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSetRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAdd(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAddAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAddRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAnd(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAndAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAndRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOr(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOrAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOrRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXor(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXorAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXorRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpDouble extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Double.BYTES;
			
			private static final double get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				return _unsafe.getDouble(be.base, be.offset);
			}

			private static final void set(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putDouble(be.base, be.offset, value);
			}

			private static final double getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getDoubleVolatile(be.base, be.offset);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putDoubleVolatile(be.base, be.offset, value);
			}

			private static final double getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getDoubleOpaque(be.base, be.offset);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putDoubleOpaque(be.base, be.offset, value);
			}

			private static final double getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getDoubleAcquire(be.base, be.offset);
			}

			private static final void setRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putDoubleRelease(be.base, be.offset, value);
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetDouble(be.base, be.offset, testValue, newValue);	
/*[ELSE]
				return _unsafe.compareAndSwapDouble(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchange(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeDouble(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeDoubleVolatile(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final double compareAndExchangeAcquire(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.compareAndExchangeDoubleAcquire(be.base, be.offset, testValue, newValue);
			}

			private static final double compareAndExchangeRelease(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.compareAndExchangeDoubleRelease(be.base, be.offset, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoublePlain(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoublePlain(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/
			}

			private static final double getAndSet(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetDouble(be.base, be.offset, value);
			}

			private static final double getAndSetAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetDoubleAcquire(be.base, be.offset, value);
			}

			private static final double getAndSetRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetDoubleRelease(be.base, be.offset, value);
			}

			private static final double getAndAdd(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndAddAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndAddRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAnd(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOr(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXor(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpFloat extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Float.BYTES;
			
			private static final float get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				return _unsafe.getFloat(be.base, be.offset);
			}

			private static final void set(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putFloat(be.base, be.offset, value);
			}

			private static final float getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getFloatVolatile(be.base, be.offset);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putFloatVolatile(be.base, be.offset, value);
			}

			private static final float getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getFloatOpaque(be.base, be.offset);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putFloatOpaque(be.base, be.offset, value);
			}

			private static final float getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getFloatAcquire(be.base, be.offset);
			}

			private static final void setRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putFloatRelease(be.base, be.offset, value);
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetFloat(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapFloat(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchange(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeFloat(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeFloatVolatile(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float compareAndExchangeAcquire(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.compareAndExchangeFloatAcquire(be.base, be.offset, testValue, newValue);
			}

			private static final float compareAndExchangeRelease(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.compareAndExchangeFloatRelease(be.base, be.offset, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatPlain(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatRelease(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatPlain(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final float getAndSet(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetFloat(be.base, be.offset, value);
			}

			private static final float getAndSetAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetFloatAcquire(be.base, be.offset, value);
			}

			private static final float getAndSetRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetFloatRelease(be.base, be.offset, value);
			}

			private static final float getAndAdd(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndAddAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndAddRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAnd(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOr(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXor(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpInt extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Integer.BYTES;
			
			private static final int get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				return _unsafe.getInt(be.base, be.offset);
			}

			private static final void set(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putInt(be.base, be.offset, value);
			}

			private static final int getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getIntVolatile(be.base, be.offset);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putIntVolatile(be.base, be.offset, value);
			}

			private static final int getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getIntOpaque(be.base, be.offset);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putIntOpaque(be.base, be.offset, value);
			}

			private static final int getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getIntAcquire(be.base, be.offset);
			}

			private static final void setRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putIntRelease(be.base, be.offset, value);
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetInt(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchange(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeInt(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeIntVolatile(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int compareAndExchangeAcquire(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.compareAndExchangeIntAcquire(be.base, be.offset, testValue, newValue);
			}

			private static final int compareAndExchangeRelease(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.compareAndExchangeIntRelease(be.base, be.offset, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntPlain(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntAcquire(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntRelease(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntPlain(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final int getAndSet(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetInt(be.base, be.offset, value);
			}

			private static final int getAndSetAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetIntAcquire(be.base, be.offset, value);
			}

			private static final int getAndSetRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetIntRelease(be.base, be.offset, value);
			}

			private static final int getAndAdd(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndAddInt(be.base, be.offset, value);
			}

			private static final int getAndAddAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndAddIntAcquire(be.base, be.offset, value);
			}

			private static final int getAndAddRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndAddIntRelease(be.base, be.offset, value);
			}

			private static final int getAndBitwiseAnd(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseAndInt(be.base, be.offset, value);
			}

			private static final int getAndBitwiseAndAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseAndIntAcquire(be.base, be.offset, value);
			}

			private static final int getAndBitwiseAndRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseAndIntRelease(be.base, be.offset, value);
			}

			private static final int getAndBitwiseOr(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseOrInt(be.base, be.offset, value);
			}

			private static final int getAndBitwiseOrAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseOrIntAcquire(be.base, be.offset, value);
			}

			private static final int getAndBitwiseOrRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseOrIntRelease(be.base, be.offset, value);
			}

			private static final int getAndBitwiseXor(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseXorInt(be.base, be.offset, value);
			}

			private static final int getAndBitwiseXorAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseXorIntAcquire(be.base, be.offset, value);
			}

			private static final int getAndBitwiseXorRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseXorIntRelease(be.base, be.offset, value);
			}
		}
		
		static final class OpLong extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Long.BYTES;
			
			private static final long get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				return _unsafe.getLong(be.base, be.offset);
			}

			private static final void set(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putLong(be.base, be.offset, value);
			}

			private static final long getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getLongVolatile(be.base, be.offset);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putLongVolatile(be.base, be.offset, value);
			}

			private static final long getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getLongOpaque(be.base, be.offset);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putLongOpaque(be.base, be.offset, value);
			}

			private static final long getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getLongAcquire(be.base, be.offset);
			}

			private static final void setRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putLongRelease(be.base, be.offset, value);
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetLong(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapLong(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long compareAndExchange(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndExchangeLong(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeLongVolatile(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long compareAndExchangeAcquire(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.compareAndExchangeLongAcquire(be.base, be.offset, testValue, newValue);
			}

			private static final long compareAndExchangeRelease(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.compareAndExchangeLongRelease(be.base, be.offset, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongPlain(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongAcquire(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongRelease(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongPlain(be.base, be.offset, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(be.base, be.offset, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final long getAndSet(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetLong(be.base, be.offset, value);
			}

			private static final long getAndSetAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetLongAcquire(be.base, be.offset, value);
			}

			private static final long getAndSetRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndSetLongRelease(be.base, be.offset, value);
			}

			private static final long getAndAdd(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndAddLong(be.base, be.offset, value);
			}

			private static final long getAndAddAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndAddLongAcquire(be.base, be.offset, value);
			}

			private static final long getAndAddRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndAddLongRelease(be.base, be.offset, value);
			}

			private static final long getAndBitwiseAnd(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseAndLong(be.base, be.offset, value);
			}

			private static final long getAndBitwiseAndAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseAndLongAcquire(be.base, be.offset, value);
			}

			private static final long getAndBitwiseAndRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseAndLongRelease(be.base, be.offset, value);
			}

			private static final long getAndBitwiseOr(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseOrLong(be.base, be.offset, value);
			}

			private static final long getAndBitwiseOrAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseOrLongAcquire(be.base, be.offset, value);
			}

			private static final long getAndBitwiseOrRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseOrLongRelease(be.base, be.offset, value);
			}

			private static final long getAndBitwiseXor(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseXorLong(be.base, be.offset, value);
			}

			private static final long getAndBitwiseXorAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseXorLongAcquire(be.base, be.offset, value);
			}

			private static final long getAndBitwiseXorRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				return _unsafe.getAndBitwiseXorLongRelease(be.base, be.offset, value);
			}
		}
		
		static final class OpShort extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Short.BYTES;
			
			private static final short get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				return _unsafe.getShort(be.base, be.offset);
			}

			private static final void set(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putShort(be.base, be.offset, value);
			}

			private static final short getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getShortVolatile(be.base, be.offset);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putShortVolatile(be.base, be.offset, value);
			}

			private static final short getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getShortOpaque(be.base, be.offset);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putShortOpaque(be.base, be.offset, value);
			}

			private static final short getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				return _unsafe.getShortAcquire(be.base, be.offset);
			}

			private static final void setRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putShortRelease(be.base, be.offset, value);
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchange(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchangeAcquire(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchangeRelease(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSet(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSetAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSetRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAdd(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAddAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAddRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAnd(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAndAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAndRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOr(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOrAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOrRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXor(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXorAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXorRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpCharConvertEndian extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Character.BYTES;
			
			private static final char get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				char result = _unsafe.getChar(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void set(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putChar(be.base, be.offset, convertEndian(value));
			}

			private static final char getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				char result = _unsafe.getCharVolatile(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putCharVolatile(be.base, be.offset, convertEndian(value));
			}

			private static final char getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				char result = _unsafe.getCharOpaque(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putCharOpaque(be.base, be.offset, convertEndian(value));
			}

			private static final char getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				char result = _unsafe.getCharAcquire(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putCharRelease(be.base, be.offset, convertEndian(value));
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchange(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchangeAcquire(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char compareAndExchangeRelease(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, char testValue, char newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSet(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSetAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndSetRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAdd(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAddAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndAddRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAnd(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAndAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseAndRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOr(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOrAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseOrRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXor(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXorAcquire(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final char getAndBitwiseXorRelease(ByteBuffer receiver, int index, char value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpDoubleConvertEndian extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Double.BYTES;
			
			private static final double get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				double result = _unsafe.getDouble(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void set(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putDouble(be.base, be.offset, convertEndian(value));
			}

			private static final double getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				double result = _unsafe.getDoubleVolatile(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putDoubleVolatile(be.base, be.offset, convertEndian(value));
			}

			private static final double getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				double result = _unsafe.getDoubleOpaque(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putDoubleOpaque(be.base, be.offset, convertEndian(value));
			}

			private static final double getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				double result = _unsafe.getDoubleAcquire(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putDoubleRelease(be.base, be.offset, convertEndian(value));
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetDouble(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.compareAndSwapDouble(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final double compareAndExchange(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				double result = _unsafe.compareAndExchangeDouble(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				double result = _unsafe.compareAndExchangeDoubleVolatile(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
				return convertEndian(result);
			}

			private static final double compareAndExchangeAcquire(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				double result = _unsafe.compareAndExchangeDoubleAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}

			private static final double compareAndExchangeRelease(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				double result = _unsafe.compareAndExchangeDoubleRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoublePlain(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, double testValue, double newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetDoublePlain(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/
			}

			private static final double getAndSet(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				double result = _unsafe.getAndSetDouble(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final double getAndSetAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				double result = _unsafe.getAndSetDoubleAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final double getAndSetRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				double result = _unsafe.getAndSetDoubleRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final double getAndAdd(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndAddAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndAddRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAnd(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseAndRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOr(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseOrRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXor(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorAcquire(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final double getAndBitwiseXorRelease(ByteBuffer receiver, int index, double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpFloatConvertEndian extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Float.BYTES;
			
			private static final float get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				float result = _unsafe.getFloat(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void set(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putFloat(be.base, be.offset, convertEndian(value));
			}

			private static final float getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				float result = _unsafe.getFloatVolatile(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putFloatVolatile(be.base, be.offset, convertEndian(value));
			}

			private static final float getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				float result = _unsafe.getFloatOpaque(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putFloatOpaque(be.base, be.offset, convertEndian(value));
			}

			private static final float getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				float result = _unsafe.getFloatAcquire(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putFloatRelease(be.base, be.offset, convertEndian(value));
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetFloat(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.compareAndSwapFloat(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final float compareAndExchange(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/
				float result = _unsafe.compareAndExchangeFloat(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				float result = _unsafe.compareAndExchangeFloatVolatile(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
				return convertEndian(result);
			}

			private static final float compareAndExchangeAcquire(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				float result = _unsafe.compareAndExchangeFloatAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}

			private static final float compareAndExchangeRelease(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				float result = _unsafe.compareAndExchangeFloatRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatPlain(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetFloatRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, float testValue, float newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.weakCompareAndSetFloatPlain(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final float getAndSet(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				float result = _unsafe.getAndSetFloat(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final float getAndSetAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				float result = _unsafe.getAndSetFloatAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final float getAndSetRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				float result = _unsafe.getAndSetFloatRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final float getAndAdd(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndAddAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndAddRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAnd(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseAndRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOr(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseOrRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXor(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorAcquire(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final float getAndBitwiseXorRelease(ByteBuffer receiver, int index, float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpIntConvertEndian extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Integer.BYTES;
			
			private static final int get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				int result = _unsafe.getInt(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void set(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putInt(be.base, be.offset, convertEndian(value));
			}

			private static final int getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				int result = _unsafe.getIntVolatile(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putIntVolatile(be.base, be.offset, convertEndian(value));
			}

			private static final int getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				int result = _unsafe.getIntOpaque(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putIntOpaque(be.base, be.offset, convertEndian(value));
			}

			private static final int getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				int result = _unsafe.getIntAcquire(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putIntRelease(be.base, be.offset, convertEndian(value));
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetInt(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.compareAndSwapInt(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final int compareAndExchange(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/
				int result = _unsafe.compareAndExchangeInt(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				int result = _unsafe.compareAndExchangeIntVolatile(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
				return convertEndian(result);
			}

			private static final int compareAndExchangeAcquire(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.compareAndExchangeIntAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}

			private static final int compareAndExchangeRelease(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.compareAndExchangeIntRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.weakCompareAndSetIntPlain(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetIntRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, int testValue, int newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.weakCompareAndSetIntPlain(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final int getAndSet(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndSetInt(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndSetAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndSetIntAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndSetRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndSetIntRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndAdd(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndAddInt(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndAddAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndAddIntAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndAddRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndAddIntRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndBitwiseAnd(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndBitwiseAndInt(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndBitwiseAndAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndBitwiseAndIntAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndBitwiseAndRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndBitwiseAndIntRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndBitwiseOr(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndBitwiseOrInt(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndBitwiseOrAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndBitwiseOrIntAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndBitwiseOrRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndBitwiseOrIntRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndBitwiseXor(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndBitwiseXorInt(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndBitwiseXorAcquire(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndBitwiseXorIntAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final int getAndBitwiseXorRelease(ByteBuffer receiver, int index, int value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				int result = _unsafe.getAndBitwiseXorIntRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}
		}
		
		static final class OpLongConvertEndian extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Long.BYTES;
			
			private static final long get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				long result = _unsafe.getLong(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void set(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putLong(be.base, be.offset, convertEndian(value));
			}

			private static final long getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				long result = _unsafe.getLongVolatile(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putLongVolatile(be.base, be.offset, convertEndian(value));
			}

			private static final long getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				long result = _unsafe.getLongOpaque(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putLongOpaque(be.base, be.offset, convertEndian(value));
			}

			private static final long getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				long result = _unsafe.getLongAcquire(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putLongRelease(be.base, be.offset, convertEndian(value));
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.compareAndSetLong(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.compareAndSwapLong(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final long compareAndExchange(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/
				long result = _unsafe.compareAndExchangeLong(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				long result = _unsafe.compareAndExchangeLongVolatile(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
				return convertEndian(result);
			}

			private static final long compareAndExchangeAcquire(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.compareAndExchangeLongAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}

			private static final long compareAndExchangeRelease(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.compareAndExchangeLongRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
				return convertEndian(result);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.weakCompareAndSetLongPlain(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/				
				return _unsafe.weakCompareAndSetLongRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, long testValue, long newValue, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
/*[IF Sidecar19-SE-OpenJ9]*/
				return _unsafe.weakCompareAndSetLongPlain(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(be.base, be.offset, convertEndian(testValue), convertEndian(newValue));
/*[ENDIF]*/				
			}

			private static final long getAndSet(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndSetLong(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndSetAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndSetLongAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndSetRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndSetLongRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndAdd(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndAddLong(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndAddAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndAddLongAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndAddRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndAddLongRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndBitwiseAnd(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndBitwiseAndLong(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndBitwiseAndAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndBitwiseAndLongAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndBitwiseAndRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndBitwiseAndLongRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndBitwiseOr(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndBitwiseOrLong(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndBitwiseOrAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndBitwiseOrLongAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndBitwiseOrRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndBitwiseOrLongRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndBitwiseXor(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndBitwiseXorLong(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndBitwiseXorAcquire(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndBitwiseXorLongAcquire(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}

			private static final long getAndBitwiseXorRelease(ByteBuffer receiver, int index, long value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				long result = _unsafe.getAndBitwiseXorLongRelease(be.base, be.offset, convertEndian(value));
				return convertEndian(result);
			}
		}
		
		static final class OpShortConvertEndian extends ByteBufferViewVarHandleOperations {
			private static final int BYTES = Short.BYTES;
			
			private static final short get(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, true);
				short result = _unsafe.getShort(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void set(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, true);
				_unsafe.putShort(be.base, be.offset, convertEndian(value));
			}

			private static final short getVolatile(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				short result = _unsafe.getShortVolatile(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setVolatile(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putShortVolatile(be.base, be.offset, convertEndian(value));
			}

			private static final short getOpaque(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				short result = _unsafe.getShortOpaque(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setOpaque(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putShortOpaque(be.base, be.offset, convertEndian(value));
			}

			private static final short getAcquire(ByteBuffer receiver, int index, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, true, false);
				short result = _unsafe.getShortAcquire(be.base, be.offset);
				return convertEndian(result);
			}

			private static final void setRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				BufferElement be = checkAndGetBufferElement(receiver, BYTES, index, false, false);
				_unsafe.putShortRelease(be.base, be.offset, convertEndian(value));
			}

			private static final boolean compareAndSet(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchange(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchangeAcquire(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short compareAndExchangeRelease(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSet(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetAcquire(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetRelease(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean weakCompareAndSetPlain(ByteBuffer receiver, int index, short testValue, short newValue, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSet(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSetAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndSetRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAdd(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAddAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndAddRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAnd(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAndAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseAndRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOr(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOrAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseOrRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXor(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXorAcquire(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final short getAndBitwiseXorRelease(ByteBuffer receiver, int index, short value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
	}
}
