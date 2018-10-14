/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.HashTable;
import com.ibm.j9ddr.vm29.j9.ModuleHashTable;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.j9.PackageHashTable;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ModulePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PackagePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.JavaVersionHelper;

/**
 * Dumpmoduleexports command displays all display all j9packages exported by the target module
 * 
 * Example:
 * !dumpmoduleexports 0x00000130550C7E88
 * Example output: 
 * !j9package 0x00000130550DB1A8
 * !j9package 0x00000130550DB1F8
 * !j9package 0x00000130550DB248
 * .....(A list of every package the module exports)
 */
public class DumpModuleExportsCommand extends Command
{

	public DumpModuleExportsCommand()
	{
		addCommand("dumpmoduleexports", "<targetModuleAddress>", "display all j9packages exported by the target module");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (args.length != 1) {
			CommandUtils.dbgPrint(out, "Usage: !dumpmoduleexports <targetModuleAddress>\n");
			return;
		}			
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (JavaVersionHelper.ensureJava9AndUp(vm, out)) {
				GCClassLoaderIterator iterator = GCClassLoaderIterator.from();
				String targetModuleAddress = args[0];
				while (iterator.hasNext()) {
					J9ClassLoaderPointer classLoaderPointer = iterator.next();
					HashTable<J9PackagePointer> packageHashTable = PackageHashTable
							.fromJ9HashTable(classLoaderPointer.packageHashTable());
					SlotIterator<J9PackagePointer> slotIterator = packageHashTable.iterator();
					while (slotIterator.hasNext()) {
						J9PackagePointer packagePtr = slotIterator.next();
						if (packagePtr.module().getHexAddress().equals(targetModuleAddress)) {
							String packageName = J9UTF8Helper.stringValue(packagePtr.packageName());
							String hexAddress = packagePtr.getHexAddress();
							out.printf("%-45s !j9package %s%n", packageName, hexAddress);
						}
					}
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
