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
import java.util.Enumeration;
import java.util.Properties;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageProcess;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RASPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9JavaVMHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

/**
 * Runs DDR extension !coreinfo
 * 
 * @author fkaraman
 *
 */
public class CoreInfoCommand extends Command 
{
	private static final String nl = System.getProperty("line.separator");
	
	public CoreInfoCommand()
	{
		addCommand("coreinfo", "", "Prints commandline, platform and -version info of VM found in the current core file.");
	}
	
	/**
     * Prints the usage for the j9reg command.
     *
     * @param out PrintStream to print the output to the console.
     */
	private void printUsage (PrintStream out) {
		out.println("coreinfo - Prints commandline, platform and -version info of VM found in the current core file.");
	}
	
	/**
	 * Run method for !coreinfo extension.
	 * 
	 * @param command  !coreinfo
	 * @param args	args passed by !coreinfo extension. 
	 * @param context Context of current core file.
	 * @param out PrintStream to print the output to the console.
	 * @throws DDRInteractiveCommandException
	 */	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (0 < args.length) {
			out.println("!coreinfo expects no args. Usage :");
			printUsage(out);
			return;
		}
		
		J9JavaVMPointer vm;
		try {
			J9RASPointer ras = DataType.getJ9RASPointer();
			vm = J9RASHelper.getVM(ras);
			IProcess process = vm.getProcess();
			J9DDRImageProcess ddrProcess = new J9DDRImageProcess(process);

			try {
				/* Print the command line of a running program that generated core file */
				out.println("COMMANDLINE\n" + ddrProcess.getCommandLine() + "\n");
			} catch (DataUnavailable e) {
				/*For Zos core files, commandline is not available */
				out.println("COMMANDLINE is not available\n");
			} catch (com.ibm.dtfj.image.CorruptDataException e) {
				throw new DDRInteractiveCommandException("CorruptDataException occured while getting the commandline from process");
			}
			
			Properties properties = J9JavaVMHelper.getSystemProperties(vm);
			
			/* Print VM service level info */
			out.println("JAVA SERVICE LEVEL INFO\n" + ras.serviceLevel().getCStringAtOffset(0));
			/* Print Java Version Info */
			out.println("JAVA VERSION INFO\n" + properties.get("java.fullversion"));
			/* Print Java VM Version Info */
			out.println("JAVA VM VERSION\t- " + properties.get("java.vm.version") + "\n");
			
			/* Print Platform Info */
			boolean is64BitPlatform = (process.bytesPerPointer() == 8) ? true : false;
			ICore core = vm.getProcess().getAddressSpace().getCore();
			Platform platform = core.getPlatform(); 
			out.println("PLATFORM INFO");
			out.print("Platform Name :\t" + platform.name());
			if (is64BitPlatform) {
				out.println(" 64Bit");
			} else {
				out.println(" 32Bit");
			}
			out.println("OS Level\t: " + ras.osnameEA().getCStringAtOffset(0) + " " + ras.osversionEA().getCStringAtOffset(0));
			out.println("Processors -");
			out.println("  Architecture\t: " + ras.osarchEA().getCStringAtOffset(0));
			out.println("  How Many\t: " + ras.cpus().longValue());
			
			try {
				properties = ddrProcess.getEnvironment();
				Enumeration processPropEnum = properties.keys();
				out.println("\nENVIRONMENT VARIABLES");
				while (processPropEnum.hasMoreElements()) {
					String key = (String)processPropEnum.nextElement();
					out.println(key + "=" + properties.get(key));
				}
			} catch (com.ibm.dtfj.image.CorruptDataException e) {
				throw new DDRInteractiveCommandException("CorruptDataException occured while getting the environment variables from process");
			} catch (DataUnavailable e) {
				out.println("Environment variables are not available\n");
			}
			
		} catch (CorruptDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} 
	}
}
