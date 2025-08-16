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

/*[IF JAVA_SPEC_VERSION >= 22]*/
import java.util.Arrays;
/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
import java.util.HashMap;
import java.util.List;
import java.util.Deque;
import java.util.ArrayDeque;
/*[IF JAVA_SPEC_VERSION >= 21]*/
import java.util.Objects;
/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
import java.util.concurrent.ConcurrentHashMap;
import java.util.Set;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import static java.lang.invoke.MethodType.methodType;
import java.lang.invoke.WrongMethodTypeException;

/*[IF JAVA_SPEC_VERSION >= 21]*/
import java.lang.foreign.AddressLayout;
import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.MemorySegment.Scope;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.ValueLayout;
/*[IF JAVA_SPEC_VERSION >= 22]*/
import jdk.internal.access.SharedSecrets;
import jdk.internal.foreign.AbstractMemorySegmentImpl;
/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
import jdk.internal.foreign.MemorySessionImpl;
import jdk.internal.foreign.Utils;
import jdk.internal.foreign.abi.LinkerOptions;
/*[ELSE] JAVA_SPEC_VERSION >= 21 */
import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.ResourceScope.Handle;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.ValueLayout;
/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

/*[IF JAVA_SPEC_VERSION >= 21]*/
import static java.lang.foreign.ValueLayout.*;
import static jdk.internal.foreign.abi.SharedUtils.*;
/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

/**
 * The internal implementation of downcall handler wraps up a method handle enabling
 * the native code to the ffi_call via the libffi interface at runtime.
 */
@SuppressWarnings("nls")
public class InternalDowncallHandler {

	private final MethodType funcMethodType;
	private final FunctionDescriptor funcDescriptor;
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private final LinkerOptions linkerOpts;
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	private long cifNativeThunkAddr;
	private long argTypesAddr;
	private MemoryLayout[] argLayoutArray;
	private MemoryLayout realReturnLayout;
	private MethodHandle boundMH;

	/*[IF JAVA_SPEC_VERSION >= 22]*/
	/* The ThreadLocal holds the information of the heap argument which includes
	 * the base object array, the offset array plus the current heap argument index
	 * for the current thread in multithreading. The heap base array stores the base
	 * object (which contains the heap address as defined in OpenJDK) of the heap
	 * arguments to extract the heap address from the base object in native.
	 */
	private static final class HeapArgInfo {
		final Object[] bases;
		final long[] offsets;
		int index;

		HeapArgInfo(int size) {
			bases = new Object[size];
			offsets = new long[size];
			index = 0;
		}

		void append(Object base, long offset) {
			bases[index] = base;
			offsets[index] = offset;
			index += 1;
		}

		boolean isFull() {
			return index >= bases.length;
		}

		void clear() {
			Arrays.fill(bases, null);
			index = 0;
		}
	}

	private final ThreadLocal<Deque<HeapArgInfo>> heapArgInfo;
	/*[ENDIF] JAVA_SPEC_VERSION >= 22 */

	/* The hashtables of sessions/scopes is intended for multithreading in which case
	 * the same downcall handler might hold various sessions/scopes being used by
	 * different threads in downcall.
	 */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private Set<Scope> memArgScopeSet;
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private Set<ResourceScope> memArgScopeSet;
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

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
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private MethodHandle memSegmtOfPtrToLongArgFilter;
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private MethodHandle memAddrToLongArgFilter;
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
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
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private MethodHandle longObjToMemSegmtRetFilter;
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private static final MethodHandle longObjToMemAddrRetFilter;
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	private static final MethodHandle objToMemSegmtRetFilter;
	/*[IF JAVA_SPEC_VERSION >= 22]*/
	private static native boolean isFfiProtoEnabled();
	private static boolean isFfiProtoOn = isFfiProtoEnabled();
	/* This native resolves the downcall handle via MethodHandleResolver.ffiCallLinkCallerMethod()
	 * given the handle must be resolved in advance when calling into the interpreter via
	 * MH.linkToNative().
	 */
	private static native void setNativeInvokeCache(MethodHandle handle);
	/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
	private static synchronized native void resolveRequiredFields();
	private native void initCifNativeThunkData(String[] argLayouts, String retLayout, boolean newArgTypes, int varArgIndex);
	private native long invokeNative(
			/*[IF JAVA_SPEC_VERSION >= 24]*/
			Object returnStateMemBase,
			/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
			/*[IF JAVA_SPEC_VERSION >= 22]*/
			Object[] bases,
			long[] offsets,
			/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			boolean isInCriticalDownCall, long returnStateMemAddr,
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
			long returnStructMemAddr, long functionAddress, long calloutThunk, long[] argValues
			);

