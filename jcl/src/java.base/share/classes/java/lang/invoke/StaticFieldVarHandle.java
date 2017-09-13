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
 * {@link VarHandle} subclass for {@link VarHandle} instances for static field.
 */
final class StaticFieldVarHandle extends FieldVarHandle {
	private final static Class<?>[] COORDINATE_TYPES = new Class<?>[0];

	/**
	 * Populates a MethodHandle[] corresponding to the provided type. 
	 * 
	 * @param type The type to create MethodHandles for.
	 * @return The populated MethodHandle[].
	 */
	static final MethodHandle[] populateMHs(final Class<?> initType) {
		Class<? extends StaticFieldVarHandleOperations> operationsClass = null;
		Class<?> type = initType;
		
		if (!type.isPrimitive()) {
			type = Object.class;
			operationsClass = StaticFieldVarHandleOperations.OpObject.class;
		} else if (int.class == type) {
			operationsClass = StaticFieldVarHandleOperations.OpInt.class;
		} else if (long.class == type) {
			operationsClass = StaticFieldVarHandleOperations.OpLong.class;
		} else if (boolean.class == type) {
			operationsClass = StaticFieldVarHandleOperations.OpBoolean.class;
		} else if (byte.class == type) {
			operationsClass = StaticFieldVarHandleOperations.OpByte.class;
		} else if (char.class == type) {
			operationsClass = StaticFieldVarHandleOperations.OpChar.class;
		} else if (double.class == type) {
			operationsClass = StaticFieldVarHandleOperations.OpDouble.class;
		} else if (float.class == type) {
			operationsClass = StaticFieldVarHandleOperations.OpFloat.class;
		} else if (short.class == type) {
			operationsClass = StaticFieldVarHandleOperations.OpShort.class;
		} else if (void.class == type) {
			throw new NoSuchFieldError();
		} else {
			/*[MSG "K0626", "Unable to handle type {0}."]*/
			throw new InternalError(com.ibm.oti.util.Msg.getString("K0626", type)); //$NON-NLS-1$
		}

		MethodType getter = methodType(type, StaticFieldVarHandle.class);
		MethodType setter = methodType(void.class, type, StaticFieldVarHandle.class);
		MethodType compareAndSet = methodType(boolean.class, type, type, StaticFieldVarHandle.class);
		MethodType compareAndExchange = compareAndSet.changeReturnType(type);
		MethodType getAndSet = setter.changeReturnType(type);
		
		MethodType[] lookupTypes = populateMTs(getter, setter, compareAndSet, compareAndExchange, getAndSet);
		MethodType[] exactTypes = lookupTypes;
		
		if (initType != type) {
			MethodType exactGetter = methodType(initType, StaticFieldVarHandle.class);
			MethodType exactSetter = methodType(void.class, initType, StaticFieldVarHandle.class);
			MethodType exactCompareAndSet = methodType(boolean.class, initType, initType, StaticFieldVarHandle.class);
			MethodType exactCompareAndExchange = compareAndSet.changeReturnType(initType);
			MethodType exactGetAndSet = setter.changeReturnType(initType);
			
			exactTypes = populateMTs(exactGetter, exactSetter, exactCompareAndSet, exactCompareAndExchange, exactGetAndSet);
		}
				
		return populateMHs(operationsClass, lookupTypes, exactTypes);
	}
	
	/**
	 * Populates a MethodType[] corresponding to the provided type. 
	 * 
	 * @param type The type to create MethodType for.
	 * @return The populated MethodType[].
	 */
	static final MethodType[] populateMTs(Class<?> type) {
		MethodType getter = methodType(type);
		MethodType setter = methodType(void.class, type);
		MethodType compareAndSet = methodType(boolean.class, type, type);
		MethodType compareAndExchange = compareAndSet.changeReturnType(type);
		MethodType getAndSet = setter.changeReturnType(type);
		
		return populateMTs(getter, setter, compareAndSet, compareAndExchange, getAndSet);
	}

