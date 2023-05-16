/*[INCLUDE-IF JAVA_SPEC_VERSION >= 19]*/
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package openj9.internal.foreign.abi;

import java.util.HashMap;
import java.util.List;
/*[IF JAVA_SPEC_VERSION >= 17]*/
/*[IF JAVA_SPEC_VERSION >= 18]*/
import java.util.Objects;
/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
import java.util.concurrent.ConcurrentHashMap;
import java.util.Set;
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import static java.lang.invoke.MethodHandles.*;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import static java.lang.invoke.MethodType.methodType;
import java.lang.invoke.WrongMethodTypeException;

/*[IF JAVA_SPEC_VERSION >= 20]*/
import java.lang.foreign.Arena;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
/*[IF JAVA_SPEC_VERSION >= 19]*/
/*[IF JAVA_SPEC_VERSION == 19]*/
import java.lang.foreign.Addressable;
/*[ENDIF] JAVA_SPEC_VERSION == 19 */
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
/*[IF JAVA_SPEC_VERSION == 19]*/
import java.lang.foreign.MemoryAddress;
/*[ENDIF] JAVA_SPEC_VERSION == 19 */
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
/*[IF JAVA_SPEC_VERSION == 19]*/
import java.lang.foreign.MemorySession;
/*[ENDIF] JAVA_SPEC_VERSION == 19 */
import java.lang.foreign.SegmentAllocator;
/*[IF JAVA_SPEC_VERSION >= 20]*/
import java.lang.foreign.SegmentScope;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
import java.lang.foreign.ValueLayout;
/*[IF JAVA_SPEC_VERSION >= 20]*/
import static java.lang.foreign.ValueLayout.*;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
import java.lang.foreign.VaList;
/*[IF JAVA_SPEC_VERSION >= 20]*/
import jdk.internal.foreign.Utils;
import jdk.internal.foreign.abi.LinkerOptions;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
import jdk.internal.foreign.MemorySessionImpl;
/*[ELSE] JAVA_SPEC_VERSION >= 19 */
import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemorySegment;
/*[IF JAVA_SPEC_VERSION == 18]*/
import jdk.incubator.foreign.NativeSymbol;
import jdk.incubator.foreign.VaList;
/*[ENDIF] JAVA_SPEC_VERSION == 18 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
import jdk.incubator.foreign.ResourceScope;
/*[IF JAVA_SPEC_VERSION == 17]*/
import jdk.incubator.foreign.ResourceScope.Handle;
/*[ENDIF] JAVA_SPEC_VERSION == 17 */
import jdk.incubator.foreign.SegmentAllocator;
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
import jdk.incubator.foreign.ValueLayout;
/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

/**
 * The internal implementation of downcall handler wraps up a method handle enabling
 * the native code to the ffi_call via the libffi interface at runtime.
 */
public class InternalDowncallHandler {

	private final MethodType funcMethodType;
	private final FunctionDescriptor funcDescriptor;
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	private final LinkerOptions linkerOpts;
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	/*[IF JAVA_SPEC_VERSION == 16]*/
	private Addressable functionAddr;
	/*[ENDIF] JAVA_SPEC_VERSION == 16 */
	private long cifNativeThunkAddr;
	private long argTypesAddr;
	private MemoryLayout[] argLayoutArray;
	private MemoryLayout realReturnLayout;

	/* The hashtables of sessions/scopes is intended for multithreading in which case
	 * the same downcall handler might hold various sessions/scopes being used by
	 * different threads in downcall.
	 */
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	private Set<SegmentScope> memArgScopeSet;
	/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
	private Set<MemorySession> memArgScopeSet;
	/*[ELSE] JAVA_SPEC_VERSION == 19 */
	private Set<ResourceScope> memArgScopeSet;
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	/*[IF JAVA_SPEC_VERSION == 17]*/
	private final ConcurrentHashMap<ResourceScope, Handle> scopeHandleMap;
	/*[ENDIF] JAVA_SPEC_VERSION == 17 */

	static final Lookup lookup = MethodHandles.lookup();

	/* The prep_cif and the corresponding argument types are cached & shared in multiple downcalls/threads. */
	private static final HashMap<Integer, Long> cachedCifNativeThunkAddr = new HashMap<>();
	private static final HashMap<Integer, Long> cachedArgTypes = new HashMap<>();

	/* Argument filters that convert the primitive types/MemoryAddress/MemorySegment to long. */
	private static final MethodHandle booleanToLongArgFilter;
	private static final MethodHandle charToLongArgFilter;
	private static final MethodHandle byteToLongArgFilter;
	private static final MethodHandle shortToLongArgFilter;
	private static final MethodHandle intToLongArgFilter;
	private static final MethodHandle floatToLongArgFilter;
	private static final MethodHandle doubleToLongArgFilter;
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	private MethodHandle memSegmtOfPtrToLongArgFilter;
	/*[ELSE] JAVA_SPEC_VERSION >= 20 */
	private MethodHandle memAddrToLongArgFilter;
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	private MethodHandle memSegmtToLongArgFilter;

	/* Return value filters that convert the Long object to the primitive types/MemoryAddress/MemorySegment. */
	private static final MethodHandle longObjToVoidRetFilter;
	private static final MethodHandle longObjToBooleanRetFilter;
	private static final MethodHandle longObjToCharRetFilter;
	private static final MethodHandle longObjToByteRetFilter;
	private static final MethodHandle longObjToShortRetFilter;
	private static final MethodHandle longObjToIntRetFilter;
	private static final MethodHandle longObjToLongRetFilter;
	private static final MethodHandle longObjToFloatRetFilter;
	private static final MethodHandle longObjToDoubleRetFilter;
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	private MethodHandle longObjToMemSegmtRetFilter;
	/*[ELSE] JAVA_SPEC_VERSION >= 20 */
	private static final MethodHandle longObjToMemAddrRetFilter;
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	private static final MethodHandle objToMemSegmtRetFilter;

