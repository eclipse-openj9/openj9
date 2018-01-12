/*[INCLUDE-IF Sidecar17] */
package com.ibm.oti.vm;
/*******************************************************************************
 * Copyright (c) 2010, 2017 IBM Corp. and others
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

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.util.Map;
import java.security.AccessController;
import java.security.PrivilegedAction;

/*[IF Sidecar19-SE]
import jdk.internal.misc.Unsafe;
import jdk.internal.reflect.CallerSensitive;
/*[ELSE]*/
import sun.reflect.CallerSensitive;
import sun.misc.Unsafe;
/*[ENDIF]*/

public class ORBVMHelpers {

	static Unsafe createUnsafe() {
		return (Unsafe)AccessController.doPrivileged (
		    new PrivilegedAction() {
			public Object run() {
			    try {
				return Unsafe.getUnsafe();
			    }catch (Throwable t){
				return null;
			    }
			}
		   }
		);
	}

	static Unsafe unsafe = createUnsafe();
	
	/**
	 * This native is a re-implementation of the JVM_LatestUserDefinedLoader.  
	 *
	 * It walks the stack to find the first user defined class loader from the top of stack.  
	 * The walk will terminate early and return null if it encounters a frame that has been 
	 * "marked" using the LUDCLMarkFrame() native.
	 *
	 * Returns the ClassLoader object or null if no user-defined loader is found
	 */
	@CallerSensitive
	public static native ClassLoader LatestUserDefinedLoader();
	
	private static native boolean is32Bit();

	private static native int getNumBitsInReferenceField();

	private static native int getNumBytesInReferenceField();

	private static native int getNumBitsInDescriptionWord();

	private static native int getNumBytesInDescriptionWord();

	private static native int getNumBytesInJ9ObjectHeader();
 
	private static native int getJ9ClassFromClass32(Class c);

	private static native int getTotalInstanceSizeFromJ9Class32(int j9clazz);

	private static native int getInstanceDescriptionFromJ9Class32(int j9clazz);

	private static native int getDescriptionWordFromPtr32(int descriptorPtr);

	private static native long getJ9ClassFromClass64(Class c);

	private static native long getTotalInstanceSizeFromJ9Class64(long j9clazz);

	private static native long getInstanceDescriptionFromJ9Class64(long j9clazz);

	private static native long getDescriptionWordFromPtr64(long descriptorPtr); 

	// TODO: JavaDoc required please
	public static int getNumSlotsInObject(Class currentClass) {
		int numSlotsInObject;
		if (is32Bit()) {
			int j9clazz = getJ9ClassFromClass32(currentClass);
			// Get the instance size out of the J9Class
			int instanceSize = getTotalInstanceSizeFromJ9Class32(j9clazz);
			numSlotsInObject = instanceSize / 4; //numBytesInReferenceField;
		} else {
			long j9clazz = getJ9ClassFromClass64(currentClass);
			// Get the instance size out of the J9Class
			long instanceSize = getTotalInstanceSizeFromJ9Class64(j9clazz);
			numSlotsInObject = (int) instanceSize / 4; //numBytesInReferenceField;
			if (instanceSize > Integer.MAX_VALUE / 4) //numBytesInReferenceField)
				numSlotsInObject = -1;
		}

		return numSlotsInObject;
	}

	// TODO: JavaDoc required please
	@SuppressWarnings("deprecation")
	public static int getSlotIndex(Field field) {
		// Depends on the definition of slot and whether it's 32 or 64 bit
		int bytesInHeader = getNumBytesInJ9ObjectHeader();
		int fieldOffset = 0;
		if (Modifier.isStatic(field.getModifiers())) {
			fieldOffset = (int) unsafe.staticFieldOffset(field);
		} else {
			fieldOffset = (int) unsafe.objectFieldOffset(field);
		}
		int index = (fieldOffset - bytesInHeader) / 4; //numBytesInReferenceField;
		return index;
	}