	/**
	 * Constructs a VarHandle to a static field.
	 * 
	 * @param lookupClass The class where we start the lookup of the field
	 * @param fieldName The field name
	 * @param fieldType The exact type of the field
	 * @param accessClass The class being used to look up the field
	 */
	StaticFieldVarHandle(Class<?> lookupClass, String fieldName, Class<?> fieldType, Class<?> accessClass) {
		super(lookupClass, fieldName, fieldType, accessClass, true, COORDINATE_TYPES, populateMHs(fieldType), populateMTs(fieldType));
	}
	
	/**
	 * Construct a VarHandle to the static field represented by the provided {@link java.lang.reflect.Field Field}.
	 * 
	 * @param field The {@link java.lang.reflect.Field} to create a VarHandle for.
	 */
	StaticFieldVarHandle(Field field, Class<?> fieldType) {
		super(field, true, COORDINATE_TYPES, populateMHs(fieldType), populateMTs(fieldType));
	}

	/**
	 * Type specific methods used by VarHandle methods for static field.
	 */
	@SuppressWarnings("unused")
	static class StaticFieldVarHandleOperations extends VarHandleOperations {
		static final class OpObject extends StaticFieldVarHandleOperations {
			private static final Object get(StaticFieldVarHandle varHandle) {
				return _unsafe.getObject(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void set(Object value, StaticFieldVarHandle varHandle) {
				_unsafe.putObject(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final Object getVolatile(StaticFieldVarHandle varHandle) {
				return _unsafe.getObjectVolatile(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setVolatile(Object value, StaticFieldVarHandle varHandle) {
				_unsafe.putObjectVolatile(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final Object getOpaque(StaticFieldVarHandle varHandle) {
				return _unsafe.getObjectOpaque(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setOpaque(Object value, StaticFieldVarHandle varHandle) {
				_unsafe.putObjectOpaque(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final Object getAcquire(StaticFieldVarHandle varHandle) {
				return _unsafe.getObjectAcquire(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setRelease(Object value, StaticFieldVarHandle varHandle) {
				_unsafe.putObjectRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(Object testValue, Object newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetObject(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapObject(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final Object compareAndExchange(Object testValue, Object newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeObject(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeObjectVolatile(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final Object compareAndExchangeAcquire(Object testValue, Object newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeObjectAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final Object compareAndExchangeRelease(Object testValue, Object newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeObjectRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(Object testValue, Object newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectPlain(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(Object testValue, Object newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(Object testValue, Object newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(Object testValue, Object newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetObjectPlain(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final Object getAndSet(Object value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetObject(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final Object getAndSetAcquire(Object value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetObjectAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final Object getAndSetRelease(Object value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetObjectRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final Object getAndAdd(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndAddAcquire(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndAddRelease(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseAnd(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseAndAcquire(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseAndRelease(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseOr(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseOrAcquire(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseOrRelease(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseXor(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseXorAcquire(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseXorRelease(Object value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpByte extends StaticFieldVarHandleOperations {
			private static final byte get(StaticFieldVarHandle varHandle) {
				return _unsafe.getByte(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void set(byte value, StaticFieldVarHandle varHandle) {
				_unsafe.putByte(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final byte getVolatile(StaticFieldVarHandle varHandle) {
				return _unsafe.getByteVolatile(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setVolatile(byte value, StaticFieldVarHandle varHandle) {
				_unsafe.putByteVolatile(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final byte getOpaque(StaticFieldVarHandle varHandle) {
				return _unsafe.getByteOpaque(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setOpaque(byte value, StaticFieldVarHandle varHandle) {
				_unsafe.putByteOpaque(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final byte getAcquire(StaticFieldVarHandle varHandle) {
				return _unsafe.getByteAcquire(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setRelease(byte value, StaticFieldVarHandle varHandle) {
				_unsafe.putByteRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(byte testValue, byte newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final byte compareAndExchange(byte testValue, byte newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return (byte)_unsafe.compareAndExchangeInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return (byte)_unsafe.compareAndExchangeIntVolatile(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final byte compareAndExchangeAcquire(byte testValue, byte newValue, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.compareAndExchangeIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final byte compareAndExchangeRelease(byte testValue, byte newValue, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.compareAndExchangeIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(byte testValue, byte newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetIntPlain(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(byte testValue, byte newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(byte testValue, byte newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(byte testValue, byte newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final byte getAndSet(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndSetInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndSetAcquire(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndSetRelease(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndAdd(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndAddInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndAddAcquire(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndAddIntAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final byte getAndAddRelease(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndAddIntRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final byte getAndBitwiseAnd(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndBitwiseAndInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseAndAcquire(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndBitwiseAndIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseAndRelease(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndBitwiseAndIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseOr(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndBitwiseOrInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseOrAcquire(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndBitwiseOrIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseOrRelease(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndBitwiseOrIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseXor(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndBitwiseXorInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseXorAcquire(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndBitwiseXorIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseXorRelease(byte value, StaticFieldVarHandle varHandle) {
				return (byte)_unsafe.getAndBitwiseXorIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
		}
		
		static final class OpChar extends StaticFieldVarHandleOperations {
			private static final char get(StaticFieldVarHandle varHandle) {
				return _unsafe.getChar(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void set(char value, StaticFieldVarHandle varHandle) {
				_unsafe.putChar(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final char getVolatile(StaticFieldVarHandle varHandle) {
				return _unsafe.getCharVolatile(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setVolatile(char value, StaticFieldVarHandle varHandle) {
				_unsafe.putCharVolatile(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final char getOpaque(StaticFieldVarHandle varHandle) {
				return _unsafe.getCharOpaque(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setOpaque(char value, StaticFieldVarHandle varHandle) {
				_unsafe.putCharOpaque(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final char getAcquire(StaticFieldVarHandle varHandle) {
				return _unsafe.getCharAcquire(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setRelease(char value, StaticFieldVarHandle varHandle) {
				_unsafe.putCharRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(char testValue, char newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final char compareAndExchange(char testValue, char newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return (char)_unsafe.compareAndExchangeInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return (char)_unsafe.compareAndExchangeIntVolatile(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final char compareAndExchangeAcquire(char testValue, char newValue, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.compareAndExchangeIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final char compareAndExchangeRelease(char testValue, char newValue, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.compareAndExchangeIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(char testValue, char newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(char testValue, char newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(char testValue, char newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(char testValue, char newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final char getAndSet(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndSetInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndSetAcquire(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndSetRelease(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndAdd(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndAddInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndAddAcquire(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndAddIntAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final char getAndAddRelease(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndAddIntRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final char getAndBitwiseAnd(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndBitwiseAndInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseAndAcquire(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndBitwiseAndIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseAndRelease(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndBitwiseAndIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseOr(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndBitwiseOrInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseOrAcquire(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndBitwiseOrIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseOrRelease(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndBitwiseOrIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseXor(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndBitwiseXorInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseXorAcquire(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndBitwiseXorIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseXorRelease(char value, StaticFieldVarHandle varHandle) {
				return (char)_unsafe.getAndBitwiseXorIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
		}
		
		static final class OpDouble extends StaticFieldVarHandleOperations {
			private static final double get(StaticFieldVarHandle varHandle) {
				return _unsafe.getDouble(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void set(double value, StaticFieldVarHandle varHandle) {
				_unsafe.putDouble(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final double getVolatile(StaticFieldVarHandle varHandle) {
				return _unsafe.getDoubleVolatile(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setVolatile(double value, StaticFieldVarHandle varHandle) {
				_unsafe.putDoubleVolatile(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final double getOpaque(StaticFieldVarHandle varHandle) {
				return _unsafe.getDoubleOpaque(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setOpaque(double value, StaticFieldVarHandle varHandle) {
				_unsafe.putDoubleOpaque(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final double getAcquire(StaticFieldVarHandle varHandle) {
				return _unsafe.getDoubleAcquire(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setRelease(double value, StaticFieldVarHandle varHandle) {
				_unsafe.putDoubleRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(double testValue, double newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final double compareAndExchange(double testValue, double newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeDouble(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeDoubleVolatile(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final double compareAndExchangeAcquire(double testValue, double newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeDoubleAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final double compareAndExchangeRelease(double testValue, double newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeDoubleRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(double testValue, double newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetDoublePlain(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(double testValue, double newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(double testValue, double newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(double testValue, double newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final double getAndSet(double value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetDouble(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final double getAndSetAcquire(double value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetDoubleAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final double getAndSetRelease(double value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetDoubleRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final double getAndAdd(double value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddDouble(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final double getAndAddAcquire(double value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddDoubleAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final double getAndAddRelease(double value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddDoubleRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final double getAndBitwiseAnd(double value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseAndAcquire(double value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseAndRelease(double value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseOr(double value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseOrAcquire(double value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseOrRelease(double value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseXor(double value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseXorAcquire(double value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseXorRelease(double value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpFloat extends StaticFieldVarHandleOperations {
			private static final float get(StaticFieldVarHandle varHandle) {
				return _unsafe.getFloat(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void set(float value, StaticFieldVarHandle varHandle) {
				_unsafe.putFloat(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final float getVolatile(StaticFieldVarHandle varHandle) {
				return _unsafe.getFloatVolatile(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setVolatile(float value, StaticFieldVarHandle varHandle) {
				_unsafe.putFloatVolatile(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final float getOpaque(StaticFieldVarHandle varHandle) {
				return _unsafe.getFloatOpaque(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setOpaque(float value, StaticFieldVarHandle varHandle) {
				_unsafe.putFloatOpaque(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final float getAcquire(StaticFieldVarHandle varHandle) {
				return _unsafe.getFloatAcquire(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setRelease(float value, StaticFieldVarHandle varHandle) {
				_unsafe.putFloatRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(float testValue, float newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapFloat(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final float compareAndExchange(float testValue, float newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeFloat(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeFloatVolatile(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final float compareAndExchangeAcquire(float testValue, float newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeFloatAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final float compareAndExchangeRelease(float testValue, float newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeFloatRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(float testValue, float newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(float testValue, float newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(float testValue, float newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(float testValue, float newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapFloat(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final float getAndSet(float value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetFloat(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final float getAndSetAcquire(float value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetFloatAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final float getAndSetRelease(float value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetFloatRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final float getAndAdd(float value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddFloat(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final float getAndAddAcquire(float value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddFloatAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final float getAndAddRelease(float value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddFloatRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final float getAndBitwiseAnd(float value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseAndAcquire(float value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseAndRelease(float value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseOr(float value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseOrAcquire(float value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseOrRelease(float value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseXor(float value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseXorAcquire(float value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseXorRelease(float value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpInt extends StaticFieldVarHandleOperations {
			private static final int get(StaticFieldVarHandle varHandle) {
				return _unsafe.getInt(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void set(int value, StaticFieldVarHandle varHandle) {
				_unsafe.putInt(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final int getVolatile(StaticFieldVarHandle varHandle) {
				return _unsafe.getIntVolatile(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setVolatile(int value, StaticFieldVarHandle varHandle) {
				_unsafe.putIntVolatile(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final int getOpaque(StaticFieldVarHandle varHandle) {
				return _unsafe.getIntOpaque(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setOpaque(int value, StaticFieldVarHandle varHandle) {
				_unsafe.putIntOpaque(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final int getAcquire(StaticFieldVarHandle varHandle) {
				return _unsafe.getIntAcquire(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setRelease(int value, StaticFieldVarHandle varHandle) {
				_unsafe.putIntRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(int testValue, int newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final int compareAndExchange(int testValue, int newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeIntVolatile(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final int compareAndExchangeAcquire(int testValue, int newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final int compareAndExchangeRelease(int testValue, int newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(int testValue, int newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(int testValue, int newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(int testValue, int newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(int testValue, int newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final int getAndSet(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndSetAcquire(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndSetRelease(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndAdd(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndAddAcquire(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddIntAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final int getAndAddRelease(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddIntRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final int getAndBitwiseAnd(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseAndInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseAndAcquire(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseAndIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseAndRelease(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseAndIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseOr(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseOrInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseOrAcquire(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseOrIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseOrRelease(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseOrIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseXor(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseXorInt(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseXorAcquire(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseXorIntAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseXorRelease(int value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseXorIntRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
		}
		
		static final class OpLong extends StaticFieldVarHandleOperations {
			private static final long get(StaticFieldVarHandle varHandle) {
				return _unsafe.getLong(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void set(long value, StaticFieldVarHandle varHandle) {
				_unsafe.putLong(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final long getVolatile(StaticFieldVarHandle varHandle) {
				return _unsafe.getLongVolatile(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setVolatile(long value, StaticFieldVarHandle varHandle) {
				_unsafe.putLongVolatile(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final long getOpaque(StaticFieldVarHandle varHandle) {
				return _unsafe.getLongOpaque(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setOpaque(long value, StaticFieldVarHandle varHandle) {
				_unsafe.putLongOpaque(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final long getAcquire(StaticFieldVarHandle varHandle) {
				return _unsafe.getLongAcquire(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setRelease(long value, StaticFieldVarHandle varHandle) {
				_unsafe.putLongRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(long testValue, long newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapLong(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final long compareAndExchange(long testValue, long newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.compareAndExchangeLong(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeLongVolatile(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final long compareAndExchangeAcquire(long testValue, long newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeLongAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final long compareAndExchangeRelease(long testValue, long newValue, StaticFieldVarHandle varHandle) {
				return _unsafe.compareAndExchangeLongRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(long testValue, long newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetLongPlain(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(long testValue, long newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(long testValue, long newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(long testValue, long newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(varHandle.definingClass, varHandle.vmslot, testValue, newValue);	
/*[ELSE]
				return _unsafe.compareAndSwapLong(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final long getAndSet(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetLong(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndSetAcquire(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetLongAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndSetRelease(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndSetLongRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndAdd(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddLong(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndAddAcquire(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddLongAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final long getAndAddRelease(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndAddLongRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final long getAndBitwiseAnd(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseAndLong(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseAndAcquire(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseAndLongAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseAndRelease(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseAndLongRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseOr(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseOrLong(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseOrAcquire(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseOrLongAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseOrRelease(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseOrLongRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseXor(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseXorLong(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseXorAcquire(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseXorLongAcquire(varHandle.definingClass, varHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseXorRelease(long value, StaticFieldVarHandle varHandle) {
				return _unsafe.getAndBitwiseXorLongRelease(varHandle.definingClass, varHandle.vmslot, value); 
			}
		}
		
		static final class OpShort extends StaticFieldVarHandleOperations {
			private static final short get(StaticFieldVarHandle varHandle) {
				return _unsafe.getShort(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void set(short value, StaticFieldVarHandle varHandle) {
				_unsafe.putShort(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final short getVolatile(StaticFieldVarHandle varHandle) {
				return _unsafe.getShortVolatile(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setVolatile(short value, StaticFieldVarHandle varHandle) {
				_unsafe.putShortVolatile(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final short getOpaque(StaticFieldVarHandle varHandle) {
				return _unsafe.getShortOpaque(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setOpaque(short value, StaticFieldVarHandle varHandle) {
				_unsafe.putShortOpaque(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final short getAcquire(StaticFieldVarHandle varHandle) {
				return _unsafe.getShortAcquire(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setRelease(short value, StaticFieldVarHandle varHandle) {
				_unsafe.putShortRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(short testValue, short newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final short compareAndExchange(short testValue, short newValue, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.compareAndExchangeIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}
			
			private static final short compareAndExchangeAcquire(short testValue, short newValue, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.compareAndExchangeIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}

			private static final short compareAndExchangeRelease(short testValue, short newValue, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.compareAndExchangeIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(short testValue, short newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(short testValue, short newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(short testValue, short newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(short testValue, short newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short getAndSet(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndSetInt(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndSetAcquire(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndSetRelease(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndAdd(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndAddInt(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndAddAcquire(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndAddIntAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndAddRelease(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndAddIntRelease(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseAnd(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndBitwiseAndInt(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseAndAcquire(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndBitwiseAndIntAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseAndRelease(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndBitwiseAndIntRelease(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseOr(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndBitwiseOrInt(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseOrAcquire(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndBitwiseOrIntAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseOrRelease(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndBitwiseOrIntRelease(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseXor(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndBitwiseXorInt(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseXorAcquire(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndBitwiseXorIntAcquire(varHandle.definingClass, varHandle.vmslot, value);
			}

			private static final short getAndBitwiseXorRelease(short value, StaticFieldVarHandle varHandle) {
				return (short)_unsafe.getAndBitwiseXorIntRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
		}
		
		static final class OpBoolean extends StaticFieldVarHandleOperations {
			private static final boolean get(StaticFieldVarHandle varHandle) {
				return _unsafe.getBoolean(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void set(boolean value, StaticFieldVarHandle varHandle) {
				_unsafe.putBoolean(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean getVolatile(StaticFieldVarHandle varHandle) {
				return _unsafe.getBooleanVolatile(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setVolatile(boolean value, StaticFieldVarHandle varHandle) {
				_unsafe.putBooleanVolatile(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean getOpaque(StaticFieldVarHandle varHandle) {
				return _unsafe.getBooleanOpaque(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setOpaque(boolean value, StaticFieldVarHandle varHandle) {
				_unsafe.putBooleanOpaque(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean getAcquire(StaticFieldVarHandle varHandle) {
				return _unsafe.getBooleanAcquire(varHandle.definingClass, varHandle.vmslot);
			}
			
			private static final void setRelease(boolean value, StaticFieldVarHandle varHandle) {
				_unsafe.putBooleanRelease(varHandle.definingClass, varHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(boolean testValue, boolean newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchange(boolean testValue, boolean newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return (0 != _unsafe.compareAndExchangeInt(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
/*[ELSE]
				return (0 != _unsafe.compareAndExchangeIntVolatile(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchangeAcquire(boolean testValue, boolean newValue, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.compareAndExchangeIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
			}

			private static final boolean compareAndExchangeRelease(boolean testValue, boolean newValue, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.compareAndExchangeIntRelease(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
			}

			private static final boolean weakCompareAndSet(boolean testValue, boolean newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetIntPlain(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(boolean testValue, boolean newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(boolean testValue, boolean newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(boolean testValue, boolean newValue, StaticFieldVarHandle varHandle) {
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.compareAndSwapInt(varHandle.definingClass, varHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean getAndSet(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndSetInt(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndSetAcquire(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndSetIntAcquire(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndSetRelease(boolean value, StaticFieldVarHandle varHandle) {	
				return (0 != _unsafe.getAndSetIntRelease(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndAdd(boolean value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddAcquire(boolean value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddRelease(boolean value, StaticFieldVarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndBitwiseAnd(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndBitwiseAndInt(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseAndAcquire(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndBitwiseAndIntAcquire(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseAndRelease(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndBitwiseAndIntRelease(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOr(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndBitwiseOrInt(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOrAcquire(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndBitwiseOrIntAcquire(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOrRelease(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndBitwiseOrIntRelease(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXor(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndBitwiseXorInt(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXorAcquire(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndBitwiseXorIntAcquire(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXorRelease(boolean value, StaticFieldVarHandle varHandle) {
				return (0 != _unsafe.getAndBitwiseXorIntRelease(varHandle.definingClass, varHandle.vmslot, value ? 1 : 0));
			}
		}
	}
}
