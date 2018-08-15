/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

/*[IF ]*/
/*
 * Note, Static Field getter/setter handles do not ensure that the class has been initialized
 * prior to returning the handle.  This matches the RI's behaviour.
 */
/*[ENDIF]*/
abstract class FieldHandle extends PrimitiveHandle {
	final Class<?> fieldClass;
	final int final_modifiers;
	
	FieldHandle(MethodType type, Class<?> referenceClass, String fieldName, Class<?> fieldClass, byte kind, Class<?> accessClass) throws IllegalAccessException, NoSuchFieldException { 	
		super(type, referenceClass, fieldName, kind, null);
		this.fieldClass = fieldClass;
		/* modifiers is set inside the native */
		this.defc = finishFieldInitialization(accessClass);
		final_modifiers = rawModifiers;
		assert(isVMSlotCorrectlyTagged());
	}
	
	FieldHandle(MethodType type, Field field, byte kind, boolean isStatic) throws IllegalAccessException {
		super(type, field.getDeclaringClass(), field.getName(), kind, field.getModifiers(), null);
		this.fieldClass = field.getType();
		assert(isStatic == Modifier.isStatic(field.getModifiers()));
		
		boolean succeed = setVMSlotAndRawModifiersFromField(this, field);
		if (!succeed) {
			throw new IllegalAccessException();
		}
		final_modifiers = rawModifiers;
		assert(isVMSlotCorrectlyTagged());
	}
	
	FieldHandle(FieldHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.fieldClass = originalHandle.fieldClass;
		final_modifiers = rawModifiers;
		assert(isVMSlotCorrectlyTagged());
	}

	final Class<?> finishFieldInitialization(Class<?> accessClass) throws IllegalAccessException, NoSuchFieldException {
		String signature = MethodType.getBytecodeStringName(fieldClass);
		try {
			boolean isStaticLookup = ((KIND_GETSTATICFIELD == this.kind) || (KIND_PUTSTATICFIELD == this.kind)); 
			return lookupField(referenceClass, name, signature, isStaticLookup, accessClass);
		} catch (NoSuchFieldError e) {
			throw new NoSuchFieldException(e.getMessage());
		} catch (LinkageError e) {
			throw (IllegalAccessException) new IllegalAccessException(e.getMessage()).initCause(e);
		} 
	}

	/* Ensure the vmSlot is low tagged if static */
	boolean isVMSlotCorrectlyTagged() {
		if ((KIND_PUTSTATICFIELD == this.kind) || (KIND_GETSTATICFIELD == this.kind)) {
			return (vmSlot & 1) == 1;
		}
		return (vmSlot & 1) == 0;  
	}
	
	@Override
	boolean canRevealDirect() {
		return true;
	}
		
	final void compareWithField(FieldHandle left, Comparator c) {
		c.compareStructuralParameter(left.referenceClass, this.referenceClass);
		c.compareStructuralParameter(left.vmSlot, this.vmSlot);
	}

	/*[IF Java11]*/
	boolean isFinal() {
		return Modifier.isFinal(this.rawModifiers);
	}
	/*[ENDIF] Java11 */
}

