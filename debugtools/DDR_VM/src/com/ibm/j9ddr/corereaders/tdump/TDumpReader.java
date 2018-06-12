/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.tdump;

import java.io.File;
import java.io.IOException;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.DataUnavailableException;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.ICoreFileReader;
import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.Addresses;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.MemoryRange;
import com.ibm.j9ddr.corereaders.memory.Module;
import com.ibm.j9ddr.corereaders.memory.ProtectedMemoryRange;
import com.ibm.j9ddr.corereaders.memory.SearchableMemory;
import com.ibm.j9ddr.corereaders.memory.Symbol;
import com.ibm.j9ddr.corereaders.osthread.IOSStackFrame;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.corereaders.osthread.IRegister;
import com.ibm.j9ddr.corereaders.osthread.Register;
import com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.AddressRange;
import com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.AddressSpace;
import com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.Dump;
import com.ibm.j9ddr.corereaders.tdump.zebedee.le.Caa;
import com.ibm.j9ddr.corereaders.tdump.zebedee.le.CaaNotFound;
import com.ibm.j9ddr.corereaders.tdump.zebedee.le.Dll;
import com.ibm.j9ddr.corereaders.tdump.zebedee.le.DllFunction;
import com.ibm.j9ddr.corereaders.tdump.zebedee.le.DllVariable;
import com.ibm.j9ddr.corereaders.tdump.zebedee.le.DsaStackFrame;
import com.ibm.j9ddr.corereaders.tdump.zebedee.le.Edb;
import com.ibm.j9ddr.corereaders.tdump.zebedee.mvs.RegisterSet;
import com.ibm.j9ddr.corereaders.tdump.zebedee.mvs.Tcb;
import com.ibm.j9ddr.corereaders.tdump.zebedee.util.FileFormatException;
import com.ibm.jzos.ZFile;


/**
 * zOS dump reader implemented as a thin wrapper around Zebedee.
 * 
 * @author andhall
 * 
 */
public class TDumpReader implements ICoreFileReader
{
	private static final Logger logger = Logger.getLogger(J9DDR_CORE_READERS_LOGGER_NAME);
	
	private final static int DR1 = 0xC4D9F140; // 31-bit OS
	private final static int DR2 = 0xC4D9F240; // 64-bit OS

	public boolean validDump(byte[] data, long filesize)
	{

	int signature = (0xFF000000 & data[0] << 24)
		| (0x00FF0000 & data[1] << 16) | (0x0000FF00 & data[2] << 8)
		| (0xFF & data[3]);
	
		return (signature == DR1 || signature == DR2);
	}

	public ICore processDump(String path) throws InvalidDumpFormatException, IOException
	{
		return new DumpAdapter(new com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.Dump(path));
	}
	
	public ICore processDump(ImageInputStream in) throws InvalidDumpFormatException, IOException
	{
		return new DumpAdapter(new com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.Dump(in));
	}

	public DumpTestResult testDump(String path) throws IOException
	{
		if (! new File(path).exists()) {
			if (System.getProperty("os.name").toLowerCase().indexOf("z/os") == -1) {
				return DumpTestResult.FILE_NOT_FOUND;
			} else {
				if (! ZFile.exists("//'" + path + "'")) {
					return DumpTestResult.FILE_NOT_FOUND;
				}
			}
		}
		
		try {
			//Creating the Zebedee dump with initialize=false will open the file (either via USS or TSO), check the header, but won't do a heavyweight load
			new com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.Dump(path, false).close();
			return DumpTestResult.RECOGNISED_FORMAT;
		} catch (FileFormatException ex) {
			return DumpTestResult.UNRECOGNISED_FORMAT;
		} catch (IOException ex) {
			throw new IOException(ex.getMessage());
		}
	}
	
	public DumpTestResult testDump(ImageInputStream in) throws IOException
	{	
		try {
			in.seek(0);
			int header = in.readInt();
			if((header == DR1) || (header == DR2)) {
				return DumpTestResult.RECOGNISED_FORMAT;
			} else {
				return DumpTestResult.UNRECOGNISED_FORMAT;
			}
		} catch (FileFormatException ex) {
			return DumpTestResult.UNRECOGNISED_FORMAT;
		} catch (IOException ex) {
			IOException ioe = new IOException(ex.getMessage());
			ioe.initCause(ex);
			throw ioe;
		} finally {
			in.seek(0);		//reset the header
		}
	}

	private static class DumpAdapter implements ICore
	{
	
		private final Dump _dump;
	
		private List<IAddressSpace> addressSpaces = null;
		
		public DumpAdapter(Dump dump)
		{
			_dump = dump;
		}

		public void close() throws IOException {
			_dump.close();
		}
		
