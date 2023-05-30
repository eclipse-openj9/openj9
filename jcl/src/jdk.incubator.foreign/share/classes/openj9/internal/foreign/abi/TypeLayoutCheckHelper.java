/*[INCLUDE-IF (JAVA_SPEC_VERSION >= 16) & (JAVA_SPEC_VERSION <= 17)]*/
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

import java.util.List;
import java.lang.invoke.MethodType;

import jdk.incubator.foreign.CLinker.TypeKind;
import static jdk.incubator.foreign.CLinker.TypeKind.*;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ValueLayout;

/**
 * The Class is a collection of helper methods to validate the method types
 * against the corresponding layouts in the function descriptor.
 */
final class TypeLayoutCheckHelper {

	/* Verify whether the specified layout and the corresponding type are valid and match each other */
	static void checkIfValidLayoutAndType(MethodType targetMethodType, MemoryLayout[] argumentLayouts, MemoryLayout returnLayout) {
		Class<?> retType = targetMethodType.returnType();
		if (!validateArgRetTypeClass(retType) && (retType != void.class)) {
			throw new IllegalArgumentException("The return type must be primitive/void, MemoryAddress or MemorySegment" + ": retType = " + retType);  //$NON-NLS-1$ //$NON-NLS-2$
		}

		validateLayoutAgainstType(returnLayout, targetMethodType.returnType());

		Class<?>[] argTypes = targetMethodType.parameterArray();
		int argTypeCount = argTypes.length;
		int argLayoutCount = argumentLayouts.length;
		if (argTypeCount != argLayoutCount) {
			throw new IllegalArgumentException("The arity (" + argTypeCount //$NON-NLS-1$
					+ ") of the argument types is inconsistent with the arity ("  //$NON-NLS-1$
					+ argLayoutCount + ") of the argument layouts");  //$NON-NLS-1$
		}

		for (int argIndex = 0; argIndex < argLayoutCount; argIndex++) {
			if (!validateArgRetTypeClass(argTypes[argIndex])) {
				throw new IllegalArgumentException("The passed-in argument type at index " + argIndex + " is neither primitive nor MemoryAddress nor MemorySegment"); //$NON-NLS-1$ //$NON-NLS-2$
			}
			validateLayoutAgainstType(argumentLayouts[argIndex], argTypes[argIndex]);
		}
	}

	/* Verify whether the specified type is primitive, MemoryAddress (for pointer) or MemorySegment (for struct) */
	private static boolean validateArgRetTypeClass(Class<?> targetType) {
		if (!targetType.isPrimitive()
		&& (targetType != MemoryAddress.class)
		&& (targetType != MemorySegment.class)
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

		/* The struct (specified by GroupLayout) for MemorySegment corresponds to GroupLayout in terms of layout */
		if (targetType == MemorySegment.class) {
			if (!GroupLayout.class.isInstance(targetLayout)) {
				throw new IllegalArgumentException("GroupLayout is expected: layout = " + targetLayout); //$NON-NLS-1$
			}
		/* Check the void layout (null for void) and the void type */
		} else if (((targetType == void.class) && (targetLayout != null))
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
			/* The 16-bit layout size is shared by char and short
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
			if ((targetType != int.class) && (targetType != float.class)) {
				mismatchedSize = true;
			}
			break;
		case 64:
			/* The 64-bit layout size is shared by long, double and MemoryAddress
			 * given the corresponding pointer size for MemoryAddress is 64 bits in C.
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
		boolean kindAttrFound = false;
		List<String> layoutAttrList = targetLayout.attributes().toList();
		for (String attrStr : layoutAttrList) {
			if (attrStr.equalsIgnoreCase("abi/kind")) { //$NON-NLS-1$
				kindAttrFound = true;
				break;
			}
		}
		if (!kindAttrFound) {
			throw new IllegalArgumentException("The layout's ABI Class is undefined: layout = " + targetLayout); //$NON-NLS-1$
		}

		/* Extract the kind from the specified layout with the ATTR_NAME "abi/kind".
		 * e.g. b32[abi/kind=INT]
		 */
		boolean mismatchType = false;
		TypeKind kind = (TypeKind)targetLayout.attribute(TypeKind.ATTR_NAME)
				.orElseThrow(() -> new IllegalArgumentException("The layout's ABI class is empty")); //$NON-NLS-1$
		switch (kind) {
		case CHAR:
			/* the CHAR layout (8bits) in Java maps to bool or byte in C */
			break;
		case SHORT:
			/* the SHORT layout (16bits) in Java maps to char or short in C */
			break;
		case INT:
			/* the INT layout (32bits) in Java maps to int in C */
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
}
