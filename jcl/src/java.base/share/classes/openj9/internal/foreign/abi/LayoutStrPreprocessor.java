/*[INCLUDE-IF JAVA_SPEC_VERSION == 19]*/
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

import java.util.List;

/*[IF JAVA_SPEC_VERSION >= 19]*/
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.MemoryAddress;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.SequenceLayout;
import java.lang.foreign.ValueLayout;
/*[ELSE] JAVA_SPEC_VERSION >= 19 */
/*[IF JAVA_SPEC_VERSION <= 17]*/
import jdk.incubator.foreign.CLinker.TypeKind;
import static jdk.incubator.foreign.CLinker.TypeKind.*;
/*[ENDIF] JAVA_SPEC_VERSION <= 17 */
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.SequenceLayout;
import jdk.incubator.foreign.ValueLayout;
/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

/**
 * The methods of the class are used to preprocess the layout specified in the function
 * descriptor of downcall or upcall by removing all unnecessary attributes and converting
 * it to a simplified symbol string.
 */
final class LayoutStrPreprocessor {

	/*[IF JAVA_SPEC_VERSION <= 17]*/
	private static final String osName = System.getProperty("os.name").toLowerCase(); //$NON-NLS-1$
	private static final String arch = System.getProperty("os.arch").toLowerCase(); //$NON-NLS-1$
	private static final String VARARGS_ATTR_NAME;

