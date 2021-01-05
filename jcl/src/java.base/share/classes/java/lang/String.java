/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 1998, 2020 IBM Corp. and others
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
package java.lang;

import java.io.Serializable;

import java.util.Locale;
import java.util.Comparator;
import java.io.UnsupportedEncodingException;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;
import java.util.Formatter;
import java.util.StringJoiner;
import java.util.Iterator;
import java.nio.charset.Charset;
/*[IF JAVA_SPEC_VERSION >= 12]*/
import java.util.function.Function;
import java.util.Optional;
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */
/*[IF Sidecar19-SE]*/
import java.util.Spliterator;
import java.util.stream.StreamSupport;

import jdk.internal.misc.Unsafe;
import java.util.stream.IntStream;
/*[ELSE] Sidecar19-SE*/
import sun.misc.Unsafe;
/*[ENDIF] Sidecar19-SE*/

/*[IF JAVA_SPEC_VERSION >= 11]*/
import java.util.stream.Stream;
/*[ENDIF] JAVA_SPEC_VERSION >= 11 */

/*[IF JAVA_SPEC_VERSION >= 12]*/
import java.lang.constant.Constable;
import java.lang.constant.ConstantDesc;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */

/**
 * Strings are objects which represent immutable arrays of characters.
 *
 * @author OTI
 * @version initial
 *
 * @see StringBuffer
 */
