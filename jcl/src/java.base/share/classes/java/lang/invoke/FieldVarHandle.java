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

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import static java.lang.invoke.MethodType.*;
import java.lang.invoke.MethodHandles.Lookup;

import com.ibm.oti.vm.VM;

abstract class FieldVarHandle extends VarHandle {
	final long vmslot;
	final Class<?> definingClass;
	final String fieldName;

	/**
	 * Constructs a VarHandle referencing a field.
	 * 
	 * @param lookupClass The class where we start the lookup of the field
	 * @param fieldName The field name
	 * @param fieldType The exact type of the field
	 * @param accessClass The class being used to look up the field
	 * @param isStatic A boolean value indicating whether the field is static
	 * @param coordinateTypes An ordered array of the parameter classes required for invoking an AccessMode on the VarHandle.
	 * @param handleTable The array of MethodHandles implementing the AccessModes.
	 * @param typeTable The VarHandle instance specific MethodTypes used to validate invocations.
	 */
	FieldVarHandle(Class<?> lookupClass, String fieldName, Class<?> fieldType, Class<?> accessClass, boolean isStatic, Class<?>[] coordinateTypes, MethodHandle[] handleTable) {
		super(fieldType, coordinateTypes, handleTable, 0);
		this.definingClass = lookupClass;
		this.fieldName = fieldName;
		int header = (isStatic ? 0 : VM.OBJECT_HEADER_SIZE);
		this.vmslot = lookupField(definingClass, fieldName, MethodType.getBytecodeStringName(fieldType), fieldType, isStatic, accessClass) + header;
		checkSetterFieldFinality(handleTable);
	}
	
	/**
	 * Constructs a VarHandle referencing the field represented by the {@link java.lang.reflect.Field}.
	 * 
	 * @param field The {@link java.lang.reflect.Field} to create a VarHandle for.
	 * @param isStatic A boolean value indicating whether the field is static
	 * @param coordinateTypes An ordered array of the parameter classes required for invoking an AccessMode on the VarHandle.
	 * @param handleTable The array of MethodHandles implementing the AccessModes.
	 * @param typeTable The VarHandle instance specific MethodTypes used to validate invocations.
	 */
	FieldVarHandle(Field field, boolean isStatic, Class<?>[] coordinateTypes, MethodHandle[] handleTable) {
		super(field.getType(), coordinateTypes, handleTable, 0);
		this.definingClass = field.getDeclaringClass();
		this.fieldName = field.getName();
		int header = (isStatic ? 0 : VM.OBJECT_HEADER_SIZE);
		this.vmslot = unreflectField(field, isStatic) + header;
		checkSetterFieldFinality(handleTable);
	}
	
	/**
	 * Checks whether the field referenced by this VarHandle, is final. 
	 * If so, MethodHandles in the handleTable that represent access modes that may modify the field, 
	 * are replaced by a MethodHandle that throws an exception when invoked.
	 * 
	 * @param handleTable The array of MethodHandles implementing the AccessModes.
	 */
	void checkSetterFieldFinality(MethodHandle[] handleTable) {
		if (Modifier.isFinal(modifiers)) {
			MethodHandle exceptionThrower;
			try {
				exceptionThrower = MethodHandles.Lookup.internalPrivilegedLookup.findStatic(FieldVarHandle.class, "finalityCheckFailedExceptionThrower", methodType(void.class));
			} catch (IllegalAccessException | NoSuchMethodException e) {
				throw new InternalError(e);
			}
			for (AccessMode mode : AccessMode.values()) {
				if (mode.isSetter) {
					MethodHandle mh = handleTable[mode.ordinal()];
					Class<?>[] args = mh.type.arguments;
					handleTable[mode.ordinal()] = MethodHandles.dropArguments(exceptionThrower, 0, args);
				}
			}
		}
	}
	
	/**
	 * checkSetterFieldFinality replaces MethodHandles in the handleTable with this method
	 * to throw an exception when the field referenced by the VarHandle is final. 
	 */
	private static void finalityCheckFailedExceptionThrower() {
		/*[MSG "K0629", "Modification access modes are not allowed on final fields."]*/
		throw new UnsupportedOperationException(com.ibm.oti.util.Msg.getString("K0629")); //$NON-NLS-1$
	}
	
	/**
	 * Looks up a field in a class (lookupClass) given a name and a signature.
	 * 
	 * @param lookupClass The class where we start the lookup of the field
	 * @param name The field name
	 * @param signature Equivalent of the String returned by MethodType.getBytecodeStringName
	 * @param type The exact type of the field. This must match the signature.
	 * @param isStatic A boolean value indicating whether the field is static.
	 * @param accessClass The class being used to look up the field.
	 * @return This is the value that should be assigned to the vmslot field. 
	 * 	It represents the offset to access the field. In the error cases, 
	 * 	this method returns -1 for instance fields, and 0 for static fields.
	 * 
	 * This method may throw any of the following exceptions or others related to field resolution.
	 * @throws IllegalAccessError
	 * @throws IncompatibleClassChangeError
	 * @throws NoSuchFieldError
	 * @throws LinkageError
	 * @throws OutOfMemoryError
	 */
	native long lookupField(Class<?> lookupClass, String name, String signature, Class<?> type, boolean isStatic, Class<?> accessClass);
	
	/**
	 * Gets the offset for a field given a {@link java.lang.reflect.Field}. This method also sets the modifiers.
	 * 
	 * @param field The {@link java.lang.reflect.Field} to get the offset for.
	 * @param isStatic A boolean value indicating whether the field is static.
	 * @return
	 */
	native long unreflectField(Field field, boolean isStatic);
	
	@Override
	final Class<?> getDefiningClass() {
		return definingClass;
	}
	
	@Override
	final String getFieldName() {
		return fieldName;
	}
}
