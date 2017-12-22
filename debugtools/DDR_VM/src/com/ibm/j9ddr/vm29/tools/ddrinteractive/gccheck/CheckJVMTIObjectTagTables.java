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
import com.ibm.j9ddr.vm29.j9.gc.GCJVMTIObjectTagTableIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCJVMTIObjectTagTableListIterator;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JVMTIDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JVMTIEnvPointer;

class CheckJVMTIObjectTagTables extends Check
{
	@Override
	public void check()
	{
		try {
			J9JVMTIDataPointer jvmtiData = J9JVMTIDataPointer.cast(getJavaVM().jvmtiData());
			if(jvmtiData.notNull()) {
				GCJVMTIObjectTagTableListIterator objectTagTableList = GCJVMTIObjectTagTableListIterator.fromJ9JVMTIData(jvmtiData);
				while(objectTagTableList.hasNext()) {
					J9JVMTIEnvPointer list = objectTagTableList.next();
					VoidPointer objectTagTable = VoidPointer.cast(list.objectTagTable());
					GCJVMTIObjectTagTableIterator objectTagTableIterator = GCJVMTIObjectTagTableIterator.fromJ9JVMTIEnv(list);
					while(objectTagTableIterator.hasNext()) {
						PointerPointer slot = PointerPointer.cast(objectTagTableIterator.nextAddress());
						if(slot.notNull()) {
							if(_engine.checkSlotPool(slot, objectTagTable) != J9MODRON_SLOT_ITERATOR_OK ){
								return;
							} 
						}
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
		return "JVMTI OBJECT TAG TABLES";
	}

	@Override
	public void print()
	{
		try {
			J9JVMTIDataPointer jvmtiData = J9JVMTIDataPointer.cast(getJavaVM().jvmtiData());
			if(jvmtiData.notNull()) {
				ScanFormatter formatter = new ScanFormatter(this, "jvmtiObjectTagTables", jvmtiData);
				GCJVMTIObjectTagTableListIterator objectTagTableList = GCJVMTIObjectTagTableListIterator.fromJ9JVMTIData(jvmtiData);
				while(objectTagTableList.hasNext()) {
					J9JVMTIEnvPointer list = objectTagTableList.next();
					GCJVMTIObjectTagTableIterator objectTagTableIterator = GCJVMTIObjectTagTableIterator.fromJ9JVMTIEnv(list);
					while(objectTagTableIterator.hasNext()) {
						formatter.entry(objectTagTableIterator.next());
					}
				}
				formatter.end("jvmtiObjectTagTables", jvmtiData);
			}	
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

}
