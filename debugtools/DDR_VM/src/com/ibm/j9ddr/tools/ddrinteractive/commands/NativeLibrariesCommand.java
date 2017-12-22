/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Collection;

import com.ibm.dtfj.image.ImageModule;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.ILibraryDependentCore;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.libraries.CoreFileResolver;
import com.ibm.j9ddr.libraries.FooterLibraryEntry;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.annotations.DebugExtension;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageProcess;

/**
 * Debug extension to extract native libraries appended to the end of
 * a core file by diagnostics collector.
 * 
 * @author Adam Pilkington
 *
 */
@DebugExtension(VMVersion="*",contentAssist="dclibs")
public class NativeLibrariesCommand extends Command {
	protected String hexformat = null;
		
	{
		addCommand("dclibs","", "List the native libraries collected by DC");
		addCommand("dclibs", "extract", "Extract collected libraries into the same directory as the core file");
		addCommand("dclibs", "extract overwrite", "as !libs extract, but overwrite any existing files");
		addCommand("dclibs", "extract <full path to dir>", "Extract the collected libraries into the specified directory");
	}

	public void run(String cmd, String[] args, Context ctx, PrintStream out) throws DDRInteractiveCommandException {
		hexformat = "0x%0" + (ctx.process.bytesPerPointer() * 2) + "x";
		if((DDRInteractive.getPath() == null) || (DDRInteractive.getPath().length() == 0)) {
			out.println("DDR Interactive was not invoked with a command line pointing to a core file. Aborting command");
			return;
		}
		if(args.length == 0) {
			showLibList(ctx, out);
			return;
		}
		if(args.length >= 1) {
			boolean overwrite = false;
			if(args[0].toLowerCase().equals("extract")) {
				File extractTo = null;
				if(args.length == 2) {		//supplemental argument supplied
					if(args[1].equalsIgnoreCase("overwrite")) {
						overwrite = true;
					} else {
						//an extraction directory has been supplied
						extractTo = new File(args[1]);
						if(!extractTo.exists()) {
							out.println("The specified extraction directory does not exist. " + extractTo.getAbsolutePath());
							return;
						}
					}
				}
				extractLibs(ctx, out, extractTo, overwrite);
				return;
			}
		}
		out.println("Command not recognised");
	}
	
	private void extractLibs(Context ctx, PrintStream out, File path, boolean overwrite) {
		if(path == null) {
			//no path was specified so derive it from the startup parameters
			boolean useCurrentDir = !DDRInteractive.getPath().contains(File.separator);
			if(useCurrentDir) {
				String curdir = System.getProperty("user.dir");
				path = new File(curdir);
			} else {
				//need to strip off the end file name
				path = new File(DDRInteractive.getPath());
				if(path.isFile()) {
					path = path.getParentFile();
				}
			}
		}
		out.println("Extracting libraries to : " + path.getAbsolutePath());
		LibReader reader = new LibReader();
		try {
			for(FooterLibraryEntry entry : reader.getEntries()) {
				if(entry == null) {
					continue;			//skip over null entries in the list
				}
				String dir = entry.getPath();
				if(dir.charAt(0) == '/') {
					//absolute path so convert to a relative one
					dir = dir.substring(1);
				}
				if((File.separatorChar == '\\') && dir.contains("/")) {
					//convert from a linux path to a windows one
					dir.replace('/', '\\');
				}
				File library = new File(path, dir).getCanonicalFile();
				//need to create the extraction directory if required
				if(library.isFile()) {
					if(library.getParentFile() != null) {
						library.getParentFile().mkdirs();
					}
					if(library.exists()) {
						if(overwrite) {
							out.println("Deleting existing library");
							library.delete();
						} else {
							out.println("Library " + library.getPath() + " already exists on disk so skipping (to overwrite run !dclibs extract overwrite)");
							continue;
						}
					}
				} else {
					if(!library.getParentFile().equals(path)) {
						library.getParentFile().mkdirs();
					}
				}
				out.println("Extracting " + library);
				reader.extractLibrary(entry.getPath(), library);
			}
		} catch(IOException e) {
			out.println("Error extracting libraries : " + e.getMessage());
		}
	}
	
	//we can use the hint mechanism in DTFJ to work out the exe location for elf cores with very long path names
	private void getExeFromDDR(Context ctx, PrintStream out) {
		try {
			ICore core = ctx.process.getAddressSpace().getCore(); 
			if(ILibraryDependentCore.class.isAssignableFrom(core.getClass())) {
				ILibraryDependentCore ldcore = (ILibraryDependentCore) core;
				J9DDRImageProcess proc = new J9DDRImageProcess(ctx.process);
				ImageModule exe = proc.getExecutable();
				out.println("exe = " + exe.getName());
				ldcore.executablePathHint(exe.getName());
			}
		} catch (Exception e) {
			out.println("Could not determine EXE name using DDR : " + e.getMessage());
		}
	}
	
	//list the libraries from the core file and report whether or not they have been collected
	private void showLibList(Context ctx, PrintStream out) {
		LibReader reader = new LibReader();
		out.println("Showing library list for " + DDRInteractive.getPath());
		Collection<? extends IModule> libs = null;
		try {
			libs = ctx.process.getModules();
		} catch (CorruptDataException e) {
			out.println("Corrupt data exception when retrieving list of libraries : " + e.getMessage());
			return;
		}
		getExeFromDDR(ctx, out);
		for(IModule lib : libs) {
			try {
				out.println("Lib : " + lib.getName());
				FooterLibraryEntry entry = reader.getEntry(lib.getName());
				if(entry == null) {
					out.println("\tLibrary is not appended to the core file, it may be present on the local disk");
				} else {
					out.println("\tLibrary has been collected");
					out.println("\tPath : " + entry.getPath());
					out.println("\tName : " + entry.getName());
					out.println("\tSize : " + entry.getSize());
				}
			} catch (CorruptDataException e) {
				out.println("Library name is corrupt");
			}
		}
	}
	
	private class LibReader extends CoreFileResolver {
		public LibReader() {
			super(DDRInteractive.getPath());
		}
		
		public FooterLibraryEntry getEntry(String path) {
			if(footer == null) {
				return null;
			}
			return footer.findEntry(path);
		}
		
		public FooterLibraryEntry[] getEntries() {
			return footer.getEntries();
		}
	}
	
}
