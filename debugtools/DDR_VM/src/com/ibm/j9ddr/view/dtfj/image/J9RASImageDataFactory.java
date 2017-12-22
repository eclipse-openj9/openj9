/*******************************************************************************
 * Copyright (c) 2009, 2016 IBM Corp. and others
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

import java.io.IOException;
import java.util.Collection;
import java.util.Iterator;
import java.util.Properties;

import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.VMDataFactory;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IProcess;

/**
 * Point of contact for Image code needing to extract
 * data from the J9RAS structure
 * @author andhall
 *
 */
public class J9RASImageDataFactory
{

	/**
	 * 
	 * @return ProcessData or NULL if we can't find a J9DDR structure in p
	 */
	public static ProcessData getProcessData(IProcess p)
	{
		return (ProcessData)getRasData(p);
	}
	
	/**
	 * 
	 * @return MachineData or NULL if we can't find a J9DDR structure in c
	 */
	public static MachineData getMachineData(ICore c)
	{
		Collection<? extends IAddressSpace> addressSpaces = c.getAddressSpaces();
		
		for (IAddressSpace as : addressSpaces) {
			MachineData data = getMachineData(as);
			
			if (data != null) {
				return data;
			}
		}
		
		return null;
	}
	
	/**
	 * 
	 * @return MachineData or NULL if we can't find a J9DDR structure in a
	 */
	public static MachineData getMachineData(IAddressSpace a)
	{
		Collection<? extends IProcess> processes = a.getProcesses();
		
		for (IProcess p : processes) {
			MachineData data = getMachineData(p);
			
			if (data != null) {
				return data;
			}
		}
		
		return null;
	}
	
	/**
	 * 
	 * @return MachineData or NULL if we can't find a J9DDR structure in p
	 */
	public static MachineData getMachineData(IProcess p)
	{
		return (MachineData)getRasData(p);
	}
	
	private static Object getRasData(IProcess p)
	{
		try {
			IVMData vmData = VMDataFactory.getVMData(p);
			
			Object[] passBackArray = new Object[1];
			
			try {
				vmData.bootstrapRelative("view.dtfj.J9RASInfoBootstrapShim", (Object)passBackArray);
			} catch (ClassNotFoundException e) {
				// Need to return null here for IDDE as the class will not be found for Node.js cores
				return null;
			}
			
			return passBackArray[0];
		} catch (IOException e) {
			//Ignore
		} catch (UnsupportedOperationException e) {
			// VMDataFactory may throw unsupported UnsupportedOperationException
			// exceptions for JVM's DDR does not support.
		} catch (Exception e) {
			// Ignore
		}
		
		return null;
	}

	public static interface MachineData
	{
		public long memoryBytes() throws CorruptDataException;
		
		public String osVersion() throws CorruptDataException;
		
		public String osArch() throws CorruptDataException;
		
		public String osName() throws CorruptDataException;
		
		public String hostName() throws DataUnavailableException, CorruptDataException;
		
		public Iterator<Object> ipaddresses() throws DataUnavailableException, CorruptDataException;
		
		public int cpus() throws CorruptDataException;
		
		public IProcess getProcess();
		
		public Properties systemInfo() throws DataUnavailableException, CorruptDataException;
		
		public long dumpTimeMillis() throws DataUnavailableException, CorruptDataException;
		
		public long dumpTimeNanos() throws DataUnavailableException, CorruptDataException;
	}
	
	public static interface ProcessData
	{
		public int version() throws CorruptDataException;
		
		public long pid() throws CorruptDataException, DataUnavailable;
		
		public long tid() throws CorruptDataException, DataUnavailable;
		
		/**
		 * Returns the information associated with a crash, such as the signal number
		 * @return if the event of a crash, the string contains the data for this field. If there was not a crash
		 * e.g. the dump was requested by the user, then an empty string is returned. If there was a crash, but there
		 * is no information available then a null is returned.
		 * @throws CorruptDataException
		 */
		public String gpInfo() throws CorruptDataException;
		
		public IProcess getProcess();
		
		public long getEnvironment() throws CorruptDataException;
	}
}