		/*
		 * (non-Javadoc)
		 * 
		 * @see com.ibm.dtfj.j9ddr.corereaders.ICoreReader#getAddressSpaces()
		 */
		public List<IAddressSpace> getAddressSpaces()
		{
			if (null == addressSpaces) {
			
				AddressSpace[] zebAddressSpaces = _dump.getAddressSpaces();
				
				addressSpaces = new ArrayList<IAddressSpace>(
						zebAddressSpaces.length);
				
				for (AddressSpace thisAs : zebAddressSpaces) {
					/* Ignore A/S 0 */
					if (thisAs.getAsid() != 0) {
						addressSpaces.add(new AddressSpaceAdapter(thisAs, this));
					}
				}
			}
	
			return addressSpaces;
		}
	
		/*
		 * (non-Javadoc)
		 * 
		 * @see com.ibm.dtfj.j9ddr.corereaders.ICoreReader#getDumpFormat()
		 */
		public String getDumpFormat()
		{
			return "tdump";
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
			if (!(obj instanceof DumpAdapter)) {
				return false;
			}
			DumpAdapter other = (DumpAdapter) obj;
			if (_dump == null) {
				if (other._dump != null) {
					return false;
				}
			} else if (!_dump.equals(other._dump)) {
				return false;
			}
			return true;
		}
	
		@Override
		public int hashCode()
		{
			final int prime = 31;
			int result = 1;
			result = prime * result + ((_dump == null) ? 0 : _dump.hashCode());
			return result;
		}
		
		public Properties getProperties()
		{
			Properties properties = new Properties();
			
			properties.setProperty(ICore.CORE_CREATE_TIME_PROPERTY, Long.toString(_dump.getCreationDate().getTime()));
			properties.setProperty(ICore.PROCESSOR_TYPE_PROPERTY,"s390");
			properties.setProperty(ICore.PROCESSOR_SUBTYPE_PROPERTY,"");
			properties.setProperty(ICore.SYSTEM_TYPE_PROPERTY,"z/OS");
			
			return properties;
		}
		
		public Platform getPlatform()
		{
			return Platform.ZOS;
		}
	}
	
	/**
	 * Adapter for Zebedee address spaces to make them implement the
	 * IAddressSpace interface
	 * 
	 * @author andhall
	 * 
	 */
	private static class AddressSpaceAdapter extends SearchableMemory implements IAddressSpace
	{

		private final AddressSpace addressSpace;
		private final ICore reader;

		/**
		 * Lazily-initialized list of Processes
		 */
		private Set<IProcess> processes;

		private AddressSpaceAdapter(AddressSpace as, ICore reader)
		{
			this.addressSpace = as;
			this.reader = reader;
		}

		public ICore getCore() {
			return reader;
		}
		
		public Collection<IProcess> getProcesses()
		{
			initializeProcesses();

			if (processes != null) {
				return Collections.unmodifiableList(new ArrayList<IProcess>(processes));
			} else {
				return Collections.emptyList();
			}
		}

		private void initializeProcesses()
		{
			if (processes != null) {
				return;
			}

			Tcb tc[] = Tcb.getTcbs(addressSpace);
			if (tc != null) {
				processes = new HashSet<IProcess>();
				
				Set<Edb> knownEdbs = new HashSet<Edb>();
				
				for (int i = 0; i < tc.length; ++i) {
					try {
						Caa ca = new Caa(tc[i]);
						Edb edb = ca.getEdb();
						if (! knownEdbs.contains(edb)) {
							knownEdbs.add(edb);
							// Some EDBs don't have any modules, so ignore them
							if (edb.getFirstDll() != null) {
								processes.add(new EdbAdapter(this, edb, ca));
							}
						}
					} catch (CaaNotFound e) {
					} catch (IOException e) {
					}
				}
			}
		}

		public int getAddressSpaceId()
		{
			return addressSpace.getAsid();
		}
		
		@Override
		public int hashCode()
		{
			final int prime = 31;
			int result = 1;
			result = prime * result
					+ ((addressSpace == null) ? 0 : addressSpace.hashCode());
			result = prime * result
					+ ((processes == null) ? 0 : processes.hashCode());
			result = prime * result
					+ ((reader == null) ? 0 : reader.hashCode());
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
			if (!(obj instanceof AddressSpaceAdapter)) {
				return false;
			}
			AddressSpaceAdapter other = (AddressSpaceAdapter) obj;
			if (addressSpace == null) {
				if (other.addressSpace != null) {
					return false;
				}
			} else if (!addressSpace.equals(other.addressSpace)) {
				return false;
			}
			if (processes == null) {
				if (other.processes != null) {
					return false;
				}
			} else if (!processes.equals(other.processes)) {
				return false;
			}
			if (reader == null) {
				if (other.reader != null) {
					return false;
				}
			} else if (!reader.equals(other.reader)) {
				return false;
			}
			return true;
		}

		public Platform getPlatform()
		{
			return Platform.ZOS;
		}

