/*[INCLUDE-IF Panama]*/
package java.lang.invoke;
/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

import java.nicl.LibrarySymbol;

import com.ibm.oti.vm.VMLangInvokeAccess;


/**
 * Helper class to allow privileged access to classes
 * from outside the java.lang.invoke package. Based on sun.misc.SharedSecrets
 * implementation.
 */
final class VMInvokeAccess implements VMLangInvokeAccess {
	/**
	 * Create a new NativeMethodHandle
	 *
	 * @param methodName - the name of the method
	 * @param type - the MethodType of the method
	 * @param nativeAddress - address of the method
	 * @param layoutStrings - a String array that contains the layout string of the return argument, followed by the layout strings of the input arguments of the method.
	 * 		The layout string is null for primitive arguments. If all the arguments are primitives, the layoutStrings array is itself null.
	 * @return A new NativeMethodHandle
	 * @throws IllegalAccessException if fails to extract native address from symbol
	 */
	@Override
	public NativeMethodHandle generateNativeMethodHandle(String methodName, MethodType type, long nativeAddress, String[] layoutStrings) throws IllegalAccessException {
		return new NativeMethodHandle(methodName, type, nativeAddress, layoutStrings);
	}
	
	/**
	 * Return a MethodHandle to a native method.
	 * 
	 * @param methodName - the name of the method
	 * @param type - the MethodType of the method
	 * @param nativeAddress - address of the method
	 * @return A MethodHandle able to invoke the requested native method
	 */
	@Override
	public MethodHandle findNative(String methodName, MethodType type, long nativeAddress) throws IllegalAccessException {
		return new NativeMethodHandle(methodName, type, nativeAddress);
	}

}
