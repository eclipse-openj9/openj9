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

/**
 * @author andhall
 *
 */
public class Symbol implements ISymbol
{
	private String name;
	private long address;
	
	public Symbol(String name, long address)
	{
		this.name = name;
		this.address = address;
	}

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.memory.ISymbol#getAddress()
	 */
	public long getAddress()
	{
		return address;
	}

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.corereaders.memory.ISymbol#getName()
	 */
	public String getName()
	{
		return name;
	}

	
	
	@Override
	public int hashCode()
	{
		final int prime = 31;
		int result = 1;
		result = prime * result + (int) (address ^ (address >>> 32));
		result = prime * result + ((name == null) ? 0 : name.hashCode());
		return result;
	}

	@Override
	public boolean equals(Object obj)
	{
		if (this == obj) {
			return true;
		}
		if (obj == null) {
			return false;
		}
		if (!(obj instanceof Symbol)) {
			return false;
		}
		Symbol other = (Symbol) obj;
		if (address != other.address) {
			return false;
		}
		if (name == null) {
			if (other.name != null) {
				return false;
			}
		} else if (!name.equals(other.name)) {
			return false;
		}
		return true;
	}

	@Override
	public String toString()
	{
		return "Symbol [address=" + address + ", name=" + name + "]";
	}

}
