/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.structure.J9Object;
import com.ibm.j9ddr.vm29.types.UDATA;

public abstract class GCObjectIterator extends GCIterator
{
	protected J9ObjectPointer object;
	protected boolean includeClassSlot;
	
	/* Do not instantiate. Use the factory */
	protected GCObjectIterator(J9ObjectPointer object, boolean includeClassSlot) throws CorruptDataException 
	{
		this.object = object;
		this.includeClassSlot = includeClassSlot;
	}	
	
	public abstract boolean hasNext();
	public abstract J9ObjectPointer next();
	public abstract VoidPointer nextAddress();
	
	/**
	 * Factory method to construct an appropriate object iterator.
	 * @param object Object to iterate
	 * @param includeClassSlot whether or not to include class of the object
	 * @throws CorruptDataException 
	 */
	public static GCObjectIterator fromJ9Object(J9ObjectPointer object, boolean includeClassSlot) throws CorruptDataException
	{
		if (ObjectModel.isIndexable(object)) {
			UDATA classShape = ObjectModel.getClassShape(object);
			if (classShape.eq(J9Object.OBJECT_HEADER_SHAPE_POINTERS)) {
				return getPointerArrayImpl(object, includeClassSlot);
			} else {
				return getEmptyObjectIteratorImpl(object, includeClassSlot);
			}
		} else {
			return getMixedObjectIteratorImpl(object, includeClassSlot);
		}
	}

	/**
	 * Factory method to construct an appropriate object iterator.
	 * @param object Object to iterate
	 * @param includeClassSlot whether or not to include class of the object
	 * @param addr pointer to object data
	 * @throws CorruptDataException 
	 */
	public static GCObjectIterator fromJ9Class(J9ClassPointer clazz, VoidPointer addr) throws CorruptDataException
	{
		if (J9ClassHelper.isArrayClass(clazz)) {
			UDATA classShape = ObjectModel.getClassShape(clazz);
			if (classShape.eq(J9Object.OBJECT_HEADER_SHAPE_POINTERS)) {
				return getPointerArrayImpl(clazz, addr);
			} else {
				return getEmptyObjectIteratorImpl(clazz, addr);
			}
		} else {
			return getMixedObjectIteratorImpl(clazz, addr);
		}
	}
	
	private static GCObjectIterator getPointerArrayImpl(J9ObjectPointer object, boolean includeClassSlot) throws CorruptDataException 
	{
		if (J9BuildFlags.gc_arraylets) {
			AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_POINTER_ARRAYLET_ITERATOR_VERSION);
			switch (version.getAlgorithmVersion()) {
				// Add case statements for new algorithm versions
				default:
					return new GCPointerArrayletIterator_V1(object, includeClassSlot);
			}
		} else {
			AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_POINTER_ARRAY_ITERATOR_VERSION);
			switch (version.getAlgorithmVersion()) {
				// Add case statements for new algorithm versions
				default:
					return new GCPointerArrayIterator_V1(object, includeClassSlot);
			}
		}
	}

	private static GCObjectIterator getPointerArrayImpl(J9ClassPointer clazz, VoidPointer addr) throws CorruptDataException 
	{
		// TODO: includeClassSlot = false;
		throw new IllegalArgumentException("Not implemented yet");
	}
	
	private static GCObjectIterator getEmptyObjectIteratorImpl(J9ObjectPointer object, boolean includeClassSlot) throws CorruptDataException 
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_EMPTY_OBJECT_ITERATOR_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new GCEmptyObjectIterator_V1(object, includeClassSlot);
		}
	}
	
	private static GCObjectIterator getEmptyObjectIteratorImpl(J9ClassPointer clazz, VoidPointer addr) throws CorruptDataException 
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_EMPTY_OBJECT_ITERATOR_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new GCEmptyObjectIterator_V1(null, false);
		}
	}
	
	private static GCObjectIterator getMixedObjectIteratorImpl(J9ObjectPointer object, boolean includeClassSlot) throws CorruptDataException 
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_MIXED_OBJECT_ITERATOR_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new GCMixedObjectIterator_V1(object, includeClassSlot);
		}
	}
	
	private static GCObjectIterator getMixedObjectIteratorImpl(J9ClassPointer clazz, VoidPointer addr) throws CorruptDataException 
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.GC_MIXED_OBJECT_ITERATOR_VERSION);
		switch (version.getAlgorithmVersion()) {
		// Add case statements for new algorithm versions
		default:
			return new GCMixedObjectIterator_V1(clazz, addr);
		}
	}
}
