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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;
import java.util.Comparator;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.gc.GCStringTableIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;

public class DumpStringTableCommand extends Command {

	public DumpStringTableCommand() {
		addCommand("dumpstringtable", "", "Dump content of string table");
	}

	private Set<J9ObjectPointer> getStringTableObjects() throws CorruptDataException {
		/* Use sorted set to display string table ordered by address */
		SortedSet<J9ObjectPointer> sortedSet = new TreeSet<J9ObjectPointer>(
				new Comparator<J9ObjectPointer>() {
					public int compare(J9ObjectPointer o1, J9ObjectPointer o2) {
						if (o1.getAddress() < o2.getAddress()) {
							return -1;
						} else if (o1.getAddress() > o2.getAddress()) {
							return 1;
						} else {
							return 0;
						}
					}
				});
				
		GCStringTableIterator it = GCStringTableIterator.from();
		while (it.hasNext()) {
			J9ObjectPointer next = it.next();
			sortedSet.add(next);
		}
		return sortedSet;
	}
	
	public void run(String command, String[] args, Context context,	PrintStream out) throws DDRInteractiveCommandException {
		Set<J9ObjectPointer> stringTableObjects = null;
		try {
			stringTableObjects = getStringTableObjects();
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e.getMessage(), e);
		}

		for (J9ObjectPointer objectPointer : stringTableObjects) {
			String value = "**CORRUPT STRING TABLE ELEMENT **";
			try {
				value = J9ObjectHelper.stringValue(objectPointer);
			} catch (CorruptDataException e) {
				// ignore
			}
			
			String hexAddr = objectPointer.formatShortInteractive();				
			out.println(String.format("%s value = <%s>", hexAddr, value));
		}
		out.println("Table Size = " + stringTableObjects.size());
	}
}