		public byte getByteAt(long address) throws MemoryFault
		{
			try {
				return addressSpace.readByte(address);
			} catch (IOException e) {
				throw new MemoryFault(address, e);
			}
		}

		public ByteOrder getByteOrder()
		{
			return ByteOrder.BIG_ENDIAN;
		}

		public int getBytesAt(long address, byte[] buffer) throws MemoryFault
		{
			return getBytesAt(address,buffer,0,buffer.length);
		}

		public int getBytesAt(long address, byte[] buffer, int offset,
				int length) throws MemoryFault
		{
			try {
				addressSpace.read(address, buffer, offset, length);
			} catch (IOException e) {
				throw new MemoryFault(address, e);
			}
			
			return length;
		}

		public int getIntAt(long address) throws MemoryFault
		{
			try {
				return addressSpace.readInt(address);
			} catch (IOException e) {
				throw new MemoryFault(address, e);
			}
		}

		public long getLongAt(long address) throws MemoryFault
		{
			try {
				return addressSpace.readLong(address);
			} catch (IOException e) {
				throw new MemoryFault(address, e);
			}
		}

		public Collection<? extends IMemoryRange> getMemoryRanges()
		{
			AddressRange[] zebedeeRanges = addressSpace.getAddressRanges();
			List<IMemoryRange> memoryRanges = new ArrayList<IMemoryRange>(zebedeeRanges.length);
			
			for (AddressRange thisRange : zebedeeRanges) {
				memoryRanges.add(new AddressRangeAdapter(addressSpace, thisRange));
			}
			
			return memoryRanges;
		}

		public short getShortAt(long address) throws MemoryFault
		{
			try {
				return addressSpace.readShort(address);
			} catch (IOException e) {
				throw new MemoryFault(address, e);
			}
		}

		//TODO look into these
		public boolean isExecutable(long address)
		{
			return false;
		}

		public boolean isReadOnly(long address)
		{
			return false;
		}

		public boolean isShared(long address)
		{
			return false;
		}
		
		public Properties getProperties(long address)
		{
			return new Properties();
		}

		public IMemoryRange getRangeForAddress(long address)
		{
			//Since we're not using AbstractMemory, it's tricky to use
			//MemorySourceTable to make this efficient. It's not used
			//often, so we'll settle for O(N) for the time being.
			for (IMemoryRange range : getMemoryRanges()) {
				if (range.contains(address)) {
					return range;
				}
			}
			return null;
		}
	}

	/**
	 * Adapter for EDB to make it implement IProcess.
	 * 
	 * TODO Adjust findPattern so it only searches the memory belonging to the
	 * Edb.
	 * 
	 * @author andhall
	 * 
	 */
	private static class EdbAdapter implements IProcess
	{
		private final AddressSpaceAdapter as;
		private final Edb edb;
		private final Caa caa;
		private IModule executable;
		private List<IModule> modules = new LinkedList<IModule>();
		private boolean modulesLoaded = false;
		private List<TCBAdapter> threads = null;

		private enum Direction { DOWN, UP };

		private EdbAdapter(AddressSpaceAdapter as, Edb edb, Caa caa)
		{
			this.edb = edb;
			this.as = as;
			this.caa = caa;
		}

		public int bytesPerPointer()
		{
			return this.caa.is64bit() ? 8 : 4;
		}

		public long getPointerAt(long address) throws MemoryFault
		{
			if (bytesPerPointer() == 8) {
				return getLongAt(address);
			} else {
				return getIntAt(address);
			}
		}

		public AddressSpaceAdapter getAddressSpace()
		{
			return as;
		}

		public long findPattern(byte[] whatBytes, int alignment, long startFrom)
		{
			return as.findPattern(whatBytes, alignment, startFrom);
		}

		public byte getByteAt(long address) throws MemoryFault
		{
			return as.getByteAt(address);
		}

		public ByteOrder getByteOrder()
		{
			return as.getByteOrder();
		}

		public int getBytesAt(long address, byte[] buffer) throws MemoryFault
		{
			return as.getBytesAt(address, buffer);
		}

		public int getBytesAt(long address, byte[] buffer, int offset,
				int length) throws MemoryFault
		{
			return as.getBytesAt(address, buffer, offset, length);
		}

		public int getIntAt(long address) throws MemoryFault
		{
			return as.getIntAt(address);
		}

		public long getLongAt(long address) throws MemoryFault
		{
			return as.getLongAt(address);
		}

		public Collection<? extends IMemoryRange> getMemoryRanges()
		{
			return as.getMemoryRanges();
		}

		public short getShortAt(long address) throws MemoryFault
		{
			return as.getShortAt(address);
		}

		@Override
		public String toString()
		{
			return "EdbAdapter for EDB 0x" + Long.toHexString(edb.address());
		}

