/*[INCLUDE-IF JAVA_SPEC_VERSION >= 19]*/
/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
package openj9.internal.foreign.abi;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;

/**
 * The meta data consists of the callee MH and a cache of 2 elements for MH resolution,
 * which are used to generate an upcall handler to the requested java method.
 */
final class UpcallMHMetaData {

	/* The target method handle intended for upcall which is placed on the java stack
	 * in icallVMprJavaUpcallImpl for call-in so as to invoke the method handle.
	 */
	private final MethodHandle calleeMH;
	/* The method type of the target method handle which is mainly used by
	 * MethodHandleResolver.upcallLinkCallerMethod() to populate
	 * the cache array as below.
	 */
	private final MethodType calleeType;
	/* The cache array store MemberName and appendix which are populated
	 * by MethodHandleResolver.upcallLinkCallerMethod().
	 */
	private Object[] invokeCache;

	private static synchronized native void resolveUpcallDataFields();

	static {
		/* Resolve the fields (offset in the JCL constant pool of VM) specific to the metadata plus the fields
		 * of MemoryAddressImpl and NativeMemorySegmentImpl given the generated macros from vmconstantpool.xml
		 * depend on their offsets to access the corresponding fields in the process of the upcall.
		 */
		resolveUpcallDataFields();
	}

	UpcallMHMetaData(MethodHandle targetHandle) {
		calleeMH = targetHandle;
		calleeType = targetHandle.type();
	}
}
