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

import com.ibm.jit.JITHelpers;
import com.ibm.oti.vm.VM;

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
		private static final JITHelpers jitHelpers = JITHelpers.getHelpers();
		
		static final void ensureClassIsInitialized(Class<?> clazz) {
			if (jitHelpers.getClassInitializeStatus(clazz) != VM.J9CLASS_INIT_SUCCEEDED) {
				_unsafe.ensureClassInitialized(clazz);
			}
		}
		
		static final class OpObject extends StaticFieldVarHandleOperations {
			private static final Object get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getObject(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putObject(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final Object getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getObjectVolatile(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putObjectVolatile(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final Object getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getObjectOpaque(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putObjectOpaque(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final Object getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getObjectAcquire(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putObjectRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetObject(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapObject(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final Object compareAndExchange(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeObject(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeObjectVolatile(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final Object compareAndExchangeAcquire(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeObjectAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final Object compareAndExchangeRelease(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeObjectRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectPlain(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetObjectRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObjectRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(Object testValue, Object newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetObjectPlain(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapObject(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final Object getAndSet(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetObject(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final Object getAndSetAcquire(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetObjectAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final Object getAndSetRelease(Object value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetObjectRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final Object getAndAdd(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndAddAcquire(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndAddRelease(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseAnd(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseAndAcquire(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseAndRelease(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseOr(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseOrAcquire(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseOrRelease(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseXor(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseXorAcquire(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final Object getAndBitwiseXorRelease(Object value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpByte extends StaticFieldVarHandleOperations {
			private static final byte get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getByte(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putByte(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getByteVolatile(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putByteVolatile(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getByteOpaque(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putByteOpaque(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getByteAcquire(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putByteRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final byte compareAndExchange(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return (byte)_unsafe.compareAndExchangeInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return (byte)_unsafe.compareAndExchangeIntVolatile(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final byte compareAndExchangeAcquire(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.compareAndExchangeIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final byte compareAndExchangeRelease(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.compareAndExchangeIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetIntPlain(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(byte testValue, byte newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final byte getAndSet(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndSetInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndSetAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndSetRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndSetIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndAdd(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndAddInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndAddAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndAddIntAcquire(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getAndAddRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndAddIntRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final byte getAndBitwiseAnd(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndBitwiseAndInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseAndAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndBitwiseAndIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseAndRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndBitwiseAndIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseOr(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndBitwiseOrInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseOrAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndBitwiseOrIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseOrRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndBitwiseOrIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseXor(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndBitwiseXorInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseXorAcquire(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndBitwiseXorIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final byte getAndBitwiseXorRelease(byte value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (byte)_unsafe.getAndBitwiseXorIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
		}
		
		static final class OpChar extends StaticFieldVarHandleOperations {
			private static final char get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getChar(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putChar(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getCharVolatile(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putCharVolatile(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getCharOpaque(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putCharOpaque(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getCharAcquire(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putCharRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final char compareAndExchange(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return (char)_unsafe.compareAndExchangeInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return (char)_unsafe.compareAndExchangeIntVolatile(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final char compareAndExchangeAcquire(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.compareAndExchangeIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final char compareAndExchangeRelease(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.compareAndExchangeIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(char testValue, char newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final char getAndSet(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndSetInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndSetAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndSetRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndSetIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndAdd(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndAddInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndAddAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndAddIntAcquire(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getAndAddRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndAddIntRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final char getAndBitwiseAnd(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndBitwiseAndInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseAndAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndBitwiseAndIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseAndRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndBitwiseAndIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseOr(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndBitwiseOrInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseOrAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndBitwiseOrIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseOrRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndBitwiseOrIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseXor(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndBitwiseXorInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseXorAcquire(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndBitwiseXorIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final char getAndBitwiseXorRelease(char value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (char)_unsafe.getAndBitwiseXorIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
		}
		
		static final class OpDouble extends StaticFieldVarHandleOperations {
			private static final double get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getDouble(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putDouble(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getDoubleVolatile(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putDoubleVolatile(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getDoubleOpaque(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putDoubleOpaque(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getDoubleAcquire(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putDoubleRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final double compareAndExchange(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeDouble(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeDoubleVolatile(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final double compareAndExchangeAcquire(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeDoubleAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final double compareAndExchangeRelease(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeDoubleRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetDoublePlain(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDouble(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetDoubleRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapDoubleRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(double testValue, double newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetDouble(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapDouble(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final double getAndSet(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetDouble(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final double getAndSetAcquire(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetDoubleAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final double getAndSetRelease(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetDoubleRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final double getAndAdd(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddDouble(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final double getAndAddAcquire(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddDoubleAcquire(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getAndAddRelease(double value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddDoubleRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final double getAndBitwiseAnd(double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseAndAcquire(double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseAndRelease(double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseOr(double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseOrAcquire(double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseOrRelease(double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseXor(double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseXorAcquire(double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final double getAndBitwiseXorRelease(double value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpFloat extends StaticFieldVarHandleOperations {
			private static final float get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getFloat(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putFloat(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getFloatVolatile(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putFloatVolatile(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getFloatOpaque(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putFloatOpaque(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getFloatAcquire(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putFloatRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapFloat(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final float compareAndExchange(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeFloat(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeFloatVolatile(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final float compareAndExchangeAcquire(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeFloatAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final float compareAndExchangeRelease(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeFloatRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatPlain(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloat(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetFloatRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapFloatRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(float testValue, float newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetFloat(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapFloat(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final float getAndSet(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetFloat(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final float getAndSetAcquire(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetFloatAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final float getAndSetRelease(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetFloatRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final float getAndAdd(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddFloat(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final float getAndAddAcquire(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddFloatAcquire(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getAndAddRelease(float value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddFloatRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final float getAndBitwiseAnd(float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseAndAcquire(float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseAndRelease(float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseOr(float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseOrAcquire(float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseOrRelease(float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseXor(float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseXorAcquire(float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
			
			private static final float getAndBitwiseXorRelease(float value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}
		}
		
		static final class OpInt extends StaticFieldVarHandleOperations {
			private static final int get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getInt(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putInt(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getIntVolatile(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putIntVolatile(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getIntOpaque(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putIntOpaque(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getIntAcquire(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putIntRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final int compareAndExchange(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndExchangeInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeIntVolatile(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final int compareAndExchangeAcquire(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final int compareAndExchangeRelease(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(int testValue, int newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final int getAndSet(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndSetAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndSetRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndAdd(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndAddAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddIntAcquire(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getAndAddRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddIntRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final int getAndBitwiseAnd(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseAndInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseAndAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseAndIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseAndRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseAndIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseOr(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseOrInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseOrAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseOrIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseOrRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseOrIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseXor(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseXorInt(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseXorAcquire(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseXorIntAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final int getAndBitwiseXorRelease(int value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseXorIntRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
		}
		
		static final class OpLong extends StaticFieldVarHandleOperations {
			private static final long get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getLong(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putLong(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getLongVolatile(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putLongVolatile(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getLongOpaque(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putLongOpaque(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getLongAcquire(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putLongRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapLong(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final long compareAndExchange(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.compareAndExchangeLong(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndExchangeLongVolatile(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final long compareAndExchangeAcquire(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeLongAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final long compareAndExchangeRelease(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.compareAndExchangeLongRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final boolean weakCompareAndSet(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetLongPlain(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLong(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetAcquire(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetRelease(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetLongRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapLongRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final boolean weakCompareAndSetPlain(long testValue, long newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetLong(definingClass, fieldVarHandle.vmslot, testValue, newValue);	
/*[ELSE]
				return _unsafe.compareAndSwapLong(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final long getAndSet(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetLong(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndSetAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetLongAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndSetRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndSetLongRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndAdd(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddLong(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndAddAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddLongAcquire(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getAndAddRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndAddLongRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final long getAndBitwiseAnd(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseAndLong(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseAndAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseAndLongAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseAndRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseAndLongRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseOr(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseOrLong(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseOrAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseOrLongAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseOrRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseOrLongRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseXor(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseXorLong(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseXorAcquire(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseXorLongAcquire(definingClass, fieldVarHandle.vmslot, value); 
			}
			
			private static final long getAndBitwiseXorRelease(long value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getAndBitwiseXorLongRelease(definingClass, fieldVarHandle.vmslot, value); 
			}
		}
		
		static final class OpShort extends StaticFieldVarHandleOperations {
			private static final short get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getShort(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putShort(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final short getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getShortVolatile(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putShortVolatile(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final short getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getShortOpaque(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putShortOpaque(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final short getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getShortAcquire(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putShortRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}
			
			private static final short compareAndExchange(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.compareAndExchangeIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}
			
			private static final short compareAndExchangeAcquire(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.compareAndExchangeIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}

			private static final short compareAndExchangeRelease(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.compareAndExchangeIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
			}

			private static final boolean weakCompareAndSet(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntPlain(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(short testValue, short newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue, newValue);
/*[ENDIF]*/				
			}

			private static final short getAndSet(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndSetInt(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndSetAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndSetRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndSetIntRelease(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndAdd(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndAddInt(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndAddAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndAddIntAcquire(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndAddRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndAddIntRelease(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseAnd(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndBitwiseAndInt(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseAndAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndBitwiseAndIntAcquire(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseAndRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndBitwiseAndIntRelease(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseOr(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndBitwiseOrInt(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseOrAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndBitwiseOrIntAcquire(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseOrRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndBitwiseOrIntRelease(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseXor(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndBitwiseXorInt(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseXorAcquire(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndBitwiseXorIntAcquire(definingClass, fieldVarHandle.vmslot, value);
			}

			private static final short getAndBitwiseXorRelease(short value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (short)_unsafe.getAndBitwiseXorIntRelease(definingClass, fieldVarHandle.vmslot, value);
			}
		}
		
		static final class OpBoolean extends StaticFieldVarHandleOperations {
			private static final boolean get(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getBoolean(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void set(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putBoolean(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean getVolatile(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getBooleanVolatile(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setVolatile(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putBooleanVolatile(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean getOpaque(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getBooleanOpaque(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setOpaque(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putBooleanOpaque(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean getAcquire(VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return _unsafe.getBooleanAcquire(definingClass, fieldVarHandle.vmslot);
			}
			
			private static final void setRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				_unsafe.putBooleanRelease(definingClass, fieldVarHandle.vmslot, value);
			}
			
			private static final boolean compareAndSet(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchange(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return (0 != _unsafe.compareAndExchangeInt(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
/*[ELSE]
				return (0 != _unsafe.compareAndExchangeIntVolatile(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
/*[ENDIF]*/				
			}

			private static final boolean compareAndExchangeAcquire(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.compareAndExchangeIntAcquire(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
			}

			private static final boolean compareAndExchangeRelease(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.compareAndExchangeIntRelease(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0));
			}

			private static final boolean weakCompareAndSet(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/
				return _unsafe.weakCompareAndSetIntPlain(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetAcquire(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntAcquire(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetRelease(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.weakCompareAndSetIntRelease(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.weakCompareAndSwapIntRelease(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean weakCompareAndSetPlain(boolean testValue, boolean newValue, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
/*[IF Sidecar19-SE-B174]*/				
				return _unsafe.compareAndSetInt(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ELSE]
				return _unsafe.compareAndSwapInt(definingClass, fieldVarHandle.vmslot, testValue ? 1 : 0, newValue ? 1 : 0);
/*[ENDIF]*/				
			}

			private static final boolean getAndSet(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndSetInt(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndSetAcquire(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndSetIntAcquire(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndSetRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndSetIntRelease(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndAdd(boolean value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddAcquire(boolean value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndAddRelease(boolean value, VarHandle varHandle) {
				throw operationNotSupported(varHandle);
			}

			private static final boolean getAndBitwiseAnd(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndBitwiseAndInt(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseAndAcquire(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndBitwiseAndIntAcquire(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseAndRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndBitwiseAndIntRelease(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOr(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndBitwiseOrInt(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOrAcquire(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndBitwiseOrIntAcquire(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseOrRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndBitwiseOrIntRelease(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXor(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndBitwiseXorInt(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXorAcquire(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndBitwiseXorIntAcquire(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}

			private static final boolean getAndBitwiseXorRelease(boolean value, VarHandle varHandle) {
				FieldVarHandle fieldVarHandle = (FieldVarHandle)varHandle;
				Class<?> definingClass = fieldVarHandle.definingClass;
				ensureClassIsInitialized(definingClass);
				return (0 != _unsafe.getAndBitwiseXorIntRelease(definingClass, fieldVarHandle.vmslot, value ? 1 : 0));
			}
		}
	}
}
