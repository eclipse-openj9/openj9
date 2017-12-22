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
package com.ibm.jvm.dtfjview.commands.xcommands;

import java.io.PrintStream;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageSymbol;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class XKCommand extends XCommand {
	{
		addCommand("x/k", "<hex address>", "displays the specified memory section as if it were a stack frame parameters");	
	}
	
	@Override
	public boolean recognises(String command, IContext context) {
		if(super.recognises(command, context)) {
			return command.toLowerCase().endsWith("k");
		}
		return false;
	}
	
	public void doCommand(String[] args)
	{
		String param = args[0];
		
		Long address = Utils.longFromStringWithPrefix(param);
		
		if (null == address)
		{
			out.println("invalid hex address specified; address must be specified as "
					+ "\"0x<hex_address>\"");
			return;
		}
		
		ImageAddressSpace ias = ctx.getAddressSpace();
		int pointerSize = getIASPointerSize(ias);
		int unitSize;
		
		if (pointerSize > 32)
			unitSize = 8;
		else
			unitSize = 4;
		
		out.print("\n");
		
		for (int index = 0; index < argUnitNumber; index++)
		{
			boolean found = false;
			long currAddr = address.longValue() + (index * unitSize);
			ImagePointer ip = ias.getPointer(currAddr);
			
			out.print("\t");
			out.print(Utils.toHex(currAddr));
			out.print("   ");

			long l = 0;
			try {
				l = ip.getPointerAt(0).getAddress();
				found = true;
			} catch (CorruptDataException e) {
				found = false;
			} catch (MemoryAccessException e) {
				found = false;
			}

			if (found) {
				long pointer = l;
				
				out.print(toAdjustedHex(l, pointerSize));
				out.print("   ");

				if (31 == pointerSize) {
					pointer = (int)(pointer & (((long)1) << pointerSize) - 1);
				}
				
				if (printSymbol(pointer, l - currAddr, pointerSize)) {
				} else if (printStackPointer(pointer, l - currAddr, pointerSize, ias)) {
				}
				out.print("\n");
			} else {
				out.print("<address not found in any address space or exception occurred>\n");
			}
		}
		
		out.print("\n");
	}
	
	private boolean printSymbol(long pointer, long diff, int pointerSize)
	{
		ImageProcess ip = ctx.getProcess();
		Iterator itModule;
		try {
			itModule = ip.getLibraries();
		} catch (CorruptDataException e) {
			// FIXME
			itModule = null;
		} catch (DataUnavailable e) {
			// FIXME
			itModule = null;
		}
		while (null != itModule && itModule.hasNext()) {
			ImageModule im = (ImageModule)itModule.next();
			Iterator itImageSection = im.getSections();
			while (itImageSection.hasNext()) {
				ImageSection is = (ImageSection)itImageSection.next();
				long startAddr = is.getBaseAddress().getAddress();
				long endAddr = startAddr + is.getSize();
				
				if (pointer >= startAddr && pointer < endAddr) {
					/* can we find a matching symbol? */
					long maxDifference = pointer - startAddr;
					ImageSymbol bestSymbol = null;
					for (Iterator iter = im.getSymbols(); iter.hasNext();) {
						Object next = iter.next();
						if (next instanceof CorruptData)
							continue;
						ImageSymbol symbol = (ImageSymbol) next;
						long symbolAddress = symbol.getAddress().getAddress();
						if (symbolAddress <= pointer && pointer - symbolAddress < maxDifference) {
							maxDifference = pointer - symbolAddress;
							bestSymbol = symbol;
						}
					}
	
					try {
						out.print(im.getName());
					} catch (CorruptDataException e) {
						out.print(Exceptions.getCorruptDataExceptionString());
					}
					out.print("::");
					if (bestSymbol == null) {
						out.print(is.getName());
					} else {
						out.print(bestSymbol.getName());
					}
					out.print("+");
					out.print(Long.toString(maxDifference));
					
					// if we find the address in the symbols, there's no need to continue
					//  trying to find it elsewhere in the address space
					return true;
				}

			}
		}
		return false;
	}
	
	private boolean printStackPointer(long pointer, long diff, int pointerSize, ImageAddressSpace ias)
	{
		if (Math.abs(diff) <= 4096) {	
			if (diff < 0) {
				out.print(" .");
				out.print(Long.toString(diff));
			} else {
				out.print(" .+");
				out.print(Long.toString(diff));
			}
			return true;
		}
		return false;
	}
	
	private int getIASPointerSize(ImageAddressSpace ias)
	{
		return ((ImageProcess)ias.getProcesses().next()).getPointerSize();
	}

	private String toAdjustedHex(long l, int pointerSize)
	{
		if (pointerSize > 32) {
			return Utils.toFixedWidthHex(l);
		} else if (31 == pointerSize) {
			return Utils.toFixedWidthHex((int)(l & (((long)1) << pointerSize) - 1));
		} else {
			return Utils.toFixedWidthHex((int)l);
		}
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		super.printDetailedHelp(out);
		out.println("displays the specified memory section as if it were a stack frame\n\n" +
				"parameters: 0x<addr>\n\n" +
				"Displays the value of each section (whose size is defined by the " +
				"pointer size of this architecture) of memory, adjusted for the " +
				"endianness of the architecture this dump file is from, starting at the " +
				"specified address.  It also displays a module with a module section " +
				"and an offset from the start of that module section in memory if the " +
				"pointer points to that module section.  " +
				"If no symbol is found, it displays a \"*\" and an offset from the " +
				"current address if the pointer points to an address within 4KB (4096 " +
				"bytes) of the current address.\n\n" +
				"While this command can work on an arbitrary section of memory, it is " +
				"probably most useful when used on a section of memory that refers to a " +
				"stack frame.  To find the memory section of a thread's stack frame, use " +
				"the \"info thread\" command.\n\n" +
				"Note: This command uses the number of items passed to " +
				"it by the \"x/\" command, but ignores the unit size.\n");
	}


}
