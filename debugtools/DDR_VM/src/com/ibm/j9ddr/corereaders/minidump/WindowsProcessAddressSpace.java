/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.minidump;

import java.nio.ByteOrder;
import java.util.LinkedList;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.EnvironmentUtils;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.ProcessAddressSpace;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;

/**
 * @author andhall
 *
 */
public class WindowsProcessAddressSpace extends ProcessAddressSpace
{
	private final MiniDumpReader reader;
	private Properties environment;

	WindowsProcessAddressSpace(int pointerSizeBytes,
			ByteOrder byteOrder, MiniDumpReader reader)
	{
		super(pointerSizeBytes, byteOrder, reader);
		
		this.reader = reader;
	}

	public ICore getCore() {
		return reader;
	}

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.memory.IProcess#getCommandLine()
	 */
	public String getCommandLine() throws CorruptDataException, DataUnavailableException
	{
		return reader.getCommandLine();
	}

	@Override
	public boolean equals(Object o) {
		if((o == null) || !(o instanceof WindowsProcessAddressSpace)) {
			return false;
		}
		WindowsProcessAddressSpace space = (WindowsProcessAddressSpace) o;
		return reader.equals(space.reader);
	}

	@Override
	public int hashCode() {
		return reader.hashCode();
	}

	/**
	 * This method tries to get environment variables by iterating through modules. 
	 * It returns the one with environment var "IBM_JAVA_COMMAND_LINE"
	 * If there is no module with IBM_JAVA_COMMAND_LINE, then it returns the last one.  
	 * 
	 * @return Properties instance of environment variables.
	 * @throws DataUnavailableException
	 * @throws CorruptDataException
	 */
	public Properties getEnvironmentVariables() throws DataUnavailableException, CorruptDataException
	{
		if (null == environment) {
			LinkedList<ISymbol> environSymbols = getEnvironmentSymbols();
			ISymbol environ = null;
			
			if (0 == environSymbols.size()) {
				throw new DataUnavailableException("Couldn't find environment symbol");
			}
			
			/* There might be more than one module with environment variables. Use the one that has IBM_JAVA_COMMAND_LINE */
			for (int i = 0; i < environSymbols.size(); i++ ) {
				environ = environSymbols.get(i);
				long environPointer = getPointerAt(environ.getAddress());
				environment = EnvironmentUtils.readEnvironment(this,environPointer);
				if (environment.containsKey("IBM_JAVA_COMMAND_LINE")) {
					break;
				}
			}
		}
	
		return environment;
	}

	
	/**
	 *  This method returns a list of symbols with the name "_environ"
	 *  
	 *  @return LinkedList instance of symbols with the name "_environ"
	 *  @throws CorruptDataException
	 *  
	 */
	private LinkedList<ISymbol> getEnvironmentSymbols() throws CorruptDataException
	{
		List<IModule> modules = getModules();
		LinkedList<ISymbol> symbols = new LinkedList<ISymbol>();
		
		for (IModule thisModule : modules) {
			try {
				for (ISymbol thisSymbol : thisModule.getSymbols()) {
					if (thisSymbol.getName().equals("_environ")) {
						symbols.add(thisSymbol);
					}
				}
			} catch (DataUnavailableException e) {
				continue;
			}
		}
		
		return symbols;
	}
	

	public IModule getExecutable() throws CorruptDataException
	{
		return reader.getExecutable();
	}

	public List<IModule> getModules() throws CorruptDataException
	{
		return reader.getModules();
	}

	public long getProcessId() throws CorruptDataException
	{
		return reader.getPid();
	}

	public List<IOSThread> getThreads() throws CorruptDataException
	{
		return reader.getThreads();
	}

	public Platform getPlatform()
	{
		return Platform.WINDOWS;
	}

	public int getSignalNumber() throws DataUnavailableException
	{
		throw new DataUnavailableException("Signal number not available on Windows");
	}

	public boolean isFailingProcess() throws DataUnavailableException {
		throw new DataUnavailableException("Not available on this platform");
	}
}
