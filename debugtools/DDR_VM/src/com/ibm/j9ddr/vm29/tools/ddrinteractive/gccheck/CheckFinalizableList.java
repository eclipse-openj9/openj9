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
import com.ibm.j9ddr.vm29.j9.gc.GCFinalizableObjectIterator;

class CheckFinalizableList extends Check
{
	@Override
	public void check()
	{
		try {	
			GCFinalizableObjectIterator finalizableObjectIterator = GCFinalizableObjectIterator.from();
			while(finalizableObjectIterator.hasNext()) {
				if(_engine.checkSlotFinalizableList(finalizableObjectIterator.next()) != CheckBase.J9MODRON_SLOT_ITERATOR_OK ) {
					return;
				}
			}
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

	@Override
	public String getCheckName()
	{
		return "FINALIZABLE";
	}

	@Override
	public void print()
	{
		try {	
			ScanFormatter formatter = new ScanFormatter(this, "finalizableList");
			GCFinalizableObjectIterator finalizableObjectIterator = GCFinalizableObjectIterator.from();
			int state = finalizableObjectIterator.getState();
			while(finalizableObjectIterator.hasNext()) {
				if(finalizableObjectIterator.getState() != state) {
					formatter.endSection();
					state = finalizableObjectIterator.getState();
					switch(state)
					{
					case GCFinalizableObjectIterator.state_system:
						formatter.section("finalizable objects created by the system classloader");
						break;
						
					case GCFinalizableObjectIterator.state_default:
						formatter.section("finalizable objects created by application class loaders");
						break;
						
						
					case GCFinalizableObjectIterator.state_reference:
						formatter.section("reference objects");
						break;							
					}
				}
				formatter.entry(finalizableObjectIterator.next());
			}
			formatter.end("finalizableList");
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

}
