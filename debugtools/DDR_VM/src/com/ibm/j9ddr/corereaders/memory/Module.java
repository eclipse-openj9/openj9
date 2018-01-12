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

import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Properties;

import com.ibm.j9ddr.DataUnavailableException;

/**
 * @author andhall
 *
 */
public class Module extends BaseModule
{
	private final Collection<? extends ISymbol> symbols;
	private final Properties properties;
	
	public Module(IProcess process, String name, List<? extends ISymbol> symbols, Collection<? extends IMemoryRange> memoryRanges, long loadAddress, Properties properties)
	{
		super(process,name,memoryRanges, loadAddress);
		this.symbols = Collections.unmodifiableList(symbols);
		this.properties = properties;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.memory.IModule#getSymbols()
	 */
	public Collection<? extends ISymbol> getSymbols()
	{
		return symbols;
	}

	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = super.hashCode();
		result = prime * result + ((symbols == null) ? 0 : symbols.hashCode());
		return result;
	}

	@Override
	public boolean equals(Object obj)
	{
		if (this == obj) {
			return true;
		}
		if (!super.equals(obj)) {
			return false;
		}
		if (!(obj instanceof Module)) {
			return false;
		}
		Module other = (Module) obj;
		if (symbols == null) {
			if (other.symbols != null) {
				return false;
			}
		} else if (!symbols.equals(other.symbols)) {
			return false;
		}
		return true;
	}

	public Properties getProperties() throws DataUnavailableException
	{
		return properties;
	}

}