	private static synchronized native void resolveRequiredFields();
	private native void initCifNativeThunkData(String[] argLayouts, String retLayout, boolean newArgTypes, int varArgIndex);
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	private native long invokeNative(long returnStateMemAddr, long returnStructMemAddr, long functionAddress, long calloutThunk, long[] argValues);
	/*[ELSE] JAVA_SPEC_VERSION >= 20 */
	private native long invokeNative(long returnStructMemAddr, long functionAddress, long calloutThunk, long[] argValues);
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	private static final class PrivateClassLock {
		PrivateClassLock() {}
	}
	private static final Object privateClassLock = new PrivateClassLock();

	static {
		try {
			/* Set up the argument filters for the primitive types and MemoryAddress. */
			booleanToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "booleanToLongArg", methodType(long.class, boolean.class)); //$NON-NLS-1$
			charToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "charToLongArg", methodType(long.class, char.class)); //$NON-NLS-1$
			byteToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "byteToLongArg", methodType(long.class, byte.class)); //$NON-NLS-1$
			shortToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "shortToLongArg", methodType(long.class, short.class)); //$NON-NLS-1$
			intToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "intToLongArg", methodType(long.class, int.class)); //$NON-NLS-1$
			floatToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "floatToLongArg", methodType(long.class, float.class)); //$NON-NLS-1$
			doubleToLongArgFilter = lookup.findStatic(Double.class, "doubleToLongBits", methodType(long.class, double.class)); //$NON-NLS-1$

			/* Set up the return value filters for the primitive types and MemoryAddress. */
			longObjToVoidRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToVoidRet", methodType(void.class, Object.class)); //$NON-NLS-1$
			longObjToBooleanRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToBooleanRet", methodType(boolean.class, Object.class)); //$NON-NLS-1$
			longObjToCharRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToCharRet", methodType(char.class, Object.class)); //$NON-NLS-1$
			longObjToByteRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToByteRet", methodType(byte.class, Object.class)); //$NON-NLS-1$
			longObjToShortRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToShortRet", methodType(short.class, Object.class)); //$NON-NLS-1$
			longObjToIntRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToIntRet", methodType(int.class, Object.class)); //$NON-NLS-1$
			longObjToLongRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToLongRet", methodType(long.class, Object.class)); //$NON-NLS-1$
			longObjToFloatRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToFloatRet", methodType(float.class, Object.class)); //$NON-NLS-1$
			longObjToDoubleRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToDoubleRet", methodType(double.class, Object.class)); //$NON-NLS-1$
			/*[IF JAVA_SPEC_VERSION <= 19]*/
			longObjToMemAddrRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToMemAddrRet", methodType(MemoryAddress.class, Object.class)); //$NON-NLS-1$
			/*[ENDIF] JAVA_SPEC_VERSION <= 19 */
			objToMemSegmtRetFilter = lookup.findStatic(InternalDowncallHandler.class, "objToMemSegmtRet", methodType(MemorySegment.class, Object.class)); //$NON-NLS-1$
		} catch (IllegalAccessException | NoSuchMethodException e) {
			throw new InternalError(e);
		}

		/* Resolve the required fields (specifically their offset in the jcl constant pool of VM)
		 * which can be shared in multiple calls or across threads given the generated macros
		 * in the vmconstantpool.xml depend on their offsets to access the corresponding fields.
		 * Note: the value of these fields varies with different instances.
		 */
		resolveRequiredFields();
	}

	/* Intended for booleanToLongArgFilter that converts boolean to long. */
	private static final long booleanToLongArg(boolean argValue) {
		return argValue ? 1 : 0;
	}

	/* Intended for charToLongArgFilter that converts char to long. */
	private static final long charToLongArg(char argValue) {
		return argValue;
	}

	/* Intended for byteToLongArgFilter that converts byte to long. */
	private static final long byteToLongArg(byte argValue) {
		return argValue;
	}

	/* Intended for shortToLongArgFilter that converts short to long given
	 * short won't be casted to long automatically in filterArguments().
	 */
	private static final long shortToLongArg(short argValue) {
		return argValue;
	}

	/* Intended for intToLongArgFilter that converts int to long given
	 * int won't be casted to long automatically in filterArguments().
	 */
	private static final long intToLongArg(int argValue) {
		return argValue;
	}

	/* Intended for floatToLongArgFilter that converts the int value from Float.floatToIntBits()
	 * to long given int won't be casted to long automatically in filterArguments().
	 */
	private static final long floatToLongArg(float argValue) {
		return Float.floatToIntBits(argValue);
	}

	/*[IF JAVA_SPEC_VERSION >= 17]*/
	/* Save the active session of the specified passed-in memory specific argument in the downcall handler
	 * given the argument might be created within different sessions/scopes.
	 */
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	private final void addMemArgScope(SegmentScope memArgScope) throws IllegalStateException
	/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
	private final void addMemArgScope(MemorySession memArgScope) throws IllegalStateException
	/*[ELSE] JAVA_SPEC_VERSION == 19 */
	private final void addMemArgScope(ResourceScope memArgScope) throws IllegalStateException
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	{
		validateMemScope(memArgScope);
		if (!memArgScopeSet.contains(memArgScope)) {
			memArgScopeSet.add(memArgScope);
		}
	}

	/* Validate the memory related scope to ensure that it is kept alive during the downcall. */
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	private void validateMemScope(SegmentScope memScope) throws IllegalStateException
	/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
	private void validateMemScope(MemorySession memScope) throws IllegalStateException
	/*[ELSE] JAVA_SPEC_VERSION == 19 */
	private void validateMemScope(ResourceScope memScope) throws IllegalStateException
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	{
		if (!memScope.isAlive())
		{
			/*[IF JAVA_SPEC_VERSION == 19]*/
			String scopeName = "session";
			/*[ELSE] JAVA_SPEC_VERSION == 19 */
			String scopeName = "scope";
			/*[ENDIF] JAVA_SPEC_VERSION == 19 */
			throw new IllegalStateException("Already closed: attempted to access the memory value in a closed " + scopeName);  //$NON-NLS-1$
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

	/*[IF JAVA_SPEC_VERSION >= 20]*/
	/* Intended for memSegmtOfPtrToLongArgFilter that converts the memory segment
	 * of the passed-in pointer argument to long.
	 */
	private final long memSegmtOfPtrToLongArg(MemorySegment argValue) throws IllegalStateException {
		addMemArgScope(argValue.scope());
		return UpcallMHMetaData.getNativeArgRetSegmentOfPtr(argValue);
	}
	/*[ELSEIF JAVA_SPEC_VERSION >= 18] */
	/* Intended for memAddrToLongArgFilter that converts the memory address to long.
	 * Note: the passed-in argument can be an instance of MemoryAddress, MemorySegment
	 * or VaList which extends Addressable in OpenJDK since Java 18 featured with
	 * JEP419 (Second Incubator).
	 */
	private final long memAddrToLongArg(Addressable argValue) throws IllegalStateException {
		/* Only check MemorySegment and VaList given MemoryAddress.scope() doesn't exist in JDK17. */
		if (argValue instanceof MemorySegment value) {
			/*[IF JAVA_SPEC_VERSION == 19]*/
			addMemArgScope(value.session());
			/*[ELSE] JAVA_SPEC_VERSION == 19 */
			addMemArgScope(value.scope());
			/*[ENDIF] JAVA_SPEC_VERSION == 19 */
		} else if (argValue instanceof VaList value) {
			/*[IF JAVA_SPEC_VERSION == 19]*/
			addMemArgScope(value.session());
			/*[ELSE] JAVA_SPEC_VERSION == 19 */
			addMemArgScope(value.scope());
			/*[ENDIF] JAVA_SPEC_VERSION == 19 */
		}
		/*[IF JAVA_SPEC_VERSION == 18]*/
		else if (argValue instanceof NativeSymbol value) {
			addMemArgScope(value.scope());
		}
		/*[ENDIF] JAVA_SPEC_VERSION == 18 */

		/* Instead of assigning nativeAddr with argValue.address().toRawLongValue() by fefault,
		 * (which triggers the exception in the case of the on-heap segment in JDK19), the
		 * argument is validated at first in the case of MemorySegment/MemoryAddress.
		 */
		long nativeAddr = 0;
		if (argValue instanceof MemorySegment value) {
			nativeAddr = UpcallMHMetaData.getNativeArgRetSegmentOfPtr(value);
		} else if (argValue instanceof MemoryAddress value) {
			nativeAddr = UpcallMHMetaData.getNativeArgRetAddrOfPtr(value);
		} else {
			nativeAddr = argValue.address().toRawLongValue();
		}
		return nativeAddr;
	}
	/*[ELSEIF JAVA_SPEC_VERSION == 17]*/
	/* Intended for memAddrToLongArgFilter that converts the memory address to long. */
	private final long memAddrToLongArg(MemoryAddress argValue) throws IllegalStateException {
		addMemArgScope(argValue.scope());
		return UpcallMHMetaData.getNativeArgRetAddrOfPtr(argValue);
	}
	/*[ELSE] JAVA_SPEC_VERSION == 17 */
	/* Intended for memAddrToLongArgFilter that converts the memory address to long. */
	private static final long memAddrToLongArg(MemoryAddress argValue) {
		return argValue.address().toRawLongValue();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	/* Intended for memSegmtToLongArgFilter that converts the memory segment to long. */
	private final long memSegmtToLongArg(MemorySegment argValue) throws IllegalStateException {
		/*[IF JAVA_SPEC_VERSION == 19]*/
		addMemArgScope(argValue.session());
		/*[ELSE] JAVA_SPEC_VERSION == 19 */
		addMemArgScope(argValue.scope());
		/*[ENDIF] JAVA_SPEC_VERSION == 19 */
		return UpcallMHMetaData.getNativeArgRetSegment(argValue);
	}

	/* Intended for longObjToVoidRetFilter that converts the Long object to void. */
	private static final void longObjToVoidRet(Object retValue) {
		return;
	}

	/* Intended for longObjToBooleanRetFilter that converts the Long object to boolean. */
	private static final boolean longObjToBooleanRet(Object retValue) {
		boolean resultValue = ((Long)retValue).longValue() != 0;
		return resultValue;
	}

	/* Intended for longObjToCharRetFilter that converts the Long object to char. */
	private static final char longObjToCharRet(Object retValue) {
		return (char)(((Long)retValue).shortValue());
	}

	/* Intended for longObjToByteRetFilter that converts the Long object to byte. */
	private static final byte longObjToByteRet(Object retValue) {
		return ((Long)retValue).byteValue();
	}

	/* Intended for longObjToShortRetFilter that converts the Long object to short. */
	private static final short longObjToShortRet(Object retValue) {
		return ((Long)retValue).shortValue();
	}

	/* Intended for longObjToIntRetFilter that converts the Long object to int. */
	private static final int longObjToIntRet(Object retValue) {
		return ((Long)retValue).intValue();
	}

	/* Intended for longObjToLongRetFilter that converts the Long object to long. */
	private static final long longObjToLongRet(Object retValue) {
		return ((Long)retValue).longValue();
	}

	/* Intended for longObjToFloatRetFilter that converts the Long object to float with Float.floatToIntBits(). */
	private static final float longObjToFloatRet(Object retValue) {
		int tmpValue = ((Long)retValue).intValue();
		return Float.intBitsToFloat(tmpValue);
	}

	/* Intended for longObjToFloatRetFilter that converts the Long object to double with Double.longBitsToDouble(). */
	private static final double longObjToDoubleRet(Object retValue) {
		long tmpValue = ((Long)retValue).longValue();
		return Double.longBitsToDouble(tmpValue);
	}

	/*[IF JAVA_SPEC_VERSION >= 20]*/
	/* Intended for longObjToMemSegmtRetFilter that converts the Long object to the memory segment. */
	private MemorySegment longObjToMemSegmtRet(Object retValue) {
		long tmpValue = ((Long)retValue).longValue();
		/* Utils.pointeeSize() introduced in JDK20 calls isUnbounded() for the ADDRESS layout
		 * to determine whether the specified address layout is an unbounded address or not.
		 * For an unbounded address, it returns Long.MAX_VALUE for direct access; otherwise,
		 * it returns zero in which case the address can't be directly accessed.
		 */
		return MemorySegment.ofAddress(tmpValue, Utils.pointeeSize(realReturnLayout));
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 20 */
	/* Intended for longObjToMemAddrRetFilter that converts the Long object to the memory address. */
	private static final MemoryAddress longObjToMemAddrRet(Object retValue) {
		long tmpValue = ((Long)retValue).longValue();
		return MemoryAddress.ofLong(tmpValue);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	/* Intended for objToMemSegmtRetFilter that simply casts the passed-in object to the memory segment
	 * given the requested the memory segment is directly returned from runNativeMethod().
	 * Note: the returned memory address is exactly the address of the memory previously allocated
	 * for the specified struct layout on return.
	 */
	private static final MemorySegment objToMemSegmtRet(Object retValue) {
		return (MemorySegment)retValue;
	}

	/*[IF JAVA_SPEC_VERSION >= 20]*/
	/**
	 * The internal constructor is responsible for mapping the preprocessed layouts
	 * of return type & argument types to the underlying prep_cif in native.
	 *
	 * @param functionMethodType The MethodType of the specified native function
	 * @param funcDesc The function descriptor of the specified native function
	 * @param options The linker options indicating additional linking requirements to the linker
	 */
	public InternalDowncallHandler(MethodType functionMethodType, FunctionDescriptor functionDescriptor, LinkerOptions options)
	/*[ELSEIF JAVA_SPEC_VERSION >= 17]*/
	/**
	 * The internal constructor is responsible for mapping the preprocessed layouts
	 * of return type & argument types to the underlying prep_cif in native.
	 *
	 * @param functionMethodType The MethodType of the specified native function
	 * @param funcDesc The function descriptor of the specified native function
	 */
	public InternalDowncallHandler(MethodType functionMethodType, FunctionDescriptor functionDescriptor)
	/*[ELSE] JAVA_SPEC_VERSION >= 17 */
	/**
	 * The internal constructor is responsible for mapping the preprocessed layouts
	 * of return type & argument types to the underlying prep_cif in native.
	 *
	 * @param downcallAddr The downcall symbol
	 * @param functionMethodType The MethodType of the specified native function
	 * @param funcDesc The function descriptor of the specified native function
	 */
	public InternalDowncallHandler(Addressable downcallAddr, MethodType functionMethodType, FunctionDescriptor functionDescriptor)
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	{
		realReturnLayout = functionDescriptor.returnLayout().orElse(null); // set to null for void
		List<MemoryLayout> argLayouts = functionDescriptor.argumentLayouts();
		argLayoutArray = argLayouts.toArray(new MemoryLayout[argLayouts.size()]);

		/*[IF JAVA_SPEC_VERSION <= 17]*/
		/* The layout check against the method type is still required for Java 16 & 17 in that
		 * both the function descriptor and the method type are passed in as arguments by users.
		 * Note: skip the validity check on function descriptor in Java 18 as it is done before
		 * initializing ProgrammableInvoker in OpenJDK. Meanwhile, the method type is directly
		 * deduced from the function descriptor itself, in which case there is no need to
		 * check the layout against the method type.
		 */
		TypeLayoutCheckHelper.checkIfValidLayoutAndType(functionMethodType, argLayoutArray, realReturnLayout);

		/*[IF JAVA_SPEC_VERSION == 16]*/
		/* 1) The native function address has been removed from the parameter lists of downcallHandle() APIs
		 * since JDK17 so as to being passed in as the first argument when invoking the returned downcall handle
		 * or bound as the first argument via MethodHandles.insertArguments() beforehand in OpenJDK.
		/* 2) As explained in the Spec of LibraryLookup in JDK16, the downcall must hold a strong reference to
		 * the native library symbol to prevent the underlying native library from being unloaded during the
		 * native calls, which is no longer required since JDK17 as the symbol intende for for the current thread
		 * is directly passed to the native invocation so as to fit in the multithreading environment.
		 * Note: the passed-in addressable parameter can be either LibraryLookup.Symbol or MemoryAddress.
		 */
		functionAddr = downcallAddr;
		/*[ENDIF] JAVA_SPEC_VERSION == 16 */
		/*[ENDIF] JAVA_SPEC_VERSION <= 17 */

		funcMethodType = functionMethodType;
		funcDescriptor = functionDescriptor;
		/*[IF JAVA_SPEC_VERSION >= 20]*/
		linkerOpts = options;
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

		cifNativeThunkAddr = 0;
		argTypesAddr = 0;

		memArgScopeSet = ConcurrentHashMap.newKeySet();
		/*[IF JAVA_SPEC_VERSION == 17]*/
		scopeHandleMap = new ConcurrentHashMap<>();
		/*[ENDIF] JAVA_SPEC_VERSION == 17 */

		try {
			/*[IF JAVA_SPEC_VERSION >= 20]*/
			longObjToMemSegmtRetFilter = lookup.bind(this, "longObjToMemSegmtRet", methodType(MemorySegment.class, Object.class)); //$NON-NLS-1$
			memSegmtOfPtrToLongArgFilter = lookup.bind(this, "memSegmtOfPtrToLongArg", methodType(long.class, MemorySegment.class)); //$NON-NLS-1$
			/*[ELSEIF JAVA_SPEC_VERSION >= 18]*/
			memAddrToLongArgFilter = lookup.bind(this, "memAddrToLongArg", methodType(long.class, Addressable.class)); //$NON-NLS-1$
			/*[ELSE] JAVA_SPEC_VERSION >= 18 */
			memAddrToLongArgFilter = lookup.bind(this, "memAddrToLongArg", methodType(long.class, MemoryAddress.class)); //$NON-NLS-1$
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

			memSegmtToLongArgFilter = lookup.bind(this, "memSegmtToLongArg", methodType(long.class, MemorySegment.class)); //$NON-NLS-1$
		} catch (ReflectiveOperationException e) {
			throw new InternalError(e);
		}

		generateAdapter();
	}

	/* Map the layouts of return type & argument types to the underlying prep_cif. */
	private void generateAdapter() {
		int argLayoutCount = argLayoutArray.length;
		String[] argLayoutStrs = new String[argLayoutCount];
		StringBuilder argLayoutStrsLine = new StringBuilder("(|"); //$NON-NLS-1$
		for (int argIndex = 0; argIndex < argLayoutCount; argIndex++) {
			MemoryLayout argLayout = argLayoutArray[argIndex];
			/* Prefix the size of layout to the layout string to be parsed in native. */
			argLayoutStrs[argIndex] = LayoutStrPreprocessor.getSimplifiedLayoutString(argLayout, true);
			argLayoutStrsLine.append(argLayoutStrs[argIndex]).append('|');
		}
		argLayoutStrsLine.append(')');

		/* Set the void layout string intended for the underlying native code
		 * as the corresponding layout doesn't exist in the Spec.
		 * Note: 'V' stands for the void type and 0 means zero byte.
		 */
		String retLayoutStr = "0V"; //$NON-NLS-1$
		if (realReturnLayout != null) {
			retLayoutStr = LayoutStrPreprocessor.getSimplifiedLayoutString(realReturnLayout, true);
		}

		synchronized(privateClassLock) {
			/* If a prep_cif for a given function descriptor exists, then the corresponding return & argument layouts
			 * were already set up for this prep_cif, in which case there is no need to check the layouts.
			 * If not the case, check at first whether the same return & argument layouts exist in the cache
			 * in case of duplicate memory allocation for the same layouts.
			 *
			 * Note: (Java <= 17)
			 * 1) C_LONG (Linux) and C_LONG_LONG(Windows/AIX 64bit) should be treated as the same layout in the cache.
			 * 2) the same layout kind with or without the layout name should be treated as the same layout.
			 * e.g.  C_INT without the layout name = b32[abi/kind=INT]
			 *  and  C_INT with the layout name = b32(int)[abi/kind=INT,layout/name=int]
			 * Note: (Java >= 18)
			 * the signature information are removed from the string of function descriptor since Java 18,
			 * e.g. (b32[abi/kind=INT],b32[abi/kind=INT])b32[abi/kind=INT] are replaced by ([8%b32, 8%b32])8%b32.
			 * So we have to unify the code in Java 17 & 18 to parse the layout with different solutions:
			 * 1) generate the layout string with CLinker.TypeKind in Java 17.
			 * 2) generate the layout string with MemoryLayout.carrier() (the type idenfied by the layout) in Java 18.
			 */
			/*[IF JAVA_SPEC_VERSION >= 20]*/
			int varArgIdx = LayoutStrPreprocessor.getVarArgIndex(funcDescriptor, linkerOpts);
			/*[ELSE] JAVA_SPEC_VERSION >= 20 */
			int varArgIdx = LayoutStrPreprocessor.getVarArgIndex(funcDescriptor);
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
			String argRetLayoutStrsLine = ((varArgIdx >= 0) ? varArgIdx : "") + argLayoutStrsLine.toString() + retLayoutStr;
			Integer argRetLayoutStrLineHash = Integer.valueOf(argRetLayoutStrsLine.hashCode());
			Integer argLayoutStrsLineHash = Integer.valueOf(argLayoutStrsLine.toString().hashCode());
			Long cifNativeThunk = cachedCifNativeThunkAddr.get(argRetLayoutStrLineHash);
			if (cifNativeThunk != null) {
				cifNativeThunkAddr = cifNativeThunk.longValue();
				argTypesAddr = cachedArgTypes.get(argLayoutStrsLineHash).longValue();
			} else {
				boolean newArgTypes = cachedArgTypes.containsKey(argLayoutStrsLineHash) ? false : true;
				if (!newArgTypes) {
					argTypesAddr = cachedArgTypes.get(argLayoutStrsLineHash).longValue();
				}

				/* Prepare the prep_cif for the native function specified by the arguments/return layouts. */
				initCifNativeThunkData(argLayoutStrs, retLayoutStr, newArgTypes, varArgIdx);

				/* Cache the address of prep_cif and argTypes after setting up via the out-of-line native code. */
				if (newArgTypes) {
					cachedArgTypes.put(argLayoutStrsLineHash, Long.valueOf(argTypesAddr));
				}
				cachedCifNativeThunkAddr.put(argRetLayoutStrLineHash, Long.valueOf(cifNativeThunkAddr));
			}
		}
	}

	/**
	 * The method is ultimately invoked by Linker on the specific platforms to generate the requested
	 * method handle to the underlying C function.
	 *
	 * @return a method handle bound to the native method
	 */
	public MethodHandle getBoundMethodHandle() {
		try {
			/*[IF JAVA_SPEC_VERSION >= 20]*/
			Class<?> downcallAddrClass = MemorySegment.class;
			/*[ELSEIF JAVA_SPEC_VERSION == 18]*/
			Class<?> downcallAddrClass = NativeSymbol.class;
			/*[ELSE] JAVA_SPEC_VERSION == 18 */
			Class<?> downcallAddrClass = Addressable.class;
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

			/*[IF JAVA_SPEC_VERSION >= 20]*/
			MethodType nativeMethodType = methodType(Object.class, downcallAddrClass, SegmentAllocator.class, MemorySegment.class, long[].class);
			/*[ELSEIF JAVA_SPEC_VERSION >= 17]*/
			MethodType nativeMethodType = methodType(Object.class, downcallAddrClass, SegmentAllocator.class, long[].class);
			/*[ELSE] JAVA_SPEC_VERSION >= 17 */
			MethodType nativeMethodType = methodType(Object.class, long[].class);
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

			MethodHandle boundHandle = lookup.bind(this, "runNativeMethod", nativeMethodType); //$NON-NLS-1$

			/* Replace the original handle with the specified types of the C function. */
			boundHandle = permuteMH(boundHandle, funcMethodType);
			return boundHandle;
		} catch (ReflectiveOperationException e) {
			throw new InternalError(e);
		}
	}

	/* Collect and convert the passed-in arguments to an Object array for the underlying native call. */
	private MethodHandle permuteMH(MethodHandle targetHandle, MethodType nativeMethodType) throws NullPointerException, WrongMethodTypeException {
		Class<?>[] argTypeClasses = nativeMethodType.parameterArray();
		int nativeArgCount = argTypeClasses.length;
		/*[IF JAVA_SPEC_VERSION >= 20]*/
		/* Skip the native function address, the segment allocator and the segment
		 * for the execution state to the native function's arguments.
		 */
		int argPosition = 3;
		/*[ELSEIF JAVA_SPEC_VERSION >= 17]*/
		/* Skip the native function address and the segment allocator to the native function's arguments. */
		int argPosition = 2;
		/*[ELSE] JAVA_SPEC_VERSION >= 17 */
		int argPosition = 0;
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
		MethodHandle resultHandle = targetHandle.asCollector(argPosition, long[].class, nativeArgCount);

		/* Convert the argument values to long via filterArguments() prior to the native call. */
		MethodHandle[] argFilters = new MethodHandle[nativeArgCount];
		for (int argIndex = 0; argIndex < nativeArgCount; argIndex++) {
			argFilters[argIndex] = getArgumentFilter(argTypeClasses[argIndex], argLayoutArray[argIndex]);
		}
		resultHandle = filterArguments(resultHandle, argPosition, argFilters);

		/* Convert the return value to the specified type via filterReturnValue() after the native call. */
		MethodHandle retFilter = getReturnValFilter(nativeMethodType.returnType());
		resultHandle = filterReturnValue(resultHandle, retFilter);

		/*[IF JAVA_SPEC_VERSION >= 20]*/
		/* Set a placeholder with a NULL segment if there is no request
		 * for the execution state from downcall in the linker options.
		 */
		if (!linkerOpts.hasCapturedCallState()) {
			resultHandle = insertArguments(resultHandle, 2, MemorySegment.NULL);
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
		return resultHandle;
	}

	/* Obtain the filter that converts the passed-in argument to long against its type. */
	private MethodHandle getArgumentFilter(Class<?> argTypeClass, MemoryLayout argLayout) {
		/* Set the filter to null in the case of long by default as there is no conversion for long. */
		MethodHandle filterMH = null;

		if (argTypeClass == boolean.class) {
			filterMH = booleanToLongArgFilter;
		} else if (argTypeClass == char.class) {
			filterMH = charToLongArgFilter;
		} else if (argTypeClass == byte.class) {
			filterMH = byteToLongArgFilter;
		} else if (argTypeClass == short.class) {
			filterMH = shortToLongArgFilter;
		} else if (argTypeClass == int.class) {
			filterMH = intToLongArgFilter;
		} else if (argTypeClass == float.class) {
			filterMH = floatToLongArgFilter;
		} else if (argTypeClass == double.class) {
			filterMH = doubleToLongArgFilter;
		} else
		/*[IF JAVA_SPEC_VERSION <= 19]*/
		if ((argTypeClass == MemoryAddress.class)
		/*[IF JAVA_SPEC_VERSION >= 18]*/
		|| (argTypeClass == Addressable.class)
		/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
		) {
			filterMH = memAddrToLongArgFilter;
		} else
		/*[ENDIF] JAVA_SPEC_VERSION <= 19 */
		if (argTypeClass == MemorySegment.class) {
			/*[IF JAVA_SPEC_VERSION >= 20]*/
			/* The address layout for pointer might come with different representations of ADDRESS. */
			if (argLayout instanceof OfAddress) {
				filterMH = memSegmtOfPtrToLongArgFilter;
			} else
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
			{
				filterMH = memSegmtToLongArgFilter;
			}
		}

		return filterMH;
	}

	/* The return value filter that converts the returned long value
	 * from the C function to the specified return type at Java level.
	 */
	private MethodHandle getReturnValFilter(Class<?> returnType) {
		MethodHandle filterMH = longObjToLongRetFilter;

		if (returnType == void.class) {
			filterMH = longObjToVoidRetFilter;
		} else if (returnType == boolean.class) {
			filterMH = longObjToBooleanRetFilter;
		} else if (returnType == char.class) {
			filterMH = longObjToCharRetFilter;
		} else if (returnType == byte.class) {
			filterMH = longObjToByteRetFilter;
		} else if (returnType == short.class) {
			filterMH = longObjToShortRetFilter;
		} else if (returnType == int.class) {
			filterMH = longObjToIntRetFilter;
		} else if (returnType == float.class) {
			filterMH = longObjToFloatRetFilter;
		} else if (returnType == double.class) {
			filterMH = longObjToDoubleRetFilter;
		} else
		/*[IF JAVA_SPEC_VERSION <= 19]*/
		if ((returnType == MemoryAddress.class)
		/*[IF JAVA_SPEC_VERSION >= 18]*/
		|| (returnType == Addressable.class)
		/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
		) {
			filterMH = longObjToMemAddrRetFilter;
		} else
		/*[ENDIF] JAVA_SPEC_VERSION <= 19 */
		if (returnType == MemorySegment.class) {
			/*[IF JAVA_SPEC_VERSION >= 20]*/
			/* A returned pointer is wrapped as a zero-sized memory segment given all
			 * MemoryAdress related classes are removed against the latest APIs as
			 * specified in JDK20+.
			 */
			if ((realReturnLayout != null) && (realReturnLayout instanceof ValueLayout)) {
				filterMH = longObjToMemSegmtRetFilter;
			} else
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
			{
				filterMH = objToMemSegmtRetFilter;
			}
		}

		return filterMH;
	}

	/*[IF JAVA_SPEC_VERSION >= 19]*/
	/* Set up the dependency from the sessions of memory related arguments to the specified session
	 * so as to keep these arguments' sessions alive till the specified session is closed.
	 */
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	private void SetDependency(SegmentScope session)
	/*[ELSE] JAVA_SPEC_VERSION >= 20 */
	private void SetDependency(MemorySession session)
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	{
		Objects.requireNonNull(session);
		/*[IF JAVA_SPEC_VERSION >= 20]*/
		for (SegmentScope memArgSession : memArgScopeSet)
		/*[ELSE] JAVA_SPEC_VERSION >= 20 */
		for (MemorySession memArgSession : memArgScopeSet)
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
		{
			/* keepAlive() is replaced with whileAlive(Runnable action) since JDK19 in which case
			 * the only way to invoke the native in downcall is to wrap it up in another thread
			 * (for the critical action) rather than the current thread owning the downcall handler,
			 * which is incorrect in our situation given the downcall must be executed by the owner
			 * thread according to the API Spec. So we have to directly implement the equivalent of
			 * keepAlive() in JDK19 to set up the state of arguments' session.
			 */
			if (memArgSession.isAlive()) {
				MemorySessionImpl memArgSessionImpl = (MemorySessionImpl)memArgSession;
				Thread owner = memArgSessionImpl.ownerThread();
				/* The check is intended for the confined session or
				 * the shared session(e.g. implicit/global session).
				 */
				if ((owner == Thread.currentThread()) || (owner == null)) {
					memArgSessionImpl.acquire0();
					((MemorySessionImpl)session).addCloseAction(memArgSessionImpl::release0);
				}
			}
		}
	}
	/*[ELSEIF JAVA_SPEC_VERSION == 18]*/
	/* Set up the dependency from the scopes of memory related arguments to the specified scope
	 * so as to keep these arguments' scopes alive till the specified scope is closed.
	 */
	private void SetDependency(ResourceScope scope) {
		Objects.requireNonNull(scope);
		for (ResourceScope memArgScope : memArgScopeSet) {
			if (memArgScope.isAlive()) {
				Thread owner = memArgScope.ownerThread();
				/* The check is intended for the confined scope or
				 * the shared scope(e.g. implicit/global scope).
				 */
				if ((owner == Thread.currentThread()) || (owner == null)) {
					scope.keepAlive(memArgScope); // keepAlive() is only used in JDK18
				}
			}
		}
	}
	/*[ELSEIF JAVA_SPEC_VERSION == 17]*/
	/* Occupy the scope by setting the scope's state in downcall which is similiar to keepAlive() in JDK18. */
	private void acquireScope() {
		for (ResourceScope memArgScope : memArgScopeSet) {
			if (memArgScope.isAlive()) {
				Thread owner = memArgScope.ownerThread();
				/* The check is intended for the confined scope or
				 * the shared scope(e.g. implicit/global scope).
				 */
				if ((owner == Thread.currentThread()) || (owner == null)) {
					Handle scopeHandle = memArgScope.acquire();
					scopeHandleMap.put(memArgScope, scopeHandle);
				}
			}
		}
	}

	/* Release the scope with the scope's handle in downcall */
	private void releaseScope() {
		for (ResourceScope memArgScope : memArgScopeSet) {
			if (memArgScope.isAlive()) {
				Thread owner = memArgScope.ownerThread();
				/* The check is intended for the confined scope or
				 * the shared scope(e.g. implicit/global scope).
				 */
				if ((owner == Thread.currentThread()) || (owner == null)) {
					Handle scopeHandle = scopeHandleMap.get(memArgScope);
					memArgScope.release(scopeHandle);
				}
			}
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

	/* The method (bound by the method handle to the native code) intends to invoke the C function via the inlined code. */
	/*[IF JAVA_SPEC_VERSION >= 17]*/
	/*[IF JAVA_SPEC_VERSION >= 20]*/
	Object runNativeMethod(MemorySegment downcallAddr, SegmentAllocator segmtAllocator, MemorySegment stateSegmt, long[] args) throws IllegalArgumentException, IllegalStateException
	/*[ELSEIF JAVA_SPEC_VERSION == 18]*/
	Object runNativeMethod(NativeSymbol downcallAddr, SegmentAllocator segmtAllocator, long[] args) throws IllegalArgumentException, IllegalStateException
	/*[ELSE] JAVA_SPEC_VERSION == 18 */
	Object runNativeMethod(Addressable downcallAddr, SegmentAllocator segmtAllocator, long[] args) throws IllegalArgumentException, IllegalStateException
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	/*[ELSE] JAVA_SPEC_VERSION >= 17 */
	Object runNativeMethod(long[] args)
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	{
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		/*[IF JAVA_SPEC_VERSION >= 20]*/
		if (downcallAddr == MemorySegment.NULL)
		/*[ELSE] JAVA_SPEC_VERSION >= 20 */
		if (downcallAddr.address() == MemoryAddress.NULL)
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
		{
			throw new IllegalArgumentException("A non-null memory address is expected for downcall"); //$NON-NLS-1$
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

		/*[IF JAVA_SPEC_VERSION >= 20]*/
		/* The scope of the downcall function address needs to be validated in OpenJ9
		 * since the related code and APIs were adjusted in JDK20 in which case the
		 * validity check on the address's scope in OpenJDK was entirely removed.
		 */
		validateMemScope(downcallAddr.scope());
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

		long retMemAddr = 0;
		MemorySegment retStruSegmt = null;
		/* Validate the returned struct type with the corresponding layout given
		 * the MemorySegment class is intended for both struct and pointer since
		 * JDK20+.
		 */
		if ((realReturnLayout != null) && (realReturnLayout instanceof GroupLayout)) {
			/*[IF JAVA_SPEC_VERSION >= 17]*/
			/* The segment allocator (introduced since Java 17 to replace NativeScope in Java 16) is confined
			 * by the memory session(Java19)/resource scope(Java17/18) defined in user applications in which
			 * case the allocated memory will be released automatically once the session/scope is closed.
			 */
			retStruSegmt = segmtAllocator.allocate(realReturnLayout);
			/*[ELSE] JAVA_SPEC_VERSION >= 17 */
			/* The memory segment will be released explicitly by users via close() in java code in Java 16. */
			retStruSegmt = MemorySegment.allocateNative(realReturnLayout);
			/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
			if (retStruSegmt == null) {
				throw new OutOfMemoryError("Failed to allocate native memory for the returned memory segment"); //$NON-NLS-1$
			}
			/*[IF JAVA_SPEC_VERSION >= 20]*/
			retMemAddr = retStruSegmt.address();
			/*[ELSE] JAVA_SPEC_VERSION >= 20 */
			retMemAddr = retStruSegmt.address().toRawLongValue();
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
		}

		long returnVal = 0;
		/* The session/scope associated with memory specific arguments must be kept alive in downcall since JDK17. */
		if (!memArgScopeSet.isEmpty()) {
			/*[IF JAVA_SPEC_VERSION > 17]*/
			/*[IF JAVA_SPEC_VERSION >= 20]*/
			try (Arena arena = Arena.openConfined())
			/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
			try (MemorySession nativeSession = MemorySession.openConfined())
			/*[ELSEIF JAVA_SPEC_VERSION == 18]*/
			try (ResourceScope nativeScope = ResourceScope.newConfinedScope())
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
			{
				/*[IF JAVA_SPEC_VERSION >= 20]*/
				SetDependency(arena.scope());
				/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
				SetDependency(nativeSession);
				/*[ELSEIF JAVA_SPEC_VERSION == 18]*/
				SetDependency(nativeScope);
				/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

				/*[IF JAVA_SPEC_VERSION >= 20]*/
				returnVal = invokeNative(stateSegmt.address(), retMemAddr, downcallAddr.address(), cifNativeThunkAddr, args);
				/*[ELSE] JAVA_SPEC_VERSION >= 20 */
				returnVal = invokeNative(retMemAddr, downcallAddr.address().toRawLongValue(), cifNativeThunkAddr, args);
				/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
			}
			/*[ELSE] JAVA_SPEC_VERSION > 17 */
			acquireScope();
			returnVal = invokeNative(retMemAddr, downcallAddr.address().toRawLongValue(), cifNativeThunkAddr, args);
			releaseScope();
			/*[ENDIF] JAVA_SPEC_VERSION > 17 */
		}
		else
		{
			/*[IF JAVA_SPEC_VERSION >= 20]*/
			returnVal = invokeNative(stateSegmt.address(), retMemAddr, downcallAddr.address(), cifNativeThunkAddr, args);
			/*[ELSEIF JAVA_SPEC_VERSION >= 17]*/
			returnVal = invokeNative(retMemAddr, downcallAddr.address().toRawLongValue(), cifNativeThunkAddr, args);
			/*[ELSE] JAVA_SPEC_VERSION >= 17 */
			/* The given function address never changes since the initialization of the downcall handler in JDK16. */
			returnVal = invokeNative(retMemAddr, functionAddr.address().toRawLongValue(), cifNativeThunkAddr, args);
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
		}

		/* This struct specific MemorySegment object returns to the current thread in the multithreading environment,
		 * in which case the native invocations from threads end up with distinct returned structs.
		 */
		return (retStruSegmt != null) ? retStruSegmt : Long.valueOf(returnVal);
	}
}
