/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.pointer.helper;

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.*;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.types.*;

public class J9ObjectHelper 
{
	private static int cacheSize = 32;
	private static J9ObjectPointer[] keys;
	private static J9ClassPointer[] values;
	private static int[] counts;
	private static long probes;
	private static long hits;
	
	/**
	 * Determines whether java/lang/String is backed by a byte[] array if true or char[] if false.
	 */
	private static Boolean isStringBackedByByteArray = null;
	
	static {
		initializeCache();
	}
	
	/**
	 * Returns TRUE if an object is indexable, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is indexable, FALSE otherwise
	 * @throws CorruptDataException 
	 */	
	public static boolean isIndexable(J9ObjectPointer objPointer) throws CorruptDataException
	{		
		return ObjectModel.isIndexable(objPointer);
	}
	
	public static U32 flags(J9ObjectPointer objPointer) throws CorruptDataException
	{		
		if(J9BuildFlags.interp_flagsInClassSlot) {
			long bitmask = (J9Consts.J9_REQUIRED_CLASS_ALIGNMENT - 1);
			J9ClassPointer clazz = objPointer.clazz();
			
			return new U32(UDATA.cast(clazz).bitAnd(bitmask));
		} else {
			throw new UnsupportedOperationException("Only builds with interp_flagsInClassSlot currently supported.");
		}
	}

	public static J9ClassPointer clazz(J9ObjectPointer objPointer) throws CorruptDataException
	{
		J9ClassPointer classPointer = checkClassCache(objPointer);
		if(null == classPointer) {
			if(J9BuildFlags.interp_flagsInClassSlot) {
				long bitmask = (~(J9Consts.J9_REQUIRED_CLASS_ALIGNMENT - 1));
				UDATA clazz = UDATA.cast(objPointer.clazz());
				
				classPointer = J9ClassPointer.cast(clazz.bitAnd(bitmask));
			} else {
				classPointer = objPointer.clazz();
			}
			setClassCache(objPointer, classPointer);
		}
		return classPointer;
	}

	
	public static UDATA monitor(J9ObjectPointer objPointer) throws CorruptDataException 
	{
		if(J9BuildFlags.thr_lockNursery) {
			// TODO : lockNursery support
			throw new UnsupportedOperationException("lockNursery not supported yet");			
		} else {
			// TODO : non-lockNursery support
			throw new UnsupportedOperationException("need a non-lockNursery blob");
			//return new UDATA(getUDATAAtOffset(J9Object._monitorOffset_));
		}
	}
	
	/**
	 * Return the name of this J9Object's class
	 */
	public static String getClassName(J9ObjectPointer objPointer) throws CorruptDataException 
	{
		return J9ClassHelper.getName(clazz(objPointer));
	}
	
	public static String getJavaName(J9ObjectPointer objPointer) throws CorruptDataException 
	{
		return J9ClassHelper.getJavaName(clazz(objPointer));
	}
	

	public static String stringValue(J9ObjectPointer objPointer) throws CorruptDataException 
	{
		if (!J9ObjectHelper.getClassName(objPointer).equals("java/lang/String")) {
			throw new IllegalArgumentException();
		}
		
		// No synchronization needed here because the type of java/lang/String.value is immutable
		if (isStringBackedByByteArray == null) {			
			try {
				getObjectField(objPointer, getFieldOffset(objPointer, "value", "[B"));

				isStringBackedByByteArray = new Boolean(true);
			} catch (NoSuchElementException e) {
				getObjectField(objPointer, getFieldOffset(objPointer, "value", "[C"));

				isStringBackedByByteArray = new Boolean(false);
			}
		}
		
		J9ObjectPointer valueObject = 
			isStringBackedByByteArray.booleanValue() ? 
				getObjectField(objPointer, getFieldOffset(objPointer, "value", "[B")) :
				getObjectField(objPointer, getFieldOffset(objPointer, "value", "[C"));
		
		if (valueObject.isNull()) {
			return "<Uninitialized String>";
		}
		
		int stringLength = 0;
		
		boolean isStringCompressed = false;

		J9IndexableObjectPointer valueArray = J9IndexableObjectPointer.cast(valueObject);
		
		if (isStringBackedByByteArray.booleanValue()) {
			byte coder = getByteField(objPointer, getFieldOffset(objPointer, "coder", "B"));

			isStringCompressed = coder == 0;

			byte[] value = (byte[]) J9IndexableObjectHelper.getData(valueArray);

			stringLength = value.length >> coder;
		} else {
			stringLength = getIntField(objPointer, getFieldOffset(objPointer, "count", "I"));

			boolean enableCompression = getBooleanField(objPointer, getFieldOffset(objPointer, "enableCompression", "Z"));

			if (enableCompression) {
				if (stringLength >= 0) {
					isStringCompressed = true;
				} else {
					stringLength = stringLength & 0x7FFFFFFF;
				}
			}
		}

		char[] charValue = new char[stringLength];
		
		if (isStringBackedByByteArray.booleanValue()) {
			byte[] value = (byte[]) J9IndexableObjectHelper.getData(valueArray);

			if (isStringCompressed) {
				for (int i = 0; i < stringLength; ++i) {
					charValue[i] = byteToCharUnsigned(getByteFromArrayByIndex(value, i));
				}
			} else {
				for (int i = 0; i < stringLength; ++i) {
					charValue[i] = getCharFromArrayByIndex(value, i);
				}
			}
		} else {
			char[] value = (char[]) J9IndexableObjectHelper.getData(valueArray);
			
			if (isStringCompressed) {
				for (int i = 0; i < stringLength; ++i) {
					charValue[i] = byteToCharUnsigned(getByteFromArrayByIndex(value, i));
				}
			} else {
				for (int i = 0; i < stringLength; ++i) {
					charValue[i] = getCharFromArrayByIndex(value, i);
				}
			}
		}
		
		return new String(charValue);		
	}
	
