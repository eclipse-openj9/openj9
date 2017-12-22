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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.Collections;
import java.util.Iterator;
import java.util.Set;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.U32;

public abstract class J9ObjectFieldOffsetIterator implements Iterator<J9ObjectFieldOffset> {

	public static Iterator<J9ObjectFieldOffset> J9ObjectFieldOffsetIteratorFor(J9ROMClassPointer romClass, J9ClassPointer clazz, J9ClassPointer superClazz, U32 flags) {
		try {
			return getImpl(J9RASHelper.getVM(DataType.getJ9RASPointer()), romClass, clazz, superClazz, flags, AlgorithmVersion.OBJECT_FIELD_OFFSET);
		} catch (CorruptDataException e) {
			//raise a corrupt event and return an empty iterator
			raiseCorruptDataEvent("Could not create a field offset iterator", e, true);
			Set<J9ObjectFieldOffset> emptySet = Collections.emptySet();
			return emptySet.iterator();
		}
 	}

	public static Iterator<J9ObjectFieldOffset> J9ObjectFieldOffsetIteratorFor(J9ClassPointer clazz, J9ClassPointer superClazz, U32 flags) {
		try {
			return getImpl(J9RASHelper.getVM(DataType.getJ9RASPointer()), clazz.romClass(), clazz, superClazz, flags, AlgorithmVersion.OBJECT_FIELD_OFFSET);
		} catch (CorruptDataException e) {
			//raise a corrupt event and return an empty iterator
			raiseCorruptDataEvent("Could not create a field offset iterator", e, true);
			Set<J9ObjectFieldOffset> emptySet = Collections.emptySet();
			return emptySet.iterator();
		}
	}

	
	private static J9ObjectFieldOffsetIterator getImpl(J9JavaVMPointer vm, J9ROMClassPointer romClass, J9ClassPointer clazz, J9ClassPointer superClazz, U32 flags, String algorithmID) {
//		AlgorithmVersion version = AlgorithmVersion.getVersionOf(algorithmID);
		switch (AlgorithmVersion.getVMMinorVersion()) {
			default:
				return new J9ObjectFieldOffsetIterator_V1(vm, romClass, clazz, superClazz, flags);
		}
	}

	public abstract boolean hasNext();
	public abstract J9ObjectFieldOffset next();
	
	public void remove() {
		throw new UnsupportedOperationException();
	}
}
