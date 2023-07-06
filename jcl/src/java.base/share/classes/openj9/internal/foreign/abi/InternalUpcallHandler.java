/*[INCLUDE-IF JAVA_SPEC_VERSION >= 20]*/
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
package openj9.internal.foreign.abi;

import java.util.HashMap;
import java.util.List;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;

/*[IF JAVA_SPEC_VERSION >= 20]*/
/*[IF JAVA_SPEC_VERSION >= 21]*/
import java.lang.foreign.Arena;
/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
/*[IF JAVA_SPEC_VERSION >= 21]*/
import jdk.internal.foreign.abi.LinkerOptions;
/*[ELSE] JAVA_SPEC_VERSION >= 21 */
import java.lang.foreign.SegmentScope;
/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
/*[ELSE] JAVA_SPEC_VERSION >= 20 */
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.ResourceScope;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

/**
 * The internal implementation of upcall handler wraps up an upcall handle
 * enabling the native call to the java code at runtime.
 */
@SuppressWarnings("nls")
public final class InternalUpcallHandler {

	private final MemoryLayout[] argLayoutArray;
	private final MemoryLayout realReturnLayout;
	private final long thunkAddr;
	private UpcallMHMetaData metaData;

	/*[IF JAVA_SPEC_VERSION >= 21]*/
	/**
	 * The constructor creates an upcall handler specific to the requested java method
	 * by generating a native thunk in upcall on a given platform.
	 *
	 * @param target the target method handle in upcall
	 * @param mt the method type of the target method handle
	 * @param cDesc the function descriptor of the target method handle
	 * @param arena the Arena related to the upcall handler
	 * @param options the LinkerOptions indicating additional linking requirements to the linker
	 * @return an internal upcall handler with the thunk address
	 */
	public InternalUpcallHandler(MethodHandle target, MethodType mt, FunctionDescriptor cDesc, Arena arena, LinkerOptions options)
	/*[ELSEIF JAVA_SPEC_VERSION == 20]*/
	/**
	 * The constructor creates an upcall handler specific to the requested java method
	 * by generating a native thunk in upcall on a given platform.
	 *
	 * @param target the target method handle in upcall
	 * @param mt the method type of the target method handle
	 * @param cDesc the function descriptor of the target method handle
	 * @param scope the segment scope related to the upcall handler
	 * @return an internal upcall handler with the thunk address
	 */
	public InternalUpcallHandler(MethodHandle target, MethodType mt, FunctionDescriptor cDesc, SegmentScope scope)
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	/**
	 * The constructor creates an upcall handler specific to the requested java method
	 * by generating a native thunk in upcall on a given platform.
	 *
	 * @param target the target method handle in upcall
	 * @param mt the method type of the target method handle
	 * @param cDesc the function descriptor of the target method handle
	 * @param scope the resource scope related to the upcall handler
	 * @return an internal upcall handler with the thunk address
	 */
	public InternalUpcallHandler(MethodHandle target, MethodType mt, FunctionDescriptor cDesc, ResourceScope scope)
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	{
		List<MemoryLayout> argLayouts = cDesc.argumentLayouts();
		argLayoutArray = argLayouts.toArray(new MemoryLayout[argLayouts.size()]);
		realReturnLayout = cDesc.returnLayout().orElse(null); // Set to null for void

		/*[IF JAVA_SPEC_VERSION == 17]*/
		/* The layout check against the method type is still required for Java 17 in that both
		 * the function descriptor and the method type are passed in as arguments by users.
		 */
		TypeLayoutCheckHelper.checkIfValidLayoutAndType(mt, argLayoutArray, realReturnLayout);
		/*[ENDIF] JAVA_SPEC_VERSION == 17 */

		/*[IF JAVA_SPEC_VERSION >= 21]*/
		thunkAddr = getUpcallThunkAddr(target, arena, options);
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		thunkAddr = getUpcallThunkAddr(target, scope);
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	}

	/**
	 * Returns the address of the generated thunk at runtime.
	 *
	 * @return the thunk address
	 */
	public long upcallThunkAddr() {
		return thunkAddr;
	}

	/* Check whether the thunk address of the requested java method exists in the cache;
	 * otherwise, call the native to request the JIT to generate an upcall thunk for this
	 * java method.
	 */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private long getUpcallThunkAddr(MethodHandle target, Arena arena, LinkerOptions options)
	/*[ELSEIF JAVA_SPEC_VERSION == 20]*/
	private long getUpcallThunkAddr(MethodHandle target, SegmentScope scope)
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private long getUpcallThunkAddr(MethodHandle target, ResourceScope scope)
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	{
		int argLayoutCount = argLayoutArray.length;
		/* The last element of the native signature array is for the return type. */
		String[] nativeSignatureStrs = new String[argLayoutCount + 1];
		for (int argIndex = 0; argIndex < argLayoutCount; argIndex++) {
			MemoryLayout argLayout = argLayoutArray[argIndex];
			nativeSignatureStrs[argIndex] = LayoutStrPreprocessor.getSimplifiedLayoutString(argLayout, false);
		}

		/* Set the void layout string intended for the underlying native code
		 * as the corresponding layout doesn't exist in the Spec.
		 * Note: 'V' stands for the void type.
		 */
		if (realReturnLayout == null) {
			nativeSignatureStrs[argLayoutCount] = "0#V";
		} else {
			nativeSignatureStrs[argLayoutCount] = LayoutStrPreprocessor.getSimplifiedLayoutString(realReturnLayout, false);
		}

		/* The thunk must be created for each upcall handler given the UpcallMHMetaData object uniquely
		 * bound to the thunk is only alive for a Scope (JDK21+)/SegmentScope (JDK20)/ResourceScope (JDK17)
		 * specified in java, which means the upcall handler and its UpcallMHMetaData object will be
		 * cleaned up automatically once their session/scope is closed.
		 */
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		metaData = new UpcallMHMetaData(target, argLayoutCount, arena.scope(), options);
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		metaData = new UpcallMHMetaData(target, argLayoutCount, scope);
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
		return allocateUpcallStub(metaData, nativeSignatureStrs);
	}

	/* This native requests the JIT to generate an upcall thunk of the specified java method
	 * by invoking createUpcallThunk() and returns the requested thunk address.
	 */
	private native long allocateUpcallStub(UpcallMHMetaData mhMetaData, String[] cSignatureStrs);

}
