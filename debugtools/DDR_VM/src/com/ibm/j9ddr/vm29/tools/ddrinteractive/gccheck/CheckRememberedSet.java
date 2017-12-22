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
import com.ibm.j9ddr.vm29.j9.gc.GCRememberedSetIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCRememberedSetSlotIterator;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.MM_SublistPuddlePointer;

class CheckRememberedSet extends Check
{
	@Override
	public void check()
	{
		try {
			/* no point checking if the scavenger wasn't turned on */
			if(!J9BuildFlags.gc_modronScavenger || !_extensions.scavengerEnabled()) {
				return;
			}
			
			GCRememberedSetIterator remSetIterator = GCRememberedSetIterator.from();
			while(remSetIterator.hasNext()) {
				MM_SublistPuddlePointer puddle = remSetIterator.next();
				GCRememberedSetSlotIterator remSetSlotIterator = GCRememberedSetSlotIterator.fromSublistPuddle(puddle);
				while(remSetSlotIterator.hasNext()) {
					PointerPointer objectIndirect = PointerPointer.cast(remSetSlotIterator.nextAddress());
					if(_engine.checkSlotRememberedSet(objectIndirect, puddle)  != J9MODRON_SLOT_ITERATOR_OK) {
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
		return "REMEMBERED SET";
	}

	@Override
	public void print()
	{
		try {
			GCRememberedSetIterator remSetIterator = GCRememberedSetIterator.from();
			ScanFormatter formatter = new ScanFormatter(this, "RememberedSet Sublist", getGCExtensions().rememberedSetEA());
			while(remSetIterator.hasNext()) {
				MM_SublistPuddlePointer puddle = remSetIterator.next();
				formatter.section("puddle", puddle);		
				GCRememberedSetSlotIterator remSetSlotIterator = GCRememberedSetSlotIterator.fromSublistPuddle(puddle);
				while(remSetSlotIterator.hasNext()) {
					formatter.entry(remSetSlotIterator.next());
				}
				formatter.endSection();
			}
			formatter.end("RememberedSet Sublist", getGCExtensions().rememberedSetEA());
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

}