	/**
	 * Returns a String field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the String value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static String getStringField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException 
	{
		J9ObjectPointer stringObject = getObjectField(objPointer, offset);
		if(stringObject.isNull()) {
			return null;
		}
		return J9ObjectHelper.stringValue(stringObject);
	}

	/**
	 * Returns an int field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the int value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static int getIntField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException 
	{
		I32Pointer pointer;
		if (offset.isStatic()) {
			pointer = I32Pointer.cast(J9ObjectHelper.clazz(objPointer).ramStatics().addOffset(offset.getOffsetOrAddress()));
		} else {
			pointer = I32Pointer.cast(objPointer.addOffset(offset.getOffsetOrAddress()).addOffset(ObjectModel.getHeaderSize(objPointer)));
		}
		return pointer.at(0).intValue();
	}
	
	/**
	 * Returns a short field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the short value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static short getShortField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException 
	{
		return (short)(getIntField(objPointer, offset) & 0xFFFF);
	}
	
	/**
	 * Returns a float field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the float value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static float getFloatField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException
	{
		int data = getIntField(objPointer, offset);
		return Float.intBitsToFloat(data);
	}
	
	/**
	 * Returns a double field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the float value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static double getDoubleField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException
	{
		long data = getLongField(objPointer, offset);
		return Double.longBitsToDouble(data);
	}
	
	/**
	 * Returns a char field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the char value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static char getCharField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException 
	{
		return (char)(getIntField(objPointer, offset) & 0xFFFF);
	}
	
	/**
	 * Returns an byte field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the byte value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static byte getByteField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException
	{
		return (byte)(getIntField(objPointer, offset) & 0xFF); 
	}
	
	/**
	 * Returns an boolean field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the boolean value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static boolean getBooleanField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException 
	{
		return (getIntField(objPointer, offset) != 0); 
	}
	
	/**
	 * Returns an long field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the long value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static long getLongField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException 
	{
		I64Pointer pointer;
		if (offset.isStatic()) {
			pointer = I64Pointer.cast(J9ObjectHelper.clazz(objPointer).ramStatics().addOffset(offset.getOffsetOrAddress()));
		} else {
			pointer = I64Pointer.cast(objPointer.addOffset(offset.getOffsetOrAddress()).addOffset(ObjectModel.getHeaderSize(objPointer)));
		}
		 return pointer.at(0).longValue();
	}
	
	/**
	 * Returns an Object field from the object or its super classes.  The field may be
	 * a static field or an instance field.
	 * 
	 * @param offset the offset of the field to return
	 * @return the J9Object value of the field
	 * @throws CorruptDataException if there is a problem reading the underlying data from the core file
	 */
	public static J9ObjectPointer getObjectField(J9ObjectPointer objPointer, J9ObjectFieldOffset offset) throws CorruptDataException 
	{
		UDATAPointer pointer;
		if (offset.isStatic()) {
			pointer = J9ObjectHelper.clazz(objPointer).ramStatics().addOffset(offset.getOffsetOrAddress());
			return J9ObjectPointer.cast(pointer.at(0));
		} else {
			return ObjectReferencePointer.cast(objPointer.addOffset(ObjectModel.getHeaderSize(objPointer)).addOffset(offset.getOffsetOrAddress())).at(0);
		}
	}
	
