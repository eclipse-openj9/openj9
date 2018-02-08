/*[INCLUDE-IF Sidecar16]*/

package java.lang;

/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

import java.io.Serializable;
import java.util.Arrays;
import java.util.Properties;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.InvalidObjectException;

/*[IF Sidecar19-SE]*/
import java.util.stream.IntStream;
/*[ENDIF]*/

/**
 * StringBuilder is not thread safe. For a synchronized implementation, use
 * StringBuffer.
 *  
 * StringBuilder is a variable size contiguous indexable array of characters.
 * The length of the StringBuilder is the number of characters it contains.
 * The capacity of the StringBuilder is the number of characters it can hold.
 * <p>
 * Characters may be inserted at any position up to the length of the
 * StringBuilder, increasing the length of the StringBuilder. Characters at any
 * position in the StringBuilder may be replaced, which does not affect the
 * StringBuilder length.
 * <p>
 * The capacity of a StringBuilder may be specified when the StringBuilder is
 * created. If the capacity of the StringBuilder is exceeded, the capacity
 * is increased.
 *
 * @author		OTI
 * @version		initial
 *
 * @see			StringBuffer
 * 
 * @since 1.5
 */
 
public final class StringBuilder extends AbstractStringBuilder implements Serializable, CharSequence, Appendable {
	private static final long serialVersionUID = 4383685877147921099L;
	
	private static final int INITIAL_SIZE = 16;
	
	/*[IF Sidecar19-SE]*/
	/**
	 * The maximum capacity of a StringBuilder.
	 */
	private static final int MAX_CAPACITY = Integer.MAX_VALUE / 2;
	/*[ENDIF]*/
	
	private static boolean TOSTRING_COPY_BUFFER_ENABLED = false;
	
	// Used to access compression related helper methods
	private static final com.ibm.jit.JITHelpers helpers = com.ibm.jit.JITHelpers.getHelpers();
	
	// Represents the bit in count field to test for whether this StringBuilder backing array is not compressed
	// under String compression mode. This bit is not used when String compression is disabled.
	private static final int uncompressedBit = 0x80000000;
	
	/*[IF !Sidecar19-SE]*/
	// Represents the bit in capacity field to test for whether this StringBuilder backing array is shared.
	private static final int sharedBit = 0x80000000;
	/*[ENDIF]*/
	
	private transient int count;
	/*[IF Sidecar19-SE]*/
	private transient byte[] value;
	private transient boolean shared;
	/*[ELSE]*/
	private transient char[] value;
	private transient int capacity;
	/*[ENDIF]*/
	
	private void decompress(int min) {
		int currentLength = lengthInternal();
		int currentCapacity = capacityInternal();
		
		/*[IF Sidecar19-SE]*/
		byte[] newValue = null;
		/*[ELSE]*/
		char[] newValue = null;
		/*[ENDIF]*/
		
		if (min > currentCapacity) {
			int twice = (currentCapacity << 1) + 2;
			
			/*[IF Sidecar19-SE]*/
			newValue = new byte[(min > twice ? min : twice) * 2];
			/*[ELSE]*/
			newValue = new char[min > twice ? min : twice];
			/*[ENDIF]*/
		} else {
			/*[IF Sidecar19-SE]*/
			newValue = new byte[currentCapacity * 2];
			/*[ELSE]*/
			newValue = new char[currentCapacity];
			/*[ENDIF]*/
		}

		String.decompress(value, 0, newValue, 0, currentLength);
		
		count = count | uncompressedBit;
		value = newValue;
		/*[IF !Sidecar19-SE]*/
		capacity = newValue.length;
		/*[ENDIF]*/
		
		String.initCompressionFlag();
	}
	
/**
 * Constructs a new StringBuffer using the default capacity.
 */
public StringBuilder() {
	this(INITIAL_SIZE);
}

/**
 * Constructs a new StringBuilder using the specified capacity.
 *
 * @param		capacity	the initial capacity
 */
public StringBuilder(int capacity) {
	/* capacity argument is used to determine the byte/char array size. If
	 * capacity argument is Integer.MIN_VALUE (-2147483648), then capacity *= 2
	 * will yield a non-negative number due to overflow. We will fail to throw
	 * NegativeArraySizeException. The check below will assure that
	 * NegativeArraySizeException is thrown if capacity argument is less than 0.
	 */
	if (capacity < 0) {
		throw new NegativeArraySizeException();
	}
	int arraySize = capacity;
	
	/*[IF Sidecar19-SE]*/
	if (capacity <= MAX_CAPACITY) {
		if (!String.enableCompression) {
			arraySize = capacity * 2;
		}
		value = new byte[arraySize];
	} else {
		/*[MSG "K05df", "Unable to allocate an array of the specified capacity. The maximum supported capacity is Integer.MAX_VALUE / 2."]*/
		throw new OutOfMemoryError(com.ibm.oti.util.Msg.getString("K05df")); //$NON-NLS-1$
	}
	/*[ELSE]*/
	if (String.enableCompression) {
		if (capacity == Integer.MAX_VALUE) {
			arraySize = (capacity / 2) + 1;
		} else {
			arraySize = (capacity + 1) / 2;
		}
	}
	value = new char[arraySize];
	/*[ENDIF]*/
	
	/*[IF !Sidecar19-SE]*/
	this.capacity = capacity;
	/*[ENDIF]*/
}

/**
 * Constructs a new StringBuilder containing the characters in
 * the specified string and the default capacity.
 *
 * @param		string	the initial contents of this StringBuilder
 * @exception	NullPointerException when string is null
 */
public StringBuilder (String string) {
	int stringLength = string.lengthInternal();
	
	int newLength = stringLength + INITIAL_SIZE;
	
	if (String.enableCompression) {
		if (string.isCompressed ()) {
			/*[IF Sidecar19-SE]*/
			value = new byte[newLength];
			/*[ELSE]*/
			value = new char[(newLength + 1) / 2];
			/*[ENDIF]*/

			/*[IF Sidecar19-SE]*/
			/*[ELSE]*/
			capacity = newLength;
			/*[ENDIF]*/
			
			string.getBytes(0, stringLength, value, 0);
			
			count = stringLength;
		} else {
			/*[IF Sidecar19-SE]*/
			value = new byte[newLength * 2];
			/*[ELSE]*/
			value = new char[newLength];
			/*[ENDIF]*/
			
			string.getCharsNoBoundChecks(0, stringLength, value, 0);

			/*[IF Sidecar19-SE]*/
			/*[ELSE]*/
			capacity = newLength;
			/*[ENDIF]*/
			
			count = stringLength | uncompressedBit;
			
			String.initCompressionFlag();
		}
	} else {
		/*[IF Sidecar19-SE]*/
		value = new byte[newLength * 2];
		/*[ELSE]*/
		value = new char[newLength];
		/*[ENDIF]*/
		
		string.getCharsNoBoundChecks(0, stringLength, value, 0);

		/*[IF !Sidecar19-SE]*/
		capacity = newLength;
		/*[ENDIF]*/
		
		count = stringLength;
	}
}

/**
 * Adds the character array to the end of this StringBuilder.
 *
 * @param		chars	the character array
 * @return		this StringBuilder
 *
 * @exception	NullPointerException when chars is null
 */
public StringBuilder append (char[] chars) {
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();
	
	int newLength = currentLength + chars.length;
	
	if (String.enableCompression) {
		// Check if the StringBuilder is compressed
		if (count >= 0 && String.compressible(chars, 0, chars.length)) {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			String.compress(chars, 0, value, currentLength, chars.length);
			
			count = newLength;
		} else {
			// Check if the StringBuilder is compressed
			if (count >= 0) {
				decompress(newLength);
			}
			
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			String.decompressedArrayCopy(chars, 0, value, currentLength, chars.length);
			
			count = newLength | uncompressedBit;
		}
	} else {
		if (newLength > currentCapacity) {
			ensureCapacityImpl(newLength);
		}
		
		String.decompressedArrayCopy(chars, 0, value, currentLength, chars.length);
		
		count = newLength;
	}
	
	return this;
}

/**
 * Adds the specified sequence of characters to the end of
 * this StringBuilder.
 *
 * @param		chars	a character array
 * @param		start	the starting offset
 * @param		length	the number of characters
 * @return		this StringBuilder
 *
 * @exception	IndexOutOfBoundsException when <code>length < 0, start < 0</code> or
 *				<code>start + length > chars.length</code>
 * @exception	NullPointerException when chars is null
 */
public StringBuilder append (char chars[], int start, int length) {
	if (start >= 0 && 0 <= length && length <= chars.length - start) {
		int currentLength = lengthInternal();
		int currentCapacity = capacityInternal();
		
		int newLength = currentLength + length;
		
		if (String.enableCompression) {
			// Check if the StringBuilder is compressed
			if (count >= 0 && String.compressible(chars, start, length)) {
				if (newLength > currentCapacity) {
					ensureCapacityImpl(newLength);
				}
				
				String.compress(chars, start, value, currentLength, length);
				
				count = newLength;
			} else {
				// Check if the StringBuilder is compressed
				if (count >= 0) {
					decompress(newLength);					
				}
				
				if (newLength > currentCapacity) {
					ensureCapacityImpl(newLength);
				}
				
				String.decompressedArrayCopy(chars, start, value, currentLength, length);
				
				count = newLength | uncompressedBit;
			}
		} else {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			String.decompressedArrayCopy(chars, start, value, currentLength, length);
			
			count = newLength;
		}
		
		return this;
	} else {
		throw new StringIndexOutOfBoundsException();
	}
}

/*[IF Sidecar19-SE]*/
StringBuilder append (byte[] chars, int start, int length, boolean compressed) {
/*[ELSE]*/
StringBuilder append (char[] chars, int start, int length, boolean compressed) {
/*[ENDIF]*/
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();
	
	int newLength = currentLength + length;
	
	if (String.enableCompression) {
		// Check if the StringBuilder is compressed
		if (count >= 0 && compressed) {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			String.compressedArrayCopy(chars, start, value, currentLength, length);
			
			count = newLength;
		} else {
			// Check if the StringBuilder is compressed
			if (count >= 0) {
				decompress(newLength);				
			}
			
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			if (compressed) {
				String.decompress(chars, start, value, currentLength, length);
			} else {
				String.decompressedArrayCopy(chars, start, value, currentLength, length);
			}
			
			count = newLength | uncompressedBit;
		}
	} else {
		if (newLength > currentCapacity) {
			ensureCapacityImpl(newLength);
		}
		
		if (compressed) {
			String.decompress(chars, start, value, currentLength, length);
		} else {
			String.decompressedArrayCopy(chars, start, value, currentLength, length);
		}
		
		count = newLength;
	}
	
	return this;
}

/**
 * Adds the specified character to the end of
 * this StringBuilder.
 *
 * @param		ch	a character
 * @return		this StringBuilder
 */
public StringBuilder append(char ch) {
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();
	
	int newLength = currentLength + 1;
	
	if (String.enableCompression) {
		// Check if the StringBuilder is compressed
		if (count >= 0 && ch <= 255) {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			helpers.putByteInArrayByIndex(value, currentLength, (byte) ch);
			
			count = newLength;
		} else {
			// Check if the StringBuilder is compressed
			if (count >= 0) {
				decompress(newLength);
			}
			
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			/*[IF Sidecar19-SE]*/
			helpers.putCharInArrayByIndex(value, currentLength, (char) ch);
			/*[ELSE]*/
			value[currentLength] = ch;
			/*[ENDIF]*/
			
			count = newLength | uncompressedBit;
		}
	} else {
		if (newLength > currentCapacity) {
			ensureCapacityImpl(newLength);
		}
		
		/*[IF Sidecar19-SE]*/
		helpers.putCharInArrayByIndex(value, currentLength, (char) ch);
		/*[ELSE]*/
		value[currentLength] = ch;
		/*[ENDIF]*/
		
		count = newLength;
	}
	
	return this;
}

/**
 * Adds the string representation of the specified double to the
 * end of this StringBuilder.
 *
 * @param		value	the double
 * @return		this StringBuilder
 */
public StringBuilder append (double value) {
	return append (String.valueOf (value));
}

/**
 * Adds the string representation of the specified float to the
 * end of this StringBuilder.
 *
 * @param		value	the float
 * @return		this StringBuilder
 */
public StringBuilder append (float value) {
	return append (String.valueOf (value));
}

/**
 * Adds the string representation of the specified integer to the
 * end of this StringBuilder.
 *
 * @param		value	the integer
 * @return		this StringBuilder
 */
public StringBuilder append(int value) {
	/*[PR JAZZ103 69835] This implementation is optimized for the JIT */
	/*[IF Sidecar19-SE]*/
	/*[IF Sidecar19-SE-B174]*/
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();

	int valueLength = Integer.stringSize(value);

	int newLength = currentLength + valueLength;

	if (newLength > currentCapacity) {
		ensureCapacityImpl(newLength);
	}

	if (String.enableCompression) {
		if (count >= 0) {
			Integer.getChars(value, newLength, this.value);
			count = newLength;
		} else {
			StringUTF16.getChars(value, newLength, this.value);
			count = newLength | uncompressedBit;
		}
	} else {
		StringUTF16.getChars(value, newLength, this.value);
		count = newLength;
	}

	return this;
	/*[ELSE]*/
	return append(Integer.toString(value));
	/*[ENDIF]*/
	/*[ELSE]*/
	if (value != Integer.MIN_VALUE) {
		if (String.enableCompression && count >= 0) {
			return append(Integer.toString(value));
		} else {
			int currentLength = lengthInternal();
			int currentCapacity = capacityInternal();

			int valueLength;
			if (value < 0) {
				/* stringSize can't handle negative numbers in Java 8 */
				valueLength = Integer.stringSize(-value) + 1;
			} else {
				valueLength = Integer.stringSize(value);
			}

			int newLength = currentLength + valueLength;

			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}

			Integer.getChars(value, newLength, this.value);

			if (String.enableCompression) {
				count = newLength | uncompressedBit;
			} else {
				count = newLength;
			}

			return this;
		}
	} else {
		// Append Integer.MIN_VALUE as a String
		return append("-2147483648"); //$NON-NLS-1$
	}
	/*[ENDIF]*/
}

/**
 * Adds the string representation of the specified long to the
 * end of this StringBuilder.
 *
 * @param		value	the long
 * @return		this StringBuilder
 */
public StringBuilder append(long value) {
	/*[PR JAZZ103 69835] This implementation is optimized for the JIT */
	/*[IF Sidecar19-SE]*/
	/*[IF Sidecar19-SE-B174]*/
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();

	int valueLength = Long.stringSize(value);

	int newLength = currentLength + valueLength;

	if (newLength > currentCapacity) {
		ensureCapacityImpl(newLength);
	}

	if (String.enableCompression) {
		if (count >= 0) {
			Long.getChars(value, newLength, this.value);
			count = newLength;
		} else {
			StringUTF16.getChars(value, newLength, this.value);
			count = newLength | uncompressedBit;
		}
	} else {
		StringUTF16.getChars(value, newLength, this.value);
		count = newLength;
	}

	return this;
	/*[ELSE]*/
	return append(Long.toString(value));
	/*[ENDIF]*/
	/*[ELSE]*/
	if (value != Long.MIN_VALUE) {
		if (String.enableCompression && count >= 0) {
			return append(Long.toString(value));
		} else {
			int currentLength = lengthInternal();
			int currentCapacity = capacityInternal();

			int valueLength;
			if (value < 0) {
				/* stringSize can't handle negative numbers in Java 8 */
				valueLength = Long.stringSize(-value) + 1;
			} else {
				valueLength = Long.stringSize(value);
			}

			int newLength = currentLength + valueLength;

			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}

			Long.getChars(value, newLength, this.value);

			if (String.enableCompression) {
				count = newLength | uncompressedBit;
			} else {
				count = newLength;
			}

			return this;
		}
	} else {
		// Append Long.MIN_VALUE as a String
		return append("-9223372036854775808"); //$NON-NLS-1$
	}
	/*[ENDIF]*/
}

