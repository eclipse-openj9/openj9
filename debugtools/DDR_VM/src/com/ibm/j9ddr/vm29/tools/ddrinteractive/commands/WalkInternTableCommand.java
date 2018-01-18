/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.generated.J9DbgROMClassBuilderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9DbgStringInternTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9InternHashTableEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SRPHashTableInternalPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SRPHashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SharedInternSRPHashTableEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SharedInvariantInternTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9TranslationBufferSetPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9SharedInternSRPHashTableEntry;

public class WalkInternTableCommand extends Command 
{

	private static final String nl = System.getProperty("line.separator");
	private static final String tab = "\t";
	
	
	public WalkInternTableCommand()
	{
		addCommand("walkinterntable", "1|2|3|4|5|6|help", "Prints string intern table information or help");
	}
	
	/**
	 * This method is used to run !walkinterntable command. 
	 * 
	 * @param	command 	!walkinterntable
	 * @param 	args		Args passed by command !walkinterntable.
	 * !walkinterntable prints the usage if no arg is passed, otherwise it accepts valid integer arg.
	 * If the arg is not valid, then user is warned and usage is printed. 
	 * @param 	context
	 * @param 	out		PrintStream is used to print the messages in this class. 
	 * @return	void
	 * 		
	 */
	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {
		try {
			int userSelection;
		
			if (args.length == 0) {
				printUsage(out);
			} else if (args.length == 1) {
				if (args[0].trim().equalsIgnoreCase("help")) {
					printUsage(out);
					return;
				}
				try {
					userSelection = Integer.parseInt(args[0]);
					runWalkInternTableSelection(userSelection, out, context, false);
					return;
				} catch (NumberFormatException e) {
					out.append("Error: Invalid Option :" + args[0] + nl);
					printUsage(out);
					return;
				}
			} else {
				out.append("Error: Too many options : ");
				for (int i = 0; i < args.length; i++ ) {
					out.append(args[i].toString());
					if (i + 1 < args.length) {
						out.append(", ");
					} 
				}
				out.append(nl + "!walkintertable expects none(to print usage and valid args) or one integer arg." + nl);
				printUsage(out);
				return;
			}			
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	/**
	 * This method is used when user requests to go into the sub menu. 
	 * User stays in the sub menu until he/she requests to exit sub menu.
	 * Therefore this method is recursive until exit is requested.
	 * @param context	Context
	 * @param out	PrintStream
	 * @throws DDRInteractiveCommandException
	 * @return void
	 */
	private void runSubMenu(Context context, PrintStream out) throws DDRInteractiveCommandException {
		String userInput;
		BufferedReader reader;
		int userSelection;
		reader = new BufferedReader(new InputStreamReader(System.in));
		out.append("!walkinterntable sub menu command >  " + nl);
		try{
			try {
				userInput = reader.readLine();
			} catch (IOException e) {
				out.append("Exception occured while reading the user input" + nl);
				e.printStackTrace();
				return;
			}
			try {			
				if (userInput.compareToIgnoreCase("exit") == 0) {
					out.append("Exiting !walkinterntable sub menu..." + nl);
					return;
				}			
				userSelection = Integer.parseInt(userInput);
				runWalkInternTableSelection(userSelection, out, context, true);
			} catch (NumberFormatException e) {
				userInput = userInput.trim();

				if (userInput.toLowerCase().equals("quit")) {
					out.println("To quit, please go back to main menu. (Type \"exit\")" + nl);
				} else {
					String components[] = userInput.split("\\s");
					String arguments[] = new String[components.length - 1];

					for (int i = 1; i < components.length; i++) {
						arguments[i - 1] = components[i];
					}
					context.execute(components[0].toLowerCase(), arguments, out);
				}
			}
			runSubMenu(context, out);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	/**
	 * 
	 * @param selection		Sub menu option selected by user.
	 * @param out			PrinStream. 
	 * @param context		Context
	 * @param inSubMenu		Used to tell whether we are in sub menu or not. 
	 * @throws CorruptDataException
	 * @throws DDRInteractiveCommandException
	 * @return void
	 */
	private void runWalkInternTableSelection(int selection, PrintStream out, Context context, boolean inSubMenu) throws CorruptDataException,DDRInteractiveCommandException {
		switch (selection) {
		case 1:
			printSharedTableStructure(out);
			break;
		case 2:
			printLocalTableStructure(out);
			break;
		case 3:
			walkSharedTable(out);
			break;
		case 4:
			walkLocalTable(out);
			break;
		case 5:
			walkSharedTable(out);
			walkLocalTable(out);
			break;
		case 6:
			printSubMenu(out);
			if (!inSubMenu) {
				runSubMenu(context, out);
			}
			break;
		default:
			out.append("Error: Invalid option = " + selection + nl);
			printMainOptions(out);
		}
	}

	/**
	 * 
	 * typedef struct J9SharedInvariantInternTable { 
	 * 		UDATA (*performNodeAction)(struct J9SharedInvariantInternTable *sharedInvariantInternTable, struct J9SharedInternSRPHashTableEntry *node, UDATA action, void* userData); 
	 * 		UDATA flags; 
	 * 		omrthread_monitor_t *tableInternFxMutex; 
	 * 		struct J9SRPHashTable* sharedInvariantSRPHashtable;
	 *		struct J9SharedInternSRPHashTableEntry* headNode; 
	 *		struct J9SharedInternSRPHashTableEntry* tailNode; 
	 *		J9SRP* sharedTailNodePtr;
	 *	 	J9SRP* sharedHeadNodePtr; 
	 *		U_32* totalSharedNodesPtr; 
	 *		U_32* totalSharedWeightPtr; 
	 *		struct J9ClassLoader* systemClassLoader; 
	 *} J9SharedInvariantInternTable;
	 * 
	 * 
	 * typedef struct J9SRPHashTable { 
	 * 		const char* tableName; 
	 * 		struct J9SRPHashTableInternal* srpHashtableInternal; 
	 * 		UDATA (*hashFn)(void *key, void *userData); 
	 * 		UDATA (*hashEqualFn)(void *existingEntry,void *key, void *userData); 
	 * 		void (*printFn)(J9PortLibrary *portLibrary, void *key, void *userData); 
	 * 		struct J9PortLibrary* portLibrary; 
	 * 		void* functionUserData; 
	 * 		UDATA flags; 
	 * } J9SRPHashTable;
	 * 
	 * 
	 * typedef struct J9SRPHashTableInternal { 
	 * 		U_32 tableSize; 	
	 * 		U_32 numberOfNodes; 
	 * 		U_32 entrySize; 
	 * 		U_32 nodeSize; 
	 * 		U_32 flags; 
	 * 		J9SRP nodes;
	 * 		J9SRP nodePool; 
	 * } J9SRPHashTableInternal;
	 *
	 * 
	 * Prints the info for three VM structures that are shown above and used for shared intern table. 
	 * 1. 	J9SharedInvariantInternTable
	 * 2.	J9SRPHashTable
	 * 3. 	J9SRPHashTableInternal
	 * 
	 * @param 	out		PrintStream
	 * @throws	CorruptDataException 
	 * @return 	void 
	 */
	private void printSharedTableStructure(PrintStream out)throws CorruptDataException  {
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		J9SharedInvariantInternTablePointer sharedInternTable;
		sharedInternTable = vm.sharedInvariantInternTable();
		if (!sharedInternTable.isNull()) {
			out.println(sharedInternTable.formatFullInteractive());
		}
		out.append("Total Shared Weight : " + sharedInternTable.totalSharedWeightPtr().at(0).longValue() + nl);
		J9SRPHashTablePointer srphashtable = sharedInternTable.sharedInvariantSRPHashtable();
		if (!srphashtable.isNull()) {
			out.println(srphashtable.formatFullInteractive());
		}

		J9SRPHashTableInternalPointer srpHashTableInternal = srphashtable.srpHashtableInternal();
		if (!srpHashTableInternal.isNull()) {
			out.println(srpHashTableInternal.formatFullInteractive());
		}
	}
	
	/**
	 * 
	 * typedef struct J9DbgStringInternTable {
	 *		struct J9JavaVM* vm;
	 *     	struct J9PortLibrary* portLibrary;
	 *      struct J9HashTable* internHashTable;
	 *      struct J9InternHashTableEntry* headNode;
	 *      struct J9InternHashTableEntry* tailNode;
	 *      UDATA nodeCount;
	 *      UDATA maximumNodeCount;
	 * } J9DbgStringInternTable;
	 * 
	 * Prints the info about the J9DbgStringInternTable structure used for local string interning. 
	 * @param out PrintStream
	 * @throws CorruptDataException
	 * @return void
	 * 
	 */
	private void printLocalTableStructure(PrintStream out)throws CorruptDataException {					
		J9DbgStringInternTablePointer stringInternTablePtr = getRomClassBuilderPtr(out);
		if (!stringInternTablePtr.isNull()) {
			out.println(stringInternTablePtr.formatFullInteractive());
		}
	}

	/**
	 * Walks through the shared string intern table nodes and prints the info about them on each line. 
	 * @param out
	 * @throws CorruptDataException
	 * @return void
	 */
	private void walkSharedTable(PrintStream out) throws CorruptDataException {
		int totalWeight = 0;
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		J9SharedInvariantInternTablePointer sharedInternTable = vm.sharedInvariantInternTable();
		/* Check if there is sharedInternTable before to go further */
		if (!sharedInternTable.isNull()) {
			J9SharedInternSRPHashTableEntryPointer currentEntry = sharedInternTable.headNode();
			int counter = 1;

			out.append("=================================================================================" + nl);
			out.append(tab(2) + "WALKING SHARED INTERN SRP HASHTABLE (SRPHashTable " + sharedInternTable.sharedInvariantSRPHashtable().getHexAddress() + ")" + nl);
			out.append(tab(2) + "FROM: MRU (MOST RECENTLY USED)" + nl);
			out.append(tab(2) + "TO: LRU (LEAST RECENTLY USED)" + nl);
			out.append("=================================================================================" + nl);

			while (!currentEntry.isNull()) {
				out.append(counter	+ "." + tab 
									+ "Shared Table Entry < !J9SharedInternSRPHashTableEntry "
									+ currentEntry.getHexAddress()
									+ " Flags: "
									+ currentEntry.flags().getHexValue()
									+ " IWeight: "
									+ currentEntry.internWeight().longValue()
									+ " IsUTF8Shared: "
									+ currentEntry.flags().allBitsIn(J9SharedInternSRPHashTableEntry.STRINGINTERNTABLES_NODE_FLAG_UTF8_IS_SHARED)
									+ ">" + tab + "UTF8 <Add: "
									+ currentEntry.utf8SRP().getHexAddress()
									+ " Data: \""
									+ J9UTF8Helper.stringValue(currentEntry.utf8SRP())
									+ "\">" + nl);
				totalWeight += currentEntry.internWeight().longValue();
				currentEntry = currentEntry.nextNode();
				counter++;
			}
			out.append("Total Weight = " + totalWeight + nl );
			out.append("=================================================================================" + nl);
			out.append(tab(2) + "WALKING SHARED INTERN SRP HASHTABLE COMPLETED" + nl);
			out.append("=================================================================================" + nl);
		}
	}

	/**
	 * Walks through the local string intern table nodes and prints the info about them on each line. 
	 * @param out
	 * @throws CorruptDataException
	 * @return void
	 * 
	 */
	private void walkLocalTable(PrintStream out) throws CorruptDataException {
		int totalWeight = 0;
		J9DbgStringInternTablePointer stringInternTablePtr = getRomClassBuilderPtr(out);
		
		if (stringInternTablePtr.isNull()) {
			out.append("StringInternTable is null" + nl);
			return;
		}
		
		J9InternHashTableEntryPointer currentEntryPtr = stringInternTablePtr.headNode();
		if (currentEntryPtr.isNull()) {
			out.append("HeadNode is null" + nl);
			return;
		}
		
		int counter = 1;
		
		out.append("=================================================================================" + nl);
		out.append(tab(2) + "WALKING LOCAL INTERN HASHTABLE (stringInternTable )" + stringInternTablePtr.getHexAddress() + ")" + nl);
		out.append(tab(2) + "FROM: MRU (MOST RECENTLY USED)" + nl);
		out.append(tab(2) + "TO: LRU (LEAST RECENTLY USED)" + nl);
		out.append("=================================================================================" + nl);
	
		while (!currentEntryPtr.isNull()) {
			out.append(counter	+ "." + tab 
								+ "Local Table Entry < !J9InternHashTableEntry "
								+ currentEntryPtr.getHexAddress()
								+ " Flags: "
								+ currentEntryPtr.flags().getHexValue()
								+ " IWeight: "
								+ currentEntryPtr.internWeight().longValue()
								+ " ClassLoader: "
								+ "!J9ClassLoader "
								+ currentEntryPtr.classLoader().getHexAddress()
								+ ">" + tab + "UTF8 <Add: "
								+ currentEntryPtr.utf8().getHexAddress()
								+ " Data: \""
								+ J9UTF8Helper.stringValue(currentEntryPtr.utf8())
								+ "\">" + nl);
			totalWeight += currentEntryPtr.internWeight().longValue();
			currentEntryPtr = currentEntryPtr.nextNode();
			counter++;			
		}
		out.append("Total Weight = " + totalWeight + nl );
		out.append("=================================================================================" + nl);
		out.append(tab(2) + "WALKING LOCAL INTERN HASHTABLE COMPLETED" + nl);
		out.append("=================================================================================" + nl);
	}

	/**
	 * Prints the !walkinterntable sub menu
	 * @param out
	 * @return void
	 */
	private void printSubMenu(PrintStream out) {
		out.append("INFO: You are in !walkinterntable sub menu." + nl);
		printMainOptions(out);
		out.append("6" + tab(1) + "To print this menu again" + nl);
		out.append("Type 'exit' to exit the sub menu and go back to the main menu. " + nl);
		out.append("Enter one of the options above or any DDR command " + nl);
		out.append("? ");
	}

	/**
	 * Prints the usage of !walkinterntable
	 * @param out
	 * @return void
	 */
	private void printUsage(PrintStream out) {
		out.append("USAGE: !walkinterntable <Sub Menu Option>" + nl);
		printMainOptions(out);
		out.append("6" + tab(1) + "To go into !walkinterntable sub menu" + nl);
	}
	
	/**
	 * Prints !walkinterntable sub menu options. 
	 * @param out
	 * @return void
	 */
	private void printMainOptions(PrintStream out) {
		out.append("Walkinterntable Sub Menu Options :" + nl);
		out.append("1" + tab(1) +  "To Print Shared Table Structural Info" + nl);
		out.append("2" + tab(1) + "To Print Local Table Structural Info" + nl);
		out.append("3" + tab(1) + "To Walk Shared Intern Table (From: Most recently used To: Least recently used)"	+ nl);
		out.append("4" + tab(1) + "To Walk Local Intern Table (From: Most recently used To: Least recently used)"	+ nl);
		out.append("5" + tab(1) + "To Walk Both Shared&Local Intern Table (From: Most recently used To; Least recently used)" + nl);
	}
	
	/**
	 * Generates a string of tabs.
	 * @param size number of tabs requested
	 * @return	String generated by number of tabs. 
	 */
	private String tab(int size) {
		String tabs = "";
		for (int i = 0; i < size; i++) {
			tabs += tab;
		}
		return tabs;
	}
	
	/**
	 * This method is used to get the pointer to the RomClassBuilder
	 * @param out	PrintStream
	 * @return void
	 * @throws CorruptDataException
	 */
	private J9DbgStringInternTablePointer getRomClassBuilderPtr(PrintStream out) throws CorruptDataException{
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (vm.isNull()) {
				out.append("VM can not be found " + nl);
				return null;
			}				
		
			J9TranslationBufferSetPointer translationBufferSetPtr = vm.dynamicLoadBuffers();
			if (translationBufferSetPtr.isNull()) {
				out.append("J9TranslationBufferSet can not be found " + nl);
				return null ;
			}
		
			J9DbgROMClassBuilderPointer romClassBuilderPtr = J9DbgROMClassBuilderPointer.cast(translationBufferSetPtr.romClassBuilder());
			if (romClassBuilderPtr.isNull()) {
				out.append("romClassBuilderPtr can not be found " + nl);
				return null;
			}
			
			return romClassBuilderPtr.stringInternTable();
			
		} catch (CorruptDataException e) {
			throw e;
		}
	}

}
