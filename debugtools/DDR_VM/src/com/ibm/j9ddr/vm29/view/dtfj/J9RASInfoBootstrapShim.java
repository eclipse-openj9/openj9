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
package com.ibm.j9ddr.vm29.view.dtfj;

import java.io.UnsupportedEncodingException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.Properties;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.IBootstrapRunnable;
import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.InvalidDataTypeException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageProcess;
import com.ibm.j9ddr.view.dtfj.image.J9RASImageDataFactory.MachineData;
import com.ibm.j9ddr.view.dtfj.image.J9RASImageDataFactory.ProcessData;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RASPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RASSystemInfoPointer;
import com.ibm.j9ddr.vm29.structure.RasdumpInternalConstants;

/**
 * Bootstrap shim for extracting information from the J9RAS structure
 * 
 * @see J9DDRImageProcess
 * @author andhall
 *
 */
public class J9RASInfoBootstrapShim implements IBootstrapRunnable
{

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.IBootstrapRunnable#run(com.ibm.j9ddr.IVMData, java.lang.Object[])
	 */
	public void run(IVMData vmData, Object[] userData)
	{
		Object[] passbackArray = (Object[]) userData[0];
		
		passbackArray[0] = new J9RASInfoObject();
	}

	private static class J9RASInfoObject implements MachineData, ProcessData
	{
		private final J9RASPointer ptr = DataType.getJ9RASPointer();
		
		public int version() throws CorruptDataException
		{
			// Return the J9RAS major version, which is the top half of the 32-bit version field in the J9RAS structure
			return ptr.version().intValue() >> 16;
		}
		
		public int cpus() throws CorruptDataException
		{
			try {
				return ptr.cpus().intValue();
			} catch (InvalidDataTypeException e) {
				//it is possible that due to corruption or there is an installed amount of memory > Long.MAX_VALUE it cannot be represented in Java
				throw new CorruptDataException("This value cannot be expressed : " + ptr.cpus().getHexValue(), e);
			}
		}

		public long memoryBytes() throws CorruptDataException
		{
			try {
				return ptr.memory().longValue();
			} catch (InvalidDataTypeException e) {
				//it is possible that due to corruption or there is an installed amount of memory > Long.MAX_VALUE it cannot be represented in Java
				throw new CorruptDataException("This value cannot be expressed : " + ptr.memory().getHexValue(), e);
			}
		}

		public String osArch() throws CorruptDataException
		{
			return ptr.osarchEA().getCStringAtOffset(0);
		}

		public String osName() throws CorruptDataException
		{
			return ptr.osnameEA().getCStringAtOffset(0);
		}

		public String osVersion() throws CorruptDataException
		{
			return ptr.osversionEA().getCStringAtOffset(0);
		}

		public long pid() throws CorruptDataException
		{
			try {
				return ptr.pid().longValue();
			} catch (InvalidDataTypeException e) {
				//it is possible that due to corruption or there is an installed amount of memory > Long.MAX_VALUE it cannot be represented in Java
				throw new CorruptDataException("This value cannot be expressed : " + ptr.pid().getHexValue(), e);
			}
		}

		public long tid() throws CorruptDataException
		{
			try {
				return ptr.tid().longValue();
			} catch (InvalidDataTypeException e) {
				//it is possible that due to corruption or there is an installed amount of memory > Long.MAX_VALUE it cannot be represented in Java
				throw new CorruptDataException("This value cannot be expressed : " + ptr.tid().getHexValue(), e);
			}
		}

		/* (non-Javadoc)
		 * @see com.ibm.j9ddr.view.dtfj.image.J9RASImageDataFactory.ProcessData#gpInfo()
		 */
		public String gpInfo() throws CorruptDataException
		{
			if (ptr.crashInfo().notNull()) {				//there is a pointer to a crash info structure
				if(ptr.crashInfo().gpInfo().notNull()) {	//can read the string
					return ptr.crashInfo().gpInfo().getCStringAtOffset(0);
				} else {									//return null as we cannot process the string
					return null;
				}
			} else {										//there is no crash info
				return "";									//return an empty string
			}
		}
		
		public String hostName() throws DataUnavailableException, CorruptDataException {
			if(ptr.hostnameEA().notNull()) {
				return ptr.hostnameEA().getCStringAtOffset(0);
			} else {
				throw new DataUnavailableException("Host name not available");
			}
		}
		
