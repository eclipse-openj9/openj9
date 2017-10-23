/*[INCLUDE-IF Sidecar16]*/

package java.lang;

/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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

import java.io.Serializable;

import java.util.Locale;
import java.util.Comparator;
import java.io.UnsupportedEncodingException;

import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;
import java.util.Formatter;
import java.util.StringJoiner;
import java.util.Iterator;
/*[IF Sidecar19-SE]*/
import jdk.internal.misc.Unsafe;
/*[ELSE]*/
import sun.misc.Unsafe;
/*[ENDIF]*/



import java.nio.charset.Charset;

/**
 * Strings are objects which represent immutable arrays of characters.
 *
 * @author OTI
 * @version initial
 *
 * @see StringBuffer
 */

public final class String implements Serializable, Comparable<String>, CharSequence {
	/*[IF]*/
	/* The STRING_OPT_IN_HW is false by default and only set to true by the JIT for platforms
	 * that support vectorized string operations (currently, zSphinx+ hardware).
	 */
	/*[ENDIF]*/
	// DO NOT CHANGE OR MOVE THIS LINE
	// IT MUST BE THE FIRST THING IN THE INITIALIZATION
	private static final boolean STRING_OPT_IN_HW = StrCheckHWAvailable();
	private static final long serialVersionUID = -6849794470754667710L;

	/**
	 * Determines whether String compression is enabled.
	 */
	static final boolean enableCompression = com.ibm.oti.vm.VM.J9_STRING_COMPRESSION_ENABLED;
	
	/*[IF Sidecar19-SE]*/
	/*[IF Java18.3]*/
	private final byte coder;
	/*[ENDIF]*/
	static final byte LATIN1 = 0;
	static final byte UTF16 = 1;
	static final boolean COMPACT_STRINGS;
	static {
 		COMPACT_STRINGS = enableCompression;
 	}

	/*[IF Sidecar19-SE]*/
	// returns UTF16 when COMPACT_STRINGS is false 
	byte coder() {
		if (enableCompression && count >= 0) {
			return LATIN1;
		} else {
			return UTF16;
		}
	}
	
