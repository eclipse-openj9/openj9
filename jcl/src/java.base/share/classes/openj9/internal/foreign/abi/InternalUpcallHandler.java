/*[INCLUDE-IF JAVA_SPEC_VERSION >= 21]*/
/*
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
 */
package openj9.internal.foreign.abi;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import static java.lang.invoke.MethodType.methodType;
import java.lang.invoke.WrongMethodTypeException;
import java.util.HashMap;
import java.util.List;
import java.util.Set;

/*[IF JAVA_SPEC_VERSION >= 21]*/
import java.lang.foreign.AddressLayout;
import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import jdk.internal.foreign.abi.LinkerOptions;
import java.lang.foreign.ValueLayout;
/*[ELSE] JAVA_SPEC_VERSION >= 21 */
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.ValueLayout;
/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

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
	private final LinkerOptions linkerOpts;
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

	static final Lookup lookup = MethodHandles.lookup();

	/* The argument filters intended to validate the pointer/struct arguments/return value of the upcall method. */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private static final MethodHandle argRetSegmtOfPtrFilter;
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private static final MethodHandle argRetAddrOfPtrFilter;
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	private static final MethodHandle argRetSegmtFilter;

	static {
		try {
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			argRetSegmtOfPtrFilter = lookup.findStatic(InternalUpcallHandler.class, "argRetSegmtOfPtr", methodType(MemorySegment.class, MemorySegment.class, MemoryLayout.class, LinkerOptions.class));
			/*[ELSE] JAVA_SPEC_VERSION >= 21 */
			argRetAddrOfPtrFilter = lookup.findStatic(InternalUpcallHandler.class, "argRetAddrOfPtr", methodType(MemoryAddress.class, MemoryAddress.class));
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
			argRetSegmtFilter = lookup.findStatic(InternalUpcallHandler.class, "argRetSegmt", methodType(MemorySegment.class, MemorySegment.class));
		} catch (IllegalAccessException | NoSuchMethodException e) {
			throw new InternalError(e);
		}
	}

	/* Intended for argRetSegmtOfPtrFilter that validates the memory segment
	 * of the passed-in pointer argument.
	 */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private static MemorySegment argRetSegmtOfPtr(MemorySegment argValue, MemoryLayout layout, LinkerOptions options) throws IllegalStateException {
		UpcallMHMetaData.validateNativeArgRetSegmentOfPtr(argValue, options);
		return UpcallMHMetaData.getArgRetAlignedSegmentOfPtr(argValue.address(), layout);
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private static MemoryAddress argRetAddrOfPtr(MemoryAddress argValue) throws IllegalStateException {
		UpcallMHMetaData.validateNativeArgRetAddrOfPtr(argValue);
		return argValue;
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

	/* Intended for argRetSegmtFilter to determine whether the segment is allocated on heap or not
	 * and return the newly allocated segment without all values copied from the heap segment.
	 */
	private static MemorySegment argRetSegmt(MemorySegment argValue) throws IllegalStateException {
		return UpcallMHMetaData.getNativeArgRetSegment(argValue);
	}

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
	 */
	public InternalUpcallHandler(MethodHandle target, MethodType mt, FunctionDescriptor cDesc, Arena arena, LinkerOptions options)
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	/**
	 * The constructor creates an upcall handler specific to the requested java method
	 * by generating a native thunk in upcall on a given platform.
	 *
	 * @param target the target method handle in upcall
	 * @param mt the method type of the target method handle
	 * @param cDesc the function descriptor of the target method handle
	 * @param scope the resource scope related to the upcall handler
	 */
	public InternalUpcallHandler(MethodHandle target, MethodType mt, FunctionDescriptor cDesc, ResourceScope scope)
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	{
		List<MemoryLayout> argLayouts = cDesc.argumentLayouts();
		argLayoutArray = argLayouts.toArray(new MemoryLayout[argLayouts.size()]);
		realReturnLayout = cDesc.returnLayout().orElse(null); // Set to null for void
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		linkerOpts = options;
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

		/*[IF JAVA_SPEC_VERSION == 17]*/
		/* The layout check against the method type is still required for Java 17 in that both
		 * the function descriptor and the method type are passed in as arguments by users.
		 */
		TypeLayoutCheckHelper.checkIfValidLayoutAndType(mt, argLayoutArray, realReturnLayout);
		/*[ENDIF] JAVA_SPEC_VERSION == 17 */

		/* Replace the original handle with the specified types of the C function. */
		MethodHandle boundHandle = getUpcallMHWithFilters(target);

		/*[IF JAVA_SPEC_VERSION >= 21]*/
		thunkAddr = getUpcallThunkAddr(boundHandle, arena, options);
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		thunkAddr = getUpcallThunkAddr(boundHandle, scope);
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

		/* The thunk must be created for each upcall handler given the UpcallMHMetaData object
		 * uniquely bound to the thunk is only alive for a memory scope specified in java, which
		 * means the upcall handler and its UpcallMHMetaData related resources will be cleaned up
		 * automatically once the scope is closed.
		 */
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		metaData = new UpcallMHMetaData(target, argLayoutCount, arena.scope(), options);
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		metaData = new UpcallMHMetaData(target, argLayoutCount, scope);
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
		return allocateUpcallStub(metaData, nativeSignatureStrs);
	}

	/* Process the upcall method handle by setting the filters for the passed-in arguments and the return value. */
	private MethodHandle getUpcallMHWithFilters(MethodHandle targetHandle) throws NullPointerException, WrongMethodTypeException {
		Class<?>[] argTypeClasses = targetHandle.type().parameterArray();
		int upcallArgCount = argTypeClasses.length;

		/* Set the filters for the passed-in arguments of the upcall method via
		 * filterArguments() to validate the arguments prior to the upcall.
		 */
		MethodHandle[] argFilters = new MethodHandle[upcallArgCount];
		for (int argIndex = 0; argIndex < upcallArgCount; argIndex++) {
			argFilters[argIndex] = getUpcallArgRetValFilter(argTypeClasses[argIndex], argLayoutArray[argIndex]);
		}
		MethodHandle boundHandle = MethodHandles.filterArguments(targetHandle, 0, argFilters);

		/* Set the filter for return value of the upcall method via filterReturnValue()
		 * to validate the return value from the upcall before returning back to the
		 * upcall dispatcher in native.
		 */
		MethodHandle retFilter = getUpcallArgRetValFilter(targetHandle.type().returnType(), realReturnLayout);
		boundHandle = MethodHandles.filterReturnValue(boundHandle, retFilter);

		return boundHandle;
	}

	/* Obtain the filter that validates the passed-in argument or the return value of the upcall method. */
	private MethodHandle getUpcallArgRetValFilter(Class<?> argTypeClass, MemoryLayout argLayout) {
		MethodHandle filterMH = null;

		/* The filter for primitive is a placeholder without any conversion involved. */
		if (argTypeClass == void.class) {
			filterMH = MethodHandles.empty(methodType(argTypeClass));
		}
		/*[IF JAVA_SPEC_VERSION == 17]*/
		else if (argTypeClass == MemoryAddress.class) {
			filterMH = argRetAddrOfPtrFilter;
		}
		/*[ENDIF] JAVA_SPEC_VERSION == 17 */
		else if (argLayout instanceof ValueLayout) {
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			if (argLayout instanceof AddressLayout) {
				filterMH = MethodHandles.insertArguments(argRetSegmtOfPtrFilter, 1, argLayout, linkerOpts);
			} else
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
			{
				/* The filter for primitive is a placeholder without any conversion involved. */
				filterMH = MethodHandles.identity(argTypeClass);
			}
		} else if (argLayout instanceof GroupLayout) {
			filterMH = argRetSegmtFilter;
		}

		return filterMH;
	}

	/* This native requests the JIT to generate an upcall thunk of the specified java method
	 * by invoking createUpcallThunk() and returns the requested thunk address.
	 */
	private native long allocateUpcallStub(UpcallMHMetaData mhMetaData, String[] cSignatureStrs);

}
