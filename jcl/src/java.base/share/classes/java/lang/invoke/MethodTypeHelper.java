/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

import com.ibm.oti.util.Msg;

/**
 * MethodTypeHelper - static methods
 * 
 * @since Java 11
 */
public final class MethodTypeHelper {
	static final Set<Class<?>> WRAPPER_SET;
	static {
		Class<?>[] wrappers = {Byte.class, Character.class, Double.class, Float.class, Integer.class, Long.class, Short.class, Boolean.class };
		WRAPPER_SET = Collections.unmodifiableSet(new HashSet<Class<?>>(Arrays.asList(wrappers)));
	}

	/*[IF ]*/
	/* Do not include 'primitives.put("V", void.class)' as void.class is not yet loaded when
	 * MethodType gets loaded and this will cause the VM not to start.  See the code
	 * in Class#getPrimitiveClass() for the issue. */
	/*[ENDIF]*/
	static final Class<?>[] primitivesArray = new Class<?>[26];
	static {
		primitivesArray['B' - 'A'] = byte.class;
		primitivesArray['C' - 'A'] = char.class;
		primitivesArray['D' - 'A'] = double.class;
		primitivesArray['F' - 'A'] = float.class;
		primitivesArray['I' - 'A'] = int.class;
		primitivesArray['J' - 'A'] = long.class;
		primitivesArray['S' - 'A'] = short.class;
		primitivesArray['Z' - 'A'] = boolean.class;
	}

	/**
	 * Returns the appropriate wrapper class or the original class
	 * @param primitiveClass The class to convert to a wrapper class
	 * @return The wrapper class or the original class if no wrapper is available
	 */
	static Class<?> wrapPrimitive(Class<?> primitveClass) {
		if (primitveClass.isPrimitive()) {
			if (int.class == primitveClass){ //I
				return Integer.class;
			}
			if (long.class == primitveClass){ //J
				return Long.class;
			}
			if (byte.class == primitveClass){	//B
				return Byte.class;
			}
			if (char.class == primitveClass){ //C
				return Character.class;
			}
			if (double.class == primitveClass){ //D
				return Double.class;
			}
			if (float.class == primitveClass){ //F
				return Float.class;
			}
			if (boolean.class == primitveClass){ //Z
				return Boolean.class;
			}
			if (void.class == primitveClass) {
				return Void.class;
			}
			if (short.class == primitveClass){ //S
				return Short.class;
			}
		}
		return primitveClass;
	}

	/**
	 * Takes a class and returns the corresponding primitive if one exists.
	 * @param wrapperClass The class to check for primitive classes.
	 * @return The corresponding primitive class or the input class.
	 */
	/*[IF ]*/
	/* Note that Void.class is not handled by this method as it is only viewed as a
	 * wrapper when it is the return class.
	 */
	/*[ENDIF]*/
	static Class<?> unwrapPrimitive(Class<?> wrapperClass){
		if (Integer.class == wrapperClass) { //I
			return int.class;
		}
		if (Long.class == wrapperClass) { //J
			return long.class;
		}
		if (Byte.class == wrapperClass) {	//B
			return byte.class;
		}
		if (Character.class == wrapperClass) { //C
			return char.class;
		}
		if (Double.class == wrapperClass) { //D
			return double.class;
		}
		if (Float.class == wrapperClass) { //F
			return float.class;
		}
		if (Short.class == wrapperClass) { //S
			return short.class;
		}
		if (Boolean.class == wrapperClass) { //Z
			return boolean.class;
		}
		return wrapperClass;
	}

	static String getBytecodeStringName(Class<?> c){
		if (c.isPrimitive()) {
			if (c == int.class) {
				return "I"; //$NON-NLS-1$
			} else if (c == long.class) {
				return "J"; //$NON-NLS-1$
			} else if (c == byte.class) {
				return "B"; //$NON-NLS-1$
			} else if (c == boolean.class) {
				return "Z"; //$NON-NLS-1$
			} else if (c == void.class) {
				return "V"; //$NON-NLS-1$
			} else if (c == char.class) {
				return "C"; //$NON-NLS-1$
			} else if (c == double.class) {
				return "D"; //$NON-NLS-1$
			} else if (c == float.class) {
				return "F"; //$NON-NLS-1$
			} else if (c == short.class) {
				return "S"; //$NON-NLS-1$
			}
		}
		String name = c.getName().replace('.', '/');
		if (c.isArray()) {
			return name;
		}
		return "L"+ name + ";"; //$NON-NLS-1$ //$NON-NLS-2$
	}