	// TODO: JavaDoc required please
	@SuppressWarnings("deprecation")
	public static void vmDeepCopy(Object srcObj, Object dest,
			Class currentClass, ClassLoader targetCL, int[] destOffsets,
			Class[] declaredType, ORBBase orbi, Map map, Class itsType) throws Exception {
		int numSlotsInObject;
		int numBitsInWord = getNumBitsInDescriptionWord();
		// Depends on the definition of slot and whether it's 32 or 64 bit
		int bytesInHeader = getNumBytesInJ9ObjectHeader();
		int numBytesInReferenceField = getNumBytesInReferenceField();
		int j9clazz = 0;
		int instanceSize = 0;
		int descriptorPtr = 0;
		long j9clazz64 = 0;
		long instanceSize64 = 0;
		long descriptorPtr64 = 0;

		if ((null == srcObj) || (null == dest)) {
			throw new NullPointerException();
		}
		
		if (is32Bit()) {
			j9clazz = getJ9ClassFromClass32(currentClass);
			// Get the instance size out of the J9Class
			instanceSize = getTotalInstanceSizeFromJ9Class32(j9clazz);
			// Get the pointer to instance descriptor out of the J9Class.  Tells which fields are refs and prims
			descriptorPtr = getInstanceDescriptionFromJ9Class32(j9clazz);
			numSlotsInObject = instanceSize / numBytesInReferenceField;
		} else {
			j9clazz64 = getJ9ClassFromClass64(currentClass);
			// Get the instance size out of the J9Class
			instanceSize64 = getTotalInstanceSizeFromJ9Class64(j9clazz64);
			// Get the pointer to instance descriptor out of the J9Class.  Tells which fields are refs and prims
			descriptorPtr64 = getInstanceDescriptionFromJ9Class64(j9clazz64);
			numSlotsInObject = (int) instanceSize64 / numBytesInReferenceField;
		}

		int countSlots = 0; //slotsInHeader;
		int descriptorWord = 0;
		long descriptorWord64 = 0;
		int bitIndex = 0;
		if (isDescriptorPointerTagged(descriptorPtr, descriptorPtr64)) {
			bitIndex++;
			if (is32Bit())
				descriptorWord = descriptorPtr >> 1;
			else
				descriptorWord64 = descriptorPtr64 >> 1;
		} else {
			if (is32Bit())
				descriptorWord = getDescriptionWordFromPtr32(descriptorPtr);
			else
				descriptorWord64 = getDescriptionWordFromPtr64(descriptorPtr64);
		}

		int srcIndex = 0;
		while (numSlotsInObject > 0) {
			int destIndex = destOffsets[(int) srcIndex];
            int nextDestIndex = -1;
            if ((getNumBytesInReferenceField() == 8) && (srcIndex + 1 < destOffsets.length))
               nextDestIndex = destOffsets[(int) srcIndex + 1];
			Class type = declaredType[srcIndex];
			
			if ((destIndex >= 0) || (nextDestIndex >= 0)) {
				if (isDescriptorPointerTagged(descriptorWord, descriptorWord64)) {
					Object fieldValue = unsafe.getObject(srcObj, bytesInHeader + (countSlots
							* numBytesInReferenceField));
					if (fieldValue != null) {
						Object result = orbi.quickCopyIfPossible(fieldValue,
								type, targetCL, map, itsType);

						if (result != null) {
							if (destIndex >= 0)
								unsafe.putObject(dest, bytesInHeader + (destIndex
										* 4), result);
							countSlots++;
							if (countSlots >= numSlotsInObject)
								break;

							if (bitIndex == (numBitsInWord - 1)) {
								if (is32Bit())
									descriptorPtr = descriptorPtr
											+ ORBVMHelpers
													.getNumBytesInDescriptionWord();
								else
									descriptorPtr64 = descriptorPtr64
											+ ORBVMHelpers
													.getNumBytesInDescriptionWord();

								bitIndex = 0;

								if (is32Bit())
									descriptorWord = getDescriptionWordFromPtr32(descriptorPtr);
								else
									descriptorWord64 = getDescriptionWordFromPtr64(descriptorPtr64);
							} else {
								if (is32Bit())
									descriptorWord = descriptorWord >> 1;
								else
									descriptorWord64 = descriptorWord64 >> 1;
								bitIndex++;
							}

                            if (getNumBytesInReferenceField() == 8)
							    srcIndex = srcIndex + 2;
                            else
                            	srcIndex++;
							continue;
						}

						fieldValue = orbi.deepCopyIfRequired(fieldValue, type,
								targetCL, map, type);

                        if (destIndex >= 0)
  						   unsafe.putObject(dest, bytesInHeader + (destIndex
 								* 4), fieldValue);
					}
				} else {
					if (getNumBytesInReferenceField() != 8)
						unsafe.putInt(dest, bytesInHeader + (destIndex * numBytesInReferenceField),
							unsafe.getInt(srcObj, bytesInHeader + (countSlots
									* numBytesInReferenceField)));
                    else
					   {
					       /*
					   if ((destIndex >= 0) && (nextDestIndex >= 0))
                                           unsafe.putLong(dest, bytesInHeader + (destIndex * numBytesInReferenceField),
	  						   unsafe.getLong(srcObj, bytesInHeader + (countSlots
								   	   * numBytesInReferenceField)));
									   */
                                        if (destIndex >= 0)
                                           unsafe.putInt(dest, bytesInHeader + (destIndex * 4),
						  	   unsafe.getInt(srcObj, bytesInHeader + (countSlots
									* numBytesInReferenceField)));
                                       if (nextDestIndex >= 0)
                                          unsafe.putInt(dest, bytesInHeader + (nextDestIndex * 4),
						  	   unsafe.getInt(srcObj, bytesInHeader + (countSlots
									* numBytesInReferenceField) + 4));
                 
					   }
				}
			}

			countSlots++;
			if (countSlots >= numSlotsInObject)
				break;

			if (bitIndex == (numBitsInWord - 1)) {
				if (is32Bit())
					descriptorPtr = descriptorPtr
							+ getNumBytesInDescriptionWord();
				else
					descriptorPtr64 = descriptorPtr64
							+ getNumBytesInDescriptionWord();

				bitIndex = 0;

				if (is32Bit())
					descriptorWord = getDescriptionWordFromPtr32(descriptorPtr);
				else
					descriptorWord64 = getDescriptionWordFromPtr64(descriptorPtr64);
			} else {
				if (is32Bit())
					descriptorWord = descriptorWord >> 1;
				else
					descriptorWord64 = descriptorWord64 >> 1;

				bitIndex++;
			}

            if (getNumBytesInReferenceField() == 8)
			    srcIndex = srcIndex + 2;
            else
            	srcIndex++;
		}
	}

	private static boolean isDescriptorPointerTagged(int descriptorPtr, long descriptorPtr64) {
		if (is32Bit()) {
			if ((descriptorPtr & 1) == 1) {
				return true;
			}
		} else {
			if ((descriptorPtr64 & 1) == 1) {
				return true;
			}
		}

		return false;
	}

}
