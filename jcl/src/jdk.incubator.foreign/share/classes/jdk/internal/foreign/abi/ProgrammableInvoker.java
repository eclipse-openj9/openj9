/*[INCLUDE-IF JAVA_SPEC_VERSION >= 16]*/
/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
package jdk.internal.foreign.abi;

import java.util.Optional;
import java.util.List;
import java.util.HashMap;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import static java.lang.invoke.MethodHandles.*;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import static java.lang.invoke.MethodType.methodType;
import java.lang.invoke.WrongMethodTypeException;

import jdk.incubator.foreign.Addressable;
/*[IF JAVA_SPEC_VERSION <= 17]*/
import jdk.incubator.foreign.CLinker.TypeKind;
import static jdk.incubator.foreign.CLinker.TypeKind.*;
/*[ENDIF] JAVA_SPEC_VERSION <= 17 */
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemorySegment;
/*[IF JAVA_SPEC_VERSION >= 18]*/
import jdk.incubator.foreign.NativeSymbol;
/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
/*[IF JAVA_SPEC_VERSION >= 17]*/
import jdk.incubator.foreign.SegmentAllocator;
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
import jdk.incubator.foreign.ValueLayout;

/**
 * The counterpart in OpenJDK is replaced with this class that wrap up a method handle
 * enabling the native code to the ffi_call via the libffi interface at runtime.
 */
public class ProgrammableInvoker {

	private final MethodType funcMethodType;
	private final FunctionDescriptor funcDescriptor;
	/*[IF JAVA_SPEC_VERSION >= 18]*/
	private NativeSymbol functionAddr;
	private static final Class<?> addrClass = Addressable.class;
	/*[ELSE] JAVA_SPEC_VERSION >= 18 */
	private Addressable functionAddr;
	private static final Class<?> addrClass = MemoryAddress.class;
	/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
	private long cifNativeThunkAddr;
	private long argTypesAddr;
	private List<Class<?>> argTypes;
	private List<MemoryLayout> argLayouts;
	private MemoryLayout[] argLayoutArray;
	private MemoryLayout realReturnLayout;

	private static final Lookup lookup = MethodHandles.lookup();

	/* The prep_cif and the corresponding argument types are cached & shared in multiple downcalls/threads */
	private static final HashMap<Integer, Long> cachedCifNativeThunkAddr = new HashMap<>();
	private static final HashMap<Integer, Long> cachedArgTypes = new HashMap<>();

	/* Argument filters that convert the primitive types or MemoryAddress to long */
	private static final MethodHandle booleanToLongArgFilter;
	private static final MethodHandle charToLongArgFilter;
	private static final MethodHandle byteToLongArgFilter;
	private static final MethodHandle shortToLongArgFilter;
	private static final MethodHandle intToLongArgFilter;
	private static final MethodHandle floatToLongArgFilter;
	private static final MethodHandle doubleToLongArgFilter;
	private static final MethodHandle memAddrToLongArgFilter;

	/* Return value filters that convert the Long object to the primitive types or MemoryAddress */
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

	private static synchronized native void resolveRequiredFields();
	private native void initCifNativeThunkData(String[] argLayouts, String retLayout, boolean newArgTypes);
	private native long invokeNative(long functionAddress, long calloutThunk, long[] argValues);

	private static final class PrivateClassLock {
		PrivateClassLock() {}
	}
	private static final Object privateClassLock = new PrivateClassLock();

