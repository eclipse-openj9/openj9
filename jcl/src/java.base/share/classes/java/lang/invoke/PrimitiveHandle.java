/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2010, 2017 IBM Corp. and others
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

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import com.ibm.oti.vm.VM;
import com.ibm.jit.JITHelpers;

/**
 * PrimitiveHandle is a subclass of MethodHandle used for grouping MethodHandles that directly refer a Java-level method. 
 * 
 */
@VMCONSTANTPOOL_CLASS
abstract class PrimitiveHandle extends MethodHandle {

	@VMCONSTANTPOOL_FIELD 
	long vmSlot; /* Either the address of the method to be invoked or the {i,v}table index */

	@VMCONSTANTPOOL_FIELD 
	int rawModifiers; /* Field/Method modifiers.  Currently only used by fields to determine if volatile */

	@VMCONSTANTPOOL_FIELD 
	Class<?> defc; /* Used by security check. Class containing the method implementation or field. */

	/* Used by staticFieldGetterDispatchTargets as the class passed to the readStatic object access barrier. */
	@VMCONSTANTPOOL_FIELD 
	final Class<?> referenceClass; /* Lookup class for the method or field. */

	final String name; /* Name used to look up method */

	Class<?> specialCaller; /* Class used as part of lookup for KIND_SPECIAL*/

	PrimitiveHandle(MethodType type, Class<?> referenceClass, String name, byte kind, int modifiers, Object thunkArg) {
		super(type, kind, thunkArg);
		this.referenceClass = referenceClass;
		this.defc = referenceClass;
		this.name = name;
		this.rawModifiers = modifiers;
	}

	PrimitiveHandle(MethodType type, Class<?> referenceClass, String name, byte kind, Object thunkArg) {
		this(type, referenceClass, name, kind, 0, thunkArg);
	}

	PrimitiveHandle(PrimitiveHandle original, MethodType newType) {
		super(original, newType);
		this.referenceClass = original.referenceClass;
		this.defc = original.defc;
		this.name = original.name;
		this.rawModifiers = original.rawModifiers;
		this.specialCaller = original.specialCaller;
		this.vmSlot = original.vmSlot;
	}

	@Override
	Class<?> getDefc() {
		return defc;
	}
	
	@Override
	Class<?> getReferenceClass() {
		return referenceClass;
	}
	
	@Override
	Class<?> getSpecialCaller() {
		return specialCaller;
	}
	
	@Override
	String getMethodName() {
		return name;
	}
	
	@Override
	int getModifiers() {
		return rawModifiers;
	}
	
	/*
	 * Wrapper on VM method lookup logic.
	 * 
	 * 	referenceClazz - class that lookup is against
	 *  name - method name
	 *  signature - method signature
	 *  specialCaller - class that invokespecial is against or null
	 *  
	 *  Sets the vmSlot to hold the correct value for the kind of PrimitiveHandle:
	 *  	KIND_STATIC			-	J9Method address
	 *  	KIND_SPECIAL		-	J9Method address
	 *  	KIND_VIRTUAL		-	Vtable index
	 *  	KIND_INTERFACE		-	Itable index
	 *  	KIND_CONSTRUCTOR	-	J9Method address
	 */
	private native Class<?> lookupMethod(Class<?> referenceClazz, String name, String signature, byte kind, Class<?> specialCaller);

	/*
	 * Wrapper on VM field lookup logic.
	 * 
	 *  referenceClazz - class that lookup is against
	 *  name - field name
	 *  signature - field signature
	 *  accessClass is the MethodHandles.Lookup().lookupClass().
	 *  
	 *  Sets the vmSlot to hold the correct value for the kind of PrimitiveHandle:
	 *  	KIND_GETFEILD		-	field offset
	 *  	KIND_GETSTATICFIELD	-	field address
	 *  	KIND_PUTFIELD		-	field offset
	 *  	KIND_PUTSTATICFIELD	-	field address
	 */
	final native Class<?> lookupField(Class<?> referenceClazz, String name, String signature, boolean isStatic, Class<?> accessClass);
	
	/*
	 * Rip apart a Field object that points to a field and fill in the
	 * vmSlot of the PH.  The vmSlot for an instance field is the 
	 * J9JNIFieldID->offset.  For a static field, its the 
	 * J9JNIField->offset + declaring classes ramStatics start address.
	 */
	static native boolean setVMSlotAndRawModifiersFromField(PrimitiveHandle handle, Field field);
	
	/*
	 * Rip apart a Method object and fill in the vmSlot of the PH.  
	 * The vmSlot for a method depends on the kind of method:
	 *  - J9Method address for static and special
	 *  - vtable index for virtual
	 *  - itable index for interface.
	 */
	static native boolean setVMSlotAndRawModifiersFromMethod(PrimitiveHandle handle, Class<?> declaringClass, Method method, byte kind, Class<?> specialToken);
	
	/*
	 * Rip apart a Constructor object and fill in the vmSlot of the PH.  
	 * The vmSlot for an <init> method is the address of the J9Method.
	 */
	static native boolean setVMSlotAndRawModifiersFromConstructor(PrimitiveHandle handle, Constructor ctor);
	
	/*
	 * Set the VMSlot for the VirtualHandle from the DirectHandle.  DirectHandle must be of KIND_SPECIAL.
	 * Necessary to deal with MethodHandles#findVirtual() being allowed to look up private methods.
	 * Does not do access checking as the access check should already have occurred when creating the
	 * DirectHandle.
	 */
	static native boolean setVMSlotAndRawModifiersFromSpecialHandle(VirtualHandle handle, DirectHandle specialHandle);

	final void initializeClassIfRequired() {
		if (Modifier.isStatic(this.rawModifiers)) {
			if (JITHELPERS.getClassInitializeStatus(defc) != VM.J9CLASS_INIT_SUCCEEDED) {
				UNSAFE.ensureClassInitialized(defc);
			}
		}
	}
	
	@Override
	public MethodHandle asVarargsCollector(Class<?> arrayParameter) throws IllegalArgumentException {
		if (!arrayParameter.isArray()) {
			throw new IllegalArgumentException();
		}
		Class<?> lastArgType = type().lastParameterType();
		if (!lastArgType.isAssignableFrom(arrayParameter)) {
			throw new IllegalArgumentException();
		}
		return new VarargsCollectorHandle(this, arrayParameter, MethodHandles.Lookup.isVarargs(rawModifiers));
	}
	
	/*
	 * Finish initialization of the receiver and return the defining class of the method it represents.
	 */
	final Class<?> finishMethodInitialization(Class<?> specialToken, MethodType oldType) throws NoSuchMethodException, IllegalAccessException {
		try {
			String signature = oldType.toMethodDescriptorString();
			return lookupMethod(referenceClass, name, signature, kind, specialToken);
		} catch (NoSuchMethodError e) {
			throw new NoSuchMethodException(e.getMessage());
		} catch (IncompatibleClassChangeError e) {
			throw new IllegalAccessException(e.getMessage());
		}
	}
}
