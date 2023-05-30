/*[INCLUDE-IF (JAVA_SPEC_VERSION >= 16) & (JAVA_SPEC_VERSION <= 18)]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
package jdk.internal.foreign.abi;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;

import jdk.incubator.foreign.FunctionDescriptor;
/*[IF JAVA_SPEC_VERSION == 18]*/
import jdk.incubator.foreign.NativeSymbol;
/*[ENDIF] JAVA_SPEC_VERSION == 18 */
import jdk.incubator.foreign.ResourceScope;
import openj9.internal.foreign.abi.InternalUpcallHandler;

/**
 * The counterpart in OpenJDK is replaced with this class that wraps up
 * an upcall handler enabling the native call to the java code at runtime.
 */
/*[IF JAVA_SPEC_VERSION >= 18]*/
/* UpcallHandler has been removed in Java 18 */
public final class ProgrammableUpcallHandler
/*[ELSE] JAVA_SPEC_VERSION >= 18 */
public final class ProgrammableUpcallHandler implements UpcallHandler
/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
{
	private final long thunkAddr;

	/* The constructor creates an upcall handler specific to the requested java method
	 * by generating a native thunk in upcall on a given platform.
	 */
	ProgrammableUpcallHandler(MethodHandle target, MethodType mt, FunctionDescriptor cDesc, ResourceScope scope) {
		InternalUpcallHandler internalUpcallHandler = new InternalUpcallHandler(target, mt, cDesc, scope);
		/* The thunk address must be set given entryPoint() is used in OpenJDK */
		thunkAddr = internalUpcallHandler.upcallThunkAddr();
	}

	/**
	 * Returns the address of the generated thunk at runtime.
	 *
	 * @return the thunk address
	 */
	public long entryPoint() {
		return thunkAddr;
	}

	/**
	 * The method invoked via Clinker generates a native thunk to create
	 * a native symbol that holds an entry point to the native function
	 * intended for the requested java method in upcall.
	 *
	 * @param target The upcall method handle to the requested java method
	 * @param mt The MethodType of the upcall method handle
	 * @param cDesc The FunctionDescriptor of the upcall method handle
	 * @param scope The ResourceScope of the upcall method handle
	 * @return the native function symbol(JDK18) or the upcall hander(JDK17) in upcall
	 */
	/*[IF JAVA_SPEC_VERSION == 18]*/
	public static NativeSymbol makeUpcall(MethodHandle target, MethodType mt, FunctionDescriptor cDesc, ResourceScope scope) {
		ProgrammableUpcallHandler upcallHandler = new ProgrammableUpcallHandler(target, mt, cDesc, scope);
		return UpcallStubs.makeUpcall(upcallHandler.entryPoint(), scope);
	}
	/*[ELSE] JAVA_SPEC_VERSION == 18 */
	public static UpcallHandler makeUpcall(MethodHandle target, MethodType mt, FunctionDescriptor cDesc, ResourceScope scope) {
		return new ProgrammableUpcallHandler(target, mt, cDesc, scope);
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 18 */
}
