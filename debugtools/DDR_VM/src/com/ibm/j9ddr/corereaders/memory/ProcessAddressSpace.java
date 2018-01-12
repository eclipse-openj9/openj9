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

import java.nio.ByteOrder;
import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.ICore;

/**
 * Object representing the single address space / process model used by Linux,
 * AIX and Windows.
 * 
 * The same object represents the process and the address space - which saves
 * creating objects that exist solely to delegate.
 * 
 * @author andhall
 * 
 */
public abstract class ProcessAddressSpace extends AbstractMemory implements IProcess,
		IAddressSpace
{

	private final int bytesPerPointer;
	private static final int addressSpaceID = 0;
	private final String id;

	public ProcessAddressSpace(int pointerSizeBytes, ByteOrder byteOrder, ICore core)
	{
		super(byteOrder);
		this.bytesPerPointer = pointerSizeBytes;
		id = core.getDumpFormat() + " : 0";
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IProcess#bytesPerPointer()
	 */
	public int bytesPerPointer()
	{
		return bytesPerPointer;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IProcess#getPointerAt(long)
	 */
	public long getPointerAt(long address) throws MemoryFault
	{
		if (bytesPerPointer() == 8) {
			return getLongAt(address);
		} else {
			return (0xFFFFFFFFL & getIntAt(address));
		}
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see
	 * com.ibm.dtfj.j9ddr.corereaders.memory.IAddressSpace#getAddressSpaceId()
	 */
	public int getAddressSpaceId()
	{
		return addressSpaceID;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IProcess#getAddressSpace()
	 */
	public IAddressSpace getAddressSpace()
	{
		return this;
	}

	/*
	 * (non-Javadoc)
	 * 
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IAddressSpace#getProcesses()
	 */
	public List<IProcess> getProcesses()
	{
		List<IProcess> toReturn = new LinkedList<IProcess>();
		toReturn.add(this);

		return toReturn;
	}

	@Override
	public String toString() {
		return id;
	}

	/*
	 * Equivalent to getProcedureNameForAddress(address, false).
	 * Default behaviour is to return DDR format strings for symbols.
	 */
	public final String getProcedureNameForAddress(long address) throws CorruptDataException
	{
		return getProcedureNameForAddress(address, false);
	}
	
	public String getProcedureNameForAddress(long address, boolean dtfjFormat) throws CorruptDataException
	{
		try {
			return SymbolUtil.getProcedureNameForAddress(this, address, dtfjFormat);
		} catch (DataUnavailableException e) {
			return "<unknown: " + e.getMessage() + ">";
		}
	}
	
}