		public String getCommandLine() throws DataUnavailableException
		{
			throw new DataUnavailableException("Command line not available on this platform");
		}

		public Properties getEnvironmentVariables() throws CorruptDataException
		{
			try {
				return edb.getEnvVars();
			} catch (IOException e) {
				throw new CorruptDataException(e);
			}
		}

		public IModule getExecutable() throws CorruptDataException
		{
			loadModules();
			
			return executable;
		}

		public List<IModule> getModules() throws CorruptDataException
		{
			loadModules();
			
			return Collections.unmodifiableList(modules);
		}
		
		private void loadModules() throws CorruptDataException
		{
			if (modulesLoaded) {
				return;
			}
			
			AddressRange rr[] = as.addressSpace.getAddressRanges();
			Map<Long, String> stackSyms = getStackSymbols();
			try {
				// Do the lookup once per symbol
				Map<Long, Dll> closestDlls = new HashMap<Long, Dll>();
				for (Iterator<Long> i = stackSyms.keySet().iterator(); i.hasNext();) {
					Long addr = (Long)i.next();
					long address = addr.longValue();
					Dll closest = closestDll(edb, address);
					closestDlls.put(addr, closest);
				}
				boolean firstPass = true;
				
				for (Dll dll = edb.getFirstDll(); dll != null; dll = dll.getNext()) {
					final DllFunction f[] = dll.getFunctions();
					final DllVariable g[] = dll.getVariables();
					// Find all the functions and variables in the DLL
					final String dllname = dll.getName();
					List<ISymbol> symbols = new ArrayList<ISymbol>();
					// Arrays of the lowest and highest symbol addresses found in each contiguous memory range
					long usedTextRangeLimits[][] = new long[rr.length][2];
					long usedDataRangeLimits[][] = new long[rr.length][2];
					for (int i = 0; i < rr.length; ++i) {
						// initialize lower limit addresses to high value
						usedTextRangeLimits[i][0] = Long.MAX_VALUE;
						usedDataRangeLimits[i][0] = Long.MAX_VALUE;
					}
					// Add functions
					for (int i = 0; i < f.length; ++i) {
						symbols.add(new Symbol(f[i].name, f[i].address));
						findRange(f[i].address, usedTextRangeLimits, rr);
					}
					
					// Add variables
					for (int i = 0; i < g.length; ++i) {
						symbols.add(new Symbol(g[i].getName(), g[i].getAddress()));
						findRange(g[i].getAddress(), usedDataRangeLimits, rr);
					}
					// Add stack symbols to this DLL if they are within the range of symbols in this DLL
					for (Iterator<Long> i = stackSyms.keySet().iterator(); i.hasNext();) {
						Long addr = i.next();
						long address = addr.longValue();
						Dll closest = (Dll)closestDlls.get(addr);
						// Fix when Zebedee does equals() for Dll
						//if (dll.equals(closest)) {
						if (closest != null && dll.getName().equals(closest.getName())) {
							// Check if the stack symbol is within the address range of functions for this DLL
							int r = findRange(address, rr);
							if (r >= 0 && (address > usedTextRangeLimits[r][0]) && (address < usedTextRangeLimits[r][1])) {
								String name = (String)stackSyms.get(addr);
								symbols.add(new Symbol(name, address));
								i.remove();
							}
						}
					}

					// Construct the memory sections for this DLL
					List<IMemoryRange> sections = new ArrayList<IMemoryRange>();
					for (int i = 0; i < rr.length; ++i) {
						if (usedTextRangeLimits[i][1] != 0) {
							long base = roundToPage(usedTextRangeLimits[i][0],Direction.DOWN);
							long size = roundToPage(usedTextRangeLimits[i][1],Direction.UP) - base;
							sections.add(new MemoryRange(this.as, base, size, ".text"));
						}
						if (usedDataRangeLimits[i][1] != 0) {
							long base = roundToPage(usedDataRangeLimits[i][0],Direction.DOWN);
							long size = roundToPage(usedDataRangeLimits[i][1],Direction.UP) - base;
							sections.add(new MemoryRange(this.as, base, size, ".data"));
						} 
					}
					
					long loadAddress;
					try {
						// Not yet implemented by Zebedee, which throws an Error() 
						loadAddress = dll.getLoadAddress();
					} catch (Error e) {	
						// since the load address for z/OS modules is not available, find the first procedure symbol
						loadAddress = Long.MAX_VALUE;
						for (int i = 0; i < f.length; ++i) {
							loadAddress = Math.min(f[i].address, loadAddress);
						}
						if (loadAddress == Long.MAX_VALUE) {
							loadAddress = 0;
						} else {
							loadAddress = roundToPage(loadAddress,Direction.DOWN);
						}
					}
					
					Properties props = new Properties();
					props.setProperty("Load address", format(loadAddress));
					props.setProperty("Writable Static Area address",format(dll.getWsa()));
					
					IModule module = new Module(this, dllname, symbols, sections, loadAddress, props);
				
					if (firstPass) {
						executable = module;
						firstPass = false;
					} else  {
						modules.add(module);
					}
				}
				
				if (stackSyms.size() > 0) {
					// Unrecognised symbols, so put them into a dummy DLL where they can be found for printing stack frames
					final String dllname = "ExtraSymbolsModule";
					long loadAddress = Long.MAX_VALUE;
					List<ISymbol> symbols = new ArrayList<ISymbol>();
					// Array of lowest and highest symbol addresses found in each contiguous memory range
					long usedTextRangeLimits[][] = new long[rr.length][2];
					// initialize start address to high
					for (int i = 0; i < rr.length; ++i) {
						usedTextRangeLimits[i][0] = Long.MAX_VALUE;
					}
					// Add stack symbols in this DLL
					for (Iterator<Long> i = stackSyms.keySet().iterator(); i.hasNext();) {
						Long addr = i.next();
						long address = addr.longValue();
						loadAddress = Math.min(address, loadAddress);
						// Find which ranges apply
						findRange(address, usedTextRangeLimits, rr);
						String name = (String)stackSyms.get(addr);
						symbols.add(new Symbol(name, address));
						i.remove();
					}
					if (loadAddress == Long.MAX_VALUE) {
						loadAddress = 0;
					} else {
						loadAddress = roundToPage(loadAddress,Direction.DOWN);
					}

					Properties props = new Properties();
					props.setProperty("Load address", format(loadAddress));
					List<IMemoryRange> sections = new ArrayList<IMemoryRange>();

					for (int i = 0; i < rr.length; ++i) {
						if (usedTextRangeLimits[i][1] != 0) {
							long base = roundToPage(usedTextRangeLimits[i][0],Direction.DOWN);
							long size = roundToPage(usedTextRangeLimits[i][1],Direction.UP) - base;
							sections.add(new MemoryRange(this.as, base, size, ".text"));
						}
					}
					
					IModule mod = new Module(this, dllname, symbols, sections, loadAddress, props);
					modules.add(mod);
				}
			} catch (IOException e) {
				logger.log(Level.FINE, "Problem for Zebedee finding module information", e);
			}
			
			modulesLoaded = true;
		}
		
