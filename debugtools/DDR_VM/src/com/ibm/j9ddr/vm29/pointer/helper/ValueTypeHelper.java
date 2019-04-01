/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9FlattenedClassCachePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.types.UDATA;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.J9AccValueType;
import static com.ibm.j9ddr.vm29.structure.J9JavaClassFlags.J9ClassIsFlattened;

public class ValueTypeHelper {

	public static boolean valueTypeFlag() {
		boolean imported = J9BuildFlags.J9VM_OPT_VALHALLA_VALUE_TYPES;
		return imported;
	}

	public static J9ClassPointer findJ9ClassInFlattenedClassCache(J9FlattenedClassCachePointer flattenedClassCache,
			String fieldName) {
		try {
			J9ClassPointer resultClazz = J9ClassPointer.NULL;
			UDATA length = flattenedClassCache.offset();
			int iLength = length.intValue();
			for (int i = 1; i <= iLength; i++) {
				J9FlattenedClassCachePointer tempCache = J9FlattenedClassCachePointer.cast(flattenedClassCache.add(i));
				J9ClassPointer currentClazz = tempCache.clazz();
				if (J9ClassHelper.getName(currentClazz).equals(fieldName)) {
					resultClazz = currentClazz;
					break;
				}
			}
			return resultClazz;
		} catch (CorruptDataException e) {
			return J9ClassPointer.NULL;

		}
	}

	public static boolean romClassJ9IfValueType(J9ROMClassPointer romClass) throws CorruptDataException {
		return romClass.modifiers().allBitsIn(J9AccValueType);

	}

	public static boolean classIsFlattened(J9ClassPointer clazz) throws CorruptDataException {
		return J9ClassHelper.extendedClassFlags(clazz).anyBitsIn(J9ClassIsFlattened);
	}

	public static boolean ifClassInFlattenedClassCache(J9FlattenedClassCachePointer flattenedClassCache,
			String fieldName) {
		return (J9ClassPointer.NULL != (findJ9ClassInFlattenedClassCache(flattenedClassCache, fieldName)));
	}

	public static boolean ifClassIsFlattened(J9FlattenedClassCachePointer flattenedClassCache, String fieldName)
			throws CorruptDataException {
		boolean result = false;
		J9ClassPointer clazz = findJ9ClassInFlattenedClassCache(flattenedClassCache, fieldName);
		if (clazz != J9ClassPointer.NULL) {
			result = classIsFlattened(clazz);
		}
		return result;
	}

}
