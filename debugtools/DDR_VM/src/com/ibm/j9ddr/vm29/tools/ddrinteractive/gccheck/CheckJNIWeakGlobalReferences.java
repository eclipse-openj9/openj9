/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.gc.GCJNIWeakGlobalReferenceIterator;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

import static com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck.CheckBase.*;

class CheckJNIWeakGlobalReferences extends Check
{
	@Override
	public void check()
	{
		try {
			VoidPointer weakRefs = VoidPointer.cast(getJavaVM().jniWeakGlobalReferences()); 
			GCJNIWeakGlobalReferenceIterator jniWeakGlobalReferenceIterator = GCJNIWeakGlobalReferenceIterator.from();
			while(jniWeakGlobalReferenceIterator.hasNext()) {
				PointerPointer slot = PointerPointer.cast(jniWeakGlobalReferenceIterator.nextAddress());
				if(slot.notNull()) {
					if (_engine.checkSlotPool(slot, weakRefs) != J9MODRON_SLOT_ITERATOR_OK) {
						return;
					}
				}
			}
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

	@Override
	public String getCheckName()
	{
		return "JNI WEAK GLOBAL REFS";
	}

	@Override
	public void print()
	{
		try {
			VoidPointer weakRefs = VoidPointer.cast(getJavaVM().jniWeakGlobalReferences()); 
			GCJNIWeakGlobalReferenceIterator jniWeakGlobalReferenceIterator = GCJNIWeakGlobalReferenceIterator.from();
			ScanFormatter formatter = new ScanFormatter(this, "jniWeakGlobalReferences", weakRefs);
			while(jniWeakGlobalReferenceIterator.hasNext()) {
				J9ObjectPointer slot = jniWeakGlobalReferenceIterator.next();
				if(slot.notNull()) {
					formatter.entry(slot);
				}
			}
			formatter.end("jniWeakGlobalReferences", weakRefs);
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

}