		private Map<Long, String> getStackSymbols() throws CorruptDataException
		{
			Map<Long, String> result = new HashMap<Long, String>();
			
			for (TCBAdapter thread : getThreads()) {
				result.putAll(thread.getStackSymbols());
			}
			
			return result;
		}

		/**
		 * Checks whether an address is found within a contiguous address range, and if so update
		 * the lowest and/or highest addresses found so far for that address range.
		 * 
		 * @param address - address of symbol (function or static variable)
		 * @param usedRangeLimits - array of low and high addresses found per range
		 * @param rr - array of contiguous memory address ranges
		 * @return void
		 */
		private void findRange(long address, long[][] usedRangeLimits, AddressRange rr[]) {
			int r = findRange(address, rr);
			
			if (r >= 0) {
				usedRangeLimits[r][0] = Math.min(usedRangeLimits[r][0], address);
				usedRangeLimits[r][1] = Math.max(usedRangeLimits[r][1], address);
			}
		}
		
		/**
		 * Sees which AddressRange the routine it is in. 
		 * @param routine
		 * @param rr array of address ranges
		 * @return -2: not found, n: index of AddressRange
		 */
		private int findRange(long routine, AddressRange rr[]) {
			// Which section?
			for (int i = 0; i < rr.length; ++i) {
				if (rr[i].getStartAddress() <= routine && routine <= rr[i].getEndAddress()) {
					/* Found */
					logger.fine("Found address "+format(routine)+" at "+format(rr[i].getStartAddress())+":"+format(rr[i].getEndAddress()));
					return i;
				}
			}
			// Not found on the address range
			logger.fine("Didn't find address "+format(routine));
			return -2;
		}

		/**
		 * Round a memory address up or down to a 4K page boundary
		 * 
		 * @param address - input memory address to be rounded
		 * @param down - down is true, up is false
		 * @return long - rounded address
		 */
		private long roundToPage(long address, Direction direction) {
			if (direction == Direction.DOWN) {
				return address & 0xfffffffffffff000L;
			} else {
				return (address & 0xfffffffffffff000L) + 0x1000L;
			}
		}

