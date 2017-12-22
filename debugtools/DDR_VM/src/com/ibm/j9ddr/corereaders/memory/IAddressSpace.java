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

import com.ibm.j9ddr.corereaders.ICore;

/**
 * A region of addressable memory that has an ID and a list of processes.
 * 
 * The multiple processes per address space is required to model enclaves on
 * zOS. Most other platforms have one process per A/S.
 * 
 * @author andhall
 * 
 */
public interface IAddressSpace extends IMemory
{
	/**
	 * 
	 * @return Numeric ID of address space.
	 */
	public int getAddressSpaceId();

	/**
	 * 
	 * @return List of processes using this address space.
	 */
	public Collection<? extends IProcess> getProcesses();
	
	/**
	 * The core file from which this address space has been created from
	 * @return the core file or null if this address space is not backed by an underlying core file
	 */
	public ICore getCore();
}