	private static final class PrivateClassLock {
		PrivateClassLock() {}
	}
	private static final Object privateClassLock = new PrivateClassLock();

	static {
		try {
			/* Set up the argument filters for the primitive types and MemoryAddress. */
			booleanToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "booleanToLongArg", methodType(long.class, boolean.class));
			charToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "charToLongArg", methodType(long.class, char.class));
			byteToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "byteToLongArg", methodType(long.class, byte.class));
			shortToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "shortToLongArg", methodType(long.class, short.class));
			intToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "intToLongArg", methodType(long.class, int.class));
			floatToLongArgFilter = lookup.findStatic(InternalDowncallHandler.class, "floatToLongArg", methodType(long.class, float.class));
			doubleToLongArgFilter = lookup.findStatic(Double.class, "doubleToLongBits", methodType(long.class, double.class));

			/* Set up the return value filters for the primitive types and MemoryAddress. */
			longObjToVoidRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToVoidRet", methodType(void.class, Object.class));
			longObjToBooleanRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToBooleanRet", methodType(boolean.class, Object.class));
			longObjToCharRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToCharRet", methodType(char.class, Object.class));
			longObjToByteRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToByteRet", methodType(byte.class, Object.class));
			longObjToShortRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToShortRet", methodType(short.class, Object.class));
			longObjToIntRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToIntRet", methodType(int.class, Object.class));
			longObjToLongRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToLongRet", methodType(long.class, Object.class));
			longObjToFloatRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToFloatRet", methodType(float.class, Object.class));
			longObjToDoubleRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToDoubleRet", methodType(double.class, Object.class));
			/*[IF JAVA_SPEC_VERSION == 17]*/
			longObjToMemAddrRetFilter = lookup.findStatic(InternalDowncallHandler.class, "longObjToMemAddrRet", methodType(MemoryAddress.class, Object.class));
			/*[ENDIF] JAVA_SPEC_VERSION == 17 */
			objToMemSegmtRetFilter = lookup.findStatic(InternalDowncallHandler.class, "objToMemSegmtRet", methodType(MemorySegment.class, Object.class));
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

	/* Save the active session of the specified passed-in memory specific argument in the downcall handler
	 * given the argument might be created within different sessions/scopes.
	 */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private final void addMemArgScope(Scope memArgScope) throws IllegalStateException
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private final void addMemArgScope(ResourceScope memArgScope) throws IllegalStateException
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	{
		validateMemScope(memArgScope);
		memArgScopeSet.add(memArgScope);
	}

	/* Validate the memory related scope to ensure that it is kept alive during the downcall. */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private void validateMemScope(Scope memScope) throws IllegalStateException
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private void validateMemScope(ResourceScope memScope) throws IllegalStateException
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	{
		if (!memScope.isAlive()) {
			throw new IllegalStateException("Already closed: attempted to access the memory value in a closed scope");
		}
	}

	/*[IF JAVA_SPEC_VERSION >= 21]*/
	/* Intended for memSegmtOfPtrToLongArgFilter that converts the memory segment
	 * of the passed-in pointer argument to long.
	 */
	private final long memSegmtOfPtrToLongArg(MemorySegment argValue, LinkerOptions options) throws IllegalStateException {
		/*[IF JAVA_SPEC_VERSION >= 22]*/
		Deque<HeapArgInfo> infoStack = heapArgInfo.get();
		HeapArgInfo info = infoStack.peek();
		/*[ENDIF] JAVA_SPEC_VERSION >= 22 */

		try {
			UpcallMHMetaData.validateNativeArgRetSegmentOfPtr(argValue, options);
			addMemArgScope(argValue.scope());
		} catch (Throwable e) {
			/*[IF JAVA_SPEC_VERSION >= 22]*/
			/* Clear up the heap argument information for the current thread when the exception
			 * is captured so as to reset the internal index and avoid retaining the references
			 * to the unreachable objects.
			 */
			if (info != null) {
				info.clear();
			}
			/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
			throw e;
		}

		long address = argValue.address();
		/*[IF JAVA_SPEC_VERSION >= 22]*/
		if (info == null || info.isFull()) {
			info = new HeapArgInfo(argLayoutArray.length);
			infoStack.push(info);
		}

		if (!argValue.isNative() && options.allowsHeapAccess()) {
			/* Store the heap argument's base object and offset. */
			AbstractMemorySegmentImpl segment = (AbstractMemorySegmentImpl)argValue;
			info.append(segment.unsafeGetBase(), segment.unsafeGetOffset());
		} else {
			/* Set null to the heap base object to denote a native array. */
			info.append(null, 0L);
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
		return address;
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	/* Intended for memAddrToLongArgFilter that converts the memory address to long. */
	private final long memAddrToLongArg(MemoryAddress argValue) throws IllegalStateException {
		UpcallMHMetaData.validateNativeArgRetAddrOfPtr(argValue);
		addMemArgScope(argValue.scope());
		return argValue.toRawLongValue();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

	/* Intended for memSegmtToLongArgFilter that converts the memory segment to long. */
	private final long memSegmtToLongArg(MemorySegment argValue) throws IllegalStateException {
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		try {
			addMemArgScope(argValue.scope());
			return UpcallMHMetaData.getNativeArgRetSegment(argValue).address();
		} catch (Throwable e) {
			/*[IF JAVA_SPEC_VERSION >= 22]*/
			/* Clear up the heap argument information for the current thread when the exception
			 * is captured so as to reset the internal index and avoid retaining the references
			 * to the unreachable objects.
			 */
			Deque<HeapArgInfo> infoStack = heapArgInfo.get();
			if (!infoStack.isEmpty()) {
				HeapArgInfo info = infoStack.peek();
				if (info != null) {
					info.clear();
				}
			}
			/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
			throw e;
		}
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		addMemArgScope(argValue.scope());
		return UpcallMHMetaData.getNativeArgRetSegment(argValue).address().toRawLongValue();
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
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

	/*[IF JAVA_SPEC_VERSION >= 21]*/
	/* Intended for longObjToMemSegmtRetFilter that converts the Long object to the memory segment. */
	private MemorySegment longObjToMemSegmtRet(Object retValue) {
		long tmpValue = ((Long)retValue).longValue();
		/* Return the created segment with the given pointer address when the address is valid
		 * against the specified target layout if exists.
		 */
		return UpcallMHMetaData.getArgRetAlignedSegmentOfPtr(tmpValue, realReturnLayout);
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	/* Intended for longObjToMemAddrRetFilter that converts the Long object to the memory address. */
	private static final MemoryAddress longObjToMemAddrRet(Object retValue) {
		long tmpValue = ((Long)retValue).longValue();
		return MemoryAddress.ofLong(tmpValue);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

	/* Intended for objToMemSegmtRetFilter that simply casts the passed-in object to the memory segment
	 * given the requested the memory segment is directly returned from runNativeMethod().
	 * Note: the returned memory address is exactly the address of the memory previously allocated
	 * for the specified struct layout on return.
	 */
	private static final MemorySegment objToMemSegmtRet(Object retValue) {
		return (MemorySegment)retValue;
	}

	/*[IF JAVA_SPEC_VERSION >= 21]*/
	/**
	 * The internal constructor is responsible for mapping the preprocessed layouts
	 * of return type & argument types to the underlying prep_cif in native.
	 *
	 * @param functionMethodType the MethodType of the specified native function
	 * @param funcDesc the function descriptor of the specified native function
	 * @param options the linker options indicating additional linking requirements to the linker
	 */
	public InternalDowncallHandler(MethodType functionMethodType, FunctionDescriptor functionDescriptor, LinkerOptions options)
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	/**
	 * The internal constructor is responsible for mapping the preprocessed layouts
	 * of return type & argument types to the underlying prep_cif in native.
	 *
	 * @param functionMethodType the MethodType of the specified native function
	 * @param funcDesc the function descriptor of the specified native function
	 */
	public InternalDowncallHandler(MethodType functionMethodType, FunctionDescriptor functionDescriptor)
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	{
		realReturnLayout = functionDescriptor.returnLayout().orElse(null); // set to null for void
		List<MemoryLayout> argLayouts = functionDescriptor.argumentLayouts();
		argLayoutArray = argLayouts.toArray(new MemoryLayout[argLayouts.size()]);

		/*[IF JAVA_SPEC_VERSION == 17]*/
		/* The layout check against the method type is still required for Java 16 & 17 in that
		 * both the function descriptor and the method type are passed in as arguments by users.
		 * Note: skip the validity check on function descriptor in Java 18 as it is done before
		 * initializing ProgrammableInvoker in OpenJDK. Meanwhile, the method type is directly
		 * deduced from the function descriptor itself, in which case there is no need to
		 * check the layout against the method type.
		 */
		TypeLayoutCheckHelper.checkIfValidLayoutAndType(functionMethodType, argLayoutArray, realReturnLayout);
		/*[ENDIF] JAVA_SPEC_VERSION == 17 */

		funcMethodType = functionMethodType;
		funcDescriptor = functionDescriptor;
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		linkerOpts = options;
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

		cifNativeThunkAddr = 0;
		argTypesAddr = 0;

		memArgScopeSet = ConcurrentHashMap.newKeySet();
		/*[IF JAVA_SPEC_VERSION == 17]*/
		scopeHandleMap = new ConcurrentHashMap<>();
		/*[ENDIF] JAVA_SPEC_VERSION == 17 */

		/*[IF JAVA_SPEC_VERSION >= 22]*/
		heapArgInfo = ThreadLocal.withInitial(ArrayDeque::new);
		/*[ENDIF] JAVA_SPEC_VERSION >= 22 */

		try {
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			longObjToMemSegmtRetFilter = lookup.bind(this, "longObjToMemSegmtRet", methodType(MemorySegment.class, Object.class));
			memSegmtOfPtrToLongArgFilter = lookup.bind(this, "memSegmtOfPtrToLongArg", methodType(long.class, MemorySegment.class, LinkerOptions.class));
			/*[ELSE] JAVA_SPEC_VERSION >= 21 */
			memAddrToLongArgFilter = lookup.bind(this, "memAddrToLongArg", methodType(long.class, MemoryAddress.class));
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
			memSegmtToLongArgFilter = lookup.bind(this, "memSegmtToLongArg", methodType(long.class, MemorySegment.class));
		} catch (ReflectiveOperationException e) {
			throw new InternalError(e);
		}

		generateAdapter();
	}

	/* Map the layouts of return type & argument types to the underlying prep_cif. */
	private void generateAdapter() {
		int argLayoutCount = argLayoutArray.length;
		String[] argLayoutStrs = new String[argLayoutCount];
		StringBuilder argLayoutStrsLine = new StringBuilder("(|");
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
		String retLayoutStr = "0V";
		if (realReturnLayout != null) {
			retLayoutStr = LayoutStrPreprocessor.getSimplifiedLayoutString(realReturnLayout, true);
		}

		synchronized (privateClassLock) {
			/* If a prep_cif for a given function descriptor exists, then the corresponding return & argument layouts
			 * were already set up for this prep_cif, in which case there is no need to check the layouts.
			 * If not the case, check at first whether the same return & argument layouts exist in the cache
			 * in case of duplicate memory allocation for the same layouts.
			 *
			 * Note: (JDK17)
			 * 1) C_LONG (Linux) and C_LONG_LONG (Windows/AIX 64bit) should be treated as the same layout in the cache.
			 * 2) the same layout kind with or without the layout name should be treated as the same layout.
			 * e.g.  C_INT without the layout name = b32[abi/kind=INT]
			 *  and  C_INT with the layout name = b32(int)[abi/kind=INT,layout/name=int]
			 */
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			int varArgIdx = LayoutStrPreprocessor.getVarArgIndex(funcDescriptor, linkerOpts);
			/*[ELSE] JAVA_SPEC_VERSION >= 21 */
			int varArgIdx = LayoutStrPreprocessor.getVarArgIndex(funcDescriptor);
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
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
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			MethodType nativeMethodType = methodType(Object.class, MemorySegment.class, SegmentAllocator.class, MemorySegment.class, long[].class);
			/*[ELSE] JAVA_SPEC_VERSION >= 21 */
			MethodType nativeMethodType = methodType(Object.class, Addressable.class, SegmentAllocator.class, long[].class);
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

			boundMH = lookup.bind(this, "runNativeMethod", nativeMethodType);

			/* Replace the original handle with the specified types of the C function. */
			boundMH = permuteMH(boundMH, funcMethodType);

			/*[IF JAVA_SPEC_VERSION >= 22]*/
			return isFfiProtoOn ? getNativeMHWithInvokeCache(boundMH) : boundMH;
			/*[ELSE] JAVA_SPEC_VERSION >= 22 */
			return boundMH;
			/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
		} catch (ReflectiveOperationException e) {
			throw new InternalError(e);
		}
	}

	/*[IF JAVA_SPEC_VERSION >= 22]*/
	/* Generate the native MethodHandle with the resolved bound handle. */
	private MethodHandle getNativeMHWithInvokeCache(MethodHandle handle) {
		MethodHandle nativeMH = SharedSecrets.getJavaLangInvokeAccess().nativeMethodHandle(handle);
		/* The cache array declared in the NativeMethodHandle (OpenJDK) stores MemberName
		 * and appendix resolved from the bound handle.
		 */
		setNativeInvokeCache(nativeMH);
		return nativeMH;
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 22 */

	/* Collect and convert the passed-in arguments to an Object array for the underlying native call. */
	private MethodHandle permuteMH(MethodHandle targetHandle, MethodType nativeMethodType) throws NullPointerException, WrongMethodTypeException {
		Class<?>[] argTypeClasses = nativeMethodType.parameterArray();
		int nativeArgCount = argTypeClasses.length;
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		/* Skip the native function address, the segment allocator and the segment
		 * for the execution state to the native function's arguments.
		 */
		int argPosition = 3;
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		/* Skip the native function address and the segment allocator to the native function's arguments. */
		int argPosition = 2;
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
		MethodHandle resultHandle = targetHandle.asCollector(argPosition, long[].class, nativeArgCount);

		/* Convert the argument values to long via filterArguments() prior to the native call. */
		MethodHandle[] argFilters = new MethodHandle[nativeArgCount];
		for (int argIndex = 0; argIndex < nativeArgCount; argIndex++) {
			argFilters[argIndex] = getArgumentFilter(argTypeClasses[argIndex], argLayoutArray[argIndex]);
		}
		resultHandle = MethodHandles.filterArguments(resultHandle, argPosition, argFilters);

		/* Convert the return value to the specified type via filterReturnValue() after the native call. */
		MethodHandle retFilter = getReturnValFilter(nativeMethodType.returnType());
		resultHandle = MethodHandles.filterReturnValue(resultHandle, retFilter);

		/*[IF JAVA_SPEC_VERSION >= 21]*/
		/* Set a placeholder with a NULL segment if there is no request
		 * for the execution state from downcall in the linker options.
		 */
		if (!linkerOpts.hasCapturedCallState()) {
			resultHandle = MethodHandles.insertArguments(resultHandle, 2, MemorySegment.NULL);
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
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
		/*[IF JAVA_SPEC_VERSION == 17]*/
		if ((argTypeClass == MemoryAddress.class)
		) {
			filterMH = memAddrToLongArgFilter;
		} else
		/*[ENDIF] JAVA_SPEC_VERSION == 17 */
		if (argTypeClass == MemorySegment.class) {
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			/* The address layout for pointer might come with different representations of ADDRESS.
			 * Note: AddressLayout is introduced in JDK21 to replace OfAddress.
			 */
			if (argLayout instanceof AddressLayout) {
				filterMH = MethodHandles.insertArguments(memSegmtOfPtrToLongArgFilter, 1, linkerOpts);
			} else
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
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
		/*[IF JAVA_SPEC_VERSION == 17]*/
		if ((returnType == MemoryAddress.class)
		) {
			filterMH = longObjToMemAddrRetFilter;
		} else
		/*[ENDIF] JAVA_SPEC_VERSION == 17 */
		if (returnType == MemorySegment.class) {
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			/* A returned pointer is wrapped as a zero-sized memory segment given all
			 * MemoryAdress related classes are removed against the latest APIs as
			 * specified in JDK20+ in which case the address layout for pointer might
			 * come with different representations of ADDRESS.
			 * Note: AddressLayout is introduced in JDK21 to replace OfAddress.
			 */
			if ((realReturnLayout != null) && (realReturnLayout instanceof AddressLayout)) {
				filterMH = longObjToMemSegmtRetFilter;
			} else
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
			{
				filterMH = objToMemSegmtRetFilter;
			}
		}

		return filterMH;
	}

	/*[IF JAVA_SPEC_VERSION >= 21]*/
	/* Set up the dependency from the sessions of memory related arguments to the specified session
	 * so as to keep these arguments' session alive till the specified session is closed.
	 */
	private void SetDependency(Scope session) {
		Objects.requireNonNull(session);
		for (Scope memArgSession : memArgScopeSet) {
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
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	/* Occupy the scope by setting the scope's state in downcall so as to keep these
	 * arguments' scope alive till the specified session is closed.
	 */
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

	/* Release the scope with the scope's handle in downcall. */
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
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

	/* Return the valid downcall related memory address by doing the validity check
	 * on the address's scope in OpenJ9 since the related code and APIs were adjusted
	 * in JDK20 in which case the scope check on the downcall in address() in OpenJDK
	 * was entirely removed.
	 */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private long getValidDowncallMemAddr(MemorySegment memAddr) {
		validateMemScope(memAddr.scope());
		return memAddr.address();
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private long getValidDowncallMemAddr(Addressable memAddr) {
		validateMemScope(memAddr.address().scope());
		return memAddr.address().toRawLongValue();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

	/* The method (bound by the method handle to the native code) intends to invoke the C function via the inlined code. */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	Object runNativeMethod(MemorySegment downcallAddr, SegmentAllocator segmtAllocator, MemorySegment stateSegmt, long[] args) throws IllegalArgumentException, IllegalStateException
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	Object runNativeMethod(Addressable downcallAddr, SegmentAllocator segmtAllocator, long[] args) throws IllegalArgumentException, IllegalStateException
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	{
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		if (downcallAddr == MemorySegment.NULL)
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		if (downcallAddr.address() == MemoryAddress.NULL)
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
		{
			throw new IllegalArgumentException("A non-null memory address is expected for downcall");
		}

		long retMemAddr = 0;
		MemorySegment retStruSegmt = null;
		/* Validate the returned struct type with the corresponding layout given
		 * the MemorySegment class is intended for both struct and pointer since
		 * JDK20+.
		 */
		if ((realReturnLayout != null) && (realReturnLayout instanceof GroupLayout)) {
			/* The segment allocator (introduced since Java 17) is confined by the memory session/scope
			 * defined in the user applications in which case the allocated memory will be released
			 * automatically once the session/scope is closed.
			 */
			retStruSegmt = segmtAllocator.allocate(realReturnLayout);
			if (retStruSegmt == null) {
				throw new OutOfMemoryError("Failed to allocate native memory for the returned memory segment");
			}
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			retMemAddr = retStruSegmt.address();
			/*[ELSE] JAVA_SPEC_VERSION >= 21 */
			retMemAddr = retStruSegmt.address().toRawLongValue();
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
		}

		/* Add the scopes of the downcall related memory addresses to the set
		 * to ensure they are kept alive till the downcall is finished.
		 */
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		addMemArgScope(downcallAddr.scope());
		addMemArgScope(stateSegmt.scope());
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		addMemArgScope(downcallAddr.address().scope());
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

		long returnVal;
		/*[IF JAVA_SPEC_VERSION >= 22]*/
		HeapArgInfo info = heapArgInfo.get();
		/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
		/* The scope associated with memory specific arguments must be kept alive
		 * during the downcall since JDK17, including the downcall adddress.
		 *
		 * Note: memArgScopeSet is not empty with the downcall address added to the set.
		 */
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		/*[IF JAVA_SPEC_VERSION >= 24]*/
		long returnStateMemAddr;
		Object returnStateMemBase;
		if (linkerOpts.hasCapturedCallState() && !stateSegmt.isNative()) {
			/* The CaptureCallState option can only use heap memory if allowed by the Critical option. */
			if (!linkerOpts.isCritical()) {
				throw new IllegalArgumentException("Heap segment not allowed");
			}
			AbstractMemorySegmentImpl segment = (AbstractMemorySegmentImpl)stateSegmt;
			returnStateMemAddr = segment.unsafeGetOffset();
			returnStateMemBase = segment.unsafeGetBase();
		} else {
			returnStateMemAddr = getValidDowncallMemAddr(stateSegmt);
			returnStateMemBase = null;
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 24 */

		try (Arena arena = Arena.ofConfined()) {
			SetDependency(arena.scope());
			returnVal = invokeNative(
					/*[IF JAVA_SPEC_VERSION >= 24]*/
					returnStateMemBase,
					/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
					/*[IF JAVA_SPEC_VERSION >= 22]*/
					(info != null) ? info.bases : null,
					(info != null) ? info.offsets : null,
					linkerOpts.isCritical(),
					/*[ELSE] JAVA_SPEC_VERSION >= 22 */
					linkerOpts.isTrivial(),
					/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
					/*[IF JAVA_SPEC_VERSION >= 24]*/
					returnStateMemAddr,
					/*[ELSE] JAVA_SPEC_VERSION >= 24 */
					getValidDowncallMemAddr(stateSegmt),
					/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
					retMemAddr,
					getValidDowncallMemAddr(downcallAddr),
					cifNativeThunkAddr,
					args);
		}
		/*[IF JAVA_SPEC_VERSION >= 22]*/
		finally {
			/* Clear up the heap argument information for the current thread after the downcall
			 * so as to reset the internal index and avoid retaining the references to the
			 * unreachable objects.
			 */
			Deque<HeapArgInfo> infoStack = heapArgInfo.get();
			if (!infoStack.isEmpty()) {
				infoStack.pop();
			}
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		acquireScope();
		returnVal = invokeNative(retMemAddr, getValidDowncallMemAddr(downcallAddr), cifNativeThunkAddr, args);
		releaseScope();
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

		/* This struct specific MemorySegment object returns to the current thread in the multithreading environment,
		 * in which case the native invocations from threads end up with distinct returned structs.
		 */
		return (retStruSegmt != null) ? retStruSegmt : Long.valueOf(returnVal);
	}
}
