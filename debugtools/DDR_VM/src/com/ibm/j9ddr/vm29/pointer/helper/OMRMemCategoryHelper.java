/*******************************************************************************
 * Copyright (c) 2010, 2018 IBM Corp. and others
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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.generated.OMRMemCategoryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OMRMemCategorySetPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OMRPortLibraryGlobalDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PortLibraryPointer;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;
import static com.ibm.j9ddr.vm29.structure.J9PortLibrary.*;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Helper routines for OMRMemCategoryPointer.
 *
 * @author andhall
 */
public class OMRMemCategoryHelper {

	public static OMRMemCategoryPointer getUnusedAllocate32HeapRegionsMemoryCategory(
			OMRPortLibraryGlobalDataPointer portGlobals) throws CorruptDataException {
		Exception exception;

		try {
			Class<?> clazz = OMRPortLibraryGlobalDataPointer.class;
			Method method = clazz.getDeclaredMethod("unusedAllocate32HeapRegionsMemoryCategory");

			return (OMRMemCategoryPointer) method.invoke(portGlobals);
		} catch (NoSuchMethodException e) {
			exception = e;
		} catch (IllegalAccessException e) {
			exception = e;
		} catch (InvocationTargetException e) {
			Throwable cause = e.getCause();

			if (cause instanceof CorruptDataException) {
				throw (CorruptDataException) cause;
			}

			exception = e;
		}

		// unexpected exception using reflection
		CorruptDataException cd = new CorruptDataException(exception.toString(), exception);
		raiseCorruptDataEvent("Error accessing unusedAllocate32HeapRegionsMemoryCategory", cd, true);

		return null;
	}

	public static OMRMemCategoryPointer getMemoryCategory(UDATA memoryCategory) throws CorruptDataException {
		J9PortLibraryPointer portLib = J9RASHelper.getVM(DataType.getJ9RASPointer()).portLibrary();
		OMRPortLibraryGlobalDataPointer portGlobals = portLib.omrPortLibrary().portGlobals();

		if (memoryCategory.eq(OMRMEM_CATEGORY_PORT_LIBRARY)) {
			return portGlobals.portLibraryMemoryCategory();
		} else if (J9BuildFlags.env_data64 && memoryCategory.eq(OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS)) {
			return getUnusedAllocate32HeapRegionsMemoryCategory(portGlobals);
		} else {
			OMRMemCategorySetPointer registeredSet = OMRMemCategorySetPointer.NULL;
			long index = 0;
			if (memoryCategory.lt(new U32(0x7FFFFFFFl))) {
				registeredSet = portGlobals.control().language_memory_categories();
				index = memoryCategory.longValue();
			} else {
				registeredSet = portGlobals.control().omr_memory_categories();
				index = (0x7FFFFFFFL & memoryCategory.longValue());
			}
			if (registeredSet.notNull()) {
				if (registeredSet.numberOfCategories().gt(index) && registeredSet.categories().at(index).notNull()) {
					return OMRMemCategoryPointer.cast(registeredSet.categories().at(index));
				} else {
					return portGlobals.unknownMemoryCategory();
				}
			} else {
				return portGlobals.unknownMemoryCategory();
			}
		}
	}

	/**
	 * Performs a depth-first walk of all the children of startNode, starting with startNode itself
	 * @param startNode Node to start walking from
	 * @param visitor Visitor object
	 */
	public static void visitMemoryCategoryChildren(OMRMemCategoryPointer startNode, IOMRMemCategoryVisitor visitor)
			throws CorruptDataException {
		visitor.visit(startNode);

		final int numberOfChildren = startNode.numberOfChildren().intValue();

		for (int i = 0; i < numberOfChildren; i++) {
			UDATA childCode = startNode.children().at(i);
			OMRMemCategoryPointer child = getMemoryCategory(childCode);

			/* getMemoryCategory will map codes to the unknown code if necessary. If we've mapped to another category code,
			 * we don't want to iterate to it
			 */
			if (child.categoryCode().eq(childCode)) {
				visitMemoryCategoryChildren(child, visitor);
			} else {
				UDATA thisCategoryCode = null;

				try {
					thisCategoryCode = startNode.categoryCode();
				} catch (CorruptDataException ex) {
					// ignore
				}

				if (thisCategoryCode == null) {
					throw new CorruptDataException("Bad memory category child relationship. Memory Category at " + startNode.getHexAddress() + " references unknown memory category code " + childCode);
				} else {
					throw new CorruptDataException("Bad memory category child relationship. Memory Category at " + startNode.getHexAddress() + " code " + thisCategoryCode + " references unknown memory category code " + childCode);
				}
			}
		}
	}

	public static interface IOMRMemCategoryVisitor {
		public void visit(OMRMemCategoryPointer category) throws CorruptDataException;
	}

}