	/*
	 * Convert the string from bytecode format to the format needed for ClassLoader#loadClass().
	 * Change all '/' to '.'.
	 * Remove the 'L' & ';' from objects, unless they are array classes.
	 */
	private static final Class<?> nonPrimitiveClassFromString(String name, ClassLoader classLoader) {
		try {
			name = name.replace('/', '.');
			if (name.indexOf('L') == 0) {
				// Remove the 'L' and ';'
				name = name.substring(1, name.length() - 1);
			}
			return Class.forName(name, false, classLoader);
		} catch(ClassNotFoundException e){
			throw new TypeNotPresentException(name, e);
		}
	}

	/*
	 * Parses the first parameter in a descriptor string into a Class object.
	 * The Class object is append to given ArrayList and the current string index is returned.
	 */
	static final int parseIntoClass(char[] signature, int index, ArrayList<Class<?>> args, ClassLoader classLoader, String descriptor) {
		char current = signature[index];
		Class<?> c;			
		
		if ((current == 'L') || (current == '[')) {
			int start = index;
			while(signature[index] == '[') {
				index++;
			}
			String name;
			if (signature[index] != 'L') {
				name = descriptor.substring(start, index + 1);
			} else { 
				int end = descriptor.indexOf(';', index);
				if (end == -1) {
					/*[MSG "K05d6", "malformed method descriptor: {0}"]*/
					throw new IllegalArgumentException(Msg.getString("K05d6", descriptor)); //$NON-NLS-1$
				}
				name = descriptor.substring(start, end + 1);
				index = end;
			}
			c = nonPrimitiveClassFromString(name, classLoader);
		} else {
			try {
				c = primitivesArray[current - 'A'];
			} catch(ArrayIndexOutOfBoundsException e) {
				c = null;
			}
			if (c == null) {
				if (current == 'V') {
					// lazily add 'V' to work around load ordering issues
					primitivesArray['V' - 'A'] = void.class;
					c = void.class;
				} else {
					/*[MSG "K05d7", "not a primitive: {0}"]*/
					throw new IllegalArgumentException(Msg.getString("K05d7", current)); //$NON-NLS-1$
				}
			}
		}
		args.add(c);

		return index;
	}

	/**
	 * This helper calls MethodType.fromMethodDescriptorString(...) or
	 * MethodType.fromMethodDescriptorStringAppendArg(...) but throws
	 * NoClassDefFoundError instead of TypeNotPresentException during
	 * the VM resolve stage.
	 *
	 * @param methodDescriptor - the method descriptor string
	 * @param loader - the ClassLoader to be used
	 * @param appendArgumentType - an extra argument type
	 *
	 * @return a MethodType object representing the method descriptor string
	 *
	 * @throws IllegalArgumentException - if the string is not well-formed
	 * @throws NoClassDefFoundError - if a named type cannot be found
	 */
	static final MethodType vmResolveFromMethodDescriptorString(String methodDescriptor, ClassLoader loader, Class<?> appendArgumentType) throws Throwable {
		try {
			MethodType result = MethodType.fromMethodDescriptorString(methodDescriptor, loader);
			if (null != appendArgumentType) {
				result = result.appendParameterTypes(appendArgumentType);
			}
			return result;
		} catch (TypeNotPresentException e) {
			Throwable cause = e.getCause();
			if (cause instanceof ClassNotFoundException) {
				NoClassDefFoundError noClassDefFoundError = new NoClassDefFoundError(cause.getMessage());
				noClassDefFoundError.initCause(cause);
				throw noClassDefFoundError;
			}
			throw e;
		}
	}
}
