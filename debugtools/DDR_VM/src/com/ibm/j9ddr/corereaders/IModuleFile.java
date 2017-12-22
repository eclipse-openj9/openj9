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
package com.ibm.j9ddr.corereaders;

import java.io.IOException;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.ISymbol;

/**
 * A binary file (shared library / executable) on disk.
 * 
 * @author andhall
 *
 */
public interface IModuleFile
{

	/**
	 * @param relocationBase Base address for symbols
	 * @return Symbols from the file.
	 * @throws IOException 
	 */
	public List<? extends ISymbol> getSymbols(long relocationBase) throws IOException;
	
	/**
	 * Creates a memory source for the .text segment.
	 * @param virtualAddress Address text segment is mapped into address space
	 * @param virtualSize Size of mapped section.
	 * @return IMemorySource backed by the on-disk file.
	 */
	public IMemorySource getTextSegment(long virtualAddress, long virtualSize);
	
	public Properties getProperties();
	
}
