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

import java.util.HashMap;
import java.util.List;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import static java.lang.invoke.MethodHandles.*;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import static java.lang.invoke.MethodType.methodType;
import java.lang.invoke.WrongMethodTypeException;

/*[IF JAVA_SPEC_VERSION >= 19]*/
import java.lang.foreign.Addressable;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemoryAddress;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.MemorySession;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.ValueLayout;
/*[ELSE] JAVA_SPEC_VERSION >= 19 */
import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemorySegment;
/*[IF JAVA_SPEC_VERSION == 18]*/
import jdk.incubator.foreign.NativeSymbol;
/*[ENDIF] JAVA_SPEC_VERSION == 18 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
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
	/*[IF JAVA_SPEC_VERSION == 18]*/
	private NativeSymbol functionAddr;
	/*[ELSE] JAVA_SPEC_VERSION == 18 */
	private Addressable functionAddr;
	/*[ENDIF] JAVA_SPEC_VERSION == 18 */
	/*[IF JAVA_SPEC_VERSION >= 18]*/
	private static final Class<?> addrClass = Addressable.class;
	/*[ELSE] JAVA_SPEC_VERSION >= 18 */
	private static final Class<?> addrClass = MemoryAddress.class;
	/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
	private long cifNativeThunkAddr;
	private long argTypesAddr;
	private MemoryLayout[] argLayoutArray;
	private MemoryLayout realReturnLayout;
	private MemorySegment returnStructSegmt;

	static final Lookup lookup = MethodHandles.lookup();

	/* The prep_cif and the corresponding argument types are cached & shared in multiple downcalls/threads */
	private static final HashMap<Integer, Long> cachedCifNativeThunkAddr = new HashMap<>();
	private static final HashMap<Integer, Long> cachedArgTypes = new HashMap<>();

	/* Argument filters that convert the primitive types/MemoryAddress/MemorySegment to long */
	private static final MethodHandle booleanToLongArgFilter;
	private static final MethodHandle charToLongArgFilter;
	private static final MethodHandle byteToLongArgFilter;
	private static final MethodHandle shortToLongArgFilter;
	private static final MethodHandle intToLongArgFilter;
	private static final MethodHandle floatToLongArgFilter;
	private static final MethodHandle doubleToLongArgFilter;
	private static final MethodHandle memAddrToLongArgFilter;
	private static final MethodHandle memSegmtToLongArgFilter;

	/* Return value filters that convert the Long object to the primitive types/MemoryAddress/MemorySegment */
	private static final MethodHandle longObjToVoidRetFilter;
	private static final MethodHandle longObjToBooleanRetFilter;
	private static final MethodHandle longObjToCharRetFilter;
	private static final MethodHandle longObjToByteRetFilter;
	private static final MethodHandle longObjToShortRetFilter;
	private static final MethodHandle longObjToIntRetFilter;
	private static final MethodHandle longObjToLongRetFilter;
	private static final MethodHandle longObjToFloatRetFilter;
	private static final MethodHandle longObjToDoubleRetFilter;
	private static final MethodHandle longObjToMemAddrRetFilter;
	private MethodHandle longObjToMemSegmtRetFilter;

	private static synchronized native void resolveRequiredFields();
	private native void initCifNativeThunkData(String[] argLayouts, String retLayout, boolean newArgTypes);
	private native long invokeNative(long returnStructMemAddr, long functionAddress, long calloutThunk, long[] argValues);

	private static final class PrivateClassLock {
		PrivateClassLock() {}
	}
	private static final Object privateClassLock = new PrivateClassLock();

	static {
		try {
			/* Set up the argument filters for the primitive types and MemoryAddress */
			booleanToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "booleanToLongArg", methodType(long.class, boolean.class)); //$NON-NLS-1$
			charToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "charToLongArg", methodType(long.class, char.class)); //$NON-NLS-1$
			byteToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "byteToLongArg", methodType(long.class, byte.class)); //$NON-NLS-1$
			shortToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "shortToLongArg", methodType(long.class, short.class)); //$NON-NLS-1$
			intToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "intToLongArg", methodType(long.class, int.class)); //$NON-NLS-1$
			floatToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "floatToLongArg", methodType(long.class, float.class)); //$NON-NLS-1$
			doubleToLongArgFilter = lookup.findStatic(Double.class, "doubleToLongBits", methodType(long.class, double.class)); //$NON-NLS-1$
			memAddrToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "memAddrToLongArg", methodType(long.class, addrClass)); //$NON-NLS-1$
			memSegmtToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "memSegmtToLongArg", methodType(long.class, MemorySegment.class)); //$NON-NLS-1$

			/* Set up the return value filters for the primitive types and MemoryAddress */
			longObjToVoidRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToVoidRet", methodType(void.class, Object.class)); //$NON-NLS-1$
			longObjToBooleanRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToBooleanRet", methodType(boolean.class, Object.class)); //$NON-NLS-1$
			longObjToCharRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToCharRet", methodType(char.class, Object.class)); //$NON-NLS-1$
			longObjToByteRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToByteRet", methodType(byte.class, Object.class)); //$NON-NLS-1$
			longObjToShortRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToShortRet", methodType(short.class, Object.class)); //$NON-NLS-1$
			longObjToIntRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToIntRet", methodType(int.class, Object.class)); //$NON-NLS-1$
			longObjToLongRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToLongRet", methodType(long.class, Object.class)); //$NON-NLS-1$
			longObjToFloatRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToFloatRet", methodType(float.class, Object.class)); //$NON-NLS-1$
			longObjToDoubleRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToDoubleRet", methodType(double.class, Object.class)); //$NON-NLS-1$
			longObjToMemAddrRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToMemAddrRet", methodType(MemoryAddress.class, Object.class)); //$NON-NLS-1$
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

	/* Intended for booleanToLongArgFilter that converts boolean to long */
	private static final long booleanToLongArg(boolean argValue) {
		return argValue ? 1 : 0;
	}

	/* Intended for charToLongArgFilter that converts char to long */
	private static final long charToLongArg(char argValue) {
		return argValue;
	}

	/* Intended for byteToLongArgFilter that converts byte to long */
	private static final long byteToLongArg(byte argValue) {
		return argValue;
	}

	/* Intended for shortToLongArgFilter that converts short to long given
	 * short won't be casted to long automatically in filterArguments()
	 */
	private static final long shortToLongArg(short argValue) {
		return argValue;
	}

	/* Intended for intToLongArgFilter that converts int to long given
	 * int won't be casted to long automatically in filterArguments()
	 */
	private static final long intToLongArg(int argValue) {
		return argValue;
	}

	/* Intended for floatToLongArgFilter that converts the int value from Float.floatToIntBits()
	 * to long given int won't be casted to long automatically in filterArguments()
	 */
	private static final long floatToLongArg(float argValue) {
		return Float.floatToIntBits(argValue);
	}

	/* Intended for memAddrToLongArgFilter that converts the memory address to long.
	 * Note: the passed-in argument can be an instance of MemoryAddress, MemorySegment
	 * or VaList which extends Addressable in OpenJDK since Java 18 featured with
	 * JEP419 (Second Incubator).
	 */
	/*[IF JAVA_SPEC_VERSION >= 18]*/
	private static final long memAddrToLongArg(Addressable argValue) throws IllegalStateException {
		/* Only check MemorySegment given MemoryAddress.scope() doesn't exist in JEP419 */
		if (argValue instanceof MemorySegment) {
			/*[IF JAVA_SPEC_VERSION >= 19]*/
			if (!((MemorySegment)argValue).session().isAlive()) {
				throw new IllegalStateException("The session of the memory segment is closed");  //$NON-NLS-1$
			}
			/*[ELSE] JAVA_SPEC_VERSION >= 19 */
			if (!((MemorySegment)argValue).scope().isAlive()) {
				throw new IllegalStateException("The scope of the memory segment is closed");  //$NON-NLS-1$
			}
			/*[ENDIF] JAVA_SPEC_VERSION >= 19 */
		}
		return argValue.address().toRawLongValue();
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 18 */
	/*[IF JAVA_SPEC_VERSION == 17]*/
	private static final long memAddrToLongArg(MemoryAddress argValue) throws IllegalStateException {
		if (!argValue.scope().isAlive()) {
			throw new IllegalStateException("The scope of the memory address is closed");  //$NON-NLS-1$
		}
		return argValue.address().toRawLongValue();
	}
	/*[ELSE] JAVA_SPEC_VERSION == 17 */
	private static final long memAddrToLongArg(MemoryAddress argValue) {
		return argValue.address().toRawLongValue();
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 17 */
	/*[ENDIF] JAVA_SPEC_VERSION >= 18 */

	/* Intended for memSegmtToLongArgFilter that converts the memory segment to long */
	private static final long memSegmtToLongArg(MemorySegment argValue) throws IllegalStateException {
		/*[IF JAVA_SPEC_VERSION >= 19]*/
		if (!argValue.session().isAlive()) {
			throw new IllegalStateException("The session of the memory segment is closed");  //$NON-NLS-1$
		}
		/*[ELSE] JAVA_SPEC_VERSION >= 19 */
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		if (!argValue.scope().isAlive())
		/*[ELSE] JAVA_SPEC_VERSION >= 17 */
		if (!argValue.isAlive())
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
		{
			throw new IllegalStateException("The scope of the memory segment is closed");  //$NON-NLS-1$
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 19 */
		return argValue.address().toRawLongValue();
	}

	/* Intended for longObjToVoidRetFilter that converts the Long object to void */
	private static final void longObjToVoidRet(Object retValue) {
		return;
	}

	/* Intended for longObjToBooleanRetFilter that converts the Long object to boolean */
	private static final boolean longObjToBooleanRet(Object retValue) {
		boolean resultValue = ((Long)retValue).longValue() != 0;
		return resultValue;
	}

	/* Intended for longObjToCharRetFilter that converts the Long object to char */
	private static final char longObjToCharRet(Object retValue) {
		return (char)(((Long)retValue).shortValue());
	}

	/* Intended for longObjToByteRetFilter that converts the Long object to byte */
	private static final byte longObjToByteRet(Object retValue) {
		return ((Long)retValue).byteValue();
	}

	/* Intended for longObjToShortRetFilter that converts the Long object to short */
	private static final short longObjToShortRet(Object retValue) {
		return ((Long)retValue).shortValue();
	}

	/* Intended for longObjToIntRetFilter that converts the Long object to int */
	private static final int longObjToIntRet(Object retValue) {
		return ((Long)retValue).intValue();
	}

	/* Intended for longObjToLongRetFilter that converts the Long object to long */
	private static final long longObjToLongRet(Object retValue) {
		return ((Long)retValue).longValue();
	}

	/* Intended for longObjToFloatRetFilter that converts the Long object to float with Float.floatToIntBits() */
	private static final float longObjToFloatRet(Object retValue) {
		int tmpValue = ((Long)retValue).intValue();
		return Float.intBitsToFloat(tmpValue);
	}

	/* Intended for longObjToFloatRetFilter that converts the Long object to double with Double.longBitsToDouble() */
	private static final double longObjToDoubleRet(Object retValue) {
		long tmpValue = ((Long)retValue).longValue();
		return Double.longBitsToDouble(tmpValue);
	}

	/* Intended for longObjToMemAddrRetFilter that converts the Long object to the memory address */
	private static final MemoryAddress longObjToMemAddrRet(Object retValue) {
		long tmpValue = ((Long)retValue).longValue();
		return MemoryAddress.ofLong(tmpValue);
	}

	/* Intended for longObjToMemSegmtRetFilter that converts the Long object to the memory address.
	 * Note: the returned memory address is exactly the address of the memory previously allocated
	 * for the specified struct layout on return.
	 */
	private final MemorySegment longObjToMemSegmtRet(Object retValue) {
		return returnStructSegmt;
	}

	/*[IF JAVA_SPEC_VERSION >= 17]*/
	InternalDowncallHandler(MethodType functionMethodType, FunctionDescriptor functionDescriptor)
	/*[ELSE] JAVA_SPEC_VERSION >= 17 */
	InternalDowncallHandler(Addressable downcallAddr, MethodType functionMethodType, FunctionDescriptor functionDescriptor)
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
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
		/*[ENDIF] JAVA_SPEC_VERSION <= 17 */

		/*[IF JAVA_SPEC_VERSION >= 17]*/
		/* The native function address has been removed from the parameter lists of downcallHandle() APIs
		 * so as to being passed in as the first argument when invoking the returned downcall handle
		 * or bound as the first argument via MethodHandles.insertArguments() beforehand in OpenJDK.
		 */
		functionAddr = null;
		/*[ELSE] JAVA_SPEC_VERSION >= 17 */
		/* As explained in the Spec of LibraryLookup in JDK16, the downcall must hold a strong reference to
		 * the native library symbol to prevent the underlying native library from being unloaded
		 * during the native calls.
		 *
		 * Note: the passed-in addressable parameter can be either LibraryLookup.Symbol or MemoryAddress.
		 */
		functionAddr = downcallAddr;
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

		funcMethodType = functionMethodType;
		funcDescriptor = functionDescriptor;

		cifNativeThunkAddr = 0;
		argTypesAddr = 0;
		returnStructSegmt = null;
		longObjToMemSegmtRetFilter = null;
		/* Create the filter for the returned memory segment (non-static) at runtime */
		if (funcMethodType.returnType() == MemorySegment.class) {
			try {
				longObjToMemSegmtRetFilter = lookup.bind(this, "longObjToMemSegmtRet", methodType(MemorySegment.class, Object.class)); //$NON-NLS-1$
			} catch (ReflectiveOperationException e) {
				throw new InternalError(e);
			}
		}
		generateAdapter();
	}

	/* Map the layouts of return type & argument types to the underlying prep_cif */
	private void generateAdapter() {
		int argLayoutCount = argLayoutArray.length;
		String[] argLayoutStrs = new String[argLayoutCount];
		StringBuilder argLayoutStrsLine = new StringBuilder("(|"); //$NON-NLS-1$
		for (int argIndex = 0; argIndex < argLayoutCount; argIndex++) {
			MemoryLayout argLayout = argLayoutArray[argIndex];
			/* Prefix the size of layout to the layout string to be parsed in native */
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
			String argRetLayoutStrsLine = argLayoutStrsLine.toString() + retLayoutStr;
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

				/* Prepare the prep_cif for the native function specified by the arguments/return layouts */
				initCifNativeThunkData(argLayoutStrs, retLayoutStr, newArgTypes);

				/* Cache the address of prep_cif and argTypes after setting up via the out-of-line native code */
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
	 * @param downcallAddr The downcall symbol
	 * @param functionMethodType The MethodType of the specified native function
	 * @param funcDesc The function descriptor of the specified native function
	 * @return a method handle bound to the native method
	 */
	/*[IF JAVA_SPEC_VERSION >= 17]*/
	public static MethodHandle getBoundMethodHandle(MethodType functionMethodType, FunctionDescriptor funcDesc)
	/*[ELSE] JAVA_SPEC_VERSION >= 17 */
	public static MethodHandle getBoundMethodHandle(Addressable downcallAddr, MethodType functionMethodType, FunctionDescriptor funcDesc)
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	{
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		InternalDowncallHandler nativeInvoker = new InternalDowncallHandler(functionMethodType, funcDesc);
		/*[ELSE] JAVA_SPEC_VERSION >= 17 */
		InternalDowncallHandler nativeInvoker = new InternalDowncallHandler(downcallAddr, functionMethodType, funcDesc);
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
		try {
			/*[IF JAVA_SPEC_VERSION >= 17]*/
			/*[IF JAVA_SPEC_VERSION == 18]*/
			MethodType nativeMethodType = methodType(Object.class, NativeSymbol.class, SegmentAllocator.class, long[].class);
			/*[ELSE] JAVA_SPEC_VERSION == 18 */
			MethodType nativeMethodType = methodType(Object.class, Addressable.class, SegmentAllocator.class, long[].class);
			/*[ENDIF] JAVA_SPEC_VERSION == 18 */
			/*[ELSE] JAVA_SPEC_VERSION >= 17 */
			MethodType nativeMethodType = methodType(Object.class, long[].class);
			/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

			MethodHandle boundHandle = lookup.bind(nativeInvoker, "runNativeMethod", nativeMethodType); //$NON-NLS-1$

			/* Replace the original handle with the specified types of the C function */
			boundHandle = nativeInvoker.permuteMH(boundHandle, functionMethodType);
			return boundHandle;
		} catch (ReflectiveOperationException e) {
			throw new InternalError(e);
		}
	}

	/* Collect and convert the passed-in arguments to an Object array for the underlying native call */
	private MethodHandle permuteMH(MethodHandle targetHandle, MethodType nativeMethodType) throws NullPointerException, WrongMethodTypeException {
		Class<?>[] argTypeClasses = nativeMethodType.parameterArray();
		int nativeArgCount = argTypeClasses.length;
		int argPosition = 0;
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		/* Skip the native function address and the segment allocator to the native function's arguments */
		argPosition = 2;
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
		MethodHandle resultHandle = targetHandle.asCollector(argPosition, long[].class, nativeArgCount);

		/* Convert the argument values to long via filterArguments() prior to the native call */
		MethodHandle[] argFilters = new MethodHandle[nativeArgCount];
		for (int argIndex = 0; argIndex < nativeArgCount; argIndex++) {
			argFilters[argIndex] = getArgumentFilter(argTypeClasses[argIndex]);
		}
		resultHandle = filterArguments(resultHandle, argPosition, argFilters);

		/* Convert the return value to the specified type via filterReturnValue() after the native call */
		MethodHandle retFilter = getReturnValFilter(nativeMethodType.returnType());
		resultHandle = filterReturnValue(resultHandle, retFilter);
		return resultHandle;
	}

	/* Obtain the filter that converts the passed-in argument to long against its type */
	private static MethodHandle getArgumentFilter(Class<?> argTypeClass) {
		/* Set the filter to null in the case of long by default as there is no conversion for long */
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
		} else if ((argTypeClass == MemoryAddress.class)
		/*[IF JAVA_SPEC_VERSION >= 18]*/
		|| (argTypeClass == Addressable.class)
		/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
		) {
			filterMH = memAddrToLongArgFilter;
		} else if (argTypeClass == MemorySegment.class) {
			filterMH = memSegmtToLongArgFilter;
		}

		return filterMH;
	}

	/* The return value filter that converts the returned long value
	 * from the C function to the specified return type at Java level
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
		} else if ((returnType == MemoryAddress.class)
		/*[IF JAVA_SPEC_VERSION >= 18]*/
		|| (returnType == Addressable.class)
		/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
		) {
			filterMH = longObjToMemAddrRetFilter;
		} else if (returnType == MemorySegment.class) {
			filterMH = longObjToMemSegmtRetFilter;
		}

		return filterMH;
	}

	/* The method (bound by the method handle to the native code) intends to invoke the C function via the inlined code */
	/*[IF JAVA_SPEC_VERSION >= 17]*/
	/*[IF JAVA_SPEC_VERSION == 18]*/
	Object runNativeMethod(NativeSymbol downcallAddr, SegmentAllocator segmtAllocator, long[] args) throws IllegalArgumentException
	/*[ELSE] JAVA_SPEC_VERSION == 18 */
	Object runNativeMethod(Addressable downcallAddr, SegmentAllocator segmtAllocator, long[] args) throws IllegalArgumentException
	/*[ENDIF] JAVA_SPEC_VERSION == 18 */
	/*[ELSE] JAVA_SPEC_VERSION >= 17 */
	Object runNativeMethod(long[] args)
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	{
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		if (downcallAddr.address() == MemoryAddress.NULL) {
			throw new IllegalArgumentException("A non-null memory address is expected for downcall"); //$NON-NLS-1$
		}
		/* Hold a strong reference in the downcall to the native library symbol to
		 * prevent the underlying native library from being unloaded.
		 */
		functionAddr = downcallAddr;
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

		if (funcMethodType.returnType() == MemorySegment.class) {
			/*[IF JAVA_SPEC_VERSION >= 17]*/
			/* The segment allocator (introduced since Java 17 to replace NativeScope in Java 16) is confined
			 * by the memory session(Java19)/resource scope(Java17/18) defined in user applications in which
			 * case the allocated memory will be released automatically once the session/scope is closed.
			 */
			returnStructSegmt = segmtAllocator.allocate(realReturnLayout);
			/*[ELSE] JAVA_SPEC_VERSION >= 17 */
			/* The memory segment will be released explicitly by users via close() in java code in Java 16 */
			returnStructSegmt = MemorySegment.allocateNative(realReturnLayout);
			/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
			if (returnStructSegmt == null) {
				throw new OutOfMemoryError("Failed to allocate native memory for the returned memory segment"); //$NON-NLS-1$
			}
		}
		long retMemAddr = (returnStructSegmt == null) ? 0 : returnStructSegmt.address().toRawLongValue();
		long returnVal = invokeNative(retMemAddr, functionAddr.address().toRawLongValue(), cifNativeThunkAddr, args);
		return Long.valueOf(returnVal);
	}
}
