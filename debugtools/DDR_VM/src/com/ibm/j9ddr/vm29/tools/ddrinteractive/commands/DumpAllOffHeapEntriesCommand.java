/*
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.HashTable;
import com.ibm.j9ddr.vm29.j9.ObjectToSparseDataHashTable;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCBase;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_SparseAddressOrderedFixedSizeDataPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_SparseDataTableEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_SparseVirtualMemoryPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Dump all off-heap entries in the core file.
 *
 * usage: !dumpalloffheapentries
 */
public class DumpAllOffHeapEntriesCommand extends Command
{
	public DumpAllOffHeapEntriesCommand()
	{
		addCommand("dumpalloffheapentries", "[help]", "dump all off-heap entries");
	}

	@Override
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		if (0 != args.length) {
			String argument = args[0];

			if (argument.equalsIgnoreCase("help")) {
				help(out);
				return;
			}
		}

		boolean offHeapPresent = false;
		try {
			MM_GCExtensionsPointer extensions = GCBase.getExtensions();
			MM_SparseVirtualMemoryPointer sparseVirualMemory = extensions.largeObjectVirtualMemory();
			if (sparseVirualMemory.notNull()) {
				MM_SparseAddressOrderedFixedSizeDataPoolPointer sparseAddressOrderedFixedSizeDataPool = sparseVirualMemory._sparseDataPool();
				if (sparseAddressOrderedFixedSizeDataPool.notNull()) {
					J9HashTablePointer objectToSparseDataTable = sparseAddressOrderedFixedSizeDataPool._objectToSparseDataTable();
					if (objectToSparseDataTable.notNull()) {
						offHeapPresent = true;
						HashTable<MM_SparseDataTableEntryPointer> readObjectToSparseDataTable = ObjectToSparseDataHashTable.fromJ9HashTable(objectToSparseDataTable);
						SlotIterator<MM_SparseDataTableEntryPointer> readSlotIterator = readObjectToSparseDataTable.iterator();
						long count = readObjectToSparseDataTable.getCount();
						out.format("Off-heap entries (%,d entries)%n", count);
						if (0 < count) {
							out.format("+------------------+------------------+------------------%n");
							out.format("| array object     | data address     | size             %n");
							out.format("+------------------+------------------+------------------%n");

							while (readSlotIterator.hasNext()) {
								MM_SparseDataTableEntryPointer readSparseDataTableEntryPtr2 = MM_SparseDataTableEntryPointer.cast(readSlotIterator.nextAddress());
								VoidPointer dataPrt = readSparseDataTableEntryPtr2._dataPtr();
								VoidPointer proxyObjPtr = readSparseDataTableEntryPtr2._proxyObjPtr();
								UDATA size = readSparseDataTableEntryPtr2._size();

								out.format(" 0x%016x 0x%016x 0x%016x%n",
										proxyObjPtr.getAddress(),
										dataPrt.getAddress(),
										size.longValue());
							}
							out.format("+------------------+------------------+------------------%n");
						}
					}
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		} catch (NoSuchFieldException e) {
			offHeapPresent = false;
		}
		if (!offHeapPresent) {
			out.format("This command requires a core file in which off-heap is enabled.%n");
		}
	}

	private static void help(PrintStream out) {
		out.println("!dumpalloffheapentries       -- dump all off-heap entries");
	}
}