	// no range checking, caller ensures bytes is in UTF16
	// coder is one of LATIN1 or UTF16
	void getBytes(byte[] bytes, int offset, byte coder) {
		int currentLength = lengthInternal();
		
		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			if (String.LATIN1 == coder) {
				compressedArrayCopy(value, 0, bytes, offset, currentLength);
			} else {
				decompress(value, 0, bytes, offset, currentLength);
			}
		} else {
			decompressedArrayCopy(value, 0, bytes, offset, currentLength);
		}
	}
	/*[ENDIF]*/
	
	static void checkIndex(int index, int length) {
		if ((0 <= index) && (index < length)) {
			return;
		}
		throw new StringIndexOutOfBoundsException("index="+index + " length="+length); //$NON-NLS-1$ //$NON-NLS-2$
	}

	static void checkOffset(int offset, int length) {
		if ((0 <= offset) && (offset <= length)) {
			return;
		}
		throw new StringIndexOutOfBoundsException("offset="+offset + " length="+length); //$NON-NLS-1$ //$NON-NLS-2$
	}
	/*[ENDIF]*/
	
	/**
	 * CaseInsensitiveComparator compares Strings ignoring the case of the characters.
	 */
	private static final class CaseInsensitiveComparator implements Comparator<String>, Serializable {
		static final long serialVersionUID = 8575799808933029326L;
		
		/**
		 * Compare the two objects to determine the relative ordering.
		 *
		 * @param o1
		 *			  an Object to compare
		 * @param o2
		 *			  an Object to compare
		 * @return an int < 0 if object1 is less than object2, 0 if they are equal, and > 0 if object1 is greater
		 *
		 * @exception ClassCastException
		 *					when objects are not the correct type
		 */
		public int compare(String o1, String o2) {
			return o1.compareToIgnoreCase(o2);
		}
	};

	/**
	 * A Comparator which compares Strings ignoring the case of the characters.
	 */
	public static final Comparator<String> CASE_INSENSITIVE_ORDER = new CaseInsensitiveComparator();
	
	/*[IF Sidecar19-SE]*/
	// Used to represent the value of an empty String
	private static final byte[] emptyValue = new byte[0];
	
	// Used to extract the value of a single ASCII character String by the integral value of the respective character as
	// an index into this table
	private static final byte[][] compressedAsciiTable;
	
	private static final byte[][] decompressedAsciiTable;
	/*[ELSE]*/
	// Used to represent the value of an empty String
	private static final char[] emptyValue = new char[0];
	
	// Used to extract the value of a single ASCII character String by the integral value of the respective character as
	// an index into this table
	private static final char[][] compressedAsciiTable;
	
	private static final char[][] decompressedAsciiTable;
	/*[ENDIF]*/

	// Used to access compression related helper methods
	private static final com.ibm.jit.JITHelpers helpers = com.ibm.jit.JITHelpers.getHelpers();

	static class StringCompressionFlag implements Serializable {
		private static final long serialVersionUID = 1346155847239551492L;
	}

	// Singleton used by all String instances to share the compression flag
	private static StringCompressionFlag compressionFlag;

	// Represents the bit in count field to test for whether this String backing array is not compressed
	// under String compression mode. This bit is not used when String compression is disabled.
	private static final int uncompressedBit = 0x80000000;

	private static String[] stringArray;
	private static final int stringArraySize = 10;

	private static class UnsafeHelpers {
		public final static long valueFieldOffset = getValueFieldOffset();

		static long getValueFieldOffset() {
			try {
				return Unsafe.getUnsafe().objectFieldOffset(String.class.getDeclaredField("value")); //$NON-NLS-1$
			} catch (NoSuchFieldException e) {
				throw new RuntimeException(e);
			}
		}
	}
	
	/**
	 * This is a System property to enable sharing of the underlying value array in {@link #String.substring(int)} and 
	 * {@link #String.substring(int, int)} if the offset is zero.
	 */
	static boolean enableSharingInSubstringWhenOffsetIsZero;

	/*[IF Sidecar19-SE]*/
	private final byte[] value;
	/*[ELSE]*/
	private final char[] value;
	/*[ENDIF]*/
	private final int count;
	private int hashCode;

	static {
		stringArray = new String[stringArraySize];

		/*[IF Sidecar19-SE]*/
		compressedAsciiTable = new byte[256][];
		/*[ELSE]*/
		compressedAsciiTable = new char[256][];
		/*[ENDIF]*/

		for (int i = 0; i < compressedAsciiTable.length; ++i) {
			/*[IF Sidecar19-SE]*/
			byte[] asciiValue = new byte[1];
			/*[ELSE]*/
			char[] asciiValue = new char[1];
			/*[ENDIF]*/
			
			helpers.putByteInArrayByIndex(asciiValue, 0, (byte) i);
			
			compressedAsciiTable[i] = asciiValue;
		}
		
		/*[IF Sidecar19-SE]*/
		decompressedAsciiTable = new byte[256][];
		/*[ELSE]*/
		decompressedAsciiTable = new char[256][];
		/*[ENDIF]*/
		
		for (int i = 0; i < decompressedAsciiTable.length; ++i) {
			/*[IF Sidecar19-SE]*/
			byte[] asciiValue = new byte[2];
			/*[ELSE]*/
			char[] asciiValue = new char[1];
			/*[ENDIF]*/
			
			helpers.putCharInArrayByIndex(asciiValue, 0, (char) i);
			
			decompressedAsciiTable[i] = asciiValue;
		}
	}

	static void initCompressionFlag() {
		if (compressionFlag == null) {
			compressionFlag = new StringCompressionFlag();
		}
	}

	static boolean compressible(char[] c, int start, int length) {
		for (int i = start; i < length; ++i) {
			if (c[i] > 255) {
				return false;
			}
		}

		return true;
	}
	
	static void compress(byte[] array1, int start1, byte[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putByteInArrayByIndex(array2, start2 + i, (byte) helpers.getCharFromArrayByIndex(array1, start1 + i));
		}
	}

	static void compress(char[] array1, int start1, byte[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putByteInArrayByIndex(array2, start2 + i, (byte) helpers.getCharFromArrayByIndex(array1, start1 + i));
		}
	}
	
	static void compress(byte[] array1, int start1, char[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putByteInArrayByIndex(array2, start2 + i, (byte) helpers.getCharFromArrayByIndex(array1, start1 + i));
		}
	}

	static void compress(char[] array1, int start1, char[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putByteInArrayByIndex(array2, start2 + i, (byte) helpers.getCharFromArrayByIndex(array1, start1 + i));
		}
	}
	
	static void decompress(byte[] array1, int start1, byte[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putCharInArrayByIndex(array2, start2 + i, helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(array1, start1 + i)));
		}
	}

	static void decompress(char[] array1, int start1, byte[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putCharInArrayByIndex(array2, start2 + i, helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(array1, start1 + i)));
		}
	}
	
	static void decompress(byte[] array1, int start1, char[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putCharInArrayByIndex(array2, start2 + i, helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(array1, start1 + i)));
		}
	}

	static void decompress(char[] array1, int start1, char[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putCharInArrayByIndex(array2, start2 + i, helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(array1, start1 + i)));
		}
	}
	
	static void compressedArrayCopy(byte[] array1, int start1, byte[] array2, int start2, int length) {
		if (array1 == array2 && start1 < start2) {
			for (int i = length - 1; i >= 0; --i) {
				helpers.putByteInArrayByIndex(array2, start2 + i, helpers.getByteFromArrayByIndex(array1, start1 + i));
			}
		} else {
			for (int i = 0; i < length; ++i) {
				helpers.putByteInArrayByIndex(array2, start2 + i, helpers.getByteFromArrayByIndex(array1, start1 + i));
			}
		}
	}

	static void compressedArrayCopy(byte[] array1, int start1, char[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putByteInArrayByIndex(array2, start2 + i, helpers.getByteFromArrayByIndex(array1, start1 + i));
		}
	}

	static void compressedArrayCopy(char[] array1, int start1, byte[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putByteInArrayByIndex(array2, start2 + i, helpers.getByteFromArrayByIndex(array1, start1 + i));
		}
	}
	
	static void compressedArrayCopy(char[] array1, int start1, char[] array2, int start2, int length) {
		if (array1 == array2 && start1 < start2) {
			for (int i = length - 1; i >= 0; --i) {
				helpers.putByteInArrayByIndex(array2, start2 + i, helpers.getByteFromArrayByIndex(array1, start1 + i));
			}
		} else {
			for (int i = 0; i < length; ++i) {
				helpers.putByteInArrayByIndex(array2, start2 + i, helpers.getByteFromArrayByIndex(array1, start1 + i));
			}
		}
	}
	
	/*[IF Sidecar19-SE]*/
	static void decompressedArrayCopy(byte[] array1, int start1, byte[] array2, int start2, int length) {
		if (array1 == array2 && start1 < start2) {
			for (int i = length - 1; i >= 0; --i) {
				helpers.putCharInArrayByIndex(array2, start2 + i, helpers.getCharFromArrayByIndex(array1, start1 + i));
			}
		} else {
			for (int i = 0; i < length; ++i) {
				helpers.putCharInArrayByIndex(array2, start2 + i, helpers.getCharFromArrayByIndex(array1, start1 + i));
			}
		}
	}

	static void decompressedArrayCopy(byte[] array1, int start1, char[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putCharInArrayByIndex(array2, start2 + i, helpers.getCharFromArrayByIndex(array1, start1 + i));
		}
	}

	static void decompressedArrayCopy(char[] array1, int start1, byte[] array2, int start2, int length) {
		for (int i = 0; i < length; ++i) {
			helpers.putCharInArrayByIndex(array2, start2 + i, helpers.getCharFromArrayByIndex(array1, start1 + i));
		}
	}
	
	static void decompressedArrayCopy(char[] array1, int start1, char[] array2, int start2, int length) {
		if (array1 == array2 && start1 < start2) {
			for (int i = length - 1; i >= 0; --i) {
				helpers.putCharInArrayByIndex(array2, start2 + i, helpers.getCharFromArrayByIndex(array1, start1 + i));
			}
		} else {
			for (int i = 0; i < length; ++i) {
				helpers.putCharInArrayByIndex(array2, start2 + i, helpers.getCharFromArrayByIndex(array1, start1 + i));
			}
		}
	}
	/*[ENDIF]*/
	
	boolean isCompressed() {
		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			return true;
		} else {
			return false;
		}
	}
	
	/*[IF Sidecar19-SE]*/
	String(byte[] byteArray, byte coder) {
		if (enableCompression) {
			if (String.LATIN1 == coder) {
				value = byteArray;
				count = byteArray.length;
			} else {
				value = byteArray;
				count = byteArray.length / 2 | uncompressedBit;
				
				initCompressionFlag();
			}
		} else {
			value = byteArray;
			count = byteArray.length / 2;
		}
		/*[IF Java18.3]*/
		this.coder = coder();
		/*[ENDIF]*/
	}

	static void checkBoundsOffCount(int offset, int count, int length) {
		if (offset >= 0 && count >= 0 && offset <= length - count) {
			return;
		}
		
		throw newStringIndexOutOfBoundsException(offset, count, length);
	}
	
	static private StringIndexOutOfBoundsException newStringIndexOutOfBoundsException(int offset, int count, int length) {
		return new StringIndexOutOfBoundsException("offset = " + offset + " count = " + count + " length = " + length); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
	}
	/*[ENDIF]*/

	/**
	 * Answers an empty string.
	 */
	public String() {
		value = emptyValue;
		count = 0;
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Converts the byte array to a String using the default encoding as specified by the file.encoding system property. If the system property is not
	 * defined, the default encoding is ISO8859_1 (ISO-Latin-1). If 8859-1 is not available, an ASCII encoding is used.
	 *
	 * @param data
	 *			  the byte array to convert to a String
	 *
	 * @throws NullPointerException
	 *				when data is null
	 *
	 * @see #getBytes()
	 * @see #getBytes(int, int, byte[], int)
	 * @see #getBytes(String)
	 * @see #valueOf(boolean)
	 * @see #valueOf(char)
	 * @see #valueOf(char[])
	 * @see #valueOf(char[], int, int)
	 * @see #valueOf(double)
	 * @see #valueOf(float)
	 * @see #valueOf(int)
	 * @see #valueOf(long)
	 * @see #valueOf(Object)
	 *
	 */
	public String(byte[] data) {
		this(data, 0, data.length);
	}

	/**
	 * Converts the byte array to a String, setting the high byte of every character to the specified value.
	 *
	 * @param data
	 *			  the byte array to convert to a String
	 * @param high
	 *			  the high byte to use
	 *
	 * @throws NullPointerException
	 *				when data is null
	 *
	 * @deprecated Use String(byte[]) or String(byte[], String) instead
	 */
	/*[IF Sidecar19-SE]*/
	@Deprecated(forRemoval=false, since="1.1")
	/*[ELSE]*/
	@Deprecated
	/*[ENDIF]*/
	public String(byte[] data, int high) {
		this(data, high, 0, data.length);
	}

	/**
	 * Converts the byte array to a String using the default encoding as specified by the file.encoding system property. If the system property is not
	 * defined, the default encoding is ISO8859_1 (ISO-Latin-1). If 8859-1 is not available, an ASCII encoding is used.
	 *
	 * @param data
	 *			  the byte array to convert to a String
	 * @param start
	 *			  the starting offset in the byte array
	 * @param length
	 *			  the number of bytes to convert
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>length < 0, start < 0</code> or <code>start + length > data.length</code>
	 * @throws NullPointerException
	 *				when data is null
	 *
	 * @see #getBytes()
	 * @see #getBytes(int, int, byte[], int)
	 * @see #getBytes(String)
	 * @see #valueOf(boolean)
	 * @see #valueOf(char)
	 * @see #valueOf(char[])
	 * @see #valueOf(char[], int, int)
	 * @see #valueOf(double)
	 * @see #valueOf(float)
	 * @see #valueOf(int)
	 * @see #valueOf(long)
	 * @see #valueOf(Object)
	 *
	 */
	public String(byte[] data, int start, int length) {
		data.getClass(); // Implicit null check
		
		if (start >= 0 && 0 <= length && length <= data.length - start) {
			/*[IF Sidecar19-SE]*/
			StringCoding.Result scResult = StringCoding.decode(data, start, length);
			
			if (enableCompression) {
				if (String.LATIN1 == scResult.coder) {
					value = scResult.value;
					count = scResult.value.length;
				} else {
					value = scResult.value;
					count = scResult.value.length / 2 | uncompressedBit;
					
					initCompressionFlag();
				}
			} else {
				value = scResult.value;
				count = scResult.value.length / 2;
			}
			/*[ELSE]*/
			char[] buffer = StringCoding.decode(data, start, length);
			
			if (enableCompression) {
				if (compressible(buffer, 0, buffer.length)) {
					value = new char[(buffer.length + 1) / 2];
					count = buffer.length;
					
					compress(buffer, 0, value, 0, buffer.length);
				} else {
					value = buffer;
					count = buffer.length | uncompressedBit;
					
					initCompressionFlag();
				}
			} else {
				value = buffer;
				count = buffer.length;
			}
			/*[ENDIF]*/
		} else {
			throw new StringIndexOutOfBoundsException();
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Converts the byte array to a String, setting the high byte of every character to the specified value.
	 *
	 * @param data
	 *			  the byte array to convert to a String
	 * @param high
	 *			  the high byte to use
	 * @param start
	 *			  the starting offset in the byte array
	 * @param length
	 *			  the number of bytes to convert
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>length < 0, start < 0</code> or <code>start + length > data.length</code>
	 * @throws NullPointerException
	 *				when data is null
	 *
	 * @deprecated Use String(byte[], int, int) instead
	 */
	/*[IF Sidecar19-SE]*/
	@Deprecated(forRemoval=false, since="1.1")
	/*[ELSE]*/
	@Deprecated
	/*[ENDIF]*/
	public String(byte[] data, int high, int start, int length) {
		data.getClass(); // Implicit null check

		if (start >= 0 && 0 <= length && length <= data.length - start) {
			if (enableCompression) {
				if (high == 0) {
					/*[IF Sidecar19-SE]*/
					value = new byte[length];
					/*[ELSE]*/
					value = new char[(length + 1) / 2];
					/*[ENDIF]*/
					count = length;
					
					compressedArrayCopy(data, start, value, 0, length);
				} else {
					/*[IF Sidecar19-SE]*/
					value = new byte[length * 2];
					/*[ELSE]*/
					value = new char[length];
					/*[ENDIF]*/
					count = length | uncompressedBit;
					
					high <<= 8;

					for (int i = 0; i < length; ++i) {
						/*[IF Sidecar19-SE]*/
						helpers.putCharInArrayByIndex(value, i, (char) (high + (data[start++] & 0xff)));
						/*[ELSE]*/
						value[i] = (char) (high + (data[start++] & 0xff));
						/*[ENDIF]*/
					}

					initCompressionFlag();
				}
				
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[length * 2];
				/*[ELSE]*/
				value = new char[length];
				/*[ENDIF]*/
				count = length;

				high <<= 8;

				for (int i = 0; i < length; ++i) {
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, i, (char) (high + (data[start++] & 0xff)));
					/*[ELSE]*/
					value[i] = (char) (high + (data[start++] & 0xff));
					/*[ENDIF]*/
				}
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Converts the byte array to a String using the specified encoding.
	 *
	 * @param data
	 *			  the byte array to convert to a String
	 * @param start
	 *			  the starting offset in the byte array
	 * @param length
	 *			  the number of bytes to convert
	 * @param encoding
	 *			  the encoding
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>length < 0, start < 0</code> or <code>start + length > data.length</code>
	 * @throws UnsupportedEncodingException
	 *				when encoding is not supported
	 * @throws NullPointerException
	 *				when data is null
	 *
	 * @see #getBytes()
	 * @see #getBytes(int, int, byte[], int)
	 * @see #getBytes(String)
	 * @see #valueOf(boolean)
	 * @see #valueOf(char)
	 * @see #valueOf(char[])
	 * @see #valueOf(char[], int, int)
	 * @see #valueOf(double)
	 * @see #valueOf(float)
	 * @see #valueOf(int)
	 * @see #valueOf(long)
	 * @see #valueOf(Object)
	 * @see UnsupportedEncodingException
	 */
	public String(byte[] data, int start, int length, final String encoding) throws UnsupportedEncodingException {
		encoding.getClass(); // Implicit null check
		
		data.getClass(); // Implicit null check

		if (start >= 0 && 0 <= length && length <= data.length - start) {
			/*[IF Sidecar19-SE]*/
			StringCoding.Result scResult = StringCoding.decode(encoding, data, start, length);
			
			if (enableCompression) {
				if (String.LATIN1 == scResult.coder) {
					value = scResult.value;
					count = scResult.value.length;
				} else {
					value = scResult.value;
					count = scResult.value.length / 2 | uncompressedBit;
					
					initCompressionFlag();
				}
			} else {
				value = scResult.value;
				count = scResult.value.length / 2;
			}
			/*[ELSE]*/
			char[] buffer = StringCoding.decode(encoding, data, start, length);
			
			if (enableCompression) {
				if (compressible(buffer, 0, buffer.length)) {
					value = new char[(buffer.length + 1) / 2];
					count = buffer.length;
					
					compress(buffer, 0, value, 0, buffer.length);
				} else {
					value = buffer;
					count = buffer.length | uncompressedBit;
					
					initCompressionFlag();
				}
			} else {
				value = buffer;
				count = buffer.length;
			}
			/*[ENDIF]*/
		} else {
			throw new StringIndexOutOfBoundsException();
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Converts the byte array to a String using the specified encoding.
	 *
	 * @param data
	 *			  the byte array to convert to a String
	 * @param encoding
	 *			  the encoding
	 *
	 * @throws UnsupportedEncodingException
	 *				when encoding is not supported
	 * @throws NullPointerException
	 *				when data is null
	 *
	 * @see #getBytes()
	 * @see #getBytes(int, int, byte[], int)
	 * @see #getBytes(String)
	 * @see #valueOf(boolean)
	 * @see #valueOf(char)
	 * @see #valueOf(char[])
	 * @see #valueOf(char[], int, int)
	 * @see #valueOf(double)
	 * @see #valueOf(float)
	 * @see #valueOf(int)
	 * @see #valueOf(long)
	 * @see #valueOf(Object)
	 * @see UnsupportedEncodingException
	 */
	public String(byte[] data, String encoding) throws UnsupportedEncodingException {
		this(data, 0, data.length, encoding);
	}
	
	/*[PR 92799] for JIT optimization */
	private String(String s, char c) {
		if (s == null) {
			s = "null"; //$NON-NLS-1$
		}
		
		int slen = s.lengthInternal();
		
		int concatlen = slen + 1;

		if (enableCompression) {
			// Check if the String is compressed
			if (s.count >= 0 && c <= 255) {
				/*[IF Sidecar19-SE]*/
				value = new byte[concatlen];
				/*[ELSE]*/
				value = new char[(concatlen + 1) / 2];
				/*[ENDIF]*/
				count = concatlen;
				
				compressedArrayCopy(s.value, 0, value, 0, slen);
				
				helpers.putByteInArrayByIndex(value, slen, (byte) c);
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[concatlen * 2];
				count = concatlen | uncompressedBit;
				
				decompressedArrayCopy(s.value, 0, value, 0, slen);
				
				helpers.putCharInArrayByIndex(value, slen, c);
				/*[ELSE]*/
				value = new char[concatlen];
				count = (concatlen) | uncompressedBit;
				
				System.arraycopy(s.value, 0, value, 0, slen);
				
				value[slen] = c;
				/*[ENDIF]*/
				
				initCompressionFlag();
			}
		} else {
			/*[IF Sidecar19-SE]*/
			value = new byte[concatlen * 2];
			count = concatlen;
			
			decompressedArrayCopy(s.value, 0, value, 0, slen);
			
			helpers.putCharInArrayByIndex(value, slen, c);
			/*[ELSE]*/
			value = new char[concatlen];
			count = concatlen;
			
			System.arraycopy(s.value, 0, value, 0, slen);
			
			value[slen] = c;
			/*[ENDIF]*/
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Initializes this String to contain the characters in the specified character array. Modifying the character array after creating the String has
	 * no effect on the String.
	 *
	 * @param data
	 *			  the array of characters
	 *
	 * @throws NullPointerException
	 *				when data is null
	 */
	public String(char[] data) {
		this(data, 0, data.length);
	}

	/**
	 * Initializes this String to use the specified character array. The character array should not be modified after the String is created.
	 *
	 * @param data
	 *			  a non-null array of characters
	 */
	String(char[] data, boolean ignore) {
		if (enableCompression) {
			if (compressible(data, 0, data.length)) {
				/*[IF Sidecar19-SE]*/
				value = new byte[data.length];
				/*[ELSE]*/
				value = new char[(data.length + 1) / 2];
				/*[ENDIF]*/
				count = data.length;
				
				compress(data, 0, value, 0, data.length);
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[data.length * 2];
				count = data.length | uncompressedBit;
				
				decompressedArrayCopy(data, 0, value, 0, data.length);
				/*[ELSE]*/
				value = new char[data.length];
				count = data.length | uncompressedBit;
				
				System.arraycopy(data, 0, value, 0, data.length);
				/*[ENDIF]*/
				
				initCompressionFlag();
			}
		} else {
			/*[IF Sidecar19-SE]*/
			value = new byte[data.length * 2];
			count = data.length;
			
			decompressedArrayCopy(data, 0, value, 0, data.length);
			/*[ELSE]*/
			value = new char[data.length];
			count = data.length;
			
			System.arraycopy(data, 0, value, 0, data.length);
			/*[ENDIF]*/
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Initializes this String to contain the specified characters in the character array. Modifying the character array after creating the String has
	 * no effect on the String.
	 *
	 * @param data
	 *			  the array of characters
	 * @param start
	 *			  the starting offset in the character array
	 * @param length
	 *			  the number of characters to use
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>length < 0, start < 0</code> or <code>start + length > data.length</code>
	 * @throws NullPointerException
	 *				when data is null
	 */
	public String(char[] data, int start, int length) {
		if (start >= 0 && 0 <= length && length <= data.length - start) {
			if (enableCompression) {
				if (compressible(data, start, length)) {
					/*[IF Sidecar19-SE]*/
					value = new byte[length];
					/*[ELSE]*/
					value = new char[(length + 1) / 2];
					/*[ENDIF]*/
					count = length;
					
					compress(data, start, value, 0, length);
				} else {
					/*[IF Sidecar19-SE]*/
					value = new byte[length * 2];
					count = length | uncompressedBit;
					
					decompressedArrayCopy(data, start, value, 0, length);
					/*[ELSE]*/
					value = new char[length];
					count = length | uncompressedBit;
					
					System.arraycopy(data, start, value, 0, length);
					/*[ENDIF]*/
					
					initCompressionFlag();
				}
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[length * 2];
				count = length;
				
				decompressedArrayCopy(data, start, value, 0, length);
				/*[ELSE]*/
				value = new char[length];
				count = length;
				
				System.arraycopy(data, start, value, 0, length);
				/*[ENDIF]*/
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/*[IF Sidecar19-SE]*/
	String(byte[] data, int start, int length, boolean compressed) {
	/*[ELSE]*/
	String(char[] data, int start, int length, boolean compressed) {
	/*[ENDIF]*/
		if (enableCompression) {
			if (length == 0) {
				value = emptyValue;
				count = 0;
			} else if (length == 1) {
				if (compressed) {
					char theChar = helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(data, start));
					
					value = compressedAsciiTable[theChar];
					count = 1;
					
					hashCode = theChar;
				} else {
					/*[IF]*/
					// TODO : We should check if start == 0 and not allocate here as well. See the compression disabled path.
					/*[ENDIF]*/
					
					/*[IF Sidecar19-SE]*/
					char theChar = helpers.getCharFromArrayByIndex(data, start);
					/*[ELSE]*/
					char theChar = data[start];
					/*[ENDIF]*/
					
					if (theChar <= 255) {
						value = decompressedAsciiTable[theChar];
					} else {
						/*[IF Sidecar19-SE]*/
						value = new byte[2];
						
						helpers.putCharInArrayByIndex(value, 0, theChar);
						/*[ELSE]*/
						value = new char[] { theChar };
						/*[ENDIF]*/
					}
					
					count = 1 | uncompressedBit;
					
					hashCode = theChar;
					
					initCompressionFlag();
				}
			} else {
				if (compressed) {
					if (start == 0) {
						value = data;
						count = length;
					} else {
						/*[IF Sidecar19-SE]*/
						value = new byte[length];
						/*[ELSE]*/
						value = new char[(length + 1) / 2];
						/*[ENDIF]*/
						count = length;
						
						compressedArrayCopy(data, start, value, 0, length);
					}
				} else {
					if (start == 0) {
						value = data;
						count = length | uncompressedBit;
					} else {
						/*[IF Sidecar19-SE]*/
						value = new byte[length * 2];
						count = length | uncompressedBit;
						
						decompressedArrayCopy(data, start, value, 0, length);
						/*[ELSE]*/
						value = new char[length];
						count = length | uncompressedBit;
						
						System.arraycopy(data, start, value, 0, length);
						/*[ENDIF]*/
					}

					initCompressionFlag();
				}
			}
		} else {
			if (length == 0) {
				value = emptyValue;
				count = 0;
				/*[IF Sidecar19-SE]*/
			} else if (length == 1 && helpers.getCharFromArrayByIndex(data, start) <= 255) {
				char theChar = helpers.getCharFromArrayByIndex(data, start);
				/*[ELSE]*/
			} else if (length == 1 && data[start] < 256) {
				char theChar = data[start];
				/*[ENDIF]*/
				
				value = decompressedAsciiTable[theChar];
				count = 1;
				
				hashCode = theChar;
			} else {
				if (start == 0) {
					value = data;
					count = length;
				} else {
					/*[IF Sidecar19-SE]*/
					value = new byte[length * 2];
					count = length;
					
					decompressedArrayCopy(data, start, value, 0, length);
					/*[ELSE]*/
					value = new char[length];
					count = length;
					
					System.arraycopy(data, start, value, 0, length);
					/*[ENDIF]*/
				}
			}
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}
	
	/*[IF Sidecar19-SE]*/
	String(byte[] data, int start, int length, boolean compressed, boolean sharingIsAllowed) {
	/*[ELSE]*/
	String(char[] data, int start, int length, boolean compressed, boolean sharingIsAllowed) {
	/*[ENDIF]*/
		if (enableCompression) {
			if (length == 0) {
				value = emptyValue;
				count = 0;
			} else if (length == 1) {
				if (compressed) {
					char theChar = helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(data, start));
					
					value = compressedAsciiTable[theChar];
					count = 1;
					
					hashCode = theChar;
				} else {
					/*[IF Sidecar19-SE]*/
					char theChar = helpers.getCharFromArrayByIndex(data, start);
					/*[ELSE]*/
					char theChar = data[start];
					/*[ENDIF]*/
					
					if (theChar <= 255) {
						value = decompressedAsciiTable[theChar];
					} else {
						/*[IF Sidecar19-SE]*/
						value = new byte[2];
						
						helpers.putCharInArrayByIndex(value, 0, theChar);
						/*[ELSE]*/
						value = new char[] { theChar };
						/*[ENDIF]*/
					}
					
					count = 1 | uncompressedBit;
					
					hashCode = theChar;
					
					initCompressionFlag();
				}
			} else {
				if (compressed) {
					if (start == 0 && sharingIsAllowed) {
						value = data;
						count = length;
					} else {
						/*[IF Sidecar19-SE]*/
						value = new byte[length];
						/*[ELSE]*/
						value = new char[(length + 1) / 2];
						/*[ENDIF]*/
						count = length;
						
						compressedArrayCopy(data, start, value, 0, length);
					}
				} else {
					if (start == 0 && sharingIsAllowed) {
						value = data;
						count = length | uncompressedBit;
					} else {
						/*[IF Sidecar19-SE]*/
						value = new byte[length * 2];
						count = length | uncompressedBit;
						
						decompressedArrayCopy(data, start, value, 0, length);
						/*[ELSE]*/
						value = new char[length];
						count = length | uncompressedBit;
						
						System.arraycopy(data, start, value, 0, length);
						/*[ENDIF]*/
					}

					initCompressionFlag();
				}
			}
		} else {
			if (length == 0) {
				value = emptyValue;
				count = 0;
				/*[IF Sidecar19-SE]*/
			} else if (length == 1 && helpers.getCharFromArrayByIndex(data, start) <= 255) {
				char theChar = helpers.getCharFromArrayByIndex(data, start);
				/*[ELSE]*/
			} else if (length == 1 && data[start] <= 255) {
				char theChar = data[start];
				/*[ENDIF]*/
				
				value = decompressedAsciiTable[theChar];
				count = 1;
				
				hashCode = theChar;
			} else {
				if (start == 0 && sharingIsAllowed) {
					value = data;
					count = length;
				} else {
					/*[IF Sidecar19-SE]*/
					value = new byte[length * 2];
					count = length;
					
					decompressedArrayCopy(data, start, value, 0, length);
					/*[ELSE]*/
					value = new char[length];
					count = length;
					
					System.arraycopy(data, start, value, 0, length);
					/*[ENDIF]*/
				}
			}
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Creates a string that is a copy of another string
	 *
	 * @param string
	 *			  the String to copy
	 */
	public String(String string) {
		value = string.value;
		count = string.count;
		hashCode = string.hashCode;
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Creates a new string from the with specified length
	 *
	 * @param numChars
	 *			  length of new string
	 */
	private String(int numChars) {
		if (enableCompression) {
			/*[IF Sidecar19-SE]*/
			value = new byte[numChars * 2];
			/*[ELSE]*/
			value = new char[numChars];
			/*[ENDIF]*/
			count = numChars | uncompressedBit;
		} else {
			/*[IF Sidecar19-SE]*/
			value = new byte[numChars * 2];
			/*[ELSE]*/
			value = new char[numChars];
			/*[ENDIF]*/
			count = numChars;
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Creates a string from the contents of a StringBuffer.
	 *
	 * @param buffer
	 *			  the StringBuffer
	 */
	public String(StringBuffer buffer) {
		synchronized (buffer) {
			/*[IF Sidecar19-SE]*/
			byte[] chars = buffer.shareValue();
			/*[ELSE]*/
			char[] chars = buffer.shareValue();
			/*[ENDIF]*/

			if (enableCompression) {
				if (buffer.isCompressed()) {
					value = chars;
					count = buffer.length();
				} else {
					value = chars;
					count = buffer.length() | uncompressedBit;
					
					initCompressionFlag();
				}
			} else {
				value = chars;
				count = buffer.length();
			}
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/*
	 * Creates a string that is s1 + s2.
	 */
	private String(String s1, String s2) {
		if (s1 == null) {
			s1 = "null"; //$NON-NLS-1$
		}

		if (s2 == null) {
			s2 = "null"; //$NON-NLS-1$
		}

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		int concatlen = s1len + s2len;

		if (enableCompression) {
			if ((s1.count | s2.count) >= 0) {
				/*[IF Sidecar19-SE]*/
				value = new byte[concatlen];
				/*[ELSE]*/
				value = new char[(concatlen + 1) / 2];
				/*[ENDIF]*/
				count = concatlen;

				compressedArrayCopy(s1.value, 0, value, 0, s1len);
				compressedArrayCopy(s2.value, 0, value, s1len, s2len);
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[concatlen * 2];
				/*[ELSE]*/
				value = new char[concatlen];
				/*[ENDIF]*/
				count = concatlen | uncompressedBit;

				// Check if the String is compressed
				if (s1.count >= 0) {
					decompress(s1.value, 0, value, 0, s1len);
				} else {
					/*[IF Sidecar19-SE]*/
					decompressedArrayCopy(s1.value, 0, value, 0, s1len);
					/*[ELSE]*/
					System.arraycopy(s1.value, 0, value, 0, s1len);
					/*[ENDIF]*/
				}

				// Check if the String is compressed
				if (s2.count >= 0) {
					decompress(s2.value, 0, value, s1len, s2len);
				} else {
					/*[IF Sidecar19-SE]*/
					decompressedArrayCopy(s2.value, 0, value, s1len, s2len);
					/*[ELSE]*/
					System.arraycopy(s2.value, 0, value, s1len, s2len);
					/*[ENDIF]*/
				}
				
				initCompressionFlag();
			}
		} else {
			/*[IF Sidecar19-SE]*/
			value = new byte[concatlen * 2];
			count = concatlen;

			decompressedArrayCopy(s1.value, 0, value, 0, s1len);
			decompressedArrayCopy(s2.value, 0, value, s1len, s2len);
			/*[ELSE]*/
			value = new char[concatlen];
			count = concatlen;

			System.arraycopy(s1.value, 0, value, 0, s1len);
			System.arraycopy(s2.value, 0, value, s1len, s2len);
			/*[ENDIF]*/
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/*
	 * Creates a string that is s1 + s2 + s3.
	 */
	private String(String s1, String s2, String s3) {
		if (s1 == null) {
			s1 = "null"; //$NON-NLS-1$
		}

		if (s2 == null) {
			s2 = "null"; //$NON-NLS-1$
		}

		if (s3 == null) {
			s3 = "null"; //$NON-NLS-1$
		}

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();
		int s3len = s3.lengthInternal();

		int concatlen = s1len + s2len + s3len;
		
		if (enableCompression) {
			if ((s1.count | s2.count | s3.count) >= 0) {
				/*[IF Sidecar19-SE]*/
				value = new byte[concatlen];
				/*[ELSE]*/
				value = new char[(concatlen + 1) / 2];
				/*[ENDIF]*/
				count = concatlen;

				compressedArrayCopy(s1.value, 0, value, 0, s1len);
				compressedArrayCopy(s2.value, 0, value, s1len, s2len);
				compressedArrayCopy(s3.value, 0, value, s1len + s2len, s3len);
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[concatlen * 2];
				/*[ELSE]*/
				value = new char[concatlen];
				/*[ENDIF]*/
				count = concatlen | uncompressedBit;

				// Check if the String is compressed
				if (s1.count >= 0) {
					decompress(s1.value, 0, value, 0, s1len);
				} else {
					/*[IF Sidecar19-SE]*/
					decompressedArrayCopy(s1.value, 0, value, 0, s1len);
					/*[ELSE]*/
					System.arraycopy(s1.value, 0, value, 0, s1len);
					/*[ENDIF]*/
				}

				// Check if the String is compressed
				if (s2.count >= 0) {
					decompress(s2.value, 0, value, s1len, s2len);
				} else {
					/*[IF Sidecar19-SE]*/
					decompressedArrayCopy(s2.value, 0, value, s1len, s2len);
					/*[ELSE]*/
					System.arraycopy(s2.value, 0, value, s1len, s2len);
					/*[ENDIF]*/
				}

				// Check if the String is compressed
				if (s3.count >= 0) {
					decompress(s3.value, 0, value, s1len + s2len, s3len);
				} else {
					/*[IF Sidecar19-SE]*/
					decompressedArrayCopy(s3.value, 0, value, (s1len + s2len), s3len);
					/*[ELSE]*/
					System.arraycopy(s3.value, 0, value, (s1len + s2len), s3len);
					/*[ENDIF]*/
				}
				
				initCompressionFlag();
			}
		} else {
			/*[IF Sidecar19-SE]*/
			value = new byte[concatlen * 2];
			/*[ELSE]*/
			value = new char[concatlen];
			/*[ENDIF]*/
			count = concatlen;

			/*[IF Sidecar19-SE]*/
			decompressedArrayCopy(s1.value, 0, value, 0, s1len);
			decompressedArrayCopy(s2.value, 0, value, s1len, s2len);
			decompressedArrayCopy(s3.value, 0, value, (s1len + s2len), s3len);
			/*[ELSE]*/
			System.arraycopy(s1.value, 0, value, 0, s1len);
			System.arraycopy(s2.value, 0, value, s1len, s2len);
			System.arraycopy(s3.value, 0, value, (s1len + s2len), s3len);
			/*[ENDIF]*/
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/*
	 * Creates a string that is s1 + v1.
	 */
	private String(String s1, int v1) {
		if (s1 == null) {
			s1 = "null"; //$NON-NLS-1$
		}

		// Char length of all the parameters respectively
		int s1len = s1.lengthInternal();
		int v1len = 1;

		int quot;
		int i = v1;
		while ((i /= 10) != 0)
			v1len++;
		if (v1 >= 0) {
			quot = -v1;
		} else {
			// Leave room for '-'
			v1len++;
			quot = v1;
		}

		// Char length of the final String
		int len = s1len + v1len;
		
		if (enableCompression) {
			// Check if the String is compressed
			if (s1.count >= 0) {
				/*[IF Sidecar19-SE]*/
				value = new byte[len];
				/*[ELSE]*/
				value = new char[(len + 1) / 2];
				/*[ENDIF]*/
				count = len;

				// Copy in v1
				int index = len - 1;

				do {
					int res = quot / 10;
					int rem = quot - (res * 10);

					quot = res;

					// Write the digit into the correct position
					helpers.putByteInArrayByIndex(value, index--, (byte) ('0' - rem));
				} while (quot != 0);

				if (v1 < 0) {
					helpers.putByteInArrayByIndex(value, index, (byte) '-');
				}

				// Copy in s1 contents
				compressedArrayCopy(s1.value, 0, value, 0, s1len);
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[len * 2];
				/*[ELSE]*/
				value = new char[len];
				/*[ENDIF]*/
				count = len | uncompressedBit;

				// Copy in v1
				int index = len - 1;

				do {
					int res = quot / 10;
					int rem = quot - (res * 10);

					quot = res;

					// Write the digit into the correct position
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, index--, (char) ('0' - rem));
					/*[ELSE]*/
					value[index--] = (char) ('0' - rem);
					/*[ENDIF]*/
				} while (quot != 0);

				if (v1 < 0) {
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, index, (char) '-');
					/*[ELSE]*/
					value[index] = '-';
					/*[ENDIF]*/
				}

				// Copy in s1 contents
				/*[IF Sidecar19-SE]*/
				decompressedArrayCopy(s1.value, 0, value, 0, s1len);
				/*[ELSE]*/
				System.arraycopy(s1.value, 0, value, 0, s1len);
				/*[ENDIF]*/
				
				initCompressionFlag();
			}
		} else {
			/*[IF Sidecar19-SE]*/
			value = new byte[len * 2];
			/*[ELSE]*/
			value = new char[len];
			/*[ENDIF]*/
			count = len;

			// Copy in v1
			int index = len - 1;

			do {
				int res = quot / 10;
				int rem = quot - (res * 10);

				quot = res;

				// Write the digit into the correct position
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, index--, (char) ('0' - rem));
				/*[ELSE]*/
				value[index--] = (char) ('0' - rem);
				/*[ENDIF]*/
			} while (quot != 0);

			if (v1 < 0) {
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, index, (char) '-');
				/*[ELSE]*/
				value[index] = '-';
				/*[ENDIF]*/
			}

			// Copy in s1 contents
			/*[IF Sidecar19-SE]*/
			decompressedArrayCopy(s1.value, 0, value, 0, s1len);
			/*[ELSE]*/
			System.arraycopy(s1.value, 0, value, 0, s1len);
			/*[ENDIF]*/
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/*
	 * Creates a string that is v1 + s1 + v2 + s2 + s3.
	 */
	private String(int v1, String s1, int v2, String s2, String s3) {
		if (s1 == null) {
			s1 = "null"; //$NON-NLS-1$
		}

		if (s2 == null) {
			s2 = "null"; //$NON-NLS-1$
		}

		if (s3 == null) {
			s3 = "null"; //$NON-NLS-1$
		}

		// Char length of all the parameters respectively
		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();
		int s3len = s3.lengthInternal();

		int v1len = 1;
		int v2len = 1;

		int quot1;
		int i1 = v1;
		while ((i1 /= 10) != 0)
			v1len++;
		if (v1 >= 0) {
			quot1 = -v1;
		} else {
			// Leave room for '-'
			v1len++;
			quot1 = v1;
		}

		int quot2;
		int i2 = v2;
		while ((i2 /= 10) != 0)
			v2len++;
		if (v2 >= 0) {
			quot2 = -v2;
		} else {
			// Leave room for '-'
			v2len++;
			quot2 = v2;
		}

		// Char length of the final String
		int len = s1len + v1len + v2len + s2len + s3len;

		if (enableCompression) {
			if ((s1.count | s2.count | s3.count) >= 0) {
				/*[IF Sidecar19-SE]*/
				value = new byte[len];
				/*[ELSE]*/
				value = new char[(len + 1) / 2];
				/*[ENDIF]*/
				count = len;

				int start = len;

				// Copy in s3 contents
				start = start - s3len;
				compressedArrayCopy(s3.value, 0, value, start, s3len);

				// Copy in s2 contents
				start = start - s2len;
				compressedArrayCopy(s2.value, 0, value, start, s2len);

				// Copy in v2
				int index2 = start - 1;

				do {
					int res = quot2 / 10;
					int rem = quot2 - (res * 10);

					quot2 = res;

					// Write the digit into the correct position
					helpers.putByteInArrayByIndex(value, index2--, (byte) ('0' - rem));
				} while (quot2 != 0);

				if (v2 < 0) {
					helpers.putByteInArrayByIndex(value, index2--, (byte) '-');
				}

				// Copy in s1 contents
				start = index2 + 1 - s1len;
				compressedArrayCopy(s1.value, 0, value, start, s1len);

				// Copy in v1
				int index1 = start - 1;

				do {
					int res = quot1 / 10;
					int rem = quot1 - (res * 10);

					quot1 = res;

					// Write the digit into the correct position
					helpers.putByteInArrayByIndex(value, index1--, (byte) ('0' - rem));
				} while (quot1 != 0);

				if (v1 < 0) {
					helpers.putByteInArrayByIndex(value, index1--, (byte) '-');
				}
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[len * 2];
				/*[ELSE]*/
				value = new char[len];
				/*[ENDIF]*/
				count = len | uncompressedBit;

				int start = len;

				// Copy in s3 contents
				start = start - s3len;
				
				// Check if the String is compressed
				if (s3.count >= 0) {
					decompress(s3.value, 0, value, start, s3len);
				} else {
					/*[IF Sidecar19-SE]*/
					decompressedArrayCopy(s3.value, 0, value, start, s3len);
					/*[ELSE]*/
					System.arraycopy(s3.value, 0, value, start, s3len);
					/*[ENDIF]*/
				}

				// Copy in s2 contents
				start = start - s2len;
				
				// Check if the String is compressed
				if (s2.count >= 0) {
					decompress(s2.value, 0, value, start, s2len);
				} else {
					/*[IF Sidecar19-SE]*/
					decompressedArrayCopy(s2.value, 0, value, start, s2len);
					/*[ELSE]*/
					System.arraycopy(s2.value, 0, value, start, s2len);
					/*[ENDIF]*/
				}

				// Copy in v2
				int index2 = start - 1;

				do {
					int res = quot2 / 10;
					int rem = quot2 - (res * 10);

					quot2 = res;

					// Write the digit into the correct position
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, index2--, (char) ('0' - rem));
					/*[ELSE]*/
					value[index2--] = (char) ('0' - rem);
					/*[ENDIF]*/
				} while (quot2 != 0);

				if (v2 < 0) {
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, index2--, (char) '-');
					/*[ELSE]*/
					value[index2--] = '-';
					/*[ENDIF]*/
				}

				// Copy in s1 contents
				start = index2 + 1 - s1len;
				
				// Check if the String is compressed
				if (s1.count >= 0) {
					decompress(s1.value, 0, value, start, s1len);
				} else {
					/*[IF Sidecar19-SE]*/
					decompressedArrayCopy(s1.value, 0, value, start, s1len);
					/*[ELSE]*/
					System.arraycopy(s1.value, 0, value, start, s1len);
					/*[ENDIF]*/
				}

				// Copy in v1
				int index1 = start - 1;

				do {
					int res = quot1 / 10;
					int rem = quot1 - (res * 10);

					quot1 = res;

					// Write the digit into the correct position
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, index1--, (char) ('0' - rem));
					/*[ELSE]*/
					value[index1--] = (char) ('0' - rem);
					/*[ENDIF]*/
				} while (quot1 != 0);

				if (v1 < 0) {
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, index1--, (char) '-');
					/*[ELSE]*/
					value[index1--] = '-';
					/*[ENDIF]*/
				}
				
				initCompressionFlag();
			}
		} else {
			/*[IF Sidecar19-SE]*/
			value = new byte[len * 2];
			/*[ELSE]*/
			value = new char[len];
			/*[ENDIF]*/
			count = len;

			int start = len;

			// Copy in s3 contents
			start = start - s3len;
			/*[IF Sidecar19-SE]*/
			decompressedArrayCopy(s3.value, 0, value, start, s3len);
			/*[ELSE]*/
			System.arraycopy(s3.value, 0, value, start, s3len);
			/*[ENDIF]*/

			// Copy in s2 contents
			start = start - s2len;
			/*[IF Sidecar19-SE]*/
			decompressedArrayCopy(s2.value, 0, value, start, s2len);
			/*[ELSE]*/
			System.arraycopy(s2.value, 0, value, start, s2len);
			/*[ENDIF]*/

			// Copy in v2
			int index2 = start - 1;

			do {
				int res = quot2 / 10;
				int rem = quot2 - (res * 10);

				quot2 = res;

				// Write the digit into the correct position
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, index2--, (char) ('0' - rem));
				/*[ELSE]*/
				value[index2--] = (char) ('0' - rem);
				/*[ENDIF]*/
			} while (quot2 != 0);

			if (v2 < 0) {
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, index2--, (char) '-');
				/*[ELSE]*/
				value[index2--] = '-';
				/*[ENDIF]*/
			}

			// Copy in s1 contents
			start = index2 + 1 - s1len;

			/*[IF Sidecar19-SE]*/
			decompressedArrayCopy(s1.value, 0, value, start, s1len);
			/*[ELSE]*/
			System.arraycopy(s1.value, 0, value, start, s1len);
			/*[ENDIF]*/

			// Copy in v1
			int index1 = start - 1;

			do {
				int res = quot1 / 10;
				int rem = quot1 - (res * 10);

				quot1 = res;

				// Write the digit into the correct position
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, index1--, (char) ('0' - rem));
				/*[ELSE]*/
				value[index1--] = (char) ('0' - rem);
				/*[ENDIF]*/
			} while (quot1 != 0);

			if (v1 < 0) {
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, index1--, (char) '-');
				/*[ELSE]*/
				value[index1--] = '-';
				/*[ENDIF]*/
			}
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/*
	 * Loads from the stringArray if concatenated result is found else it creates a string that is s1 + s2 which is stored in stringArray and then
	 * returned.
	 */
	static private String cachedConstantString(String s1, String s2, int index) {
		if (index < stringArraySize) {
			if (stringArray[index] == null) {
				stringArray[index] = new String(s1, s2);
			}
		} else {
			return new String(s1, s2);
		}
		return stringArray[index];
	}

	/**
	 * Answers the character at the specified offset in this String.
	 *
	 * @param index
	 *			  the zero-based index in this string
	 * @return the character at the index
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>index < 0</code> or <code>index >= length()</code>
	 */
	/*[IF]*/
	// TODO : Is it better to throw the AIOOB here implicitly and catch and rethrow?
	/*[ENDIF]*/
	public char charAt(int index) {
		if (0 <= index && index < lengthInternal()) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
			} 

			/*[IF Sidecar19-SE]*/
			return helpers.getCharFromArrayByIndex(value, index);
			/*[ELSE]*/
			return value[index];
			/*[ENDIF]*/
		}

		throw new StringIndexOutOfBoundsException();
	}

	// Internal version of charAt used for extracting a char from a String in compression related code.
	char charAtInternal(int index) {
		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		}

		/*[IF Sidecar19-SE]*/
		return helpers.getCharFromArrayByIndex(value, index);
		/*[ELSE]*/
		return value[index];
		/*[ENDIF]*/
	}
	
	// This method is needed so idiom recognition properly recognizes idiomatic loops where we are doing an operation on
	// the byte[] value of two Strings. In such cases we extract the String.value fields before entering the operation loop.
	// However if chatAt is used inside the loop then the JIT will anchor the load of the value byte[] inside of the loop thus
	// causing us to load the String.value on every iteration. This is very suboptimal and breaks some of the common idioms
	// that we recognize. The most prominent one is the regionMathes arraycmp idiom that is not recognized unless this method
	// is being used.
	/*[IF Sidecar19-SE]*/
	char charAtInternal(int index, byte[] value) {
	/*[ELSE]*/
	char charAtInternal(int index, char[] value) {
	/*[ENDIF]*/
		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		}

		/*[IF Sidecar19-SE]*/
		return helpers.getCharFromArrayByIndex(value, index);
		/*[ELSE]*/
		return value[index];
		/*[ENDIF]*/
	}

	/**
	 * Compares the specified String to this String using the Unicode values of the characters. Answer 0 if the strings contain the same characters in
	 * the same order. Answer a negative integer if the first non-equal character in this String has a Unicode value which is less than the Unicode
	 * value of the character at the same position in the specified string, or if this String is a prefix of the specified string. Answer a positive
	 * integer if the first non-equal character in this String has a Unicode value which is greater than the Unicode value of the character at the same
	 * position in the specified string, or if the specified String is a prefix of the this String.
	 *
	 * @param string
	 *			  the string to compare
	 * @return 0 if the strings are equal, a negative integer if this String is before the specified String, or a positive integer if this String is
	 *			after the specified String
	 *
	 * @throws NullPointerException
	 *				when string is null
	 */
	public int compareTo(String string) {
		String s1 = this;
		String s2 = string;

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		// Upper bound index on the last char to compare
		int end = s1len < s2len ? s1len : s2len;

		int o1 = 0;
		int o2 = 0;

		int result;

		/*[IF Sidecar19-SE]*/
		byte[] s1Value = s1.value;
		byte[] s2Value = s2.value;
		/*[ELSE]*/
		char[] s1Value = s1.value;
		char[] s2Value = s2.value;
		/*[ENDIF]*/
		
		s1Value.getClass(); // Implicit null check
		s2Value.getClass(); // Implicit null check

		if (enableCompression && (s1.count | s2.count) >= 0) {
			while (o1 < end) {
            if ((result = 
                  helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(s1Value, o1++)) - 
                  helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(s2Value, o2++))) != 0) {
               return result;
				}
			}
		} else {
			while (o1 < end) {
				if ((result = s1.charAtInternal(o1++, s1Value) - s2.charAtInternal(o2++, s2Value)) != 0) {
					return result;
				}
			}
		}

		return s1len - s2len;
	}

	private char compareValue(char c) {
		if (c < 128) {
			if ('A' <= c && c <= 'Z') {
				return (char) (c + ('a' - 'A'));
			}
		}

		return Character.toLowerCase(Character.toUpperCase(c));
	}

	private char compareValue(byte b) {
		if ('A' <= b && b <= 'Z') {
			return (char) (helpers.byteToCharUnsigned(b) + ('a' - 'A'));
		}
		/*[IF]*/
		// TODO : Optimization : Can we speed this up?
		/*[ENDIF]*/
		return Character.toLowerCase(Character.toUpperCase(helpers.byteToCharUnsigned(b)));
	}

	/**
	 * Compare the receiver to the specified String to determine the relative ordering when the case of the characters is ignored.
	 *
	 * @param string
	 *			  a String
	 * @return an int < 0 if this String is less than the specified String, 0 if they are equal, and > 0 if this String is greater
	 */
	public int compareToIgnoreCase(String string) {
		String s1 = this;
		String s2 = string;

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		// Upper bound index on the last char to compare
		int end = s1len < s2len ? s1len : s2len;

		int o1 = 0;
		int o2 = 0;

		int result;
		
		/*[IF Sidecar19-SE]*/
		byte[] s1Value = s1.value;
		byte[] s2Value = s2.value;
		/*[ELSE]*/
		char[] s1Value = s1.value;
		char[] s2Value = s2.value;
		/*[ENDIF]*/
		
		s1Value.getClass(); // Implicit null check
		s2Value.getClass(); // Implicit null check

		if (enableCompression && (s1.count | s2.count) >= 0) {
			while (o1 < end) {
				byte byteAtO1;
				byte byteAtO2;

				if ((byteAtO1 = helpers.getByteFromArrayByIndex(s1Value, o1++)) == (byteAtO2 = helpers.getByteFromArrayByIndex(s2Value, o2++))) {
					continue;
				}

				if ((result = compareValue(byteAtO1) - compareValue(byteAtO2)) != 0) {
					return result;
				}
			}
		} else {
			while (o1 < end) {
				char charAtO1;
				char charAtO2;

				if ((charAtO1 = s1.charAtInternal(o1++, s1Value)) == (charAtO2 = s2.charAtInternal(o2++, s2Value))) {
					continue;
				}

				if ((result = compareValue(charAtO1) - compareValue(charAtO2)) != 0) {
					return result;
				}
			}
		}

		return s1len - s2len;
	}

	/**
	 * Concatenates this String and the specified string.
	 *
	 * @param string
	 *			  the string to concatenate
	 * @return a String which is the concatenation of this String and the specified String
	 *
	 * @throws NullPointerException
	 *				if string is null
	 */
	public String concat(String string) {
		String s1 = this;
		String s2 = string;

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		if (s2len == 0) {
			return s1;
		}

		int concatlen = s1len + s2len;

		if (enableCompression && (s1.count | s2.count) >= 0) {
			/*[IF Sidecar19-SE]*/
			byte[] buffer = new byte[concatlen];
			/*[ELSE]*/
			char[] buffer = new char[(concatlen + 1) / 2];
			/*[ENDIF]*/

			compressedArrayCopy(s1.value, 0, buffer, 0, s1len);
			compressedArrayCopy(s2.value, 0, buffer, s1len, s2len);

			return new String(buffer, 0, concatlen, true);
		} else {
			/*[IF Sidecar19-SE]*/
			byte[] buffer = new byte[concatlen * 2];
			/*[ELSE]*/
			char[] buffer = new char[concatlen];
			/*[ENDIF]*/

			// Check if the String is compressed
			if (enableCompression && s1.count >= 0) {
				decompress(s1.value, 0, buffer, 0, s1len);
			} else {
				/*[IF Sidecar19-SE]*/
				decompressedArrayCopy(s1.value, 0, buffer, 0, s1len);
				/*[ELSE]*/
				System.arraycopy(s1.value, 0, buffer, 0, s1len);
				/*[ENDIF]*/
			}

			// Check if the String is compressed
			if (enableCompression && s2.count >= 0) {
				decompress(s2.value, 0, buffer, s1len, s2len);
			} else {
				/*[IF Sidecar19-SE]*/
				decompressedArrayCopy(s2.value, 0, buffer, s1len, s2len);
				/*[ELSE]*/
				System.arraycopy(s2.value, 0, buffer, s1len, s2len);
				/*[ENDIF]*/
			}

			return new String(buffer, 0, concatlen, false);
		}
	}

	/**
	 * Creates a new String containing the characters in the specified character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *			  the array of characters
	 * @return the new String
	 *
	 * @throws NullPointerException
	 *				if data is null
	 */
	public static String copyValueOf(char[] data) {
		return new String(data, 0, data.length);
	}

	/**
	 * Creates a new String containing the specified characters in the character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *			  the array of characters
	 * @param start
	 *			  the starting offset in the character array
	 * @param length
	 *			  the number of characters to use
	 * @return the new String
	 *
	 * @throws IndexOutOfBoundsException
	 *				if <code>length < 0, start < 0</code> or <code>start + length > data.length</code>
	 * @throws NullPointerException
	 *				if data is null
	 */
	public static String copyValueOf(char[] data, int start, int length) {
		return new String(data, start, length);
	}

	/**
	 * Compares the specified string to this String to determine if the specified string is a suffix.
	 *
	 * @param suffix
	 *			  the string to look for
	 * @return true when the specified string is a suffix of this String, false otherwise
	 *
	 * @throws NullPointerException
	 *				if suffix is null
	 */
	public boolean endsWith(String suffix) {
		return regionMatches(lengthInternal() - suffix.lengthInternal(), suffix, 0, suffix.lengthInternal());
	}

	/**
	 * Compares the specified object to this String and answer if they are equal. The object must be an instance of String with the same characters in
	 * the same order.
	 *
	 * @param object
	 *			  the object to compare
	 * @return true if the specified object is equal to this String, false otherwise
	 *
	 * @see #hashCode()
	 */
	public boolean equals(Object object) {
		if (object == this) {
			return true;
		}

		if (object instanceof String) {
			String s1 = this;
			String s2 = (String) object;

			int s1len = s1.lengthInternal();
			int s2len = s2.lengthInternal();

			if (s1len != s2len) {
				return false;
			}

			if (s1.value == s2.value) {
				return true;
			}

			// There was a time hole between first read of s.hashCode and second read if another thread does hashcode
			// computing for incoming string object
			int s1hash = s1.hashCode;
			int s2hash = s2.hashCode;

			if (s1hash != 0 && s2hash != 0 && s1hash != s2hash) {
				return false;
			}
			
			boolean matches = regionMatches(0, s2, 0, s1len);
			
			if (!matches) {
				return false;
			}

			if (com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY != com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY_DISABLED) {
				deduplicateStrings(s1, s2);
			}
			
			return true;
		}

		return false;
	}

	private static final void deduplicateStrings(String s1, String s2) {
		if ((s1.count ^ s2.count) >= 0) {
			long valueFieldOffset = UnsafeHelpers.valueFieldOffset;

			if (com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY == com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY_FAVOUR_LOWER) {
				if (com.ibm.oti.vm.VM.FJ9OBJECT_SIZE == 4) {
					if (helpers.getIntFromObject(s1, valueFieldOffset) < helpers.getIntFromObject(s2, valueFieldOffset)) {
						helpers.putObjectInObject(s2, valueFieldOffset, s1.value);
					} else {
						helpers.putObjectInObject(s1, valueFieldOffset, s2.value);
					}
				} else {
					if (helpers.getLongFromObject(s1, valueFieldOffset) < helpers.getLongFromObject(s2, valueFieldOffset)) {
						helpers.putObjectInObject(s2, valueFieldOffset, s1.value);
					} else {
						helpers.putObjectInObject(s1, valueFieldOffset, s2.value);
					}
				}
			} else {
				if (com.ibm.oti.vm.VM.FJ9OBJECT_SIZE == 4) {
					if (helpers.getIntFromObject(s1, valueFieldOffset) > helpers.getIntFromObject(s2, valueFieldOffset)) {
						helpers.putObjectInObject(s2, valueFieldOffset, s1.value);
					} else {
						helpers.putObjectInObject(s1, valueFieldOffset, s2.value);
					}
				} else {
					if (helpers.getLongFromObject(s1, valueFieldOffset) > helpers.getLongFromObject(s2, valueFieldOffset)) {
						helpers.putObjectInObject(s2, valueFieldOffset, s1.value);
					} else {
						helpers.putObjectInObject(s1, valueFieldOffset, s2.value);
					}
				}
			}
		}
	}

	/**
	 * Compares the specified String to this String ignoring the case of the characters and answer if they are equal.
	 *
	 * @param string
	 *			  the string to compare
	 * @return true if the specified string is equal to this String, false otherwise
	 */
	public boolean equalsIgnoreCase(String string) {
		String s1 = this;
		String s2 = string;

		if (s1 == s2) {
			return true;
		}
		
		if (s2 == null) {
			return false;
		}

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		if (s1len != s2len) {
			return false;
		}

		int o1 = 0;
		int o2 = 0;

		// Upper bound index on the last char to compare
		int end = s1len;
		
		/*[IF Sidecar19-SE]*/
		byte[] s1Value = s1.value;
		byte[] s2Value = s2.value;
		/*[ELSE]*/
		char[] s1Value = s1.value;
		char[] s2Value = s2.value;
		/*[ENDIF]*/

		if (enableCompression && (s1.count | s2.count) >= 0) {
			while (o1 < end) {
				byte byteAtO1 = helpers.getByteFromArrayByIndex(s1Value, o1++);
				byte byteAtO2 = helpers.getByteFromArrayByIndex(s2Value, o2++);

				/*[IF]*/
				// TODO : Optimization : Any better way of avoiding the widening? Perhaps we need toUpperCase / toLowerCase byte
				// overloads rather than code points.
				/*[ENDIF]*/
				if (byteAtO1 != byteAtO2 
						&& toUpperCase(helpers.byteToCharUnsigned(byteAtO1)) != toUpperCase(helpers.byteToCharUnsigned(byteAtO2))
						&& toLowerCase(helpers.byteToCharUnsigned(byteAtO1)) != toLowerCase(helpers.byteToCharUnsigned(byteAtO2))) {
					return false;
				}
			}
		} else {
			while (o1 < end) {
				char charAtO1 = s1.charAtInternal(o1++, s1Value);
				char charAtO2 = s2.charAtInternal(o2++, s2Value);

				if (charAtO1 != charAtO2 && toUpperCase(charAtO1) != toUpperCase(charAtO2) && toLowerCase(charAtO1) != toLowerCase(charAtO2)) {
					return false;
				}
			}
		}

		return true;
	}

	/**
	 * Converts this String to a byte encoding using the default encoding as specified by the file.encoding system property. If the system property is
	 * not defined, the default encoding is ISO8859_1 (ISO-Latin-1). If 8859-1 is not available, an ASCII encoding is used.
	 *
	 * @return the byte array encoding of this String
	 *
	 * @see String
	 */
	public byte[] getBytes() {
		int currentLength = lengthInternal();
		
		/*[IF Sidecar19-SE]*/
		byte[] buffer = value;
		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			if (buffer.length != currentLength) {
				buffer = new byte[currentLength];
				compressedArrayCopy(value, 0, buffer, 0, currentLength);
			}
			return StringCoding.encode(String.LATIN1, buffer);
		} else {
			if (buffer.length != currentLength << 1) {
				buffer = new byte[currentLength << 1];
				decompressedArrayCopy(value, 0, buffer, 0, currentLength);
			}
			return StringCoding.encode(String.UTF16, buffer);
		}
		/*[ELSE]*/
		/*[IF]*/
		// TODO : Optimization : Can we specialize these for both char[] and byte[] to avoid this decompression?
		/*[ENDIF]*/
		
		char[] buffer;

		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			buffer = new char[currentLength];
			decompress(value, 0, buffer, 0, currentLength);
		} else {
			buffer = value;
		}
		
		return StringCoding.encode(buffer, 0, currentLength);
		/*[ENDIF]*/
	}

	/**
	 * Converts this String to a byte array, ignoring the high order bits of each character.
	 *
	 * @param start
	 *			  the starting offset of characters to copy
	 * @param end
	 *			  the ending offset of characters to copy
	 * @param data
	 *			  the destination byte array
	 * @param index
	 *			  the starting offset in the byte array
	 *
	 * @throws NullPointerException
	 *				when data is null
	 * @throws IndexOutOfBoundsException
	 *				when <code>start < 0, end > length(), index < 0, end - start > data.length - index</code>
	 *
	 * @deprecated Use getBytes() or getBytes(String)
	 */
	/*[IF Sidecar19-SE]*/
	@Deprecated(forRemoval=false, since="1.1")
	/*[ELSE]*/
	@Deprecated
	/*[ENDIF]*/
	public void getBytes(int start, int end, byte[] data, int index) {
		if (0 <= start && start <= end && end <= lengthInternal()) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				compressedArrayCopy(value, start, data, index, end - start);
			} else {
				compress(value, start, data, index, end - start);
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}
	
	void getBytes(int start, int end, char[] data, int index) {
		if (0 <= start && start <= end && end <= lengthInternal()) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				compressedArrayCopy(value, start, data, index, end - start);
			} else {
				compress(value, start, data, index, end - start);
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Converts this String to a byte encoding using the specified encoding.
	 *
	 * @param encoding
	 *			  the encoding
	 * @return the byte array encoding of this String
	 *
	 * @throws UnsupportedEncodingException
	 *				when the encoding is not supported
	 *
	 * @see String
	 * @see UnsupportedEncodingException
	 */
	public byte[] getBytes(String encoding) throws UnsupportedEncodingException {
		if (encoding == null) {
			throw new NullPointerException();
		}

		int currentLength = lengthInternal();
		/*[IF Sidecar19-SE]*/
		byte[] buffer = value;
		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			if (buffer.length != currentLength) {
				buffer = new byte[currentLength];
				compressedArrayCopy(value, 0, buffer, 0, currentLength);
			}
			return StringCoding.encode(encoding, String.LATIN1, buffer);
		} else {
			if (buffer.length != currentLength << 1) {
				buffer = new byte[currentLength << 1];
				decompressedArrayCopy(value, 0, buffer, 0, currentLength);
			}
			return StringCoding.encode(encoding, String.UTF16, buffer);
		}
		/*[ELSE]*/
		/*[IF]*/
		// TODO : Optimization : Can we specialize these for both char[] and byte[] to avoid this decompression?
		/*[ENDIF]*/
		
		char[] buffer;

		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			buffer = new char[currentLength];
			decompress(value, 0, buffer, 0, currentLength);
		} else {
			buffer = value;
		}
		
		return StringCoding.encode(encoding, buffer, 0, currentLength);
		/*[ENDIF]*/
	}

	/**
	 * Copies the specified characters in this String to the character array starting at the specified offset in the character array.
	 *
	 * @param start
	 *			  the starting offset of characters to copy
	 * @param end
	 *			  the ending offset of characters to copy
	 * @param data
	 *			  the destination character array
	 * @param index
	 *			  the starting offset in the character array
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>start < 0, end > length(),
	 * 				start > end, index < 0, end - start > buffer.length - index</code>
	 * @throws NullPointerException
	 *				when buffer is null
	 */
	public void getChars(int start, int end, char[] data, int index) {
		if (0 <= start && start <= end && end <= lengthInternal()) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				decompress(value, start, data, index, end - start);
			} else {
				/*[IF Sidecar19-SE]*/
				decompressedArrayCopy(value, start, data, index, end - start);
				/*[ELSE]*/
				System.arraycopy(value, start, data, index, end - start);
				/*[ENDIF]*/
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/*[IF Sidecar19-SE]*/
	void getChars(int start, int end, byte[] data, int index) {
		if (0 <= start && start <= end && end <= lengthInternal()) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				decompress(value, start, data, index, end - start);
			} else {
				decompressedArrayCopy(value, start, data, index, end - start);
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}
	/*[ENDIF]*/

	/**
	 * Answers an integer hash code for the receiver. Objects which are equal answer the same value for this method.
	 *
	 * @return the receiver's hash
	 *
	 * @see #equals
	 */
	public int hashCode() {
		if (hashCode == 0) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				hashCode = hashCodeImplCompressed(value, 0, lengthInternal());
			} else {
				hashCode = hashCodeImplDecompressed(value, 0, lengthInternal());
			}
		}

		return hashCode;
	}

	/*[IF]*/
	// TODO : Optimization : Note I have changed the name and signature of these recognized methods. This will need to be
	// reflected in the JIT.
	/*[ENDIF]*/
	/*[IF Sidecar19-SE]*/
	private static int hashCodeImplCompressed(byte[] value, int offset, int count) {
	/*[ELSE]*/
	private static int hashCodeImplCompressed(char[] value, int offset, int count) {
	/*[ENDIF]*/
		int hash = 0, end = offset + count;

		for (int i = offset; i < end; ++i) {
			hash = (hash << 5) - hash + helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, i));
		}

		return hash;
	}

	/*[IF Sidecar19-SE]*/
	private static int hashCodeImplDecompressed(byte[] value, int offset, int count) {
	/*[ELSE]*/
	private static int hashCodeImplDecompressed(char[] value, int offset, int count) {
	/*[ENDIF]*/
		int hash = 0, end = offset + count;

		for (int i = offset; i < end; ++i) {
			/*[IF Sidecar19-SE]*/
			hash = (hash << 5) - hash + helpers.getCharFromArrayByIndex(value, i);
			/*[ELSE]*/
			hash = (hash << 5) - hash + value[i];
			/*[ENDIF]*/
		}

		return hash;
	}

	/**
	 * Searches in this String for the first index of the specified character. The search for the character starts at the beginning and moves towards
	 * the end of this String.
	 *
	 * @param c
	 *			  the character to find
	 * @return the index in this String of the specified character, -1 if the character isn't found
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int indexOf(int c) {
		return indexOf(c, 0);
	}

	/**
	 * Searches in this String for the index of the specified character. The search for the character starts at the specified offset and moves towards
	 * the end of this String.
	 *
	 * @param c
	 *			  the character to find
	 * @param start
	 *			  the starting offset
	 * @return the index in this String of the specified character, -1 if the character isn't found
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int indexOf(int c, int start) {
		int len = lengthInternal();

		if (start < len) {
			if (start < 0) {
				start = 0;
			}

			if (c >= 0 && c <= Character.MAX_VALUE) {
				/*[IF Sidecar19-SE]*/
				byte[] array = value;
				/*[ELSE]*/
				char[] array = value;
				/*[ENDIF]*/
				
				array.getClass(); // Implicit null check

				// Check if the String is compressed
				if (enableCompression && count >= 0) {
					if (c <= 255) {
						byte b = (byte) c;

						for (int i = start; i < len; ++i) {
							if (helpers.getByteFromArrayByIndex(array, i) == b) {
								return i;
							}
						}
					}
				} else {
					for (int i = start; i < len; ++i) {
						/*[IF Sidecar19-SE]*/
						if (helpers.getCharFromArrayByIndex(array, i) == c) {
						/*[ELSE]*/
						if (array[i] == c) {
						/*[ENDIF]*/
							return i;
						}
					}
				}
			} else if (c <= Character.MAX_CODE_POINT) {
				for (int i = start; i < len; ++i) {
					int codePoint = codePointAt(i);

					if (codePoint == c) {
						return i;
					}

					if (codePoint >= Character.MIN_SUPPLEMENTARY_CODE_POINT) {
						++i;
					}
				}
			}
		}

		return -1;
	}

	/**
	 * Searches in this String for the first index of the specified string. The search for the string starts at the beginning and moves towards the end
	 * of this String.
	 *
	 * @param string
	 *			  the string to find
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *				when string is null
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 *
	 */
	public int indexOf(String string) {
		return indexOf(string, 0);
	}

	/**
	 * Searches in this String for the index of the specified string. The search for the string starts at the specified offset and moves towards the
	 * end of this String.
	 *
	 * @param subString
	 *			  the string to find
	 * @param start
	 *			  the starting offset
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *				when string is null
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int indexOf(String subString, int start) {
		if (start < 0) {
			start = 0;
		}

		String s1 = this;
		String s2 = subString;

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		if (s2len > 0) {
			if (s2len + start > s1len) {
				return -1;
			}

			/*[IF Sidecar19-SE]*/
			byte[] s1Value = s1.value;
			byte[] s2Value = s2.value;
			/*[ELSE]*/
			char[] s1Value = s1.value;
			char[] s2Value = s2.value;
			/*[ENDIF]*/

			s1Value.getClass(); // Implicit null check
			s2Value.getClass(); // Implicit null check

			if (enableCompression && (s1.count | s2.count) >= 0) {
				char firstChar = helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(s2Value, 0));

				while (true) {
					/*[IF]*/
					// TODO : Optimization : We can specialize indexOf for byte here
					/*[ENDIF]*/
					int i = indexOf(firstChar, start);

					// Handles subCount > count || start >= count
					if (i == -1 || s2len + i > s1len) {
						return -1;
					}

					int o1 = i;
					int o2 = 0;

					while (++o2 < s2len && helpers.getByteFromArrayByIndex(s1Value, ++o1) == helpers.getByteFromArrayByIndex(s2Value, o2))
						;

					if (o2 == s2len) {
						return i;
					}

					start = i + 1;
				}
			} else {
				char firstChar = s2.charAtInternal(0, s2Value);

				while (true) {
					int i = indexOf(firstChar, start);

					// Handles subCount > count || start >= count
					if (i == -1 || s2len + i > s1len) {
						return -1;
					}

					int o1 = i;
					int o2 = 0;

					while (++o2 < s2len && s1.charAtInternal(++o1, s1Value) == s2.charAtInternal(o2, s2Value))
						;

					if (o2 == s2len) {
						return i;
					}

					start = i + 1;
				}
			}
		} else {
			return start < s1len ? start : s1len;
		}
	}

	/**
	 * Searches an internal table of strings for a string equal to this String. If the string is not in the table, it is added. Answers the string
	 * contained in the table which is equal to this String. The same string object is always answered for strings which are equal.
	 *
	 * @return the interned string equal to this String
	 */
	public native String intern();

	/**
	 * Searches in this String for the last index of the specified character. The search for the character starts at the end and moves towards the
	 * beginning of this String.
	 *
	 * @param c
	 *			  the character to find
	 * @return the index in this String of the specified character, -1 if the character isn't found
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int lastIndexOf(int c) {
		return lastIndexOf(c, lengthInternal() - 1);
	}

	/**
	 * Searches in this String for the index of the specified character. The search for the character starts at the specified offset and moves towards
	 * the beginning of this String.
	 *
	 * @param c
	 *			  the character to find
	 * @param start
	 *			  the starting offset
	 * @return the index in this String of the specified character, -1 if the character isn't found
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int lastIndexOf(int c, int start) {
		if (start >= 0) {
			int len = lengthInternal();

			if (start >= len) {
				start = len - 1;
			}

			if (c >= 0 && c <= Character.MAX_VALUE) {
				/*[IF Sidecar19-SE]*/
				byte[] array = value;
				/*[ELSE]*/
				char[] array = value;
				/*[ENDIF]*/
				
				array.getClass(); // Implicit null check
				
				// Check if the String is compressed
				if (enableCompression && count >= 0) {
					if (c <= 255) {
						byte b = (byte) c;

						for (int i = start; i >= 0; --i) {
							if (helpers.getByteFromArrayByIndex(array, i) == b) {
								return i;
							}
						}
					}
				} else {
					for (int i = start; i >= 0; --i) {
						/*[IF Sidecar19-SE]*/
						if (helpers.getCharFromArrayByIndex(array, i) == c) {
						/*[ELSE]*/
						if (array[i] == c) {
						/*[ENDIF]*/
							return i;
						}
					}
				}
			} else if (c <= Character.MAX_CODE_POINT) {
				for (int i = start; i >= 0; --i) {
					int codePoint = codePointAt(i);

					if (codePoint == c) {
						return i;
					}

					if (codePoint >= Character.MIN_SUPPLEMENTARY_CODE_POINT) {
						--i;
					}
				}
			}
		}

		return -1;
	}

	/**
	 * Searches in this String for the last index of the specified string. The search for the string starts at the end and moves towards the beginning
	 * of this String.
	 *
	 * @param string
	 *			  the string to find
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *				when string is null
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int lastIndexOf(String string) {
		// Use count instead of count - 1 so lastIndexOf("") answers count
		return lastIndexOf(string, lengthInternal());
	}

	/**
	 * Searches in this String for the index of the specified string. The search for the string starts at the specified offset and moves towards the
	 * beginning of this String.
	 *
	 * @param subString
	 *			  the string to find
	 * @param start
	 *			  the starting offset
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *				when string is null
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int lastIndexOf(String subString, int start) {
		String s1 = this;
		String s2 = subString;

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		if (s2len <= s1len && start >= 0) {
			if (s2len > 0) {
				if (start > s1len - s2len) {
					start = s1len - s2len; // s1len and s2len are both >= 1
				}

				/*[IF Sidecar19-SE]*/
				byte[] s1Value = s1.value;
				byte[] s2Value = s2.value;
				/*[ELSE]*/
				char[] s1Value = s1.value;
				char[] s2Value = s2.value;
				/*[ENDIF]*/

				s1Value.getClass(); // Implicit null check
				s2Value.getClass(); // Implicit null check

				if (enableCompression && (s1.count | s2.count) >= 0) {
					char firstChar = helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(s2Value, 0));

					while (true) {
						/*[IF]*/
						// TODO : Optimization : We can specialize lastIndexOf for byte here
						/*[ENDIF]*/
						int i = lastIndexOf(firstChar, start);

						if (i == -1) {
							return -1;
						}

						int o1 = i;
						int o2 = 0;

						while (++o2 < s2len && helpers.getByteFromArrayByIndex(s1Value, ++o1) == helpers.getByteFromArrayByIndex(s2Value, o2))
							;

						if (o2 == s2len) {
							return i;
						}

						start = i - 1;
					}
				} else {
					char firstChar = s2.charAtInternal(0, s2Value);

					while (true) {
						int i = lastIndexOf(firstChar, start);

						if (i == -1) {
							return -1;
						}

						int o1 = i;
						int o2 = 0;

						while (++o2 < s2len && s1.charAtInternal(++o1, s1Value) == s2.charAtInternal(o2, s2Value))
							;

						if (o2 == s2len) {
							return i;
						}

						start = i - 1;
					}
				}
			} else {
				return start < s1len ? start : s1len;
			}
		} else {
			return -1;
		}
	}

	/**
	 * Answers the size of this String.
	 *
	 * @return the number of characters in this String
	 */
	public int length() {
		return lengthInternal();
	}
	
	/**
	 * Answers the size of this String. This method is to be used internally within the current package whenever
	 * possible as the JIT compiler will take special precaution to avoid generating HCR guards for calls to this
	 * method.
	 *
	 * @return the number of characters in this String
	 */
	int lengthInternal() {
		if (enableCompression) {
			// Check if the String is compressed
			if (count >= 0) {
				return count;
			}
			
			return count & ~uncompressedBit;
		}
		
		return count;
	}

	/**
	 * Compares the specified string to this String and compares the specified range of characters to determine if they are the same.
	 *
	 * @param thisStart
	 *			  the starting offset in this String
	 * @param string
	 *			  the string to compare
	 * @param start
	 *			  the starting offset in string
	 * @param length
	 *			  the number of characters to compare
	 * @return true if the ranges of characters is equal, false otherwise
	 *
	 * @throws NullPointerException
	 *				when string is null
	 */
	public boolean regionMatches(int thisStart, String string, int start, int length) {
		string.getClass(); // Implicit null check

		String s1 = this;
		String s2 = string;

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		if (start < 0 || s2len - start < length) {
			return false;
		}

		if (thisStart < 0 || s1len - thisStart < length) {
			return false;
		}

		if (length <= 0) {
			return true;
		}

		int o1 = thisStart;
		int o2 = start;

		// Index of the last char to compare
		int end = length - 1;

		/*[IF Sidecar19-SE]*/
		byte[] s1Value = s1.value;
		byte[] s2Value = s2.value;
		/*[ELSE]*/
		char[] s1Value = s1.value;
		char[] s2Value = s2.value;
		/*[ENDIF]*/

		s1Value.getClass(); // Implicit null check
		s2Value.getClass(); // Implicit null check

		if (enableCompression && (s1.count | s2.count) >= 0) {
			if (helpers.getByteFromArrayByIndex(s1Value, o1 + end) != helpers.getByteFromArrayByIndex(s2Value, o2 + end)) {
				return false;
			} else {
				for (int i = 0; i < end; ++i) {
					if (helpers.getByteFromArrayByIndex(s1Value, o1 + i) != helpers.getByteFromArrayByIndex(s2Value, o2 + i)) {
						return false;
					}
				}
			}
		} else {
			if (s1.charAtInternal(o1 + end, s1Value) != s2.charAtInternal(o2 + end, s2Value)) {
				return false;
			} else {
				for (int i = 0; i < end; ++i) {
					if (s1.charAtInternal(o1 + i, s1Value) != s2.charAtInternal(o2 + i, s2Value)) {
						return false;
					}
				}
			}
		}

		return true;
	}

	/**
	 * Compares the specified string to this String and compares the specified range of characters to determine if they are the same. When ignoreCase
	 * is true, the case of the characters is ignored during the comparison.
	 *
	 * @param ignoreCase
	 *			  specifies if case should be ignored
	 * @param thisStart
	 *			  the starting offset in this String
	 * @param string
	 *			  the string to compare
	 * @param start
	 *			  the starting offset in string
	 * @param length
	 *			  the number of characters to compare
	 * @return true if the ranges of characters is equal, false otherwise
	 *
	 * @throws NullPointerException
	 *				when string is null
	 */
	public boolean regionMatches(boolean ignoreCase, int thisStart, String string, int start, int length) {
		if (!ignoreCase) {
			return regionMatches(thisStart, string, start, length);
		}

		string.getClass(); // Implicit null check

		String s1 = this;
		String s2 = string;

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		if (thisStart < 0 || length > s1len - thisStart) {
			return false;
		}

		if (start < 0 || length > s2len - start) {
			return false;
		}

		if (length <= 0) {
			return true;
		}

		int o1 = thisStart;
		int o2 = start;

		// Upper bound index on the last char to compare
		int end = thisStart + length;

		/*[IF Sidecar19-SE]*/
		byte[] s1Value = s1.value;
		byte[] s2Value = s2.value;
		/*[ELSE]*/
		char[] s1Value = s1.value;
		char[] s2Value = s2.value;
		/*[ENDIF]*/

		s1Value.getClass(); // Implicit null check
		s2Value.getClass(); // Implicit null check

		if (enableCompression && (s1.count | s2.count) >= 0) {
			while (o1 < end) {
				byte byteAtO1 = helpers.getByteFromArrayByIndex(s1Value, o1++);
				byte byteAtO2 = helpers.getByteFromArrayByIndex(s2Value, o2++);

				/*[IF]*/
				// TODO : Optimization : Any better way of avoiding the widening? Perhaps we need toUpperCase / toLowerCase byte
				// overloads rather than code points.
				/*[ENDIF]*/
				if (byteAtO1 != byteAtO2 
						&& toUpperCase(helpers.byteToCharUnsigned(byteAtO1)) != toUpperCase(helpers.byteToCharUnsigned(byteAtO2))
						&& toLowerCase(helpers.byteToCharUnsigned(byteAtO1)) != toLowerCase(helpers.byteToCharUnsigned(byteAtO2))) {
					return false;
				}
			}
		} else {
			while (o1 < end) {
				char charAtO1 = s1.charAtInternal(o1++, s1Value);
				char charAtO2 = s2.charAtInternal(o2++, s2Value);

				if (charAtO1 != charAtO2 
						&& toUpperCase(charAtO1) != toUpperCase(charAtO2) 
						&& toLowerCase(charAtO1) != toLowerCase(charAtO2)) {
					return false;
				}
			}
		}

		return true;
	}

	/**
	 * Replaces occurrences of the specified character with another character.
	 *
	 * @param oldChar
	 *			  the character to replace
	 * @param newChar
	 *			  the replacement character
	 * @return a String with occurrences of oldChar replaced by newChar
	 */
	public String replace(char oldChar, char newChar) {
		int index = indexOf(oldChar, 0);

		if (index == -1) {
			return this;
		}

		int len = lengthInternal();

		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			if (newChar <= 255) {
				/*[IF Sidecar19-SE]*/
				byte[] buffer = new byte[len];
				/*[ELSE]*/
				char[] buffer = new char[(len + 1) / 2];
				/*[ENDIF]*/

				compressedArrayCopy(value, 0, buffer, 0, len);

				do {
					helpers.putByteInArrayByIndex(buffer, index++, (byte) newChar);
				} while ((index = indexOf(oldChar, index)) != -1);

				return new String(buffer, 0, len, true);
			} else {
				/*[IF Sidecar19-SE]*/
				byte[] buffer = new byte[len * 2];
				/*[ELSE]*/
				char[] buffer = new char[len];
				/*[ENDIF]*/

				decompress(value, 0, buffer, 0, len);

				do {
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(buffer, index++, (char) newChar);
					/*[ELSE]*/
					buffer[index++] = newChar;
					/*[ENDIF]*/
				} while ((index = indexOf(oldChar, index)) != -1);

				return new String(buffer, 0, len, false);
			}
		} else {
			/*[IF Sidecar19-SE]*/
			byte[] buffer = new byte[len * 2];
			
			decompressedArrayCopy(value, 0, buffer, 0, len);
			/*[ELSE]*/
			char[] buffer = new char[len];

			System.arraycopy(value, 0, buffer, 0, len);
			/*[ENDIF]*/


			do {
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(buffer, index++, (char) newChar);
				/*[ELSE]*/
				buffer[index++] = newChar;
				/*[ENDIF]*/
			} while ((index = indexOf(oldChar, index)) != -1);

			return new String(buffer, 0, len, false);
		}
	}

	/**
	 * Compares the specified string to this String to determine if the specified string is a prefix.
	 *
	 * @param prefix
	 *			  the string to look for
	 * @return true when the specified string is a prefix of this String, false otherwise
	 *
	 * @throws NullPointerException
	 *				when prefix is null
	 */
	public boolean startsWith(String prefix) {
		return startsWith(prefix, 0);
	}

	/**
	 * Compares the specified string to this String, starting at the specified offset, to determine if the specified string is a prefix.
	 *
	 * @param prefix
	 *			  the string to look for
	 * @param start
	 *			  the starting offset
	 * @return true when the specified string occurs in this String at the specified offset, false otherwise
	 *
	 * @throws NullPointerException
	 *				when prefix is null
	 */
	public boolean startsWith(String prefix, int start) {
		return regionMatches(start, prefix, 0, prefix.lengthInternal());
	}

	/**
	 * Copies a range of characters into a new String.
	 *
	 * @param start
	 *			  the offset of the first character
	 * @return a new String containing the characters from start to the end of the string
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>start < 0</code> or <code>start > length()</code>
	 */
	public String substring(int start) {
		if (start == 0) {
			return this;
		}

		int len = lengthInternal();

		if (0 <= start && start <= len) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				return new String(value, start, len - start, true, enableSharingInSubstringWhenOffsetIsZero);
			} else {
				return new String(value, start, len - start, false, enableSharingInSubstringWhenOffsetIsZero);
			}
		} else {
			throw new StringIndexOutOfBoundsException(start);
		}
	}

	/**
	 * Copies a range of characters.
	 *
	 * @param start
	 *			  the offset of the first character
	 * @param end
	 *			  the offset one past the last character
	 * @return a String containing the characters from start to end - 1
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>start < 0, start > end</code> or <code>end > length()</code>
	 */
	public String substring(int start, int end) {
		int len = lengthInternal();

		if (start == 0 && end == len) {
			return this;
		}

		if (0 <= start && start <= end && end <= len) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				return new String(value, start, end - start, true, enableSharingInSubstringWhenOffsetIsZero);
			} else {
				return new String(value, start, end - start, false, enableSharingInSubstringWhenOffsetIsZero);
			}
		} else {
			throw new StringIndexOutOfBoundsException(start);
		}
	}

	/**
	 * Copies the characters in this String to a character array.
	 *
	 * @return a character array containing the characters of this String
	 */
	public char[] toCharArray() {
		int len = lengthInternal();

		char[] buffer = new char[len];

		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			decompress(value, 0, buffer, 0, len);
		} else {
			/*[IF Sidecar19-SE]*/
			decompressedArrayCopy(value, 0, buffer, 0, len);
			/*[ELSE]*/
			System.arraycopy(value, 0, buffer, 0, len);
			/*[ENDIF]*/
		}

		return buffer;
	}

	/**
	 * Converts the characters in this String to lowercase, using the default Locale. To convert to lower case independent of any locale, use
	 * toLowerCase(Locale.ROOT).
	 *
	 * @return a new String containing the lowercase characters equivalent to the characters in this String
	 */
	public String toLowerCase() {
		return toLowerCase(Locale.getDefault());
	}

	/*[IF]*/
	// TODO : Optimization : Can we specialize these for bytes?
	/*[ENDIF]*/
	private int toLowerCase(int codePoint) {
		if (codePoint < 128) {
			if ('A' <= codePoint && codePoint <= 'Z') {
				return codePoint + ('a' - 'A');
			} else {
				return codePoint;
			}
		} else {
			return Character.toLowerCase(codePoint);
		}
	}

	private int toUpperCase(int codePoint) {
		if (codePoint < 128) {
			if ('a' <= codePoint && codePoint <= 'z') {
				return codePoint - ('a' - 'A');
			} else {
				return codePoint;
			}
		} else {
			return Character.toUpperCase(codePoint);
		}
	}

	// Some of the data below originated from the Unicode Character Database file
	// www.unicode.org/Public/4.0-Update/SpecialCasing-4.0.0.txt. Data from this
	// file was extracted, used in the code and/or converted to an array
	// representation for performance and size.

