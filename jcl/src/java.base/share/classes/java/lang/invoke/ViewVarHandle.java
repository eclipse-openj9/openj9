/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package java.lang.invoke;

abstract class ViewVarHandle extends VarHandle {
	ViewVarHandle(Class<?> fieldType, Class<?>[] coordinateTypes, MethodHandle[] handleTable, int modifiers) {
		super(fieldType, coordinateTypes, handleTable, modifiers);
	}
	
	@Override
	public boolean isAccessModeSupported(AccessMode accessMode) {
		switch (accessMode) {
		case GET:
		case GET_VOLATILE:
		case GET_OPAQUE:
		case GET_ACQUIRE:
		case SET:
		case SET_VOLATILE:
		case SET_OPAQUE:
		case SET_RELEASE:
			return true;
		case COMPARE_AND_SET:
		case COMPARE_AND_EXCHANGE_ACQUIRE:
		case COMPARE_AND_EXCHANGE_RELEASE:
		case COMPARE_AND_EXCHANGE:
		case WEAK_COMPARE_AND_SET:
		case WEAK_COMPARE_AND_SET_ACQUIRE:
		case WEAK_COMPARE_AND_SET_RELEASE:
		case WEAK_COMPARE_AND_SET_PLAIN:
		case GET_AND_SET:
		case GET_AND_SET_ACQUIRE:
		case GET_AND_SET_RELEASE:
			return ((char.class != fieldType) && (short.class != fieldType));
		case GET_AND_ADD:
		case GET_AND_ADD_ACQUIRE:
		case GET_AND_ADD_RELEASE:
		case GET_AND_BITWISE_AND:
		case GET_AND_BITWISE_AND_ACQUIRE:
		case GET_AND_BITWISE_AND_RELEASE:
		case GET_AND_BITWISE_OR:
		case GET_AND_BITWISE_OR_ACQUIRE:
		case GET_AND_BITWISE_OR_RELEASE:
		case GET_AND_BITWISE_XOR:
		case GET_AND_BITWISE_XOR_ACQUIRE:
		case GET_AND_BITWISE_XOR_RELEASE:
			return ((int.class == fieldType) || (long.class == fieldType));
		default:
			throw new InternalError("Invalid AccessMode"); //$NON-NLS-1$
		}
	}
	
	static class ViewVarHandleOperations extends VarHandleOperations {
		static final char convertEndian(char value) {
			return Character.reverseBytes(value);
		}

		static final double convertEndian(double value) {
			long bits = Double.doubleToRawLongBits(value);
			bits = Long.reverseBytes(bits);
			return Double.longBitsToDouble(bits);
		}

		static final float convertEndian(float value) {
			int bits = Float.floatToRawIntBits(value);
			bits = Integer.reverseBytes(bits);
			return Float.intBitsToFloat(bits);
		}

		static final int convertEndian(int value) {
			return Integer.reverseBytes(value);
		}

		static final long convertEndian(long value) {
			return Long.reverseBytes(value);
		}

		static final short convertEndian(short value) {
			return Short.reverseBytes(value);
		}
		
		static final void boundsCheck(int capacity, int viewTypeSize, int index) {
			if ((index < 0) || (index > (capacity - viewTypeSize))) {
				/*[MSG "K0621", "Index {0} is not within the bounds of the provided array of size {1}."]*/
				throw new ArrayIndexOutOfBoundsException(com.ibm.oti.util.Msg.getString("K0621", Integer.toString(index), Integer.toString(capacity))); //$NON-NLS-1$
			}
		}
		
		static final void alignmentCheck(long offset, int viewTypeSize, boolean allowUnaligned) {
			if ((!allowUnaligned) && ((offset % viewTypeSize) != 0)) {
				/*[MSG "K062A", "The requested access mode does not permit unaligned access."]*/
				throw new IllegalStateException(com.ibm.oti.util.Msg.getString("K062A")); //$NON-NLS-1$
			}
		}
	}
}
