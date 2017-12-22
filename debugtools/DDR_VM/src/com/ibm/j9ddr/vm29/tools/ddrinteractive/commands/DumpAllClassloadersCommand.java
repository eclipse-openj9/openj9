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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPuddlePointer;

public class DumpAllClassloadersCommand extends Command 
{
	/**
	 * Constructor
	 */
	public DumpAllClassloadersCommand()
	{
		addCommand("dumpallclassloaders", "", "dump all class loaders");
	}
	
	/**
     * Prints the usage for the dumpallclassloaders command.
     *
     * @param out  the PrintStream the usage statement prints to
     */
	private void printUsage (PrintStream out) {
		out.println("dumpallclassloaders - dump all class loaders");
	}
	
	/**
	 * Run method for !dumpallclassloaders extension.
	 * 
	 * @param command  !dumpallclassloaders
	 * @param args	!dumpallclassloaders extension accepts no args
	 * @param context Context
	 * @param out PrintStream
	 * @throws DDRInteractiveCommandException
	 */
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (args.length != 0) {
			printUsage(out);
			return;
		}
		
		try {
 			if (J9BuildFlags.env_data64) {
				out.append("+------------------------------------------------------------------------- \n");
				out.append("|    ClassLoader    | SharedLibraries | ClassHashTable  |   jniIDs Pool   |\n");
				out.append("|                   |      Pool       |                 |                 |\n");
				out.append("|                   |-----------------+-----------------+-----------------| \n");
				out.append("|      Address      | used | capacity | used | capacity | used | capacity |\n");
				out.append("+-------------------+-----------------+-----------------+----------------- \n");
			} else {
				out.append("+----------------------------------------------------------------- \n");
				out.append("|ClassLoader| SharedLibraries | ClassHashTable  |   jniIDs Pool   |\n");
				out.append("|           |      Pool       |                 |                 |\n");
				out.append("|           |-----------------+-----------------+-----------------| \n");
				out.append("| Address   | used | capacity | used | capacity | used | capacity |\n");
				out.append("+-----------+-----------------+-----------------+----------------- \n");
			}
			
			GCClassLoaderIterator iterator = GCClassLoaderIterator.from();
			
			J9ClassLoaderPointer classLoaderPointer;
			String classLoaderAddress;
			long sharedLibPoolNumOfelements;
			long sharedLibPoolCapacity;
			long classHashTableNumOfelements;
			long classHashTableCapacity;
			long jniIDsPoolNumOfelements;
			long jniIDsPoolCapacity;
			
			while (iterator.hasNext()) {
				classLoaderPointer = iterator.next();
				classLoaderAddress = classLoaderPointer.getHexAddress();
				
				J9PoolPointer sharedLibraries = classLoaderPointer.sharedLibraries();
				if (!sharedLibraries.isNull()) {
					Pool<J9PoolPuddlePointer> pool = Pool.fromJ9Pool(sharedLibraries, J9PoolPuddlePointer.class);
					sharedLibPoolNumOfelements = pool.numElements();
					sharedLibPoolCapacity = pool.capacity();
				} else {
					sharedLibPoolNumOfelements = 0;
					sharedLibPoolCapacity = 0;
				}

				J9HashTablePointer classHashTable = classLoaderPointer.classHashTable();
				if (!classHashTable.isNull()) {
					if (!classHashTable.listNodePool().isNull()) {
						J9PoolPointer listNodePool = classHashTable.listNodePool();
						Pool<J9PoolPuddlePointer> pool = Pool.fromJ9Pool(listNodePool, J9PoolPuddlePointer.class);
						classHashTableNumOfelements = pool.numElements();
						classHashTableCapacity = pool.capacity();
					} else {
						classHashTableNumOfelements = classHashTable.numberOfNodes().longValue();
						classHashTableCapacity = classHashTable.tableSize().longValue();
					}
				} else {
					classHashTableNumOfelements = 0;
					classHashTableCapacity = 0;
				}

				J9PoolPointer jniIDs = classLoaderPointer.jniIDs();
				if (!jniIDs.isNull()) {
					Pool<J9PoolPuddlePointer> pool = Pool.fromJ9Pool(jniIDs, J9PoolPuddlePointer.class);
					jniIDsPoolNumOfelements = pool.numElements();
					jniIDsPoolCapacity = pool.capacity();
				} else {
					jniIDsPoolNumOfelements = 0;
					jniIDsPoolCapacity = 0;
				}
				
				String output = format(classLoaderAddress, 
						sharedLibPoolNumOfelements, sharedLibPoolCapacity, 
						classHashTableNumOfelements, classHashTableCapacity,
						jniIDsPoolNumOfelements, jniIDsPoolCapacity);
				out.println(output);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	/**
	 *  This method is used to generate a string with the given values which is aligned with the columns.
	 *  
	 * @param addr ClassLoader Address
	 * @param used1 sharedLibPoolNumOfelements
	 * @param capacity1 sharedLibPoolCapacity
	 * @param used2 classHashTableNumOfelements
	 * @param capacity2 classHashTableCapacity
	 * @param used3 jniIDsPoolNumOfelements
	 * @param capacity3 jniIDsPoolCapacity
	 * @return Formatted string that aligns with columns
	 */
	private String format(String addr, long used1, long capacity1, long used2, long capacity2, long used3, long capacity3) {
		String output;
		String format = " %s%4d" 
				+ "%" + (7 - Long.toString(used1).length() + Long.toString(capacity1).length()) + "d"  
				+ "%" + (11 - Long.toString(capacity1).length() + Long.toString(used2).length()) + "d"  
				+ "%" + (7 - Long.toString(used2).length() + Long.toString(capacity2).length()) + "d"  
				+ "%" + (11 - Long.toString(capacity2).length() + Long.toString(used3).length()) + "d"  
				+ "%" + (7 - Long.toString(used3).length() + Long.toString(capacity3).length()) + "d" ; 
		output = String.format(format, addr, used1, capacity1, used2, capacity2, used3, capacity3);
		return output;
	}
}
