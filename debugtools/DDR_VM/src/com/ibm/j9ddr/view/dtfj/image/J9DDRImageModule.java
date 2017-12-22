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
package com.ibm.j9ddr.view.dtfj.image;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageSymbol;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.view.dtfj.DTFJCorruptDataException;

/**
 * @author andhall
 *
 */
public class J9DDRImageModule implements ImageModule
{
	private final IProcess process;
	private final IModule delegate;
	private final String moduleNameOverride;
	
	public J9DDRImageModule(IProcess process, IModule module, String nameOverride)
	{
		if (module == null) {
			throw new NullPointerException("Module cannot be null");
		}
		
		if (process == null) {
			throw new NullPointerException("Process cannot be null");
		}
		this.process = process;
		this.delegate = module;
		moduleNameOverride = nameOverride;
	}

	public J9DDRImageModule(IProcess process, IModule module)
	{
		this(process,module, null);
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageModule#getName()
	 */
	public String getName() throws CorruptDataException
	{
		if (moduleNameOverride != null) {
			return moduleNameOverride;
		}
		
		try {
			return delegate.getName();
		} catch (com.ibm.j9ddr.CorruptDataException e) {
			throw new DTFJCorruptDataException(process, e);
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageModule#getProperties()
	 */
	public Properties getProperties() throws CorruptDataException
	{
		try {
			return delegate.getProperties();
		} catch (DataUnavailableException e) {
			throw new DTFJCorruptDataException(new J9DDRCorruptData(process,e.getMessage()), e);
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageModule#getSections()
	 */
	@SuppressWarnings("unchecked")
	public Iterator getSections()
	{
		Collection<? extends IMemoryRange> ranges = delegate.getMemoryRanges();
		
		List<ImageSection> sections = new ArrayList<ImageSection>(ranges.size());
		
		for (IMemoryRange range : ranges) {
			sections.add(new J9DDRImageSection(process, range.getBaseAddress(), range.getSize(), range.getName()));
		}
		
		return sections.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.image.ImageModule#getSymbols()
	 */
	public Iterator<?> getSymbols()
	{
		Collection<? extends ISymbol> symbols;
		try {
			symbols = delegate.getSymbols();
		} catch (DataUnavailableException e) {
			//JEXTRACT slightly arbitrary code here. DTFJ/JExtract uses the base address of the .text section as the 
			//address of the corrupt data. We do the same for compatibility's sake.
			Collection<? extends IMemoryRange> memoryRanges = delegate.getMemoryRanges();
			
			long baseAddress = 0;
			
			for (IMemoryRange range : memoryRanges) {
				if (range.getName().contains(".text")) {
					baseAddress = range.getBaseAddress();
					break;
				}
			}
			
			return Collections.singletonList(new J9DDRCorruptData(process,e.getMessage(),baseAddress)).iterator();
		}
		
		List<ImageSymbol> dtfjSymbols = new ArrayList<ImageSymbol>(symbols.size());
		
		for (ISymbol symbol : symbols) {
			dtfjSymbols.add(new J9DDRImageSymbol(symbol.getName(), new J9DDRImagePointer(process, symbol.getAddress())));
		}
		
		return dtfjSymbols.iterator();
	}

	@Override
	public boolean equals(Object obj)
	{
		if (obj instanceof J9DDRImageModule) {
			J9DDRImageModule other = (J9DDRImageModule)obj;
			
			if (! this.process.equals(other.process)) {
				return false;
			}
			
			return this.delegate.equals(other.delegate);
		} else {
			return false;
		}
	}

	@Override
	public int hashCode()
	{
		return delegate.hashCode();
	}

	@Override
	public String toString()
	{
		try {
			return "ImageModule: " + getName();
		} catch (CorruptDataException e) {
			return "ImageModule: <couldn't get name>";
		}
	}
	
	public long getLoadAddress() throws DataUnavailable {
		return delegate.getLoadAddress();
	}

	
}