	private static J9ObjectFieldOffset getFieldOffset(J9ObjectPointer objPointer, String name, String signature) throws CorruptDataException
	{
		J9ObjectFieldOffset result = J9ClassHelper.checkFieldOffsetCache(J9ObjectHelper.clazz(objPointer), name, signature);
		if (result == null) {
			result = readFieldOffset(objPointer, name, signature);
			J9ClassHelper.setFieldOffsetCache(J9ObjectHelper.clazz(objPointer), result, name, signature);
		}
		return result;
	}
	
	// FIXME: Probably want to cache the entire class hierarchy the 1st time through for any field
	private static J9ObjectFieldOffset readFieldOffset(J9ObjectPointer objPointer, String name, String signature) throws CorruptDataException
	{
		J9ClassPointer currentClass = J9ObjectHelper.clazz(objPointer);
		while (currentClass.notNull()) {
			Iterator<J9ObjectFieldOffset> fields = J9ClassHelper.getFieldOffsets(currentClass);
			while (fields.hasNext()) {
				J9ObjectFieldOffset field = (J9ObjectFieldOffset) fields.next();
				if (field.getName().equals(name) && field.getSignature().equals(signature)) {
					return field;
				}
			}
			currentClass = J9ClassHelper.superclass(currentClass);
		}
	
		throw new NoSuchElementException(String.format("No field named %s with signature %s in %s", name, signature, J9ObjectHelper.getClassName(objPointer)));
	}

	
	private static J9ClassPointer checkClassCache(J9ObjectPointer objPointer)
	{
		probes++;
		for(int i = 0; i < cacheSize; i++) {
			if(keys[i].equals(objPointer)) {
				hits++;
				counts[i]++;
				return values[i];
			}
		}
		return null;
	}
	
	private static void setClassCache(J9ObjectPointer objPointer, J9ClassPointer classPointer)
	{
		int min = counts[0];
		int minIndex = 0;
		for(int i = 1; i < cacheSize; i++) {
			if(counts[i] < min) {
				min = counts[i];
				minIndex = i;
			}
		}
		keys[minIndex] = objPointer;
		values[minIndex] = classPointer;
		counts[minIndex] = 1;
	}
	
	private static void initializeCache()
	{
		keys = new J9ObjectPointer[cacheSize];
		values = new J9ClassPointer[cacheSize];
		counts = new int[cacheSize];
		probes = 0;
		hits = 0;
		for(int i = 0; i < cacheSize; i++) {
			keys[i] = J9ObjectPointer.NULL;
		}
	}
	
	public static byte getByteFromArrayByIndex(Object obj, int index) {
		Class<?> clazz = obj.getClass();
		
		if (clazz == byte[].class) {
			return ((byte[]) obj)[index];
		} else if (clazz == char[].class) {
			char[] array = (char[]) obj;
			
			if (J9BuildFlags.env_littleEndian) {
				if ((index % 2) == 1) {
					return (byte) ((array[index / 2] & 0xFF00) >>> 8);
				} else {
					return (byte) ((array[index / 2] & 0x00FF));
				}
			} else {
				if ((index % 2) == 1) {
					return (byte) ((array[index / 2] & 0x00FF));
				} else {
					return (byte) ((array[index / 2] & 0xFF00) >>> 8);
				}
			}
		} else {
			throw new RuntimeException("Unknown array type for bit manipulation");
		}
	}

	private static char getCharFromArrayByIndex(Object obj, int index) {
		Class<?> clazz = obj.getClass();

		if (clazz == byte[].class) {
			index = index << 1;
			
			byte[] array = (byte[]) obj;
			
			if (J9BuildFlags.env_littleEndian) {
				return (char) ((byteToCharUnsigned(array[index + 1]) << 8) | (byteToCharUnsigned(array[index]) << 0));
			} else {
				return (char) ((byteToCharUnsigned(array[index + 1]) << 0) | (byteToCharUnsigned(array[index]) << 8));
			}
		} else if (clazz == char[].class) {
			return ((char[]) obj)[index];
		} else {
			throw new RuntimeException("Unknown array type for bit manipulation");
		}
	}
	
	private static char byteToCharUnsigned(byte b) {
		return (char) ((char) b & (char) 0x00FF);
	}
	
	public static void reportClassCacheStats()
	{
		double hitRate = (double)hits / (double)probes * 100.0;
		System.out.println("J9ObjectHelper probes: " + probes + " hit rate: " + hitRate + "%");
		initializeCache();
	}
}