/**
 * Adds the string representation of the specified object to the
 * end of this StringBuilder.
 *
 * @param		value	the object
 * @return		this StringBuilder
 */
public StringBuilder append (Object value) {
	return append (String.valueOf (value));
}

/**
 * Adds the specified string to the end of this StringBuilder.
 *
 * @param		string	the string
 * @return		this StringBuilder
 */
public StringBuilder append (String string) {
	if (string == null) {
		string = "null"; //$NON-NLS-1$
	}
	
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();
	
	int stringLength = string.lengthInternal();
	
	int newLength = currentLength + stringLength;
	
	if (String.enableCompression) {
		// Check if the StringBuilder is compressed
		if (count >= 0 && string.isCompressed ()) {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			string.getBytes(0, stringLength, value, currentLength);
			
			count = newLength;
		} else {
			// Check if the StringBuilder is compressed
			if (count >= 0) {
				decompress(newLength);
			}
			
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			string.getCharsNoBoundChecks(0, stringLength, value, currentLength);
			
			count = newLength | uncompressedBit;
		}
	} else {
		if (newLength > currentCapacity) {
			ensureCapacityImpl(newLength);
		}
		
		string.getCharsNoBoundChecks(0, stringLength, value, currentLength);
		
		count = newLength;
	}
	
	return this;
}

/**
 * Adds the string representation of the specified boolean to the
 * end of this StringBuilder.
 *
 * @param		value	the boolean
 * @return		this StringBuilder
 */
public StringBuilder append (boolean value) {
	return append (String.valueOf (value));
}

/**
 * Answers the number of characters this StringBuilder can hold without
 * growing.
 *
 * @return		the capacity of this StringBuilder
 *
 * @see			#ensureCapacity
 * @see			#length
 */
public int capacity() {
	return capacityInternal();
}

/**
 * Answers the number of characters this StringBuilder can hold without growing. This method is to be used internally
 * within the current package whenever possible as the JIT compiler will take special precaution to avoid generating
 * HCR guards for calls to this method.
 *
 * @return the capacity of this StringBuilder
 *
 * @see	#ensureCapacity
 * @see	#length
 */
int capacityInternal() {
	/*[IF Sidecar19-SE]*/
	if (String.enableCompression && count >= 0) {
		return value.length;
	} else {
		return value.length / 2;
	}
	/*[ELSE]*/
	return capacity & ~sharedBit;
	/*[ENDIF]*/
}

/**
 * Answers the character at the specified offset in this StringBuilder.
 *
 * @param 		index	the zero-based index in this StringBuilder
 * @return		the character at the index
 *
 * @exception	IndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index >= length()</code>
 */
public char charAt(int index) {
	int currentLength = lengthInternal();
	
	if (index >= 0 && index < currentLength) {
		// Check if the StringBuilder is compressed
		if (String.enableCompression && count >= 0) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		} else {
			/*[IF Sidecar19-SE]*/
			return helpers.getCharFromArrayByIndex(value, index);
			/*[ELSE]*/
			return value[index];
			/*[ENDIF]*/
		}
	}
	
	throw new StringIndexOutOfBoundsException(index);
}

/**
 * Deletes a range of characters.
 *
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>start < 0, start > end</code> or
 *				<code>end > length()</code>
 */
public StringBuilder delete(int start, int end) {
	int currentLength = lengthInternal();
	
	if (start >= 0) {
		if (end > currentLength) {
			end = currentLength;
		}
		
		if (end > start) {
			int numberOfTailChars = currentLength - end;

			/*[PR CMVC 104709] Optimize String sharing for more performance */
			try {
				// Check if the StringBuilder is not shared
				/*[IF Sidecar19-SE]*/
				if (!shared) {
				/*[ELSE]*/
				if (capacity >= 0) {
				/*[ENDIF]*/
					if (numberOfTailChars > 0) {
						// Check if the StringBuilder is compressed
						if (String.enableCompression && count >= 0) {
							String.compressedArrayCopy(value, end, value, start, numberOfTailChars);
						} else {
							String.decompressedArrayCopy(value, end, value, start, numberOfTailChars);
						}
					}
				} else {
					/*[IF Sidecar19-SE]*/
					byte[] newData = new byte[value.length];
					/*[ELSE]*/
					char[] newData = new char[value.length];
					/*[ENDIF]*/
					
					// Check if the StringBuilder is compressed
					if (String.enableCompression && count >= 0) {
						if (start > 0) {
							String.compressedArrayCopy(value, 0, newData, 0, start);
						}
						
						if (numberOfTailChars > 0) {
							String.compressedArrayCopy(value, end, newData, start, numberOfTailChars);
						}
					} else {
						if (start > 0) {
							String.decompressedArrayCopy(value, 0, newData, 0, start);
						}
						
						if (numberOfTailChars > 0) {
							String.decompressedArrayCopy(value, end, newData, start, numberOfTailChars);
						}
					}
					
					value = newData;
					
					/*[IF Sidecar19-SE]*/
					shared = false;
					/*[ELSE]*/
					capacity = capacity & ~sharedBit;
					/*[ENDIF]*/
				}
			} catch (IndexOutOfBoundsException e) {
				throw new StringIndexOutOfBoundsException();
			}
			
			if (String.enableCompression) {
				// Check if the StringBuilder is compressed
				if (count >= 0) {
					count = currentLength - (end - start);
				} else {
					count = (currentLength - (end - start)) | uncompressedBit;
					
					String.initCompressionFlag();
				}
			} else {
				count = currentLength - (end - start);
			}
			
			return this;
		}
		
		if (start == end) {
			return this;
		}
	}
	
	throw new StringIndexOutOfBoundsException();
}

/**
 * Deletes a single character
 *
 * @param		location	the offset of the character to delete
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>location < 0</code> or
 *				<code>location >= length()</code>
 */
public StringBuilder deleteCharAt(int location) {
	int currentLength = lengthInternal();
	
	if (currentLength != 0) {
		return delete (location, location + 1);
	} else {
		throw new StringIndexOutOfBoundsException ();
	}
}

/**
 * Ensures that this StringBuilder can hold the specified number of characters
 * without growing.
 *
 * @param		min	 the minimum number of elements that this
 *				StringBuilder will hold before growing
 */
public void ensureCapacity(int min) {
	int currentCapacity = capacityInternal();
	
	if (min > currentCapacity) {
		ensureCapacityImpl(min);
	}
}

private void ensureCapacityImpl(int min) {
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();
	
	int newCapacity = (currentCapacity << 1) + 2;

	int newLength = min > newCapacity ? min : newCapacity;
	
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		/*[IF Sidecar19-SE]*/
		byte[] newData = new byte[newLength];
		/*[ELSE]*/
		char[] newData = new char[(newLength + 1) / 2];
		/*[ENDIF]*/
		
		String.compressedArrayCopy(value, 0, newData, 0, currentLength);
		
		value = newData;
		
	} else {
		/*[IF Sidecar19-SE]*/
		byte[] newData = new byte[newLength * 2];
		/*[ELSE]*/
		char[] newData = new char[newLength];
		/*[ENDIF]*/
		
		String.decompressedArrayCopy(value, 0, newData, 0, currentLength);
		value = newData;
	}
	
	/*[IF !Sidecar19-SE]*/
	capacity = newLength;
	/*[ENDIF]*/
}

