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

import java.util.List;

/*[IF JAVA_SPEC_VERSION >= 21]*/
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.PaddingLayout;
import java.lang.foreign.SequenceLayout;
import java.lang.foreign.StructLayout;
import java.lang.foreign.UnionLayout;
import java.lang.foreign.ValueLayout;
import static java.lang.foreign.ValueLayout.*;
import jdk.internal.foreign.abi.LinkerOptions;
/*[ELSE] JAVA_SPEC_VERSION >= 21 */
import jdk.incubator.foreign.CLinker.TypeKind;
import static jdk.incubator.foreign.CLinker.TypeKind.*;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.SequenceLayout;
import jdk.incubator.foreign.ValueLayout;
/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

/**
 * The methods of the class are used to preprocess the layout specified in the function
 * descriptor of downcall or upcall by removing all unnecessary attributes and converting
 * it to a simplified symbol string.
 */
@SuppressWarnings("nls")
final class LayoutStrPreprocessor {
	private static final String osName = System.getProperty("os.name").toLowerCase();
	private static final String arch = System.getProperty("os.arch").toLowerCase();
	private static final boolean isX86_64 = arch.equals("amd64") || arch.equals("x86_64");
	private static final boolean isLinuxS390x = arch.equals("s390x") && osName.startsWith("linux");

	/*[IF JAVA_SPEC_VERSION == 17]*/
	private static final String VARARGS_ATTR_NAME;

