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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;

/*[IF JAVA_SPEC_VERSION >= 20]*/
import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentScope;
import jdk.internal.foreign.MemorySessionImpl;
/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
import java.lang.foreign.Addressable;
import java.lang.foreign.MemoryAddress;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.MemorySession;
/*[ELSE] JAVA_SPEC_VERSION == 19 */
import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

/**
 * The meta data consists of the callee MH and a cache of 2 elements for MH resolution,
 * which are used to generate an upcall handler to the requested java method.
 */
final class UpcallMHMetaData {

	/* The target method handle intended for upcall which is placed on the java stack
	 * in native2InterpJavaUpcallImpl for call-in so as to invoke the method handle.
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
	/* The argument array stores the memory specific argument (struct/pointer) object
	 * being allocated in native for upcall to stop GC from updating the previously
	 * allocated argument reference when allocating the next argument.
	 */
	private Object[] nativeArgArray;

	/*[IF JAVA_SPEC_VERSION >= 20]*/
	private SegmentScope scope;
	/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
	private MemorySession session;
	/*[ELSE] JAVA_SPEC_VERSION == 19 */
	private ResourceScope scope;
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	private static synchronized native void resolveUpcallDataInfo();

	static {
		/* Resolve the methods/fields (offset in the JCL constant pool of VM) related to the metadata
		 * given the generated macros from vmconstantpool.xml depend on their offsets to access the
		 * corresponding methods/fields in the process of the upcall.
		 */
		resolveUpcallDataInfo();
	}

	/*[IF JAVA_SPEC_VERSION >= 20]*/
	UpcallMHMetaData(MethodHandle targetHandle, int nativeArgCount, SegmentScope scope)
	/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
	UpcallMHMetaData(MethodHandle targetHandle, int nativeArgCount, MemorySession session)
	/*[ELSE] JAVA_SPEC_VERSION == 19 */
	UpcallMHMetaData(MethodHandle targetHandle, int nativeArgCount, ResourceScope scope)
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	{
		calleeMH = targetHandle;
		calleeType = targetHandle.type();
		nativeArgArray = new Object[nativeArgCount];
		/* Only hold the confined session/scope (owned by the current thread)
		 * or the shared session/scope will be used to construct a MemorySegment
		 * object for argument in the native dispatcher in upcall.
		 */
		/*[IF JAVA_SPEC_VERSION >= 20]*/
		this.scope = ((scope != null) && (((MemorySessionImpl)scope).ownerThread() != null)) ? scope : Arena.openShared().scope();
		/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
		this.session = ((session != null) && (session.ownerThread() != null)) ? session : MemorySession.openShared();
		/*[ELSE] JAVA_SPEC_VERSION == 19 */
		this.scope = ((scope != null) && (scope.ownerThread() != null)) ? scope : ResourceScope.newSharedScope();
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	}

	/* Determine whether the memory segment of the passed-in/returned pointer is allocated
	 * in the native memory or not and return its native address if valid.
	 *
	 * Note:
	 * The method is shared in java (downcall) and in native (upcall) via the calling-in from the dispatcher.
	 */
	static long getNativeArgRetSegmentOfPtr(MemorySegment argRetSegmentOfPtr) {
		/*[IF JAVA_SPEC_VERSION >= 20]*/
		/* Verify MemorySegment.NULL as it is introduced since JDK20. */
		if (argRetSegmentOfPtr == MemorySegment.NULL) {
			throw new NullPointerException("A NULL memory segment is not allowed for pointer.");
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
		if (!argRetSegmentOfPtr.isNative()) {
			throw new IllegalArgumentException("Heap segment not allowed: " + argRetSegmentOfPtr);
		}

		/*[IF JAVA_SPEC_VERSION >= 20]*/
		return argRetSegmentOfPtr.address();
		/*[ELSE] JAVA_SPEC_VERSION >= 20 */
		return argRetSegmentOfPtr.address().toRawLongValue();
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	}

	/*[IF JAVA_SPEC_VERSION <= 19]*/
	/* Determine whether the memory address of the passed-in/returned pointer is allocated
	 * in the native memory or not and return its native address if valid.
	 *
	 * Note:
	 * The method is shared in java (downcall) and in native (upcall) via the calling-in from the dispatcher.
	 */
	static long getNativeArgRetAddrOfPtr(MemoryAddress argRetAddrOfPtr) {
		if (argRetAddrOfPtr == MemoryAddress.NULL) {
			throw new NullPointerException("A NULL memory address is not allowed for pointer.");
		}

		/*[IF JAVA_SPEC_VERSION > 17]*/
		/* Validate the native address as MemoryAddress.isNative() is removed in JDK18/19. */
		if (argRetAddrOfPtr.toRawLongValue() == 0)
		/*[ELSE] JAVA_SPEC_VERSION > 17 */
		if (!argRetAddrOfPtr.isNative())
		/*[ENDIF] JAVA_SPEC_VERSION > 17 */
		{
			throw new IllegalArgumentException("A heap address is not allowed: " + argRetAddrOfPtr);
		}

		return argRetAddrOfPtr.toRawLongValue();
	}
	/*[ENDIF] JAVA_SPEC_VERSION <= 19 */

	/* Determine whether the passed-in/returned segment is allocated in the native memory or not
	 * and return its native address if valid; otherwise, return the address of an newly allocated
	 * native segment with all values copied from the heap segment.
	 *
	 * Note:
	 * The method is shared in java (downcall) and in native (upcall) via the calling-in from the dispatcher.
	 */
	static long getNativeArgRetSegment(MemorySegment argRetSegment) {
		/*[IF JAVA_SPEC_VERSION >= 20]*/
		/* MemorySegment.NULL is introduced since JDK20+. */
		if (argRetSegment == MemorySegment.NULL)
		{
			throw new NullPointerException("A NULL memory segment is not allowed for struct.");
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
		MemorySegment nativeSegment = argRetSegment;

		/* Copy all values in the heap segment to a newly allocated native segment
		 * given a heap segment with a zero address can't be accessed in native.
		 */
		if (!argRetSegment.isNative()) {
			/*[IF JAVA_SPEC_VERSION >= 20]*/
			SegmentScope scope = SegmentScope.global();
			/*[ELSEIF JAVA_SPEC_VERSION == 19]*/
			MemorySession scope = MemorySession.global();
			/*[ELSE] JAVA_SPEC_VERSION == 19 */
			ResourceScope scope = ResourceScope.globalScope();
			/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
			nativeSegment = MemorySegment.allocateNative(argRetSegment.byteSize(), scope);
			nativeSegment.copyFrom(argRetSegment);
		}

		/*[IF JAVA_SPEC_VERSION >= 20]*/
		return nativeSegment.address();
		/*[ELSE] JAVA_SPEC_VERSION >= 20 */
		return nativeSegment.address().toRawLongValue();
		/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
	}
}