/**
 * Copies the specified characters in this StringBuilder to the character array
 * starting at the specified offset in the character array.
 *
 * @param		start	the starting offset of characters to copy
 * @param		end	the ending offset of characters to copy
 * @param		buffer	the destination character array
 * @param		index	the starting offset in the character array
 *
 * @exception	IndexOutOfBoundsException when <code>start < 0, end > length(),
 *				start > end, index < 0, end - start > buffer.length - index</code>
 * @exception	NullPointerException when buffer is null
 */
public void getChars(int start, int end, char[] buffer, int index) {
	try {
		int currentLength = lengthInternal();
		
		if (start <= currentLength && end <= currentLength) {
			// Note that we must explicitly check all the conditions because we are not using System.arraycopy which would
			// have implicitly checked these conditions
			if (start >= 0 && start <= end && index >= 0 && end - start <= buffer.length - index) {
				// Check if the StringBuilder is compressed
				if (String.enableCompression && count >= 0) {
					String.decompress(value, start, buffer, index, end - start);
					
					return;
				} else {
					/*[IF Sidecar19-SE]*/
					String.decompressedArrayCopy(value, start, buffer, index, end - start);
					/*[ELSE]*/
					System.arraycopy(value, start, buffer, index, end - start);
					/*[ENDIF]*/
				
					return;
				}
			}
		}
	/*[IF]*/
	// TODO : Is this IOOB ever possible given the above check?
	/*[ENDIF]*/
	} catch(IndexOutOfBoundsException e) {
		// Void
	}
	
	throw new StringIndexOutOfBoundsException ();
}

/**
 * Inserts the character array at the specified offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		chars	the character array to insert
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 * @exception	NullPointerException when chars is null
 */
public StringBuilder insert(int index, char[] chars) {
	int currentLength = lengthInternal();
	
	/*[PR 101359] insert(-1, (char[])null) should throw StringIndexOutOfBoundsException */
	if (0 <= index && index <= currentLength) {
		move(chars.length, index);
		
		if (String.enableCompression) {
			// Check if the StringBuilder is compressed
			if (count >= 0 && String.compressible(chars, 0, chars.length)) {
				String.compress(chars, 0, value, index, chars.length);
				
				count = currentLength + chars.length;
				
				return this;
			} else {
				count = currentLength + chars.length;

				// Check if the StringBuilder is compressed
				if (count >= 0) {
					decompress(value.length);
				}
				
				String.decompressedArrayCopy(chars, 0, value, index, chars.length);
				
				count = count | uncompressedBit;
				
				return this;
			}
		} else {
			String.decompressedArrayCopy(chars, 0, value, index, chars.length);
			
			count = currentLength + chars.length;
			
			return this;
		}
	} else {
		throw new StringIndexOutOfBoundsException(index);
	}
}


/**
 * Inserts the specified sequence of characters at the
 * specified offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		chars	a character array
 * @param		start	the starting offset
 * @param		length	the number of characters
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>length < 0, start < 0,</code>
 *				<code>start + length > chars.length, index < 0</code> or
 *				<code>index > length()</code>
 * @exception	NullPointerException when chars is null
 */
public StringBuilder insert(int index, char[] chars, int start, int length) {
	int currentLength = lengthInternal();
	
	/*[PR 101359] insert(-1, (char[])null, x, x) should throw StringIndexOutOfBoundsException */
	if (0 <= index && index <= currentLength) {
		if (start >= 0 && 0 <= length && length <= chars.length - start) {
			move(length, index);
			
			if (String.enableCompression) {
				// Check if the StringBuilder is compressed
				if (count >= 0 && String.compressible(chars, start, length)) {
					String.compress(chars, start, value, index, length);
					
					count = currentLength + length;
					
					return this;
				} else {
					count = currentLength + length;

					// Check if the StringBuilder is compressed
					if (count >= 0) {
						decompress(value.length);
					}
					
					String.decompressedArrayCopy(chars, start, value, index, length);
					
					count = count | uncompressedBit;
					
					return this;
				}
			} else {
				String.decompressedArrayCopy(chars, start, value, index, length);
				
				count = currentLength + length;
				
				return this;
			}
		} else {
			throw new StringIndexOutOfBoundsException();
		}
	} else {
		throw new StringIndexOutOfBoundsException(index);
	}
}

/*[IF Sidecar19-SE]*/
StringBuilder insert(int index, byte[] chars, int start, int length, boolean compressed) {
/*[ELSE]*/
StringBuilder insert(int index, char[] chars, int start, int length, boolean compressed) {
/*[ENDIF]*/
	int currentLength = lengthInternal();
	
	move(length, index);
	
	if (String.enableCompression) {
		// Check if the StringBuilder is compressed
		if (count >= 0 && compressed) {
			String.compressedArrayCopy(chars, start, value, index, length);
			
			count = currentLength + length;
			
			return this;
		} else {
			count = currentLength + length;

			if (count >= 0) {
				decompress(value.length);
			}
			
			String.decompressedArrayCopy(chars, start, value, index, length);
			
			count = count | uncompressedBit;
			
			return this;
		}
	} else {
		String.decompressedArrayCopy(chars, start, value, index, length);
		
		count = currentLength + length;
		
		return this;
	}
}

/**
 * Inserts the character at the specified offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		ch	the character to insert
 * @return		this StringBuilder
 *
 * @exception	IndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 */
public StringBuilder insert(int index, char ch) {
	int currentLength = lengthInternal();
	
	if (0 <= index && index <= currentLength) {
		move(1, index);
		
		if (String.enableCompression ) {
			// Check if the StringBuilder is compressed
			if (count >= 0 && ch <= 255) {
				helpers.putByteInArrayByIndex(value, index, (byte) ch);
				
				count = currentLength + 1;
				
				return this;
			} else {
				count = currentLength + 1;

				// Check if the StringBuilder is compressed
				if (count >= 0) {
					decompress(value.length);
				}
				
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, index, (char) ch);
				/*[ELSE]*/
				value[index] = ch;
				/*[ENDIF]*/
				
				count = count | uncompressedBit;
				
				return this;
			}
		} else {
			/*[IF Sidecar19-SE]*/
			helpers.putCharInArrayByIndex(value, index, (char) ch);
			/*[ELSE]*/
			value[index] = ch;
			/*[ENDIF]*/
			
			count = currentLength + 1;
			
			return this;
		}
	} else {
		throw new StringIndexOutOfBoundsException(index);
	}
}

/**
 * Inserts the string representation of the specified double at the specified
 * offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		value	the double to insert
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 */
public StringBuilder insert(int index, double value) {
	return insert(index, String.valueOf(value));
}

/**
 * Inserts the string representation of the specified float at the specified
 * offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		value	the float to insert
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 */
public StringBuilder insert(int index, float value) {
	return insert(index, String.valueOf(value));
}

/**
 * Inserts the string representation of the specified integer at the specified
 * offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		value	the integer to insert
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 */
public StringBuilder insert(int index, int value) {
	return insert(index, Integer.toString(value));
}

/**
 * Inserts the string representation of the specified long at the specified
 * offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		value	the long to insert
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 */
public StringBuilder insert(int index, long value) {
	return insert(index, Long.toString(value));
}

/**
 * Inserts the string representation of the specified object at the specified
 * offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		value	the object to insert
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 */
public StringBuilder insert(int index, Object value) {
	return insert(index, String.valueOf(value));
}

/**
 * Inserts the string at the specified offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		string	the string to insert
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 */
public StringBuilder insert(int index, String string) {
	int currentLength = lengthInternal();
	
	if (0 <= index && index <= currentLength) {
		if (string == null) {
			string = "null"; //$NON-NLS-1$
		}
		
		int stringLength = string.lengthInternal();
		
		move(stringLength, index);
		
		if (String.enableCompression) {
			// Check if the StringBuilder is compressed
			if (count >= 0 && string.isCompressed()) {
				string.getBytes(0, stringLength, value, index);
				
				count = currentLength + stringLength;
				
				return this;
			} else {
				count = currentLength + stringLength;

				// Check if the StringBuilder is compressed
				if (count >= 0) {
					decompress(value.length);
				}
				
				string.getCharsNoBoundChecks(0, stringLength, value, index);
				
				count = count | uncompressedBit;
				
				return this;
			}
		} else {
			string.getCharsNoBoundChecks(0, stringLength, value, index);
			
			count = currentLength + stringLength;
			
			return this;
		}
	} else {
		throw new StringIndexOutOfBoundsException(index);
	}
}

/**
 * Inserts the string representation of the specified boolean at the specified
 * offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		value	the boolean to insert
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 */
public StringBuilder insert(int index, boolean value) {
	return insert(index, String.valueOf(value));
}

/**
 * Answers the size of this StringBuilder.
 *
 * @return		the number of characters in this StringBuilder
 */
public int length() {
	return lengthInternal();
}

/**
 * Answers the size of this StringBuilder. This method is to be used internally within the current package whenever
 * possible as the JIT compiler will take special precaution to avoid generating HCR guards for calls to this
 * method.
 *
 * @return the number of characters in this StringBuilder
 */
int lengthInternal() {
	if (String.enableCompression) {
		// Check if the StringBuilder is compressed
		if (count >= 0) {
			return count;
		} else {
			return count & ~uncompressedBit;
		}
	} else {
		return count;
	}
}