	static {
		/* Note: the attributes intended for the layout with variadic argument are defined in OpenJDK. */
		if (isX86_64) {
			if (osName.startsWith("windows")) {
				VARARGS_ATTR_NAME = "abi/windows/varargs";
			} else {
				VARARGS_ATTR_NAME = "abi/sysv/varargs";
			}
		} else if (arch.equals("aarch64")) {
			if (osName.startsWith("mac")) {
				VARARGS_ATTR_NAME = "abi/aarch64/stack_varargs";
			} else {
				VARARGS_ATTR_NAME = "abi/sysv/varargs";
			}
		} else if (arch.startsWith("ppc64")) {
			if (osName.startsWith("linux")) {
				VARARGS_ATTR_NAME = "abi/ppc64/sysv/varargs";
			} else {
				VARARGS_ATTR_NAME = "abi/ppc64/aix/varargs";
			}
		} else if (isLinuxS390x) {
			VARARGS_ATTR_NAME = "abi/s390x/sysv/varargs";
		} else {
			throw new InternalError("Unsupported platform: " + arch + "_" + osName);
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 17 */

	/* Get the index of the variadic argument layout in the function descriptor if exists. */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	static int getVarArgIndex(FunctionDescriptor funcDesc, LinkerOptions options)
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	static int getVarArgIndex(FunctionDescriptor funcDesc)
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	{
		List<MemoryLayout> argLayouts = funcDesc.argumentLayouts();
		int argLayoutsSize = argLayouts.size();
		/* -1 is the default value defined since JDK18+ when the
		 * function descriptor has no variadic arguments.
		 */
		int varArgIdx = -1;

		for (int argIndex = 0; argIndex < argLayoutsSize; argIndex++) {
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			if (options.isVarargsIndex(argIndex))
			/*[ELSE] JAVA_SPEC_VERSION >= 21 */
			if (argLayouts.get(argIndex).attribute(VARARGS_ATTR_NAME).isPresent())
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
			{
				varArgIdx = argIndex;
				break;
			}
		}

		return varArgIdx;
	}

	/* Get the simplified layout string prefixed with layout size by parsing the structure of the layout. */
	static String getSimplifiedLayoutString(MemoryLayout targetLayout, boolean isDownCall) {
		StringBuilder layoutStrBuilder = preprocessLayout(targetLayout, isDownCall);
		long layoutByteSize = targetLayout.byteSize();
		if (isDownCall) {
			/* The padding bytes are not required as they will be handled in native in downcall. */
			long paddingBytes = getTotalPaddingBytesOfStruct(targetLayout);
			layoutStrBuilder.insert(0, layoutByteSize - paddingBytes);
		} else {
			layoutStrBuilder.insert(0, '#').insert(0, layoutByteSize);
		}
		return layoutStrBuilder.toString();
	}

	/* Compute all padding elements of a struct layout in bytes. */
	private static long getTotalPaddingBytesOfStruct(MemoryLayout targetLayout) {
		long paddingBytes = 0;

		/*[IF JAVA_SPEC_VERSION >= 21]*/
		if (targetLayout instanceof StructLayout structLayout)
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		if (targetLayout instanceof GroupLayout structLayout)
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
		{
			List<MemoryLayout> elementLayoutList = structLayout.memberLayouts();
			for (MemoryLayout structElement : elementLayoutList) {
				if (isPaddingLayout(structElement)) {
					long tempPaddingBytes = structElement.byteSize();
					/* The padding bits must be 8 bits (1 byte), 16 bits (2 bytes), 24 bits (3 bytes)
					 * or 32 bits (4 bytes) as requested by ffi_call.
					 *
					 * Note:
					 * there are a couple of padding bytes allowed on platforms as follows;
					 * 1) 5 padding bytes for struct [bool/byte + short, long/double] (padding between short and long/double),
					 * 2) 6 padding bytes for struct [short, long/double],
					 * 3) 7 padding bytes for struct [bool/byte, long/double].
					 */
					if ((tempPaddingBytes <= 0) || (tempPaddingBytes > 7)) {
						throw new IllegalArgumentException("The padding bits is invalid: x" + (tempPaddingBytes * 8));
					}
					paddingBytes += tempPaddingBytes;
				} else {
					paddingBytes += getTotalPaddingBytesOfStruct(structElement);
				}
			}
		}

		return paddingBytes;
	}

	/* Preprocess the layout to generate a concise layout string with all kind symbols
	 * extracted from the layout to simplify parsing the layout string in native.
	 * e.g. a struct layout string with nested struct is as follows: (Only for Java 17)
	 * [
	 *   [
	 *    b32(elem1)[abi/kind=INT,layout/name=elem1]
	 *    b32(elem2)[abi/kind=INT,layout/name=elem2]
	 *   ](Struct1_II)[layout/name=Struct1_II]
	 *   [
	 *    b32(elem1)[abi/kind=INT,layout/name=elem1]
	 *    b32(elem2)[abi/kind=INT,layout/name=elem2]
	 *   ](Struct2_II)[layout/name=Struct2_II]
	 * ](nested_struct)[layout/name=nested_struct]
	 *
	 * which ends up with "16#2[#2[II]#2[II]]" by conversion as follows:
	 *
	 *   16#2[  (16 is the byte size of the layout and 2 is the count of the struct elements
	 *        #2[ 2 is the count of the int elements
	 *           I  (INT)
	 *           I  (INT)
	 *          ]
	 *        #2[ 2 is the count of the int elements
	 *           I  (INT)
	 *           I  (INT)
	 *          ]
	 *        ]
	 *  where "#" denotes the start of struct.
	 *
	 *  Note:
	 *  The prefix "ByteSize#CountOfElmemnt" and "#CountOfElmemnt" are not required in
	 *  the upcall given the converted layout strings are further parsed for the generated
	 *  thunk in native, which is logically different from downcall.
	 */
	private static StringBuilder preprocessLayout(MemoryLayout targetLayout, boolean isDownCall) {
		StringBuilder targetLayoutStr = new StringBuilder();

		/* Directly obtain the kind symbol of the primitive layout. */
		if (targetLayout instanceof ValueLayout valueLayout) {
			targetLayoutStr.append(getPrimitiveTypeSymbol(valueLayout));
		} else if (targetLayout instanceof SequenceLayout arrayLayout) { /* Intended for nested arrays. */
			MemoryLayout elementLayout = arrayLayout.elementLayout();
			/*[IF JAVA_SPEC_VERSION >= 21]*/
			long elementCount = arrayLayout.elementCount();
			/*[ELSE] JAVA_SPEC_VERSION >= 21 */
			long elementCount = arrayLayout.elementCount().getAsLong();
			/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

			/* The padding bytes is required in the native signature for upcall thunk generation. */
			if (isPaddingLayout(elementLayout) && !isDownCall) {
				targetLayoutStr.append('(').append(arrayLayout.byteSize()).append(')');
			} else {
				targetLayoutStr.append(elementCount).append(':').append(preprocessLayout(elementLayout, isDownCall));
			}
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		} else if (targetLayout instanceof UnionLayout unionLayout) { /* Intended for the nested union since JDK21. */
			targetLayoutStr = encodeUnionLayoutStr(unionLayout, targetLayoutStr, isDownCall);
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
		} else if (targetLayout instanceof GroupLayout groupLayout) { /* Intended for the nested struct. */
			targetLayoutStr = encodeStructLayoutStr(groupLayout, targetLayoutStr, isDownCall);
		}

		return targetLayoutStr;
	}

	/* Encode the types in the struct layout to a symbol string to simplify the further processing in native. */
	private static StringBuilder encodeStructLayoutStr(GroupLayout structLayout, StringBuilder targetLayoutStr, boolean isDownCall) {
		List<MemoryLayout> elementLayoutList = structLayout.memberLayouts();
		int structElementCount = elementLayoutList.size();
		StringBuilder elementLayoutStrs = new StringBuilder();
		int paddingElements = 0;

		for (MemoryLayout structElement : elementLayoutList) {
			/* Ignore any padding element in the struct as it is handled by ffi_call by default. */
			if (isPaddingLayout(structElement)) {
				paddingElements += 1;
				/* The padding bytes is required in the native signature for upcall thunk generation. */
				if (!isDownCall) {
					elementLayoutStrs.append('(').append(structElement.byteSize()).append(')');
				}
			} else {
				elementLayoutStrs.append(preprocessLayout(structElement, isDownCall));
			}
		}

		/* Prefix "#" to denote the start of this layout string in the case of downcall. */
		if (isDownCall) {
			targetLayoutStr.append('#').append(structElementCount - paddingElements);
		}
		targetLayoutStr.append('[').append(elementLayoutStrs).append(']');

		return targetLayoutStr;
	}

	/*[IF JAVA_SPEC_VERSION >= 21]*/
	/* Encode the types in the union layout to a symbol string to simplify the further processing in native. */
	private static StringBuilder encodeUnionLayoutStr(UnionLayout unionLayout, StringBuilder targetLayoutStr, boolean isDownCall) {
		long unionByteSize = unionLayout.byteSize();
		StringBuilder elementLayoutStrs = new StringBuilder();
		int byteAlignment = (int)unionLayout.byteAlignment();
		ValueLayout sequenceElement;

		/* The idea is to select the primitive type with the biggest alignment of the union
		 * (obtained via unionLayout.byteAlignment()) to be laid out as a sequence which is
		 * correctly mappped onto the given GPRs/FPRs/stack on the hardware.
		 */
		switch (byteAlignment) {
		case 1: /* 1 Byte. */
			sequenceElement = JAVA_BYTE;
			break;
		case 2: /* 2 Bytes. */
			sequenceElement = JAVA_SHORT;
			break;
		case 4: /* 4 Bytes. */
			if (validatePrimTypesOfUnionForGPROrStack(unionLayout, byteAlignment)) {
				sequenceElement = JAVA_INT;
			} else {
				sequenceElement = JAVA_FLOAT;
			}
			break;
		case 8: /* 8 Bytes. */
			if (validatePrimTypesOfUnionForGPROrStack(unionLayout, byteAlignment)) {
				sequenceElement = JAVA_LONG;
			} else {
				sequenceElement = JAVA_DOUBLE;
			}
			break;
		default:
			throw new IllegalArgumentException("Invalid alignment of the union layout: " + unionLayout);
		}

		long elementByteSize = sequenceElement.byteSize();
		long sequenceCount = (unionByteSize / elementByteSize)
				+ ((0 == (unionByteSize % elementByteSize)) ? 0 : 1);

		/* There is no need to create a sequence layout for a single element. */
		MemoryLayout sequenceLayout = (sequenceCount != 1)
				? MemoryLayout.sequenceLayout(sequenceCount, sequenceElement)
				: sequenceElement;
		elementLayoutStrs.append(preprocessLayout(sequenceLayout, isDownCall));

		/* Prefix "#" to denote the start of this layout string in the case of downcall. */
		if (isDownCall) {
			targetLayoutStr.append('#').append(1);
			targetLayoutStr.append('[').append(elementLayoutStrs).append(']');
		} else {
			/* '{' and '}' are intended for a union in upcall. */
			targetLayoutStr.append('{').append(elementLayoutStrs).append('}');
		}

		return targetLayoutStr;
	}

	/* Check whether the specified 4/8-byte aligned group layout (struct/union) layout contains
	 * non-floating-point primitives so as to determine whether the elements of union can be safely
	 * mapped to GPRs/stack which is uniquely determined by the underlying libffi/compiler.
	 */
	private static boolean validatePrimTypesOfUnionForGPROrStack(GroupLayout groupLayout, int byteAlignment) {
		List<MemoryLayout> elementLayoutList = groupLayout.memberLayouts();
		boolean arePrimTypesForGPROrStack = false;
		for (MemoryLayout elementLayout : elementLayoutList) {
			if (((elementLayout instanceof ValueLayout elemValueLayout)
					&& validatePrimTypeForGPROrStack(elemValueLayout, byteAlignment))
			|| ((elementLayout instanceof GroupLayout elemGroupLayout)
					&& validatePrimTypesOfUnionForGPROrStack(elemGroupLayout, byteAlignment))
			) {
				arePrimTypesForGPROrStack = true;
				break;
			}
		}

		return arePrimTypesForGPROrStack;
	}

	/* Check whether the primitive type can be safely mappped to GPR/stack when it is not float/double
	 * in the struct/union including some exceptions due to the platform restrictions.
	 *
	 * Note:
	 * The primitive type is mappped to GPR/stack as long as it is not float/double on most platforms
	 * with a couple of exceptions that:
	 * 1) the biggest alignment is 8 bytes on the non-X86_64 platforms even though the type is float.
	 * 2) it is always mapped onto GPR/stack in any case on zLinux.
	 */
	private static boolean validatePrimTypeForGPROrStack(ValueLayout primLayout, int byteAlignment) {
		Class<?> carrier = primLayout.carrier();
		boolean isPrimTypeForGPROrStack;

		if (carrier == float.class) {
			isPrimTypeForGPROrStack = isLinuxS390x || ((8 == byteAlignment) && !isX86_64);
		} else if (carrier == double.class) {
			isPrimTypeForGPROrStack = isLinuxS390x;
		} else {
			isPrimTypeForGPROrStack = true;
		}

		return isPrimTypeForGPROrStack;
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */

	/* Determine whether the specfied layout is a padding layout or not. */
	private static boolean isPaddingLayout(MemoryLayout targetLayout) {
		/*[IF JAVA_SPEC_VERSION >= 21]*/
		return targetLayout instanceof PaddingLayout;
		/*[ELSE] JAVA_SPEC_VERSION >= 21 */
		return targetLayout.isPadding();
		/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	}

	/* Map the specified primitive layout's kind to the symbol for primitive type in VM Spec. */
	/*[IF JAVA_SPEC_VERSION >= 21]*/
	private static String getPrimitiveTypeSymbol(ValueLayout targetLayout) {
		Class<?> javaType = targetLayout.carrier();
		String typeSymbol;

		if (javaType == byte.class) {
			/* JAVA_BYTE corresponds to C_CHAR (1 byte) in native. */
			typeSymbol = "C";
		} else if (javaType == char.class) {
			/* JAVA_CHAR in Java corresponds to C_SHORT (2 bytes) in native. */
			typeSymbol = "S";
		} else if (javaType == long.class) {
			/* Map JAVA_LONG to 'J' so as to keep consistent with the existing VM Spec. */
			typeSymbol = "J";
		} else if (javaType == MemorySegment.class) {
			typeSymbol = "P";
		} else {
			/* Obtain the 1st character of the type class as the symbol of the native signature. */
			typeSymbol = javaType.getSimpleName().substring(0, 1).toUpperCase();
		}

		return typeSymbol;
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 21 */
	private static String getPrimitiveTypeSymbol(ValueLayout targetLayout) {
		/* Extract the kind from the specified layout with the ATTR_NAME "abi/kind".
		 * e.g. b32[abi/kind=INT]
		 */
		TypeKind kind = (TypeKind)targetLayout.attribute(TypeKind.ATTR_NAME)
				.orElseThrow(() -> new IllegalArgumentException("The layout's ABI class is empty"));
		String typeSymbol;

		switch (kind) {
		case CHAR:
			typeSymbol = "C";
			break;
		case SHORT:
			typeSymbol = "S";
			break;
		case INT:
			typeSymbol = "I";
			break;
		case LONG:
		case LONG_LONG: /* A 8-byte long type on 64bit Windows as specified in the Spec. */
			/* Map the long layout to 'J' so as to keep consistent with the existing VM Spec. */
			typeSymbol = "J";
			break;
		case FLOAT:
			typeSymbol = "F";
			break;
		case DOUBLE:
			typeSymbol = "D";
			break;
		case POINTER:
			typeSymbol = "P";
			break;
		default:
			throw new IllegalArgumentException("The layout's ABI Class is undefined: layout = " + targetLayout);
		}

		return typeSymbol;
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
}
