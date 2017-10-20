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
import static java.lang.invoke.StaticFieldVarHandle.StaticFieldVarHandleOperations.*;

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
		} else if (void.class == type) {
			throw new NoSuchFieldError();
		} else {
			/*[MSG "K0626", "Unable to handle type {0}."]*/
			throw new InternalError(com.ibm.oti.util.Msg.getString("K0626", type)); //$NON-NLS-1$
		}

		MethodType getter = methodType(type, VarHandle.class);
		MethodType setter = methodType(void.class, type, VarHandle.class);
		MethodType compareAndSet = methodType(boolean.class, type, type, VarHandle.class);
		MethodType compareAndExchange = compareAndSet.changeReturnType(type);
		MethodType getAndSet = setter.changeReturnType(type);
		
		MethodType[] lookupTypes = populateMTs(getter, setter, compareAndSet, compareAndExchange, getAndSet);
		MethodType[] exactTypes = lookupTypes;
		
		if (initType != type) {
			MethodType exactGetter = methodType(initType, VarHandle.class);
			MethodType exactSetter = methodType(void.class, initType, VarHandle.class);
			MethodType exactCompareAndSet = methodType(boolean.class, initType, initType, VarHandle.class);
			MethodType exactCompareAndExchange = exactCompareAndSet.changeReturnType(initType);
			MethodType exactGetAndSet = exactSetter.changeReturnType(initType);
			
			exactTypes = populateMTs(exactGetter, exactSetter, exactCompareAndSet, exactCompareAndExchange, exactGetAndSet);
		}
				
		return populateMHs(operationsClass, lookupTypes, exactTypes);
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
		super(lookupClass, fieldName, fieldType, accessClass, true, COORDINATE_TYPES, populateMHs(fieldType));
	}
	
	/**
	 * Construct a VarHandle to the static field represented by the provided {@link java.lang.reflect.Field Field}.
	 * 
	 * @param field The {@link java.lang.reflect.Field} to create a VarHandle for.
	 */
	StaticFieldVarHandle(Field field, Class<?> fieldType) {
		super(field, true, COORDINATE_TYPES, populateMHs(fieldType));
	}

	/**
	 * Type specific methods used by VarHandle methods for static field.
	 */
	@SuppressWarnings("unused")
	static class StaticFieldVarHandleOperations extends VarHandleOperations {
		static final class OpObject extends StaticFieldVarHandleOperations {
			private static final Object get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getObject(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putObject(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final Object getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getObjectVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putObjectVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final Object getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getObjectOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putObjectOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final Object getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getObjectAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putObjectRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetObject(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapObject(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final Object compareAndExchange(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeObject(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeObjectVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final Object compareAndExchangeAcquire(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeObjectAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final Object compareAndExchangeRelease(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeObjectRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectPlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetObjectPlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final Object getAndSet(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetObject(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final Object getAndSetAcquire(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetObjectAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final Object getAndSetRelease(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetObjectRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final Object getAndAdd(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndAddAcquire(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndAddRelease(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseAnd(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseAndAcquire(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseAndRelease(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseOr(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseOrAcquire(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseOrRelease(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseXor(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseXorAcquire(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseXorRelease(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpByte extends StaticFieldVarHandleOperations {
			private static final byte get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getByte(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putByte(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getByteVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putByteVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getByteOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putByteOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getByteAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putByteRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final byte compareAndExchange(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return (byte)_unsafe.compareAndExchangeInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return (byte)_unsafe.compareAndExchangeIntVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final byte compareAndExchangeAcquire(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.compareAndExchangeIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final byte compareAndExchangeRelease(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.compareAndExchangeIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetIntPlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final byte getAndSet(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndSetAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndSetRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndAdd(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndAddInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndAddAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndAddIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getAndAddRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndAddIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getAndBitwiseAnd(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndBitwiseAndInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseAndAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndBitwiseAndIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseAndRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndBitwiseAndIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseOr(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndBitwiseOrInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseOrAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndBitwiseOrIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseOrRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndBitwiseOrIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseXor(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndBitwiseXorInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseXorAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndBitwiseXorIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseXorRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (byte)_unsafe.getAndBitwiseXorIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
		}
		
		static final class OpChar extends StaticFieldVarHandleOperations {
			private static final char get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getChar(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putChar(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getCharVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putCharVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getCharOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putCharOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getCharAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putCharRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final char compareAndExchange(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return (char)_unsafe.compareAndExchangeInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return (char)_unsafe.compareAndExchangeIntVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final char compareAndExchangeAcquire(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.compareAndExchangeIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final char compareAndExchangeRelease(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.compareAndExchangeIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final char getAndSet(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndSetAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndSetRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndAdd(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndAddInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndAddAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndAddIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getAndAddRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndAddIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getAndBitwiseAnd(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndBitwiseAndInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseAndAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndBitwiseAndIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseAndRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndBitwiseAndIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseOr(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndBitwiseOrInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseOrAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndBitwiseOrIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseOrRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndBitwiseOrIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseXor(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndBitwiseXorInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseXorAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndBitwiseXorIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseXorRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (char)_unsafe.getAndBitwiseXorIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
		}
		
		static final class OpDouble extends StaticFieldVarHandleOperations {
			private static final double get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getDoubleVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putDoubleVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getDoubleOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putDoubleOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getDoubleAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putDoubleRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final double compareAndExchange(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeDoubleVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final double compareAndExchangeAcquire(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeDoubleAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final double compareAndExchangeRelease(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeDoubleRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetDoublePlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final double getAndSet(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final double getAndSetAcquire(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetDoubleAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final double getAndSetRelease(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetDoubleRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final double getAndAdd(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddDouble(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final double getAndAddAcquire(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddDoubleAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getAndAddRelease(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddDoubleRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getAndBitwiseAnd(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseAndAcquire(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseAndRelease(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseOr(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseOrAcquire(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseOrRelease(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseXor(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseXorAcquire(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseXorRelease(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpFloat extends StaticFieldVarHandleOperations {
			private static final float get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getFloatVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putFloatVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getFloatOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putFloatOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getFloatAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putFloatRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final float compareAndExchange(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeFloatVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final float compareAndExchangeAcquire(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeFloatAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final float compareAndExchangeRelease(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeFloatRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final float getAndSet(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final float getAndSetAcquire(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetFloatAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final float getAndSetRelease(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetFloatRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final float getAndAdd(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddFloat(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final float getAndAddAcquire(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddFloatAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getAndAddRelease(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddFloatRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getAndBitwiseAnd(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseAndAcquire(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseAndRelease(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseOr(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseOrAcquire(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseOrRelease(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseXor(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseXorAcquire(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseXorRelease(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpInt extends StaticFieldVarHandleOperations {
			private static final int get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getIntVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putIntVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getIntOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putIntOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final int compareAndExchange(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeIntVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final int compareAndExchangeAcquire(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final int compareAndExchangeRelease(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final int getAndSet(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndSetAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndSetRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndAdd(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndAddAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getAndAddRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getAndBitwiseAnd(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseAndInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseAndAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseAndIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseAndRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseAndIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseOr(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseOrInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseOrAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseOrIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseOrRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseOrIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseXor(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseXorInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseXorAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseXorIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseXorRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseXorIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
		}
		
		static final class OpLong extends StaticFieldVarHandleOperations {
			private static final long get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getLongVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putLongVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getLongOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putLongOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getLongAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putLongRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final long compareAndExchange(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.compareAndExchangeLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeLongVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final long compareAndExchangeAcquire(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeLongAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final long compareAndExchangeRelease(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.compareAndExchangeLongRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetLongPlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);	
/*[ELSE]
				return _unsafe.compareAndSwapLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final long getAndSet(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndSetAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetLongAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndSetRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndSetLongRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndAdd(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndAddAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddLongAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getAndAddRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndAddLongRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getAndBitwiseAnd(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseAndLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseAndAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseAndLongAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseAndRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseAndLongRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseOr(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseOrLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseOrAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseOrLongAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseOrRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseOrLongRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseXor(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseXorLong(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseXorAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseXorLongAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseXorRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getAndBitwiseXorLongRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value); 
			}
		}
		
		static final class OpShort extends StaticFieldVarHandleOperations {
			private static final short get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getShort(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putShort(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final short getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getShortVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putShortVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final short getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getShortOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putShortOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final short getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getShortAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putShortRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final short compareAndExchange(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.compareAndExchangeIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final short compareAndExchangeAcquire(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.compareAndExchangeIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}

			private static final short compareAndExchangeRelease(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.compareAndExchangeIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short getAndSet(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndSetAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndSetRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndAdd(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndAddInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndAddAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndAddIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndAddRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndAddIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseAnd(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndBitwiseAndInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseAndAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndBitwiseAndIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseAndRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndBitwiseAndIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseOr(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndBitwiseOrInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseOrAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndBitwiseOrIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseOrRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndBitwiseOrIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseXor(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndBitwiseXorInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseXorAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndBitwiseXorIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseXorRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (short)_unsafe.getAndBitwiseXorIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
		}
		
		static final class OpBoolean extends StaticFieldVarHandleOperations {
			private static final boolean get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getBoolean(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putBoolean(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getBooleanVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putBooleanVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getBooleanOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putBooleanOpaque(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return _unsafe.getBooleanAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				_unsafe.putBooleanRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchange(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return (0 != _unsafe.compareAndExchangeInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
/*[ELSE]
				return (0 != _unsafe.compareAndExchangeIntVolatile(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchangeAcquire(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.compareAndExchangeIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
			}

			private static final boolean compareAndExchangeRelease(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.compareAndExchangeIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
			}

			private static final boolean weakCompareAndSet(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetIntPlain(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.compareAndSwapInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean getAndSet(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndSetInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndSetAcquire(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndSetIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndSetRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndSetIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndAdd(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddAcquire(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndBitwiseAnd(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndBitwiseAndInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseAndAcquire(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndBitwiseAndIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseAndRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndBitwiseAndIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOr(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndBitwiseOrInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOrAcquire(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndBitwiseOrIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOrRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndBitwiseOrIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXor(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndBitwiseXorInt(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXorAcquire(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndBitwiseXorIntAcquire(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXorRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				return (0 != _unsafe.getAndBitwiseXorIntRelease(fieldVarHandle.definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}
		}
	}
}