private void move(int size, int index) {
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();
	
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		int newLength;
		
		if (currentCapacity - currentLength >= size) {
			// Check if the StringBuilder is not shared
			/*[IF Sidecar19-SE]*/
			if (!shared) {
			/*[ELSE]*/
			if (capacity >= 0) {
			/*[ENDIF]*/
				String.compressedArrayCopy(value, index, value, index + size, currentLength - index);
				
				return;
			}
			
			newLength = currentCapacity;
		} else {
			newLength = Integer.max(currentLength + size, (currentCapacity << 1) + 2);
		}
		
		/*[IF Sidecar19-SE]*/
		byte[] newData = new byte[newLength];
		/*[ELSE]*/
		char[] newData = new char[(newLength + 1) / 2];
		/*[ENDIF]*/
		
		String.compressedArrayCopy(value, 0, newData, 0, index);
		String.compressedArrayCopy(value, index, newData, index + size, currentLength - index);
		
		value = newData;
		
		/*[IF !Sidecar19-SE]*/
		capacity = newLength;
		/*[ENDIF]*/
	} else {
		int newLength;
		
		if (currentCapacity - currentLength >= size) {
			// Check if the StringBuilder is not shared
			/*[IF Sidecar19-SE]*/
			if (!shared) {
				String.decompressedArrayCopy(value, index, value, index + size, currentLength - index);
			/*[ELSE]*/
			if (capacity >= 0) {
				String.decompressedArrayCopy(value, index, value, index + size, currentLength - index);
			/*[ENDIF]*/
				
				return;
			}
			
			newLength = currentCapacity;
		} else {
			newLength = Integer.max(currentLength + size, (currentCapacity << 1) + 2);
		}
		
		/*[IF Sidecar19-SE]*/
		byte[] newData = new byte[newLength * 2];
		/*[ELSE]*/
		char[] newData = new char[newLength];
		/*[ENDIF]*/

		String.decompressedArrayCopy(value, 0, newData, 0, index);
		String.decompressedArrayCopy(value, index, newData, index + size, currentLength - index);
		
		value = newData;

		/*[IF !Sidecar19-SE]*/
		capacity = newLength;
		/*[ENDIF]*/
	}
}

/**
 * Replace a range of characters with the characters in the specified String.
 *
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @param		string	a String
 * @return		this StringBuilder
 *
 * @exception	StringIndexOutOfBoundsException when <code>start < 0</code> or
 *				<code>start > end</code>
 */
public StringBuilder replace(int start, int end, String string) {
	int currentLength = lengthInternal();
	
	if (String.enableCompression) {
		// Check if the StringBuilder is compressed
		if (count >= 0 && string.isCompressed()) {
			if (start >= 0) {
				if (end > currentLength) {
					end = currentLength;
				}
				
				if (end > start) {
					int size = string.lengthInternal();
					
					// Difference between the substring we wish to replace and the size of the string parameter
					int difference = end - start - size;
					
					if (difference > 0) {
						// Check if the StringBuilder is not shared
						/*[IF Sidecar19-SE]*/
						if (!shared) {
						/*[ELSE]*/
						if (capacity >= 0) {
						/*[ENDIF]*/
							String.compressedArrayCopy(value, end, value, start + size, currentLength - end);
						} else {
							/*[IF Sidecar19-SE]*/
							byte[] newData = new byte[value.length];
							/*[ELSE]*/
							char[] newData = new char[value.length];
							/*[ENDIF]*/
							
							String.compressedArrayCopy(value, 0, newData, 0, start);
							String.compressedArrayCopy(value, end, newData, start + size, currentLength - end);
							
							value = newData;
							
							/*[IF Sidecar19-SE]*/
							shared = false;
							/*[ELSE]*/
							capacity = capacity & ~sharedBit;
							/*[ENDIF]*/
						}
					} else if (difference < 0) {
						move(-difference, end);
					/*[IF Sidecar19-SE]*/
					} else if (shared) {
					/*[ELSE]*/
					} else if (capacity < 0) {
					/*[ENDIF]*/
						value = value.clone();
						
						/*[IF Sidecar19-SE]*/
						shared = false;
						/*[ELSE]*/
						capacity = capacity & ~sharedBit;
						/*[ENDIF]*/
					}
					
					string.getBytes(0, size, value, start);
					
					count = currentLength - difference;
					
					return this;
				}
				
				if (start == end) {
					return insert(start, string);
				}
			}
		} else {
			// Check if the StringBuilder is compressed
			if (count >= 0) {
				decompress(value.length);				
			}
			
			if (start >= 0) {
				if (end > currentLength) {
					end = currentLength;
				}
				
				if (end > start) {
					int size = string.lengthInternal();
					
					// Difference between the substring we wish to replace and the size of the string parameter
					int difference = end - start - size;
					
					if (difference > 0) {
						// Check if the StringBuilder is not shared
						/*[IF Sidecar19-SE]*/
						if (!shared) {
							String.decompressedArrayCopy(value, end, value, start + size, currentLength - end);
						/*[ELSE]*/
						if (capacity >= 0) {
							String.decompressedArrayCopy(value, end, value, start + size, currentLength - end);
						/*[ENDIF]*/
						} else {
							/*[IF Sidecar19-SE]*/
							byte[] newData = new byte[value.length];							
							/*[ELSE]*/
							char[] newData = new char[value.length];
							/*[ENDIF]*/

							String.decompressedArrayCopy(value, 0, newData, 0, start);
							String.decompressedArrayCopy(value, end, newData, start + size, currentLength - end);
							
							value = newData;
							
							/*[IF Sidecar19-SE]*/
							shared = false;
							/*[ELSE]*/
							capacity = capacity & ~sharedBit;
							/*[ENDIF]*/
						}
					} else if (difference < 0) {
						move(-difference, end);
					/*[IF Sidecar19-SE]*/
					} else if (shared) {
					/*[ELSE]*/
					} else if (capacity < 0) {
					/*[ENDIF]*/
						value = value.clone();
						
						/*[IF Sidecar19-SE]*/
						shared = false;
						/*[ELSE]*/
						capacity = capacity & ~sharedBit;
						/*[ENDIF]*/
					}
					
					string.getCharsNoBoundChecks(0, size, value, start);
					
					count = (currentLength - difference) | uncompressedBit;
					
					return this;
				}
				
				if (start == end) {
					string.getClass(); // Implicit null check
					
					return insert(start, string);
				}
			}
		}
	} else {
		if (start >= 0) {
			if (end > currentLength) {
				end = currentLength;
			}
			
			if (end > start) {
				int size = string.lengthInternal();
				
				// Difference between the substring we wish to replace and the size of the string parameter
				int difference = end - start - size;
				
				if (difference > 0) {
					// Check if the StringBuilder is not shared
					/*[IF Sidecar19-SE]*/
					if (!shared) {
						String.decompressedArrayCopy(value, end, value, start + size, currentLength - end);
					/*[ELSE]*/
					if (capacity >= 0) {
						String.decompressedArrayCopy(value, end, value, start + size, currentLength - end);
					/*[ENDIF]*/
					} else {
						/*[IF Sidecar19-SE]*/
						byte[] newData = new byte[value.length];
						/*[ELSE]*/
						char[] newData = new char[value.length];
						/*[ENDIF]*/
												
						String.decompressedArrayCopy(value, 0, newData, 0, start);
						String.decompressedArrayCopy(value, end, newData, start + size, currentLength - end);
						
						value = newData;
						
						/*[IF Sidecar19-SE]*/
						shared = false;
						/*[ELSE]*/
						capacity = capacity & ~sharedBit;
						/*[ENDIF]*/
					}
				} else if (difference < 0) {
					move(-difference, end);
				/*[IF Sidecar19-SE]*/
				} else if (shared) {
				/*[ELSE]*/
				} else if (capacity < 0) {
				/*[ENDIF]*/
					value = value.clone();
					
					/*[IF Sidecar19-SE]*/
					shared = false;
					/*[ELSE]*/
					capacity = capacity & ~sharedBit;
					/*[ENDIF]*/
				}
				
				string.getCharsNoBoundChecks(0, size, value, start);
				
				count = currentLength - difference;
				
				return this;
			}
			
			if (start == end) {
				string.getClass(); // Implicit null check
				
				return insert(start, string);
			}
		}
	}
	
	throw new StringIndexOutOfBoundsException();
}

/**
 * Reverses the order of characters in this StringBuilder.
 *
 * @return		this StringBuilder
 */
public StringBuilder reverse() {
	int currentLength = lengthInternal();
	
	if (currentLength < 2) {
		return this;
	}
	
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		// Check if the StringBuilder is not shared
		/*[IF Sidecar19-SE]*/
		if (!shared) {
		/*[ELSE]*/
		if (capacity >= 0) {
		/*[ENDIF]*/
			for (int i = 0, mid = currentLength / 2, j = currentLength - 1; i < mid; ++i, --j) {
				byte a = helpers.getByteFromArrayByIndex(value, i);
				byte b = helpers.getByteFromArrayByIndex(value, j);
				
				helpers.putByteInArrayByIndex(value, i, b);
				helpers.putByteInArrayByIndex(value, j, a);
			}
			
			return this;
		} else {
			/*[IF Sidecar19-SE]*/
			byte[] newData = new byte[value.length];
			/*[ELSE]*/
			char[] newData = new char[value.length];
			/*[ENDIF]*/
			
			for (int i = 0, j = currentLength - 1; i < currentLength; ++i, --j) {
				helpers.putByteInArrayByIndex(newData, j, helpers.getByteFromArrayByIndex(value, i));
			}
			
			value = newData;
			
			/*[IF Sidecar19-SE]*/
			shared = false;
			/*[ELSE]*/
			capacity = capacity & ~sharedBit;
			/*[ENDIF]*/
			
			return this;
		}
	} else {
		// Check if the StringBuilder is not shared
		/*[IF Sidecar19-SE]*/
		if (!shared) {
		/*[ELSE]*/
		if (capacity >= 0) {
		/*[ENDIF]*/
			int end = currentLength - 1;
			
			/*[IF Sidecar19-SE]*/
			char frontHigh = helpers.getCharFromArrayByIndex(value, 0);
			char endLow = helpers.getCharFromArrayByIndex(value, end);
			/*[ELSE]*/
			char frontHigh = value[0];
			char endLow = value[end];
			/*[ENDIF]*/
			boolean allowFrontSur = true, allowEndSur = true;
			for (int i = 0, mid = currentLength / 2; i < mid; i++, --end) {
				/*[IF Sidecar19-SE]*/
				char frontLow = helpers.getCharFromArrayByIndex(value, i + 1);
				char endHigh = helpers.getCharFromArrayByIndex(value, end - 1);
				/*[ELSE]*/
				char frontLow = value[i + 1];
				char endHigh = value[end - 1];
				/*[ENDIF]*/
				boolean surAtFront = false, surAtEnd = false;
				if (allowFrontSur && frontLow >= Character.MIN_LOW_SURROGATE && frontLow <= Character.MAX_LOW_SURROGATE && frontHigh >= Character.MIN_HIGH_SURROGATE && frontHigh <= Character.MAX_HIGH_SURROGATE) {
					surAtFront = true;
					/*[PR 117344, CMVC 93149] ArrayIndexOutOfBoundsException in StringBuffer.reverse() */
					if (currentLength < 3) return this;
				}
				if (allowEndSur && endHigh >= Character.MIN_HIGH_SURROGATE && endHigh <= Character.MAX_HIGH_SURROGATE && endLow >= Character.MIN_LOW_SURROGATE && endLow <= Character.MAX_LOW_SURROGATE) {
					surAtEnd = true;
				}
				allowFrontSur = true;
				allowEndSur = true;
				if (surAtFront == surAtEnd) {
					if (surAtFront) {
						// both surrogates
						/*[IF Sidecar19-SE]*/
						helpers.putCharInArrayByIndex(value, end, frontLow);
						helpers.putCharInArrayByIndex(value, end - 1, frontHigh);
						helpers.putCharInArrayByIndex(value, i, endHigh);
						helpers.putCharInArrayByIndex(value, i + 1, endLow);
						frontHigh = helpers.getCharFromArrayByIndex(value, i + 2);
						endLow = helpers.getCharFromArrayByIndex(value, end - 2);
						/*[ELSE]*/
						value[end] = frontLow;
						value[end - 1] = frontHigh;
						value[i] = endHigh;
						value[i + 1] = endLow;
						frontHigh = value[i + 2];
						endLow = value[end - 2];
						/*[ENDIF]*/
						i++;
						--end;
					} else {
						// neither surrogates
						/*[IF Sidecar19-SE]*/
						helpers.putCharInArrayByIndex(value, end, frontHigh);
						helpers.putCharInArrayByIndex(value, i, endLow);
						/*[ELSE]*/
						value[end] = frontHigh;
						value[i] = endLow;
						/*[ENDIF]*/
						frontHigh = frontLow;
						endLow = endHigh;
					}
				} else {
					if (surAtFront) {
						// surrogate only at the front
						/*[IF Sidecar19-SE]*/
						helpers.putCharInArrayByIndex(value, end, frontLow);
						helpers.putCharInArrayByIndex(value, i, endLow);
						/*[ELSE]*/
						value[end] = frontLow;
						value[i] = endLow;
						/*[ENDIF]*/
						endLow = endHigh;
						allowFrontSur = false;
					} else {
						// surrogate only at the end
						/*[IF Sidecar19-SE]*/
						helpers.putCharInArrayByIndex(value, end, frontHigh);
						helpers.putCharInArrayByIndex(value, i, endHigh);
						/*[ELSE]*/
						value[end] = frontHigh;
						value[i] = endHigh;
						/*[ENDIF]*/
						frontHigh = frontLow;
						allowEndSur = false;
					}
				}
			}
			if ((currentLength & 1) == 1 && (!allowFrontSur || !allowEndSur)) {
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, end, allowFrontSur ? endLow : frontHigh);
				/*[ELSE]*/
				value[end] = allowFrontSur ? endLow : frontHigh;
				/*[ENDIF]*/
			}
		} else {
			/*[IF Sidecar19-SE]*/
			byte[] newData = new byte[value.length];
			/*[ELSE]*/
			char[] newData = new char[value.length];
			/*[ENDIF]*/
			
			for (int i = 0, end = currentLength; i < currentLength; i++) {
				/*[IF Sidecar19-SE]*/
				char high = helpers.getCharFromArrayByIndex(value, i);
				/*[ELSE]*/
				char high = value[i];
				/*[ENDIF]*/

				if ((i + 1) < currentLength && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
					/*[IF Sidecar19-SE]*/
					char low = helpers.getCharFromArrayByIndex(value, i + 1);
					/*[ELSE]*/
					char low = value[i + 1];
					/*[ENDIF]*/
					
					if (low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
						/*[IF Sidecar19-SE]*/
						helpers.putCharInArrayByIndex(newData, --end, low);
						/*[ELSE]*/
						newData[--end] = low;
						/*[ENDIF]*/
						i++;
					}
				}
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(newData, --end, high);
				/*[ELSE]*/
				newData[--end] = high;
				/*[ENDIF]*/
			}
			
			value = newData;
			
			/*[IF Sidecar19-SE]*/
			shared = false;
			/*[ELSE]*/
			capacity = capacity & ~sharedBit;
			/*[ENDIF]*/
		}
		
		return this;
	}
}

/**
 * Sets the character at the specified offset in this StringBuilder.
 *
 * @param 		index	the zero-based index in this StringBuilder
 * @param		ch	the character
 *
 * @exception	IndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index >= length()</code>
 */
public void setCharAt(int index, char ch) {
	int currentLength = lengthInternal();
	
	if (0 <= index && index < currentLength) {
		if (String.enableCompression) {
			// Check if the StringBuilder is compressed
			if (count >= 0 && ch <= 255) {
				/*[IF Sidecar19-SE]*/
				if (shared) {
				/*[ELSE]*/
				if (capacity < 0) {
				/*[ENDIF]*/
					value = value.clone();
					
					/*[IF Sidecar19-SE]*/
					shared = false;
					/*[ELSE]*/
					capacity = capacity & ~sharedBit;
					/*[ENDIF]*/
				}
				
				helpers.putByteInArrayByIndex(value, index, (byte) ch);
			} else {
				// Check if the StringBuilder is compressed
				if (count >= 0) {
					decompress(value.length);
				}

				/*[IF Sidecar19-SE]*/
				if (shared) {
				/*[ELSE]*/
				if (capacity < 0) {
				/*[ENDIF]*/
					value = value.clone();
					
					/*[IF Sidecar19-SE]*/
					shared = false;
					/*[ELSE]*/
					capacity = capacity & ~sharedBit;
					/*[ENDIF]*/
				}
				
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, index, (char) ch);
				/*[ELSE]*/
				value[index] = ch;
				/*[ENDIF]*/
			}
		} else {
			/*[IF Sidecar19-SE]*/
			if (shared) {
			/*[ELSE]*/
			if (capacity < 0) {
			/*[ENDIF]*/
				value = value.clone();
				
				/*[IF Sidecar19-SE]*/
				shared = false;
				/*[ELSE]*/
				capacity = capacity & ~sharedBit;
				/*[ENDIF]*/
			}

			/*[IF Sidecar19-SE]*/
			helpers.putCharInArrayByIndex(value, index, (char) ch);
			/*[ELSE]*/
			value[index] = ch;
			/*[ENDIF]*/
		}
	} else {
		throw new StringIndexOutOfBoundsException(index);
	}
}

/**
 * Sets the length of this StringBuilder to the specified length. If there
 * are more than length characters in this StringBuilder, the characters
 * at end are lost. If there are less than length characters in the
 * StringBuilder, the additional characters are set to <code>\\u0000</code>.
 *
 * @param		length	the new length of this StringBuilder
 *
 * @exception	IndexOutOfBoundsException when <code>length < 0</code>
 *
 * @see			#length
 */
public void setLength(int length) {
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();
	
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		if (length > currentCapacity) {
			ensureCapacityImpl(length);
		} else if (length > currentLength) {
			/*[IF Sidecar19-SE]*/
			Arrays.fill(value, currentLength, length, (byte) 0);
		} else if (shared) {
		/*[ELSE]*/
			for (int i = currentLength; i < length; ++i) {
				helpers.putByteInArrayByIndex(value, i, (byte) 0);
			}
		} else if (capacity < 0) {
		/*[ENDIF]*/
			if (length < 0) {
				throw new IndexOutOfBoundsException();
			}
			
			/*[IF Sidecar19-SE]*/
			byte[] newData = new byte[value.length];
			/*[ELSE]*/
			char[] newData = new char[value.length];
			/*[ENDIF]*/
			
			if (length > 0) {
				String.compressedArrayCopy(value, 0, newData, 0, length);
			}
			
			value = newData;
			
			/*[IF Sidecar19-SE]*/
			shared = false;
			/*[ELSE]*/
			capacity = capacity & ~sharedBit;
			/*[ENDIF]*/
		} else if (length < 0) {
			throw new IndexOutOfBoundsException();
		}
	} else {
		if (length > currentCapacity) {
			ensureCapacityImpl(length);
		} else if (length > currentLength) {
			/*[PR CMVC 104709] Zero characters when growing */
			/*[IF Sidecar19-SE]*/
			Arrays.fill(value, currentLength * 2, length * 2, (byte) 0);
		} else if (shared) {
			/*[ELSE]*/
			Arrays.fill(value, currentLength, length, (char) 0);
		} else if (capacity < 0) {
			/*[ENDIF]*/
			if (length < 0) {
				throw new IndexOutOfBoundsException();
			}
			
			/*[PR 109954] Do not reduce capacity */
			/*[IF Sidecar19-SE]*/
			byte[] newData = new byte[value.length];
			/*[ELSE]*/
			char[] newData = new char[value.length];
			/*[ENDIF]*/
			
			if (length > 0) {
				String.decompressedArrayCopy(value, 0, newData, 0, length);
			}
			
			value = newData;
			
			/*[IF Sidecar19-SE]*/
			shared = false;
			/*[ELSE]*/
			capacity = capacity & ~sharedBit;
			/*[ENDIF]*/
		} else if (length < 0) {
			throw new IndexOutOfBoundsException();
		}
	}
	
	if (String.enableCompression) {
		// Check if the StringBuilder is compressed
		if (count >= 0) {
			count = length;
		} else {
			count = length | uncompressedBit;
		}
	} else {
		count = length;
	}
}

/**
 * Copies a range of characters into a new String.
 *
 * @param		start	the offset of the first character
 * @return		a new String containing the characters from start to the end
 *				of the string
 *
 * @exception	StringIndexOutOfBoundsException when <code>start < 0</code> or
 *				<code>start > length()</code>
 */
public String substring(int start) {
	int currentLength = lengthInternal();
	
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		if (0 <= start && start <= currentLength) {
			/*[PR CMVC 104709] Remove String sharing for more performance */
			return new String(value, start, currentLength - start, true, false);
		}
	} else {
		if (0 <= start && start <= currentLength) {
			/*[PR CMVC 104709] Remove String sharing for more performance */
			return new String(value, start, currentLength - start, false, false);
		}
	}
	
	throw new StringIndexOutOfBoundsException(start);
}

/**
 * Copies a range of characters into a new String.
 *
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @return		a new String containing the characters from start to end - 1
 *
 * @exception	StringIndexOutOfBoundsException when <code>start < 0, start > end</code> or
 *				<code>end > length()</code>
 */
public String substring(int start, int end) {
	int currentLength = lengthInternal();
	
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		if (0 <= start && start <= end && end <= currentLength) {
			/*[PR CMVC 104709] Remove String sharing for more performance */
			return new String(value, start, end - start, true, false);
		}
	} else {
		if (0 <= start && start <= end && end <= currentLength) {
			/*[PR CMVC 104709] Remove String sharing for more performance */
			return new String(value, start, end - start, false, false);
		}
	}
	
	throw new StringIndexOutOfBoundsException();
}

/*[IF]*/
// TODO : This is no longer applicable because String does not have an offset field. Needs to be removed.
/*[ENDIF]*/
static void initFromSystemProperties(Properties props) {
	String prop = props.getProperty("java.lang.string.create.unique"); //$NON-NLS-1$
	TOSTRING_COPY_BUFFER_ENABLED = "true".equals(prop) || "StringBuilder".equals(prop); //$NON-NLS-1$ //$NON-NLS-2$
	/*[IF]*/
	if (TOSTRING_COPY_BUFFER_ENABLED) {
		com.ibm.oti.vm.VM.dumpString("PMR 67389 - Creating unique String.char[]s from java.lang.StringBuilder.toString(). Disable using -Djava.lang.string.create.unique=false\n"); //$NON-NLS-1$
	}
	/*[ENDIF]*/
}

/**
 * Answers the contents of this StringBuilder.
 *
 * @return		a String containing the characters in this StringBuilder
 */
public String toString () {
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();
	
	if (false == TOSTRING_COPY_BUFFER_ENABLED) {
		/*[PR 96029] Copy char[] when too much memory wasted */
		/*[PR CMVC 104709] Optimize String sharing for more performance */
		int wasted = currentCapacity - currentLength;
		/*[PR CVMC 106450] Fix CaffeineMark StringAtom benchmark */
		if (wasted >= 768 || (wasted >= INITIAL_SIZE && wasted >= (currentCapacity >> 1))) {
			// Check if the StringBuffer is compressed
			if (String.enableCompression && count >= 0) {
				return new String (value, 0, currentLength, true, false);
			} else {
				return new String (value, 0, currentLength, false, false);
			}
		}
	} else {
		// Do not copy the char[] if it will not get smaller because of object alignment
		int roundedCount = (currentLength + 3) & ~3;
		if (roundedCount < currentCapacity) {
			// Check if the StringBuffer is compressed
			if (String.enableCompression && count >= 0) {
				return new String (value, 0, currentLength, true, false);
			} else {
				return new String (value, 0, currentLength, false, false);
			}
		}
	}
	
	/*[IF Sidecar19-SE]*/
	shared = true;
	/*[ELSE]*/
	capacity = capacity | sharedBit;
	/*[ENDIF]*/
	
	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		return new String (value, 0, currentLength, true);
	} else {
		return new String (value, 0, currentLength, false);
	}
}

private void writeObject(ObjectOutputStream stream) throws IOException {
	int currentLength = lengthInternal();
	
	/*[PR CVMC 121958] Must always call defaultReadObject() / defautWriteObject() or readFields() / writeFields() */
	stream.defaultWriteObject();
	stream.writeInt(currentLength);
	
	/*[IF Sidecar19-SE]*/
	char[] newData = new char[currentLength];
	
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		String.decompress(value, 0, newData, 0, currentLength);
	} else {
		String.decompressedArrayCopy(value, 0, newData, 0, currentLength);
	}
	
	stream.writeObject(newData);
	/*[ELSE]*/
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		char[] newData = new char[currentLength];
		
		String.decompress(value, 0, newData, 0, currentLength);

		stream.writeObject(newData);
	} else {
		stream.writeObject(value);
	}
	/*[ENDIF]*/
}

