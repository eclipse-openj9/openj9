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
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageSymbol;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;

@DTFJPlugin(version="1.*", runtime=false)
public class InfoSymCommand extends BaseJdmpviewCommand {

	protected static final String longDesc = "parameters: none, module name\n\n"
			+ "if no parameters are passed then for each process in the "
			+ "address spaces, outputs a list of module sections for each "
			+ "module with their start and end addresses, names, and sizes\n"
			+ "if a module name is passed then it outputs the above "
			+ "information for that module plus a list of all the known symbols "
			+ "for that module. "
			+ "Module names can be specified with leading or trailing * characters "
			+ "to save specifying full paths or remembering what file extension is "
			+ "used on this platform. e.g. info mod *libzip* "
			+ "info mod * will all modules with sections " + "and symbols.";

	{
		addCommand("info sym", "", "an alias for 'mod'");				//this is kept for backwards compatibility
		addCommand("info mod", "", "outputs module information");
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		switch(args.length) {
			case 0 :
				listModules(null);
				break;
			case 1 :
				listModules(args[0]);
				break;
			default :
				out.println("\"info sym\" command takes either 0 or 1 parameters");
				break;
		}
	}

	private void listModules(String moduleName) {
		ImageProcess ip = ctx.getProcess();

		try {
			Object e = ip.getExecutable();
			if( e instanceof ImageModule ) {
				ImageModule exe = (ImageModule) e;
				if (moduleName != null ) {
					if ( checkModuleName(exe.getName(), moduleName)) {
						printModule(exe, true);
					}
				} else {
					printModule(exe, false);
				}
			} else if (e instanceof CorruptData) {
				CorruptData corruptObj = (CorruptData) e;
				// warn the user that this image library is corrupt
				out.print("\t  <corrupt executable encountered: "
						+ corruptObj.toString() + ">\n\n");
			}
		} catch (DataUnavailable e) {
			out.println(Exceptions.getDataUnavailableString());
		} catch (CorruptDataException e) {
			out.println(Exceptions.getCorruptDataExceptionString());
		}
		Iterator iLibs;
		try {
			iLibs = ip.getLibraries();
		} catch (DataUnavailable du) {
			iLibs = null;
			out.println(Exceptions.getDataUnavailableString());
		} catch (CorruptDataException cde) {
			iLibs = null;
			out.println(Exceptions.getCorruptDataExceptionString());
		}
				
		// iterate through the libraries
		while (null != iLibs && iLibs.hasNext()) {
			Object next = iLibs.next();
			if (next instanceof ImageModule) {
				ImageModule mod = (ImageModule) next;
				String currentName = null;
				try {
					currentName = mod.getName();
				} catch (CorruptDataException e) {
					out.print("\t  <corrupt library name: "
							+ mod.toString() + ">\n\n");
				}
				if (moduleName != null ) {
					if ( checkModuleName(currentName, moduleName)) {
						printModule(mod, true);
					}
				} else {
					printModule(mod, false);
				}
			} else if (next instanceof CorruptData) {
				CorruptData corruptObj = (CorruptData) next;
				// warn the user that this image library is corrupt
				out.print("\t  <corrupt library encountered: "
						+ corruptObj.toString() + ">\n\n");
			} else {
				// unexpected type in iterator
				out.print("\t  <corrupt library encountered>\n\n");
			}
		}
	}

	private void printModule(ImageModule imageModule, boolean printSymbols) {
		try {
			out.print("\t  " + imageModule.getName());
		} catch (CorruptDataException cde) {
			out.print("\t  " + Exceptions.getCorruptDataExceptionString());
		}
		
		try {
			String addressInHex = String.format("0x%x",imageModule.getLoadAddress());
			out.print(" @ " + addressInHex);
		} catch (DataUnavailable e) {
			// if we do not have the load address, simply omit it
		} catch (CorruptDataException e) {
			// if we do not have the load address, simply omit it
		}

		Iterator itSection = imageModule.getSections();
		
		if (itSection.hasNext()) {
			out.print(", sections:\n");		
		} else {
			out.print(", <no section information>\n");
		}

		// iterate through the library sections
		while (itSection.hasNext()) {
			Object next = itSection.next();
			if (next instanceof ImageSection) {
				ImageSection is = (ImageSection) next;
				out.print("\t   0x"
						+ Long.toHexString(is.getBaseAddress().getAddress())
						+ " - 0x"
						+ Long.toHexString(is.getBaseAddress().getAddress()
								+ is.getSize()));
				out.print(", name: \"");
				out.print(is.getName());
				out.print("\", size: 0x");
				out.print(Long.toHexString(is.getSize()));
				out.print("\n");
			} else if (next instanceof CorruptData) {
				// warn the user that this image section is corrupt
				out.print("\t   <corrupt section encountered>\n");
			} else {
				// unexpected type in iterator
				out.print("\t   <corrupt section encountered>\n");
			}
		}
		if (printSymbols) {
			out.print("\t  " + "symbols:\n");
			Iterator itSymbols = imageModule.getSymbols();
			while (itSymbols.hasNext()) {
				Object next = itSymbols.next();
				if (next instanceof ImageSymbol) {
					ImageSymbol is = (ImageSymbol) next;
					out.print("\t   0x"
							+ Long.toHexString(is.getAddress().getAddress()));

					out.print(", name: \"");
					out.print(is.getName());
					out.print("\"\n");
				} else if (next instanceof CorruptData) {
					// warn the user that this image section is corrupt
					out.print("\t   <corrupt symbol encountered>\n");
				} else {
					// unexpected type in iterator
					out.print("\t   <corrupt symbol encountered>\n");
				}
			}
		}
		out.print("\n");
	}

	private boolean checkModuleName(final String name,
			String moduleName) {
		boolean wildCardStart = false;
		boolean wildCardEnd = false;
		
		moduleName = moduleName.trim();
		if (moduleName.startsWith("*")) {
			wildCardStart = true;
			moduleName = moduleName.substring(1);
			if (moduleName.length() == 0) {
				// The search was "*" so print everything.
				wildCardEnd = true;
			}
		}
		if (moduleName.endsWith("*")) {
			wildCardEnd = true;
			moduleName = moduleName.substring(0, moduleName.length() - 1);
		}
		if (!wildCardStart && !wildCardEnd && moduleName.equals(name)) {
			return true;
		} else if (wildCardStart && !wildCardEnd
				&& name.endsWith(moduleName)) {
			return true;
		} else if (!wildCardStart && wildCardEnd
				&& name.startsWith(moduleName)) {
			return true;
		} else if (wildCardStart && wildCardEnd
				&& name.indexOf(moduleName) >= 0) {
			return true;
		}
		return false;
	}
	
	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println(longDesc);
		
	}
}
