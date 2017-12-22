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
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionManagerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_OwnableSynchronizerObjectListPointer;
import com.ibm.j9ddr.vm29.structure.J9Consts;

class CheckOwnableSynchronizerList extends Check
{
	@Override
	public void check()
	{		
		try {
			MM_HeapRegionManagerPointer heapRegionManager = getGCExtensions().heapRegionManager();
			long maximumOwnableSynchronizerCountOnHeap = heapRegionManager._totalHeapSize().longValue()/J9Consts.J9_GC_MINIMUM_OBJECT_SIZE;
			MM_OwnableSynchronizerObjectListPointer ownableSynchronizerObjectList = getGCExtensions().ownableSynchronizerObjectLists();

			while (ownableSynchronizerObjectList.notNull()) {
				J9ObjectPointer object = ownableSynchronizerObjectList._head();
				long ownableSynchronizerCount = 0;
				_engine.clearPreviousObjects();
				
				while (object.notNull()) {
					if(_engine.checkSlotOwnableSynchronizerList(object, ownableSynchronizerObjectList) != CheckBase.J9MODRON_SLOT_ITERATOR_OK) {
						break;
					}
					
					ownableSynchronizerCount += 1;
					if (ownableSynchronizerCount > maximumOwnableSynchronizerCountOnHeap) {
						_engine.reportOwnableSynchronizerCircularReferenceError(object, ownableSynchronizerObjectList);
						break;
					}					
					_engine.pushPreviousObject(object);
					object = ObjectAccessBarrier.getOwnableSynchronizerLink(object);
				}
				ownableSynchronizerObjectList = ownableSynchronizerObjectList._nextList();
			}
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}

		if (_engine.needVerifyOwnableSynchronizerConsistency()) {
			/* call verifyOwnableSynchronizerObjectCounts() only at the end of CheckOwnableSynchronizerList,
			 * verifyOwnableSynchronizerObjectCounts need both the count calculated in CheckObjectHeap and in CheckOwnableSynchronizerList
			 * assumption: CheckOwnableSynchronizerList always happens after CheckObjectHeap in CheckCycle  
			 */
			_engine.verifyOwnableSynchronizerObjectCounts();
		}

	}

	@Override
	public String getCheckName()
	{
		return "OWNABLE_SYNCHRONIZER";
	}

	@Override
	public void print()
	{
		try {
			MM_OwnableSynchronizerObjectListPointer ownableSynchronizerObjectList = getGCExtensions().ownableSynchronizerObjectLists();
			ScanFormatter formatter = new ScanFormatter(this, "ownableSynchronizerObjectList");
			while (ownableSynchronizerObjectList.notNull()) {
				formatter.section("list", ownableSynchronizerObjectList);
				J9ObjectPointer object = ownableSynchronizerObjectList._head();
				while (object.notNull()) {
					formatter.entry(object);
					object = ObjectAccessBarrier.getOwnableSynchronizerLink(object);
				}
				formatter.endSection();
				ownableSynchronizerObjectList = ownableSynchronizerObjectList._nextList();
			}
			formatter.end("ownableSynchronizerObjectList");
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

}