private void readObject(ObjectInputStream stream) throws IOException, ClassNotFoundException {
	/*[PR CVMC 121958] Must always call defaultReadObject() / defautWriteObject() or readFields() / writeFields() */
	stream.defaultReadObject();
	
	int streamCount = stream.readInt();

	char[] streamValue = (char[])stream.readObject();
	
	if (streamCount > streamValue.length) {
		throw new InvalidObjectException(com.ibm.oti.util.Msg.getString("K0199")); //$NON-NLS-1$
	} 
	
	if (String.enableCompression) {
		if (String.compressible(streamValue, 0, streamValue.length)) {
			/*[IF Sidecar19-SE]*/
			value = new byte[streamValue.length];
			/*[ELSE]*/
			if (streamValue.length == Integer.MAX_VALUE) {
				value = new char[(streamValue.length / 2) + 1];
			} else {
				value = new char[(streamValue.length + 1) / 2];
			}
			/*[ENDIF]*/
			
			String.compress(streamValue, 0, value, 0, streamValue.length);
			
			count = streamCount;

			/*[IF !Sidecar19-SE]*/
			capacity = streamValue.length;
			/*[ENDIF]*/
		} else {
			/*[IF Sidecar19-SE]*/
			value = new byte[streamValue.length * 2];
			
			String.decompressedArrayCopy(streamValue, 0, value, 0, streamValue.length);
			/*[ELSE]*/
			value = new char[streamValue.length];
			
			System.arraycopy(streamValue, 0, value, 0, streamValue.length);
			/*[ENDIF]*/
			
			count = streamCount | uncompressedBit;

			/*[IF !Sidecar19-SE]*/
			capacity = streamValue.length;
			/*[ENDIF]*/
			
			String.initCompressionFlag();
		}
	} else {
		/*[IF Sidecar19-SE]*/
		value = new byte[streamValue.length * 2];
		
		String.decompressedArrayCopy(streamValue, 0, value, 0, streamValue.length);
		/*[ELSE]*/
		value = new char[streamValue.length];
		
		System.arraycopy(streamValue, 0, value, 0, streamValue.length);
		/*[ENDIF]*/
		
		count = streamCount;

		/*[IF !Sidecar19-SE]*/
		capacity = streamValue.length;
		/*[ENDIF]*/
	}
}

