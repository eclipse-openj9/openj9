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

package com.ibm.j9ddr.libraries;

import static java.util.logging.Level.SEVERE;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.corereaders.Builder;
import com.ibm.dtfj.corereaders.ClosingFileReader;
import com.ibm.dtfj.corereaders.DumpFactory;
import com.ibm.dtfj.corereaders.NewElfDump;
import com.ibm.dtfj.corereaders.NewAixDump;
import com.ibm.dtfj.corereaders.ICoreFileReader;

@SuppressWarnings("restriction")
public class DTFJLibraryAdapter implements Builder, LibraryAdapter {
	private static final Logger logger = Logger.getLogger(LibraryCollector.LOGGER_NAME);
	private File _parentDir;
	private ArrayList<String> moduleNames = null;
	private final ArrayList<String> errorMessages = new ArrayList<String>();
	private boolean isAIX = false;			//flag to indicate if running on AIX
		
	public static void main(String[] args)
	{
		if (args.length > 0) {
			for (int x = 0; x < args.length; x++) {
				File coreFile = new File(args[x]);
				System.out.println("Reading \"" + coreFile.getAbsolutePath() + "\"...");
				DTFJLibraryAdapter adapter = new DTFJLibraryAdapter();
				ArrayList<String> modules = adapter.getLibraryList(coreFile);
				for(String module : modules) {
					System.out.println("\t" + module);
				}
			}
		} else {
			System.out.println("Usage:  \"LibraryAdapter <core files> ...\"");
			System.exit(1);
		}
	}
	
	public boolean isLibraryCollectionRequired(File coreFile) {
		ICoreFileReader reader = null;
		try {
			ClosingFileReader closingFile = new ClosingFileReader(coreFile);
			reader = DumpFactory.createDumpForCore(closingFile);
		} catch (Exception e) {
			logger.log(SEVERE, "Could not determine if library collection is required for " + coreFile.getAbsolutePath(), e);
			errorMessages.add(e.getMessage());
			return false;			//if this fails, then so would any collection attempt as well
		}
		if (reader instanceof NewElfDump) {
			return true;
		}
		if (reader instanceof NewAixDump) {
			return true;
		}
		return false;
	}



	public ArrayList<String> getLibraryList(final File coreFile)
	{
		if(moduleNames == null) {
			moduleNames = new ArrayList<String>();
			try {
				_parentDir = coreFile.getParentFile();
				logger.fine("Creating DTFJ core file reader");
				ClosingFileReader closingFile = new ClosingFileReader(coreFile);
				ICoreFileReader reader = DumpFactory.createDumpForCore(closingFile);
				isAIX = (reader instanceof NewAixDump);
				logger.fine("Extracting modules");
				reader.extract(this);
			} catch (FileNotFoundException e) {
				logger.log(SEVERE, "Could not open core file " + coreFile.getAbsolutePath(), e);
				errorMessages.add(e.getMessage());
			} catch (IOException e) {
				logger.log(SEVERE, "Error processing core file " + coreFile.getAbsolutePath(), e);
				errorMessages.add(e.getMessage());
			}
		}
		return moduleNames;
	}
	
	public ArrayList<String> getErrorMessages() {
		return errorMessages;
	}
	
	public ClosingFileReader openFile(String filename) throws IOException
	{
		//given that we may be looking for a file which came from another system, look it up in our support file dir first (we need that to over-ride the absolute path since it may be looking for a file in the same location as one on the local machine - this happens often if moving a core file from one Unix system to another)
		ClosingFileReader candidate = null;
		File absolute = new File(filename);
		String fileName = absolute.getName();
		File supportFileCopy = new File(_parentDir, fileName);
		if (supportFileCopy.exists()) {
			candidate = new ClosingFileReader(supportFileCopy);
		} else {
			candidate = new ClosingFileReader(absolute);
		}
		return candidate;
	}
	
	@SuppressWarnings("unchecked")
	public Object buildModule(String name, Properties properties, Iterator sections, Iterator symbols, long loadAddress)
	{
		logger.fine("Module : " + name);
		if(isAIX) {
			int pos = name.indexOf(".a("); 		//check on AIX if module is the .a file or library
			if((pos != -1) && (name.lastIndexOf(')') == name.length() - 1)) {
				moduleNames.add(name.substring(0, pos + 2));
			} else {
				moduleNames.add(name);
			}
		} else {
			moduleNames.add(name);
		}
		return null;
	}
	
	@SuppressWarnings("unchecked")
	public Object buildProcess(Object addressSpace, String pid, String commandLine, Properties environment, Object currentThread, Iterator threads, Object executable, Iterator libraries, int addressSize)
	{
		if(logger.isLoggable(Level.FINEST)) {
			String msg = String.format("Building process %s : %s", pid, commandLine);
			logger.finest(msg);
		}
		return null;			//no-op
	}

	public Object buildAddressSpace(String name, int id)
	{
		if(logger.isLoggable(Level.FINEST)) {
			String msg = String.format("Building address space %s[%d]", name, id);
			logger.finest(msg);
		}
		return null;			//no-op
	}

	public Object buildRegister(String name, Number value)
	{
		return null;			//no-op
	}

	public Object buildStackSection(Object addressSpace, long stackStart, long stackEnd)
	{
		return null;			//no-op
	}

	@SuppressWarnings("unchecked")
	public Object buildThread(String name, Iterator registers, Iterator stackSections, Iterator stackFrames, Properties properties, int signalNumber)
	{
		if(logger.isLoggable(Level.FINEST)) {
			String msg = String.format("Building thread %s [signal %d]", name, signalNumber);
			logger.finest(msg);
		}
		return null;			//no-op
	}

	public Object buildModuleSection(Object addressSpace, String name, long imageStart, long imageEnd)
	{
		if(logger.isLoggable(Level.FINEST)) {
			String msg = String.format("Building module %s [0x%s - 0x%s]", name, Long.toHexString(imageStart), Long.toHexString(imageEnd));
			logger.finest(msg);
		}
		return null;			//no-op
	}

	public Object buildStackFrame(Object addressSpace, long stackBasePointer, long pc)
	{
		return null;			//no-op
	}

	public Object buildSymbol(Object addressSpace, String functionName, long relocatedFunctionAddress)
	{
		return null;			//no-op
	}

	public Object buildCorruptData(Object addressSpace, String message, long address)
	{
		return null;			//no-op
	}

	public long getEnvironmentAddress()
	{
		return 0;			//no-op
	}

	@SuppressWarnings("unchecked")
	public long getValueOfNamedRegister(List registers, String string)
	{
		return 0;			//no-op
	}

	public void setExecutableUnavailable(String description)
	{
		errorMessages.add("Library collection not possible as the executable cannot be found [" + description + "]");
		if(logger.isLoggable(Level.FINEST)) {
			String msg = String.format("Executable not available : %s", description);
			logger.warning(msg);
		}
	}

	public void setOSType(String osType)
	{
		if(logger.isLoggable(Level.FINEST)) {
			String msg = String.format("OS Type : %s", osType);
			logger.finest(msg);
		}
	}

	public void setCPUType(String cpuType)
	{
		//no-op
	}

	public void setCPUSubType(String subType)
	{
		//no-op
	}

	public void setCreationTime(long millis)
	{
		//no-op
	}

	@SuppressWarnings("unchecked")
	public Object buildModule(String name, Properties properties, Iterator sections, Iterator symbols)
	{
		if(logger.isLoggable(Level.FINEST)) {
			String msg = String.format("Building module %s", name);
			logger.finest(msg);
		}
		return null;
	}

}