/*
UNICODE, INC. LICENSE AGREEMENT - DATA FILES AND SOFTWARE

Unicode Data Files include all data files under the directories
http://www.unicode.org/Public/, http://www.unicode.org/reports/,
http://www.unicode.org/cldr/data/, http://source.icu-project.org/repos/icu/, and
http://www.unicode.org/utility/trac/browser/.

Unicode Data Files do not include PDF online code charts under the
directory http://www.unicode.org/Public/.

Software includes any source code published in the Unicode Standard
or under the directories
http://www.unicode.org/Public/, http://www.unicode.org/reports/,
http://www.unicode.org/cldr/data/, http://source.icu-project.org/repos/icu/, and
http://www.unicode.org/utility/trac/browser/.

NOTICE TO USER: Carefully read the following legal agreement.
BY DOWNLOADING, INSTALLING, COPYING OR OTHERWISE USING UNICODE INC.'S
DATA FILES ("DATA FILES"), AND/OR SOFTWARE ("SOFTWARE"),
YOU UNEQUIVOCALLY ACCEPT, AND AGREE TO BE BOUND BY, ALL OF THE
TERMS AND CONDITIONS OF THIS AGREEMENT.
IF YOU DO NOT AGREE, DO NOT DOWNLOAD, INSTALL, COPY, DISTRIBUTE OR USE
THE DATA FILES OR SOFTWARE.

COPYRIGHT AND PERMISSION NOTICE

Copyright (c) 1991-2017 Unicode, Inc. All rights reserved.
Distributed under the Terms of Use in http://www.unicode.org/copyright.html.

Permission is hereby granted, free of charge, to any person obtaining
a copy of the Unicode data files and any associated documentation
(the "Data Files") or Unicode software and any associated documentation
(the "Software") to deal in the Data Files or Software
without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, and/or sell copies of
the Data Files or Software, and to permit persons to whom the Data Files
or Software are furnished to do so, provided that either
(a) this copyright and permission notice appear with all copies
of the Data Files or Software, or
(b) this copyright and permission notice appear in associated
Documentation.

THE DATA FILES AND SOFTWARE ARE PROVIDED "AS IS", WITHOUT WARRANTY OF
ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT OF THIRD PARTY RIGHTS.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS
NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL
DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THE DATA FILES OR SOFTWARE.

Except as contained in this notice, the name of a copyright holder
shall not be used in advertising or otherwise to promote the sale,
use or other dealings in these Data Files or Software without prior
written authorization of the copyright holder.
*/

	/**
	 * Converts the characters in this String to lowercase, using the specified Locale.
	 *
	 * @param locale
	 *			  the Locale
	 * @return a String containing the lowercase characters equivalent to the characters in this String
	 */
	public String toLowerCase(Locale locale) {
		// check locale for null
		String language = locale.getLanguage();

		if (isEmpty()) {
			return this;
		}

		if (StrHWAvailable() && language == "en") { //$NON-NLS-1$
			String output = toLowerHWOptimized(new String(lengthInternal()));

			if (output != null) {
				return output;
			}
		}

		return toLowerCaseCore(language);
	}

	/**
	 * The core of lower case conversion. This is the old, not-as-fast path.
	 *
	 * @param language
	 *			  a string representing the Locale
	 * @return a new string object
	 */
	private String toLowerCaseCore(String language) {
		/*[IF]*/
		// TODO : How do we handle the language here properly?
		// TODO : Need to "compressify" this method. All chars in range [0, 255] have toLowerCase also in [0, 255] where the distance between
		// the char and its lower case is either 0 or 32.
		/*
		 * char[] c = new char[256];
		 * 
		 * for (int i = 0; i <= 255; ++i) { c[i] = (char) i; }
		 * 
		 * String s1 = new String(c);
		 * 
		 * String s2 = s1.toLowerCase();
		 * 
		 * for (int i = 0; i <= 255; ++i) { if (s1.charAt(i) != s2.charAt(i)) { int i1 = (int) s1.charAt(i); int i2 = (int) s2.charAt(i);
		 * System.out.println(" " + i1 + " " + s1.charAt(i) + " -- " + i2 + " " + s2.charAt(i) + "  difference = " + (i2 - i1)); } }
		 */
		/*[ENDIF]*/
		boolean turkishAzeri = false;
		boolean lithuanian = false;

		StringBuilder builder = null;

		value.getClass(); // Implicit null check

		int len = lengthInternal();

		for (int i = 0; i < len; i++) {
			int codePoint = charAtInternal(i);

			if (codePoint >= Character.MIN_HIGH_SURROGATE && codePoint <= Character.MAX_HIGH_SURROGATE) {
				codePoint = codePointAt(i);
			}

			int lowerCase = toLowerCase(codePoint);

			if (codePoint != lowerCase) {
				if (builder == null) {
					turkishAzeri = language == "tr" || language == "az"; //$NON-NLS-1$ //$NON-NLS-2$
					lithuanian = language == "lt"; //$NON-NLS-1$

					builder = new StringBuilder(len);
					
					// Check if the String is compressed
					if (enableCompression && count >= 0) {
						builder.append(value, 0, i, true);
					} else {
						builder.append(value, 0, i, false);
					}
				}

				if (codePoint == 0x3A3) {
					builder.append(convertSigma(i));

					continue;
				}
				
				if (!turkishAzeri && (0x0130 == codePoint)) {
					builder.append("i\u0307"); //$NON-NLS-1$
					
					continue;
				}

				if (turkishAzeri) {
					if (codePoint == 0x49) {
						// Special case mappings. Latin Capital Letter I becomes Latin Small Letter Dotless i, unless followed by Combining Dot Above
						boolean combiningDotAbove = (i + 1) < len && charAtInternal(i + 1) == '\u0307';

						builder.append(combiningDotAbove ? 'i' : '\u0131');

						if (combiningDotAbove) {
							++i;
						}
					} else {
						builder.appendCodePoint(lowerCase);
					}
				} else if (lithuanian) {
					// Latin Capital Letter I, Latin Capital Letter J, Latin Capital Letter I with Ogonek
					if (codePoint == 0x49 || codePoint == 0x4A || codePoint == 0x12E) {
						builder.append(codePoint == 0x12E ? '\u012F' : (char) (codePoint + 0x20));

						if ((i + 1) < len) {
							int nextPoint = codePointAt(i + 1);

							if (isCombiningAbove(nextPoint)) {
								builder.append('\u0307');
							}
						}
						// Latin Capital Letter I with Grave
					} else if (codePoint == 0xCC) {
						builder.append("i\u0307\u0300"); //$NON-NLS-1$
						// Latin Capital Letter I with Acute
					} else if (codePoint == 0xCD) {
						builder.append("i\u0307\u0301"); //$NON-NLS-1$
						// Latin Capital Letter I with Tilde
					} else if (codePoint == 0x128) {
						builder.append("i\u0307\u0303"); //$NON-NLS-1$
					} else {
						builder.appendCodePoint(lowerCase);
					}
				} else {
					builder.appendCodePoint(lowerCase);

				}
			} else if (builder != null) {
				builder.appendCodePoint(codePoint);
			}

			if (codePoint >= Character.MIN_SUPPLEMENTARY_CODE_POINT) {
				++i;
			}
		}

		if (builder == null) {
			return this;
		}

		return builder.toString();
	}

	private static int binarySearchRange(char[] data, char c) {
		char value = 0;

		int low = 0;
		int mid = -1;
		int high = data.length - 1;

		while (low <= high) {
			mid = (low + high) >> 1;

			value = data[mid];

			if (c > value) {
				low = mid + 1;
			} else if (c == value) {
				return mid;
			} else {
				high = mid - 1;
			}
		}

		return mid - (c < value ? 1 : 0);
	}

	private static char[] startCombiningAbove = { '\u0300', '\u033D', '\u0346', '\u034A', '\u0350', '\u0357', '\u0363', '\u0483', '\u0592', '\u0597',
			'\u059C', '\u05A8', '\u05AB', '\u05AF', '\u05C4', '\u0610', '\u0653', '\u0657', '\u06D6', '\u06DF', '\u06E4', '\u06E7', '\u06EB', '\u0730',
			'\u0732', '\u0735', '\u073A', '\u073D', '\u073F', '\u0743', '\u0745', '\u0747', '\u0749', '\u0951', '\u0953', '\u0F82', '\u0F86', '\u17DD',
			'\u193A', '\u20D0', '\u20D4', '\u20DB', '\u20E1', '\u20E7', '\u20E9', '\uFE20' };
	private static char[] endCombiningAbove = { '\u0314', '\u0344', '\u0346', '\u034C', '\u0352', '\u0357', '\u036F', '\u0486', '\u0595', '\u0599',
			'\u05A1', '\u05A9', '\u05AC', '\u05AF', '\u05C4', '\u0615', '\u0654', '\u0658', '\u06DC', '\u06E2', '\u06E4', '\u06E8', '\u06EC', '\u0730',
			'\u0733', '\u0736', '\u073A', '\u073D', '\u0741', '\u0743', '\u0745', '\u0747', '\u074A', '\u0951', '\u0954', '\u0F83', '\u0F87', '\u17DD',
			'\u193A', '\u20D1', '\u20D7', '\u20DC', '\u20E1', '\u20E7', '\u20E9', '\uFE23' };
	private static char[] upperValues = { '\u0053', '\u0053', '\u0000', '\u02BC', '\u004E', '\u0000', '\u004A', '\u030C', '\u0000', '\u0399',
			'\u0308', '\u0301', '\u03A5', '\u0308', '\u0301', '\u0535', '\u0552', '\u0000', '\u0048', '\u0331', '\u0000', '\u0054', '\u0308', '\u0000',
			'\u0057', '\u030A', '\u0000', '\u0059', '\u030A', '\u0000', '\u0041', '\u02BE', '\u0000', '\u03A5', '\u0313', '\u0000', '\u03A5', '\u0313',
			'\u0300', '\u03A5', '\u0313', '\u0301', '\u03A5', '\u0313', '\u0342', '\u1F08', '\u0399', '\u0000', '\u1F09', '\u0399', '\u0000', '\u1F0A',
			'\u0399', '\u0000', '\u1F0B', '\u0399', '\u0000', '\u1F0C', '\u0399', '\u0000', '\u1F0D', '\u0399', '\u0000', '\u1F0E', '\u0399', '\u0000',
			'\u1F0F', '\u0399', '\u0000', '\u1F08', '\u0399', '\u0000', '\u1F09', '\u0399', '\u0000', '\u1F0A', '\u0399', '\u0000', '\u1F0B', '\u0399',
			'\u0000', '\u1F0C', '\u0399', '\u0000', '\u1F0D', '\u0399', '\u0000', '\u1F0E', '\u0399', '\u0000', '\u1F0F', '\u0399', '\u0000', '\u1F28',
			'\u0399', '\u0000', '\u1F29', '\u0399', '\u0000', '\u1F2A', '\u0399', '\u0000', '\u1F2B', '\u0399', '\u0000', '\u1F2C', '\u0399', '\u0000',
			'\u1F2D', '\u0399', '\u0000', '\u1F2E', '\u0399', '\u0000', '\u1F2F', '\u0399', '\u0000', '\u1F28', '\u0399', '\u0000', '\u1F29', '\u0399',
			'\u0000', '\u1F2A', '\u0399', '\u0000', '\u1F2B', '\u0399', '\u0000', '\u1F2C', '\u0399', '\u0000', '\u1F2D', '\u0399', '\u0000', '\u1F2E',
			'\u0399', '\u0000', '\u1F2F', '\u0399', '\u0000', '\u1F68', '\u0399', '\u0000', '\u1F69', '\u0399', '\u0000', '\u1F6A', '\u0399', '\u0000',
			'\u1F6B', '\u0399', '\u0000', '\u1F6C', '\u0399', '\u0000', '\u1F6D', '\u0399', '\u0000', '\u1F6E', '\u0399', '\u0000', '\u1F6F', '\u0399',
			'\u0000', '\u1F68', '\u0399', '\u0000', '\u1F69', '\u0399', '\u0000', '\u1F6A', '\u0399', '\u0000', '\u1F6B', '\u0399', '\u0000', '\u1F6C',
			'\u0399', '\u0000', '\u1F6D', '\u0399', '\u0000', '\u1F6E', '\u0399', '\u0000', '\u1F6F', '\u0399', '\u0000', '\u1FBA', '\u0399', '\u0000',
			'\u0391', '\u0399', '\u0000', '\u0386', '\u0399', '\u0000', '\u0391', '\u0342', '\u0000', '\u0391', '\u0342', '\u0399', '\u0391', '\u0399',
			'\u0000', '\u1FCA', '\u0399', '\u0000', '\u0397', '\u0399', '\u0000', '\u0389', '\u0399', '\u0000', '\u0397', '\u0342', '\u0000', '\u0397',
			'\u0342', '\u0399', '\u0397', '\u0399', '\u0000', '\u0399', '\u0308', '\u0300', '\u0399', '\u0308', '\u0301', '\u0399', '\u0342', '\u0000',
			'\u0399', '\u0308', '\u0342', '\u03A5', '\u0308', '\u0300', '\u03A5', '\u0308', '\u0301', '\u03A1', '\u0313', '\u0000', '\u03A5', '\u0342',
			'\u0000', '\u03A5', '\u0308', '\u0342', '\u1FFA', '\u0399', '\u0000', '\u03A9', '\u0399', '\u0000', '\u038F', '\u0399', '\u0000', '\u03A9',
			'\u0342', '\u0000', '\u03A9', '\u0342', '\u0399', '\u03A9', '\u0399', '\u0000', '\u0046', '\u0046', '\u0000', '\u0046', '\u0049', '\u0000',
			'\u0046', '\u004C', '\u0000', '\u0046', '\u0046', '\u0049', '\u0046', '\u0046', '\u004C', '\u0053', '\u0054', '\u0000', '\u0053', '\u0054',
			'\u0000', '\u0544', '\u0546', '\u0000', '\u0544', '\u0535', '\u0000', '\u0544', '\u053B', '\u0000', '\u054E', '\u0546', '\u0000', '\u0544',
			'\u053D', '\u0000' };
	private static char[] upperIndexs = { '\u000B', '\u0000', '\f', '\u0000', '\r', '\u0000', '\u000E', '\u0000', '\u0000', '\u0000', '\u0000',
			'\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000',
			'\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000',
			'\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u000F', '\u0010', '\u0011', '\u0012', '\u0013',
			'\u0014', '\u0015', '\u0016', '\u0017', '\u0018', '\u0019', '\u001A', '\u001B', '\u001C', '\u001D', '\u001E', '\u001F', '\u0020', '\u0021',
			'\u0022', '\u0023', '\u0024', '\u0025', '\u0026', '\'', '\u0028', '\u0029', '\u002A', '\u002B', '\u002C', '\u002D', '\u002E', '\u002F',
			'\u0030', '\u0031', '\u0032', '\u0033', '\u0034', '\u0035', '\u0036', '\u0037', '\u0038', '\u0039', '\u003A', '\u003B', '\u003C', '\u003D',
			'\u003E', '\u0000', '\u0000', '\u003F', '\u0040', '\u0041', '\u0000', '\u0042', '\u0043', '\u0000', '\u0000', '\u0000', '\u0000', '\u0044',
			'\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0045', '\u0046', '\u0047', '\u0000', '\u0048', '\u0049', '\u0000', '\u0000', '\u0000',
			'\u0000', '\u004A', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u004B', '\u004C', '\u0000', '\u0000', '\u004D', '\u004E', '\u0000',
			'\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u004F', '\u0050', '\u0051', '\u0000', '\u0052',
			'\u0053', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0000', '\u0054', '\u0055', '\u0056',
			'\u0000', '\u0057', '\u0058', '\u0000', '\u0000', '\u0000', '\u0000', '\u0059' };

	private static boolean isCombiningAbove(int codePoint) {
		if (codePoint < 0xFFFF) {
			int index = binarySearchRange(startCombiningAbove, (char) codePoint);

			return index >= 0 && endCombiningAbove[index] >= codePoint;
		} else if ((codePoint >= 0x1D185 && codePoint <= 0x1D189) || (codePoint >= 0x1D1AA && codePoint <= 0x1D1AD)) {
			return true;
		}

		return false;
	}

	private boolean isWordPart(int codePoint) {
		return codePoint == 0x345 || isWordStart(codePoint);
	}

	private boolean isWordStart(int codePoint) {
		int type = Character.getType(codePoint);

		return (type >= Character.UPPERCASE_LETTER && type <= Character.TITLECASE_LETTER) || (codePoint >= 0x2B0 && codePoint <= 0x2B8)
				|| (codePoint >= 0x2C0 && codePoint <= 0x2C1) || (codePoint >= 0x2E0 && codePoint <= 0x2E4) || codePoint == 0x37A
				|| (codePoint >= 0x2160 && codePoint <= 0x217F) || (codePoint >= 0x1D2C && codePoint <= 0x1D61);
	}

	private char convertSigma(int pos) {
		if (pos == 0 || !isWordStart(codePointBefore(pos)) || ((pos + 1) < lengthInternal() && isWordPart(codePointAt(pos + 1)))) {
			return '\u03C3';
		}
		return '\u03C2';
	}

	/**
	 * Answers a string containing a concise, human-readable description of the receiver.
	 *
	 * @return this String
	 */
	public String toString() {
		return this;
	}

	/**
	 * Converts the characters in this String to uppercase, using the default Locale. To convert to upper case independent of any locale, use
	 * toUpperCase(Locale.ROOT).
	 *
	 * @return a String containing the uppercase characters equivalent to the characters in this String
	 */
	public String toUpperCase() {
		return toUpperCase(Locale.getDefault());
	}

	/**
	 * Return the index of the specified character into the upperValues table. The upperValues table contains three entries at each position. These
	 * three characters are the upper case conversion. If only two characters are used, the third character in the table is \u0000.
	 *
	 * @param ch
	 *			  the char being converted to upper case
	 *
	 * @return the index into the upperValues table, or -1
	 */
	private int upperIndex(int ch) {
		int index = -1;

		if (ch <= 0x587) {
			if (ch == 0xDF) {
				index = 0;
			} else if (ch <= 0x149) {
				if (ch == 0x149) {
					index = 1;
				}
			} else if (ch <= 0x1F0) {
				if (ch == 0x1F0) {
					index = 2;
				}
			} else if (ch <= 0x390) {
				if (ch == 0x390) {
					index = 3;
				}
			} else if (ch <= 0x3B0) {
				if (ch == 0x3B0) {
					index = 4;
				}
			} else if (ch <= 0x587) {
				if (ch == 0x587) {
					index = 5;
				}
			}
		} else if (ch >= 0x1E96) {
			if (ch <= 0x1E9A) {
				index = 6 + ch - 0x1E96;
			} else if (ch >= 0x1F50 && ch <= 0x1FFC) {
				index = upperIndexs[ch - 0x1F50];

				if (index == 0) {
					index = -1;
				}
			} else if (ch >= 0xFB00) {
				if (ch <= 0xFB06) {
					index = 90 + ch - 0xFB00;
				} else if (ch >= 0xFB13 && ch <= 0xFB17) {
					index = 97 + ch - 0xFB13;
				}
			}
		}

		return index;
	}

	/**
	 * Converts the characters in this String to uppercase, using the specified Locale.
	 *
	 * @param locale
	 *			  the Locale
	 * @return a String containing the uppercase characters equivalent to the characters in this String
	 */
	public String toUpperCase(Locale locale) {
		String language = locale.getLanguage();

		if (isEmpty()) {
			return this;
		}

		if (StrHWAvailable() && language == "en") { //$NON-NLS-1$
			String output = toUpperHWOptimized(new String(lengthInternal()));

			if (output != null)
				return output;
		}

		return toUpperCaseCore(language);
	}

	/**
	 * The core of upper case conversion. This is the old, not-as-fast path.
	 *
	 * @param language
	 *			  the string representing the locale
	 * @return the upper case string
	 */
	private String toUpperCaseCore(String language) {
		/*[IF]*/
		// TODO : How do we handle the language here properly?
		// TODO : Need to "compressify" this method. NOT all chars in range [0, 255] have toUpperCase also in [0, 255].
		/*
		 * char[] c = new char[256];
		 * 
		 * for (int i = 0; i <= 255; ++i) { c[i] = (char) i; }
		 * 
		 * String s1 = new String(c);
		 * 
		 * String s2 = s1.toUpperCase();
		 * 
		 * for (int i = 0; i <= 255; ++i) { if (s1.charAt(i) != s2.charAt(i)) { int i1 = (int) s1.charAt(i); int i2 = (int) s2.charAt(i);
		 * System.out.println(" " + i1 + " " + s1.charAt(i) + " -- " + i2 + " " + s2.charAt(i) + "  difference = " + (i2 - i1)); } }
		 */
		/*[ENDIF]*/

		boolean turkishAzeri = language == "tr" || language == "az"; //$NON-NLS-1$ //$NON-NLS-2$
		boolean lithuanian = language == "lt"; //$NON-NLS-1$

		StringBuilder builder = null;

		value.getClass(); // Implicit null check

		int len = lengthInternal();

		for (int i = 0; i < len; i++) {
			int codePoint = charAtInternal(i);

			if (codePoint >= Character.MIN_HIGH_SURROGATE && codePoint <= Character.MAX_HIGH_SURROGATE) {
				codePoint = codePointAt(i);
			}

			int index = -1;

			if (codePoint >= 0xDF && codePoint <= 0xFB17) {
				index = upperIndex(codePoint);
			}

			if (index == -1) {
				int upper = (!turkishAzeri || codePoint != 0x69) ? toUpperCase(codePoint) : 0x130;

				if (codePoint != upper) {
					if (builder == null) {
						builder = new StringBuilder(len);
						
						// Check if the String is compressed
						if (enableCompression && count >= 0) {
							builder.append(value, 0, i, true);
						} else {
							builder.append(value, 0, i, false);
						}
					}

					builder.appendCodePoint(upper);
				} else if (builder != null) {
					builder.appendCodePoint(codePoint);
				}

				/*[IF]*/
				// TODO : This string will be compressed. Need to pull it out to a char[] array similar to others. See above.
				/*[ENDIF]*/
				if (lithuanian && codePoint <= 0x1ECB && (i + 1) < len && charAt(i + 1) == '\u0307'
						&& "ij\u012F\u0268\u0456\u0458\u1E2D\u1ECB".indexOf(codePoint, 0) != -1) //$NON-NLS-1$
				{
					++i;
				}
			} else {
				if (builder == null) {
					builder = new StringBuilder(len + (len / 6) + 2).append(this, 0, i);
				}

				int target = index * 3;

				builder.append(upperValues[target]);
				builder.append(upperValues[target + 1]);

				char val = upperValues[target + 2];

				if (val != 0) {
					builder.append(val);
				}
			}

			if (codePoint >= Character.MIN_SUPPLEMENTARY_CODE_POINT) {
				++i;
			}
		}

		if (builder == null) {
			return this;
		}

		return builder.toString();
	}

	/**
	 * Removes white space characters from the beginning and end of the string.
	 *
	 * @return a String with characters <code><= \\u0020</code> removed from the beginning and the end
	 */
	public String trim() {
		int start = 0;
		int last = lengthInternal() - 1;
		int end = last;

		value.getClass(); // Implicit null check

		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			while ((start <= end) && (helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, start)) <= ' ')) {
				start++;
			}

			while ((end >= start) && (helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, end)) <= ' ')) {
				end--;
			}

			if (start == 0 && end == last) {
				return this;
			} else {
				return new String(value, start, end - start + 1, true);
			}
		} else {
			while ((start <= end) && (charAtInternal(start) <= ' ')) {
				start++;
			}

			while ((end >= start) && (charAtInternal(end) <= ' ')) {
				end--;
			}

			if (start == 0 && end == last) {
				return this;
			} else {
				return new String(value, start, end - start + 1, false);
			}
		}
	}

	/**
	 * Returns a String containing the characters in the specified character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *			  the array of characters
	 * @return the String
	 *
	 * @throws NullPointerException
	 *				when data is null
	 */
	public static String valueOf(char[] data) {
		return new String(data, 0, data.length);
	}

	/**
	 * Returns a String containing the specified characters in the character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *			  the array of characters
	 * @param start
	 *			  the starting offset in the character array
	 * @param length
	 *			  the number of characters to use
	 * @return the String
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>length < 0, start < 0</code> or <code>start + length > data.length</code>
	 * @throws NullPointerException
	 *				when data is null
	 */
	public static String valueOf(char[] data, int start, int length) {
		return new String(data, start, length);
	}

	/**
	 * Converts the specified character to its string representation.
	 *
	 * @param value
	 *			  the character
	 * @return the character converted to a string
	 */
	public static String valueOf(char value) {
		String string;

		if (value <= 255) {
		   if (enableCompression) {
		       string = new String(compressedAsciiTable[value], 0, 1, true);
		   } else {
		       string = new String(decompressedAsciiTable[value], 0, 1, false);
		   }
		} else {
			/*[IF Sidecar19-SE]*/
			byte[] buffer = new byte[2];
			
			helpers.putCharInArrayByIndex(buffer, 0, value);
			
		   string = new String(buffer, 0, 1, false);
			/*[ELSE]*/
			string = new String(new char[] { value }, 0, 1, false);
			/*[ENDIF]*/
		}

		return string;
	}

	/**
	 * Converts the specified double to its string representation.
	 *
	 * @param value
	 *			  the double
	 * @return the double converted to a string
	 */
	public static String valueOf(double value) {
		return Double.toString(value);
	}

	/**
	 * Converts the specified float to its string representation.
	 *
	 * @param value
	 *			  the float
	 * @return the float converted to a string
	 */
	public static String valueOf(float value) {
		return Float.toString(value);
	}

	/**
	 * Converts the specified integer to its string representation.
	 *
	 * @param value
	 *			  the integer
	 * @return the integer converted to a string
	 */
	public static String valueOf(int value) {
		return Integer.toString(value);
	}

	/**
	 * Converts the specified long to its string representation.
	 *
	 * @param value
	 *			  the long
	 * @return the long converted to a string
	 */
	public static String valueOf(long value) {
		return Long.toString(value);
	}

	/**
	 * Converts the specified object to its string representation. If the object is null answer the string <code>"null"</code>, otherwise use
	 * <code>toString()</code> to get the string representation.
	 *
	 * @param value
	 *			  the object
	 * @return the object converted to a string
	 */
	public static String valueOf(Object value) {
		return value != null ? value.toString() : "null"; //$NON-NLS-1$
	}

	/**
	 * Converts the specified boolean to its string representation. When the boolean is true answer <code>"true"</code>, otherwise answer
	 * <code>"false"</code>.
	 *
	 * @param value
	 *			  the boolean
	 * @return the boolean converted to a string
	 */
	public static String valueOf(boolean value) {
		return value ? "true" : "false"; //$NON-NLS-1$ //$NON-NLS-2$
	}

	/**
	 * Answers whether the characters in the StringBuffer buffer are the same as those in this String.
	 *
	 * @param buffer
	 *			  the StringBuffer to compare this String to
	 * @return true when the characters in buffer are identical to those in this String. If they are not, false will be returned.
	 *
	 * @throws NullPointerException
	 *				when buffer is null
	 *
	 * @since 1.4
	 */
	public boolean contentEquals(StringBuffer buffer) {
		synchronized (buffer) {
			int size = buffer.length();

			if (lengthInternal() != size) {
				return false;
			}
			
			if (enableCompression && buffer.isCompressed()) {
				return regionMatches(0, new String(buffer.getValue(), 0, size, true), 0, size);
			} else {
				return regionMatches(0, new String(buffer.getValue(), 0, size, false), 0, size);
			}
		}
	}

	/**
	 * Determines whether a this String matches a given regular expression.
	 *
	 * @param expr
	 *			  the regular expression to be matched
	 * @return true if the expression matches, otherwise false
	 *
	 * @throws PatternSyntaxException
	 *				if the syntax of the supplied regular expression is not valid
	 * @throws NullPointerException
	 *				if expr is null
	 *
	 * @since 1.4
	 */
	public boolean matches(String expr) {
		return Pattern.matches(expr, this);
	}

	/**
	 * Replace any substrings within this String that match the supplied regular expression expr, with the String substitute.
	 *
	 * @param expr
	 *			  the regular expression to match
	 * @param substitute
	 *			  the string to replace the matching substring with
	 * @return the new string
	 *
	 * @throws NullPointerException
	 *				if expr is null
	 *
	 * @since 1.4
	 */
	public String replaceAll(String expr, String substitute) {
		return Pattern.compile(expr).matcher(this).replaceAll(substitute);
	}

	/**
	 * Replace any substrings within this String that match the supplied regular expression expr, with the String substitute.
	 *
	 * @param expr
	 *			  the regular expression to match
	 * @param substitute
	 *			  the string to replace the matching substring with
	 * @return the new string
	 *
	 * @throws NullPointerException
	 *				if expr is null
	 *
	 * @since 1.4
	 */
	public String replaceFirst(String expr, String substitute) {
		return Pattern.compile(expr).matcher(this).replaceFirst(substitute);
	}

	/**
	 * Splits this string around matches of the given regular expression. Calling this method is same as calling split(regex,0). Therefore, empty
	 * string(s) at the end of the returned array will be discarded.
	 *
	 *
	 * @param regex
	 *			  Regular expression that is used as a delimiter
	 * @return The array of strings which are splitted around the regex
	 *
	 * @throws PatternSyntaxException
	 *				if the syntax of regex is invalid
	 *
	 * @since 1.4
	 */
	public String[] split(String regex) {
		return split(regex, 0);
	}

	private static final char[] regexMetaChars = new char[]
		{ '.', '$', '|', '(', ')', '[', ']', '{', '}', '^', '?', '*', '+', '\\' };

	private static final boolean hasMetaChars(String s) {
		for (int i = 0; i < s.lengthInternal(); ++i) {
			char ch = s.charAtInternal(i);

			// Note the surrogate ranges are HIGH: \uD800-\uDBFF; LOW: \uDC00-\uDFFF
			// this check is, therefore, equivalent to returning true if the character
			// falls anywhere in this range including the range between MAX_LOW_SURROGATE
			// and MIN_HIGH_SURROGATE which happen to be adjacent
			if (ch >= Character.MIN_HIGH_SURROGATE
					&& ch <= Character.MAX_LOW_SURROGATE) { return true; }

			for (int j = 0; j < regexMetaChars.length; ++j) {
				if (ch == regexMetaChars[j]) { return true; }
			}
		}
		return false;
	}

	/**
	 * Splits this String using the given regular expression.
	 *
	 * max controls the number of times the regex is applied to this string.
	 * If max is positive, then regex can be applied to this String max-1 times.
	 * The returned array size can not be bigger than max, and the last element of
	 * the returned array contains all input after the last match of the regex.
	 * If max is negative or zero, then regex can be applied to this string as many times as
	 * possible and there is no size limit in the returned array.
	 * If max is 0, all the empty string(s) at the end of the returned array will be discarded.
	 * 
	 * 
	 * 
	 * @param regex Regular expression that is used as a delimiter
	 * @param max The threshold of the returned array
	 * @return The array of strings which are splitted around the regex
	 *
	 * @throws PatternSyntaxException if the syntax of regex is invalid
	 *
	 * @since 1.4
	 */
	public String[] split(String regex, int max) {
		// it is faster to handle simple splits inline (i.e. no fancy regex matching)
		// so we test for a suitable string and handle this here if we can
		if (regex != null && regex.lengthInternal() > 0 && !hasMetaChars(regex)) {
			if (max == 1) {
				return new String[] { this };
			}
			java.util.ArrayList<String> parts = new java.util.ArrayList<String>((max > 0 && max < 100) ? max : 10);

			/*[IF Sidecar19-SE]*/
			byte[]
			/*[ELSE]*/
			char[]
			/*[ENDIF]*/
			chars = this.value;

			final boolean compressed = enableCompression && count >= 0;

			chars.getClass();
			int start = 0, current = 0, end = lengthInternal();
			if (regex.lengthInternal() == 1) {
				char splitChar = regex.charAtInternal(0);
				while (current < end) {
					if (charAtInternal(current, chars) == splitChar) {
						parts.add(new String(chars, start, current - start, compressed));
						start = current + 1;
						if (max > 0 && parts.size() == max - 1) {
							parts.add(new String(chars, start, end - start, compressed));
							break;
						}
					}
					current = current + 1;
				}
			} else {
				int rLength = regex.lengthInternal();

				/*[IF Sidecar19-SE]*/
				byte[]
				/*[ELSE]*/
				char[]
				/*[ENDIF]*/
				splitChars = regex.value;

				splitChars.getClass();
				char firstChar = charAtInternal(0, regex.value);
				while (current < end) {
					if (charAtInternal(current, chars) == firstChar) {
						int idx = current + 1;
						int matchIdx = 1;
						while (matchIdx < rLength && idx < end) {
							if (charAtInternal(idx, chars) != charAtInternal(matchIdx, splitChars)) {
								break;
							}
							matchIdx++;
							idx++;
						}
						if (matchIdx == rLength) {
							parts.add(new String(chars, start, current - start, compressed));
							start = current + rLength;
							if (max > 0 && parts.size() == max - 1) {
								parts.add(new String(chars, start, end - start, compressed));
								break;
							}
							current = current + rLength;
							continue;
						}
					}
					current = current + 1;
				}
			}
			if (parts.size() == 0) {
				return new String[] { this };
			} else if (start <= current && parts.size() != max) {
				parts.add(new String(chars, start, current - start, compressed));
			}
			if (max == 0) {
				end = parts.size();
				while (end > 0 && parts.get(end - 1).lengthInternal() == 0) {
					end -= 1;
					parts.remove(end);
				}
			}
			return parts.toArray(new String[parts.size()]);
		}
		return Pattern.compile(regex).split(this, max);
	}

	/**
	 * Has the same result as the substring function, but is present so that String may implement the CharSequence interface.
	 *
	 * @param start
	 *			  the offset the first character
	 * @param end
	 *			  the offset of one past the last character to include
	 *
	 * @return the subsequence requested
	 *
	 * @throws IndexOutOfBoundsException
	 *				when start or end is less than zero, start is greater than end, or end is greater than the length of the String.
	 *
	 * @see java.lang.CharSequence#subSequence(int, int)
	 *
	 * @since 1.4
	 */
	public CharSequence subSequence(int start, int end) {
		return substring(start, end);
	}

	/**
	 * @param data
	 *			  the byte array to convert to a String
	 * @param start
	 *			  the starting offset in the byte array
	 * @param length
	 *			  the number of bytes to convert
	 *
	 * @since 1.5
	 */
	public String(int[] data, int start, int length) {
		if (start >= 0 && 0 <= length && length <= data.length - start) {
			int size = 0;

			// Optimistically assume we can compress data[]
			boolean compressible = enableCompression;
					
			for (int i = start; i < start + length; ++i) {
				int codePoint = data[i];

				if (codePoint < Character.MIN_CODE_POINT) {
					throw new IllegalArgumentException();
				} else if (codePoint < Character.MIN_SUPPLEMENTARY_CODE_POINT) {
					if (compressible && codePoint > 255) {
						compressible = false;
					}
					
					++size;
				} else if (codePoint <= Character.MAX_CODE_POINT) {
					if (compressible) {
						codePoint -= Character.MIN_SUPPLEMENTARY_CODE_POINT;
	
						int codePoint1 = Character.MIN_HIGH_SURROGATE + (codePoint >> 10);
						int codePoint2 = Character.MIN_LOW_SURROGATE + (codePoint & 0x3FF);
	
						if (codePoint1 > 255 || codePoint2 > 255) {
							compressible = false;
						}
					}
					
					size += 2;
				} else {
					throw new IllegalArgumentException();
				}
			}

			if (compressible) {
				/*[IF Sidecar19-SE]*/
				value = new byte[size];
				/*[ELSE]*/
				value = new char[(size + 1) / 2];
				/*[ENDIF]*/
				count = size;

				for (int i = start, j = 0; i < start + length; ++i) {
					int codePoint = data[i];

					if (codePoint < Character.MIN_SUPPLEMENTARY_CODE_POINT) {
						helpers.putByteInArrayByIndex(value, j++, (byte) codePoint);
					} else {
						codePoint -= Character.MIN_SUPPLEMENTARY_CODE_POINT;

						int codePoint1 = Character.MIN_HIGH_SURROGATE + (codePoint >> 10);
						int codePoint2 = Character.MIN_LOW_SURROGATE + (codePoint & 0x3FF);

						helpers.putByteInArrayByIndex(value, j++, (byte) codePoint1);
						helpers.putByteInArrayByIndex(value, j++, (byte) codePoint2);
					}
				}
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[size * 2];
				/*[ELSE]*/
				value = new char[size];
				/*[ENDIF]*/

				if (enableCompression) {
					count = size | uncompressedBit;
					
					initCompressionFlag();
				} else {
					count = size;
				}

				for (int i = start, j = 0; i < start + length; ++i) {
					int codePoint = data[i];

					if (codePoint < Character.MIN_SUPPLEMENTARY_CODE_POINT) {
						/*[IF Sidecar19-SE]*/
						helpers.putCharInArrayByIndex(value, j++, (char) codePoint);
						/*[ELSE]*/
						value[j++] = (char) codePoint;
						/*[ENDIF]*/
					} else {
						codePoint -= Character.MIN_SUPPLEMENTARY_CODE_POINT;

						/*[IF Sidecar19-SE]*/
						int codePoint1 = Character.MIN_HIGH_SURROGATE + (codePoint >> 10);
						int codePoint2 = Character.MIN_LOW_SURROGATE + (codePoint & 0x3FF);

						helpers.putCharInArrayByIndex(value, j++, (char) codePoint1);
						helpers.putCharInArrayByIndex(value, j++, (char) codePoint2);
						/*[ELSE]*/
						value[j++] = (char) (Character.MIN_HIGH_SURROGATE + (codePoint >> 10));
						value[j++] = (char) (Character.MIN_LOW_SURROGATE + (codePoint & 0x3FF));
						/*[ENDIF]*/
					}
				}
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Creates a string from the contents of a StringBuilder.
	 *
	 * @param builder
	 *			  the StringBuilder
	 *
	 * @since 1.5
	 */
	public String(StringBuilder builder) {
		/*[IF Sidecar19-SE]*/
		byte[] chars = builder.shareValue();
		/*[ELSE]*/
		char[] chars = builder.shareValue();
		/*[ENDIF]*/

		if (enableCompression) {
			if (builder.isCompressed()) {
				value = chars;
				count = builder.lengthInternal();
			} else {
				value = chars;
				count = builder.lengthInternal() | uncompressedBit;
				
				initCompressionFlag();
			}
		} else {
			value = chars;
			count = builder.lengthInternal();
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Returns the Unicode character at the given point.
	 *
	 * @param index
	 *			  the character index
	 * @return the Unicode character value at the index
	 *
	 * @since 1.5
	 */
	public int codePointAt(int index) {
		int len = lengthInternal();

		if (index >= 0 && index < len) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
			} else {
				int high = charAtInternal(index);

				if ((index + 1) < len && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
					int low = charAtInternal(index + 1);

					if (low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
						return Character.MIN_SUPPLEMENTARY_CODE_POINT + ((high - Character.MIN_HIGH_SURROGATE) << 10) + (low - Character.MIN_LOW_SURROGATE);
					}
				}

				return high;
			}
		} else {
			throw new StringIndexOutOfBoundsException(index);
		}
	}

	/**
	 * Returns the Unicode character before the given point.
	 *
	 * @param index
	 *			  the character index
	 * @return the Unicode character value before the index
	 *
	 * @since 1.5
	 */
	public int codePointBefore(int index) {
		int len = lengthInternal();

		if (index > 0 && index <= len) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index - 1));
			} else {
				int low = charAtInternal(index - 1);

				if (index > 1 && low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
					int high = charAtInternal(index - 2);

					if (high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
						return Character.MIN_SUPPLEMENTARY_CODE_POINT + ((high - Character.MIN_HIGH_SURROGATE) << 10) + (low - Character.MIN_LOW_SURROGATE);
					}
				}

				return low;
			}
		} else {
			throw new StringIndexOutOfBoundsException(index);
		}
	}

	/**
	 * Returns the total Unicode values in the specified range.
	 *
	 * @param start
	 *			  first index
	 * @param end
	 *			  last index
	 * @return the total Unicode values
	 *
	 * @since 1.5
	 */
	public int codePointCount(int start, int end) {
		int len = lengthInternal();

		if (start >= 0 && start <= end && end <= len) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				return end - start;
			} else {
				int count = 0;

				for (int i = start; i < end; ++i) {
					int high = charAtInternal(i);

					if (i + 1 < end && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
						int low = charAtInternal(i + 1);

						if (low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
							++i;
						}
					}

					++count;
				}

				return count;
			}
		} else {
			throw new IndexOutOfBoundsException();
		}
	}

	/**
	 * Returns the index of the code point that was offset by <code>codePointCount</code>.
	 *
	 * @param start
	 *			  the position to offset
	 * @param codePointCount
	 *			  the code point count
	 * @return the offset index
	 *
	 * @since 1.5
	 */
	public int offsetByCodePoints(int start, int codePointCount) {
		int len = lengthInternal();

		if (start >= 0 && start <= len) {
			// Check if the String is compressed
			if (enableCompression && count >= 0) {
				int index = start + codePointCount;

				if (index >= len) {
					throw new IndexOutOfBoundsException();
				} else {
					return index;
				}
			} else {
				int index = start;

				if (codePointCount == 0) {
					return start;
				} else if (codePointCount > 0) {
					for (int i = 0; i < codePointCount; ++i) {
						if (index == len) {
							throw new IndexOutOfBoundsException();
						}

						int high = charAtInternal(index);

						if ((index + 1) < len && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
							int low = charAtInternal(index + 1);

							if (low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
								index++;
							}
						}

						index++;
					}
				} else {
					for (int i = codePointCount; i < 0; ++i) {
						if (index < 1) {
							throw new IndexOutOfBoundsException();
						}

						int low = charAtInternal(index - 1);

						if (index > 1 && low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
							int high = charAtInternal(index - 2);

							if (high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
								index--;
							}
						}

						index--;
					}
				}

				return index;
			}
		} else {
			throw new IndexOutOfBoundsException();
		}
	}

	/**
	 * Compares the content of the character sequence to this String
	 *
	 * @param sequence
	 *			  the character sequence
	 * @return <code>true</code> if the content of this String is equal to the character sequence, <code>false</code> otherwise.
	 *
	 * @since 1.5
	 */
	public boolean contentEquals(CharSequence sequence) {
		int len = lengthInternal();

		if (len != sequence.length()) {
			return false;
		}

		for (int i = 0; i < len; ++i) {
			if (charAtInternal(i) != sequence.charAt(i)) {
				return false;
			}
		}

		return true;
	}

	/**
	 * @param sequence
	 *			  the sequence to compare to
	 * @return <code>true</code> if this String contains the sequence, <code>false</code> otherwise.
	 *
	 * @since 1.5
	 */
	public boolean contains(CharSequence sequence) {
		int len = lengthInternal();

		int sequencelen = sequence.length();

		if (sequencelen > len) {
			return false;
		}

		int start = 0;

		if (sequencelen > 0) {
			if (sequencelen + start > len) {
				return false;
			}

			char charAt0 = sequence.charAt(0);

			while (true) {
				int i = indexOf(charAt0, start);

				if (i == -1 || sequencelen + i > len) {
					return false;
				}

				int o1 = i;
				int o2 = 0;

				while (++o2 < sequencelen && charAtInternal(++o1) == sequence.charAt(o2))
					;

				if (o2 == sequencelen) {
					return true;
				}

				start = i + 1;
			}
		} else {
			return true;
		}
	}

	/**
	 * @param sequence1
	 *			  the old character sequence
	 * @param sequence2
	 *			  the new character sequence
	 * @return the new String
	 *
	 * @since 1.5
	 */
	public String replace(CharSequence sequence1, CharSequence sequence2) {
		if (sequence2 == null) {
			throw new NullPointerException();
		}

		int len = lengthInternal();

		int sequence1len = sequence1.length();

		if (sequence1len == 0) {
			StringBuilder builder = new StringBuilder((len + 1) * sequence2.length());

			builder.append(sequence2);

			for (int i = 0; i < len; ++i) {
				builder.append(charAt(i)).append(sequence2);
			}

			return builder.toString();
		} else {
			StringBuilder builder = new StringBuilder();

			int start = 0, copyStart = 0, firstIndex;

			char charAt0 = sequence1.charAt(0);

			while (start < len) {
				if ((firstIndex = indexOf(charAt0, start)) == -1) {
					break;
				}

				boolean found = true;

				if (sequence1.length() > 1) {
					if (firstIndex + sequence1len > len) {
						break;
					}

					for (int i = 1; i < sequence1len; i++) {
						if (charAt(firstIndex + i) != sequence1.charAt(i)) {
							found = false;

							break;
						}
					}
				}

				if (found) {
					builder.append(substring(copyStart, firstIndex)).append(sequence2);

					copyStart = start = firstIndex + sequence1len;
				} else {
					start = firstIndex + 1;
				}
			}

			if (builder.length() == 0 && copyStart == 0) {
				return this;
			}

			builder.append(substring(copyStart));

			return builder.toString();
		}
	}

	/**
	 * Format the receiver using the specified format and args.
	 *
	 * @param format
	 *			  the format to use
	 * @param args
	 *			  the format arguments to use
	 *
	 * @return the formatted result
	 *
	 * @see java.util.Formatter#format(String, Object...)
	 */
	public static String format(String format, Object... args) {
		return new Formatter().format(format, args).toString();
	}

	/**
	 * Format the receiver using the specified local, format and args.
	 *
	 * @param locale
	 *			  the locale used to create the Formatter, may be null
	 * @param format
	 *			  the format to use
	 * @param args
	 *			  the format arguments to use
	 *
	 * @return the formatted result
	 *
	 * @see java.util.Formatter#format(String, Object...)
	 */
	public static String format(Locale locale, String format, Object... args) {
		return new Formatter(locale).format(format, args).toString();
	}

	private static final java.io.ObjectStreamField[] serialPersistentFields = {};

	/**
	 * Answers if this String has no characters, a length of zero.
	 *
	 * @return true if this String has no characters, false otherwise
	 *
	 * @since 1.6
	 *
	 * @see #length
	 */
	public boolean isEmpty() {
		return lengthInternal() == 0;
	}

	/**
	 * Converts the byte array to a String using the specified Charset.
	 *
	 * @param data
	 *			  the byte array to convert to a String
	 * @param charset
	 *			  the Charset to use
	 *
	 * @throws NullPointerException
	 *				when data is null
	 *
	 * @since 1.6
	 *
	 * @see #String(byte[], int, int, Charset)
	 * @see #getBytes(Charset)
	 */
	public String(byte[] data, Charset charset) {
		this(data, 0, data.length, charset);
	}

	/**
	 * Converts the byte array to a String using the specified Charset.
	 *
	 * @param data
	 *			  the byte array to convert to a String
	 * @param start
	 *			  the starting offset in the byte array
	 * @param length
	 *			  the number of bytes to convert
	 * @param charset
	 *			  the Charset to use
	 *
	 * @throws IndexOutOfBoundsException
	 *				when <code>length < 0, start < 0</code> or <code>start + length > data.length</code>
	 * @throws NullPointerException
	 *				when data is null
	 *
	 * @since 1.6
	 *
	 * @see #String(byte[], Charset)
	 * @see #getBytes(Charset)
	 */
	public String(byte[] data, int start, int length, Charset charset) {
		if (charset == null) {
			throw new NullPointerException();
		}

		if (start >= 0 && 0 <= length && length <= data.length - start) {
			/*[IF Sidecar19-SE]*/
			StringCoding.Result scResult = StringCoding.decode(charset, data, start, length);
			
			if (enableCompression) {
				if (String.LATIN1 == scResult.coder) {
					value = scResult.value;
					count = scResult.value.length;
				} else {
					value = scResult.value;
					count = scResult.value.length / 2 | uncompressedBit;

					initCompressionFlag();
				}
			} else {
				value = scResult.value;
				count = scResult.value.length / 2;
			}
			/*[ELSE]*/
			char[] chars = StringCoding.decode(charset, data, start, length);
			
			if (enableCompression) {
				if (compressible(chars, 0, chars.length)) {
					value = new char[(chars.length + 1) / 2];
					count = chars.length;
					
					compress(chars, 0, value, 0, chars.length);
				} else {
					value = chars;
					count = chars.length | uncompressedBit;
					
					initCompressionFlag();
				}
			} else {
				value = chars;
				count = chars.length;
			}
			/*[ENDIF]*/
		} else {
			throw new StringIndexOutOfBoundsException();
		}
		/*[IF Java18.3]*/
		coder = coder();
		/*[ENDIF]*/
	}

	/**
	 * Converts this String to a byte encoding using the specified Charset.
	 *
	 * @param charset
	 *			  the Charset to use
	 * @return the byte array encoding of this String
	 *
	 * @since 1.6
	 */
	public byte[] getBytes(Charset charset) {
		int currentLength = lengthInternal();
		
		/*[IF Sidecar19-SE]*/
		byte[] buffer = value;
		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			if (buffer.length != currentLength) {
				buffer = new byte[currentLength];
				compressedArrayCopy(value, 0, buffer, 0, currentLength);
			}
			return StringCoding.encode(charset, String.LATIN1, buffer);
		} else {
			if (buffer.length != currentLength << 1) {
				buffer = new byte[currentLength << 1];
				decompressedArrayCopy(value, 0, buffer, 0, currentLength);
			}
			return StringCoding.encode(charset, String.UTF16, buffer);
		}
		/*[ELSE]*/
		/*[IF]*/
		// TODO : Optimization : Can we specialize these for both char[] and byte[] to avoid this decompression?
		/*[ENDIF]*/
		
		char[] buffer;

		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			buffer = new char[currentLength];
			decompress(value, 0, buffer, 0, currentLength);
		} else {
			buffer = value;
		}
		
		return StringCoding.encode(charset, buffer, 0, currentLength);
		/*[ENDIF]*/
	}

	/**
	 * Creates a new String by putting each element together joined by the delimiter. If an element is null, then "null" is used as string to join.
	 *
	 * @param delimiter
	 *			  Used as joiner to put elements together
	 * @param elements
	 *			  Elements to be joined
	 * @return string of joined elements by delimiter
	 * @throws NullPointerException
	 *				if one of the arguments is null
	 *
	 */
	public static String join(CharSequence delimiter, CharSequence... elements) {
		StringJoiner stringJoiner = new StringJoiner(delimiter);

		for (CharSequence element : elements) {
			stringJoiner.add(element);
		}

		return stringJoiner.toString();
	}

	/**
	 * Creates a new String by putting each element together joined by the delimiter. If an element is null, then "null" is used as string to join.
	 *
	 * @param delimiter
	 *			  Used as joiner to put elements together
	 * @param elements
	 *			  Elements to be joined
	 * @return string of joined elements by delimiter
	 * @throws NullPointerException
	 *				if one of the arguments is null
	 *
	 */
	public static String join(CharSequence delimiter, Iterable<? extends CharSequence> elements) {
		StringJoiner stringJoiner = new StringJoiner(delimiter);

		Iterator<? extends CharSequence> elementsIterator = elements.iterator();

		while (elementsIterator.hasNext()) {
			stringJoiner.add(elementsIterator.next());
		}

		return stringJoiner.toString();
	}

	// DO NOT MODIFY THIS METHOD
	/*
	 * The method is only called once to setup the flag DFP_HW_AVAILABLE Return value true - when JIT compiled this method, replaces it with loadconst
	 * 1 if -Xjit:disableHWAcceleratedStringCaseConv hasn't been supplied false - if still interpreting this method or disabled by VM option
	 */
	private final static boolean StrCheckHWAvailable() {
		return false;
	}

	// DO NOT MODIFY THIS METHOD
	/*
	 * Return value true - when JIT compiled this method, replaces it with loadconst 1 if -Xjit:disableHWAcceleratedStringCaseConv hasn't been supplied
	 * false - if still interpreting this method or disabled by VM option
	 */
	private final static boolean StrHWAvailable() {
		return STRING_OPT_IN_HW;
	}

	// DO NOT CHANGE CONTENTS OF THESE METHODS
	private final String toUpperHWOptimized(String input) {
		input = toUpperCaseCore("en"); //$NON-NLS-1$

		return input;
	}

	private final String toLowerHWOptimized(String input) {
		input = toLowerCaseCore("en");//$NON-NLS-1$

		return input;
	}
	
/*[IF Sidecar19-SE]*/
	static void checkBoundsBeginEnd(int begin, int end, int length) {
		if ((begin >= 0) && (begin <= end) && (end <= length)) {
			return;
		}
		throw newStringIndexOutOfBoundsException(begin, end, length);
	}
/*[ENDIF]*/	
}