	static {
		/* Note: the attributes intended for the layout with variadic argument are defined in OpenJDK */
		if ((arch.equals("amd64") || arch.equals("x86_64"))) { //$NON-NLS-1$ //$NON-NLS-2$
			if (osName.startsWith("windows")) { //$NON-NLS-1$
				VARARGS_ATTR_NAME = "abi/windows/varargs"; //$NON-NLS-1$
			} else {
				VARARGS_ATTR_NAME = "abi/sysv/varargs";; //$NON-NLS-1$
			}
		} else if (arch.equals("aarch64")) { //$NON-NLS-1$
			if (osName.startsWith("mac")) { //$NON-NLS-1$
				VARARGS_ATTR_NAME = "abi/aarch64/stack_varargs";; //$NON-NLS-1$
			} else {
				VARARGS_ATTR_NAME = "abi/sysv/varargs"; //$NON-NLS-1$;
			}
		} else if (arch.startsWith("ppc64")) { //$NON-NLS-1$
			if (osName.startsWith("linux")) { //$NON-NLS-1$
				VARARGS_ATTR_NAME = "abi/ppc64/sysv/varargs"; //$NON-NLS-1$
			} else {
				VARARGS_ATTR_NAME = "abi/ppc64/aix/varargs"; //$NON-NLS-1$
			}
		} else if (arch.equals("s390x") && osName.startsWith("linux")) { //$NON-NLS-1$ //$NON-NLS-2$
			VARARGS_ATTR_NAME = "abi/s390x/sysv/varargs"; //$NON-NLS-1$
		} else {
			throw new InternalError("Unsupported platform: " + arch + "_" + osName); //$NON-NLS-1$ //$NON-NLS-2$
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION <= 17 */

	/* Get the index of the variadic argument layout in the function descriptor if exists */
	static int getVarArgIndex(FunctionDescriptor funcDesc) {
		/* -1 is the default value defined in JDK18+ when the function descriptor has no variadic arguments */
		int varArgIdx = -1;

		/*[IF JAVA_SPEC_VERSION >= 18]*/
		varArgIdx = funcDesc.firstVariadicArgumentIndex();
		/*[ELSE] JAVA_SPEC_VERSION >= 18 */
		List<MemoryLayout> argLayouts = funcDesc.argumentLayouts();
		for (int argIndex = 0; argIndex < argLayouts.size(); argIndex++) {
			MemoryLayout argLayout = argLayouts.get(argIndex);
			if (argLayout.attribute(VARARGS_ATTR_NAME).isPresent()) {
				varArgIdx = argIndex;
				break;
			}
		}
		/*[ENDIF] JAVA_SPEC_VERSION >= 18 */

		return varArgIdx;
	}

	/* Get the simplified layout string prefixed with layout size by parsing the structure of the layout */
	static String getSimplifiedLayoutString(MemoryLayout targetLayout, boolean isDownCall) {
		StringBuilder layoutStrBuilder = preprocessLayout(targetLayout, isDownCall);
		long layoutByteSize = targetLayout.byteSize();
		if (isDownCall) {
			/* The padding bytes are not required as they will be handled in native in downcall */
			int paddingBytes = getCountOfPaddingBytesOfStruct(targetLayout);
			layoutStrBuilder.insert(0, layoutByteSize - paddingBytes);
		} else {
			layoutStrBuilder.insert(0, '#').insert(0, layoutByteSize);
		}
		return layoutStrBuilder.toString();
	}

	/* Compute all padding elements of a struct layout in bytes */
	private static int getCountOfPaddingBytesOfStruct(MemoryLayout targetLayout) {
		int paddingBytes = 0;

		if (targetLayout instanceof GroupLayout) {
			GroupLayout structLayout = (GroupLayout)targetLayout;
			List<MemoryLayout> elementLayoutList = structLayout.memberLayouts();
			for (MemoryLayout structElement : elementLayoutList) {
				if (structElement.isPadding()) {
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
						throw new IllegalArgumentException("The padding bits is invalid: x" + (tempPaddingBytes * 8));  //$NON-NLS-1$
					}
					paddingBytes += tempPaddingBytes;
				} else {
					paddingBytes += getCountOfPaddingBytesOfStruct(structElement);
				}
			}
		}

		return paddingBytes;
	}

	/* Preprocess the layout to generate a concise layout string with all kind symbols
	 * extracted from the layout to simplify parsing the layout string in native.
	 * e.g. a struct layout string with nested struct is as follows: (Only for Java <= 17)
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
	 *  1) the prefix "ByteSize#CountOfElmemnt" and "#CountOfElmemnt" are not required in
	 *  the upcall given the converted layout stirngs are further parsed for the generated
	 *  thunk in native, which is logically different from downcall.
	 *  2) the parsing of primitives in layouts in Java 18 is based on the MemoryLayout.carrier()
	 *  rather than the CLinker.TypeKind which is entirely removed in OpenJDK.
	 */
	private static StringBuilder preprocessLayout(MemoryLayout targetLayout, boolean isDownCall) {
		StringBuilder targetLayoutString = new StringBuilder(""); //$NON-NLS-1$

		/* Directly obtain the kind symbol of the primitive layout */
		if (targetLayout instanceof ValueLayout) {
			targetLayoutString.append(getPrimitiveTypeSymbol((ValueLayout)targetLayout));
		} else if (targetLayout instanceof SequenceLayout) { // Intended for nested arrays
			SequenceLayout arrayLayout = (SequenceLayout)targetLayout;
			MemoryLayout elementLayout = arrayLayout.elementLayout();
			/*[IF JAVA_SPEC_VERSION >= 19]*/
			long elementCount = arrayLayout.elementCount();
			/*[ELSE] JAVA_SPEC_VERSION >= 19 */
			long elementCount = arrayLayout.elementCount().getAsLong();
			/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

			/* The padding bytes is required in the native signature for upcall thunk generation */
			if (elementLayout.isPadding() && !isDownCall) {
				targetLayoutString.append('(').append(arrayLayout.byteSize()).append(')');
			} else {
				targetLayoutString.append(elementCount).append(':').append(preprocessLayout(elementLayout, isDownCall));
			}
		} else if (targetLayout instanceof GroupLayout) { // Intended for the nested structs
			GroupLayout structLayout = (GroupLayout)targetLayout;
			List<MemoryLayout> elementLayoutList = structLayout.memberLayouts();
			int structElementCount = elementLayoutList.size();
			StringBuilder elementLayoutStrs = new StringBuilder(""); //$NON-NLS-1$
			int paddingElements = 0;
			for (int elemIndex = 0; elemIndex < structElementCount; elemIndex++) {
				MemoryLayout structElement = elementLayoutList.get(elemIndex);
				/* Ignore any padding element in the struct as it is handled by ffi_call by default */
				if (structElement.isPadding()) {
					paddingElements += 1;
					/* The padding bytes is required in the native signature for upcall thunk generation */
					if (!isDownCall) {
						elementLayoutStrs.append('(').append(structElement.byteSize()).append(')');
					}
				} else {
					elementLayoutStrs.append(preprocessLayout(structElement, isDownCall));
				}
			}
			/* Prefix "#" to denote the start of this layout string in the case of downcall */
			if (isDownCall) {
				targetLayoutString.append('#').append(structElementCount - paddingElements);
			}
			targetLayoutString.append('[').append(elementLayoutStrs).append(']');
		}

		return targetLayoutString;
	}

	/* Map the specified primitive layout's kind to the symbol for primitive type in VM Spec
	 *
	 * Note:
	 * CLinker.TypeKind is entirely removed since Java 18, in which case we need to
	 * reply on MemoryLayout.carrier() (simply the type identified by the layout) for the native
	 * signature of the layout.
	 */
	/*[IF JAVA_SPEC_VERSION >= 18]*/
	private static String getPrimitiveTypeSymbol(ValueLayout targetLayout) {
		Class<?> javaType = targetLayout.carrier();
		String typeSymbol = ""; //$NON-NLS-1$

		if (javaType == byte.class) { // JAVA_BYTE corresponds to C_CHAR (1 byte) in native
			typeSymbol = "C"; //$NON-NLS-1$
		} else if (javaType == char.class) { // JAVA_CHAR in Java corresponds to C_SHORT (2 bytes) in native
			typeSymbol = "S"; //$NON-NLS-1$
		} else if (javaType == long.class) { // JAVA_CHAR in Java corresponds to C_SHORT (2 bytes) in native
			typeSymbol = "J"; //$NON-NLS-1$  // Map JAVA_LONG to 'J' so as to keep consistent with the existing VM Spec
		} else if (javaType == MemoryAddress.class) {
			typeSymbol = "P"; //$NON-NLS-1$
		} else {
			/* Obtain the 1st character of the type class as the symbol of the native signature */
			typeSymbol = javaType.getSimpleName().toUpperCase().substring(0, 1);
		}
		return typeSymbol;
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 18 */
	private static String getPrimitiveTypeSymbol(ValueLayout targetLayout) {
		/* Extract the kind from the specified layout with the ATTR_NAME "abi/kind".
		 * e.g. b32[abi/kind=INT]
		 */
		TypeKind kind = (TypeKind)targetLayout.attribute(TypeKind.ATTR_NAME)
				.orElseThrow(() -> new IllegalArgumentException("The layout's ABI class is empty")); //$NON-NLS-1$
		String typeSymbol = ""; //$NON-NLS-1$

		switch (kind) {
		case CHAR:
			typeSymbol = "C"; //$NON-NLS-1$
			break;
		case SHORT:
			typeSymbol = "S"; //$NON-NLS-1$
			break;
		case INT:
			typeSymbol = "I"; //$NON-NLS-1$
			break;
		case LONG:
		case LONG_LONG: // A 8-byte long type on 64bit Windows as specified in the Spec.
			/* Map the long layout to 'J' so as to keep consistent with the existing VM Spec */
			typeSymbol = "J"; //$NON-NLS-1$
			break;
		case FLOAT:
			typeSymbol = "F"; //$NON-NLS-1$
			break;
		case DOUBLE:
			typeSymbol = "D"; //$NON-NLS-1$
			break;
		case POINTER:
			typeSymbol = "P"; //$NON-NLS-1$
			break;
		default:
			throw new IllegalArgumentException("The layout's ABI Class is undefined: layout = " + targetLayout); //$NON-NLS-1$
		}

		return typeSymbol;
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 18 */
}
