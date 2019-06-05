/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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

/**
 * A reference to a cracked MethodHandle, which allows access to its symbolic parts.
 * Call Lookup.revealDirect to crack a MethodHandle.
 * 
 * @since 1.8
 */
public interface MethodHandleInfo {

	/**
	 * Getter MethodHandle for an instance field
	 */
	static final int REF_getField = 1;
	
	/**
	 * Getter MethodHandle for an static field
	 */
	static final int REF_getStatic = 2;
	
	/**
	 * Setter MethodHandle for an instance field
	 */
	static final int REF_putField = 3;
	
	/**
	 * Setter MethodHandle for an static field
	 */
	static final int REF_putStatic = 4;
	
	/**
	 * MethodHandle for an instance method
	 */
	static final int REF_invokeVirtual = 5;
	
	/**
	 * MethodHandle for a static method
	 */
	static final int REF_invokeStatic = 6;
	
	/**
	 * MethodHandle for an special method
	 */
	static final int REF_invokeSpecial = 7;
	
	/**
	 * MethodHandle for a constructor
	 */
	static final int REF_newInvokeSpecial = 8;
	
	/**
	 * MethodHandle for an interface method
	 */
	static final int REF_invokeInterface = 9;
	
	/** 
	 * Returns the Class where the cracked MethodHandle's underlying method, field or constructor is declared.
	 * 
	 * @return class that declares the underlying member
	 */
	Class<?> getDeclaringClass();
	
	/**
	 * Returns the simple name of the MethodHandle's underlying member.
	 * 
	 * @return A string representing the name of the method or field, or "&lt;init&gt;" for constructor. 
	 */
	String getName();
	
	/**
	 * Returns the type of the MethodHandle's underlying member as a MethodType.
	 * If the underlying member is non-static, the receiver parameter will not be included.
	 * If the underlying member is field getter, the MethodType will take no parameters, and the return type will be the field type.
	 * If the underlying member is field setter, the MethodType will take one parameter of the field type, and the return type will be void.
	 * 
	 * @return A MethodType object representing the signature of the method or field
	 */
	MethodType getMethodType();
	
	/**
	 * Returns the modifiers of the MethodHandle's underlying member.
	 * 
	 * @return An int representing the member's modifiers, or -1 if the underlying member is not accessible.
	 */
	int getModifiers();
	
	/**
	 * Returns the reference kind of the MethodHandle. The possible reference kinds are the declared MethodHandleInfo.REF fields.
	 * 
	 * @return Returns one of the defined reference kinds which represent the MethodHandle kind.
	 */
	int getReferenceKind();
	
	/**
	 * Returns whether the MethodHandle's underlying method or constructor has variable argument arity.
	 * 
	 * @return whether the underlying method has variable arity
	 */
	default boolean isVarArgs() {
		// Check whether the MethodHandle refers to a Method or Constructor (not Field)
		if (getReferenceKind() >= REF_invokeVirtual) {
			return ((getModifiers() & MethodHandles.Lookup.VARARGS) != 0);
		}
		return false;
	}
	
	/**
	 * Reflects the underlying member as a Method, Field or Constructor. The member must be accessible to the provided lookup object.
	 * Public members are reflected as if by <code>getMethod</code>, <code>getField</code> or <code>getConstructor</code>. 
	 * Non-public members are reflected as if by <code>getDeclaredMethod</code>, <code>getDeclaredField</code> or <code>getDeclaredConstructor</code>.
	 * 
	 * @param <T> The expected type of the returned Member
	 * @param expected The expected Class of the returned Member
	 * @param lookup The lookup that was used to create the MethodHandle, or a lookup object with equivalent access
	 * @return A Method, Field or Constructor representing the underlying member of the MethodHandle
	 * @throws NullPointerException If either argument is null
	 * @throws IllegalArgumentException If the underlying member is not accessible to the provided lookup object
	 * @throws ClassCastException If the underlying member is not of the expected type 
	 */
	<T extends java.lang.reflect.Member> T reflectAs(Class<T> expected, MethodHandles.Lookup lookup);
	
	/**
	 * Returns a string representing the equivalent bytecode for the referenceKind.
	 * 
	 * @param referenceKind The referenceKind to lookup
	 * @return a String representing the equivalent bytecode
	 * @throws IllegalArgumentException If the provided referenceKind is invalid 
	 */
	static String referenceKindToString(int referenceKind) throws IllegalArgumentException {
		switch(referenceKind) {
		case REF_getField: 			return "getField"; //$NON-NLS-1$
		case REF_getStatic: 		return "getStatic"; //$NON-NLS-1$
		case REF_putField: 			return "putField"; //$NON-NLS-1$
		case REF_putStatic: 		return "putStatic"; //$NON-NLS-1$
		case REF_invokeVirtual: 	return "invokeVirtual"; //$NON-NLS-1$
		case REF_invokeStatic: 		return "invokeStatic"; //$NON-NLS-1$
		case REF_invokeSpecial: 	return "invokeSpecial"; //$NON-NLS-1$
		case REF_newInvokeSpecial: 	return "newInvokeSpecial"; //$NON-NLS-1$
		case REF_invokeInterface: 	return "invokeInterface"; //$NON-NLS-1$
		}
		/*[MSG "K0582", "Reference kind \"{0\"} is invalid"]*/
		throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0582", referenceKind)); //$NON-NLS-1$
	}
	
	/**
	 * Answers a string containing a concise, human-readable description of the receiver.
	 * 
	 * @param kind the reference kind, one of the declared MethodHandleInfo.REF fields.
	 * @param defc the class where the member is declared
	 * @param name the name of the member
	 * @param type the member's MethodType
	 * @return a String of the format "K C.N:MT"
	 */
	static String toString(int kind, Class<?> defc, String name, MethodType type) {
		return referenceKindToString(kind) + " " + defc.getName() + "." + name + ":" + type; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
	}
		
}

