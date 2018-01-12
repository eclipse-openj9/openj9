/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands.infocommands;

import java.io.PrintStream;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;

@DTFJPlugin(version="1.*", runtime=false)
public class InfoHeapCommand extends BaseJdmpviewCommand {
	{
		addCommand("info heap", "[*|heap name]", "Displays information about Java heaps");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		out.print("\n");
		
		if (args.length == 0){
			printHeapInfo(null,ctx.getRuntime(), out);
			out.print("\nUse \"info heap *\" or \"info heap <heap_name>\" " +
					"for more information.\n");
			return;
		}
		if (args[0].equals("*") || args[0].equals("ALL")){
			printHeapInfo(args[0],ctx.getRuntime(), out);
			return;
		} else {
			boolean foundHeap = searchForHeap(args[0], ctx.getRuntime(), out);
		
			if (!foundHeap){
				out.print("Unable to locate heap: \"" + args[0] +"\".");
				out.print("\tAvailable heaps: \n");
				printHeapInfo(null,ctx.getRuntime(), out);
			}
		}
	}
	


	private void printHeapInfo(String param, JavaRuntime runtime, PrintStream out){
		
		Iterator itHeaps = runtime.getHeaps();
		int countheaps = 1;
		
		while (itHeaps.hasNext())
		{
			Object heap = itHeaps.next();
			if(heap instanceof CorruptData) {
				out.println("[skipping corrupt heap");
				continue;
			}
			JavaHeap theHeap = (JavaHeap)heap;
			out.print("\t Heap #" + countheaps + ":  " + theHeap.getName()+ "\n");
			if (param != null){
				printSectionInfo(theHeap, out);
			}
			countheaps++;
		}
	}
	private void printSectionInfo(JavaHeap theHeap, PrintStream out){
		
		Iterator itSections = theHeap.getSections();
		int countSections = 1;
		
		while (itSections.hasNext()){
			
			ImageSection theSection = (ImageSection)itSections.next();
			out.print("\t  Section #"+ countSections + ":  " + theSection.getName() + "\n");
			out.print("\t   Size:        " + theSection.getSize() + " bytes\n");
			try 
			{
				out.print("\t   Shared:      " + theSection.isShared() + "\n");
				out.print("\t   Executable:  " + theSection.isExecutable() +"\n");
				out.print("\t   Read Only:   " + theSection.isReadOnly() + "\n");
			}
			catch(DataUnavailable e){
				out.print("\t   Shared:      <unknown>\n");
				out.print("\t   Executable:  <unknown>\n");
				out.print("\t   Read Only:   <unknown>\n");
			}
			countSections++;
		}
	}
	private boolean searchForHeap(String param, JavaRuntime jr, PrintStream out){
		
		boolean foundHeap = false;
		
		Iterator itHeaps = jr.getHeaps();
		int countheaps = 1;
		
		while (itHeaps.hasNext())
		{
			JavaHeap theHeap = (JavaHeap)itHeaps.next();
			if (theHeap.getName().indexOf(param)==0){
				out.print("\t Heap #" + countheaps + ":  " + theHeap.getName()+"\n");
				printOccupancyInfo(theHeap, out);
				printSectionInfo(theHeap, out);
				foundHeap = true;
			}
			countheaps++;
		}
		
		return foundHeap;
	}

	private void printOccupancyInfo(JavaHeap theHeap, PrintStream out){
		/*
		 * This method takes a lot of time and hence this information is only included 
		 * when using "info heap <heapname>". If this method was run for every heap 
		 * when using "info heap *" the amount of time it would take would be astronomical.
		 */
		long size = 0;
		long totalObjectSize = 0;
		long totalObjects = 0;				//total number of objects on the heap
		long totalCorruptObjects = 0;		//total number of corrupt objects 
		
		Iterator itSections = theHeap.getSections();
		Object obj = null;					//object returned from various iterators
		CorruptData cdata = null;			//corrupt data
		while (itSections.hasNext()){
			obj = itSections.next();
			if(obj instanceof CorruptData) {	//returned section is corrupt
				cdata = (CorruptData) obj;
				out.print("\t\t Warning - corrupt image section found");
				if(cdata.getAddress() != null) {
					out.print(" at 0x" + cdata.getAddress().toString());
				}
				out.print("\n");
			} else {							//returned section is valid, so process it
				ImageSection theSection = (ImageSection) obj;
				size = size + theSection.getSize();
			}
		}
		out.print("\t  Size of heap: "+ size + " bytes\n");
		
		Iterator itObjects = theHeap.getObjects();
		try{
			while (itObjects.hasNext()){
				obj = itObjects.next();
				totalObjects++;
				if(obj instanceof CorruptData) {
					totalCorruptObjects++;
					cdata = (CorruptData) obj;
					out.print("\t\t Warning - corrupt heap object found at position " + totalObjects);
					if(cdata.getAddress() != null) {
						out.print(" address 0x" + cdata.getAddress().toString());
					}
					out.print("\n");
				} else {
					JavaObject theObject = (JavaObject) obj;
					totalObjectSize = totalObjectSize + theObject.getSize();
				}
			}
			
			float percentage = ((float)totalObjectSize/(float)size)*10000; 
			int trimmedPercent = ((int)percentage); // Sending this float through an int gets it down to 2 decimal places.
			percentage = ((float)trimmedPercent)/100;
			
			out.print("\t  Occupancy               :   "+ totalObjectSize + " bytes  (" + percentage + "%)\n");
			out.print("\t  Total objects           :   "+ totalObjects + "\n");
			out.print("\t  Total corrupted objects :   "+ totalCorruptObjects + "\n");
		
		} catch (CorruptDataException e){
			out.print("\t  Occupancy :   <unknown>\n");
		}
		
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("displays information about Java heaps\n\n" +
				"parameters: none, \"*\", or heap name\n\n" +
				" none prints:\n" +
				" - heap name\n" +
				" - heap section\n" +
				" \"*\" or heap name prints the following information\n" +
				" about all heaps or the specified heap, respectively:\n" +
				" - heap name\n" +
				" - (heap size and occupancy)\n" +
				" - heap sections\n" +
				"  - section name\n" +
				"  - section size\n" +
				"  - whether the section is shared\n" +
				"  - whether the section is executable\n" +
				"  - whether the section is read only\n"
				);
	}
	
}