/**
 * Adds the specified StringBuffer to the end of this StringBuilder.
 *
 * @param		buffer	the StringBuffer
 * @return		this StringBuilder
 */
public StringBuilder append(StringBuffer buffer) {
	if (buffer == null) {
		return append((String)null);
	}
	
	synchronized (buffer) {
		if (String.enableCompression && buffer.isCompressed()) {
			return append(buffer.getValue(), 0, buffer.length(), true);
		} else {
			return append(buffer.getValue(), 0, buffer.length(), false);
		}
	}
}

/**
 * Copies a range of characters into a new String.
 *
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @return		a new String containing the characters from start to end - 1
 *
 * @exception	IndexOutOfBoundsException when <code>start < 0, start > end</code> or
 *				<code>end > length()</code>
 */
public CharSequence subSequence(int start, int end) {
	return substring(start, end);
}

/**
 * Searches in this StringBuilder for the first index of the specified character. The
 * search for the character starts at the beginning and moves towards the
 * end.
 *
 * @param		string	the string to find
 * @return		the index in this StringBuilder of the specified character, -1 if the
 *				character isn't found
 *
 * @see			#lastIndexOf(String)
 */
public int indexOf(String string) {
	return indexOf(string, 0);
}

/**
 * Searches in this StringBuilder for the index of the specified character. The
 * search for the character starts at the specified offset and moves towards
 * the end.
 *
 * @param		subString		the string to find
 * @param		start	the starting offset
 * @return		the index in this StringBuilder of the specified character, -1 if the
 *				character isn't found
 *
 * @see			#lastIndexOf(String,int)
 */
public int indexOf(String subString, int start) {
	int currentLength = lengthInternal();
	
	if (start < 0) {
		start = 0;
	}
	
	int subStringLength = subString.lengthInternal();
	
	if (subStringLength > 0) {
		if (subStringLength + start > currentLength) {
			return -1;
		}
		
		char firstChar = subString.charAtInternal(0);
		
		// Check if the StringBuilder is compressed
		if (String.enableCompression && count >= 0) {
			if (!subString.isCompressed()) {
				return -1;
			}
			
			while (true) {
				int i = start;
				
				boolean found = false;
				
				for (; i < currentLength; ++i) {
					if (helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, i)) == firstChar) {
						found = true;
						break;
					}
				}
				
				// Handles subStringLength > currentLength || start >= currentLength
				if (!found || subStringLength + i > currentLength) {
					return -1; 
				}
				
				int o1 = i;
				int o2 = 0;
				
				while (++o2 < subStringLength && helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, ++o1)) == subString.charAtInternal(o2));
				
				if (o2 == subStringLength) {
					return i;
				}
				
				start = i + 1;
			}
		} else {
			while (true) {
				int i = start;
				
				boolean found = false;
				
				for (; i < currentLength; ++i) {
					/*[IF Sidecar19-SE]*/
					if (helpers.getCharFromArrayByIndex(value, i) == firstChar) {
					/*[ELSE]*/
					if (value[i] == firstChar) {
					/*[ENDIF]*/
						found = true;
						break;
					}
				}
				
				// Handles subStringLength > currentLength || start >= currentLength
				if (!found || subStringLength + i > currentLength) {
					return -1; 
				}
				
				int o1 = i;
				int o2 = 0;
				
				/*[IF Sidecar19-SE]*/
				while (++o2 < subStringLength && helpers.getCharFromArrayByIndex(value, ++o1) == subString.charAtInternal(o2));
				/*[ELSE]*/
				while (++o2 < subStringLength && value[++o1] == subString.charAtInternal(o2));
				/*[ENDIF]*/
				
				if (o2 == subStringLength) {
					return i;
				}
				
				start = i + 1;
			}
		}
		
	} else {
		return (start < currentLength || start == 0) ? start : currentLength;
	}
}

/**
 * Searches in this StringBuilder for the last index of the specified character. The
 * search for the character starts at the end and moves towards the beginning.
 *
 * @param		string	the string to find
 * @return		the index in this StringBuilder of the specified character, -1 if the
 *				character isn't found
 *
 * @see			#indexOf(String)
 */
public int lastIndexOf(String string) {
	int currentLength = lengthInternal();
	
	return lastIndexOf(string, currentLength);
}

/**
 * Searches in this StringBuilder for the index of the specified character. The
 * search for the character starts at the specified offset and moves towards
 * the beginning.
 *
 * @param		subString		the string to find
 * @param		start	the starting offset
 * @return		the index in this StringBuilder of the specified character, -1 if the
 *				character isn't found
 *
 * @see			#indexOf(String,int)
 */
public int lastIndexOf(String subString, int start) {
	int currentLength = lengthInternal();
	
	int subStringLength = subString.lengthInternal();
	
	if (subStringLength <= currentLength && start >= 0) {
		if (subStringLength > 0) {
			if (start > currentLength - subStringLength) {
				start = currentLength - subStringLength;
			}
			
			char firstChar = subString.charAtInternal(0);
			
			// Check if the StringBuilder is compressed
			if (String.enableCompression && count >= 0) {
				if (!subString.isCompressed()) {
					return -1;
				}
				
				while (true) {
					int i = start;
					
					boolean found = false;
					
					for (; i >= 0; --i) {
						if (helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, i)) == firstChar) {
							found = true;
							break;
						}
					}
					
					if (!found) {
						return -1;
					}
					
					int o1 = i;
					int o2 = 0;
					
					while (++o2 < subStringLength && helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, ++o1)) == subString.charAtInternal(o2));
					
					if (o2 == subStringLength) {
						return i;
					}
					
					start = i - 1;
				}
			} else {
				while (true) {
					int i = start;
					
					boolean found = false;
					
					for (; i >= 0; --i) {
						/*[IF Sidecar19-SE]*/
						if (helpers.getCharFromArrayByIndex(value, i) == firstChar) {
						/*[ELSE]*/
						if (value[i] == firstChar) {
						/*[ENDIF]*/
							found = true;
							break;
						}
					}
					
					if (!found) {
						return -1;
					}
					
					int o1 = i;
					int o2 = 0;
					
					/*[IF Sidecar19-SE]*/
					while (++o2 < subStringLength && helpers.getCharFromArrayByIndex(value, ++o1) == subString.charAtInternal(o2));
					/*[ELSE]*/
					while (++o2 < subStringLength && value[++o1] == subString.charAtInternal(o2));
					/*[ENDIF]*/
					
					if (o2 == subStringLength) {
						return i;
					}
					
					start = i - 1;
				}
			}
		} else {
			return start < currentLength ? start : currentLength;
		}
	} else {
		return -1;
	}
}

/**
 * Return the underlying buffer and set the shared flag.
 *
 */
/*[IF Sidecar19-SE]*/
byte[] shareValue() {
	shared = true;
/*[ELSE]*/
char[] shareValue() {
	capacity = capacity | sharedBit;
/*[ENDIF]*/
	
	return value;
}

boolean isCompressed() {
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		return true;
	} else {
		return false;
	}
}

