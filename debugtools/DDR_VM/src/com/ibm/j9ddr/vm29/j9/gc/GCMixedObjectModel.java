/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.gc;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.types.UDATA;


abstract class GCMixedObjectModel  extends GCBase
{
	/**
	 * Factory method to construct an appropriate mixed (i.e. non-array) object model.
	 * @param structure the J9JavaVM structure to use
	 * 
	 * @return an instance of MixedObjectModel
	 * @throws CorruptDataException 
	 */
	public static GCMixedObjectModel from() throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_MIXED_OBJECT_MODEL_VERSION);
		switch (version.getAlgorithmVersion()) {
		// Add case statements for new algorithm versions
		default:
			return new GCMixedObjectModel_V1();
		}
		
	}
	
	/**
	 * Returns the size of a class, in bytes, excluding the header.
	 * @param clazz Pointer to the class whose size is required
	 * @return Size of class in bytes excluding the header
	 * @throws CorruptDataException 
	 */
	public abstract UDATA getSizeInBytesWithoutHeader(J9ClassPointer clazz) throws CorruptDataException;

	/**
	 * Returns the size of a mixed object, in bytes, excluding the header.
	 * @param object Pointer to the object whose size is required
	 * @return Size of object in bytes excluding the header
	 * @throws CorruptDataException 
	 */	
	public abstract UDATA getSizeInBytesWithoutHeader(J9ObjectPointer object) throws CorruptDataException;

	/**
	 * Returns the size of a mixed object, in bytes, including the header.
	 * @param object Pointer to the object whose size is required
	 * @return Size of object in bytes including the header
	 * @throws CorruptDataException 
	 */		
	public UDATA getSizeInBytesWithHeader(J9ObjectPointer object) throws CorruptDataException
	{
		return getSizeInBytesWithoutHeader(object).add(getHeaderSize(object));
	}

	/**
	 * Returns the size of a mixed object, in slots, excluding the header.
	 * @param object Pointer to the object whose size is required
	 * @return Size of object in slots excluding the header
	 * @throws CorruptDataException 
	 */		
	public UDATA getSizeInSlotsWithoutHeader(J9ObjectPointer object) throws CorruptDataException
	{
		return UDATA.convertBytesToSlots(getSizeInBytesWithoutHeader(object));
	}

	/**
	 * Returns the size of a mixed object, in slots, including the header.
	 * @param object Pointer to the object whose size is required
	 * @return Size of object in slots including the header
	 * @throws CorruptDataException 
	 */		
	public UDATA getSizeInSlotsWithHeader(J9ObjectPointer object) throws CorruptDataException
	{
		return UDATA.convertBytesToSlots(getSizeInBytesWithHeader(object));
	}

	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * @param arrayPtr Pointer to the object
	 * @return offset of the hashcode slot
	 */
	public abstract UDATA getHashcodeOffset(J9ObjectPointer object) throws CorruptDataException;
	
	/**
	 * Returns the offset of the hashcode slot, in bytes, from the beginning of the header.
	 * @param clazzPtr Pointer to the class of the object
	 * @return offset of the hashcode slot
	 */
	public abstract UDATA getHashcodeOffset(J9ClassPointer clazz) throws CorruptDataException;

	/**
	 * Returns the header size of a given object.
	 * @param arrayPtr Ptr to an array for which header size will be returned
	 * @return Size of header in bytes
	 */
	public abstract UDATA getHeaderSize(J9ObjectPointer object) throws CorruptDataException;
}
