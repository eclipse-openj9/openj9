/*[INCLUDE-IF Sidecar16 & !Sidecar19-SE]*/

package java.lang;

/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

/**
 * StringBuffer is a variable size contiguous indexable array of characters.
 * The length of the StringBuffer is the number of characters it contains.
 * The capacity of the StringBuffer is the number of characters it can hold.
 * <p>
 * Characters may be inserted at any position up to the length of the
 * StringBuffer, increasing the length of the StringBuffer. Characters at any
 * position in the StringBuffer may be replaced, which does not affect the
 * StringBuffer length.
 * <p>
 * The capacity of a StringBuffer may be specified when the StringBuffer is
 * created. If the capacity of the StringBuffer is exceeded, the capacity
 * is increased.
 *
 * @author		OTI
 * @version		initial
 *
 * @see			String
 */
 

public final class StringBuffer extends AbstractStringBuilder implements Serializable, CharSequence, Appendable {
	private static final long serialVersionUID = 3388685877147921107L;
	
	private static final int INITIAL_SIZE = 16;
	
	
	private static boolean TOSTRING_COPY_BUFFER_ENABLED = false;
	
	// Used to access compression related helper methods
	private static final com.ibm.jit.JITHelpers helpers = com.ibm.jit.JITHelpers.getHelpers();
	
	// Represents the bit in count field to test for whether this StringBuffer backing array is not compressed
	// under String compression mode. This bit is not used when String compression is disabled.
	private static final int uncompressedBit = 0x80000000;
	
	// Represents the bit in capacity field to test for whether this StringBuffer backing array is shared.
	private static final int sharedBit = 0x80000000;
	
	private static final java.io.ObjectStreamField serialPersistentFields[] = new java.io.ObjectStreamField[] {
		new java.io.ObjectStreamField("count", Integer.TYPE), //$NON-NLS-1$
		new java.io.ObjectStreamField("value", char[].class), //$NON-NLS-1$
		new java.io.ObjectStreamField("shared", Boolean.TYPE), //$NON-NLS-1$
	};
	
	private int count;
	private char[] value;
	private int capacity;
	
