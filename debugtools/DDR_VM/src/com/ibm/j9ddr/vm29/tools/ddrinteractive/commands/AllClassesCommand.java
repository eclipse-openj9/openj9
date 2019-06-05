/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ROMClassesIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ROMClassesRangeIterator;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;


public class AllClassesCommand extends Command 
{
	private static final String nl = System.getProperty("line.separator");
	private static final String rangeDelim = "..";
	
	private boolean dumpROMClasses;
	private boolean dumpRAMClasses;
	private U8Pointer rangeStart;
	private U8Pointer rangeEnd;
	
	public AllClassesCommand()
	{
		addCommand("allclasses", "[help|rom|ram [startAddr..endAddr]]", "Dump a list of loaded ROM/RAM classes. Use 'help' to see more details about the usage.");
	}
	
	private void init() 
	{
		dumpROMClasses = dumpRAMClasses = false;
		rangeStart = rangeEnd = null;
	}
	
	private void printHelp(PrintStream out) {
		CommandUtils.dbgPrint(out, "!allclasses rom [startAddr..endAddr]   -- Dump a list of J9ROMClasses in the given range. If no range is given, all J9ROMClasses are displayed.\n");
		CommandUtils.dbgPrint(out, "                                          Note that 'startAddr' in the range should point to valid J9ROMClass.\n");
		CommandUtils.dbgPrint(out, "!allclasses ram [startAddr..endAddr]   -- Dump a list of J9Classes in the given range. If no range is given, all J9Classes are displayed.\n");
		CommandUtils.dbgPrint(out, "User may specify both 'rom' and 'ram' to dump J9ROMClasses and J9Classes\n");
		CommandUtils.dbgPrint(out, "Note: if no argument is specified, then all J9ROMClasses and J9Classes will be displayed\n");
	}
	
	private boolean getRange(PrintStream out, String arg) throws DDRInteractiveCommandException {
		String addr;
		
		addr = arg.substring(0, arg.indexOf(rangeDelim));
		rangeStart = U8Pointer.cast(CommandUtils.parsePointer(addr, J9BuildFlags.env_data64));
		
		addr = arg.substring(arg.indexOf(rangeDelim) + rangeDelim.length());
		rangeEnd = U8Pointer.cast(CommandUtils.parsePointer(addr, J9BuildFlags.env_data64));
		
		if (rangeStart.getAddress() >= rangeEnd.getAddress()) {
			out.append("Invalid range specified. Ensure 'startAddr' < 'endAddr'" + nl);
			return false;
		}
		return true;
	}
	
	private boolean parseROMRAMRange(PrintStream out, String arg) throws DDRInteractiveCommandException {
		if (arg.equals("rom")) {
			if (dumpROMClasses) {
				out.append("Argument '" + arg + "' is specified twice" + nl);
				return false;
			}
			dumpROMClasses = true;
		} else if (arg.equals("ram")) {
			if (dumpRAMClasses) {
				out.append("Argument '" + arg + "' is specified twice" + nl);
				return false;
			}
			dumpRAMClasses = true;
		} else if (arg.indexOf(rangeDelim) != -1) {
			return getRange(out, arg);
		} else {
			out.append("Unrecognized argument: " + arg + nl);
			return false;
		}
		return true;
	}
	
	private boolean parseROMRAM(PrintStream out, String arg) throws DDRInteractiveCommandException {
		if (arg.equals("rom")) {
			if (dumpROMClasses) {
				out.append("Argument '" + arg + "' is specified twice" + nl);
				return false;
			}
			dumpROMClasses = true;
		} else if (arg.equals("ram")) {
			if (dumpRAMClasses) {
				out.append("Argument '" + arg + "' is specified twice" + nl);
				return false;
			}
			dumpRAMClasses = true;
		} else {
			out.append("Invalid argument: " + arg + nl);
			return false;
		}
		return true;
	}
	
	private boolean parseRange(PrintStream out, String arg) throws DDRInteractiveCommandException {
		if (arg.indexOf(rangeDelim) != -1) {
			return getRange(out, arg);
		} else {
			out.append("Invalid argument: " + arg);
			return false;
		}
	}
	