/**
 * Constructs a new StringBuilder containing the characters in
 * the specified CharSequence and the default capacity.
 *
 * @param		sequence	the initial contents of this StringBuilder
 * @exception	NullPointerException when sequence is null
 */
public StringBuilder(CharSequence sequence) {	
	int size = sequence.length();
	
	if (size < 0) {
		size = 0;
	}
	
	int newLength = INITIAL_SIZE + size;
	
	if (String.enableCompression) {
		/*[IF Sidecar19-SE]*/
		value = new byte[newLength];
		/*[ELSE]*/
		value = new char[(newLength + 1) / 2];
		/*[ENDIF]*/
	} else {
		/*[IF Sidecar19-SE]*/
		value = new byte[newLength * 2];
		/*[ELSE]*/
		value = new char[newLength];
		/*[ENDIF]*/
	}

	/*[IF !Sidecar19-SE]*/
	capacity = newLength;
	/*[ENDIF]*/
	
	if (sequence instanceof String) {
		append((String)sequence);
	} else if (sequence instanceof StringBuffer) {		
		append((StringBuffer)sequence);
	} else {		
		if (String.enableCompression) {
			boolean isCompressed = true;
			
			for (int i = 0; i < size; ++i) {
				if (sequence.charAt(i) > 255) {
					isCompressed = false;
					
					break;
				}
			}
			
			if (isCompressed) {				
				count = size;
				
				for (int i = 0; i < size; ++i) {
					helpers.putByteInArrayByIndex(value, i, (byte) sequence.charAt(i));
				}
			} else {
				/*[IF Sidecar19-SE]*/
				value = new byte[newLength * 2];
				/*[ELSE]*/
				value = new char[newLength];
				/*[ENDIF]*/
				
				count = size | uncompressedBit;
				
				for (int i = 0; i < size; ++i) {
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, i, (char) sequence.charAt(i));
					/*[ELSE]*/
					value[i] = sequence.charAt(i);
					/*[ENDIF]*/
				}
				
				String.initCompressionFlag();
			}
		} else {			
			count = size;
			
			for (int i = 0; i < size; ++i) {
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, i, (char) sequence.charAt(i));
				/*[ELSE]*/
				value[i] = sequence.charAt(i);
				/*[ENDIF]*/
			}
		}
	}
}

/**
 * Adds the specified CharSequence to the end of this StringBuilder.
 *
 * @param		sequence	the CharSequence
 * @return		this StringBuilder
 */
public StringBuilder append(CharSequence sequence) {
	if (sequence == null) {
		return append(String.valueOf(sequence));
	} else if (sequence instanceof String) {
		return append((String)sequence);
	} else if (sequence instanceof StringBuffer) {
		return append((StringBuffer)sequence);
	} else if (sequence instanceof StringBuilder) {
		StringBuilder builder = (StringBuilder) sequence;
		
		// Check if the StringBuilder is compressed
		if (String.enableCompression && builder.count >= 0) {
			return append(builder.value, 0, builder.lengthInternal(), true);
		} else {
			return append(builder.value, 0, builder.lengthInternal(), false);
		}
	} else {
		int currentLength = lengthInternal();
		int currentCapacity = capacityInternal();
		
		int sequenceLength = sequence.length();
		
		int newLength = currentLength + sequenceLength;
		
		if (String.enableCompression) {
			boolean isCompressed = true;
			
			if (count >= 0) {
				for (int i = 0; i < sequence.length(); ++i) {
					if (sequence.charAt(i) > 255) {
						isCompressed = false;
						
						break;
					}
				}
			}
			
			// Check if the StringBuilder is compressed
			if (count >= 0 && isCompressed) {
				if (newLength > currentCapacity) {
					ensureCapacityImpl(newLength);
				}
				
				for (int i = 0; i < sequence.length(); ++i) {
					helpers.putByteInArrayByIndex(value, currentLength + i, (byte) sequence.charAt(i));
				}
				
				count = newLength;
			} else {
				// Check if the StringBuilder is compressed
				if (count >= 0) {
					decompress(newLength);
				}
				
				if (newLength > currentCapacity) {
					ensureCapacityImpl(newLength);
				}
				
				for (int i = 0; i < sequence.length(); ++i) {
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, currentLength + i, (char) sequence.charAt(i));
					/*[ELSE]*/
					value[currentLength + i] = sequence.charAt(i);
					/*[ENDIF]*/
				}
				
				count = newLength | uncompressedBit;
			}
		} else {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			for (int i = 0; i < sequence.length(); ++i) {
				/*[IF Sidecar19-SE]*/
				helpers.putCharInArrayByIndex(value, currentLength + i, (char) sequence.charAt(i));
				/*[ELSE]*/
				value[currentLength + i] = sequence.charAt(i);
				/*[ENDIF]*/
			}
			
			count = newLength;
		}
		
		return this;
	}
}

/**
 * Adds the specified CharSequence to the end of this StringBuilder.
 *
 * @param		sequence	the CharSequence
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @return		this StringBuilder
 * 
 * @exception	IndexOutOfBoundsException when <code>start < 0, start > end</code> or
 *				<code>end > length()</code>
 */
public StringBuilder append(CharSequence sequence, int start, int end) {
	if (sequence == null) {
		return append(String.valueOf(sequence), start, end);
	} else if (sequence instanceof String) {
		return append(((String)sequence).substring(start, end));
	} else if (start >= 0 && end >= 0 && start <= end && end <= sequence.length()) {
		if (sequence instanceof StringBuffer) {
			synchronized (sequence) {
				StringBuffer buffer = (StringBuffer) sequence;
				
				if (String.enableCompression && buffer.isCompressed()) {
					return append(buffer.getValue(), start, end - start, true);
				} else {
					return append(buffer.getValue(), start, end - start, false);
				}
			}
		} else if (sequence instanceof StringBuilder) {
			synchronized (sequence) {
				StringBuilder builder = (StringBuilder) sequence;
				
				// Check if the StringBuilder is compressed
				if (String.enableCompression && builder.count >= 0) {
					return append(builder.value, start, end - start, true);
				} else {
					return append(builder.value, start, end - start, false);
				}
			}
		} else {
			int currentLength = lengthInternal();
			int currentCapacity = capacityInternal();
			
			int newLength = currentLength + end - start;
			
			if (String.enableCompression) {
				boolean isCompressed = true;
				
				if (count >= 0) {
					for (int i = 0; i < sequence.length(); ++i) {
						if (sequence.charAt(i) > 255) {
							isCompressed = false;
							
							break;
						}
					}
				}
				
				// Check if the StringBuilder is compressed
				if (count >= 0 && isCompressed) {
					if (newLength > currentCapacity) {
						ensureCapacityImpl(newLength);
					}
					
					for (int i = 0; i < end - start; ++i) {
						helpers.putByteInArrayByIndex(value, currentLength + i, (byte) sequence.charAt(start + i));
					}
					
					count = newLength;
				} else {
					// Check if the StringBuilder is compressed
					if (count >= 0) {
						decompress(newLength);
					}
					
					if (newLength > currentCapacity) {
						ensureCapacityImpl(newLength);
					}
					
					for (int i = 0; i < end - start; ++i) {
						/*[IF Sidecar19-SE]*/
						helpers.putCharInArrayByIndex(value, currentLength + i, (char) sequence.charAt(start + i));
						/*[ELSE]*/
						value[currentLength + i] = sequence.charAt(start + i);
						/*[ENDIF]*/
					}
					
					count = newLength | uncompressedBit;
				}
			} else {
				if (newLength > currentCapacity) {
					ensureCapacityImpl(newLength);
				}
				
				for (int i = 0; i < end - start; ++i) {
					/*[IF Sidecar19-SE]*/
					helpers.putCharInArrayByIndex(value, currentLength + i, (char) sequence.charAt(start + i));
					/*[ELSE]*/
					value[currentLength + i] = sequence.charAt(start + i);
					/*[ENDIF]*/
				}
				
				count = newLength;
			}
			
			return this;
		}	
	} else {
		throw new IndexOutOfBoundsException();
	}
}

/**
 * Inserts the CharSequence at the specified offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		sequence	the CharSequence to insert
 * @return		this StringBuilder
 *
 * @exception	IndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>
 */
public StringBuilder insert(int index, CharSequence sequence) {
	int currentLength = lengthInternal();
	
	if (index >= 0 && index <= currentLength) {
		if (sequence == null) {
			return insert(index, String.valueOf(sequence));
		} else if (sequence instanceof String) {
			return insert(index, (String) sequence);
		} else if (sequence instanceof StringBuffer) {
			synchronized(sequence) {
				StringBuffer buffer = (StringBuffer) sequence;
				
				if (String.enableCompression && buffer.isCompressed()) {
					return insert(index, buffer.getValue(), 0, buffer.length(), true);
				} else {
					return insert(index, buffer.getValue(), 0, buffer.length(), false);
				}
			}
		} else if (sequence instanceof StringBuilder) {
			synchronized (sequence) {
				StringBuilder builder = (StringBuilder) sequence;
				
				// Check if the StringBuilder is compressed
				if (String.enableCompression && builder.count >= 0) {
					return insert(index, builder.value, 0, builder.lengthInternal(), true);
				} else {
					return insert(index, builder.value, 0, builder.lengthInternal(), false);
				}
			}
		} else {
			int sequneceLength = sequence.length();
			
			if (sequneceLength > 0) {
				move(sequneceLength, index);
				
				int newLength = currentLength + sequneceLength;
				
				if (String.enableCompression) {
					/*[IF]*/
					// TODO : This is very suboptimal. CharSequence needs to be compressified and an isCompressed method needs to be added.
					/*[ENDIF]*/
					boolean isCompressed = true;
					
					for (int i = 0; i < sequneceLength; ++i) {
						if (sequence.charAt(i) > 255) {
							isCompressed = false;
							
							break;
						}
					}
					
					// Check if the StringBuilder is compressed
					if (count >= 0 && isCompressed) {
						for (int i = 0; i < sequneceLength; ++i) {
							helpers.putByteInArrayByIndex(value, index + i, (byte) sequence.charAt(i));
						}
						
						count = newLength;
						
						return this;
					} else {
						count = newLength;

						// Check if the StringBuilder is compressed
						if (count >= 0) {
							decompress(value.length);
						}
						
						for (int i = 0; i < sequneceLength; ++i) {
							/*[IF Sidecar19-SE]*/
							helpers.putCharInArrayByIndex(value, index + i, (char) sequence.charAt(i));
							/*[ELSE]*/
							value[index + i] = sequence.charAt(i);
							/*[ENDIF]*/
						}
						
						count = count | uncompressedBit;
					}
				} else {
					for (int i = 0; i < sequneceLength; ++i) {
						/*[IF Sidecar19-SE]*/
						helpers.putCharInArrayByIndex(value, index + i, (char) sequence.charAt(i));
						/*[ELSE]*/
						value[index + i] = sequence.charAt(i);
						/*[ENDIF]*/
					}
					
					count = newLength;
				}
			}
			
			return this;
		}
	} else {
		throw new IndexOutOfBoundsException();
	}
}

