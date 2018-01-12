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
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageProcess;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RASPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

/**
 * Runs DDR extension !runtimesettings
 */
public class RuntimeSettingsCommand extends Command 
{
	static final private Pattern p = Pattern.compile("(\\d+[KMG]).*");
	static final private String usageInfo = "Prints VM information that can change during runtime (e.g. initial and current softmx values).";
	static final private int PADDING = 4;
	
	public RuntimeSettingsCommand() {
		addCommand("runtimesettings", "", usageInfo);
	}
	
	/**
     * Prints the usage for the runtimesettings command.
     *
     * @param out PrintStream to print the output to the console.
     */
	private void printUsage (PrintStream out) {
		out.println("runtimesettings - " + usageInfo);
	}
	
	/**
	 * Run method for !runtimesettings extension.
	 * 
	 * @param command  !runtimesettings
	 * @param args	args passed by !runtimesettings extension. 
	 * @param context Context of current core file.
	 * @param out PrintStream to print the output to the console.
	 * @throws DDRInteractiveCommandException
	 */	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		if (0 < args.length) {
			out.println("!runtimesettings expects no args. Usage :");
			printUsage(out);
			return;
		}
		
		try {
			Long currentSoftmx;
			String qualifiedCurrentSoftmx = "";
			String initialSoftmx = "not set";
			J9RASPointer ras = DataType.getJ9RASPointer();
			J9JavaVMPointer vm = J9RASHelper.getVM(ras);
			IProcess process = vm.getProcess();
			J9DDRImageProcess ddrProcess = new J9DDRImageProcess(process);
			String cmdline;

			/* Parse the command line of a running program that generated core
			 * file to get the original -Xsoftmx setting
			 */
			cmdline = ddrProcess.getCommandLine();
			int start = cmdline.indexOf("-Xsoftmx");
			int length = "-Xsoftmx".length();
			int end = cmdline.indexOf(" ", start);
			if (-1 != start) {
				/* extract the value from the end of the option */
				initialSoftmx = cmdline.substring(start + length, end);
				initialSoftmx = initialSoftmx.toUpperCase();
			}
			currentSoftmx = new Long(GCExtensions.softMx().longValue());
			qualifiedCurrentSoftmx = currentSoftmx.toString();
			Matcher m = p.matcher(initialSoftmx);

			/* if initial softmx value is set on the command line as qualified
			 * value, print current softmx value in qualified form, otherwise
			 * print current in byte value
			 */
			if (m.matches()) {
				/* User may add multiple letters after the number on the command
				 * line, currently GC parser accepts this and simply ignores
				 * extra letters, so we need to do the same, set initialSoftmx
				 * to the first match of the pattern
				 */
				initialSoftmx = m.group(1);

				/* convert size in bytes held in currentSoftmx to canonical form */
				qualifiedCurrentSoftmx = qualifiedSize(currentSoftmx);

				/* If qualifiedSize() returns size in bytes, it could not
				 * convert the value, so print the initialSoftmx variable in
				 * bytes
				 */
				m = p.matcher(qualifiedCurrentSoftmx);
				if (!m.matches()) {
					initialSoftmx = sizeInBytes(initialSoftmx);
				}
			} else {
				/* InitialSoftmx value has either not been set or is in byte
				 * form, so print current value as byte form
				 */
				qualifiedCurrentSoftmx = currentSoftmx.toString();
			}

			printTableOfEqualSpacedColumns(out, new String[] { "name", "initial value", "current value" }, 
					new String[][] {{ "-Xsoftmx", initialSoftmx, qualifiedCurrentSoftmx }});
			
		} catch (DataUnavailable e) {
			/* For Z/OS core files, command line is not available */
			out.println("COMMANDLINE is not available\n");
		} catch (com.ibm.dtfj.image.CorruptDataException e) {
			throw new DDRInteractiveCommandException(
					"CorruptDataException occured while getting the commandline from process");
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}

	/* TODO: Various suggestions to improve table printing:
	 * - give optional column sizing (in cases where there are long Strings, so table won't get too big)
	 * - move the printing methods to a helper class so other commands could use them
	 */
	private void printTableOfEqualSpacedColumns(PrintStream out, String[] headings, String[][] values) {
		if (!isColumnNumbersValid(headings,values)) {
			out.println("Warning: the number of headers do not match the number of values");
			assert false;
		} else {
			/* use length of the longest string to determine column spacing */
			int max = getLongestStringLength(headings);
			for (int i = 0; i < values.length; i++) {
				max = max(max, getLongestStringLength(values[i]));
			}
			max += PADDING;
			printTableDivider(out, headings.length, max);
			printTableRows(out, max, headings);
			printTableDivider(out, headings.length, max);
			for (String[] strings : values) {
				printTableRows(out, max, strings);
				printTableDivider(out, headings.length, max);
			}
		}
	}
	
	private boolean isColumnNumbersValid(String[] headings, String[][] values) {
		for (int i = 0; i < values.length; i++) {
			if (headings.length != values[i].length) {
				return false;
			}
		}
		return true;
	}
	
	private void printTableRows(PrintStream out, int cellSize, String[] values) {
		for (int j=0; j != values.length; j++) {
			out.print('|');
			int padding = (cellSize-values[j].length())/2;
			for (int i=0; i != padding; i++) {
				out.print(' ');
			}
			out.print(values[j]);
			for (int i=0; i != padding; i++) {
				out.print(' ');
			}
			if (0 == (values[j].length() % 2)) {
				out.print(' ');
			}	
		}
		out.print("|\n");
	}
	
	private void printTableDivider(PrintStream out, int numColumns, int cellSize) {
		for (int j=0; j != numColumns; j++) {
			out.print('+');
			for (int i=0; i != cellSize; i++) {
				out.print('-');
			}
		}
		out.print("+\n");
	}
	
	private int max(int i, int j) {
		if (i > j) {
			return i;
		}
		return j;
	}
	
	private int getLongestStringLength(String[] values){
		String str = values[0];
		for (int i=1; i<values.length; i++) {
			if((values[i]).length() > str.length()) {
				str = values[i];
			}
		}
		return str.length();
	}
	
	/* There is an assumption that the parameter qSize is
	 * a qualified size such as '4K' 
	 */
	private String sizeInBytes(String qSize) {
		Long number = new Long(qSize.substring(0, qSize.length()-1));
		if (qSize.endsWith("K")) {
			number = number * 1024;
		} else if (qSize.endsWith("M")) {
			number = number * 1024 * 1024;
		} else if (qSize.endsWith("G")) {
			number = number * 1024 * 1024 * 1024;
		} else {
			assert false;
		}
		return number.toString();
	}
	
	private String qualifiedSize(Long size) {
		String suffix = "";
		if(0 == (size % 1024)) {
			size /= 1024;
			suffix = "K";
			if((0 < size) && (0 == (size % 1024))) {
				size /= 1024;
				suffix = "M";
				if((0 < size)  && (0 == (size % 1024))) {
					size /= 1024;
					suffix = "G";
				}
			}
		}
		return  size + suffix;
	}
}