		/**
		 * Find the Dll with a symbol closest to the requested address
		 * @param edb
		 * @param address
		 * @return The DLL which contains a symbol closest to the address
		 * @throws IOException
		 */
		private Dll closestDll(Edb edb, long address) throws IOException {
			Dll closestDll = null;
			long closestDist = Long.MAX_VALUE;
			for (Dll dll = edb.getFirstDll(); dll != null; dll = dll.getNext()) {
				//CMVC 175870 : catch exceptions to allow the search to proceed to the next DLL
				try {
					final DllFunction f[] = dll.getFunctions();
					for (int i = 0; i < f.length; ++i) {
						long dist = Math.abs(f[i].address - address);
						if (dist < closestDist) {
							closestDll = dll;
							closestDist = dist;
						}
					}
					final DllVariable g[] = dll.getVariables();
					for (int i = 0; i < g.length; ++i) {
						long dist = Math.abs(g[i].getAddress() - address);
						if (dist < closestDist) {
							closestDll = dll;
							closestDist = dist;
						}
					}	
				} catch (IOException e) {
					logger.log(Level.FINE, "Skipping DDL due to IOException ", e);
				}
			}
			return closestDll;
		}

		public long getProcessId() throws CorruptDataException
		{
			return edb.address();
		}
		
		public boolean isExecutable(long address)
		{
			return as.isExecutable(address);
		}

		public boolean isReadOnly(long address)
		{
			return as.isReadOnly(address);
		}

		public boolean isShared(long address)
		{
			return as.isShared(address);
		}
		
		public Properties getProperties(long address) {
			return as.getProperties(address);
		}
		
		public String getProcedureNameForAddress(long address) throws CorruptDataException, DataUnavailableException {
			return getProcedureNameForAddress(address, false);
		}
		
		public String getProcedureNameForAddress(long address, boolean dtfjFormat) throws CorruptDataException, DataUnavailableException
		{
			//Weirdness in z/OS DLL regions (some overlapping) breaks the SymbolUtil.getProcedureNameForAddress() code.
			//Instead, iterate over all symbols and find the closest.
			
			ISymbol closestSymbol = null;
			IModule closestModule = null;
			long diff = Long.MAX_VALUE;
			
			for (IModule thisModule : getModules()) {
				for (ISymbol thisSymbol : thisModule.getSymbols()) {
					if (Addresses.lessThan(thisSymbol.getAddress(), address)) {
						long thisDiff = address - thisSymbol.getAddress();
						
						if (thisDiff < diff) {
							diff = thisDiff;
							closestSymbol = thisSymbol;
							closestModule = thisModule;
						}
					}
				}
			}
			
			if (closestSymbol != null) {
				return closestModule.getName() + "::" + closestSymbol.getName() + "+0x" + Long.toHexString(diff);
			} else {
				return "<unknown>";
			}
		}

		public List<TCBAdapter> getThreads() throws CorruptDataException
		{
			if( threads == null ) {
				List<TCBAdapter> threadsList = new LinkedList<TCBAdapter>();
				Tcb tcbs[] = Tcb.getTcbs(as.addressSpace);

				for (Tcb tcb : tcbs) {
					if (null == tcb) {
						continue;
					}

					Caa caa = null;
					try {
						caa = new Caa(tcb);
					} catch (CaaNotFound e) {
						continue;
					}

					if (caa.getEdb().address() != edb.address()) {
						continue;
					}

					threadsList.add(new TCBAdapter(tcb, caa, this));
				}
				threads = Collections.unmodifiableList(threadsList);
			}
			
			return threads;
		}

		public Platform getPlatform()
		{
			return Platform.ZOS;
		}

		@Override
		public int hashCode()
		{
			final int prime = 31;
			int result = 1;
			result = prime * result + ((caa == null) ? 0 : caa.hashCode());
			result = prime * result + ((edb == null) ? 0 : edb.hashCode());
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
			if (!(obj instanceof EdbAdapter)) {
				return false;
			}
			EdbAdapter other = (EdbAdapter) obj;
			if (caa == null) {
				if (other.caa != null) {
					return false;
				}
			} else if (!caa.equals(other.caa)) {
				return false;
			}
			if (edb == null) {
				if (other.edb != null) {
					return false;
				}
			} else if (!edb.equals(other.edb)) {
				return false;
			}
			return true;
		}

		public int getSignalNumber() throws DataUnavailableException
		{
			throw new DataUnavailableException("Signal number not available on z/OS");
		}

		public boolean isFailingProcess() throws DataUnavailableException {
			try {
				return caa.getTcb().tcbcmp() != 0L;
			} catch (IOException e) {
				throw new DataUnavailableException("Couldn't determine TCB CMP status",e);
			}
		}
		
	}

	/**
	 * Adapter for Zebedee AddressRanges to make them look like IMemoryRanges
	 * 
	 * @author andhall
	 * 
	 */
	private static class AddressRangeAdapter extends ProtectedMemoryRange
	{

		private final AddressSpace addressSpace;
		private final String name;

		public AddressRangeAdapter(AddressSpace space, AddressRange range, String name)
		{
			super(range.getStartAddress(),range.getLength());
			this.addressSpace = space;
			this.name = name;
		}
		