	static {
		try {
			/* Set up the argument filters for the primitive types and MemoryAddress */
			booleanToLongArgFilter = lookup.findStatic(ProgrammableInvoker.class, "booleanToLongArg", methodType(long.class, boolean.class)); //$NON-NLS-1$
			charToLongArgFilter = lookup.findStatic(ProgrammableInvoker.class, "charToLongArg", methodType(long.class, char.class)); //$NON-NLS-1$
			byteToLongArgFilter = lookup.findStatic(ProgrammableInvoker.class, "byteToLongArg", methodType(long.class, byte.class)); //$NON-NLS-1$
			shortToLongArgFilter = lookup.findStatic(ProgrammableInvoker.class, "shortToLongArg", methodType(long.class, short.class)); //$NON-NLS-1$
			intToLongArgFilter = lookup.findStatic(ProgrammableInvoker.class, "intToLongArg", methodType(long.class, int.class)); //$NON-NLS-1$
			floatToLongArgFilter = lookup.findStatic(ProgrammableInvoker.class, "floatToLongArg", methodType(long.class, float.class)); //$NON-NLS-1$
			doubleToLongArgFilter = lookup.findStatic(Double.class, "doubleToLongBits", methodType(long.class, double.class)); //$NON-NLS-1$
			memAddrToLongArgFilter = lookup.findStatic(ProgrammableInvoker.class, "memAddrToLongArg", methodType(long.class, addrClass)); //$NON-NLS-1$

			/* Set up the return value filters for the primitive types and MemoryAddress */
			longObjToVoidRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToVoidRet", methodType(void.class, Object.class)); //$NON-NLS-1$
			longObjToBooleanRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToBooleanRet", methodType(boolean.class, Object.class)); //$NON-NLS-1$
			longObjToCharRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToCharRet", methodType(char.class, Object.class)); //$NON-NLS-1$
			longObjToByteRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToByteRet", methodType(byte.class, Object.class)); //$NON-NLS-1$
			longObjToShortRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToShortRet", methodType(short.class, Object.class)); //$NON-NLS-1$
			longObjToIntRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToIntRet", methodType(int.class, Object.class)); //$NON-NLS-1$
			longObjToLongRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToLongRet", methodType(long.class, Object.class)); //$NON-NLS-1$
			longObjToFloatRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToFloatRet", methodType(float.class, Object.class)); //$NON-NLS-1$
			longObjToDoubleRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToDoubleRet", methodType(double.class, Object.class)); //$NON-NLS-1$
			longObjToMemAddrRetFilter = lookup.findStatic(ProgrammableInvoker.class, "longObjToMemAddrRet", methodType(MemoryAddress.class, Object.class)); //$NON-NLS-1$
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
	 * or VaList which extends Addressable in OpenJDK since Java 18.
	 */
	/*[IF JAVA_SPEC_VERSION >= 18]*/
	private static final long memAddrToLongArg(Addressable argValue)
	/*[ELSE] JAVA_SPEC_VERSION >= 18 */
	private static final long memAddrToLongArg(MemoryAddress argValue)
	/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
	{
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

	/*[IF JAVA_SPEC_VERSION >= 17]*/
	ProgrammableInvoker(MethodType functionMethodType, FunctionDescriptor functionDescriptor)
	/*[ELSE] JAVA_SPEC_VERSION >= 17 */
	ProgrammableInvoker(Addressable downcallAddr, MethodType functionMethodType, FunctionDescriptor functionDescriptor)
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	{
		argTypes = functionMethodType.parameterList();
		Optional<MemoryLayout> returnLayout = functionDescriptor.returnLayout();
		realReturnLayout = returnLayout.orElse(null); // set to null for void
		argLayouts = functionDescriptor.argumentLayouts();
		argLayoutArray = argLayouts.toArray(new MemoryLayout[argLayouts.size()]);

		/*[IF JAVA_SPEC_VERSION <= 17]*/
		/* The layout check against the method type is still required for Java 16 & 17 in that
		 * both the function descriptor and the method type are passed in as arguments by users.
		 * Note: skip the validity check on function descriptor in Java 18 as it is done before
		 * initializing ProgrammableInvoker in OpenJDK. Meanwhile, the method type is directly
		 * deduced from the function descriptor itself, in which case there is no need to
		 * check the layout against the method type.
		 */
		checkIfValidLayoutAndType(functionMethodType, functionDescriptor);
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
		generateAdapter();
	}

	/* Map the layouts of return type & argument types to the underlying prep_cif */
	private void generateAdapter() {
		/* Set the void layout string intended for the underlying native code as the corresponding layout doesn't exist in the Spec */
		String retLayoutStr = (realReturnLayout == null) ? "b0[abi/kind=VOID]" : convertToNativeTypeString(realReturnLayout); //$NON-NLS-1$

		int argLayoutCount = argLayoutArray.length;
		String[] argLayoutStrs = new String[argLayoutCount];
		for (int argIndex = 0; argIndex < argLayoutCount; argIndex++) {
			MemoryLayout argLayout = argLayoutArray[argIndex];
			argLayoutStrs[argIndex] = convertToNativeTypeString(argLayout);
		}

		synchronized(privateClassLock) {
			/* If a prep_cif for a given function descriptor exists, then the corresponding return & argument layouts
			 * were already set up for this prep_cif, in which case there is no need to check the layouts.
			 * If not the case, check at first whether the same return & argument layouts exist in the cache
			 * in case of duplicate memory allocation for the same layouts.
			 * Note: the signature information are removed from the string of function descriptor since Java 18,
			 * e.g. (long,long)long (method type) corresponds to ([8%b64, 8%b64])8%b64 (function descriptor).
			 * So we have to rely on the method type (deduced from the function descriptor in OpenJDK)
			 * to differentiate the functions' signature (Only for primitives).
			 */
			int funcSigHash = funcMethodType.hashCode();
			int argTypesHash = argTypes.hashCode();
			Long cifNativeThunk = cachedCifNativeThunkAddr.get(funcSigHash);
			if (cifNativeThunk != null) {
				cifNativeThunkAddr = cifNativeThunk.longValue();
				argTypesAddr = cachedArgTypes.get(argTypesHash).longValue();
			} else {
				boolean newArgTypes = cachedArgTypes.containsKey(argTypesHash) ? false : true;
				if (!newArgTypes) {
					argTypesAddr = cachedArgTypes.get(argTypesHash).longValue();
				}

				/* Prepare the prep_cif for the native function specified by the arguments/return layouts */
				initCifNativeThunkData(argLayoutStrs, retLayoutStr, newArgTypes);

				/* Cache the address of prep_cif and argTypes after setting up via the out-of-line native code */
				if (newArgTypes) {
					cachedArgTypes.put(argTypesHash, Long.valueOf(argTypesAddr));
				}
				cachedCifNativeThunkAddr.put(funcSigHash, Long.valueOf(cifNativeThunkAddr));
			}
		}
	}

	/* Convert the specified layout to the TypeKind-based native type string to
	 * keep backward compatibility with Java17 in native during the downcall.
	 */
	private static String convertToNativeTypeString(MemoryLayout targetLayout) {
		String typeKindStr = null;

		/*[IF JAVA_SPEC_VERSION >= 18]*/
		typeKindStr = "b" + targetLayout.bitSize() + "[abi/kind=";
		Class<?> javaType = ((ValueLayout)targetLayout).carrier();

		if (javaType == byte.class) { // JAVA_BYTE corresponds to C_CHAR (1 byte) in native
			typeKindStr += "CHAR"; //$NON-NLS-1$
		} else if (javaType == char.class) { // JAVA_CHAR in Java corresponds to C_SHORT (2 bytes) in native
			typeKindStr += "SHORT"; //$NON-NLS-1$
		} else if (javaType == MemoryAddress.class) {
			typeKindStr += "POINTER"; //$NON-NLS-1$
		} else {
			typeKindStr += javaType.getSimpleName().toUpperCase();
		}
		typeKindStr += "]"; //$NON-NLS-1$
		/*[ELSE] JAVA_SPEC_VERSION >= 18 */
		typeKindStr = targetLayout.toString();
		/*[ENDIF] JAVA_SPEC_VERSION >= 18 */

		return typeKindStr;
	}

	/**
	 * The method is ultimately invoked by Clinker on the specific platforms to generate the requested
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
		ProgrammableInvoker nativeInvoker = new ProgrammableInvoker(functionMethodType, funcDesc);
		/*[ELSE] JAVA_SPEC_VERSION >= 17 */
		ProgrammableInvoker nativeInvoker = new ProgrammableInvoker(downcallAddr, functionMethodType, funcDesc);
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

		try {
			/*[IF JAVA_SPEC_VERSION >= 17]*/
			/*[IF JAVA_SPEC_VERSION >= 18]*/
			MethodType nativeMethodType = methodType(Object.class, NativeSymbol.class, SegmentAllocator.class, long[].class);
			/*[ELSE] JAVA_SPEC_VERSION >= 18 */
			MethodType nativeMethodType = methodType(Object.class, Addressable.class, SegmentAllocator.class, long[].class);
			/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
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
		Class<?> nativeReturnType = nativeMethodType.returnType();
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
		MethodHandle retFilter = getReturnValFilter(nativeReturnType);
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
		}

		return filterMH;
	}

	/* The method (bound by the method handle to the native code) intends to invoke the C function via the inlined code */
	/*[IF JAVA_SPEC_VERSION >= 17]*/
	/*[IF JAVA_SPEC_VERSION >= 18]*/
	Object runNativeMethod(NativeSymbol downcallAddr, SegmentAllocator segmtAllocator, long[] args)
	/*[ELSE] JAVA_SPEC_VERSION >= 18 */
	Object runNativeMethod(Addressable downcallAddr, SegmentAllocator segmtAllocator, long[] args)
	/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
	/*[ELSE] JAVA_SPEC_VERSION >= 17 */
	Object runNativeMethod(long[] args)
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
	{
		/*[IF JAVA_SPEC_VERSION >= 17]*/
		/* Hold a strong reference in the downcall to the native library symbol to
		 * prevent the underlying native library from being unloaded.
		 */
		functionAddr = downcallAddr;
		/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
		long returnVal = invokeNative(functionAddr.address().toRawLongValue(), cifNativeThunkAddr, args);
		return Long.valueOf(returnVal);
	}

	/*[IF JAVA_SPEC_VERSION <= 17]*/
	/* Verify whether the specified layout and the corresponding type are valid and match each other.
	 * Note: will update after the struct layout (phase 2 & 3) is fully implemented.
	 */
	private void checkIfValidLayoutAndType(MethodType targetMethodType, FunctionDescriptor funcDesc) {
		Class<?> retType = targetMethodType.returnType();
		if (!validateArgRetTypeClass(retType) && (retType != void.class)) {
			throw new IllegalArgumentException("The return type must be primitive/void or MemoryAddress" + ": retType = " + retType);  //$NON-NLS-1$ //$NON-NLS-2$
		}
		validateLayoutAgainstType(realReturnLayout, targetMethodType.returnType());

		Class<?>[] argTypes = targetMethodType.parameterArray();
		int argTypeCount = argTypes.length;
		int argLayoutCount = argLayouts.size();
		if (argTypeCount != argLayoutCount) {
			throw new IllegalArgumentException("The arity (" + argTypeCount //$NON-NLS-1$
				+ ") of the argument types is inconsistent with the arity ("  //$NON-NLS-1$
				+ argLayoutCount + ") of the argument layouts");  //$NON-NLS-1$
		}

		for (int argIndex = 0; argIndex < argLayoutCount; argIndex++) {
			if (!validateArgRetTypeClass(argTypes[argIndex])) {
				throw new IllegalArgumentException("The passed-in argument type at index " + argIndex + " is neither primitive nor MemoryAddress"); //$NON-NLS-1$ //$NON-NLS-2$
			}
			validateLayoutAgainstType(argLayoutArray[argIndex], argTypes[argIndex]);
		}
	}

	/* Verify whether the specified type is primitive, MemoryAddress (for pointer) */
	private static boolean validateArgRetTypeClass(Class<?> targetType) {
		if (!targetType.isPrimitive() && (targetType != MemoryAddress.class)
		) {
			return false;
		}
		return true;
	}

	/* Check the validity of the layout against the corresponding type */
	private static void validateLayoutAgainstType(MemoryLayout targetLayout, Class<?> targetType) {
		if (targetLayout != null) {
			if (!targetLayout.hasSize()) {
				throw new IllegalArgumentException("The layout's size is expected: layout = " + targetLayout); //$NON-NLS-1$
			} else if (targetLayout.bitSize() <= 0) {
				throw new IllegalArgumentException("The layout's size must be greater than zero: layout = " + targetLayout); //$NON-NLS-1$
			}
		}

		if (((targetType == void.class) && (targetLayout != null))
		|| ((targetType != void.class) && (targetLayout == null))
		) {
			throw new IllegalArgumentException("Mismatch between the layout and the type: layout = "  //$NON-NLS-1$
				+ ((targetLayout == null) ? "VOID" : targetLayout) //$NON-NLS-1$
				+ ", type = " + targetType);  //$NON-NLS-1$
		/* Check the primitive type and MemoryAddress against the ValueLayout */
		} else if (targetType != void.class) {
			if (!ValueLayout.class.isInstance(targetLayout)) {
				throw new IllegalArgumentException("ValueLayout is expected: layout = " + targetLayout); //$NON-NLS-1$
			}
			/* Check the size and kind of the ValueLayout for the primitive types and MemoryAddress */
			validateValueLayoutSize(targetLayout, targetType);
			validateValueLayoutKind(targetLayout, targetType);
		}
	}

	/* Check the size of the specified primitive layout to determine whether it matches the specified type */
	private static void validateValueLayoutSize(MemoryLayout TypeLayout, Class<?> targetType) {
		int layoutSize = (int)TypeLayout.bitSize();
		boolean mismatchedSize = false;

		switch (layoutSize) {
		case 8:
			/* The 8-bit layout is shared by boolean and byte
			 * given the boolean size specified in Java18 is 8 bits.
			 */
			if ((targetType != boolean.class) && (targetType != byte.class)) {
				mismatchedSize = true;
			}
			break;
		case 16:
			/* The 16-bit layout is shared by char and short
			 * given the char size is 16 bits in Java.
			 */
			if ((targetType != char.class) && (targetType != short.class) ) {
				mismatchedSize = true;
			}
			break;
		case 32:
			/* The 32-bit layout is shared by int and float
			 * given the float size is 32 bits in Java.
			 */
			if ((targetType != int.class) && (targetType != float.class)
			) {
				mismatchedSize = true;
			}
			break;
		case 64:
			/* The 64-bit layout is shared by long, double and the MemoryAddress class
			 * given the corresponding pointer size is 64 bits in C.
			 */
			if ((targetType != long.class)
			&& (targetType != double.class)
			&& (targetType != MemoryAddress.class)
			) {
				mismatchedSize = true;
			}
			break;
		default:
			mismatchedSize = true;
			break;
		}

		if (mismatchedSize) {
			throw new IllegalArgumentException("Mismatched size between the layout and the type: layout = " //$NON-NLS-1$
				+ TypeLayout + ", type = " + targetType.getSimpleName());  //$NON-NLS-1$
		}
	}

	/* Check the kind (type) of the specified primitive layout to determine whether it matches the specified type */
	private static void validateValueLayoutKind(MemoryLayout targetLayout, Class<?> targetType) {
		boolean mismatchType = false;

		if (!targetLayout.toString().contains("[abi/kind=")) { //$NON-NLS-1$
			throw new IllegalArgumentException("The layout's ABI Class is undefined: layout = " + targetLayout); //$NON-NLS-1$
		}

		/* Extract the kind from the specified layout with the ATTR_NAME "abi/kind".
		 * e.g. b32[abi/kind=INT]
		 */
		TypeKind kind = (TypeKind)targetLayout.attribute(TypeKind.ATTR_NAME)
				.orElseThrow(() -> new IllegalArgumentException("The layout's ABI class is empty")); //$NON-NLS-1$
		switch (kind) {
		case CHAR:
			/* the CHAR layout (8bits) in Java only matches with byte in C */
			break;
		case SHORT:
			/* the SHORT layout (16bits) in Java only matches char and short in C */
			break;
		case INT:
			/* the INT layout (32bits) in Java only matches boolean and int in C */
			if (targetType != int.class) {
				mismatchType = true;
			}
			break;
		case LONG:
		case LONG_LONG:
			/* the LONG/LONG_LONG layout (64bits) in Java only matches long in C */
			if (targetType != long.class) {
				mismatchType = true;
			}
			break;
		case FLOAT:
			/* the FLOAT layout (32bits) in Java only matches float in C */
			if (targetType != float.class) {
				mismatchType = true;
			}
			break;
		case DOUBLE:
			/* the DOUBLE layout (64bits) in Java only matches double in C */
			if (targetType != double.class) {
				mismatchType = true;
			}
			break;
		case POINTER:
			/* the POINTER layout (64bits) in Java only matches MemoryAddress */
			if (targetType != MemoryAddress.class) {
				mismatchType = true;
			}
			break;
		default:
			mismatchType = true;
			break;
		}

		if (mismatchType) {
			throw new IllegalArgumentException("Mismatch between the layout and the type: layout = " + targetLayout + ", type = " + targetType);  //$NON-NLS-1$ //$NON-NLS-2$
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION <= 17 */
}