public final class String implements Serializable, Comparable<String>, CharSequence
/*[IF JAVA_SPEC_VERSION >= 12]*/
	, Constable, ConstantDesc
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */
{

	/*
	 * Last character of String substitute in String.replaceAll(regex, substitute) can't be \ or $.
	 * The backslash (\) is used to escape literal characters, and the dollar sign ($) is treated as
	 * references to captured subsequences.
	 */
	private void checkLastChar(char lastChar) {
		if (lastChar == '\\') {
			/*[MSG "K0801", "Last character in replacement string can't be \, character to be escaped is required."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0801")); //$NON-NLS-1$
		} else if (lastChar == '$') {
			/*[MSG "K0802", "Last character in replacement string can't be $, group index is required."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0802")); //$NON-NLS-1$
		}
	}

/*[IF Sidecar19-SE]*/
	// DO NOT CHANGE OR MOVE THIS LINE
	// IT MUST BE THE FIRST THING IN THE INITIALIZATION
	private static final long serialVersionUID = -6849794470754667710L;

	/**
	 * Determines whether String compression is enabled.
	 */
	static final boolean enableCompression = com.ibm.oti.vm.VM.J9_STRING_COMPRESSION_ENABLED;

	static final byte LATIN1 = 0;
	static final byte UTF16 = 1;
	static final boolean COMPACT_STRINGS;
	static {
		COMPACT_STRINGS = enableCompression;
	}

	// returns UTF16 when COMPACT_STRINGS is false
	byte coder() {
		if (enableCompression) {
			return coder;
		} else {
			return UTF16;
		}
	}

/*[IF JAVA_SPEC_VERSION >= 16]*/
	/**
	 * Copy bytes from value starting at srcIndex into the bytes array starting at
	 * destIndex. No range checking is needed. Caller ensures bytes is in UTF16.
	 * 
	 * @param bytes copy destination
	 * @param srcIndex index into value
	 * @param destIndex index into bytes
	 * @param coder LATIN1 or UTF16
	 * @param length the number of elements to copy
	 */
	void getBytes(byte[] bytes, int srcIndex, int destIndex, byte coder, int length) {
		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || this.coder == LATIN1)) {
			if (String.LATIN1 == coder) {
				compressedArrayCopy(value, srcIndex, bytes, destIndex, length);
			} else {
				decompress(value, srcIndex, bytes, destIndex, length);
			}
		} else {
			decompressedArrayCopy(value, srcIndex, bytes, destIndex, length);
		}
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 16 */
	
	// no range checking, caller ensures bytes is in UTF16
	// coder is one of LATIN1 or UTF16
	void getBytes(byte[] bytes, int offset, byte coder) {
		int currentLength = lengthInternal();

		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || this.coder == LATIN1)) {
			if (String.LATIN1 == coder) {
				compressedArrayCopy(value, 0, bytes, offset, currentLength);
			} else {
				decompress(value, 0, bytes, offset, currentLength);
			}
		} else {
			decompressedArrayCopy(value, 0, bytes, offset, currentLength);
		}
	}

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

	/**
	 * CaseInsensitiveComparator compares Strings ignoring the case of the characters.
	 */
	private static final class CaseInsensitiveComparator implements Comparator<String>, Serializable {
		static final long serialVersionUID = 8575799808933029326L;

		/**
		 * Compare the two objects to determine the relative ordering.
		 *
		 * @param o1
		 *          an Object to compare
		 * @param o2
		 *          an Object to compare
		 * @return {@code < 0} if o1 is less than o2, {@code 0} if they are equal, and {@code > 0} if o1 is greater
		 *
		 * @exception ClassCastException
		 *          when objects are not the correct type
		 */
		public int compare(String o1, String o2) {
			return o1.compareToIgnoreCase(o2);
		}
	};

	/**
	 * A Comparator which compares Strings ignoring the case of the characters.
	 */
	public static final Comparator<String> CASE_INSENSITIVE_ORDER = new CaseInsensitiveComparator();

	// Used to represent the value of an empty String
	private static final byte[] emptyValue = new byte[0];

	// Used to extract the value of a single ASCII character String by the integral value of the respective character as
	// an index into this table
	private static final byte[][] compressedAsciiTable;

	private static final byte[][] decompressedAsciiTable;

	// Used to access compression related helper methods
	private static final com.ibm.jit.JITHelpers helpers = com.ibm.jit.JITHelpers.getHelpers();

	static class StringCompressionFlag implements Serializable {
		private static final long serialVersionUID = 1346155847239551492L;
	}

	// Singleton used by all String instances to indicate a non-compressed string has been
	// allocated. JIT attempts to fold away the null check involving this static if the
	// StringCompressionFlag class has not been initialized and patches the code to bring back
	// the null check if a non-compressed String is constructed.
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

	private final byte[] value;
	private final byte coder;
	private int hashCode;

	static {
		stringArray = new String[stringArraySize];

		compressedAsciiTable = new byte[256][];

		for (int i = 0; i < compressedAsciiTable.length; ++i) {
			byte[] asciiValue = new byte[1];

			helpers.putByteInArrayByIndex(asciiValue, 0, (byte) i);

			compressedAsciiTable[i] = asciiValue;
		}

		decompressedAsciiTable = new byte[256][];

		for (int i = 0; i < decompressedAsciiTable.length; ++i) {
			byte[] asciiValue = new byte[2];

			helpers.putCharInArrayByIndex(asciiValue, 0, (char) i);

			decompressedAsciiTable[i] = asciiValue;
		}
	}

	static void initCompressionFlag() {
		if (compressionFlag == null) {
			compressionFlag = new StringCompressionFlag();
		}
	}

	/**
	 * Determines whether the input character array can be encoded as a compact
	 * Latin1 string.
	 *
	 * <p>This API implicitly assumes the following:
	 * <blockquote><pre>
	 *     - {@code length >= 0}
	 *     - {@code start >= 0}
	 *     - {@code start + length <= data.length}
	 * <blockquote><pre>
	 *
	 * @param c      the array of characters to check
	 * @param start  the starting offset in the character array
	 * @param length the number of characters to check starting at {@code start}
	 * @return       {@code true} if the input character array can be encoded
	 *               using the Latin1 encoding; {@code false} otherwise
	 */
	static boolean canEncodeAsLatin1(char[] c, int start, int length) {
		for (int i = start; i < start + length; ++i) {
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
		System.arraycopy(array1, start1, array2, start2, length);
	}

	boolean isCompressed() {
		// Check if the String is compressed
		if (enableCompression) {
			if (null == compressionFlag) {
				return true;
			} else {
				return coder == String.LATIN1;
			}
		} else {
			return false;
		}
	}

	String(byte[] byteArray, byte coder) {
		if (enableCompression) {
			if (String.LATIN1 == coder) {
				value = byteArray;
			} else {
				value = byteArray;

				initCompressionFlag();
			}
		} else {
			value = byteArray;
		}
		this.coder = coder;
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

	/**
	 * Answers an empty string.
	 */
	public String() {
		value = emptyValue;

		if (enableCompression) {
			coder = LATIN1;
		} else {
			coder = UTF16;
		}
	}

	/**
	 * Converts the byte array to a String using the default encoding as specified by the file.encoding system property. If the system property is not
	 * defined, the default encoding is ISO8859_1 (ISO-Latin-1). If 8859-1 is not available, an ASCII encoding is used.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 *
	 * @throws NullPointerException
	 *          when data is null
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
	 *          the byte array to convert to a String
	 * @param high
	 *          the high byte to use
	 *
	 * @throws NullPointerException
	 *          when data is null
	 *
	 * @deprecated Use String(byte[]) or String(byte[], String) instead
	 */
	@Deprecated(forRemoval=false, since="1.1")
	public String(byte[] data, int high) {
		this(data, high, 0, data.length);
	}

	/**
	 * Converts the byte array to a String using the default encoding as specified by the file.encoding system property. If the system property is not
	 * defined, the default encoding is ISO8859_1 (ISO-Latin-1). If 8859-1 is not available, an ASCII encoding is used.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
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
			StringCoding.Result scResult = StringCoding.decode(data, start, length);

			value = scResult.value;
			coder = scResult.coder;

			if (enableCompression && scResult.coder == UTF16) {
				initCompressionFlag();
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Converts the byte array to a String, setting the high byte of every character to the specified value.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 * @param high
	 *          the high byte to use
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
	 *
	 * @deprecated Use String(byte[], int, int) instead
	 */
	@Deprecated(forRemoval=false, since="1.1")
	public String(byte[] data, int high, int start, int length) {
		data.getClass(); // Implicit null check

		if (start >= 0 && 0 <= length && length <= data.length - start) {
			if (enableCompression && high == 0) {
				value = new byte[length];
				coder = LATIN1;

				compressedArrayCopy(data, start, value, 0, length);
			} else {
				value = StringUTF16.newBytesFor(length);
				coder = UTF16;

				high <<= 8;

				for (int i = 0; i < length; ++i) {
					helpers.putCharInArrayByIndex(value, i, (char) (high + (data[start++] & 0xff)));
				}

				if (enableCompression) {
					initCompressionFlag();
				}
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Converts the byte array to a String using the specified encoding.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 * @param encoding
	 *          the encoding
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws UnsupportedEncodingException
	 *          when encoding is not supported
	 * @throws NullPointerException
	 *          when data is null
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
		data.getClass(); // Implicit null check
		encoding.getClass(); // Implicit null check

		if (start >= 0 && 0 <= length && length <= data.length - start) {
			StringCoding.Result scResult = StringCoding.decode(encoding, data, start, length);

			value = scResult.value;
			coder = scResult.coder;

			if (enableCompression && scResult.coder == UTF16) {
				initCompressionFlag();
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Converts the byte array to a String using the specified encoding.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 * @param encoding
	 *          the encoding
	 *
	 * @throws UnsupportedEncodingException
	 *          when encoding is not supported
	 * @throws NullPointerException
	 *          when data is null
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

	private String(String s, char c) {
		if (s == null) {
			s = "null"; //$NON-NLS-1$
		}

		int slen = s.lengthInternal();

		int concatlen = slen + 1;
		if (concatlen < 0) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || s.coder == LATIN1) && c <= 255) {
			value = new byte[concatlen];
			coder = LATIN1;

			compressedArrayCopy(s.value, 0, value, 0, slen);

			helpers.putByteInArrayByIndex(value, slen, (byte) c);
		} else {
			value = StringUTF16.newBytesFor(concatlen);
			coder = UTF16;

			decompressedArrayCopy(s.value, 0, value, 0, slen);

			helpers.putCharInArrayByIndex(value, slen, c);

			if (enableCompression) {
				initCompressionFlag();
			}
		}
	}

	/**
	 * Initializes this String to contain the characters in the specified character array. Modifying the character array after creating the String has
	 * no effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 *
	 * @throws NullPointerException
	 *          when data is null
	 */
	public String(char[] data) {
		this(data, 0, data.length);
	}

	/**
	 * Initializes this String to use the specified character array. The character array should not be modified after the String is created.
	 *
	 * @param data
	 *          a non-null array of characters
	 */
	String(char[] data, boolean ignore) {
		if (enableCompression && canEncodeAsLatin1(data, 0, data.length)) {
			value = new byte[data.length];
			coder = LATIN1;

			compress(data, 0, value, 0, data.length);
		} else {
			value = StringUTF16.newBytesFor(data.length);
			coder = UTF16;

			decompressedArrayCopy(data, 0, value, 0, data.length);

			if (enableCompression) {
				initCompressionFlag();
			}
		}
	}

	/**
	 * Initializes this String to contain the specified characters in the character array. Modifying the character array after creating the String has
	 * no effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 * @param start
	 *          the starting offset in the character array
	 * @param length
	 *          the number of characters to use
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
	 */
	public String(char[] data, int start, int length) {
		if (start >= 0 && 0 <= length && length <= data.length - start) {
			if (enableCompression && canEncodeAsLatin1(data, start, length)) {
				value = new byte[length];
				coder = LATIN1;

				compress(data, start, value, 0, length);
			} else {
				value = StringUTF16.newBytesFor(length);
				coder = UTF16;

				decompressedArrayCopy(data, start, value, 0, length);

				if (enableCompression) {
					initCompressionFlag();
				}
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	String(byte[] data, int start, int length, boolean compressed) {
		if (length == 0) {
			value = emptyValue;

			if (enableCompression) {
				coder = LATIN1;
			} else {
				coder = UTF16;
			}
		} else if (length == 1) {
			if (enableCompression && compressed) {
				char theChar = helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(data, start));

				value = compressedAsciiTable[theChar];
				coder = LATIN1;
				hashCode = theChar;
			} else {
				char theChar = helpers.getCharFromArrayByIndex(data, start);

				if (theChar <= 255) {
					value = decompressedAsciiTable[theChar];
				} else {
					value = new byte[2];

					helpers.putCharInArrayByIndex(value, 0, theChar);
				}

				coder = UTF16;
				hashCode = theChar;

				if (enableCompression) {
					initCompressionFlag();
				}
			}
		} else {
			if (enableCompression && compressed) {
				if (start == 0 && data.length == length) {
					value = data;
				} else {
					value = new byte[length];

					compressedArrayCopy(data, start, value, 0, length);
				}

				coder = LATIN1;
			} else {
				if (start == 0 && data.length == length * 2) {
					value = data;
				} else {
					value = StringUTF16.newBytesFor(length);

					decompressedArrayCopy(data, start, value, 0, length);
				}

				coder = UTF16;

				if (enableCompression) {
					initCompressionFlag();
				}
			}
		}
	}

	String(byte[] data, int start, int length, boolean compressed, boolean sharingIsAllowed) {
		if (length == 0) {
			value = emptyValue;

			if (enableCompression) {
				coder = LATIN1;
			} else {
				coder = UTF16;
			}
		} else if (length == 1) {
			if (enableCompression && compressed) {
				char theChar = helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(data, start));

				value = compressedAsciiTable[theChar];
				coder = LATIN1;
				hashCode = theChar;
			} else {
				char theChar = helpers.getCharFromArrayByIndex(data, start);

				if (theChar <= 255) {
					value = decompressedAsciiTable[theChar];
				} else {
					value = new byte[2];

					helpers.putCharInArrayByIndex(value, 0, theChar);
				}

				coder = UTF16;
				hashCode = theChar;

				if (enableCompression) {
					initCompressionFlag();
				}
			}
		} else {
			if (enableCompression && compressed) {
				if (sharingIsAllowed && start == 0 && data.length == length) {
					value = data;
				} else {
					value = new byte[length];

					compressedArrayCopy(data, start, value, 0, length);
				}

				coder = LATIN1;
			} else {
				if (sharingIsAllowed && start == 0 && data.length == length * 2) {
					value = data;
				} else {
					value = StringUTF16.newBytesFor(length);

					decompressedArrayCopy(data, start, value, 0, length);
				}

				coder = UTF16;

				if (enableCompression) {
					initCompressionFlag();
				}
			}
		}
	}

	/**
	 * Creates a string that is a copy of another string
	 *
	 * @param string
	 *          the String to copy
	 */
	public String(String string) {
		value = string.value;
		coder = string.coder;
		hashCode = string.hashCode;
	}

	/**
	 * Creates a string from the contents of a StringBuffer.
	 *
	 * @param buffer
	 *          the StringBuffer
	 */
	public String(StringBuffer buffer) {
		this(buffer.toString());
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
		if (concatlen < 0) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		if (enableCompression && (null == compressionFlag || (s1.coder | s2.coder) == LATIN1)) {
			value = new byte[concatlen];
			coder = LATIN1;

			compressedArrayCopy(s1.value, 0, value, 0, s1len);
			compressedArrayCopy(s2.value, 0, value, s1len, s2len);
		} else {
			value = StringUTF16.newBytesFor(concatlen);
			coder = UTF16;

			// Check if the String is compressed
			if (enableCompression && s1.coder == LATIN1) {
				decompress(s1.value, 0, value, 0, s1len);
			} else {
				decompressedArrayCopy(s1.value, 0, value, 0, s1len);
			}

			// Check if the String is compressed
			if (enableCompression && s2.coder == LATIN1) {
				decompress(s2.value, 0, value, s1len, s2len);
			} else {
				decompressedArrayCopy(s2.value, 0, value, s1len, s2len);
			}

			if (enableCompression) {
				initCompressionFlag();
			}
		}
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

		long totalLen = (long) s1len + (long) s2len + (long) s3len;
		if (totalLen > Integer.MAX_VALUE) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}
		int concatlen = (int) totalLen;

		if (enableCompression && (null == compressionFlag || (s1.coder | s2.coder | s3.coder) == LATIN1)) {
			value = new byte[concatlen];
			coder = LATIN1;

			compressedArrayCopy(s1.value, 0, value, 0, s1len);
			compressedArrayCopy(s2.value, 0, value, s1len, s2len);
			compressedArrayCopy(s3.value, 0, value, s1len + s2len, s3len);
		} else {
			value = StringUTF16.newBytesFor(concatlen);
			coder = UTF16;

			// Check if the String is compressed
			if (enableCompression && s1.coder == LATIN1) {
				decompress(s1.value, 0, value, 0, s1len);
			} else {
				decompressedArrayCopy(s1.value, 0, value, 0, s1len);
			}

			// Check if the String is compressed
			if (enableCompression && s2.coder == LATIN1) {
				decompress(s2.value, 0, value, s1len, s2len);
			} else {
				decompressedArrayCopy(s2.value, 0, value, s1len, s2len);
			}

			// Check if the String is compressed
			if (enableCompression && s3.coder == LATIN1) {
				decompress(s3.value, 0, value, s1len + s2len, s3len);
			} else {
				decompressedArrayCopy(s3.value, 0, value, (s1len + s2len), s3len);
			}

			if (enableCompression) {
				initCompressionFlag();
			}
		}
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
		if (len < 0) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		if (enableCompression && (null == compressionFlag || s1.coder == LATIN1)) {
			value = new byte[len];
			coder = LATIN1;

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
			value = StringUTF16.newBytesFor(len);
			coder = UTF16;

			// Copy in v1
			int index = len - 1;

			do {
				int res = quot / 10;
				int rem = quot - (res * 10);

				quot = res;

				// Write the digit into the correct position
				helpers.putCharInArrayByIndex(value, index--, (char) ('0' - rem));
			} while (quot != 0);

			if (v1 < 0) {
				helpers.putCharInArrayByIndex(value, index, (char) '-');
			}

			// Copy in s1 contents
			decompressedArrayCopy(s1.value, 0, value, 0, s1len);

			if (enableCompression) {
				initCompressionFlag();
			}
		}
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
		long totalLen = (long) s1len + (long) v1len + (long) v2len + (long) s2len + (long) s3len;
		if (totalLen > Integer.MAX_VALUE) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}
		int len = (int) totalLen;

		if (enableCompression && (null == compressionFlag || (s1.coder | s2.coder | s3.coder) == LATIN1)) {
			value = new byte[len];
			coder = LATIN1;

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
			value = StringUTF16.newBytesFor(len);
			coder = UTF16;

			int start = len;

			// Copy in s3 contents
			start = start - s3len;

			// Check if the String is compressed
			if (enableCompression && s3.coder == LATIN1) {
				decompress(s3.value, 0, value, start, s3len);
			} else {
				decompressedArrayCopy(s3.value, 0, value, start, s3len);
			}

			// Copy in s2 contents
			start = start - s2len;

			// Check if the String is compressed
			if (enableCompression && s2.coder == LATIN1) {
				decompress(s2.value, 0, value, start, s2len);
			} else {
				decompressedArrayCopy(s2.value, 0, value, start, s2len);
			}

			// Copy in v2
			int index2 = start - 1;

			do {
				int res = quot2 / 10;
				int rem = quot2 - (res * 10);

				quot2 = res;

				// Write the digit into the correct position
				helpers.putCharInArrayByIndex(value, index2--, (char) ('0' - rem));
			} while (quot2 != 0);

			if (v2 < 0) {
				helpers.putCharInArrayByIndex(value, index2--, (char) '-');
			}

			// Copy in s1 contents
			start = index2 + 1 - s1len;

			// Check if the String is compressed
			if (enableCompression && s1.coder == LATIN1) {
				decompress(s1.value, 0, value, start, s1len);
			} else {
				decompressedArrayCopy(s1.value, 0, value, start, s1len);
			}

			// Copy in v1
			int index1 = start - 1;

			do {
				int res = quot1 / 10;
				int rem = quot1 - (res * 10);

				quot1 = res;

				// Write the digit into the correct position
				helpers.putCharInArrayByIndex(value, index1--, (char) ('0' - rem));
			} while (quot1 != 0);

			if (v1 < 0) {
				helpers.putCharInArrayByIndex(value, index1--, (char) '-');
			}

			if (enableCompression) {
				initCompressionFlag();
			}
		}
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
	 *          the zero-based index in this string
	 * @return the character at the index
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code index < 0} or {@code index >= length()}
	 */
	public char charAt(int index) {
		if (0 <= index && index < lengthInternal()) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
				return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
			} else {
				return helpers.getCharFromArrayByIndex(value, index);
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	// Internal version of charAt used for extracting a char from a String in compression related code.
	char charAtInternal(int index) {
		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		} else {
			return helpers.getCharFromArrayByIndex(value, index);
		}
	}

	// This method is needed so idiom recognition properly recognizes idiomatic loops where we are doing an operation on
	// the byte[] value of two Strings. In such cases we extract the String.value fields before entering the operation loop.
	// However if chatAt is used inside the loop then the JIT will anchor the load of the value byte[] inside of the loop thus
	// causing us to load the String.value on every iteration. This is very suboptimal and breaks some of the common idioms
	// that we recognize. The most prominent one is the regionMatches arraycmp idiom that is not recognized unless this method
	// is being used.
	char charAtInternal(int index, byte[] value) {
		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		} else {
			return helpers.getCharFromArrayByIndex(value, index);
		}
	}

	/**
	 * Compares the specified String to this String using the Unicode values of the characters. Answer 0 if the strings contain the same characters in
	 * the same order. Answer a negative integer if the first non-equal character in this String has a Unicode value which is less than the Unicode
	 * value of the character at the same position in the specified string, or if this String is a prefix of the specified string. Answer a positive
	 * integer if the first non-equal character in this String has a Unicode value which is greater than the Unicode value of the character at the same
	 * position in the specified string, or if the specified String is a prefix of the this String.
	 *
	 * @param string
	 *          the string to compare
	 * @return 0 if the strings are equal, a negative integer if this String is before the specified String, or a positive integer if this String is
	 *          after the specified String
	 *
	 * @throws NullPointerException
	 *          when string is null
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

		byte[] s1Value = s1.value;
		byte[] s2Value = s2.value;

		if (enableCompression && (null == compressionFlag || (s1.coder | s2.coder) == LATIN1)) {
			while (o1 < end) {
				int result =
					helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(s1Value, o1++)) -
					helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(s2Value, o2++));

				if (result != 0) {
					return result;
				}
			}
		} else {
			while (o1 < end) {
				int result =
					s1.charAtInternal(o1++, s1Value) -
					s2.charAtInternal(o2++, s2Value);

				if (result != 0) {
					return result;
				}
			}
		}

		return s1len - s2len;
	}

	private int compareValue(int codepoint) {
		if ('A' <= codepoint && codepoint <= 'Z') {
			return codepoint + ('a' - 'A');
		}

		return Character.toLowerCase(Character.toUpperCase(codepoint));
	}

	private char compareValue(char c) {
		if ('A' <= c && c <= 'Z') {
			return (char) (c + ('a' - 'A'));
		}

		return Character.toLowerCase(Character.toUpperCase(c));
	}

	private char compareValue(byte b) {
		if ('A' <= b && b <= 'Z') {
			return (char)(helpers.byteToCharUnsigned(b) + ('a' - 'A'));
		}
		return Character.toLowerCase(Character.toUpperCase(helpers.byteToCharUnsigned(b)));
	}

	/**
	 * Compare the receiver to the specified String to determine the relative ordering when the case of the characters is ignored.
	 *
	 * @param string
	 *          a String
	 * @return an {@code int < 0} if this String is less than the specified String, 0 if they are equal, and {@code > 0} if this String is greater
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

		byte[] s1Value = s1.value;
		byte[] s2Value = s2.value;

		if (enableCompression && (null == compressionFlag || (s1.coder | s2.coder) == LATIN1)) {
			while (o1 < end) {
				byte byteAtO1 = helpers.getByteFromArrayByIndex(s1Value, o1++);
				byte byteAtO2 = helpers.getByteFromArrayByIndex(s2Value, o2++);

				if (byteAtO1 == byteAtO2) {
					continue;
				}

				int result = compareValue(byteAtO1) - compareValue(byteAtO2);

				if (result != 0) {
					return result;
				}
			}
		} else {
			while (o1 < end) {
				char charAtO1 = s1.charAtInternal(o1++, s1Value);
				char charAtO2 = s2.charAtInternal(o2++, s2Value);
				int codepointAtO1 = charAtO1;
				int codepointAtO2 = charAtO2;

				if (charAtO1 == charAtO2) {
					/*[IF JAVA_SPEC_VERSION >= 16]*/
					if (Character.isHighSurrogate(charAtO1) && (o1 < end)) {
						codepointAtO1 = Character.toCodePoint(charAtO1, s1.charAtInternal(o1++, s1Value));
						codepointAtO2 = Character.toCodePoint(charAtO2, s2.charAtInternal(o2++, s2Value));
						if (codepointAtO1 == codepointAtO2) {
							continue;
						}
					} else {
						continue;
					}
					/*[ELSE]*/
					continue;
					/*[ENDIF] JAVA_SPEC_VERSION >= 16 */
				}

				int result = compareValue(codepointAtO1) - compareValue(codepointAtO2);

				if (result != 0) {
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
	 *          the string to concatenate
	 * @return a String which is the concatenation of this String and the specified String
	 *
	 * @throws NullPointerException
	 *          if string is null
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
		if (concatlen < 0) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		if (enableCompression && ((null == compressionFlag) || ((s1.coder | s2.coder) == LATIN1))) {
			byte[] buffer = new byte[concatlen];

			compressedArrayCopy(s1.value, 0, buffer, 0, s1len);
			compressedArrayCopy(s2.value, 0, buffer, s1len, s2len);

			return new String(buffer, LATIN1);
		} else {
			byte[] buffer = StringUTF16.newBytesFor(concatlen);

			// Check if the String is compressed
			if (enableCompression && s1.coder == LATIN1) {
				decompress(s1.value, 0, buffer, 0, s1len);
			} else {
				decompressedArrayCopy(s1.value, 0, buffer, 0, s1len);
			}

			// Check if the String is compressed
			if (enableCompression && s2.coder == LATIN1) {
				decompress(s2.value, 0, buffer, s1len, s2len);
			} else {
				decompressedArrayCopy(s2.value, 0, buffer, s1len, s2len);
			}

			return new String(buffer, UTF16);
		}
	}

	/**
	 * Creates a new String containing the characters in the specified character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 * @return the new String
	 *
	 * @throws NullPointerException
	 *          if data is null
	 */
	public static String copyValueOf(char[] data) {
		return new String(data, 0, data.length);
	}

	/**
	 * Creates a new String containing the specified characters in the character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 * @param start
	 *          the starting offset in the character array
	 * @param length
	 *          the number of characters to use
	 * @return the new String
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          if data is null
	 */
	public static String copyValueOf(char[] data, int start, int length) {
		return new String(data, start, length);
	}

	/**
	 * Compares the specified string to this String to determine if the specified string is a suffix.
	 *
	 * @param suffix
	 *          the string to look for
	 * @return true when the specified string is a suffix of this String, false otherwise
	 *
	 * @throws NullPointerException
	 *          if suffix is null
	 */
	public boolean endsWith(String suffix) {
		return regionMatches(lengthInternal() - suffix.lengthInternal(), suffix, 0, suffix.lengthInternal());
	}

	/**
	 * Compares the specified object to this String and answer if they are equal. The object must be an instance of String with the same characters in
	 * the same order.
	 *
	 * @param object
	 *          the object to compare
	 * @return true if the specified object is equal to this String, false otherwise
	 *
	 * @see #hashCode()
	 */
	public boolean equals(Object object) {
		if (object == this) {
			return true;
		} else {
			if (object instanceof String) {
				String s1 = this;
				String s2 = (String) object;

				int s1len = s1.lengthInternal();
				int s2len = s2.lengthInternal();

				if (s1len != s2len) {
					return false;
				}

				byte[] s1Value = s1.value;
				byte[] s2Value = s2.value;

				if (s1Value == s2Value) {
					return true;
				} else {
					// There was a time hole between first read of s.hashCode and second read if another thread does hashcode
					// computing for incoming string object
					int s1hash = s1.hashCode;
					int s2hash = s2.hashCode;

					if (s1hash != 0 && s2hash != 0 && s1hash != s2hash) {
						return false;
					}

					if (!regionMatchesInternal(s1, s2, s1Value, s2Value, 0, 0, s1len)) {
						return false;
					}

					if (com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY != com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY_DISABLED) {
						deduplicateStrings(s1, s1Value, s2, s2Value);
					}

					return true;
				}
			}

			return false;
		}
	}

	/**
	 * Deduplicate the backing buffers of the given strings.
	 *
	 * This updates the {@link #value} of one of the two given strings so that
	 * they both share a single backing buffer. The strings must have identical
	 * contents.
	 *
	 * Deduplication helps save space, and lets {@link #equals(Object)} exit
	 * early more often.
	 *
	 * The strings' corresponding backing buffers are accepted as parameters
	 * because the caller likely already has them.
	 *
	 * @param s1 The first string
	 * @param value1 {@code s1.value}
	 * @param s2 The second string
	 * @param value2 {@code s2.value}
	 */
	private static final void deduplicateStrings(String s1, Object value1, String s2, Object value2) {
		if (s1.coder == s2.coder) {
			long valueFieldOffset = UnsafeHelpers.valueFieldOffset;

			if (com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY == com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY_FAVOUR_LOWER) {
				if (helpers.acmplt(value1, value2)) {
					helpers.putObjectInObject(s2, valueFieldOffset, value1);
				} else {
					helpers.putObjectInObject(s1, valueFieldOffset, value2);
				}
			} else {
				if (helpers.acmplt(value2, value1)) {
					helpers.putObjectInObject(s2, valueFieldOffset, value1);
				} else {
					helpers.putObjectInObject(s1, valueFieldOffset, value2);
				}
			}
		}
	}

	/**
	 * Compares the specified String to this String ignoring the case of the characters and answer if they are equal.
	 *
	 * @param string
	 *          the string to compare
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

		// Zero length strings are equal
		if (s1len == 0) {
			return true;
		}

		int o1 = 0;
		int o2 = 0;

		// Upper bound index on the last char to compare
		int end = s1len;

		byte[] s1Value = s1.value;
		byte[] s2Value = s2.value;

		if (enableCompression && (null == compressionFlag || (s1.coder | s2.coder) == LATIN1)) {
			// Compare the last chars. Under string compression, the compressible char set obeys 1-1 mapping for upper/lower
			// case, converting to lower cases then compare should be sufficient.
			byte byteAtO1Last = helpers.getByteFromArrayByIndex(s1Value, s1len - 1);
			byte byteAtO2Last = helpers.getByteFromArrayByIndex(s2Value, s1len - 1);

			if (byteAtO1Last != byteAtO2Last &&
					toUpperCase(helpers.byteToCharUnsigned(byteAtO1Last)) != toUpperCase(helpers.byteToCharUnsigned(byteAtO2Last))) {
				return false;
			}

			while (o1 < end - 1) {
				byte byteAtO1 = helpers.getByteFromArrayByIndex(s1Value, o1++);
				byte byteAtO2 = helpers.getByteFromArrayByIndex(s2Value, o2++);

				if (byteAtO1 != byteAtO2 &&
						toUpperCase(helpers.byteToCharUnsigned(byteAtO1)) != toUpperCase(helpers.byteToCharUnsigned(byteAtO2))) {
					return false;
				}
			}
		} else {
			// Compare the last chars. Under string compression, the compressible char set obeys 1-1 mapping for upper/lower
			// case, converting to lower cases then compare should be sufficient.
			char charAtO1Last = s1.charAtInternal(s1len - 1, s1Value);
			char charAtO2Last = s2.charAtInternal(s1len - 1, s2Value);

			if (charAtO1Last != charAtO2Last
					&& toUpperCase(charAtO1Last) != toUpperCase(charAtO2Last)
					&& ((charAtO1Last <= 255 && charAtO2Last <= 255) || Character.toLowerCase(charAtO1Last) != Character.toLowerCase(charAtO2Last))
					/*[IF JAVA_SPEC_VERSION >= 16]*/
					&& (!Character.isLowSurrogate(charAtO1Last) || !Character.isLowSurrogate(charAtO2Last))
					/*[ENDIF] JAVA_SPEC_VERSION >= 16 */
			) {
				return false;
			}

			/*[IF JAVA_SPEC_VERSION >= 16]*/
			while (o1 < end) {
			/*[ELSE]*/
			while (o1 < end - 1) {
			/*[ENDIF] JAVA_SPEC_VERSION >= 16 */
				char charAtO1 = s1.charAtInternal(o1++, s1Value);
				char charAtO2 = s2.charAtInternal(o2++, s2Value);

				/*[IF JAVA_SPEC_VERSION >= 16]*/
				if (Character.isHighSurrogate(charAtO1) && Character.isHighSurrogate(charAtO2) && (o1 < end)) {
					int codepointAtO1 = Character.toCodePoint(charAtO1, s1.charAtInternal(o1++, s1Value));
					int codepointAtO2 = Character.toCodePoint(charAtO2, s2.charAtInternal(o2++, s2Value));
					if ((codepointAtO1 != codepointAtO2) && (compareValue(codepointAtO1) != compareValue(codepointAtO2))) {
						return false;
					} else {
						continue;
					}
				}
				/*[ENDIF] JAVA_SPEC_VERSION >= 16 */

				if (charAtO1 != charAtO2 &&
						toUpperCase(charAtO1) != toUpperCase(charAtO2) &&
						((charAtO1 <= 255 && charAtO2 <= 255) || Character.toLowerCase(charAtO1) != Character.toLowerCase(charAtO2))) {
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
		return StringCoding.encode(coder, value);
	}

	/**
	 * Converts this String to a byte array, ignoring the high order bits of each character.
	 *
	 * @param start
	 *          the starting offset of characters to copy
	 * @param end
	 *          the ending offset of characters to copy
	 * @param data
	 *          the destination byte array
	 * @param index
	 *          the starting offset in the byte array
	 *
	 * @throws NullPointerException
	 *          when data is null
	 * @throws IndexOutOfBoundsException
	 *          when {@code start < 0, end > length(), index < 0, end - start > data.length - index}
	 *
	 * @deprecated Use getBytes() or getBytes(String)
	 */
	@Deprecated(forRemoval=false, since="1.1")
	public void getBytes(int start, int end, byte[] data, int index) {
		if (0 <= start && start <= end && end <= lengthInternal() && 0 <= index && ((end - start) <= (data.length - index))) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
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
	 *          the encoding
	 * @return the byte array encoding of this String
	 *
	 * @throws UnsupportedEncodingException
	 *          when the encoding is not supported
	 *
	 * @see String
	 * @see UnsupportedEncodingException
	 */
	public byte[] getBytes(String encoding) throws UnsupportedEncodingException {
		encoding.getClass(); // Implicit null check
		return StringCoding.encode(encoding, coder, value);
	}

	/**
	 * Copies the specified characters in this String to the character array starting at the specified offset in the character array.
	 *
	 * @param start
	 *          the starting offset of characters to copy
	 * @param end
	 *          the ending offset of characters to copy
	 * @param data
	 *          the destination character array
	 * @param index
	 *          the starting offset in the character array
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code start < 0, end > length(), start > end, index < 0, end - start > buffer.length - index}
	 * @throws NullPointerException
	 *          when buffer is null
	 */
	public void getChars(int start, int end, char[] data, int index) {
		if (0 <= start && start <= end && end <= lengthInternal() && 0 <= index && ((end - start) <= (data.length - index))) {
			getCharsNoBoundChecks(start, end, data, index);
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	// This is a package protected method that performs the getChars operation without explicit bound checks.
	// Caller of this method must validate bound safety for String indexing and array copying.
	void getCharsNoBoundChecks(int start, int end, char[] data, int index) {
		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			decompress(value, start, data, index, end - start);
		} else {
			decompressedArrayCopy(value, start, data, index, end - start);
		}
	}

	// This is a package protected method that performs the getChars operation without explicit bound checks.
	// Caller of this method must validate bound safety for String indexing and array copying.
	void getCharsNoBoundChecks(int start, int end, byte[] data, int index) {
		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			decompress(value, start, data, index, end - start);
		} else {
			decompressedArrayCopy(value, start, data, index, end - start);
		}
	}

	/**
	 * Answers an integer hash code for the receiver. Objects which are equal answer the same value for this method.
	 *
	 * @return the receiver's hash
	 *
	 * @see #equals
	 */
	public int hashCode() {
		if (hashCode == 0 && value.length > 0) {
			// Check if the String is compressed
			if (enableCompression && (compressionFlag == null || coder == LATIN1)) {
				hashCode = hashCodeImplCompressed(value, 0, lengthInternal());
			} else {
				hashCode = hashCodeImplDecompressed(value, 0, lengthInternal());
			}
		}

		return hashCode;
	}

	private static int hashCodeImplCompressed(byte[] value, int offset, int count) {
		int hash = 0, end = offset + count;

		for (int i = offset; i < end; ++i) {
			hash = (hash << 5) - hash + helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, i));
		}

		return hash;
	}

	private static int hashCodeImplDecompressed(byte[] value, int offset, int count) {
		int hash = 0, end = offset + count;

		for (int i = offset; i < end; ++i) {
			hash = (hash << 5) - hash + helpers.getCharFromArrayByIndex(value, i);
		}

		return hash;
	}

	/**
	 * Searches in this String for the first index of the specified character. The search for the character starts at the beginning and moves towards
	 * the end of this String.
	 *
	 * @param c
	 *          the character to find
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
	 *          the character to find
	 * @param start
	 *          the starting offset
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
				byte[] array = value;

				// Check if the String is compressed
				if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
					if (c <= 255) {
						return helpers.intrinsicIndexOfLatin1(array, (byte)c, start, len);
					}
				} else {
					return helpers.intrinsicIndexOfUTF16(array, (char)c, start, len);
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
	 *          the string to find
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *          when string is null
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
	 *          the string to find
	 * @param start
	 *          the starting offset
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *          when string is null
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int indexOf(String subString, int start) {
		if (subString.length() == 1) {
			return indexOf(subString.charAtInternal(0), start);
		}

		return indexOf(value, coder, lengthInternal(), subString, start);
	}

	static int indexOf(byte[] value, byte coder, int count, String str, int fromIndex) {
		int s1Length = count;
		int s2Length = str.lengthInternal();

		if (fromIndex < 0) {
			fromIndex = 0;
		} else if (fromIndex >= s1Length) {
			// Handle the case where the substring is of zero length, in which case we have an indexOf hit at the end
			// of this string
			return s2Length == 0 ? s1Length : -1;
		}

		if (s2Length == 0) {
			// At this point we know fromIndex < s1Length so there is a hit at fromIndex
			return fromIndex;
		}

		byte[] s1Value = value;
		byte[] s2Value = str.value;

		if (coder == str.coder) {
			if (coder == LATIN1) {
				return StringLatin1.indexOf(s1Value, s1Length, s2Value, s2Length, fromIndex);
			} else {
				return StringUTF16.indexOf(s1Value, s1Length, s2Value, s2Length, fromIndex);
			}
		}

		if (coder == UTF16) {
			return StringUTF16.indexOfLatin1(s1Value, s1Length, s2Value, s2Length, fromIndex);
		}

		return -1;
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
	 *          the character to find
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
	 *          the character to find
	 * @param start
	 *          the starting offset
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
				byte[] array = value;

				// Check if the String is compressed
				if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
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
						if (helpers.getCharFromArrayByIndex(array, i) == c) {
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
	 *          the string to find
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *          when string is null
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int lastIndexOf(String string) {
		return lastIndexOf(string, lengthInternal());
	}

	/**
	 * Searches in this String for the index of the specified string. The search for the string starts at the specified offset and moves towards the
	 * beginning of this String.
	 *
	 * @param subString
	 *          the string to find
	 * @param start
	 *          the starting offset
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *          when string is null
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int lastIndexOf(String subString, int start) {
		return lastIndexOf(value, coder, lengthInternal(), subString, start);
	}

	static int lastIndexOf(byte[] value, byte coder, int count, String str, int fromIndex) {
		int s1Length = count;
		int s2Length = str.lengthInternal();

		if (fromIndex > s1Length - s2Length) {
			fromIndex = s1Length - s2Length;
		}

		if (fromIndex < 0) {
			return -1;
		}

		if (s2Length == 0) {
			return fromIndex;
		}

		byte[] s1Value = value;
		byte[] s2Value = str.value;

		if (coder == str.coder) {
			if (coder == LATIN1) {
				return StringLatin1.lastIndexOf(s1Value, s1Length, s2Value, s2Length, fromIndex);
			} else {
				return StringUTF16.lastIndexOf(s1Value, s1Length, s2Value, s2Length, fromIndex);
			}
		}

		if (coder == UTF16) {
			return StringUTF16.lastIndexOfLatin1(s1Value, s1Length, s2Value, s2Length, fromIndex);
		}

		return -1;
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
			return value.length >> coder;
		} else {
			return value.length >> 1;
		}
	}

	/**
	 * Compares the specified string to this String and compares the specified range of characters to determine if they are the same.
	 *
	 * @param thisStart
	 *          the starting offset in this String
	 * @param string
	 *          the string to compare
	 * @param start
	 *          the starting offset in string
	 * @param length
	 *          the number of characters to compare
	 * @return true if the ranges of characters is equal, false otherwise
	 *
	 * @throws NullPointerException
	 *          when string is null
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

		return regionMatchesInternal(s1, s2, s1.value, s2.value, thisStart, start, length);
	}

	private static boolean regionMatchesInternal(String s1, String s2, byte[] s1Value, byte[] s2Value, int s1Start, int s2Start, int length)
	{
		if (length <= 0) {
			return true;
		}

		// Index of the last char to compare
		int end = length - 1;

		if (enableCompression && ((compressionFlag == null) || ((s1.coder | s2.coder) == LATIN1))) {
			if (helpers.getByteFromArrayByIndex(s1Value, s1Start + end) != helpers.getByteFromArrayByIndex(s2Value, s2Start + end)) {
				return false;
			} else {
				for (int i = 0; i < end; ++i) {
					if (helpers.getByteFromArrayByIndex(s1Value, s1Start + i) != helpers.getByteFromArrayByIndex(s2Value, s2Start + i)) {
						return false;
					}
				}
			}
		} else {
			if (s1.charAtInternal(s1Start + end, s1Value) != s2.charAtInternal(s2Start + end, s2Value)) {
				return false;
			} else {
				for (int i = 0; i < end; ++i) {
					if (s1.charAtInternal(s1Start + i, s1Value) != s2.charAtInternal(s2Start + i, s2Value)) {
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
	 *          specifies if case should be ignored
	 * @param thisStart
	 *          the starting offset in this String
	 * @param string
	 *          the string to compare
	 * @param start
	 *          the starting offset in string
	 * @param length
	 *          the number of characters to compare
	 * @return true if the ranges of characters is equal, false otherwise
	 *
	 * @throws NullPointerException
	 *          when string is null
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

		byte[] s1Value = s1.value;
		byte[] s2Value = s2.value;

		if (enableCompression && (null == compressionFlag || (s1.coder | s2.coder) == LATIN1)) {
			while (o1 < end) {
				byte byteAtO1 = helpers.getByteFromArrayByIndex(s1Value, o1++);
				byte byteAtO2 = helpers.getByteFromArrayByIndex(s2Value, o2++);

				if (byteAtO1 != byteAtO2 &&
						toUpperCase(helpers.byteToCharUnsigned(byteAtO1)) != toUpperCase(helpers.byteToCharUnsigned(byteAtO2)) &&
						toLowerCase(helpers.byteToCharUnsigned(byteAtO1)) != toLowerCase(helpers.byteToCharUnsigned(byteAtO2))) {
					return false;
				}
			}
		} else {
			while (o1 < end) {
				char charAtO1 = s1.charAtInternal(o1++, s1Value);
				char charAtO2 = s2.charAtInternal(o2++, s2Value);

				/*[IF JAVA_SPEC_VERSION >= 16]*/
				if (Character.isHighSurrogate(charAtO1) && Character.isHighSurrogate(charAtO2) && (o1 < end)) {
					int codepointAtO1 = Character.toCodePoint(charAtO1, s1.charAtInternal(o1++, s1Value));
					int codepointAtO2 = Character.toCodePoint(charAtO2, s2.charAtInternal(o2++, s2Value));
					if ((codepointAtO1 != codepointAtO2) && (compareValue(codepointAtO1) != compareValue(codepointAtO2))) {
						return false;
					}
				}
				/*[ENDIF] JAVA_SPEC_VERSION >= 16 */

				if (charAtO1 != charAtO2 &&
						toUpperCase(charAtO1) != toUpperCase(charAtO2) &&
						toLowerCase(charAtO1) != toLowerCase(charAtO2)) {
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
	 *          the character to replace
	 * @param newChar
	 *          the replacement character
	 * @return a String with occurrences of oldChar replaced by newChar
	 */
	public String replace(char oldChar, char newChar) {
		int index = indexOf(oldChar, 0);

		if (index == -1) {
			return this;
		}

		int len = lengthInternal();

		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			if (newChar <= 255) {
				byte[] buffer = new byte[len];

				compressedArrayCopy(value, 0, buffer, 0, len);

				do {
					helpers.putByteInArrayByIndex(buffer, index++, (byte) newChar);
				} while ((index = indexOf(oldChar, index)) != -1);

				return new String(buffer, LATIN1);
			} else {
				byte[] buffer = StringUTF16.newBytesFor(len);

				decompress(value, 0, buffer, 0, len);

				do {
					helpers.putCharInArrayByIndex(buffer, index++, (char) newChar);
				} while ((index = indexOf(oldChar, index)) != -1);

				return new String(buffer, UTF16);
			}
		} else {
			byte[] buffer = StringUTF16.newBytesFor(len);

			decompressedArrayCopy(value, 0, buffer, 0, len);

			do {
				helpers.putCharInArrayByIndex(buffer, index++, (char) newChar);
			} while ((index = indexOf(oldChar, index)) != -1);

			return new String(buffer, UTF16);
		}
	}

	/**
	 * Compares the specified string to this String to determine if the specified string is a prefix.
	 *
	 * @param prefix
	 *          the string to look for
	 * @return true when the specified string is a prefix of this String, false otherwise
	 *
	 * @throws NullPointerException
	 *          when prefix is null
	 */
	public boolean startsWith(String prefix) {
		return startsWith(prefix, 0);
	}

	/**
	 * Compares the specified string to this String, starting at the specified offset, to determine if the specified string is a prefix.
	 *
	 * @param prefix
	 *          the string to look for
	 * @param start
	 *          the starting offset
	 * @return true when the specified string occurs in this String at the specified offset, false otherwise
	 *
	 * @throws NullPointerException
	 *          when prefix is null
	 */
	public boolean startsWith(String prefix, int start) {
		if (prefix.length() == 1) {
			if (start < 0 || start >= this.length()) {
				return false;
			}
			return charAtInternal(start) == prefix.charAtInternal(0);
		}
		return regionMatches(start, prefix, 0, prefix.lengthInternal());
	}

/*[IF JAVA_SPEC_VERSION >= 11]*/
	/**
	 * Strip leading and trailing white space from a string.
	 *
	 * @return a substring of the original containing no leading
	 * or trailing white space
	 *
	 * @since 11
	 */
	public String strip() {
		String result;

		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			result = StringLatin1.strip(value);
		} else {
			result = StringUTF16.strip(value);
		}

		return (result == null) ? this : result;
	}

	/**
	 * Strip leading white space from a string.
	 *
	 * @return a substring of the original containing no leading
	 * white space
	 *
	 * @since 11
	 */
	public String stripLeading() {
		String result;

		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			result = StringLatin1.stripLeading(value);
		} else {
			result = StringUTF16.stripLeading(value);
		}

		return (result == null) ? this : result;
	}

	/**
	 * Strip trailing white space from a string.
	 *
	 * @return a substring of the original containing no trailing
	 * white space
	 *
	 * @since 11
	 */
	public String stripTrailing() {
		String result;

		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			result = StringLatin1.stripTrailing(value);
		} else {
			result = StringUTF16.stripTrailing(value);
		}

		return (result == null) ? this : result;
	}

	/**
	 * Determine if the string contains only white space characters.
	 *
	 * @return true if the string is empty or contains only white space
	 * characters, otherwise false.
	 *
	 * @since 11
	 */
	public boolean isBlank() {
		int index;

		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			index = StringLatin1.indexOfNonWhitespace(value);
		} else {
			index = StringUTF16.indexOfNonWhitespace(value);
		}

		return index >= lengthInternal();
	}

	/**
	 * Returns a stream of substrings extracted from this string partitioned by line terminators.
	 *
	 * Line terminators recognized are line feed "\n", carriage return "\r", and carriage return
	 * followed by line feed "\r\n".
	 *
	 * @return the stream of this string's substrings partitioned by line terminators
	 *
	 * @since 11
	 */
	public Stream<String> lines() {
		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			return StringLatin1.lines(value);
		} else {
			return StringUTF16.lines(value);
		}
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 11 */

	/**
	 * Copies a range of characters into a new String.
	 *
	 * @param start
	 *          the offset of the first character
	 * @return a new String containing the characters from start to the end of the string
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code start < 0} or {@code start > length()}
	 */
	public String substring(int start) {
		if (start == 0) {
			return this;
		}

		int len = lengthInternal();

		if (0 <= start && start <= len) {
			boolean isCompressed = false;

			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
				isCompressed = true;
			}

			return new String(value, start, len - start, isCompressed, enableSharingInSubstringWhenOffsetIsZero);
		} else {
			throw new StringIndexOutOfBoundsException(start);
		}
	}

	/**
	 * Copies a range of characters.
	 *
	 * @param start
	 *          the offset of the first character
	 * @param end
	 *          the offset one past the last character
	 * @return a String containing the characters from start to end - 1
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code start < 0, start > end} or {@code end > length()}
	 */
	public String substring(int start, int end) {
		int len = lengthInternal();

		if (start == 0 && end == len) {
			return this;
		}

		if (0 <= start && start <= end && end <= len) {
			boolean isCompressed = false;
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
				isCompressed = true;
			}

			return new String(value, start, end - start, isCompressed, enableSharingInSubstringWhenOffsetIsZero);
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
		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			decompress(value, 0, buffer, 0, len);
		} else {
			decompressedArrayCopy(value, 0, buffer, 0, len);
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

	private static int toLowerCase(int codePoint) {
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

	private static int toUpperCase(int codePoint) {
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

	/**
	 * Converts the characters in this String to lowercase, using the specified Locale.
	 *
	 * @param locale
	 *          the Locale
	 * @return a String containing the lowercase characters equivalent to the characters in this String
	 */
	public String toLowerCase(Locale locale) {
		// check locale for null
		String language = locale.getLanguage();
		int sLength = lengthInternal();

		if (sLength == 0) {
			return this;
		}

		boolean useIntrinsic = helpers.supportsIntrinsicCaseConversion()
				&& (language == "en") //$NON-NLS-1$
				&& (sLength <= (Integer.MAX_VALUE / 2));

		if (enableCompression && ((null == compressionFlag) || (coder == LATIN1))) {
			if (useIntrinsic) {
				byte[] output = new byte[sLength << coder];

				if (helpers.toLowerIntrinsicLatin1(value, output, sLength)) {
					return new String(output, LATIN1);
				}
			}
			return StringLatin1.toLowerCase(this, value, locale);
		} else {
			if (useIntrinsic) {
				byte[] output = new byte[sLength << coder];

				if (helpers.toLowerIntrinsicUTF16(value, output, sLength * 2)) {
					return new String(output, UTF16);
				}
			}
			return StringUTF16.toLowerCase(this, value, locale);
		}
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
	 * Converts the characters in this String to uppercase, using the specified Locale.
	 *
	 * @param locale
	 *          the Locale
	 * @return a String containing the uppercase characters equivalent to the characters in this String
	 */
	public String toUpperCase(Locale locale) {
		String language = locale.getLanguage();
		int sLength = lengthInternal();

		if (sLength == 0) {
			return this;
		}

		boolean useIntrinsic = helpers.supportsIntrinsicCaseConversion()
				&& (language == "en") //$NON-NLS-1$
				&& (sLength <= (Integer.MAX_VALUE / 2));

		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			if (useIntrinsic) {
				byte[] output = new byte[sLength << coder];

				if (helpers.toUpperIntrinsicLatin1(value, output, sLength)) {
					return new String(output, LATIN1);
				}
			}
			return StringLatin1.toUpperCase(this, value, locale);
		} else {
			if (useIntrinsic) {
				byte[] output = new byte[sLength << coder];

				if (helpers.toUpperIntrinsicUTF16(value, output, sLength * 2)) {
					return new String(output, UTF16);
				}
			}
			return StringUTF16.toUpperCase(this, value, locale);
		}
	}

	/**
	 * Removes white space characters from the beginning and end of the string.
	 *
	 * @return a String with characters {@code <= \\u0020} removed from the beginning and the end
	 */
	public String trim() {
		int start = 0;
		int last = lengthInternal() - 1;
		int end = last;

		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
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
	 *          the array of characters
	 * @return the String
	 *
	 * @throws NullPointerException
	 *          when data is null
	 */
	public static String valueOf(char[] data) {
		return new String(data, 0, data.length);
	}

	/**
	 * Returns a String containing the specified characters in the character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 * @param start
	 *          the starting offset in the character array
	 * @param length
	 *          the number of characters to use
	 * @return the String
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
	 */
	public static String valueOf(char[] data, int start, int length) {
		return new String(data, start, length);
	}

	/**
	 * Converts the specified character to its string representation.
	 *
	 * @param value
	 *          the character
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
			byte[] buffer = new byte[2];

			helpers.putCharInArrayByIndex(buffer, 0, value);

			string = new String(buffer, 0, 1, false);
		}

		return string;
	}

	/**
	 * Converts the specified double to its string representation.
	 *
	 * @param value
	 *          the double
	 * @return the double converted to a string
	 */
	public static String valueOf(double value) {
		return Double.toString(value);
	}

	/**
	 * Converts the specified float to its string representation.
	 *
	 * @param value
	 *          the float
	 * @return the float converted to a string
	 */
	public static String valueOf(float value) {
		return Float.toString(value);
	}

	/**
	 * Converts the specified integer to its string representation.
	 *
	 * @param value
	 *          the integer
	 * @return the integer converted to a string
	 */
	public static String valueOf(int value) {
		return Integer.toString(value);
	}

	/**
	 * Converts the specified long to its string representation.
	 *
	 * @param value
	 *          the long
	 * @return the long converted to a string
	 */
	public static String valueOf(long value) {
		return Long.toString(value);
	}

	/**
	 * Converts the specified object to its string representation. If the object is null answer the string {@code "null"}, otherwise use
	 * {@code toString()} to get the string representation.
	 *
	 * @param value
	 *          the object
	 * @return the object converted to a string
	 */
	public static String valueOf(Object value) {
		return value != null ? value.toString() : "null"; //$NON-NLS-1$
	}

	/**
	 * Converts the specified boolean to its string representation. When the boolean is true answer {@code "true"}, otherwise answer
	 * {@code "false"}.
	 *
	 * @param value
	 *          the boolean
	 * @return the boolean converted to a string
	 */
	public static String valueOf(boolean value) {
		return value ? "true" : "false"; //$NON-NLS-1$ //$NON-NLS-2$
	}

	/**
	 * Answers whether the characters in the StringBuffer buffer are the same as those in this String.
	 *
	 * @param buffer
	 *          the StringBuffer to compare this String to
	 * @return true when the characters in buffer are identical to those in this String. If they are not, false will be returned.
	 *
	 * @throws NullPointerException
	 *          when buffer is null
	 *
	 * @since 1.4
	 */
	public boolean contentEquals(StringBuffer buffer) {
		synchronized (buffer) {
			int s1Length = lengthInternal();
			int sbLength = buffer.length();

			if (s1Length != sbLength) {
				return false;
			}

			byte[] s1Value = value;
			byte[] sbValue = buffer.getValue();

			if (coder == buffer.getCoder()) {
				for (int i = 0; i < s1Value.length; ++i) {
					if (s1Value[i] != sbValue[i]) {
						return false;
					}
				}

				return true;
			}

			// String objects are always compressed if compression is possible, thus if the buffer is compressed and the
			// String object is not it is impossible for their contents to equal
			if (coder == UTF16) {
				return false;
			}

			// Otherwise we have a LATIN1 String and a UTF16 StringBuffer
			return StringUTF16.contentEquals(s1Value, sbValue, s1Length);
		}
	}

	/**
	 * Determines whether a this String matches a given regular expression.
	 *
	 * @param expr
	 *          the regular expression to be matched
	 * @return true if the expression matches, otherwise false
	 *
	 * @throws PatternSyntaxException
	 *          if the syntax of the supplied regular expression is not valid
	 * @throws NullPointerException
	 *          if expr is null
	 *
	 * @since 1.4
	 */
	public boolean matches(String expr) {
		return Pattern.matches(expr, this);
	}

	/**
	 * Replace any substrings within this String that match the supplied regular expression expr, with the String substitute.
	 *
	 * @param regex
	 *          the regular expression to match
	 * @param substitute
	 *          the string to replace the matching substring with
	 * @return the new string
	 *
	 * @throws NullPointerException
	 *          if expr is null
	 *
	 * @since 1.4
	 */
	public String replaceAll(String regex, String substitute) {
		// this is a fast path to handle replacements of 1 character with another or the deletion of
		// a single character (common operations when dealing with things like package names, file
		// system paths etc). In these simple cases a linear scan of the string is all that is necessary
		// and we can avoid the cost of building a full regex pattern matcher
		if (regex != null && substitute != null && regex.lengthInternal() == 1 && !hasMetaChars(regex)) {
			int substituteLength = substitute.lengthInternal();
			int length = lengthInternal();
			if (substituteLength < 2) {
				if (enableCompression && isCompressed() && (substituteLength == 0 || substitute.isCompressed())) {
					byte[] newChars = new byte[length];
					byte toReplace = helpers.getByteFromArrayByIndex(regex.value, 0);
					byte replacement = (byte)-1;  // assign dummy value that will never be used
					if (substituteLength == 1) {
						replacement = helpers.getByteFromArrayByIndex(substitute.value, 0);
						checkLastChar((char)replacement);
					}
					int newCharIndex = 0;
					for (int i = 0; i < length; ++i) {
						byte current = helpers.getByteFromArrayByIndex(value, i);
						if (current != toReplace) {
							helpers.putByteInArrayByIndex(newChars, newCharIndex++, current);
						} else if (substituteLength == 1) {
							helpers.putByteInArrayByIndex(newChars, newCharIndex++, replacement);
						}
					}
					return new String(newChars, 0, newCharIndex, true);
				} else if (!enableCompression || !isCompressed()) {
					byte[] newChars = StringUTF16.newBytesFor(length);
					char toReplace = regex.charAtInternal(0);
					char replacement = (char)-1; // assign dummy value that will never be used
					if (substituteLength == 1) {
						replacement = substitute.charAtInternal(0);
						checkLastChar(replacement);
					}
					int newCharIndex = 0;
					for (int i = 0; i < length; ++i) {
						char current = helpers.getCharFromArrayByIndex(value, i);
						if (current != toReplace) {
							helpers.putCharInArrayByIndex(newChars, newCharIndex++, current);
						} else if (substituteLength == 1) {
							helpers.putCharInArrayByIndex(newChars, newCharIndex++, replacement);
						}
					}
					return new String(newChars, 0, newCharIndex, false);
				}
			}
		}
		return Pattern.compile(regex).matcher(this).replaceAll(substitute);
	}

	/**
	 * Replace any substrings within this String that match the supplied regular expression expr, with the String substitute.
	 *
	 * @param expr
	 *          the regular expression to match
	 * @param substitute
	 *          the string to replace the matching substring with
	 * @return the new string
	 *
	 * @throws NullPointerException
	 *          if expr is null
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
	 *          Regular expression that is used as a delimiter
	 * @return The array of strings which are split around the regex
	 *
	 * @throws PatternSyntaxException
	 *          if the syntax of regex is invalid
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

	private static final boolean isSingleEscapeLiteral(String s) {
		if ((s != null) && (s.lengthInternal() == 2) && (s.charAtInternal(0) == '\\')) {
			char literal = s.charAtInternal(1);
			for (int j = 0; j < regexMetaChars.length; ++j) {
				if (literal == regexMetaChars[j]) return true;
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
	 * @param regex Regular expression that is used as a delimiter
	 * @param max The threshold of the returned array
	 * @return The array of strings which are split around the regex
	 *
	 * @throws PatternSyntaxException if the syntax of regex is invalid
	 *
	 * @since 1.4
	 */
	public String[] split(String regex, int max) {
		// it is faster to handle simple splits inline (i.e. no fancy regex matching),
		// including single escaped literal character (e.g. \. \{),
		// so we test for a suitable string and handle this here if we can
		boolean singleEscapeLiteral = isSingleEscapeLiteral(regex);
		if ((regex != null) && (regex.lengthInternal() > 0) && (!hasMetaChars(regex) || singleEscapeLiteral)) {
			if (max == 1) {
				return new String[] { this };
			}
			java.util.ArrayList<String> parts = new java.util.ArrayList<String>((max > 0 && max < 100) ? max : 10);

			byte[] chars = this.value;

			final boolean compressed = enableCompression && (null == compressionFlag || coder == LATIN1);

			int start = 0, current = 0, end = lengthInternal();
			if (regex.lengthInternal() == 1 || singleEscapeLiteral) {
				// if matching single escaped character, use the second char.
				char splitChar = regex.charAtInternal(singleEscapeLiteral ? 1 : 0);
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

				byte[] splitChars = regex.value;

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
	 *          the offset the first character
	 * @param end
	 *          the offset of one past the last character to include
	 *
	 * @return the subsequence requested
	 *
	 * @throws IndexOutOfBoundsException
	 *          when start or end is less than zero, start is greater than end, or end is greater than the length of the String.
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
	 *          the byte array to convert to a String
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 *
	 * @since 1.5
	 */
	public String(int[] data, int start, int length) {
		if (start >= 0 && 0 <= length && length <= data.length - start) {
			if (enableCompression) {
				byte[] bytes = StringLatin1.toBytes(data, start, length);

				if (bytes != null) {
					value = bytes;
					coder = LATIN1;
				} else {
					bytes = StringUTF16.toBytes(data, start, length);

					value = bytes;
					coder = UTF16;

					initCompressionFlag();
				}
			} else {
				byte[] bytes = StringUTF16.toBytes(data, start, length);

				value = bytes;
				coder = UTF16;
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Creates a string from the contents of a StringBuilder.
	 *
	 * @param builder
	 *          the StringBuilder
	 *
	 * @since 1.5
	 */
	public String(StringBuilder builder) {
		this(builder.toString());
	}

	/**
	 * Returns the Unicode character at the given point.
	 *
	 * @param index
	 *          the character index
	 * @return the Unicode character value at the index
	 *
	 * @since 1.5
	 */
	public int codePointAt(int index) {
		int len = lengthInternal();

		if (index >= 0 && index < len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
				return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
			} else {
				char high = charAtInternal(index);

				if ((index < (len - 1)) && Character.isHighSurrogate(high)) {
					char low = charAtInternal(index + 1);

					if (Character.isLowSurrogate(low)) {
						return Character.toCodePoint(high, low);
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
	 *          the character index
	 * @return the Unicode character value before the index
	 *
	 * @since 1.5
	 */
	public int codePointBefore(int index) {
		int len = lengthInternal();

		if (index > 0 && index <= len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
				return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index - 1));
			} else {
				char low = charAtInternal(index - 1);

				if ((index > 1) && Character.isLowSurrogate(low)) {
					char high = charAtInternal(index - 2);

					if (Character.isHighSurrogate(high)) {
						return Character.toCodePoint(high, low);
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
	 *          first index
	 * @param end
	 *          last index
	 * @return the total Unicode values
	 *
	 * @since 1.5
	 */
	public int codePointCount(int start, int end) {
		int len = lengthInternal();

		if (start >= 0 && start <= end && end <= len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
				return end - start;
			} else {
				int count = 0;

				for (int i = start; i < end; ++i) {
					if ((i < (end - 1))
							&& Character.isHighSurrogate(charAtInternal(i))
							&& Character.isLowSurrogate(charAtInternal(i + 1))) {
						++i;
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
	 * Returns the index of the code point that was offset by codePointCount.
	 *
	 * @param start
	 *          the position to offset
	 * @param codePointCount
	 *          the code point count
	 * @return the offset index
	 *
	 * @since 1.5
	 */
	public int offsetByCodePoints(int start, int codePointCount) {
		int len = lengthInternal();

		if (start >= 0 && start <= len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
				int index = start + codePointCount;

				if (index > len) {
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

						if ((index < (len - 1))
								&& Character.isHighSurrogate(charAtInternal(index))
								&& Character.isLowSurrogate(charAtInternal(index + 1))) {
							index++;
						}

						index++;
					}
				} else {
					for (int i = codePointCount; i < 0; ++i) {
						if (index < 1) {
							throw new IndexOutOfBoundsException();
						}

						if ((index > 1)
								&& Character.isLowSurrogate(charAtInternal(index - 1))
								&& Character.isHighSurrogate(charAtInternal(index - 2))) {
							index--;
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
	 *          the character sequence
	 * @return {@code true} if the content of this String is equal to the character sequence, {@code false} otherwise.
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
	 *          the sequence to compare to
	 * @return {@code true} if this String contains the sequence, {@code false} otherwise.
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
	 *          the old character sequence
	 * @param sequence2
	 *          the new character sequence
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
			int sequence2len = sequence2.length();

			if ((sequence2len != 0) && (len >= ((Integer.MAX_VALUE - len) / sequence2len))) {
				/*[MSG "K0D01", "Array capacity exceeded"]*/
				throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
			}

			StringBuilder builder = new StringBuilder(len + ((len + 1) * sequence2len));

			builder.append(sequence2);

			for (int i = 0; i < len; ++i) {
				builder.append(charAt(i)).append(sequence2);
			}

			return builder.toString();
		} else {
			StringBuilder builder = new StringBuilder();

			int start = 0;
			int copyStart = 0;

			char charAt0 = sequence1.charAt(0);

			while (start < len) {
				int firstIndex = indexOf(charAt0, start);

				if (firstIndex == -1) {
					break;
				}

				boolean found = true;

				if (sequence1len > 1) {
					if (sequence1len > len - firstIndex) {
						/* the tail of this string is too short to find sequence1 */
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
	 *          the format to use
	 * @param args
	 *          the format arguments to use
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
	 *          the locale used to create the Formatter, may be null
	 * @param format
	 *          the format to use
	 * @param args
	 *          the format arguments to use
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
	 *          the byte array to convert to a String
	 * @param charset
	 *          the Charset to use
	 *
	 * @throws NullPointerException
	 *          when data is null
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
	 *          the byte array to convert to a String
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 * @param charset
	 *          the Charset to use
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
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
			StringCoding.Result scResult = StringCoding.decode(charset, data, start, length);

			value = scResult.value;
			coder = scResult.coder;

			if (enableCompression) {
				initCompressionFlag();
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Converts this String to a byte encoding using the specified Charset.
	 *
	 * @param charset
	 *          the Charset to use
	 * @return the byte array encoding of this String
	 *
	 * @since 1.6
	 */
	public byte[] getBytes(Charset charset) {
		return StringCoding.encode(charset, coder, value);
	}

	/**
	 * Creates a new String by putting each element together joined by the delimiter. If an element is null, then "null" is used as string to join.
	 *
	 * @param delimiter
	 *          Used as joiner to put elements together
	 * @param elements
	 *          Elements to be joined
	 * @return string of joined elements by delimiter
	 * @throws NullPointerException
	 *          if one of the arguments is null
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
	 *          Used as joiner to put elements together
	 * @param elements
	 *          Elements to be joined
	 * @return string of joined elements by delimiter
	 * @throws NullPointerException
	 *          if one of the arguments is null
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

	static void checkBoundsBeginEnd(int begin, int end, int length) {
		if ((begin >= 0) && (begin <= end) && (end <= length)) {
			return;
		}
		throw newStringIndexOutOfBoundsException(begin, end, length);
	}

	@Override
	public IntStream chars() {
		Spliterator.OfInt spliterator;

		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			spliterator = new StringLatin1.CharsSpliterator(value, Spliterator.IMMUTABLE);
		} else {
			spliterator = new StringUTF16.CharsSpliterator(value, Spliterator.IMMUTABLE);
		}

		return StreamSupport.intStream(spliterator, false);
	}

	@Override
	public IntStream codePoints() {
		Spliterator.OfInt spliterator;

		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			spliterator = new StringLatin1.CharsSpliterator(value, Spliterator.IMMUTABLE);
		} else {
			spliterator = new StringUTF16.CodePointsSpliterator(value, Spliterator.IMMUTABLE);
		}

		return StreamSupport.intStream(spliterator, false);
	}

	/*
	 * Internal API, assuming no modification to the byte array returned.
	 */
	byte[] value() {
		return value;
	}

	/*[IF JAVA_SPEC_VERSION >= 11]*/
	/**
	 * Returns a string object containing the character (Unicode code point)
	 * specified.
	 *
	 * @param codePoint
	 *          a Unicode code point.
	 * @return a string containing the character (Unicode code point) supplied.
	 * @throws IllegalArgumentException
	 *          if the codePoint is not a valid Unicode code point.
	 * @since 11
	 */
	static String valueOfCodePoint(int codePoint) {
		String string;
		if ((codePoint < Character.MIN_CODE_POINT) || (codePoint > Character.MAX_CODE_POINT)) {
			/*[MSG "K0800", "Invalid Unicode code point - {0}"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0800", Integer.toString(codePoint)));   //$NON-NLS-1$
		} else if (codePoint <= 255) {
			if (enableCompression) {
				string = new String(compressedAsciiTable[codePoint], LATIN1);
			} else {
				string = new String(decompressedAsciiTable[codePoint], UTF16);
			}
		} else if (codePoint < Character.MIN_SUPPLEMENTARY_CODE_POINT) {
			byte[] buffer = new byte[2];
			helpers.putCharInArrayByIndex(buffer, 0, (char) codePoint);
			string = new String(buffer, UTF16);
		} else {
			byte[] buffer = new byte[4];
			helpers.putCharInArrayByIndex(buffer, 0, Character.highSurrogate(codePoint));
			helpers.putCharInArrayByIndex(buffer, 1, Character.lowSurrogate(codePoint));
			string = new String(buffer, UTF16);
		}
		return string;
	}

	/**
	 * Returns a string whose value is the concatenation of this string repeated
	 * count times.
	 *
	 * @param count
	 *          a positive integer indicating the number of times to be repeated
	 * @return a string whose value is the concatenation of this string repeated count times
	 * @throws IllegalArgumentException
	 *          if the count is negative
	 * @since 11
	 */
	public String repeat(int count) {
		if (count < 0) {
			throw new IllegalArgumentException();
		} else if (count == 0 || isEmpty()) {
			return ""; //$NON-NLS-1$
		} else if (count == 1) {
			return this;
		}

		int length = lengthInternal();
		if (length > Integer.MAX_VALUE / count) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}
		int repeatlen = length * count;

		if (enableCompression && (null == compressionFlag || coder == LATIN1)) {
			byte[] buffer = new byte[repeatlen];

			for (int i = 0; i < count; i++) {
				compressedArrayCopy(value, 0, buffer, i * length, length);
			}

			return new String(buffer, LATIN1);
		} else {
			byte[] buffer = StringUTF16.newBytesFor(repeatlen);

			for (int i = 0; i < count; i++) {
				decompressedArrayCopy(value, 0, buffer, i * length, length);
			}

			return new String(buffer, UTF16);
		}
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 11 */

/*[ELSE] Sidecar19-SE*/
	// DO NOT CHANGE OR MOVE THIS LINE
	// IT MUST BE THE FIRST THING IN THE INITIALIZATION
	private static final long serialVersionUID = -6849794470754667710L;

	/**
	 * Determines whether String compression is enabled.
	 */
	static final boolean enableCompression = com.ibm.oti.vm.VM.J9_STRING_COMPRESSION_ENABLED;

	/**
	 * CaseInsensitiveComparator compares Strings ignoring the case of the characters.
	 */
	private static final class CaseInsensitiveComparator implements Comparator<String>, Serializable {
		static final long serialVersionUID = 8575799808933029326L;

		/**
		 * Compare the two objects to determine the relative ordering.
		 *
		 * @param o1
		 *            an Object to compare
		 * @param o2
		 *            an Object to compare
		 * @return an int < 0 if object1 is less than object2, 0 if they are equal, and > 0 if object1 is greater
		 *
		 * @exception ClassCastException
		 *                  when objects are not the correct type
		 */
		public int compare(String o1, String o2) {
			return o1.compareToIgnoreCase(o2);
		}
	};

	/**
	 * A Comparator which compares Strings ignoring the case of the characters.
	 */
	public static final Comparator<String> CASE_INSENSITIVE_ORDER = new CaseInsensitiveComparator();

	// Used to represent the value of an empty String
	private static final char[] emptyValue = new char[0];

	// Used to extract the value of a single ASCII character String by the integral value of the respective character as
	// an index into this table
	private static final char[][] compressedAsciiTable;

	private static final char[][] decompressedAsciiTable;

	// Used to access compression related helper methods
	private static final com.ibm.jit.JITHelpers helpers = com.ibm.jit.JITHelpers.getHelpers();

	static class StringCompressionFlag implements Serializable {
		private static final long serialVersionUID = 1346155847239551492L;
	}

	// Singleton used by all String instances to indicate a non-compressed string has been
	// allocated. JIT attempts to fold away the null check involving this static if the
	// StringCompressionFlag class has not been initialized and patches the code to bring back
	// the null check if a non-compressed String is constructed.
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

	private final char[] value;
	private final int count;
	private int hashCode;

	static {
		stringArray = new String[stringArraySize];

		compressedAsciiTable = new char[256][];

		for (int i = 0; i < compressedAsciiTable.length; ++i) {
			char[] asciiValue = new char[1];

			helpers.putByteInArrayByIndex(asciiValue, 0, (byte) i);

			compressedAsciiTable[i] = asciiValue;
		}

		decompressedAsciiTable = new char[256][];

		for (int i = 0; i < decompressedAsciiTable.length; ++i) {
			char[] asciiValue = new char[1];

			helpers.putCharInArrayByIndex(asciiValue, 0, (char) i);

			decompressedAsciiTable[i] = asciiValue;
		}
	}

	static void initCompressionFlag() {
		if (compressionFlag == null) {
			compressionFlag = new StringCompressionFlag();
		}
	}

	/**
	 * Determines whether the input character array can be encoded as a compact
	 * Latin1 string.
	 *
	 * <p>This API implicitly assumes the following:
	 * <blockquote><pre>
	 *     - {@code length >= 0}
	 *     - {@code start >= 0}
	 *     - {@code start + length <= data.length}
	 * <blockquote><pre>
	 *
	 * @param c      the array of characters to check
	 * @param start  the starting offset in the character array
	 * @param length the number of characters to check starting at {@code start}
	 * @return       {@code true} if the input character array can be encoded
	 *               using the Latin1 encoding; {@code false} otherwise
	 */
	static boolean canEncodeAsLatin1(char[] c, int start, int length) {
		for (int i = start; i < start + length; ++i) {
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

	static void decompressedArrayCopy(char[] array1, int start1, char[] array2, int start2, int length) {
		System.arraycopy(array1, start1, array2, start2, length);
	}

	boolean isCompressed() {
		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || count >= 0)) {
			return true;
		} else {
			return false;
		}
	}

	/**
	 * Answers an empty string.
	 */
	public String() {
		value = emptyValue;
		count = 0;
	}

	/**
	 * Converts the byte array to a String using the default encoding as specified by the file.encoding system property. If the system property is not
	 * defined, the default encoding is ISO8859_1 (ISO-Latin-1). If 8859-1 is not available, an ASCII encoding is used.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 *
	 * @throws NullPointerException
	 *          when data is null
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
	 */
	public String(byte[] data) {
		this(data, 0, data.length);
	}

	/**
	 * Converts the byte array to a String, setting the high byte of every character to the specified value.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 * @param high
	 *          the high byte to use
	 *
	 * @throws NullPointerException
	 *          when data is null
	 *
	 * @deprecated Use String(byte[]) or String(byte[], String) instead
	 */
	@Deprecated
	public String(byte[] data, int high) {
		this(data, high, 0, data.length);
	}

	/**
	 * Converts the byte array to a String using the default encoding as specified by the file.encoding system property. If the system property is not
	 * defined, the default encoding is ISO8859_1 (ISO-Latin-1). If 8859-1 is not available, an ASCII encoding is used.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
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
	 */
	public String(byte[] data, int start, int length) {
		data.getClass(); // Implicit null check

		if (start >= 0 && 0 <= length && length <= data.length - start) {
			char[] buffer = StringCoding.decode(data, start, length);

			if (enableCompression) {
				if (canEncodeAsLatin1(buffer, 0, buffer.length)) {
					value = new char[(buffer.length + 1) >>> 1];
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
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Converts the byte array to a String, setting the high byte of every character to the specified value.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 * @param high
	 *          the high byte to use
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
	 *
	 * @deprecated Use String(byte[], int, int) instead
	 */
	@Deprecated
	public String(byte[] data, int high, int start, int length) {
		data.getClass(); // Implicit null check

		if (start >= 0 && 0 <= length && length <= data.length - start) {
			if (enableCompression) {
				if (high == 0) {
					value = new char[(length + 1) >>> 1];
					count = length;

					compressedArrayCopy(data, start, value, 0, length);
				} else {
					value = new char[length];
					count = length | uncompressedBit;

					high <<= 8;

					for (int i = 0; i < length; ++i) {
						value[i] = (char) (high + (data[start++] & 0xff));
					}

					initCompressionFlag();
				}

			} else {
				value = new char[length];
				count = length;

				high <<= 8;

				for (int i = 0; i < length; ++i) {
					value[i] = (char) (high + (data[start++] & 0xff));
				}
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Converts the byte array to a String using the specified encoding.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 * @param encoding
	 *          the encoding
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws UnsupportedEncodingException
	 *          when encoding is not supported
	 * @throws NullPointerException
	 *          when data is null
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
			char[] buffer = StringCoding.decode(encoding, data, start, length);

			if (enableCompression) {
				if (canEncodeAsLatin1(buffer, 0, buffer.length)) {
					value = new char[(buffer.length + 1) >>> 1];
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
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Converts the byte array to a String using the specified encoding.
	 *
	 * @param data
	 *          the byte array to convert to a String
	 * @param encoding
	 *          the encoding
	 *
	 * @throws UnsupportedEncodingException
	 *          when encoding is not supported
	 * @throws NullPointerException
	 *          when data is null
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

	private String(String s, char c) {
		if (s == null) {
			s = "null"; //$NON-NLS-1$
		}

		int slen = s.lengthInternal();

		int concatlen = slen + 1;
		if (concatlen < 0) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		if (enableCompression) {
			// Check if the String is compressed
			if ((null == compressionFlag || s.count >= 0) && c <= 255) {
				value = new char[(concatlen + 1) >>> 1];
				count = concatlen;

				compressedArrayCopy(s.value, 0, value, 0, slen);

				helpers.putByteInArrayByIndex(value, slen, (byte) c);
			} else {
				value = new char[concatlen];
				count = (concatlen) | uncompressedBit;

				System.arraycopy(s.value, 0, value, 0, slen);

				value[slen] = c;

				initCompressionFlag();
			}
		} else {
			value = new char[concatlen];
			count = concatlen;

			System.arraycopy(s.value, 0, value, 0, slen);

			value[slen] = c;
		}
	}

	/**
	 * Initializes this String to contain the characters in the specified character array. Modifying the character array after creating the String has
	 * no effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 *
	 * @throws NullPointerException
	 *          when data is null
	 */
	public String(char[] data) {
		this(data, 0, data.length);
	}

	/**
	 * Initializes this String to use the specified character array. The character array should not be modified after the String is created.
	 *
	 * @param data
	 *          a non-null array of characters
	 */
	String(char[] data, boolean ignore) {
		if (enableCompression) {
			if (canEncodeAsLatin1(data, 0, data.length)) {
				value = new char[(data.length + 1) >>> 1];
				count = data.length;

				compress(data, 0, value, 0, data.length);
			} else {
				value = new char[data.length];
				count = data.length | uncompressedBit;

				System.arraycopy(data, 0, value, 0, data.length);

				initCompressionFlag();
			}
		} else {
			value = new char[data.length];
			count = data.length;

			System.arraycopy(data, 0, value, 0, data.length);
		}
	}

	/**
	 * Initializes this String to contain the specified characters in the character array. Modifying the character array after creating the String has
	 * no effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 * @param start
	 *          the starting offset in the character array
	 * @param length
	 *          the number of characters to use
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
	 */
	public String(char[] data, int start, int length) {
		if (start >= 0 && 0 <= length && length <= data.length - start) {
			if (enableCompression) {
				if (canEncodeAsLatin1(data, start, length)) {
					value = new char[(length + 1) >>> 1];
					count = length;

					compress(data, start, value, 0, length);
				} else {
					value = new char[length];
					count = length | uncompressedBit;

					System.arraycopy(data, start, value, 0, length);

					initCompressionFlag();
				}
			} else {
				value = new char[length];
				count = length;

				System.arraycopy(data, start, value, 0, length);
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	String(char[] data, int start, int length, boolean compressed) {
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

					char theChar = data[start];

					if (theChar <= 255) {
						value = decompressedAsciiTable[theChar];
					} else {
						value = new char[] { theChar };
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
						value = new char[(length + 1) >>> 1];
						count = length;

						compressedArrayCopy(data, start, value, 0, length);
					}
				} else {
					if (start == 0) {
						value = data;
						count = length | uncompressedBit;
					} else {
						value = new char[length];
						count = length | uncompressedBit;

						System.arraycopy(data, start, value, 0, length);
					}

					initCompressionFlag();
				}
			}
		} else {
			if (length == 0) {
				value = emptyValue;
				count = 0;
			} else if (length == 1 && data[start] <= 255) {
				char theChar = data[start];

				value = decompressedAsciiTable[theChar];
				count = 1;

				hashCode = theChar;
			} else {
				if (start == 0) {
					value = data;
					count = length;
				} else {
					value = new char[length];
					count = length;

					System.arraycopy(data, start, value, 0, length);
				}
			}
		}
	}

	String(char[] data, int start, int length, boolean compressed, boolean sharingIsAllowed) {
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
					char theChar = data[start];

					if (theChar <= 255) {
						value = decompressedAsciiTable[theChar];
					} else {
						value = new char[] { theChar };
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
						value = new char[(length + 1) >>> 1];
						count = length;

						compressedArrayCopy(data, start, value, 0, length);
					}
				} else {
					if (start == 0 && sharingIsAllowed) {
						value = data;
						count = length | uncompressedBit;
					} else {
						value = new char[length];
						count = length | uncompressedBit;

						System.arraycopy(data, start, value, 0, length);
					}

					initCompressionFlag();
				}
			}
		} else {
			if (length == 0) {
				value = emptyValue;
				count = 0;
			} else if (length == 1 && data[start] <= 255) {
				char theChar = data[start];

				value = decompressedAsciiTable[theChar];
				count = 1;

				hashCode = theChar;
			} else {
				if (start == 0 && sharingIsAllowed) {
					value = data;
					count = length;
				} else {
					value = new char[length];
					count = length;

					System.arraycopy(data, start, value, 0, length);
				}
			}
		}
	}

	/**
	 * Creates a string that is a copy of another string
	 *
	 * @param string
	 *          the String to copy
	 */
	public String(String string) {
		value = string.value;
		count = string.count;
		hashCode = string.hashCode;
	}

	/**
	 * Creates a string from the contents of a StringBuffer.
	 *
	 * @param buffer
	 *          the StringBuffer
	 */
	public String(StringBuffer buffer) {
		synchronized (buffer) {
			char[] chars = buffer.shareValue();

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
		if (concatlen < 0) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		if (enableCompression) {
			if (null == compressionFlag || (s1.count | s2.count) >= 0) {
				value = new char[(concatlen + 1) >>> 1];
				count = concatlen;

				compressedArrayCopy(s1.value, 0, value, 0, s1len);
				compressedArrayCopy(s2.value, 0, value, s1len, s2len);
			} else {
				value = new char[concatlen];
				count = concatlen | uncompressedBit;

				// Check if the String is compressed
				if (s1.count >= 0) {
					decompress(s1.value, 0, value, 0, s1len);
				} else {
					System.arraycopy(s1.value, 0, value, 0, s1len);
				}

				// Check if the String is compressed
				if (s2.count >= 0) {
					decompress(s2.value, 0, value, s1len, s2len);
				} else {
					System.arraycopy(s2.value, 0, value, s1len, s2len);
				}

				initCompressionFlag();
			}
		} else {
			value = new char[concatlen];
			count = concatlen;

			System.arraycopy(s1.value, 0, value, 0, s1len);
			System.arraycopy(s2.value, 0, value, s1len, s2len);
		}
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
		long totalLen = (long) s1len + (long) s2len + (long) s3len;
		if (totalLen > Integer.MAX_VALUE) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		int concatlen = (int) totalLen;

		if (enableCompression) {
			if (null == compressionFlag || (s1.count | s2.count | s3.count) >= 0) {
				value = new char[(concatlen + 1) >>> 1];
				count = concatlen;

				compressedArrayCopy(s1.value, 0, value, 0, s1len);
				compressedArrayCopy(s2.value, 0, value, s1len, s2len);
				compressedArrayCopy(s3.value, 0, value, s1len + s2len, s3len);
			} else {
				value = new char[concatlen];
				count = concatlen | uncompressedBit;

				// Check if the String is compressed
				if (s1.count >= 0) {
					decompress(s1.value, 0, value, 0, s1len);
				} else {
					System.arraycopy(s1.value, 0, value, 0, s1len);
				}

				// Check if the String is compressed
				if (s2.count >= 0) {
					decompress(s2.value, 0, value, s1len, s2len);
				} else {
					System.arraycopy(s2.value, 0, value, s1len, s2len);
				}

				// Check if the String is compressed
				if (s3.count >= 0) {
					decompress(s3.value, 0, value, s1len + s2len, s3len);
				} else {
					System.arraycopy(s3.value, 0, value, (s1len + s2len), s3len);
				}

				initCompressionFlag();
			}
		} else {
			value = new char[concatlen];
			count = concatlen;

			System.arraycopy(s1.value, 0, value, 0, s1len);
			System.arraycopy(s2.value, 0, value, s1len, s2len);
			System.arraycopy(s3.value, 0, value, (s1len + s2len), s3len);
		}
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
		if (len < 0) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		if (enableCompression) {
			// Check if the String is compressed
			if (null == compressionFlag || s1.count >= 0) {
				value = new char[(len + 1) >>> 1];
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
				value = new char[len];
				count = len | uncompressedBit;

				// Copy in v1
				int index = len - 1;

				do {
					int res = quot / 10;
					int rem = quot - (res * 10);

					quot = res;

					// Write the digit into the correct position
					value[index--] = (char) ('0' - rem);
				} while (quot != 0);

				if (v1 < 0) {
					value[index] = '-';
				}

				// Copy in s1 contents
				System.arraycopy(s1.value, 0, value, 0, s1len);

				initCompressionFlag();
			}
		} else {
			value = new char[len];
			count = len;

			// Copy in v1
			int index = len - 1;

			do {
				int res = quot / 10;
				int rem = quot - (res * 10);

				quot = res;

				// Write the digit into the correct position
				value[index--] = (char) ('0' - rem);
			} while (quot != 0);

			if (v1 < 0) {
				value[index] = '-';
			}

			// Copy in s1 contents
			System.arraycopy(s1.value, 0, value, 0, s1len);
		}
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
		long totalLen = (long) s1len + (long) v1len + (long) v2len + (long) s2len + (long) s3len;
		if (totalLen > Integer.MAX_VALUE) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		int len = (int) totalLen;

		if (enableCompression) {
			if (null == compressionFlag || (s1.count | s2.count | s3.count) >= 0) {
				value = new char[(len + 1) >>> 1];
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
				value = new char[len];
				count = len | uncompressedBit;

				int start = len;

				// Copy in s3 contents
				start = start - s3len;

				// Check if the String is compressed
				if (s3.count >= 0) {
					decompress(s3.value, 0, value, start, s3len);
				} else {
					System.arraycopy(s3.value, 0, value, start, s3len);
				}

				// Copy in s2 contents
				start = start - s2len;

				// Check if the String is compressed
				if (s2.count >= 0) {
					decompress(s2.value, 0, value, start, s2len);
				} else {
					System.arraycopy(s2.value, 0, value, start, s2len);
				}

				// Copy in v2
				int index2 = start - 1;

				do {
					int res = quot2 / 10;
					int rem = quot2 - (res * 10);

					quot2 = res;

					// Write the digit into the correct position
					value[index2--] = (char) ('0' - rem);
				} while (quot2 != 0);

				if (v2 < 0) {
					value[index2--] = '-';
				}

				// Copy in s1 contents
				start = index2 + 1 - s1len;

				// Check if the String is compressed
				if (s1.count >= 0) {
					decompress(s1.value, 0, value, start, s1len);
				} else {
					System.arraycopy(s1.value, 0, value, start, s1len);
				}

				// Copy in v1
				int index1 = start - 1;

				do {
					int res = quot1 / 10;
					int rem = quot1 - (res * 10);

					quot1 = res;

					// Write the digit into the correct position
					value[index1--] = (char) ('0' - rem);
				} while (quot1 != 0);

				if (v1 < 0) {
					value[index1--] = '-';
				}

				initCompressionFlag();
			}
		} else {
			value = new char[len];
			count = len;

			int start = len;

			// Copy in s3 contents
			start = start - s3len;
			System.arraycopy(s3.value, 0, value, start, s3len);

			// Copy in s2 contents
			start = start - s2len;
			System.arraycopy(s2.value, 0, value, start, s2len);

			// Copy in v2
			int index2 = start - 1;

			do {
				int res = quot2 / 10;
				int rem = quot2 - (res * 10);

				quot2 = res;

				// Write the digit into the correct position
				value[index2--] = (char) ('0' - rem);
			} while (quot2 != 0);

			if (v2 < 0) {
				value[index2--] = '-';
			}

			// Copy in s1 contents
			start = index2 + 1 - s1len;

			System.arraycopy(s1.value, 0, value, start, s1len);

			// Copy in v1
			int index1 = start - 1;

			do {
				int res = quot1 / 10;
				int rem = quot1 - (res * 10);

				quot1 = res;

				// Write the digit into the correct position
				value[index1--] = (char) ('0' - rem);
			} while (quot1 != 0);

			if (v1 < 0) {
				value[index1--] = '-';
			}
		}
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
	 *          the zero-based index in this string
	 * @return the character at the index
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code index < 0} or {@code index >= length()}
	 */
	public char charAt(int index) {
		if (0 <= index && index < lengthInternal()) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || count >= 0)) {
				return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
			}

			return value[index];
		}

		throw new StringIndexOutOfBoundsException();
	}

	// Internal version of charAt used for extracting a char from a String in compression related code.
	char charAtInternal(int index) {
		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || count >= 0)) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		}

		return value[index];
	}

	// This method is needed so idiom recognition properly recognizes idiomatic loops where we are doing an operation on
	// the byte[] value of two Strings. In such cases we extract the String.value fields before entering the operation loop.
	// However if chatAt is used inside the loop then the JIT will anchor the load of the value byte[] inside of the loop thus
	// causing us to load the String.value on every iteration. This is very suboptimal and breaks some of the common idioms
	// that we recognize. The most prominent one is the regionMatches arraycmp idiom that is not recognized unless this method
	// is being used.
	char charAtInternal(int index, char[] value) {
		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || count >= 0)) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		}

		return value[index];
	}

	/**
	 * Compares the specified String to this String using the Unicode values of the characters. Answer 0 if the strings contain the same characters in
	 * the same order. Answer a negative integer if the first non-equal character in this String has a Unicode value which is less than the Unicode
	 * value of the character at the same position in the specified string, or if this String is a prefix of the specified string. Answer a positive
	 * integer if the first non-equal character in this String has a Unicode value which is greater than the Unicode value of the character at the same
	 * position in the specified string, or if the specified String is a prefix of the this String.
	 *
	 * @param string
	 *          the string to compare
	 * @return 0 if the strings are equal, a negative integer if this String is before the specified String, or a positive integer if this String is
	 *          after the specified String
	 *
	 * @throws NullPointerException
	 *          when string is null
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

		char[] s1Value = s1.value;
		char[] s2Value = s2.value;

		if (enableCompression && (null == compressionFlag || (s1.count | s2.count) >= 0)) {
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

	private static char compareValue(char c) {
		if ('A' <= c && c <= 'Z') {
			return (char) (c + ('a' - 'A'));
		}

		return Character.toLowerCase(Character.toUpperCase(c));
	}

	private static char compareValue(byte b) {
		if ('A' <= b && b <= 'Z') {
			return (char) (helpers.byteToCharUnsigned(b) + ('a' - 'A'));
		}
		return Character.toLowerCase(Character.toUpperCase(helpers.byteToCharUnsigned(b)));
	}

	/**
	 * Compare the receiver to the specified String to determine the relative ordering when the case of the characters is ignored.
	 *
	 * @param string
	 *          a String
	 * @return an {@code int < 0} if this String is less than the specified String, 0 if they are equal, and {@code > 0} if this String is greater
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

		char[] s1Value = s1.value;
		char[] s2Value = s2.value;

		if (enableCompression && ((null == compressionFlag) || ((s1.count | s2.count) >= 0))) {
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
	 *          the string to concatenate
	 * @return a String which is the concatenation of this String and the specified String
	 *
	 * @throws NullPointerException
	 *          if string is null
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
		if (concatlen < 0) {
			/*[MSG "K0D01", "Array capacity exceeded"]*/
			throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
		}

		if (enableCompression && ((null == compressionFlag) || ((s1.count | s2.count) >= 0))) {
			char[] buffer = new char[(concatlen + 1) >>> 1];

			compressedArrayCopy(s1.value, 0, buffer, 0, s1len);
			compressedArrayCopy(s2.value, 0, buffer, s1len, s2len);

			return new String(buffer, 0, concatlen, true);
		} else {
			char[] buffer = new char[concatlen];

			// Check if the String is compressed
			if (enableCompression && s1.count >= 0) {
				decompress(s1.value, 0, buffer, 0, s1len);
			} else {
				System.arraycopy(s1.value, 0, buffer, 0, s1len);
			}

			// Check if the String is compressed
			if (enableCompression && s2.count >= 0) {
				decompress(s2.value, 0, buffer, s1len, s2len);
			} else {
				System.arraycopy(s2.value, 0, buffer, s1len, s2len);
			}

			return new String(buffer, 0, concatlen, false);
		}
	}

	/**
	 * Creates a new String containing the characters in the specified character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 * @return the new String
	 *
	 * @throws NullPointerException
	 *          if data is null
	 */
	public static String copyValueOf(char[] data) {
		return new String(data, 0, data.length);
	}

	/**
	 * Creates a new String containing the specified characters in the character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 * @param start
	 *          the starting offset in the character array
	 * @param length
	 *          the number of characters to use
	 * @return the new String
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          if data is null
	 */
	public static String copyValueOf(char[] data, int start, int length) {
		return new String(data, start, length);
	}

	/**
	 * Compares the specified string to this String to determine if the specified string is a suffix.
	 *
	 * @param suffix
	 *          the string to look for
	 * @return true when the specified string is a suffix of this String, false otherwise
	 *
	 * @throws NullPointerException
	 *          if suffix is null
	 */
	public boolean endsWith(String suffix) {
		return regionMatches(lengthInternal() - suffix.lengthInternal(), suffix, 0, suffix.lengthInternal());
	}

	/**
	 * Compares the specified object to this String and answer if they are equal. The object must be an instance of String with the same characters in
	 * the same order.
	 *
	 * @param object
	 *          the object to compare
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

			char[] s1Value = s1.value;
			char[] s2Value = s2.value;
			if (s1Value == s2Value) {
				return true;
			}

			// There was a time hole between first read of s.hashCode and second read if another thread does hashcode
			// computing for incoming string object
			int s1hash = s1.hashCode;
			int s2hash = s2.hashCode;

			if (s1hash != 0 && s2hash != 0 && s1hash != s2hash) {
				return false;
			}

			if (!regionMatchesInternal(s1, s2, s1Value, s2Value, 0, 0, s1len)) {
				return false;
			}

			if (com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY != com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY_DISABLED) {
				deduplicateStrings(s1, s1Value, s2, s2Value);
			}

			return true;
		}

		return false;
	}

	/**
	 * Deduplicate the backing buffers of the given strings.
	 *
	 * This updates the {@link #value} of one of the two given strings so that
	 * they both share a single backing buffer. The strings must have identical
	 * contents.
	 *
	 * Deduplication helps save space, and lets {@link #equals(Object)} exit
	 * early more often.
	 *
	 * The strings' corresponding backing buffers are accepted as parameters
	 * because the caller likely already has them.
	 *
	 * @param s1 The first string
	 * @param value1 {@code s1.value}
	 * @param s2 The second string
	 * @param value2 {@code s2.value}
	 */
	private static final void deduplicateStrings(String s1, Object value1, String s2, Object value2) {
		/* This test ensures that we only deduplicate strings that are both
		 * compressed or both uncompressed.
		 *
		 * When one string is compressed and the other isn't, we can't
		 * deduplicate because doing so would require updating both value and
		 * count, and other threads could see an inconsistent state.
		 *
		 * If (!enableCompression), then both strings are always uncompressed.
		 * If OTOH (enableCompression) but (null == compressionFlag), then both
		 * strings must be compressed. So it's only necessary to check the
		 * compression bits when (enableCompression && null != compressionFlag).
		 */
		if (!enableCompression || null == compressionFlag || (s1.count ^ s2.count) >= 0) {
			long valueFieldOffset = UnsafeHelpers.valueFieldOffset;

			if (com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY == com.ibm.oti.vm.VM.J9_JIT_STRING_DEDUP_POLICY_FAVOUR_LOWER) {
				if (helpers.acmplt(value1, value2)) {
					helpers.putObjectInObject(s2, valueFieldOffset, value1);
				} else {
					helpers.putObjectInObject(s1, valueFieldOffset, value2);
				}
			} else {
				if (helpers.acmplt(value2, value1)) {
					helpers.putObjectInObject(s2, valueFieldOffset, value1);
				} else {
					helpers.putObjectInObject(s1, valueFieldOffset, value2);
				}
			}
		}
	}

	/**
	 * Compares the specified String to this String ignoring the case of the characters and answer if they are equal.
	 *
	 * @param string
	 *          the string to compare
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

		// Zero length strings are equal
		if (0 == s1len) {
			return true;
		}

		int o1 = 0;
		int o2 = 0;

		// Upper bound index on the last char to compare
		int end = s1len;

		char[] s1Value = s1.value;
		char[] s2Value = s2.value;

		if (enableCompression && (null == compressionFlag || (s1.count | s2.count) >= 0)) {
			// Compare the last chars.
			// In order to tell 2 chars are different:
			// Under string compression, the compressible char set obeys 1-1 mapping for upper/lower case,
			// converting to lower cases then compare should be sufficient.
			byte byteAtO1Last = helpers.getByteFromArrayByIndex(s1Value, s1len - 1);
			byte byteAtO2Last = helpers.getByteFromArrayByIndex(s2Value, s1len - 1);

			if (byteAtO1Last != byteAtO2Last
					&& toUpperCase(helpers.byteToCharUnsigned(byteAtO1Last)) != toUpperCase(helpers.byteToCharUnsigned(byteAtO2Last))) {
				return false;
			}

			while (o1 < end - 1) {
				byte byteAtO1 = helpers.getByteFromArrayByIndex(s1Value, o1++);
				byte byteAtO2 = helpers.getByteFromArrayByIndex(s2Value, o2++);

				if (byteAtO1 != byteAtO2
						&& toUpperCase(helpers.byteToCharUnsigned(byteAtO1)) != toUpperCase(helpers.byteToCharUnsigned(byteAtO2))) {
					return false;
				}
			}
		} else {
			// Compare the last chars.
			// In order to tell 2 chars are different:
			// If at least one char is ASCII, converting to upper cases then compare should be sufficient.
			// If both chars are not in ASCII char set, need to convert to lower case and compare as well.
			char charAtO1Last = s1.charAtInternal(s1len - 1, s1Value);
			char charAtO2Last = s2.charAtInternal(s1len - 1, s2Value);

			if (charAtO1Last != charAtO2Last
					&& toUpperCase(charAtO1Last) != toUpperCase(charAtO2Last)
					&& ((charAtO1Last <= 255 && charAtO2Last <= 255) || Character.toLowerCase(charAtO1Last) != Character.toLowerCase(charAtO2Last))) {
				return false;
			}

			while (o1 < end - 1) {
				char charAtO1 = s1.charAtInternal(o1++, s1Value);
				char charAtO2 = s2.charAtInternal(o2++, s2Value);

				if (charAtO1 != charAtO2
						&& toUpperCase(charAtO1) != toUpperCase(charAtO2)
						&& ((charAtO1 <= 255 && charAtO2 <= 255) || Character.toLowerCase(charAtO1) != Character.toLowerCase(charAtO2))) {
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

		char[] buffer;

		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			buffer = new char[currentLength];
			decompress(value, 0, buffer, 0, currentLength);
		} else {
			buffer = value;
		}

		return StringCoding.encode(buffer, 0, currentLength);
	}

	/**
	 * Converts this String to a byte array, ignoring the high order bits of each character.
	 *
	 * @param start
	 *          the starting offset of characters to copy
	 * @param end
	 *          the ending offset of characters to copy
	 * @param data
	 *          the destination byte array
	 * @param index
	 *          the starting offset in the byte array
	 *
	 * @throws NullPointerException
	 *          when data is null
	 * @throws IndexOutOfBoundsException
	 *          when {@code start < 0, end > length(), index < 0, end - start > data.length - index}
	 *
	 * @deprecated Use getBytes() or getBytes(String)
	 */
	@Deprecated
	public void getBytes(int start, int end, byte[] data, int index) {
		if (0 <= start && start <= end && end <= lengthInternal() && 0 <= index && ((end - start) <= (data.length - index))) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || count >= 0)) {
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
			if (enableCompression && (null == compressionFlag || count >= 0)) {
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
	 *          the encoding
	 * @return the byte array encoding of this String
	 *
	 * @throws UnsupportedEncodingException
	 *          when the encoding is not supported
	 *
	 * @see String
	 * @see UnsupportedEncodingException
	 */
	public byte[] getBytes(String encoding) throws UnsupportedEncodingException {
		if (encoding == null) {
			throw new NullPointerException();
		}

		int currentLength = lengthInternal();

		char[] buffer;

		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			buffer = new char[currentLength];
			decompress(value, 0, buffer, 0, currentLength);
		} else {
			buffer = value;
		}

		return StringCoding.encode(encoding, buffer, 0, currentLength);
	}

	/**
	 * Copies the specified characters in this String to the character array starting at the specified offset in the character array.
	 *
	 * @param start
	 *          the starting offset of characters to copy
	 * @param end
	 *          the ending offset of characters to copy
	 * @param data
	 *          the destination character array
	 * @param index
	 *          the starting offset in the character array
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code start < 0, end > length(), start > end, index < 0, end - start > buffer.length - index}
	 * @throws NullPointerException
	 *          when buffer is null
	 */
	public void getChars(int start, int end, char[] data, int index) {
		if (0 <= start && start <= end && end <= lengthInternal() && 0 <= index && ((end - start) <= (data.length - index))) {
			if (enableCompression && (null == compressionFlag || count >= 0)) {
				decompress(value, start, data, index, end - start);
			} else {
				System.arraycopy(value, start, data, index, end - start);
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	// This is a package protected method that performs the getChars operation without explicit bound checks.
	// Caller of this method must validate bound safety for String indexing and array copying.
	void getCharsNoBoundChecks(int start, int end, char[] data, int index) {
		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || count >= 0)) {
			decompress(value, start, data, index, end - start);
		} else {
			decompressedArrayCopy(value, start, data, index, end - start);
		}
	}

	/**
	 * Answers an integer hash code for the receiver. Objects which are equal answer the same value for this method.
	 *
	 * @return the receiver's hash
	 *
	 * @see #equals
	 */
	public int hashCode() {
		if (hashCode == 0) {
			int length = lengthInternal();
			if (length > 0) {
				// Check if the String is compressed
				if (enableCompression && (compressionFlag == null || count >= 0)) {
					hashCode = hashCodeImplCompressed(value, 0, length);
				} else {
					hashCode = hashCodeImplDecompressed(value, 0, length);
				}
			}
		}
		return hashCode;
	}

	private static int hashCodeImplCompressed(char[] value, int offset, int count) {
		int hash = 0, end = offset + count;

		for (int i = offset; i < end; ++i) {
			hash = (hash << 5) - hash + helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, i));
		}

		return hash;
	}

	private static int hashCodeImplDecompressed(char[] value, int offset, int count) {
		int hash = 0, end = offset + count;

		for (int i = offset; i < end; ++i) {
			hash = (hash << 5) - hash + value[i];
		}

		return hash;
	}

	/**
	 * Searches in this String for the first index of the specified character. The search for the character starts at the beginning and moves towards
	 * the end of this String.
	 *
	 * @param c
	 *          the character to find
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
	 *          the character to find
	 * @param start
	 *          the starting offset
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
				char[] array = value;

				// Check if the String is compressed
				if (enableCompression && (null == compressionFlag || count >= 0)) {
					if (c <= 255) {
						return helpers.intrinsicIndexOfLatin1(array, (byte)c, start, len);
					}
				} else {
					return helpers.intrinsicIndexOfUTF16(array, (char)c, start, len);
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
	 *          the string to find
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *          when string is null
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int indexOf(String string) {
		return indexOf(string, 0);
	}

	/**
	 * Searches in this String for the index of the specified string. The search for the string starts at the specified offset and moves towards the
	 * end of this String.
	 *
	 * @param subString
	 *          the string to find
	 * @param start
	 *          the starting offset
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *          when string is null
	 *
	 * @see #lastIndexOf(int)
	 * @see #lastIndexOf(int, int)
	 * @see #lastIndexOf(String)
	 * @see #lastIndexOf(String, int)
	 */
	public int indexOf(String subString, int start) {
		if (subString.length() == 1) {
			return indexOf(subString.charAtInternal(0), start);
		}

		if (start < 0) {
			start = 0;
		}

		String s1 = this;
		String s2 = subString;

		int s1len = s1.lengthInternal();
		int s2len = s2.lengthInternal();

		if (s2len > 0) {
			if (start > s1len - s2len) {
				return -1;
			}

			char[] s1Value = s1.value;
			char[] s2Value = s2.value;

			if (enableCompression) {
				if (null == compressionFlag || (s1.count | s2.count) >= 0) {
					// Both s1 and s2 are compressed.
					return helpers.intrinsicIndexOfStringLatin1(s1Value, s1len, s2Value, s2len, start);
				} else if ((s1.count & s2.count) < 0) {
					// Both s1 and s2 are decompressed.
					return helpers.intrinsicIndexOfStringUTF16(s1Value, s1len, s2Value, s2len, start);
				} else {
					// Mixed case.
					char firstChar = s2.charAtInternal(0, s2Value);

					while (true) {
						int i = indexOf(firstChar, start);

						// Handles subCount > count || start >= count.
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
				// Both s1 and s2 are decompressed.
				return helpers.intrinsicIndexOfStringUTF16(s1Value, s1len, s2Value, s2len, start);
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
	 *          the character to find
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
	 *          the character to find
	 * @param start
	 *          the starting offset
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
				char[] array = value;

				// Check if the String is compressed
				if (enableCompression && (null == compressionFlag || count >= 0)) {
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
						if (array[i] == c) {
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
	 *          the string to find
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *          when string is null
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
	 *          the string to find
	 * @param start
	 *          the starting offset
	 * @return the index in this String of the specified string, -1 if the string isn't found
	 *
	 * @throws NullPointerException
	 *          when string is null
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

				char[] s1Value = s1.value;
				char[] s2Value = s2.value;

				if (enableCompression && (null == compressionFlag || (s1.count | s2.count) >= 0)) {
					char firstChar = helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(s2Value, 0));

					while (true) {
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
			if (compressionFlag == null || count >= 0) {
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
	 *          the starting offset in this String
	 * @param string
	 *          the string to compare
	 * @param start
	 *          the starting offset in string
	 * @param length
	 *          the number of characters to compare
	 * @return true if the ranges of characters is equal, false otherwise
	 *
	 * @throws NullPointerException
	 *          when string is null
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

		return regionMatchesInternal(s1, s2, s1.value, s2.value, thisStart, start, length);
	}

	private static boolean regionMatchesInternal(String s1, String s2, char[] s1Value, char[] s2Value, int s1Start, int s2Start, int length)
	{
		if (length <= 0) {
			return true;
		}

		// Index of the last char to compare
		int end = length - 1;

		if (enableCompression && ((compressionFlag == null) || ((s1.count | s2.count) >= 0))) {
			if (helpers.getByteFromArrayByIndex(s1Value, s1Start + end) != helpers.getByteFromArrayByIndex(s2Value, s2Start + end)) {
				return false;
			} else {
				for (int i = 0; i < end; ++i) {
					if (helpers.getByteFromArrayByIndex(s1Value, s1Start + i) != helpers.getByteFromArrayByIndex(s2Value, s2Start + i)) {
						return false;
					}
				}
			}
		} else {
			if (s1.charAtInternal(s1Start + end, s1Value) != s2.charAtInternal(s2Start + end, s2Value)) {
				return false;
			} else {
				for (int i = 0; i < end; ++i) {
					if (s1.charAtInternal(s1Start + i, s1Value) != s2.charAtInternal(s2Start + i, s2Value)) {
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
	 *          specifies if case should be ignored
	 * @param thisStart
	 *          the starting offset in this String
	 * @param string
	 *          the string to compare
	 * @param start
	 *          the starting offset in string
	 * @param length
	 *          the number of characters to compare
	 * @return true if the ranges of characters is equal, false otherwise
	 *
	 * @throws NullPointerException
	 *          when string is null
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

		char[] s1Value = s1.value;
		char[] s2Value = s2.value;

		if (enableCompression && (null == compressionFlag || (s1.count | s2.count) >= 0)) {
			while (o1 < end) {
				byte byteAtO1 = helpers.getByteFromArrayByIndex(s1Value, o1++);
				byte byteAtO2 = helpers.getByteFromArrayByIndex(s2Value, o2++);

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
	 *          the character to replace
	 * @param newChar
	 *          the replacement character
	 * @return a String with occurrences of oldChar replaced by newChar
	 */
	public String replace(char oldChar, char newChar) {
		int index = indexOf(oldChar, 0);

		if (index == -1) {
			return this;
		}

		int len = lengthInternal();

		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || count >= 0)) {
			if (newChar <= 255) {
				char[] buffer = new char[(len + 1) >>> 1];

				compressedArrayCopy(value, 0, buffer, 0, len);

				do {
					helpers.putByteInArrayByIndex(buffer, index++, (byte) newChar);
				} while ((index = indexOf(oldChar, index)) != -1);

				return new String(buffer, 0, len, true);
			} else {
				char[] buffer = new char[len];

				decompress(value, 0, buffer, 0, len);

				do {
					buffer[index++] = newChar;
				} while ((index = indexOf(oldChar, index)) != -1);

				return new String(buffer, 0, len, false);
			}
		} else {
			char[] buffer = new char[len];

			System.arraycopy(value, 0, buffer, 0, len);

			do {
				buffer[index++] = newChar;
			} while ((index = indexOf(oldChar, index)) != -1);

			return new String(buffer, 0, len, false);
		}
	}

	/**
	 * Compares the specified string to this String to determine if the specified string is a prefix.
	 *
	 * @param prefix
	 *          the string to look for
	 * @return true when the specified string is a prefix of this String, false otherwise
	 *
	 * @throws NullPointerException
	 *          when prefix is null
	 */
	public boolean startsWith(String prefix) {
		return startsWith(prefix, 0);
	}

	/**
	 * Compares the specified string to this String, starting at the specified offset, to determine if the specified string is a prefix.
	 *
	 * @param prefix
	 *          the string to look for
	 * @param start
	 *          the starting offset
	 * @return true when the specified string occurs in this String at the specified offset, false otherwise
	 *
	 * @throws NullPointerException
	 *          when prefix is null
	 */
	public boolean startsWith(String prefix, int start) {
		if (prefix.length() == 1) {
			if (start < 0 || start >= this.length()) {
				return false;
			}
			return charAtInternal(start) == prefix.charAtInternal(0);
		}
		return regionMatches(start, prefix, 0, prefix.lengthInternal());
	}

	/**
	 * Copies a range of characters into a new String.
	 *
	 * @param start
	 *          the offset of the first character
	 * @return a new String containing the characters from start to the end of the string
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code start < 0} or {@code start > length()}
	 */
	public String substring(int start) {
		if (start == 0) {
			return this;
		}

		int len = lengthInternal();

		if (0 <= start && start <= len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || count >= 0)) {
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
	 *          the offset of the first character
	 * @param end
	 *          the offset one past the last character
	 * @return a String containing the characters from start to end - 1
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code start < 0, start > end} or {@code end > length()}
	 */
	public String substring(int start, int end) {
		int len = lengthInternal();

		if (start == 0 && end == len) {
			return this;
		}

		if (0 <= start && start <= end && end <= len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || count >= 0)) {
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
		if (enableCompression && (null == compressionFlag || count >= 0)) {
			decompress(value, 0, buffer, 0, len);
		} else {
			System.arraycopy(value, 0, buffer, 0, len);
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

	private static int toLowerCase(int codePoint) {
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

	private static int toUpperCase(int codePoint) {
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
	 *          the Locale
	 * @return a String containing the lowercase characters equivalent to the characters in this String
	 */
	public String toLowerCase(Locale locale) {
		// check locale for null
		String language = locale.getLanguage();
		int sLength = lengthInternal();

		if (sLength == 0) {
			return this;
		}

		if (helpers.supportsIntrinsicCaseConversion() && (language == "en")) { //$NON-NLS-1$
			if (enableCompression && ((null == compressionFlag) || (count >= 0))) {
				char[] output = new char[(sLength + 1) >>> 1];
				if (helpers.toLowerIntrinsicLatin1(value, output, sLength)) {
					return new String(output, 0, sLength, true);
				}
			} else if (sLength <= (Integer.MAX_VALUE / 2)) {
				char[] output = new char[sLength];
				if (helpers.toLowerIntrinsicUTF16(value, output, sLength * 2)) {
					return new String(output, 0, sLength, false);
				}
			}
		}

		return toLowerCaseCore(language);
	}

	/**
	 * The core of lower case conversion. This is the old, not-as-fast path.
	 *
	 * @param language
	 *          a string representing the Locale
	 * @return a new string object
	 */
	private String toLowerCaseCore(String language) {
		boolean turkishAzeri = false;
		boolean lithuanian = false;

		StringBuilder builder = null;

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
					if (enableCompression && (null == compressionFlag || count >= 0)) {
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
			mid = (low + high) >>> 1;

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

	/* The following code points are extracted from the Canonical_Combining_Class=Above table found in:
	 * https://www.unicode.org/Public/6.2.0/ucd/extracted/DerivedCombiningClass.txt
	 */
	private static char[] startCombiningAbove = { '\u0300', '\u033D', '\u0346', '\u034A', '\u0350', '\u0357', '\u035B', '\u0363', '\u0483', '\u0592',
			'\u0597', '\u059C', '\u05A8', '\u05AB', '\u05AF', '\u05C4', '\u0610', '\u0653', '\u0657', '\u065D', '\u06D6', '\u06DF', '\u06E4', '\u06E7',
			'\u06EB', '\u0730', '\u0732', '\u0735', '\u073A', '\u073D', '\u073F', '\u0743', '\u0745', '\u0747', '\u0749', '\u07EB', '\u07F3', '\u0816',
			'\u081B', '\u0825', '\u0829', '\u08E4', '\u08E7', '\u08EA', '\u08F3', '\u08F7', '\u08FB', '\u0951', '\u0953', '\u0F82', '\u0F86', '\u135D',
			'\u17DD', '\u193A', '\u1A17', '\u1A75', '\u1B6B', '\u1B6D', '\u1CD0', '\u1CDA', '\u1CE0', '\u1CF4', '\u1DC0', '\u1DC3', '\u1DCB', '\u1DD1',
			'\u1DFE', '\u20D0', '\u20D4', '\u20DB', '\u20E1', '\u20E7', '\u20E9', '\u20F0', '\u2CEF', '\u2DE0', '\uA66F', '\uA674', '\uA69F', '\uA6F0',
			'\uA8E0', '\uAAB0', '\uAAB2', '\uAAB7', '\uAABE', '\uAAC1', '\uFE20' };
	private static char[] endCombiningAbove = { '\u0314', '\u0344', '\u0346', '\u034C', '\u0352', '\u0357', '\u035B', '\u036F', '\u0487', '\u0595',
			'\u0599', '\u05A1', '\u05A9', '\u05AC', '\u05AF', '\u05C4', '\u0617', '\u0654', '\u065B', '\u065E', '\u06DC', '\u06E2', '\u06E4', '\u06E8',
			'\u06EC', '\u0730', '\u0733', '\u0736', '\u073A', '\u073D', '\u0741', '\u0743', '\u0745', '\u0747', '\u074A', '\u07F1', '\u07F3', '\u0819',
			'\u0823', '\u0827', '\u082D', '\u08E5', '\u08E8', '\u08EC', '\u08F5', '\u08F8', '\u08FE', '\u0951', '\u0954', '\u0F83', '\u0F87', '\u135F',
			'\u17DD', '\u193A', '\u1A17', '\u1A7C', '\u1B6B', '\u1B73', '\u1CD2', '\u1CDB', '\u1CE0', '\u1CF4', '\u1DC1', '\u1DC9', '\u1DCC', '\u1DE6',
			'\u1DFE', '\u20D1', '\u20D7', '\u20DC', '\u20E1', '\u20E7', '\u20E9', '\u20F0', '\u2CF1', '\u2DFF', '\uA66F', '\uA67D', '\uA69F', '\uA6F1',
			'\uA8F1', '\uAAB0', '\uAAB3', '\uAAB8', '\uAABF', '\uAAC1', '\uFE26' };
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
		/* The following code points are extracted from the Canonical_Combining_Class=Above table found in:
		 * https://www.unicode.org/Public/6.2.0/ucd/extracted/DerivedCombiningClass.txt
		 */
		} else if (codePoint == 0x10A0F || codePoint == 0x10A38 ||
			(codePoint >= 0x11100 && codePoint <= 0x11102) ||
			(codePoint >= 0x1D185 && codePoint <= 0x1D189) ||
			(codePoint >= 0x1D1AA && codePoint <= 0x1D1AD) ||
			(codePoint >= 0x1D242 && codePoint <= 0x1D244)) {
			return true;
		}

		return false;
	}

	private static boolean isWordPart(int codePoint) {
		return codePoint == 0x345 || isWordStart(codePoint);
	}

	private static boolean isWordStart(int codePoint) {
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
	 *          the char being converted to upper case
	 *
	 * @return the index into the upperValues table, or -1
	 */
	private static int upperIndex(int ch) {
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
	 *          the Locale
	 * @return a String containing the uppercase characters equivalent to the characters in this String
	 */
	public String toUpperCase(Locale locale) {
		String language = locale.getLanguage();
		int sLength = lengthInternal();

		if (sLength == 0) {
			return this;
		}

		if (helpers.supportsIntrinsicCaseConversion() && (language == "en")) { //$NON-NLS-1$
			if (enableCompression && ((null == compressionFlag) || (count >= 0))) {
				char[] output = new char[(sLength + 1) >>> 1];
				if (helpers.toUpperIntrinsicLatin1(value, output, sLength)) {
					return new String(output, 0, sLength, true);
				}
			} else if (sLength <= (Integer.MAX_VALUE / 2)) {
				char[] output = new char[sLength];
				if (helpers.toUpperIntrinsicUTF16(value, output, sLength * 2)) {
					return new String(output, 0, sLength, false);
				}
			}
		}

		return toUpperCaseCore(language);
	}

	/**
	 * The core of upper case conversion. This is the old, not-as-fast path.
	 *
	 * @param language
	 *          the string representing the locale
	 * @return the upper case string
	 */
	private String toUpperCaseCore(String language) {
		boolean turkishAzeri = language == "tr" || language == "az"; //$NON-NLS-1$ //$NON-NLS-2$
		boolean lithuanian = language == "lt"; //$NON-NLS-1$

		StringBuilder builder = null;

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
						if (enableCompression && (null == compressionFlag || count >= 0)) {
							builder.append(value, 0, i, true);
						} else {
							builder.append(value, 0, i, false);
						}
					}

					builder.appendCodePoint(upper);
				} else if (builder != null) {
					builder.appendCodePoint(codePoint);
				}

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
	 * @return a String with characters {@code <= \\u0020} removed from the beginning and the end
	 */
	public String trim() {
		int start = 0;
		int last = lengthInternal() - 1;
		int end = last;

		// Check if the String is compressed
		if (enableCompression && (null == compressionFlag || count >= 0)) {
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
	 *          the array of characters
	 * @return the String
	 *
	 * @throws NullPointerException
	 *          when data is null
	 */
	public static String valueOf(char[] data) {
		return new String(data, 0, data.length);
	}

	/**
	 * Returns a String containing the specified characters in the character array. Modifying the character array after creating the String has no
	 * effect on the String.
	 *
	 * @param data
	 *          the array of characters
	 * @param start
	 *          the starting offset in the character array
	 * @param length
	 *          the number of characters to use
	 * @return the String
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
	 */
	public static String valueOf(char[] data, int start, int length) {
		return new String(data, start, length);
	}

	/**
	 * Converts the specified character to its string representation.
	 *
	 * @param value
	 *          the character
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
			string = new String(new char[] { value }, 0, 1, false);
		}

		return string;
	}

	/**
	 * Converts the specified double to its string representation.
	 *
	 * @param value
	 *          the double
	 * @return the double converted to a string
	 */
	public static String valueOf(double value) {
		return Double.toString(value);
	}

	/**
	 * Converts the specified float to its string representation.
	 *
	 * @param value
	 *          the float
	 * @return the float converted to a string
	 */
	public static String valueOf(float value) {
		return Float.toString(value);
	}

	/**
	 * Converts the specified integer to its string representation.
	 *
	 * @param value
	 *          the integer
	 * @return the integer converted to a string
	 */
	public static String valueOf(int value) {
		return Integer.toString(value);
	}

	/**
	 * Converts the specified long to its string representation.
	 *
	 * @param value
	 *          the long
	 * @return the long converted to a string
	 */
	public static String valueOf(long value) {
		return Long.toString(value);
	}

	/**
	 * Converts the specified object to its string representation. If the object is null answer the string {@code "null"}, otherwise use
	 * {@code toString()} to get the string representation.
	 *
	 * @param value
	 *          the object
	 * @return the object converted to a string
	 */
	public static String valueOf(Object value) {
		return value != null ? value.toString() : "null"; //$NON-NLS-1$
	}

	/**
	 * Converts the specified boolean to its string representation. When the boolean is true answer {@code "true"}, otherwise answer
	 * {@code "false"}.
	 *
	 * @param value
	 *          the boolean
	 * @return the boolean converted to a string
	 */
	public static String valueOf(boolean value) {
		return value ? "true" : "false"; //$NON-NLS-1$ //$NON-NLS-2$
	}

	/**
	 * Answers whether the characters in the StringBuffer buffer are the same as those in this String.
	 *
	 * @param buffer
	 *          the StringBuffer to compare this String to
	 * @return true when the characters in buffer are identical to those in this String. If they are not, false will be returned.
	 *
	 * @throws NullPointerException
	 *          when buffer is null
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
	 *          the regular expression to be matched
	 * @return true if the expression matches, otherwise false
	 *
	 * @throws PatternSyntaxException
	 *          if the syntax of the supplied regular expression is not valid
	 * @throws NullPointerException
	 *          if expr is null
	 *
	 * @since 1.4
	 */
	public boolean matches(String expr) {
		return Pattern.matches(expr, this);
	}

	/**
	 * Replace any substrings within this String that match the supplied regular expression expr, with the String substitute.
	 *
	 * @param regex
	 *          the regular expression to match
	 * @param substitute
	 *          the string to replace the matching substring with
	 * @return the new string
	 *
	 * @throws NullPointerException
	 *          if expr is null
	 *
	 * @since 1.4
	 */
	public String replaceAll(String regex, String substitute) {
		// this is a fast path to handle replacements of 1 character with another or the deletion of
		// a single character (common operations when dealing with things like package names, file
		// system paths etc). In these simple cases a linear scan of the string is all that is necessary
		// and we can avoid the cost of building a full regex pattern matcher
		if (regex != null && substitute != null && regex.lengthInternal() == 1 && !hasMetaChars(regex)) {
			int substituteLength = substitute.lengthInternal();
			int length = lengthInternal();
			if (substituteLength < 2) {
				if (enableCompression && isCompressed() && (substituteLength == 0 || substitute.isCompressed())) {
					char[] newChars = new char[(length + 1) >>> 1];
					byte toReplace = helpers.getByteFromArrayByIndex(regex.value, 0);
					byte replacement = (byte)-1;  // assign dummy value that will never be used
					if (substituteLength == 1) {
						replacement = helpers.getByteFromArrayByIndex(substitute.value, 0);
						checkLastChar((char)replacement);
					}
					int newCharIndex = 0;
					for (int i = 0; i < length; ++i) {
						byte current = helpers.getByteFromArrayByIndex(value, i);
						if (current != toReplace) {
							helpers.putByteInArrayByIndex(newChars, newCharIndex++, current);
						} else if (substituteLength == 1) {
							helpers.putByteInArrayByIndex(newChars, newCharIndex++, replacement);
						}
					}
					return new String(newChars, 0, newCharIndex, true);
				} else if (!enableCompression || !isCompressed()) {
					char[] newChars = new char[length];
					char toReplace = regex.charAtInternal(0);
					char replacement = (char)-1; // assign dummy value that will never be used
					if (substituteLength == 1) {
						replacement = substitute.charAtInternal(0);
						checkLastChar(replacement);
					}
					int newCharIndex = 0;
					for (int i = 0; i < length; ++i) {
						char current = helpers.getCharFromArrayByIndex(value, i);
						if (current != toReplace) {
							helpers.putCharInArrayByIndex(newChars, newCharIndex++, current);
						} else if (substituteLength == 1) {
							helpers.putCharInArrayByIndex(newChars, newCharIndex++, replacement);
						}
					}
					return new String(newChars, 0, newCharIndex, false);
				}
			}
		}
		return Pattern.compile(regex).matcher(this).replaceAll(substitute);
	}

	/**
	 * Replace any substrings within this String that match the supplied regular expression expr, with the String substitute.
	 *
	 * @param expr
	 *          the regular expression to match
	 * @param substitute
	 *          the string to replace the matching substring with
	 * @return the new string
	 *
	 * @throws NullPointerException
	 *          if expr is null
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
	 *          Regular expression that is used as a delimiter
	 * @return The array of strings which are split around the regex
	 *
	 * @throws PatternSyntaxException
	 *          if the syntax of regex is invalid
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

	private static final boolean isSingleEscapeLiteral(String s) {
		if ((s != null) && (s.lengthInternal() == 2) && (s.charAtInternal(0) == '\\')) {
			char literal = s.charAtInternal(1);
			for (int j = 0; j < regexMetaChars.length; ++j) {
				if (literal == regexMetaChars[j]) return true;
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
	 * @param regex Regular expression that is used as a delimiter
	 * @param max The threshold of the returned array
	 * @return The array of strings which are split around the regex
	 *
	 * @throws PatternSyntaxException if the syntax of regex is invalid
	 *
	 * @since 1.4
	 */
	public String[] split(String regex, int max) {
		// it is faster to handle simple splits inline (i.e. no fancy regex matching),
		// including single escaped literal character (e.g. \. \{),
		// so we test for a suitable string and handle this here if we can
		boolean singleEscapeLiteral = isSingleEscapeLiteral(regex);
		if ((regex != null) && (regex.lengthInternal() > 0) && (!hasMetaChars(regex) || singleEscapeLiteral)) {
			if (max == 1) {
				return new String[] { this };
			}
			java.util.ArrayList<String> parts = new java.util.ArrayList<String>((max > 0 && max < 100) ? max : 10);

			char[] chars = this.value;

			final boolean compressed = enableCompression && (null == compressionFlag || count >= 0);

			int start = 0, current = 0, end = lengthInternal();
			if (regex.lengthInternal() == 1 || singleEscapeLiteral) {
				// if matching single escaped character, use the second char.
				char splitChar = regex.charAtInternal(singleEscapeLiteral ? 1 : 0);
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

				char[] splitChars = regex.value;

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
	 *          the offset the first character
	 * @param end
	 *          the offset of one past the last character to include
	 *
	 * @return the subsequence requested
	 *
	 * @throws IndexOutOfBoundsException
	 *          when start or end is less than zero, start is greater than end, or end is greater than the length of the String.
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
	 *          the byte array to convert to a String
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 *
	 * @since 1.5
	 */
	public String(int[] data, int start, int length) {
		if (start >= 0 && 0 <= length && length <= data.length - start) {
			int size = 0;

			// Optimistically assume we can compress data[]
			boolean canEncodeAsLatin1 = enableCompression;

			for (int i = start; i < start + length; ++i) {
				int codePoint = data[i];

				if (codePoint < Character.MIN_CODE_POINT) {
					throw new IllegalArgumentException();
				} else if (codePoint < Character.MIN_SUPPLEMENTARY_CODE_POINT) {
					if (canEncodeAsLatin1 && codePoint > 255) {
						canEncodeAsLatin1 = false;
					}

					++size;
				} else if (codePoint <= Character.MAX_CODE_POINT) {
					if (canEncodeAsLatin1) {
						codePoint -= Character.MIN_SUPPLEMENTARY_CODE_POINT;

						int codePoint1 = Character.MIN_HIGH_SURROGATE + (codePoint >> 10);
						int codePoint2 = Character.MIN_LOW_SURROGATE + (codePoint & 0x3FF);

						if (codePoint1 > 255 || codePoint2 > 255) {
							canEncodeAsLatin1 = false;
						}
					}

					size += 2;
				} else {
					throw new IllegalArgumentException();
				}
			}

			if (canEncodeAsLatin1) {
				value = new char[(size + 1) >>> 1];
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
				value = new char[size];

				if (enableCompression) {
					count = size | uncompressedBit;

					initCompressionFlag();
				} else {
					count = size;
				}

				for (int i = start, j = 0; i < start + length; ++i) {
					int codePoint = data[i];

					if (codePoint < Character.MIN_SUPPLEMENTARY_CODE_POINT) {
						value[j++] = (char) codePoint;
					} else {
						codePoint -= Character.MIN_SUPPLEMENTARY_CODE_POINT;

						value[j++] = (char) (Character.MIN_HIGH_SURROGATE + (codePoint >> 10));
						value[j++] = (char) (Character.MIN_LOW_SURROGATE + (codePoint & 0x3FF));
					}
				}
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Creates a string from the contents of a StringBuilder.
	 *
	 * @param builder
	 *          the StringBuilder
	 *
	 * @since 1.5
	 */
	public String(StringBuilder builder) {
		char[] chars = builder.shareValue();

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
	}

	/**
	 * Returns the Unicode character at the given point.
	 *
	 * @param index
	 *          the character index
	 * @return the Unicode character value at the index
	 *
	 * @since 1.5
	 */
	public int codePointAt(int index) {
		int len = lengthInternal();

		if (index >= 0 && index < len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || count >= 0)) {
				return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
			} else {
				char high = charAtInternal(index);

				if ((index < (len - 1)) && Character.isHighSurrogate(high)) {
					char low = charAtInternal(index + 1);

					if (Character.isLowSurrogate(low)) {
						return Character.toCodePoint(high, low);
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
	 *          the character index
	 * @return the Unicode character value before the index
	 *
	 * @since 1.5
	 */
	public int codePointBefore(int index) {
		int len = lengthInternal();

		if (index > 0 && index <= len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || count >= 0)) {
				return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index - 1));
			} else {
				char low = charAtInternal(index - 1);

				if ((index > 1) && Character.isLowSurrogate(low)) {
					char high = charAtInternal(index - 2);

					if (Character.isHighSurrogate(high)) {
						return Character.toCodePoint(high, low);
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
	 *          first index
	 * @param end
	 *          last index
	 * @return the total Unicode values
	 *
	 * @since 1.5
	 */
	public int codePointCount(int start, int end) {
		int len = lengthInternal();

		if (start >= 0 && start <= end && end <= len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || count >= 0)) {
				return end - start;
			} else {
				int count = 0;

				for (int i = start; i < end; ++i) {
					if ((i < (end - 1))
							&& Character.isHighSurrogate(charAtInternal(i))
							&& Character.isLowSurrogate(charAtInternal(i + 1))) {
						++i;
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
	 * Returns the index of the code point that was offset by codePointCount.
	 *
	 * @param start
	 *          the position to offset
	 * @param codePointCount
	 *          the code point count
	 * @return the offset index
	 *
	 * @since 1.5
	 */
	public int offsetByCodePoints(int start, int codePointCount) {
		int len = lengthInternal();

		if (start >= 0 && start <= len) {
			// Check if the String is compressed
			if (enableCompression && (null == compressionFlag || count >= 0)) {
				int index = start + codePointCount;

				if (index > len) {
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

						if ((index < (len - 1))
								&& Character.isHighSurrogate(charAtInternal(index))
								&& Character.isLowSurrogate(charAtInternal(index + 1))) {
							index++;
						}

						index++;
					}
				} else {
					for (int i = codePointCount; i < 0; ++i) {
						if (index < 1) {
							throw new IndexOutOfBoundsException();
						}

						if ((index > 1)
								&& Character.isLowSurrogate(charAtInternal(index - 1))
								&& Character.isHighSurrogate(charAtInternal(index - 2))) {
							index--;
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
	 *          the character sequence
	 * @return {@code true} if the content of this String is equal to the character sequence, {@code false} otherwise.
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
	 *          the sequence to compare to
	 * @return {@code true} if this String contains the sequence, {@code false} otherwise.
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
	 *          the old character sequence
	 * @param sequence2
	 *          the new character sequence
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
			int sequence2len = sequence2.length();

			if ((sequence2len != 0) && (len >= ((Integer.MAX_VALUE - len) / sequence2len))) {
				/*[MSG "K0D01", "Array capacity exceeded"]*/
				throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K0D01")); //$NON-NLS-1$
			}

			StringBuilder builder = new StringBuilder(len + ((len + 1) * sequence2len));

			builder.append(sequence2);

			for (int i = 0; i < len; ++i) {
				builder.append(charAt(i)).append(sequence2);
			}

			return builder.toString();
		} else {
			StringBuilder builder = new StringBuilder();

			int start = 0;
			int copyStart = 0;

			char charAt0 = sequence1.charAt(0);

			while (start < len) {
				int firstIndex = indexOf(charAt0, start);

				if (firstIndex == -1) {
					break;
				}

				boolean found = true;

				if (sequence1len > 1) {
					if (sequence1len > len - firstIndex) {
						/* the tail of this string is too short to find sequence1 */
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
	 *          the format to use
	 * @param args
	 *          the format arguments to use
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
	 *          the locale used to create the Formatter, may be null
	 * @param format
	 *          the format to use
	 * @param args
	 *          the format arguments to use
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
	 *          the byte array to convert to a String
	 * @param charset
	 *          the Charset to use
	 *
	 * @throws NullPointerException
	 *          when data is null
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
	 *          the byte array to convert to a String
	 * @param start
	 *          the starting offset in the byte array
	 * @param length
	 *          the number of bytes to convert
	 * @param charset
	 *          the Charset to use
	 *
	 * @throws IndexOutOfBoundsException
	 *          when {@code length < 0, start < 0} or {@code start + length > data.length}
	 * @throws NullPointerException
	 *          when data is null
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
			char[] chars = StringCoding.decode(charset, data, start, length);

			if (enableCompression) {
				if (canEncodeAsLatin1(chars, 0, chars.length)) {
					value = new char[(chars.length + 1) >>> 1];
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
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	}

	/**
	 * Converts this String to a byte encoding using the specified Charset.
	 *
	 * @param charset
	 *          the Charset to use
	 * @return the byte array encoding of this String
	 *
	 * @since 1.6
	 */
	public byte[] getBytes(Charset charset) {
		int currentLength = lengthInternal();

		char[] buffer;

		// Check if the String is compressed
		if (enableCompression && count >= 0) {
			buffer = new char[currentLength];
			decompress(value, 0, buffer, 0, currentLength);
		} else {
			buffer = value;
		}

		return StringCoding.encode(charset, buffer, 0, currentLength);
	}

	/**
	 * Creates a new String by putting each element together joined by the delimiter. If an element is null, then "null" is used as string to join.
	 *
	 * @param delimiter
	 *          Used as joiner to put elements together
	 * @param elements
	 *          Elements to be joined
	 * @return string of joined elements by delimiter
	 * @throws NullPointerException
	 *          if one of the arguments is null
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
	 *          Used as joiner to put elements together
	 * @param elements
	 *          Elements to be joined
	 * @return string of joined elements by delimiter
	 * @throws NullPointerException
	 *          if one of the arguments is null
	 */
	public static String join(CharSequence delimiter, Iterable<? extends CharSequence> elements) {
		StringJoiner stringJoiner = new StringJoiner(delimiter);

		Iterator<? extends CharSequence> elementsIterator = elements.iterator();

		while (elementsIterator.hasNext()) {
			stringJoiner.add(elementsIterator.next());
		}

		return stringJoiner.toString();
	}

/*[ENDIF] Sidecar19-SE*/

/*[IF JAVA_SPEC_VERSION >= 12]*/
	/**
	 * Apply a function to this string. The function expects a single String input
	 * and returns an R.
	 *
	 * @param f
	 *          the functional interface to be applied
	 *
	 * @return the result of application of the function to this string
	 *
	 * @since 12
	 */
	public <R> R transform(Function<? super String, ? extends R> f) {
		return f.apply(this);
	}

	/**
	 * Returns the nominal descriptor of this String instance, or an empty optional
	 * if construction is not possible.
	 *
	 * @return Optional with nominal descriptor of String instance
	 *
	 * @since 12
	 */
	public Optional<String> describeConstable() {
		return Optional.of(this);
	}

	/**
	 * Resolves this ConstantDesc instance.
	 *
	 * @param lookup
	 *          parameter is ignored
	 *
	 * @return the resolved Constable value
	 *
	 * @since 12
	 */
	public String resolveConstantDesc(MethodHandles.Lookup lookup) {
		return this;
	}

	/**
	 * Indents each line of the string depending on the value of n, and normalizes
	 * line terminators to the newline character "\n".
	 *
	 * @param n
	 *          the number of spaces to indent the string
	 *
	 * @return the indented string with normalized line terminators
	 *
	 * @since 12
	 */
	public String indent(int n) {
		Stream<String> lines = lines();
		Iterator<String> iter = lines.iterator();
		StringBuilder builder = new StringBuilder();

		String spaces = n > 0 ? " ".repeat(n) : null;
		int absN = Math.abs(n);

		while (iter.hasNext()) {
			String currentLine = iter.next();

			if (n > 0) {
				builder.append(spaces);
			} else if (n < 0) {
				int start = 0;

				while ((currentLine.length() > start)
					&& (Character.isWhitespace(currentLine.charAt(start)))
				) {
					start++;

					if (start >= absN) {
						break;
					}
				}
				currentLine = currentLine.substring(start);
			}

			/**
			 * Line terminators are removed when lines() is called. A newline character is
			 * added to the end of each line, to normalize line terminators.
			 */
			builder.append(currentLine);
			builder.append("\n");
		}

		return builder.toString();
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */

/*[IF JAVA_SPEC_VERSION >= 13]*/
	/**
	 * Determine if current String object is LATIN1.
	 *
	 * @return true if it is LATIN1, otherwise false.
	 */
	boolean isLatin1() {
		return LATIN1 == coder();
	}

	/**
	 * Format the string using this string as the format with supplied args.
	 *
	 * @param args
	 *          the format arguments to use
	 *
	 * @return the formatted result
	 *
	 * @see #format(String, Object...)
	 * 
	 * @since 15
	 */
	public String formatted(Object... args) {
		return String.format(this, args);
	}

	/**
	 * Removes the minimum indentation from the beginning of each line and
	 * removes the trailing spaces in every line from the string
	 *
	 * @return this string with incidental whitespaces removed from every line
	 * 
	 * @since 15 
	 */
	public String stripIndent() {
		if (isEmpty()) {
			return this;
		}

		char lastChar = charAt(length() - 1);
		boolean trailingNewLine = ('\n' == lastChar) || ('\r' == lastChar);

		Iterator<String> iter = lines().iterator();
		int min = Integer.MAX_VALUE;

		if (trailingNewLine) {
			min = 0;
		} else {
			while (iter.hasNext()) {
				String line = iter.next();

				/* The minimum indentation is calculated based on the number of leading
				 * whitespace characters from all non-blank lines and the last line even
				 * if it is blank.
				 */
				if (!line.isBlank() || !iter.hasNext()) {
					int count = 0;
					int limit = Math.min(min, line.length());
					while ((count < limit) && Character.isWhitespace(line.charAt(count))) {
						count++;
					}

					if (min > count) {
						min = count;
					}
				}
			}
			/* reset iterator to beginning of the string */
			iter = lines().iterator();
		}

		StringBuilder builder = new StringBuilder();

		while (iter.hasNext()) {
			String line = iter.next();

			if (line.isBlank()) {
				builder.append("");
			} else {
				line = line.substring(min);
				builder.append(line.stripTrailing());
			}
			builder.append("\n");
		}


		if (!trailingNewLine) {
			builder.setLength(builder.length() - 1);
		}

		return builder.toString();
	}

	/**
	 * Translate the escape sequences in this string as if in a string literal
	 *
	 * @return result string after translation
	 *
	 * @throws IllegalArgumentException
	 *          If invalid escape sequence is detected
	 * 
	 * @since 15
	 */
	public String translateEscapes() {
		StringBuilder builder = new StringBuilder();
		char[] charArray = toCharArray();
		int index = 0;
		int strLength = length();
		while (index < strLength) {
			if ('\\' == charArray[index]) {
				index++;
				if (index >= strLength) {
					/*[MSG "K0D00", "Invalid escape sequence detected: {0}"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0D00", "\\")); //$NON-NLS-1$ //$NON-NLS-2$
				}
				int octal = 0;
				switch (charArray[index]) {
				case 'b':
					builder.append('\b');
					break;
				case 't':
					builder.append('\t');
					break;
				case 'n':
					builder.append('\n');
					break;
				case 'f':
					builder.append('\f');
					break;
				case 'r':
					builder.append('\r');
					break;
				case 's':
					builder.append(' '); /* '\s' is a new escape sequence for space (U+0020) added in JEP 368: Text Blocks (Second Preview) */
					break;
				case '\"':
				case '\'':
				case '\\':
					builder.append(charArray[index]);
					break;
				case '0':
				case '1':
				case '2':
				case '3':
					octal = charArray[index] - '0';
					/* If the octal escape sequence only has a single digit, then translate the escape and search for next escape sequence 
					 * If there is more than one digit, fall though to case 4-7 and save the current digit as the first digit of the sequence
					 */
					if ((index < strLength - 1) && ('0' <= charArray[index + 1]) && ('7' >= charArray[index + 1])) {
						index++;
					} else {
						builder.append((char)octal);
						break;
					}
					//$FALL-THROUGH$
				case '4':
				case '5':
				case '6':
				case '7':
					/* Shift octal value (either 0 or value fall through from the previous case) left one digit then add the current digit */
					octal = (octal * 010) + (charArray[index] - '0');

					/* Check for last possible digit in octal escape sequence */
					if ((index < strLength - 1) && ('0' <= charArray[index + 1]) && ('7' >= charArray[index + 1])) {
						index++;
						octal = (octal * 010) + (charArray[index] - '0');
					}
					builder.append((char)octal);
					break;
				/**
				 * JEP 368: Text Blocks (Second Preview)
				 * '\r', "\r\n" and '\n' are ignored as per new continuation \<line-terminator> escape sequence 
				 * i.e. ignore line terminator and continue line
				 * */
				case '\r':
					/* Check if the next character is the newline character, i.e. case "\r\n" */
					if (((index + 1) < strLength) && ('\n' == charArray[index + 1])) {
						index++;
					}
					break;
				case '\n':
					break;
				default:
					/*[MSG "K0D00", "Invalid escape sequence detected: {0}"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0D00", "\\" + charArray[index])); //$NON-NLS-1$ //$NON-NLS-2$
				}
			} else {
				builder.append(charArray[index]);
			}
			index++;
		}
		return builder.toString();
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 13 */
}