	private void decompress(int min) {
		int currentLength = lengthInternalUnsynchronized();
		int currentCapacity = capacityInternal();
		
		char[] newValue = null;
		
		if (min > currentCapacity) {
			int twice = (currentCapacity << 1) + 2;
			
			newValue = new char[min > twice ? min : twice];
		} else {
			newValue = new char[currentCapacity];
		}

		String.decompress(value, 0, newValue, 0, currentLength);
		
		count = count | uncompressedBit;
		value = newValue;
		capacity = newValue.length;
		
		String.initCompressionFlag();
	}
	
/**
 * Constructs a new StringBuffer using the default capacity.
 */
public StringBuffer() {
	this(INITIAL_SIZE);
}

/**
 * Constructs a new StringBuffer using the specified capacity.
 *
 * @param		capacity	the initial capacity
 */
public StringBuffer(int capacity) {
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
	
	if (String.enableCompression) {
		if (capacity == Integer.MAX_VALUE) {
			arraySize = (capacity / 2) + 1;
		} else {
			arraySize = (capacity + 1) / 2;
		}
	}
	value = new char[arraySize];
	
	this.capacity = capacity;
}

/**
 * Constructs a new StringBuffer containing the characters in
 * the specified string and the default capacity.
 *
 * @param		string	the initial contents of this StringBuffer
 * @exception	NullPointerException when string is null
 */
public StringBuffer (String string) {
	int stringLength = string.lengthInternal();
	
	int newLength = stringLength + INITIAL_SIZE;
	
	if (String.enableCompression) {
		if (string.isCompressed ()) {
			value = new char[(newLength + 1) / 2];

			string.getBytes(0, stringLength, value, 0);
			
			capacity = newLength;
			
			count = stringLength;
		} else {
			value = new char[newLength];

			string.getCharsNoBoundChecks(0, stringLength, value, 0);

			capacity = newLength;
			
			count = stringLength | uncompressedBit;
			
			String.initCompressionFlag();
		}
	} else {
		value = new char[newLength];
		
		string.getCharsNoBoundChecks(0, stringLength, value, 0);
		
		capacity = newLength;
		
		count = stringLength;
	}
}

/**
 * Adds the character array to the end of this StringBuffer.
 *
 * @param		chars	the character array
 * @return		this StringBuffer
 *
 * @exception	NullPointerException when chars is null
 */
public synchronized StringBuffer append (char[] chars) {
	int currentLength = lengthInternalUnsynchronized();
	int currentCapacity = capacityInternal();
	
	int newLength = currentLength + chars.length;
	
	if (String.enableCompression) {
		// Check if the StringBuffer is compressed
		if (count >= 0 && String.compressible(chars, 0, chars.length)) {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			String.compress(chars, 0, value, currentLength, chars.length);
			
			count = newLength;
		} else {
			// Check if the StringBuffer is compressed
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
 * this StringBuffer.
 *
 * @param		chars	a character array
 * @param		start	the starting offset
 * @param		length	the number of characters
 * @return		this StringBuffer
 *
 * @exception	IndexOutOfBoundsException when {@code length < 0, start < 0} or
 *				{@code start + length > chars.length}
 * @exception	NullPointerException when chars is null
 */
public synchronized StringBuffer append (char chars[], int start, int length) {
	if (start >= 0 && 0 <= length && length <= chars.length - start) {
		int currentLength = lengthInternalUnsynchronized();
		int currentCapacity = capacityInternal();
		
		int newLength = currentLength + length;
		
		if (String.enableCompression) {
			// Check if the StringBuffer is compressed
			if (count >= 0 && String.compressible(chars, start, length)) {
				if (newLength > currentCapacity) {
					ensureCapacityImpl(newLength);
				}
				
				String.compress(chars, start, value, currentLength, length);
				
				count = newLength;
			} else {
				// Check if the StringBuffer is compressed
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

synchronized StringBuffer append (char[] chars, int start, int length, boolean compressed) {
	int currentLength = lengthInternalUnsynchronized();
	int currentCapacity = capacityInternal();
	
	int newLength = currentLength + length;
	
	if (String.enableCompression) {
		// Check if the StringBuffer is compressed
		if (count >= 0 && compressed) {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			String.compressedArrayCopy(chars, start, value, currentLength, length);
			
			count = newLength;
		} else {
			// Check if the StringBuffer is compressed
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
 * this StringBuffer.
 *
 * @param		ch	a character
 * @return		this StringBuffer
 */
public synchronized StringBuffer append(char ch) {
	int currentLength = lengthInternalUnsynchronized();
	int currentCapacity = capacityInternal();
	
	int newLength = currentLength + 1;
	
	if (String.enableCompression) {
		// Check if the StringBuffer is compressed
		if (count >= 0 && ch <= 255) {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			helpers.putByteInArrayByIndex(value, currentLength, (byte) ch);
			
			count = newLength;
		} else {
			// Check if the StringBuffer is compressed
			if (count >= 0) {
				decompress(newLength);
			}
			
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			value[currentLength] = ch;
			
			count = newLength | uncompressedBit;
		}
	} else {
		if (newLength > currentCapacity) {
			ensureCapacityImpl(newLength);
		}
		
		value[currentLength] = ch;
		
		count = newLength;
	}
	
	return this;
}

/**
 * Adds the string representation of the specified double to the
 * end of this StringBuffer.
 *
 * @param		value	the double
 * @return		this StringBuffer
 */
public StringBuffer append (double value) {
	return append (String.valueOf (value));
}

/**
 * Adds the string representation of the specified float to the
 * end of this StringBuffer.
 *
 * @param		value	the float
 * @return		this StringBuffer
 */
public StringBuffer append (float value) {
	return append (String.valueOf (value));
}

/**
 * Adds the string representation of the specified integer to the
 * end of this StringBuffer.
 *
 * @param		value	the integer
 * @return		this StringBuffer
 */
public synchronized StringBuffer append(int value) {
	if (value != Integer.MIN_VALUE) {
		if (String.enableCompression && count >= 0) {
			return append(Integer.toString(value));
		} else {
			int currentLength = lengthInternalUnsynchronized();
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
}

/**
 * Adds the string representation of the specified long to the
 * end of this StringBuffer.
 *
 * @param		value	the long
 * @return		this StringBuffer
 */
public synchronized StringBuffer append(long value) {
	if (value != Long.MIN_VALUE) {
		if (String.enableCompression && count >= 0) {
			return append(Long.toString(value));
		} else {
			int currentLength = lengthInternalUnsynchronized();
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
}

/**
 * Adds the string representation of the specified object to the
 * end of this StringBuffer.
 *
 * @param		value	the object
 * @return		this StringBuffer
 */
public StringBuffer append (Object value) {
	return append (String.valueOf (value));
}

/**
 * Adds the specified string to the end of this StringBuffer.
 *
 * @param		string	the string
 * @return		this StringBuffer
 */
public synchronized StringBuffer append (String string) {
	if (string == null) {
		string = "null"; //$NON-NLS-1$
	}
	
	int currentLength = lengthInternalUnsynchronized();
	int currentCapacity = capacityInternal();
	
	int stringLength = string.lengthInternal();
	
	int newLength = currentLength + stringLength;
	
	if (String.enableCompression) {
		// Check if the StringBuffer is compressed
		if (count >= 0 && string.isCompressed ()) {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			string.getBytes(0, stringLength, value, currentLength);
			
			count = newLength;
		} else {
			// Check if the StringBuffer is compressed
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
 * end of this StringBuffer.
 *
 * @param		value	the boolean
 * @return		this StringBuffer
 */
public StringBuffer append (boolean value) {
	return append (String.valueOf (value));
}

/**
 * Answers the number of characters this StringBuffer can hold without
 * growing.
 *
 * @return		the capacity of this StringBuffer
 *
 * @see			#ensureCapacity
 * @see			#length
 */
public int capacity() {
	return capacityInternal();
}

/**
 * Answers the number of characters this StringBuffer can hold without growing. This method is to be used internally
 * within the current package whenever possible as the JIT compiler will take special precaution to avoid generating
 * HCR guards for calls to this method.
 *
 * @return the capacity of this StringBuffer
 *
 * @see #ensureCapacity
 * @see #length
 */
int capacityInternal() {
	return capacity & ~sharedBit;
}

/**
 * Answers the character at the specified offset in this StringBuffer.
 *
 * @param 		index	the zero-based index in this StringBuffer
 * @return		the character at the index
 *
 * @exception	IndexOutOfBoundsException
 *              If {@code index < 0} or {@code index >= length()}
 */
public synchronized char charAt(int index) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (index >= 0 && index < currentLength) {
		// Check if the StringBuffer is compressed
		if (String.enableCompression && count >= 0) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		} else {
			return value[index];
		}
	}
	
	throw new StringIndexOutOfBoundsException(index);
}

/**
 * Deletes a range of characters.
 *
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code start < 0, start > end} or
 *				{@code end > length()}
 */
public synchronized StringBuffer delete(int start, int end) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (start >= 0) {
		if (end > currentLength) {
			end = currentLength;
		}
		
		if (end > start) {
			int numberOfTailChars = currentLength - end;

			try {
				// Check if the StringBuffer is not shared
				if (capacity >= 0) {
					if (numberOfTailChars > 0) {
						// Check if the StringBuffer is compressed
						if (String.enableCompression && count >= 0) {
							String.compressedArrayCopy(value, end, value, start, numberOfTailChars);
						} else {
							String.decompressedArrayCopy(value, end, value, start, numberOfTailChars);
						}
					}
				} else {
					char[] newData = new char[value.length];
					
					// Check if the StringBuffer is compressed
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
					
					capacity = capacity & ~sharedBit;
				}
			} catch (IndexOutOfBoundsException e) {
				throw new StringIndexOutOfBoundsException();
			}
			
			if (String.enableCompression) {
				// Check if the StringBuffer is compressed
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
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code location < 0} or
 *				{@code location >= length()}
 */
public synchronized StringBuffer deleteCharAt(int location) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (currentLength != 0) {
		return delete (location, location + 1);
	} else {
		throw new StringIndexOutOfBoundsException ();
	}
}

/**
 * Ensures that this StringBuffer can hold the specified number of characters
 * without growing.
 *
 * @param		min	 the minimum number of elements that this
 *				StringBuffer will hold before growing
 */
public synchronized void ensureCapacity(int min) {
	int currentCapacity = capacityInternal();
	
	if (min > currentCapacity) {
		ensureCapacityImpl(min);
	}
}

private void ensureCapacityImpl(int min) {
	int currentLength = lengthInternalUnsynchronized();
	int currentCapacity = capacityInternal();
	
	int newCapacity = (currentCapacity << 1) + 2;

	int newLength = min > newCapacity ? min : newCapacity;
	
	// Check if the StringBuilder is compressed
	if (String.enableCompression && count >= 0) {
		char[] newData = new char[(newLength + 1) / 2];
		
		String.compressedArrayCopy(value, 0, newData, 0, currentLength);
		
		value = newData;
		
	} else {
		char[] newData = new char[newLength];
		
		String.decompressedArrayCopy(value, 0, newData, 0, currentLength);
		value = newData;
	}
	
	capacity = newLength;
}

/**
 * Copies the specified characters in this StringBuffer to the character array
 * starting at the specified offset in the character array.
 *
 * @param		start	the starting offset of characters to copy
 * @param		end	the ending offset of characters to copy
 * @param		buffer	the destination character array
 * @param		index	the starting offset in the character array
 *
 * @exception	IndexOutOfBoundsException when {@code start < 0, end > length(),
 *				start > end, index < 0, end - start > buffer.length - index}
 * @exception	NullPointerException when buffer is null
 */
public synchronized void getChars(int start, int end, char[] buffer, int index) {
	try {
		int currentLength = lengthInternalUnsynchronized();
		
		if (start <= currentLength && end <= currentLength) {
			// Note that we must explicitly check all the conditions because we are not using System.arraycopy which would
			// have implicitly checked these conditions
			if (start >= 0 && start <= end && index >= 0 && end - start <= buffer.length - index) {
				// Check if the StringBuffer is compressed
				if (String.enableCompression && count >= 0) {
					String.decompress(value, start, buffer, index, end - start);
					
					return;
				} else {
					System.arraycopy(value, start, buffer, index, end - start);
				
					return;
				}
			}
		}
	} catch(IndexOutOfBoundsException e) {
		// Void
	}
	
	throw new StringIndexOutOfBoundsException ();
}

/**
 * Inserts the character array at the specified offset in this StringBuffer.
*
 * @param		index	the index at which to insert
 * @param		chars	the character array to insert
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 * @exception	NullPointerException when chars is null
 */
public synchronized StringBuffer insert(int index, char[] chars) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (0 <= index && index <= currentLength) {
		move(chars.length, index);
		
		if (String.enableCompression) {
			// Check if the StringBuffer is compressed
			if (count >= 0 && String.compressible(chars, 0, chars.length)) {
				String.compress(chars, 0, value, index, chars.length);
				
				count = currentLength + chars.length;
				
				return this;
			} else {
				// Check if the StringBuffer is compressed
				if (count >= 0) {
					decompress(value.length);
				}
				
				String.decompressedArrayCopy(chars, 0, value, index, chars.length);
				
				count = (currentLength + chars.length) | uncompressedBit;
				
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
 * specified offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		chars	a character array
 * @param		start	the starting offset
 * @param		length	the number of characters
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code length < 0, start < 0,
 *				start + length > chars.length, index < 0} or
 *				{@code index > length()}
 * @exception	NullPointerException when chars is null
 */
public synchronized StringBuffer insert(int index, char[] chars, int start, int length) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (0 <= index && index <= currentLength) {
		if (start >= 0 && 0 <= length && length <= chars.length - start) {
			move(length, index);
			
			if (String.enableCompression) {
				// Check if the StringBuffer is compressed
				if (count >= 0 && String.compressible(chars, start, length)) {
					String.compress(chars, start, value, index, length);
					
					count = currentLength + length;
					
					return this;
				} else {
					// Check if the StringBuffer is compressed
					if (count >= 0) {
						decompress(value.length);
					}
					
					String.decompressedArrayCopy(chars, start, value, index, length);
					
					count = (currentLength + length) | uncompressedBit;
					
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

synchronized StringBuffer insert(int index, char[] chars, int start, int length, boolean compressed) {
	int currentLength = lengthInternalUnsynchronized();
	
	move(length, index);
	
	if (String.enableCompression) {
		// Check if the StringBuffer is compressed
		if (count >= 0 && compressed) {
			String.compressedArrayCopy(chars, start, value, index, length);
			
			count = currentLength + length;
			
			return this;
		} else {
			if (count >= 0) {
				decompress(value.length);
			}
			
			String.decompressedArrayCopy(chars, start, value, index, length);
			
			count = (currentLength + length) | uncompressedBit;
			
			return this;
		}
	} else {
		String.decompressedArrayCopy(chars, start, value, index, length);
		
		count = currentLength + length;
		
		return this;
	}
}

/**
 * Inserts the character at the specified offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		ch	the character to insert
 * @return		this StringBuffer
 *
 * @exception	IndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 */
public synchronized StringBuffer insert(int index, char ch) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (0 <= index && index <= currentLength) {
		move(1, index);
		
		if (String.enableCompression ) {
			// Check if the StringBuffer is compressed
			if (count >= 0 && ch <= 255) {
				helpers.putByteInArrayByIndex(value, index, (byte) ch);
				
				count = currentLength + 1;
				
				return this;
			} else {
				// Check if the StringBuffer is compressed
				if (count >= 0) {
					decompress(value.length);
				}
				
				value[index] = ch;
				
				count = (currentLength + 1) | uncompressedBit;
				
				return this;
			}
		} else {
			value[index] = ch;
			
			count = currentLength + 1;
			
			return this;
		}
	} else {
		throw new StringIndexOutOfBoundsException(index);
	}
}

/**
 * Inserts the string representation of the specified double at the specified
 * offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		value	the double to insert
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 */
public StringBuffer insert(int index, double value) {
	return insert(index, String.valueOf(value));
}

/**
 * Inserts the string representation of the specified float at the specified
 * offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		value	the float to insert
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 */
public StringBuffer insert(int index, float value) {
	return insert(index, String.valueOf(value));
}

/**
 * Inserts the string representation of the specified integer at the specified
 * offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		value	the integer to insert
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 */
public StringBuffer insert(int index, int value) {
	return insert(index, Integer.toString(value));
}

/**
 * Inserts the string representation of the specified long at the specified
 * offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		value	the long to insert
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 */
public StringBuffer insert(int index, long value) {
	return insert(index, Long.toString(value));
}

/**
 * Inserts the string representation of the specified object at the specified
 * offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		value	the object to insert
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 */
public StringBuffer insert(int index, Object value) {
	return insert(index, String.valueOf(value));
}

/**
 * Inserts the string at the specified offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		string	the string to insert
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 */
public synchronized StringBuffer insert(int index, String string) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (0 <= index && index <= currentLength) {
		if (string == null) {
			string = "null"; //$NON-NLS-1$
		}
		
		int stringLength = string.lengthInternal();
		
		move(stringLength, index);
		
		if (String.enableCompression) {
			// Check if the StringBuffer is compressed
			if (count >= 0 && string.isCompressed()) {
				string.getBytes(0, stringLength, value, index);
				
				count = currentLength + stringLength;
				
				return this;
			} else {
				// Check if the StringBuffer is compressed
				if (count >= 0) {
					decompress(value.length);
				}
				
				string.getCharsNoBoundChecks(0, stringLength, value, index);
				
				count = (currentLength + stringLength) | uncompressedBit;
				
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
 * offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		value	the boolean to insert
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 */
public StringBuffer insert(int index, boolean value) {
	return insert(index, String.valueOf(value));
}

/**
 * Answers the size of this StringBuffer.
 *
 * @return		the number of characters in this StringBuffer
 */
public synchronized int length() {
	return lengthInternalUnsynchronized();
}

/**
 * Answers the size of this StringBuffer. This is an unsynchronized private method and is meant to be called only
 * from methods of this class which have already synchronized on the this StringBuffer object.
 *
 * The JIT compiler will take special precaution to avoid generating HCR guards for calls to this method.
 * @return the number of characters in this StringBuffer
 */
private int lengthInternalUnsynchronized() {
	if (String.enableCompression) {
		// Check if the StringBuffer is compressed
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
	int currentLength = lengthInternalUnsynchronized();
	int currentCapacity = capacityInternal();
	
	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		int newLength;
		
		if (currentCapacity - currentLength >= size) {
			// Check if the StringBuffer is not shared
			if (capacity >= 0) {
				String.compressedArrayCopy(value, index, value, index + size, currentLength - index);
				
				return;
			}
			
			newLength = currentCapacity;
		} else {
			newLength = Integer.max(currentLength + size, (currentCapacity << 1) + 2);
		}
		
		char[] newData = new char[(newLength + 1) / 2];
		
		String.compressedArrayCopy(value, 0, newData, 0, index);
		String.compressedArrayCopy(value, index, newData, index + size, currentLength - index);
		
		value = newData;
		
		capacity = newLength;
	} else {
		int newLength;
		
		if (currentCapacity - currentLength >= size) {
			// Check if the StringBuffer is not shared
			if (capacity >= 0) {
				String.decompressedArrayCopy(value, index, value, index + size, currentLength - index);
				
				return;
			}
			
			newLength = currentCapacity;
		} else {
			newLength = Integer.max(currentLength + size, (currentCapacity << 1) + 2);
		}
		
		char[] newData = new char[newLength];
		
		String.decompressedArrayCopy(value, 0, newData, 0, index);
		String.decompressedArrayCopy(value, index, newData, index + size, currentLength - index);
		
		value = newData;
		
		capacity = newLength;
	}
}

/**
 * Replace a range of characters with the characters in the specified String.
 *
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @param		string	a String
 * @return		this StringBuffer
 *
 * @exception	StringIndexOutOfBoundsException when {@code start < 0} or
 *				{@code start > end}
 */
public synchronized StringBuffer replace(int start, int end, String string) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (String.enableCompression) {
		// Check if the StringBuffer is compressed
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
						// Check if the StringBuffer is not shared
						if (capacity >= 0) {
							String.compressedArrayCopy(value, end, value, start + size, currentLength - end);
						} else {
							char[] newData = new char[value.length];
							
							String.compressedArrayCopy(value, 0, newData, 0, start);
							String.compressedArrayCopy(value, end, newData, start + size, currentLength - end);
							
							value = newData;
							
							capacity = capacity & ~sharedBit;
						}
					} else if (difference < 0) {
						move(-difference, end);
					} else if (capacity < 0) {
						value = value.clone();
						
						capacity = capacity & ~sharedBit;
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
			// Check if the StringBuffer is compressed
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
						// Check if the StringBuffer is not shared
						if (capacity >= 0) {
							String.decompressedArrayCopy(value, end, value, start + size, currentLength - end);
						} else {
							char[] newData = new char[value.length];
						
							String.decompressedArrayCopy(value, 0, newData, 0, start);
							String.decompressedArrayCopy(value, end, newData, start + size, currentLength - end);	
							
							value = newData;
							
							capacity = capacity & ~sharedBit;
						}
					} else if (difference < 0) {
						move(-difference, end);
					} else if (capacity < 0) {
						value = value.clone();
						
						capacity = capacity & ~sharedBit;
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
					// Check if the StringBuffer is not shared
					if (capacity >= 0) {
						String.decompressedArrayCopy(value, end, value, start + size, currentLength - end);
					} else {
						char[] newData = new char[value.length];

						String.decompressedArrayCopy(value, 0, newData, 0, start);
						String.decompressedArrayCopy(value, end, newData, start + size, currentLength - end);	
						value = newData;
						
						capacity = capacity & ~sharedBit;
					}
				} else if (difference < 0) {
					move(-difference, end);
				} else if (capacity < 0) {
					value = value.clone();
					
					capacity = capacity & ~sharedBit;
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
 * Reverses the order of characters in this StringBuffer.
 *
 * @return		this StringBuffer
 */
public synchronized StringBuffer reverse() {
	int currentLength = lengthInternalUnsynchronized();
	
	if (currentLength < 2) {
		return this;
	}
	
	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		// Check if the StringBuffer is not shared
		if (capacity >= 0) {
			for (int i = 0, mid = currentLength / 2, j = currentLength - 1; i < mid; ++i, --j) {
				byte a = helpers.getByteFromArrayByIndex(value, i);
				byte b = helpers.getByteFromArrayByIndex(value, j);
				
				helpers.putByteInArrayByIndex(value, i, b);
				helpers.putByteInArrayByIndex(value, j, a);
			}
			
			return this;
		} else {
			char[] newData = new char[value.length];
			
			for (int i = 0, j = currentLength - 1; i < currentLength; ++i, --j) {
				helpers.putByteInArrayByIndex(newData, j, helpers.getByteFromArrayByIndex(value, i));
			}
			
			value = newData;
			
			capacity = capacity & ~sharedBit;
			
			return this;
		}
	} else {
		// Check if the StringBuffer is not shared
		if (capacity >= 0) {
			int end = currentLength - 1;
			
			char frontHigh = value[0];
			char endLow = value[end];
			boolean allowFrontSur = true, allowEndSur = true;
			for (int i = 0, mid = currentLength / 2; i < mid; i++, --end) {
				char frontLow = value[i + 1];
				char endHigh = value[end - 1];
				boolean surAtFront = false, surAtEnd = false;
				if (allowFrontSur && frontLow >= Character.MIN_LOW_SURROGATE && frontLow <= Character.MAX_LOW_SURROGATE && frontHigh >= Character.MIN_HIGH_SURROGATE && frontHigh <= Character.MAX_HIGH_SURROGATE) {
					surAtFront = true;
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
						value[end] = frontLow;
						value[end - 1] = frontHigh;
						value[i] = endHigh;
						value[i + 1] = endLow;
						frontHigh = value[i + 2];
						endLow = value[end - 2];
						i++;
						--end;
					} else {
						// neither surrogates
						value[end] = frontHigh;
						value[i] = endLow;
						frontHigh = frontLow;
						endLow = endHigh;
					}
				} else {
					if (surAtFront) {
						// surrogate only at the front
						value[end] = frontLow;
						value[i] = endLow;
						endLow = endHigh;
						allowFrontSur = false;
					} else {
						// surrogate only at the end
						value[end] = frontHigh;
						value[i] = endHigh;
						frontHigh = frontLow;
						allowEndSur = false;
					}
				}
			}
			if ((currentLength & 1) == 1 && (!allowFrontSur || !allowEndSur)) {
				value[end] = allowFrontSur ? endLow : frontHigh;
			}
		} else {
			char[] newData = new char[value.length];
			
			for (int i = 0, end = currentLength; i < currentLength; i++) {
				char high = value[i];

				if ((i + 1) < currentLength && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
					char low = value[i + 1];
					
					if (low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
						newData[--end] = low;
						i++;
					}
				}
				newData[--end] = high;
			}
			
			value = newData;
			
			capacity = capacity & ~sharedBit;
		}
		
		return this;
	}
}

/**
 * Sets the character at the specified offset in this StringBuffer.
 *
 * @param 		index	the zero-based index in this StringBuffer
 * @param		ch	the character
 *
 * @exception	IndexOutOfBoundsException when {@code index < 0} or
 *				{@code index >= length()}
 */
public synchronized void setCharAt(int index, char ch) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (0 <= index && index < currentLength) {
		if (String.enableCompression) {
			// Check if the StringBuffer is compressed
			if (count >= 0 && ch <= 255) {
				if (capacity < 0) {
					value = value.clone();
					
					capacity = capacity & ~sharedBit;
				}
				
				helpers.putByteInArrayByIndex(value, index, (byte) ch);
			} else {
				// Check if the StringBuffer is compressed
				if (count >= 0) {
					decompress(value.length);
				}

				if (capacity < 0) {
					value = value.clone();
					
					capacity = capacity & ~sharedBit;
				}
				
				value[index] = ch;
			}
		} else {
			if (capacity < 0) {
				value = value.clone();
				
				capacity = capacity & ~sharedBit;
			}

			value[index] = ch;
		}
	} else {
		throw new StringIndexOutOfBoundsException(index);
	}
}

/**
 * Sets the length of this StringBuffer to the specified length. If there
 * are more than length characters in this StringBuffer, the characters
 * at end are lost. If there are less than length characters in the
 * StringBuffer, the additional characters are set to {@code \\u0000}.
 *
 * @param		length	the new length of this StringBuffer
 *
 * @exception	IndexOutOfBoundsException when {@code length < 0}
 *
 * @see			#length
 */
public synchronized void setLength(int length) {
	int currentLength = lengthInternalUnsynchronized();
	int currentCapacity = capacityInternal();
	
	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		if (length > currentCapacity) {
			ensureCapacityImpl(length);
		} else if (length > currentLength) {
			for (int i = currentLength; i < length; ++i) {
				helpers.putByteInArrayByIndex(value, i, (byte) 0);
			}
		} else if (capacity < 0) {
			if (length < 0) {
				throw new IndexOutOfBoundsException();
			}
			
			char[] newData = new char[value.length];
			
			if (length > 0) {
				String.compressedArrayCopy(value, 0, newData, 0, length);
			}
			
			value = newData;
			
			capacity = capacity & ~sharedBit;
		} else if (length < 0) {
			throw new IndexOutOfBoundsException();
		}
	} else {
		if (length > currentCapacity) {
			ensureCapacityImpl(length);
		} else if (length > currentLength) {
			Arrays.fill(value, currentLength, length, (char) 0);
		} else if (capacity < 0) {
			if (length < 0) {
				throw new IndexOutOfBoundsException();
			}
			
			char[] newData = new char[value.length];
			
			if (length > 0) {
				String.decompressedArrayCopy(value, 0, newData, 0, length);
			}
			
			value = newData;
			
			capacity = capacity & ~sharedBit;
		} else if (length < 0) {
			throw new IndexOutOfBoundsException();
		}
	}
	
	if (String.enableCompression) {
		// Check if the StringBuffer is compressed
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
 * @exception	StringIndexOutOfBoundsException when {@code start < 0} or
 *				{@code start > length()}
 */
public synchronized String substring(int start) {
	int currentLength = lengthInternalUnsynchronized();

	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		if (0 <= start && start <= currentLength) {
			return new String(value, start, currentLength - start, true, false);
		}
	} else {
		if (0 <= start && start <= currentLength) {
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
 * @exception	StringIndexOutOfBoundsException when {@code start < 0, start > end} or
 *				{@code end > length()}
 */
public synchronized String substring(int start, int end) {
	int currentLength = lengthInternalUnsynchronized();
	
	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		if (0 <= start && start <= end && end <= currentLength) {
			return new String(value, start, end - start, true, false);
		}
	} else {
		if (0 <= start && start <= end && end <= currentLength) {
			return new String(value, start, end - start, false, false);
		}
	}
	
	throw new StringIndexOutOfBoundsException();
}

static void initFromSystemProperties(Properties props) {
	String prop = props.getProperty("java.lang.string.create.unique"); //$NON-NLS-1$
	TOSTRING_COPY_BUFFER_ENABLED = "true".equals(prop) || "StringBuffer".equals(prop); //$NON-NLS-1$ //$NON-NLS-2$
}

/**
 * Answers the contents of this StringBuffer.
 *
 * @return		a String containing the characters in this StringBuffer
 */
public synchronized String toString () {
	int currentLength = lengthInternalUnsynchronized();
	int currentCapacity = capacityInternal();
	
	if (false == TOSTRING_COPY_BUFFER_ENABLED) {
		int wasted = currentCapacity - currentLength;
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
	
	capacity = capacity | sharedBit;
	
	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		return new String (value, 0, currentLength, true);
	} else {
		return new String (value, 0, currentLength, false);
	}
}

private synchronized void writeObject(ObjectOutputStream stream) throws IOException {
	int currentLength = lengthInternalUnsynchronized();
	
	ObjectOutputStream.PutField pf = stream.putFields();
	
	pf.put("count", currentLength); //$NON-NLS-1$  

	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		char[] newData = new char[currentLength];
		
		String.decompress(value, 0, newData, 0, currentLength);
		
		pf.put("value", newData); //$NON-NLS-1$
	} else {
		pf.put("value", value); //$NON-NLS-1$
	}
	
	pf.put("shared", false); //$NON-NLS-1$
	
	stream.writeFields();
}

private void readObject(ObjectInputStream stream) throws IOException, ClassNotFoundException {
	ObjectInputStream.GetField gf = stream.readFields();
	
	int streamCount = gf.get("count", 0); //$NON-NLS-1$

	char[] streamValue = (char[]) gf.get("value", null); //$NON-NLS-1$
	
	if (streamCount > streamValue.length) {
		throw new InvalidObjectException(com.ibm.oti.util.Msg.getString("K0199")); //$NON-NLS-1$
	} 
	
	if (String.enableCompression) {
		if (String.compressible(streamValue, 0, streamValue.length)) {
			if (streamValue.length == Integer.MAX_VALUE) {
				value = new char[(streamValue.length / 2) + 1];
			} else {
				value = new char[(streamValue.length + 1) / 2];
			}
			
			String.compress(streamValue, 0, value, 0, streamValue.length);
			
			count = streamCount;
			
			capacity = streamValue.length;
		} else {
			value = new char[streamValue.length];
			
			System.arraycopy(streamValue, 0, value, 0, streamValue.length);
			
			count = streamCount | uncompressedBit;

			capacity = streamValue.length;
			
			String.initCompressionFlag();
		}
	} else {
		value = new char[streamValue.length];
		
		System.arraycopy(streamValue, 0, value, 0, streamValue.length);
		
		count = streamCount;
		
		capacity = streamValue.length;
	}
}

/**
 * Adds the specified StringBuffer to the end of this StringBuffer.
 *
 * @param		buffer	the StringBuffer
 * @return		this StringBuffer
 * 
 * @since 1.4
 */
public synchronized StringBuffer append(StringBuffer buffer) {
	if (buffer == null) {
		return append((String)null);
	} else {
		synchronized (buffer) {
			// Check if the StringBuffer is compressed
			if (String.enableCompression && buffer.count >= 0) {
				return append(buffer.value, 0, buffer.lengthInternalUnsynchronized(), true);
			} else {
				return append(buffer.value, 0, buffer.lengthInternalUnsynchronized(), false);
			}
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
 * @exception	IndexOutOfBoundsException when {@code start < 0, start > end} or
 *				{@code end > length()}
 * 
 * @since 1.4
 */
public CharSequence subSequence(int start, int end) {
	return substring(start, end);
}

/**
 * Searches in this StringBuffer for the first index of the specified character. The
 * search for the character starts at the beginning and moves towards the
 * end.
 *
 * @param		string	the string to find
 * @return		the index in this StringBuffer of the specified character, -1 if the
 *				character isn't found
 *
 * @see			#lastIndexOf(String)
 * 
 * @since 1.4
 */
public int indexOf(String string) {
	return indexOf(string, 0);
}

/**
 * Searches in this StringBuffer for the index of the specified character. The
 * search for the character starts at the specified offset and moves towards
 * the end.
 *
 * @param		subString		the string to find
 * @param		start	the starting offset
 * @return		the index in this StringBuffer of the specified character, -1 if the
 *				character isn't found
 *
 * @see			#lastIndexOf(String,int)
 * 
 * @since 1.4
 */
public synchronized int indexOf(String subString, int start) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (start < 0) {
		start = 0;
	}
	
	int subStringLength = subString.lengthInternal();
	
	if (subStringLength > 0) {
		if (subStringLength + start > currentLength) {
			return -1;
		}
		
		char firstChar = subString.charAtInternal(0);
		
		// Check if the StringBuffer is compressed
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
					if (value[i] == firstChar) {
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
				
				while (++o2 < subStringLength && value[++o1] == subString.charAtInternal(o2));
				
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
 * Searches in this StringBuffer for the last index of the specified character. The
 * search for the character starts at the end and moves towards the beginning.
 *
 * @param		string	the string to find
 * @return		the index in this StringBuffer of the specified character, -1 if the
 *				character isn't found
 *
 * @see			#indexOf(String)
 * 
 * @since 1.4
 */
public synchronized int lastIndexOf(String string) {
	int currentLength = lengthInternalUnsynchronized();
	
	return lastIndexOf(string, currentLength);
}

/**
 * Searches in this StringBuffer for the index of the specified character. The
 * search for the character starts at the specified offset and moves towards
 * the beginning.
 *
 * @param		subString		the string to find
 * @param		start	the starting offset
 * @return		the index in this StringBuffer of the specified character, -1 if the
 *				character isn't found
 *
 * @see			#indexOf(String,int)
 * 
 * @since 1.4
 */
public synchronized int lastIndexOf(String subString, int start) {
	int currentLength = lengthInternalUnsynchronized();
	
	int subStringLength = subString.lengthInternal();
	
	if (subStringLength <= currentLength && start >= 0) {
		if (subStringLength > 0) {
			if (start > currentLength - subStringLength) {
				start = currentLength - subStringLength;
			}
			
			char firstChar = subString.charAtInternal(0);
			
			// Check if the StringBuffer is compressed
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
						if (value[i] == firstChar) {
							found = true;
							break;
						}
					}
					
					if (!found) {
						return -1;
					}
					
					int o1 = i;
					int o2 = 0;
					
					while (++o2 < subStringLength && value[++o1] == subString.charAtInternal(o2));
					
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

/*
 * Return the underlying buffer and set the shared flag.
 *
 */
char[] shareValue() {
	capacity = capacity | sharedBit;
	
	return value;
}

boolean isCompressed() {
	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		return true;
	} else {
		return false;
	}
}

/**
 * Constructs a new StringBuffer containing the characters in
 * the specified CharSequence and the default capacity.
 *
 * @param		sequence	the initial contents of this StringBuffer
 * @exception	NullPointerException when sequence is null
 * 
 * @since 1.5
 */
public StringBuffer(CharSequence sequence) {
	int size = sequence.length();
	
	if (size < 0) {
		size = 0;
	}
	
	int newLength = INITIAL_SIZE + size;
	
	if (String.enableCompression) {
		value = new char[(newLength + 1) / 2];
	} else {
		value = new char[newLength];
	}

	capacity = newLength;
	
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
				value = new char[newLength];
				
				count = size | uncompressedBit;
				
				for (int i = 0; i < size; ++i) {
					value[i] = sequence.charAt(i);
				}
				
				String.initCompressionFlag();
			}
		} else {			
			count = size;
			
			for (int i = 0; i < size; ++i) {
				value[i] = sequence.charAt(i);
			}
		}
	}
}

/**
 * Adds the specified CharSequence to the end of this StringBuffer.
 *
 * @param		sequence	the CharSequence
 * @return		this StringBuffer
 * 
 * @since 1.5
 */
public synchronized StringBuffer append(CharSequence sequence) {
	if (sequence == null) {
		return append(String.valueOf(sequence));
	} else if (sequence instanceof String) {
		return append((String)sequence);
	} else if (sequence instanceof StringBuffer) {
		return append((StringBuffer)sequence);
	} else {
		int currentLength = lengthInternalUnsynchronized();
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
			
			// Check if the StringBuffer is compressed
			if (count >= 0 && isCompressed) {
				if (newLength > currentCapacity) {
					ensureCapacityImpl(newLength);
				}
				
				for (int i = 0; i < sequence.length(); ++i) {
					helpers.putByteInArrayByIndex(value, currentLength + i, (byte) sequence.charAt(i));
				}
				
				count = newLength;
			} else {
				// Check if the StringBuffer is compressed
				if (count >= 0) {
					decompress(newLength);
				}
				
				if (newLength > currentCapacity) {
					ensureCapacityImpl(newLength);
				}
				
				for (int i = 0; i < sequence.length(); ++i) {
					value[currentLength + i] = sequence.charAt(i);
				}
				
				count = newLength | uncompressedBit;
			}
		} else {
			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}
			
			for (int i = 0; i < sequence.length(); ++i) {
				value[currentLength + i] = sequence.charAt(i);
			}
			
			count = newLength;
		}
		
		return this;
	}
}

/**
 * Adds the specified CharSequence to the end of this StringBuffer.
 *
 * @param		sequence	the CharSequence
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @return		this StringBuffer
 * 
 * @exception	IndexOutOfBoundsException when {@code start < 0, start > end} or
 *				{@code end > length()}
 * 
 * @since 1.5
 */
public synchronized StringBuffer append(CharSequence sequence, int start, int end) {
	if (sequence == null) {
		return append(String.valueOf(sequence), start, end);
	} else if (sequence instanceof String) {
		return append(((String)sequence).substring(start, end));
	} else if (start >= 0 && end >= 0 && start <= end && end <= sequence.length()) {
		if (sequence instanceof StringBuffer) {
			synchronized (sequence) {
				StringBuffer buffer = (StringBuffer) sequence;
				
				// Check if the StringBuffer is compressed
				if (String.enableCompression && buffer.count >= 0) {
					return append(buffer.value, start, end - start, true);
				} else {
					return append(buffer.value, start, end - start, false);
				}
			}
		} else if (sequence instanceof StringBuilder) {
			synchronized (sequence) {
				StringBuilder builder = (StringBuilder) sequence;
				
				if (String.enableCompression && builder.isCompressed()) {
					return append(builder.getValue(), start, end - start, true);
				} else {
					return append(builder.getValue(), start, end - start, false);
				}
			}
		} else {
			int currentLength = lengthInternalUnsynchronized();
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
				
				// Check if the StringBuffer is compressed
				if (count >= 0 && isCompressed) {
					if (newLength > currentCapacity) {
						ensureCapacityImpl(newLength);
					}
					
					for (int i = 0; i < end - start; ++i) {
						helpers.putByteInArrayByIndex(value, currentLength + i, (byte) sequence.charAt(start + i));
					}
					
					count = newLength;
				} else {
					// Check if the StringBuffer is compressed
					if (count >= 0) {
						decompress(newLength);
					}
					
					if (newLength > currentCapacity) {
						ensureCapacityImpl(newLength);
					}
					
					for (int i = 0; i < end - start; ++i) {
						value[currentLength + i] = sequence.charAt(start + i);
					}
					
					count = newLength | uncompressedBit;
				}
			} else {
				if (newLength > currentCapacity) {
					ensureCapacityImpl(newLength);
				}
				
				for (int i = 0; i < end - start; ++i) {
					value[currentLength + i] = sequence.charAt(start + i);
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
 * Inserts the CharSequence at the specified offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		sequence	the CharSequence to insert
 * @return		this StringBuffer
 *
 * @exception	IndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}
 * 
 * @since 1.5
 */
public synchronized StringBuffer insert(int index, CharSequence sequence) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (index >= 0 && index <= currentLength) {
		if (sequence == null) {
			return insert(index, String.valueOf(sequence));
		} else if (sequence instanceof String) {
			return insert(index, (String) sequence);
		} else if (sequence instanceof StringBuffer) {
			synchronized(sequence) {
				StringBuffer buffer = (StringBuffer) sequence;
				
				// Check if the StringBuffer is compressed
				if (String.enableCompression && buffer.count >= 0) {
					return insert(index, buffer.value, 0, buffer.lengthInternalUnsynchronized(), true);
				} else {
					return insert(index, buffer.value, 0, buffer.lengthInternalUnsynchronized(), false);
				}
			}
		} else if (sequence instanceof StringBuilder) {
			synchronized (sequence) {
				StringBuilder builder = (StringBuilder) sequence;
				
				if (String.enableCompression && builder.isCompressed()) {
					return insert(index, builder.getValue(), 0, builder.lengthInternal(), true);
				} else {
					return insert(index, builder.getValue(), 0, builder.lengthInternal(), false);
				}
			}
		} else {
			int sequneceLength = sequence.length();
			
			if (sequneceLength > 0) {
				move(sequneceLength, index);
				
				int newLength = currentLength + sequneceLength;
				
				if (String.enableCompression) {
					boolean isCompressed = true;
					
					for (int i = 0; i < sequneceLength; ++i) {
						if (sequence.charAt(i) > 255) {
							isCompressed = false;
							
							break;
						}
					}
					
					// Check if the StringBuffer is compressed
					if (count >= 0 && isCompressed) {
						for (int i = 0; i < sequneceLength; ++i) {
							helpers.putByteInArrayByIndex(value, index + i, (byte) sequence.charAt(i));
						}
						
						count = newLength;
						
						return this;
					} else {
						// Check if the StringBuffer is compressed
						if (count >= 0) {
							decompress(value.length);
						}
						
						for (int i = 0; i < sequneceLength; ++i) {
							value[index + i] = sequence.charAt(i);
						}
						
						count = newLength | uncompressedBit;
					}
				} else {
					for (int i = 0; i < sequneceLength; ++i) {
						value[index + i] = sequence.charAt(i);
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
 * Inserts the CharSequence at the specified offset in this StringBuffer.
 *
 * @param		index	the index at which to insert
 * @param		sequence	the CharSequence to insert
 * @param		start	the offset of the first character
 * @param		end	the offset one past the last character
 * @return		this StringBuffer
 *
 * @exception	IndexOutOfBoundsException when {@code index < 0} or
 *				{@code index > length()}, or when {@code start < 0, start > end} or
 *				{@code end > length()}
 * 
 * @since 1.5
 */
public synchronized StringBuffer insert(int index, CharSequence sequence, int start, int end) {
	int currentLength = lengthInternalUnsynchronized();
	
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
					
					// Check if the StringBuffer is compressed
					if (String.enableCompression && buffer.count >= 0) {
						return insert(index, buffer.value, start, end - start, true);
					} else {
						return insert(index, buffer.value, start, end - start, false);
					}
				}
			} else if (sequence instanceof StringBuilder) {
				synchronized(sequence) {
					StringBuilder builder = (StringBuilder) sequence;
					
					if (String.enableCompression && builder.isCompressed()) {
						return insert(index, builder.getValue(), start, end - start, true);
					} else {
						return insert(index, builder.getValue(), start, end - start, false);
					}
				}
			} else {
				int sequenceLength = end - start;
				
				if (sequenceLength > 0) {
					move(sequenceLength, index);
					
					int newLength = currentLength + sequenceLength;
					
					if (String.enableCompression) {
						boolean isCompressed = true;
						
						for (int i = 0; i < sequenceLength; ++i) {
							if (sequence.charAt(start + i) > 255) {
								isCompressed = false;
								
								break;
							}
						}
						
						// Check if the StringBuffer is compressed
						if (count >= 0 && isCompressed) {
							for (int i = 0; i < sequenceLength; ++i) {
								helpers.putByteInArrayByIndex(value, index + i, (byte) sequence.charAt(start + i));
							}
							
							count = newLength;
							
							return this;
						} else {
							// Check if the StringBuffer is compressed
							if (count >= 0) {
								decompress(value.length);
							}
							
							for (int i = 0; i < sequenceLength; ++i) {
								value[index + i] = sequence.charAt(start + i);
							}
							
							count = newLength | uncompressedBit;
						}
					} else {
						for (int i = 0; i < sequenceLength; ++i) {
							value[index + i] = sequence.charAt(start + i);
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
 * 
 * @since		1.5
 */
public synchronized void trimToSize() {
	int currentLength = lengthInternalUnsynchronized();
	int currentCapacity = capacityInternal();
	
	// Check if the StringBuffer is compressed
	if (String.enableCompression && count >= 0) {
		// Check if the StringBuffer is not shared
		if (capacity >= 0 && currentCapacity != currentLength) {
			char[] newData = new char[(currentLength + 1) / 2];
			
			String.compressedArrayCopy(value, 0, newData, 0, currentLength);
			
			value = newData;
			
			capacity = currentLength;
		}
	} else {
		// Check if the StringBuffer is not shared
		if (capacity >= 0 && currentCapacity != currentLength) {
			char[] newData = new char[currentLength];
		
			String.decompressedArrayCopy(value, 0, newData, 0, currentLength);	
			value = newData;
			
			capacity = currentLength;
		}
	}
}

/**
 * Returns the Unicode character at the given point.
 * 
 * @param 		index		the character index
 * @return		the Unicode character value at the index
 * 
 * @since		1.5
 */
public synchronized int codePointAt(int index) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (index >= 0 && index < currentLength) {
		// Check if the StringBuffer is compressed
		if (String.enableCompression && count >= 0) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index));
		} else {
			int high = value[index];
			
			if ((index + 1) < currentLength && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
				int low = value[index + 1];
				
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
 * 
 * @since		1.5
 */
public synchronized int codePointBefore(int index) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (index > 0 && index <= currentLength) {
		// Check if the StringBuffer is compressed
		if (String.enableCompression && count >= 0) {
			return helpers.byteToCharUnsigned(helpers.getByteFromArrayByIndex(value, index - 1));
		} else {
			int low = value[index - 1];
			
			if (index > 1 && low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
				int high = value[index - 2];
				
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
 * 
 * @since		1.5
 */
public synchronized int codePointCount(int start, int end) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (start >= 0 && start <= end && end <= currentLength) {
		// Check if the StringBuffer is compressed
		if (String.enableCompression && count >= 0) {
			return end - start;
		} else {
			int count = 0;
			
			for (int i = start; i < end; ++i) {
				int high = value[i];
				
				if (i + 1 < end && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
					int low = value[i + 1];
					
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
 * Returns the index of the code point that was offset by {@code codePointCount}.
 * 
 * @param 		start			the position to offset
 * @param		codePointCount	the code point count
 * @return		the offset index
 * 
 * @since		1.5
 */
public synchronized int offsetByCodePoints(int start, int codePointCount) {
	int currentLength = lengthInternalUnsynchronized();
	
	if (start >= 0 && start <= currentLength) {
		// Check if the StringBuffer is compressed
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

					int high = value[index];

					if ((index + 1) < currentLength && high >= Character.MIN_HIGH_SURROGATE && high <= Character.MAX_HIGH_SURROGATE) {
						int low = value[index + 1];

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

					int low = value[index - 1];

					if (index > 1 && low >= Character.MIN_LOW_SURROGATE && low <= Character.MAX_LOW_SURROGATE) {
						int high = value[index - 2];

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
 * 
 * @since 1.5
 */
public synchronized StringBuffer appendCodePoint(int codePoint) {
	if (codePoint >= 0) {
		if (codePoint < 0x10000) {
			return append((char)codePoint);
		} else if (codePoint < 0x110000) {
			// Check if the StringBuffer is compressed
			if (String.enableCompression && count >= 0) {
				decompress(value.length);
			}
			
			int currentLength = lengthInternalUnsynchronized();
			int currentCapacity = capacityInternal();

			int newLength = currentLength + 2;

			if (newLength > currentCapacity) {
				ensureCapacityImpl(newLength);
			}

			codePoint -= 0x10000;

			value[currentLength] = (char) (Character.MIN_HIGH_SURROGATE + (codePoint >> 10));
			value[currentLength + 1] = (char) (Character.MIN_LOW_SURROGATE + (codePoint & 0x3ff));

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
 * Returns the character array for this StringBuffer.
 */
char[] getValue() {
	return value;
}

}