/**
 * Inserts the CharSequence at the specified offset in this StringBuilder.
 *
 * @param		index	the index at which to insert
 * @param		sequence	the CharSequence to insert
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @return		this StringBuilder
 *
 * @exception	IndexOutOfBoundsException when <code>index < 0</code> or
 *				<code>index > length()</code>, or when <code>start < 0, start > end</code> or
 *				<code>end > length()</code>
 */
public StringBuilder insert(int index, CharSequence sequence, int start, int end) {
	int currentLength = lengthInternal();
	
	if (index >= 0 && index <= currentLength) {
		if (sequence == null)
			return insert(index, String.valueOf(sequence), start, end);
		if (sequence instanceof String) {
			return insert(index, ((String) sequence).substring(start, end));
		}
		if (start >= 0 && end >= 0 && start <= end && end <= sequence.length()) {
			if (sequence instanceof StringBuffer) {
				synchronized(sequence) {
					StringBuffer buffer = (StringBuffer) sequence;
					
					if (String.enableCompression && buffer.isCompressed()) {
						return insert(index, buffer.getValue(), start, end - start, true);
					} else {
						return insert(index, buffer.getValue(), start, end - start, false);
					}
				}
			} else if (sequence instanceof StringBuilder) {
				synchronized(sequence) {
					StringBuilder builder = (StringBuilder) sequence;
					
					// Check if the StringBuilder is compressed
					if (String.enableCompression && builder.count >= 0) {
						return insert(index, builder.value, start, end - start, true);
					} else {
						return insert(index, builder.value, start, end - start, false);
					}
				}
			} else {
				int sequenceLength = end - start;
				
				if (sequenceLength > 0) {
					move(sequenceLength, index);
					
					int newLength = currentLength + sequenceLength;
					
					if (String.enableCompression) {
						/*[IF]*/
						// TODO : This is very suboptimal. CharSequence needs to be compressified and an isCompressed method needs to be added.
						/*[ENDIF]*/
						boolean isCompressed = true;
						
						for (int i = 0; i < sequenceLength; ++i) {
							if (sequence.charAt(start + i) > 255) {
								isCompressed = false;
								
								break;
							}
						}
						
						// Check if the StringBuilder is compressed
						if (count >= 0 && isCompressed) {
							for (int i = 0; i < sequenceLength; ++i) {
								helpers.putByteInArrayByIndex(value, index + i, (byte) sequence.charAt(start + i));
							}
							
							count = newLength;
							
							return this;
						} else {
							count = newLength;

							// Check if the StringBuilder is compressed
							if (count >= 0) {
								decompress(value.length);
							}
							
							for (int i = 0; i < sequenceLength; ++i) {
								/*[IF Sidecar19-SE]*/
								helpers.putCharInArrayByIndex(value, index + i, (char) sequence.charAt(start + i));
								/*[ELSE]*/
								value[index + i] = sequence.charAt(start + i);
								/*[ENDIF]*/
							}
							
							count = count | uncompressedBit;
						}
					} else {
						for (int i = 0; i < sequenceLength; ++i) {
							/*[IF Sidecar19-SE]*/
							helpers.putCharInArrayByIndex(value, index + i, (char) sequence.charAt(start + i));
							/*[ELSE]*/
							value[index + i] = sequence.charAt(start + i);
							/*[ENDIF]*/
						}
						
						count = newLength;
					}
				}
				
				return this;
			}
		} else {
			throw new IndexOutOfBoundsException();
		}
	} else {
		throw new IndexOutOfBoundsException();
	}
}

/**
 * Optionally modify the underlying char array to only
 * be large enough to hold the characters in this StringBuffer. 
 */
public void trimToSize() {
	int currentLength = lengthInternal();
	int currentCapacity = capacityInternal();
	
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		// Check if the StringBuilder is not shared
		/*[IF Sidecar19-SE]*/
		if (!shared && currentCapacity != currentLength) {
			byte[] newData = new byte[currentLength];
		/*[ELSE]*/
		if (capacity >= 0 && currentCapacity != currentLength) {
			char[] newData = new char[(currentLength + 1) / 2];
		/*[ENDIF]*/
			
			String.compressedArrayCopy(value, 0, newData, 0, currentLength);
			
			value = newData;
			
			/*[IF !Sidecar19-SE]*/
			capacity = currentLength;
			/*[ENDIF]*/
		}
	} else {
		// Check if the StringBuilder is not shared
		/*[IF Sidecar19-SE]*/
		if (!shared && currentCapacity != currentLength) {
			byte[] newData = new byte[currentLength * 2];
		/*[ELSE]*/
		if (capacity >= 0 && currentCapacity != currentLength) {
			char[] newData = new char[currentLength];
		/*[ENDIF]*/

			String.decompressedArrayCopy(value, 0, newData, 0, currentLength);
			
			value = newData;
			
			/*[IF !Sidecar19-SE]*/
			capacity = currentLength;
			/*[ENDIF]*/
		}
	}
}

/**
 * Returns the Unicode character at the given point.
 * 
 * @param 		index		the character index
 * @return		the Unicode character value at the index
 */
public int codePointAt(int index) {
	int currentLength = lengthInternal();
	
	if (index >= 0 && index < currentLength) {
		// Check if the StringBuilder is compressed
		if (String.enableCompression && count >= 0) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		} else {
			/*[IF Sidecar19-SE]*/
			int high = helpers.getCharFromArrayByIndex(value, index);
			/*[ELSE]*/
			int high = value[index];
			/*[ENDIF]*/
			
			if ((index + 1) < currentLength && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
				/*[IF Sidecar19-SE]*/
				int low = helpers.getCharFromArrayByIndex(value, index + 1);
				/*[ELSE]*/
				int low = value[index + 1];
				/*[ENDIF]*/
				
				if (low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
					return 0x10000 + ((high - Character.MIN_HIGH_SURROGATE) << 10) + (low - Character.MIN_LOW_SURROGATE);
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
 * @param 		index		the character index
 * @return		the Unicode character value before the index
 */
public int codePointBefore(int index) {
	int currentLength = lengthInternal();
	
	if (index > 0 && index <= currentLength) {
		// Check if the StringBuilder is compressed
		if (String.enableCompression && count >= 0) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index - 1));
		} else {
			/*[IF Sidecar19-SE]*/
			int low = helpers.getCharFromArrayByIndex(value, index - 1);
			/*[ELSE]*/
			int low = value[index - 1];
			/*[ENDIF]*/
			
			if (index > 1 && low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
				/*[IF Sidecar19-SE]*/
				int high = helpers.getCharFromArrayByIndex(value, index - 2);
				/*[ELSE]*/
				int high = value[index - 2];
				/*[ENDIF]*/
				
				if (high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
					return 0x10000 + ((high - Character.MIN_HIGH_SURROGATE) << 10) + (low - Character.MIN_LOW_SURROGATE);
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
 * @param 		start		first index
 * @param		end			last index
 * @return		the total Unicode values
 */
public int codePointCount(int start, int end) {
	int currentLength = lengthInternal();
	
	if (start >= 0 && start <= end && end <= currentLength) {
		// Check if the StringBuilder is compressed
		if (String.enableCompression && count >= 0) {
			return end - start;
		} else {
			int count = 0;
			
			for (int i = start; i < end; ++i) {
				/*[IF Sidecar19-SE]*/
				int high = helpers.getCharFromArrayByIndex(value, i);
				/*[ELSE]*/
				int high = value[i];
				/*[ENDIF]*/
				
				if (i + 1 < end && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
					/*[IF Sidecar19-SE]*/
					int low = helpers.getCharFromArrayByIndex(value, i + 1);
					/*[ELSE]*/
					int low = value[i + 1];
					/*[ENDIF]*/
					
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
 * @param 		start			the position to offset
 * @param		codePointCount	the code point count
 * @return		the offset index
 */
public int offsetByCodePoints(int start, int codePointCount) {
	int currentLength = lengthInternal();
	
	if (start >= 0 && start <= currentLength) {
		// Check if the StringBuilder is compressed
		if (String.enableCompression && count >= 0) {
			int index = start + codePointCount;

			if (index >= currentLength) {
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
					if (index == currentLength) {
						throw new IndexOutOfBoundsException();
					}

					/*[IF Sidecar19-SE]*/
					int high = helpers.getCharFromArrayByIndex(value, index);
					/*[ELSE]*/
					int high = value[index];
					/*[ENDIF]*/

					if ((index + 1) < currentLength && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
						/*[IF Sidecar19-SE]*/
						int low = helpers.getCharFromArrayByIndex(value, index + 1);
						/*[ELSE]*/
						int low = value[index + 1];
						/*[ENDIF]*/

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

					/*[IF Sidecar19-SE]*/
					int low = helpers.getCharFromArrayByIndex(value, index - 1);
					/*[ELSE]*/
					int low = value[index - 1];
					/*[ENDIF]*/

					if (index > 1 && low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
						/*[IF Sidecar19-SE]*/
						int high = helpers.getCharFromArrayByIndex(value, index - 2);
						/*[ELSE]*/
						int high = value[index - 2];
						/*[ENDIF]*/

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
 * Adds the specified code point to the end of this StringBuffer.
 *
 * @param		codePoint	the code point
 * @return		this StringBuffer
 */
public StringBuilder appendCodePoint(int codePoint) {
	if (codePoint >= 0) {
		if (codePoint < 0x10000) {
			return append((char)codePoint);
		} else if (codePoint < 0x110000) {
			// Check if the StringBuilder is compressed
			if (String.enableCompression && count >= 0) {
				decompress(value.length);
			}

			int currentLength = lengthInternal();
			int currentCapacity = capacityInternal();

			int newLength = currentLength + 2;

			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}

			codePoint -= 0x10000;

			/*[IF Sidecar19-SE]*/
			helpers.putCharInArrayByIndex(value, currentLength, (char) (Character.MIN_HIGH_SURROGATE + (codePoint >> 10)));
			helpers.putCharInArrayByIndex(value, currentLength + 1, (char) (Character.MIN_LOW_SURROGATE + (codePoint & 0x3ff)));
			/*[ELSE]*/
			value[currentLength] = (char) (Character.MIN_HIGH_SURROGATE + (codePoint >> 10));
			value[currentLength + 1] = (char) (Character.MIN_LOW_SURROGATE + (codePoint & 0x3ff));
			/*[ENDIF]*/			

			if (String.enableCompression) {
				count = newLength | uncompressedBit;
			} else {
				count = newLength;
			}

			return this;
		}
	}
	
	throw new IllegalArgumentException();
}

/*
 * Returns the character array for this StringBuilder.
 */
/*[IF Sidecar19-SE]*/
byte[] getValue() {
/*[ELSE]*/
char[] getValue() {
/*[ENDIF]*/
	return value;
}

/*[IF Sidecar19-SE]*/
	@Override
	public IntStream chars() {
		/* Following generic CharSequence method invoking need to be updated with optimized implementation specifically for this class */
		return CharSequence.super.chars();
	}
	
	@Override
	public IntStream codePoints() {
		/* Following generic CharSequence method invoking need to be updated with optimized implementation specifically for this class */
		return CharSequence.super.codePoints();
	}
/*[ENDIF]*/

}