		public Iterator<Object> ipaddresses() throws DataUnavailableException, CorruptDataException {
			ArrayList<Object> addresses = new ArrayList<Object>();
			U8Pointer ip = ptr.ipAddressesEA();
			if(ip.isNull()) {
				throw new DataUnavailableException("IP addresses not available");
			}
			int type = ip.at(0).intValue();
			byte[] ipv4 = new byte[4];
			byte[] ipv6 = new byte[16];
			InetAddress address = null;
ADDRESS_PROCESSING : while(0 != type) {
				try {
					switch(type) {
						case 4 :	//IPv4
							DataType.getProcess().getBytesAt(ip.getAddress() + 1, ipv4);
							ip = ip.add(5);		//increment before getting address in case an UnknownHostException is thrown
							address = InetAddress.getByAddress(ipv4);
							break;
						case 6 :	//IPv6
							DataType.getProcess().getBytesAt(ip.getAddress() + 1, ipv6);
							ip = ip.add(7);		//increment before getting address in case an UnknownHostException is thrown
							address = InetAddress.getByAddress(ipv6);
							break;
						default :	//unknown type, so add a corrupt data
							addresses.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Unknown IP address type identifier : " + type));
							break ADDRESS_PROCESSING;		//and exit loop as do not know how big the address is to skip to the next one
					}
					if(!addresses.contains(address)) {
						addresses.add(address);
					}
				} catch (UnknownHostException e) {
					addresses.add(J9DDRDTFJUtils.newCorruptData(DTFJContext.getProcess(), "Corrupt IP address : " + e.getMessage()));
				}
				type = ip.at(0).intValue();
			}
			if(0 == addresses.size()) {
				throw new DataUnavailableException("IP addresses not available");
			}
			return addresses.iterator();
		}

		public IProcess getProcess()
		{
			return DataType.getProcess();
		}

		public long getEnvironment() throws CorruptDataException
		{
			return ptr.environment().longValue();
		}
		
		/**
		 * Return OS properties. OS properties obtained by the JVM are stored as a linked list of system information
		 * key/value pairs, off the J9RAS structure
		 * 
		 * @return Properties (may be empty)
		 */
		public Properties systemInfo() throws DataUnavailableException, CorruptDataException {
			Properties properties = new Properties();
			
			J9RASSystemInfoPointer systemInfo = ptr.systemInfo();
			if (systemInfo.notNull()) {
				J9RASSystemInfoPointer firstSystemInfo = systemInfo;
				do {
					// Walk the linked list, converting the key/values pairs into property strings
					int key = systemInfo.key().intValue();
					if (key == (int)RasdumpInternalConstants.J9RAS_SYSTEMINFO_SCHED_COMPAT_YIELD) {
						try {
							// Since sched_compat_yield is just one character we actually write that
							// into the data field instead of a pointer to a string.
							byte[] data = new byte[1];
							systemInfo.dataEA().getBytesAtOffset(0, data);
							properties.put("/proc/sys/kernel/sched_compat_yield", new String(data, "UTF-8") );
						} catch (UnsupportedEncodingException e) {
						}
					} else if (key == (int)RasdumpInternalConstants.J9RAS_SYSTEMINFO_HYPERVISOR) {
						properties.put("Hypervisor", U8Pointer.cast(systemInfo.data()).getCStringAtOffset(0));
					} else if (key == (int)RasdumpInternalConstants.J9RAS_SYSTEMINFO_CORE_PATTERN) {
						properties.put("/proc/sys/kernel/core_pattern", U8Pointer.cast(systemInfo.data()).getCStringAtOffset(0));
					} else if (key == (int)RasdumpInternalConstants.J9RAS_SYSTEMINFO_CORE_USES_PID) {
						properties.put("/proc/sys/kernel/core_uses_pid", U8Pointer.cast(systemInfo.data()).getCStringAtOffset(0));
					} else {
						// ignore any unknown/unsupported values
					}
					systemInfo = systemInfo.linkNext();
				} while(!systemInfo.equals(firstSystemInfo));
			}
			return properties;
		}
		
		/**
		 * Return the dump creation time. If this was not set by the dump agent when the dump was
		 * triggered (for example if the dump was triggered externally using Linux gcore or Windows
		 * task manager) we throw a DataUnavailableException.
		 * 
		 * @return long - dump creation time (milliseconds since 1970)
		 */
		public long dumpTimeMillis() throws DataUnavailableException, CorruptDataException
		{
			long timestamp = ptr.dumpTimeMillis().longValue();
			if (timestamp == 0) {
				throw new DataUnavailableException("Dump creation time (millisecs) not set");
			} else {
				return timestamp;
			}
		}
		
		/**
		 * Return the value of the system nanotime (high resolution timer) at dump creation time. If this
		 * was not set by the dump agent when the dump was triggered (for example if the dump was triggered
		 * externally using Linux gcore or Windows task manager) we throw a DataUnavailableException.
		 * 
		 * @return long - system nanotime
		 */
		public long dumpTimeNanos() throws DataUnavailableException, CorruptDataException
		{
			long timestamp = ptr.dumpTimeNanos().longValue();
			if (timestamp == 0) {
				throw new DataUnavailableException("Dump creation time (nanosecs) not set");
			} else {
				return timestamp;
			}
		}
	}
}