		public AddressRangeAdapter(AddressSpace space, AddressRange range)
		{
			this(space,range,null);
		}

		public int getAddressSpaceId()
		{
			return addressSpace.getAsid();
		}

		@Override
		public String toString()
		{
			return String.format("TDumpReader address range from 0x%X to 0x%X in address space %x",getBaseAddress(),getTopAddress(),getAddressSpaceId());
		}

		public String getName()
		{
			return name;
		}

		@Override
		public int hashCode()
		{
			final int prime = 31;
			int result = 1;
			result = prime * result
					+ ((addressSpace == null) ? 0 : addressSpace.hashCode());
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
			if (!(obj instanceof AddressRangeAdapter)) {
				return false;
			}
			AddressRangeAdapter other = (AddressRangeAdapter) obj;
			if (addressSpace == null) {
				if (other.addressSpace != null) {
					return false;
				}
			} else if (!addressSpace.equals(other.addressSpace)) {
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
		
	}
	
	private static class TCBAdapter implements IOSThread
	{
		private static final int NUMBER_OF_GP_REGISTERS = 16;
		private final Tcb tcb;
		private final Caa caa;
		private final Properties props = new Properties();
		private final EdbAdapter edb;
		private List<DSAAdapter> stackFrames = new LinkedList<DSAAdapter>();
		private boolean stacksWalked = false;
		
		public TCBAdapter(Tcb tcb, Caa caa, EdbAdapter edb)
		{
			this.tcb = tcb;
			this.caa = caa;
			this.edb = edb;
			
			props.setProperty("TCB", format(tcb.address()));
			props.setProperty("CAA", format(caa.address()));
			props.setProperty("EDB", format(caa.getEdb().address()));
			try {
				props.setProperty("PTHREADID", format(caa.getPthreadId()));
			} catch (IOException e) {
				props.setProperty("PTHREADID", "unknown");
			}
			
			try {
				props.setProperty("Stack direction", caa.ceecaa_stackdirection() == 0 ? "up" : "down");
				props.setProperty("CAA CEL level", format(caa.ceecaalevel()));
			} catch (IOException e) {
				//TODO handle
			}
			
			// Get the Task Completion Code
			try {
				int code = tcb.space().readInt(tcb.address()+0x10);
				props.setProperty("Task Completion Code", format(code));
			} catch (IOException e) {
				//TODO handle
			}
			
			try {
				long psw = tcb.getRegisters().getPSW();
				String pswn = null;
				switch ((int)(psw >> 31) & 3) {
				case 0x0:
					pswn = "PSW:24";
					break;
				case 0x1:
					pswn = "PSW:31";
					break;
				default:
					pswn = "PSW:64";
					break;
				}
				
				props.setProperty(pswn, format(psw));
			} catch (IOException e) {
				//TODO handle
			}
			
			/*
			 * see if this is a failing thread - in the zebedee code if the task completion code is non-zero then
			 * the code also sets a failingregisters RegisterSet on the CAA. This is the same set as is returned 
			 * for registers, so all we need to do is set a property if the failingregister set is not null.
			 */
			if(null != caa.getFailingRegisters()) {
				props.setProperty("FailingRegisters", "true");
			}
			
			// Set up IFA/zAAP/zIIP enabled properties for this thread, if the information is available
			try {
				props.setProperty("IFA Enabled", tcb.isIFAEnabled() ? "yes" : "no");
				props.setProperty("WEBFLAG2", String.format("0x%02x",tcb.getIFAFlags())); // the actual bit-field
			} catch (IOException e) {
				// Unable to get IFA information, leave properties unset
			}
		}

		public Map<Long, String> getStackSymbols()
		{
			//Populate stackFrames
			this.getStackFrames();
			
			Map<Long,String> symbols = new HashMap<Long,String>();
			
			for (DSAAdapter frame : getStackFrames()) {
				Symbol sym = frame.getSymbol();
				symbols.put(sym.getAddress(), sym.getName());
			}
			
			return symbols;
		}

		public Collection<? extends IMemoryRange> getMemoryRanges()
		{
			List<? extends IOSStackFrame> frames = getStackFrames();
			Set<IMemoryRange> ranges = new HashSet<IMemoryRange>();
			
			for (IOSStackFrame frame : frames) {
				IMemoryRange range = edb.getAddressSpace().getRangeForAddress(frame.getBasePointer());
				
				if (range != null) {
					ranges.add(new MemoryRange(edb.getAddressSpace(),range,"Stack"));
				}
			}
			
			return ranges;
		}

		public Properties getProperties()
		{
			return props;
		}

		public List<IRegister> getRegisters()
		{
			RegisterSet rs = new RegisterSet();
			DsaStackFrame dsa = caa.getCurrentFrame();
			
			// Get registers from the current (top) stack frame if available, else from the TCB
			if (dsa != null && dsa.getRegisterSet() != null) {
				rs = dsa.getRegisterSet();
			} else {
				try {
					rs = tcb.getRegisters();
				} catch (IOException e) {
					try {
						rs = tcb.getRegistersFromBPXGMSTA();
					} catch (IOException e2) {
						// Failed to find registers, leave RegisterSet empty
					}
				}
			}
			List<IRegister> regs = new ArrayList<IRegister>(NUMBER_OF_GP_REGISTERS + 1);
			long psw = rs.getPSW();
			
			regs.add(new Register("PSW", psw));
			// There isn't an easy way to get the total number of registers, so hard code it
			for (int i = 0; i < NUMBER_OF_GP_REGISTERS; ++i) {
				regs.add(new Register("R"+i, rs.getRegister(i)));
			}
			
			return regs;
		}

		public List<DSAAdapter> getStackFrames()
		{
			if (!stacksWalked) {
				DsaStackFrame dsa = null;
				try {
					dsa = caa.getCurrentFrame();
				} catch (Exception e) {
					//TODO handle
				}
				
				if (dsa == null) {
					// No stack frames available, leave an indication of the problem
					//TODO handle this case
					// Don't add corruptData to the stackFrames as getStackFrames throws DataUnavailable for us if the list is empty
				} else { 
					try {
						for (; dsa != null; dsa = dsa.getParentFrame()) {
							stackFrames.add(new DSAAdapter(dsa, edb));
						}
					} catch (Exception e) {
						//TODO handle
					}
				}
				
				stacksWalked = true;
			}

			return stackFrames;
		}

		public long getThreadId() throws CorruptDataException
		{
			try {
				return caa.getPthreadId() >>> 32;
			} catch (IOException ex) {
				throw new CorruptDataException(ex);
			}
		}

		public long getInstructionPointer() {
			DSAAdapter dsa = null;
			if (stacksWalked) {
				if (stackFrames.isEmpty()) {
					logger.log(Level.FINE, "No stack frames for DSA");
					return 0;
				} else {
					dsa = stackFrames.get(0);
				}
			} else {
				try {
					dsa = new DSAAdapter(caa.getCurrentFrame(), null);
				} catch (Error e) {
					logger.log(Level.FINE, "Failed to read dsa from core", e);
					return 0;
				}
			}
				
			return dsa.getInstructionPointer();
		}

		public long getBasePointer() {
			DSAAdapter dsa = null;
			if (stacksWalked) {
				if (stackFrames.isEmpty()) {
					logger.log(Level.FINE, "No stack frames for DSA");
					return 0;
				} else {
					dsa = stackFrames.get(0);
				}
			} else {
				try {
					dsa = new DSAAdapter(caa.getCurrentFrame(), null);
				} catch (Error e) {
					logger.log(Level.FINE, "Failed to read dsa from core", e);
					return 0;
				}
			}
				
			return dsa.getBasePointer();
		}

		public long getStackPointer() {
			/*
			 * Reference is http://publibz.boulder.ibm.com/epubs/pdf/ceev1190.pdf , see chapter 3, register usage sections.
			 * 
			 * Calling conventions:
			 * standard
			 * fastlink - GPR13 => the callers stack frame in the Language Environment stack. Each such stack frame begins with a 36-word save area.
			 * xplink - GPR4 => the callers stack frame in the downward- growing stack. This is biased and actually points to 2048 bytes before the real start of the stack frame 
			 */
			DSAAdapter dsa = null;
			if (stacksWalked) {
				if (stackFrames.isEmpty()) {
					logger.log(Level.FINE, "No stack frames for DSA");
					return 0;
				} else {
					dsa = stackFrames.get(0);
				}
			} else {
				try {
					dsa = new DSAAdapter(caa.getCurrentFrame(), null);
				} catch (Error e) {
					logger.log(Level.FINE, "Failed to read dsa from core", e);
					return 0;
				}
			}
				
			return dsa.getStackPointer();
		}

	}

	private static class DSAAdapter implements IOSStackFrame
	{
		private final DsaStackFrame dsa;
		private final EdbAdapter edb;
		
		public DSAAdapter(DsaStackFrame dsa, EdbAdapter edb)
		{
			this.dsa = dsa;
			this.edb = edb;
		}

		public Symbol getSymbol()
		{
			return new Symbol(dsa.getEntryName(),dsa.getEntryPoint());
		}

		public long getBasePointer()
		{
			return dsa.getDsaAddress();
		}

		public long getInstructionPointer()
		{
			return dsa.getEntryPoint() + dsa.getEntryOffset();
		}

		public long getStackPointer() {
			/*
			 * File jit/codert/SignalHandler.c:761 states that the java SP is r5
			 */
			return dsa.getRegisterSet().getRegister(5);
		}

	}

	private static String format(long l) {
		return "0x"+Long.toHexString(l);
	}
}