	private boolean parseArgs(PrintStream out, String[] args) throws DDRInteractiveCommandException {
		if (args.length > 3) {
			/* Maximum 3 arguments can be specified as in: 
			 * 	!allclasses rom ram 0x1000..0x2000 
			 */
			out.append("Invalid number of arguments" + nl);
			return false;
		}
		switch (args.length) {
		case 1:
			if (args[0].equals("help")) {
				printHelp(out);
				return false;
			}
			if (!parseROMRAM(out, args[0])) {
				return false;
			}
			break;
		case 2:
			if (!parseROMRAM(out, args[0])) {
				return false;
			}
			if (!parseROMRAMRange(out, args[1])) {
				return false;
			}
			break;
		case 3:
			if (!parseROMRAM(out, args[0])) {
				return false;
			}
			if (!parseROMRAM(out, args[1])) {
				return false;
			}
			if (!parseRange(out, args[2])) {
				return false;
			}
			break;
		}
		return true;
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		init();
		
		if (args != null) {
			if (!parseArgs(out, args)) {
				return;
			}
		}
		
		boolean useRange = (rangeStart != null && rangeEnd != null);

		/* If none of "rom" or "ram" is specified, then list both type of classes */
		if (!dumpROMClasses && !dumpRAMClasses) {
			dumpROMClasses = dumpRAMClasses = true;
		}
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());

			if (dumpRAMClasses) {
				out.append("RAM classes (ram size loader rom replacement name)" + nl);
				out.append("Class addr\tsize\t\tClassLoader\tROM class addr\tArray\tClass name" + nl);
				out.append(nl);
				ClassSegmentIterator classSegmentIterator = new ClassSegmentIterator(vm.classMemorySegments());
				while (classSegmentIterator.hasNext()) {
					J9ClassPointer classPointer = J9ClassPointer.NULL;
					try {
						classPointer = (J9ClassPointer) classSegmentIterator.next();
		
						if (useRange) {
							if (classPointer.getAddress() < rangeStart.getAddress() ||
								classPointer.getAddress() >= rangeEnd.getAddress()) {
								/* J9Class is outside of specified range; skip it. */
								continue;
							}
						}
						// 0x02713400 0x00000148 0x001210cc 0x02da2f20 0x00000000
						// java/util/regex/Pattern$Begin
						out.append(classPointer.getHexAddress());
						out.append('\t');
						out.append(J9ClassHelper.size(classPointer, vm).getHexValue());
						out.append('\t');
						out.append(classPointer.classLoader().getHexAddress());
						out.append('\t');
						out.append(classPointer.romClass().getHexAddress());
						out.append('\t');
						if (J9ClassHelper.isSwappedOut(classPointer)) {
							out.append(classPointer.arrayClass().getHexAddress());
						} else {
							out.append('0');
						}
						out.append('\t');
						out.append(J9ClassHelper.getJavaName(classPointer));
						out.append(nl);
					} catch (CorruptDataException e) {
						if (useRange) {
							if (classPointer.getAddress() < rangeStart.getAddress() ||
								classPointer.getAddress() >= rangeEnd.getAddress()) {
								/* J9Class that caused CorruptDataException is outside of specified range; skip it. */
								continue;
							}
						}
					}
				}
			}
			
			if (dumpROMClasses) {
				ROMClassesIterator iterator = null;
				
				out.append(nl);
				if (useRange) {
					iterator = new ROMClassesRangeIterator(out, rangeStart, rangeEnd);
					/* Classloader is not available when using ROMClassesRangeIterator */
					out.append("ROM classes (rom size modifiers name)" + nl);
					out.append("Class addr\tROM size\tModifiers\tExtra\t\tClass name" + nl);
				} else {
					iterator = new ROMClassesIterator(out, vm.classMemorySegments());
					out.append("ROM classes (rom size loader modifiers name)" + nl);
					out.append("Class addr\tROM size\tClassLoader\tModifiers\tExtra\t\tClass name" + nl);
				}
				out.append(nl);

				while (iterator.hasNext()) {
					J9ROMClassPointer classPointer = iterator.next();
					out.append(classPointer.getHexAddress());
					out.append('\t');
					out.append(classPointer.romSize().getHexValue());
					/* MemorySegment is not available when using ROMClassesRangeIterator */
					if (iterator.getMemorySegmentPointer() != J9MemorySegmentPointer.NULL) {
						out.append('\t');
						out.append(iterator.getMemorySegmentPointer().classLoader().getHexAddress());
					}
					out.append('\t');
					out.append(classPointer.modifiers().getHexValue());
					out.append('\t');
					out.append(classPointer.extraModifiers().getHexValue());
					out.append('\t');
					out.append(J9UTF8Helper.stringValue(classPointer.className()));
					out.append(nl);
				}
				out.append(nl);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
