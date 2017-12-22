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

import static com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck.CheckBase.J9MODRON_SLOT_ITERATOR_OK;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadListIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadIterator;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;

class CheckVMThreads extends Check
{
	@Override
	public void check()
	{
		try {
			GCVMThreadListIterator vmThreadListIterator = GCVMThreadListIterator.from();
			while(vmThreadListIterator.hasNext()) {
				J9VMThreadPointer walkThread = vmThreadListIterator.next();
				GCVMThreadIterator vmthreadIterator = GCVMThreadIterator.fromJ9VMThread(walkThread);
				while(vmthreadIterator.hasNext()) {
					PointerPointer slot = PointerPointer.cast(vmthreadIterator.nextAddress());
					if(_engine.checkSlotVMThread(slot, VoidPointer.cast(walkThread), CheckError.check_type_other, vmthreadIterator.getState()) != J9MODRON_SLOT_ITERATOR_OK ) {
						continue;
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
		return "VM THREAD SLOTS";
	}

	@Override
	public void print()
	{
		try {
			GCVMThreadListIterator vmThreadListIterator = GCVMThreadListIterator.from();
			ScanFormatter formatter = new ScanFormatter(this, "VMThread Slots");
			while(vmThreadListIterator.hasNext()) {
				J9VMThreadPointer walkThread = vmThreadListIterator.next();
				formatter.section("thread", walkThread);
				GCVMThreadIterator vmthreadIterator = GCVMThreadIterator.fromJ9VMThread(walkThread);
				while(vmthreadIterator.hasNext()) {
					formatter.entry(vmthreadIterator.next());
				}
				formatter.endSection();
			}
			formatter.end("VMThread Slots");
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

}
