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
package com.ibm.j9ddr.corereaders.memory;

import java.util.Collections;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.DataUnavailableException;

/**
 * IModule placeholder for modules we don't have the file for
 * (and therefore can't get the symbols from).
 * 
 * @author andhall
 *
 */
public class MissingFileModule extends BaseModule
{
	private Properties props = new Properties();				//empty properties 

	public MissingFileModule(IProcess process, String name,
			List<? extends IMemoryRange> memoryRanges)
	{
		super(process, name, memoryRanges, 0);
	}
	
	/*
	 * Constructor for when we have a module in memory and so have
	 * a load address, even though we do not have the original file.
	 * This occurs when we only have the module within the core file. 
	 */
	public MissingFileModule(IProcess process, String name,
			List<? extends IMemoryRange> memoryRanges, long loadAddress)
	{
		super(process, name, memoryRanges, loadAddress);
	}	
	

	@Override
	public String getName() {
		return super.getName();
	}



	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.memory.IModule#getSymbols()
	 */
	public List<? extends ISymbol> getSymbols() throws DataUnavailableException
	{
		return Collections.emptyList();
	}

	public Properties getProperties() throws DataUnavailableException
	{
		return props;
	}

}
