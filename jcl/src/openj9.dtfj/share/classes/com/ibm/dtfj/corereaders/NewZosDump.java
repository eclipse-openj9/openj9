/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
package com.ibm.dtfj.corereaders;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.addressspace.DumpReaderAddressSpace;
import com.ibm.dtfj.addressspace.IAbstractAddressSpace;
import com.ibm.dtfj.corereaders.zos.dumpreader.AddressRange;
import com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace;
import com.ibm.dtfj.corereaders.zos.le.Caa;
import com.ibm.dtfj.corereaders.zos.le.CaaNotFound;
import com.ibm.dtfj.corereaders.zos.le.Dll;
import com.ibm.dtfj.corereaders.zos.le.DllFunction;
import com.ibm.dtfj.corereaders.zos.le.DllVariable;
import com.ibm.dtfj.corereaders.zos.le.DsaStackFrame;
import com.ibm.dtfj.corereaders.zos.le.Edb;
import com.ibm.dtfj.corereaders.zos.mvs.RegisterSet;
import com.ibm.dtfj.corereaders.zos.mvs.Tcb;

public class NewZosDump implements ICoreFileReader {

	private final static int DR1 = 0xC4D9F140;		// 31-bit OS
	private final static int DR2 = 0xC4D9F240;		// 64-bit OS
	// Shared memory address space has defined ASID, ebcdic 'IARH'.
	private final static int SHARED_MEMORY_ASID = 0xc9c1d9c8;

	// z/OS dumps are split into lots and lots of records
	private final static int RECORD_HEADER_LEN = 0x40;
	private final static int RECORD_BODY_LEN = 0x1000;

	private final static int RECORD_LEN = RECORD_HEADER_LEN + RECORD_BODY_LEN;

	private List _additionalFileNames = new ArrayList();

	private IAbstractAddressSpace _space;

	private ImageInputStream stream;
	private boolean _is64Bit;
	
	private Object _failingThread = null;

	
	
	/** Maintains the list of Java address spaces */
	private HashMap _javaAddressSpaces = new LinkedHashMap();
	
    /** Logger */
    private static Logger log = Logger.getLogger(NewZosDump.class.getName());

	// Zebedee variables
	private com.ibm.dtfj.corereaders.zos.dumpreader.Dump _dump;
	private AddressSpace _zebedeeAddressSpaces[];
	private boolean zebedeeInitialized;
	
	private J9RASReader _j9rasReader = null;
	private NewZosDump(ImageInputStream in)
	{
		this.stream = in;
		_is64Bit = false;

		// Read the memory ranges and also look for the Java address spaces
		List memoryRanges = readTDUMP();

		// List of memory ranges which are in Java address spaces
		List keepList = null;
		
		// Collect together the address ranges associated with the IDs
		for (Iterator iter = _javaAddressSpaces.values().iterator(); iter.hasNext();) {
			int []asidinfo = (int[])iter.next();
			List onASID = keepMemoryRangesWithAsid(asidinfo, memoryRanges);
			if (keepList != null) {
				keepList.addAll(onASID);
			} else {
				keepList = onASID;
			}
			// Accumulate a global 64-bit flag
			if (asidinfo[1] != 0) _is64Bit = true; 
		}
		
		if (keepList != null) {
			MemoryRange[] rawRanges = (MemoryRange[]) keepList.toArray(new MemoryRange[keepList.size()]);
			_space = new DumpReaderAddressSpace(rawRanges, new DumpReader(stream, _is64Bit), false, _is64Bit);
		}
		
		_j9rasReader = new J9RASReader(_space, _is64Bit);
		
	}

	public String format(int i) {
		return "0x"+Integer.toHexString(i);
	}
	
	public String format(long l) {
		return "0x"+Long.toHexString(l);
	}
	
	/**
	 * Create a Zebedee viewer of the dump file
	 * @param file
	 */
	private void initializeZebedeeDump(ClosingFileReader file) {
		if (!zebedeeInitialized) {
			try {
				log.fine("Building Zebedee dump from "+file);
				String fileName = file.getAbsolutePath();
				if (file.isMVSFile()) {
					fileName = fileName.substring(fileName.lastIndexOf('/')+1);
				}
				_dump = new com.ibm.dtfj.corereaders.zos.dumpreader.Dump(fileName, file, false);				

				_zebedeeAddressSpaces = _dump.getAddressSpaces();
				zebedeeInitialized = true;
			} catch (FileNotFoundException e) {
				log.log(Level.FINE, "Problem for Zebedee finding dump file", e);
			}
		}
	}
	
	private void initializeZebedeeDump(ImageInputStream stream) {
		if (!zebedeeInitialized) {
			try {
				log.fine("Building Zebedee dump from stream");
				
				_dump = new com.ibm.dtfj.corereaders.zos.dumpreader.Dump("Stream", stream, false);				

				_zebedeeAddressSpaces = _dump.getAddressSpaces();
				zebedeeInitialized = true;
			} catch (FileNotFoundException e) {
				log.log(Level.FINE, "Problem for Zebedee finding dump file", e);
			}
		}
	}
	
	/**
	 * Finds the Zebedee address space associated with the address space identifier
	 * @param asid
	 * @return Zebedee address space
	 */
	private AddressSpace findZebedeeAddressSpace(int asid) {
		AddressSpace adrJava = null;
		if (_zebedeeAddressSpaces != null) {
			for (int i = 0; i < _zebedeeAddressSpaces.length; ++i) {
				if (asid == _zebedeeAddressSpaces[i].getAsid()) {
					adrJava = _zebedeeAddressSpaces[i];
					break;
				}
			}
		}
		return adrJava;
	}
	
	/**
	 * Get an array of EDBs for the current address space
	 * Convert to using HashSet when is Zebedee fixed to have Edb.equals work properly
	 * @param as Zebedee address space
	 * @return All the EDBs for the address space
	 */
	private Edb[] getEdbs(AddressSpace as) {
		Map edbs = new HashMap();
		Tcb tc[] = Tcb.getTcbs(as);
		if (tc != null) for (int i = 0; i < tc.length; ++i) {
			 try {
				 Caa ca = new Caa(tc[i]);
				 Edb edb = ca.getEdb();
				 // Some EDBs don't have any modules, so ignore them
				 if (edb.getFirstDll() != null) {
					 edbs.put(Long.valueOf(edb.address()), edb);
				 }
			 } catch (CaaNotFound e) {
			 } catch (IOException e) {
			 }
		}
		Edb edb[] = new Edb[edbs.size()];
		edbs.values().toArray(edb);
		return edb;
	}

	/**
	 * Gets a list of threads in the Java address space
	 * @param builder
	 * @param imgAdr The Image address space
	 * @param addressSpace The Zebedee address space
	 * @param edb only consider threads from this enclave in the address space
	 * @param stackSyms a map of (address,symbol) pairs of stack frame symbols found when processing frames  
	 * @return List of ImageThreads
	 */
	private List getThreads(final Builder builder, final Object imgAdr, AddressSpace addressSpace, Edb edb, Map stackSyms) {
		List threads = new ArrayList();
		
		//get the thread ID of the failing thread
		long tid = -1;
		if (null != _j9rasReader) {
			try {
				tid = _j9rasReader.getThreadID();
			} catch (Exception e) {
				//do nothing
			}
		}
		
		AddressRange rr[] = addressSpace.getAddressRanges();
		Tcb tc[] = Tcb.getTcbs(addressSpace);
		// Walk through TCBs
		if (tc != null) for (int j = 0; j < tc.length; ++j) {
			try {
				log.fine("TCB "+format(tc[j].address()));
				final Caa caa = new Caa(tc[j]);
				log.fine("CAA "+format(caa.address())+" "+caa.whereFound());
				
				if (caa.getEdb().address() != edb.address()) {
					log.fine("Skipping CAA as edb "+format(caa.getEdb().address())+" != edb for process "+format(edb.address()));
					continue;
				}

				 
				RegisterSet rs;
				try {
					rs = tc[j].getRegisters();
				} catch (IOException e) {
					try {
						rs = tc[j].getRegistersFromBPXGMSTA();
					} catch (IOException e2) {						
						rs = caa.getCurrentFrame().getRegisterSet();
					}
				}
				ArrayList regs = new ArrayList();
				long psw = rs.getPSW();
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
				regs.add(builder.buildRegister("PSW", Long.valueOf(psw)));
				// There isn't an easy way to get the total number of registers, so hard code it
				for (int i = 0; i < 16; ++i) {
					regs.add(builder.buildRegister("R"+i, Long.valueOf(rs.getRegister(i))));
				}
				Properties props = new Properties();				
				props.setProperty("TCB", format(caa.getTcb().address()));
				props.setProperty("CAA", format(caa.address()));
				props.setProperty("EDB", format(caa.getEdb().address()));
				props.setProperty(pswn, format(psw));
				try {
					props.setProperty("Stack direction", caa.ceecaa_stackdirection() == 0 ? "up" : "down");
					props.setProperty("CAA CEL level", format(caa.ceecaalevel()));
				} catch (IOException e) {
					log.log(Level.FINE, "Problem finding stack information", e);			
				}
				// Get the Task Completion Code
				try {
					int code = tc[j].space().readInt(tc[j].address()+0x10);
					props.setProperty("Task Completion Code", format(code));
				} catch (IOException e) {
				}
				// Not yet implemented by Zebedee
				//props.setProperty("Failed",Boolean.toString(caa.hasFailed()));
							
				// List of stack frames from thread
				ArrayList stackFrames = new ArrayList();
				
				// Find sections for stack frames
				ArrayList stackSections = new ArrayList();
				// Avoid adding a range as section multiple times
				boolean usedRange[] = new boolean[rr.length];

				try {
					DsaStackFrame dsa = caa.getCurrentFrame();
					if (dsa == null) {
						log.fine("Null current frame for CAA "+format(caa.address()));												
						// No stack frames available, leave an indication of the problem
						stackSections.add(builder.buildCorruptData(imgAdr, "Null stack frame so no stack sections with CAA", caa.address()));
						// Don't add corruptData to the stackFrames as getStackFrames throws DataUnavailable for us if the list is empty
					} else try {
						for (; dsa != null; dsa = dsa.getParentFrame()) {
							final Object builderStackFrame = builder.buildStackFrame(imgAdr, dsa.getDsaAddress(), dsa.getEntryPoint()+dsa.getEntryOffset());
							// Remember the entry point in case it is not exported from a DLL
							stackSyms.put(Long.valueOf(dsa.getEntryPoint()), dsa.getEntryName());
							stackFrames.add(builderStackFrame);										

							long dsaAddr = dsa.getDsaAddress();
							int i = findRange(dsaAddr, usedRange, rr);
							if (i >= 0) {
								// Should stack sections be more related to z/OS up-stack pieces and down-stack areas rather than dump pages?
								stackSections.add(builder.buildStackSection(imgAdr,rr[i].getStartAddress(),rr[i].getStartAddress()+rr[i].getLength()));
							} else if (i == -2) {
								log.fine("Unable to find stack section for DSA "+format(dsa.getDsaAddress()));																				
							}
						}
					} catch (Error e) {
						long dsaAddress = dsa != null ? dsa.getDsaAddress() : 0;
						log.log(Level.FINE, "Problem finding parent frame for DSA "+format(dsaAddress), e);												
						// Catch the error and abort processing the stack frame, leave an indication of the problem
						stackFrames.add(builder.buildCorruptData(imgAdr, "Corrupt stack frame with DSA "+e.getMessage(), dsaAddress));
						stackSections.add(builder.buildCorruptData(imgAdr, "Corrupt stack frame with DSA "+e.getMessage(), dsaAddress));						
					}
				} catch (Error e) {
					log.log(Level.FINE, "Problem finding current frame for CAA "+format(caa.address()), e);												
					// Catch the error and abort processing any stack frames, leave an indication of the problem
					stackFrames.add(builder.buildCorruptData(imgAdr, "Corrupt stack frames with CAA "+e.getMessage(), caa.address()));
					stackSections.add(builder.buildCorruptData(imgAdr, "Corrupt stack frames with CAA "+e.getMessage(), caa.address()));
				}
				// We use the pthread id, if available, as identity for the thread because this allows correlation with the Java 
				// threads, which also use pthread id. Note that the zOS TCB address is also provided, via an ImageThread property.
				String threadId = "";
				try {
					threadId = format(caa.getPThreadID());
				} catch (IOException e) {
					threadId = "<unknown>";
				}	
				log.fine("Building thread "+threadId+" with "+stackSections.size()+" sections");
				Object it = builder.buildThread(threadId, regs.iterator(), stackSections.iterator(), 
					stackFrames.iterator(), props, 0 /* signal number */);
				
				if (-1 != tid && format(tid).equals(threadId)) {
					_failingThread = it;
				}

				threads.add(it);
				
			} catch (CaaNotFound e) {
				log.log(Level.FINE, "Problem finding CAA for TCB "+format(tc[j].address()), e);												
			}
		}
		return threads;
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
				long dist = Math.abs(g[i].address - address);
				if (dist < closestDist) {
					closestDll = dll;
					closestDist = dist;
				}
			}			
		}
		return closestDll;
	}
	
	/**
	 * Gets list of modules in the enclave/address space
	 * @param builder
	 * @param imgAdr The Image address space
	 * @param addressSpace The Zebedee address space
	 * @param edb The enclave to consider
	 * @param stackSyms extra symbols from stack frames which might not be exported from DLLs
	 * @return list of ImageModules
	 */
	private List getModules(final Builder builder, final Object imgAdr, AddressSpace addressSpace, Edb edb, Map stackSyms) {
		List modules = new ArrayList();
		AddressRange rr[] = addressSpace.getAddressRanges();
		try {
			// Do the lookup once per symbol
			Map closestDlls = new HashMap();
			for (Iterator i = stackSyms.keySet().iterator(); i.hasNext();) {
				Long addr = (Long)i.next();
				long address = addr.longValue();
				Dll closest = closestDll(edb, address);
				closestDlls.put(addr, closest);
			}
			for (Dll dll = edb.getFirstDll(); dll != null; dll = dll.getNext()) {
				final DllFunction f[] = dll.getFunctions();
				final DllVariable g[] = dll.getVariables();
				// Find all the functions and variables in the DLL
				final String dllname = dll.getName();
				ArrayList symbols = new ArrayList();
				boolean usedRange[] = new boolean[rr.length];
				boolean usedDataRange[] = new boolean[rr.length];
				// Add functions
				for (int i = 0; i < f.length; ++i) {
					symbols.add(builder.buildSymbol(imgAdr, f[i].name, f[i].address));
					findRange(f[i].address, usedRange, rr);
				}
				// Add variables
				for (int i = 0; i < g.length; ++i) {
					symbols.add(builder.buildSymbol(imgAdr, g[i].name, g[i].address));
					findRange(g[i].address, usedDataRange, rr);
				}
				// Add stack symbols in this DLL if they are closest to symbols in this DLL
				for (Iterator i = stackSyms.keySet().iterator(); i.hasNext();) {					
					Long addr = (Long)i.next();
					long address = addr.longValue();
					Dll closest = (Dll)closestDlls.get(addr);
					// Fix when Zebedee does equals() for Dll
					//if (dll.equals(closest)) {
					if (closest != null && dll.getName().equals(closest.getName())) {
						// Check the stack symbol is in this DLL's ranges
						int r = findRange(address, rr);
						if (r >= 0 && usedRange[r]) {
							String name = (String)stackSyms.get(addr);
							symbols.add(builder.buildSymbol(imgAdr, name, address));
							i.remove();
						}
					}
				}

				Properties props = new Properties();
				ArrayList sections = new ArrayList();

				for (int i = 0; i < usedRange.length; ++i) {
					if (usedRange[i]) {
						sections.add(builder.buildModuleSection(imgAdr,".text", rr[i].getStartAddress(),rr[i].getStartAddress()+rr[i].getLength()));						
					} else if (usedDataRange[i]) {
						sections.add(builder.buildModuleSection(imgAdr,".data", rr[i].getStartAddress(),rr[i].getStartAddress()+rr[i].getLength()));						
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
					loadAddress = guessLoadAddress(loadAddress, rr);
				}
				props.setProperty("Load address", format(loadAddress));
				props.setProperty("Writable Static Area address",format(dll.getWsa()));
				Object mod = builder.buildModule(dllname, props, sections.iterator(), symbols.iterator(), loadAddress);
				modules.add(mod);
			}
			if (stackSyms.size() > 0) {
				// Unrecognised symbols, so put them into a dummy DLL where they can be found for printing stack frames
				final String dllname = "ExtraSymbolsModule";
				long loadAddress = Long.MAX_VALUE;
				ArrayList symbols = new ArrayList();
				boolean usedRange[] = new boolean[rr.length];
				// Add stack symbols in this DLL
				for (Iterator i = stackSyms.keySet().iterator(); i.hasNext();) {					
					Long addr = (Long)i.next();
					long address = addr.longValue();
					loadAddress = Math.min(address, loadAddress);
					// Find which ranges apply
					findRange(address, usedRange, rr);
					String name = (String)stackSyms.get(addr);
					symbols.add(builder.buildSymbol(imgAdr, name, address));
					i.remove();
				}
				loadAddress = guessLoadAddress(loadAddress, rr);

				Properties props = new Properties();
				props.setProperty("Load address", format(loadAddress));
				ArrayList sections = new ArrayList();

				for (int i = 0; i < usedRange.length; ++i) {
					if (usedRange[i]) {
						sections.add(builder.buildModuleSection(imgAdr,".text", rr[i].getStartAddress(),rr[i].getStartAddress()+rr[i].getLength()));
					}
				}
				
				Object mod = builder.buildModule(dllname, props, sections.iterator(), symbols.iterator(), loadAddress);
				modules.add(mod);
			}
		} catch (IOException e) {
			log.log(Level.FINE, "Problem for Zebedee finding module information", e);			
		}
		return modules;
	}

	/**
	 * @param loadAddress - Long.MAX_VALUE means no address found
	 * @param rr - list of segments
	 * @return the base of the segment if found in a segment, else the load address, else 0
	 */
	private long guessLoadAddress(long loadAddress, AddressRange[] rr) {
		if (loadAddress != Long.MAX_VALUE) {
			// Found, so use the beginning of the segment
			int r = findRange(loadAddress, rr);
			if (r >= 0) {
				loadAddress = rr[r].getStartAddress();
			}
		} else {
			// Not found, so use zero
			loadAddress = 0;
		}
		return loadAddress;
	}

	/**
	 * Sees if the routine is already in an AddressRange,
	 * or if not then returns which AddressRange it is in 
	 * @param routine
	 * @param used array showing if range has already been used
	 * @param rr array of address ranges
	 * @return -1: already has been used, -2: not found, n: index of AddressRange
	 */
	private int findRange(long routine, boolean used[], AddressRange rr[]) {
		int r = findRange(routine, rr);
		if (r >= 0) {
			// Found
			if (used[r]) {
				// Already used
				r = -1;
			} else {
				// Not used, so remember
				used[r] = true;
			}
		}
		return r;
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
				log.fine("Found address "+format(routine)+" at "+format(rr[i].getStartAddress())+":"+format(rr[i].getEndAddress()));
				return i;
			}
		}
		// Not found on the address range
		log.fine("Didn't find address "+format(routine));
		return -2;
	}
	
	/**
	 * Gets the environment variables for the address space
	 * @param build
	 * @param addressSpace Image address space
	 * @param edb the enclave
	 * @return
	 */
	private Properties getEnvironment(Builder build, AddressSpace addressSpace, Edb edb) {
		try {
			log.fine("Get environment for EDB = "+edb);
			Properties p = edb.getEnvVars();
			return p;
		} catch (IOException e) {
			log.log(Level.FINE, "Problem for Zebedee environment", e);						
		}
		return new Properties();
	}
	
	/**
	 * When was the dump created?
	 * @return time in Java long format
	 */
	private long getCreationTime() {
		long tm = _dump.getCreationDate().getTime();
		log.fine("Java time of dump:"+format(tm)+" as date:"+new Date(tm));
		return _dump.getCreationDate().getTime();
	}
	
	/**
	 * Get list of address ranges which match the supplied address space identifier
	 * Also set the asidinfo[1] to 1 if the selected ranges are 64-bit
	 * @param asidinfo asidinfo[0] = asid, asidinfo[1] set to 1 if the address space is 64-bit
	 * @param rangeArray list of core reader address ranges
	 * @return The list of ranges
	 */
	private List keepMemoryRangesWithAsid(int[] asidinfo, List rangeArray) {
		List keep = new ArrayList();
		for (Iterator iter = rangeArray.iterator(); iter.hasNext();) {
			MemoryRange range = (MemoryRange) iter.next();
			boolean range64 = range.getVirtualAddress() + range.getSize() >= 0x100000000L; 
			if (range64) {
				log.finer("Found 64-bit address range "+format(range.getVirtualAddress())+":"+format(range.getSize())+" in address space "+format(range.getAsid()));
			}
			if (range.getAsid() == asidinfo[0]) {
				keep.add(range);
				if (range64) {
					if (asidinfo[1] == 0) {
						log.fine("Found 64-bit address in Java address space");
						asidinfo[1] = 1;
					}
				}
			}
			
			// Add in any memory ranges found in a shared memory ASID. Note: do not adjust 
			// the 31/64 decision for these.
			if (range.getAsid() == SHARED_MEMORY_ASID) {
				// System.out.println("found a shared memory range at " + range.getVirtualAddress());
				MemoryRange rangeCopy = new MemoryRange(range, asidinfo[0]);
				keep.add(rangeCopy);
			}
		}
		return keep;
	}

	private boolean bufferHasJ9RASEyeCatcher(byte[] buf) {
		byte[] j9vmras = { (byte)'J', (byte)'9', (byte)'V', (byte)'M', (byte)'R', (byte)'A', (byte)'S', (byte)'\0' };
		for (int i = 0; i < buf.length; i += 8) {
			boolean found = true;
			for (int j = 0; j < j9vmras.length; j++)
				if (buf[i + j] != j9vmras[j])
					found = false;
			if (found) {
				return true;
			}
		}
		return false;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.ICoreFileReader#getAdditionalFileNames()
	 */
	public Iterator getAdditionalFileNames()
	{
		return _additionalFileNames.iterator();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.ICoreFileReader#extract(com.ibm.dtfj.corereaders.Builder)
	 */
	public void extract(Builder builder)
	{
		builder.setOSType("z/OS");
		builder.setCPUType("s390");
		builder.setCPUSubType("");
		
		log.fine("Address spaces "+_javaAddressSpaces.size());
		for (Iterator i = _javaAddressSpaces.values().iterator(); i.hasNext(); ) {
			int asidinfo[] = (int[])i.next();
			buildAddressSpace(builder, asidinfo[0], asidinfo[1] != 0);
		}
		
		if (_dump != null) {
			builder.setCreationTime(getCreationTime());
		}
	}

	private void buildAddressSpace(Builder builder, int asid, boolean is64Bit) {
		log.fine("Building address space "+format(asid));
		Object addressSpace = builder.buildAddressSpace("z/OS Address Space", asid);
		AddressSpace adrJava;
		// Optimization for JExtract - don't bother to build other information
		if (addressSpace.getClass() == java.lang.Object.class) {
			adrJava = null;
		} else {
			if(stream instanceof ClosingFileReader) {
				initializeZebedeeDump((ClosingFileReader)stream);
			} else {
				initializeZebedeeDump(stream);
			}
			adrJava = findZebedeeAddressSpace(asid);
		}
		Edb edbs[] = adrJava != null ? getEdbs(adrJava) : new Edb[1];
		for (int i = 0; i < edbs.length; ++i) {
			final Edb edb = edbs[i];
			Properties environment = edb == null ? new Properties() : getEnvironment(builder, adrJava, edb);
			Map stackSymbols = new HashMap();
			List threads = edb == null ? Collections.singletonList(builder.buildCorruptData(addressSpace, "unable to extract thread information this time!", 0)) : getThreads(builder, addressSpace, adrJava, edb, stackSymbols);
			List modules = edb == null ? Collections.EMPTY_LIST : getModules(builder, addressSpace, adrJava, edb, stackSymbols);
			Iterator mods = modules.iterator();
			Object executable = mods.hasNext() ? mods.next() : null;
			if (executable == null) {
				builder.setExecutableUnavailable("unable to extract executable information");
				//System.out.println("Bad EDB "+edb);
			}
			String commandLine = environment.getProperty("IBM_JAVA_COMMAND_LINE", "");
			//Use the EDB address for the process id, if available, otherwise use the Address space id.
			String pid = edb != null ? format(edb.address()) : format(asid);
			if (null != _j9rasReader) {
				try {
					pid = format(_j9rasReader.getProcessID());
				} catch (Exception e) {
					//swallow
				}
			}
			
			if (null == _failingThread) {
				_failingThread = adrJava == null ? null : threads.get(0);
			}
			
			//CMVC 156226 - DTFJ exception: XML and core file pointer sizes differ (zOS)
			int addressSize = 31;			//default value for the address size we are dealing with
			if(adrJava != null) {			//use the zebedee address space to determine the address size, however this not available when running jextract
				addressSize = adrJava.getWordLength() == 8 ? 64 : 31;
			} else {						//fall back to the previous version
				addressSize = is64Bit ? 64 : 32;
			}
			
			Object process = builder.buildProcess(addressSpace, pid, commandLine, environment, _failingThread , threads.iterator(), executable, mods, addressSize);
			log.fine("Built process "+process);
		}
	}

	public static boolean isSupportedDump(ImageInputStream stream) throws IOException, CorruptCoreException
	{
		stream.seek(0);
		int signature = stream.readInt();
		if (0x4452 == (0xFFFF & (signature >> 16))) {
			throw new CorruptCoreException("This is a z/OS core file which has been corrupted by EBCDIC to ASCII conversion.  Reverse the conversion and try again.");
		}
		return (signature == DR1 || signature == DR2);
	}

	public static ICoreFileReader dumpFromFile(ImageInputStream stream) throws IOException
	{
		try
		{
			assert isSupportedDump(stream) : "Tried to create a core reader from an unsupported file type";
		}
		catch (CorruptCoreException e)
		{
			assert false : "Tried to create a core reader from a corrupt core";
		}

		return new NewZosDump(stream);
	}

	protected List readTDUMP()
	{
		log.fine("Reading address ranges");
		List ranges = new ArrayList();
		byte[] buf = new byte[RECORD_BODY_LEN];
		try {
			long pos = 0;

			while (true) {
				MemoryRange range = readRecord(pos);
				pos += RECORD_LEN;

				// Ignore address space zero ... is this still necessary?
				if ((null != range) && (0 != range.getAsid()) && (0 != range.getVirtualAddress())) {
					//note that we are probably reading too far into the file since we do find duplicate records later in the file but their contents seem to be incorrect
					ranges.add(range);
					// Search for Java spaces
					int asid = range.getAsid();
					if (!_javaAddressSpaces.containsKey(Integer.valueOf(asid))) {
						stream.seek(range.getFileOffset());				
						stream.readFully(buf);
						if (bufferHasJ9RASEyeCatcher(buf)) {
							_javaAddressSpaces.put(Integer.valueOf(asid), new int[] {asid, 0});
							log.fine("Found Java asid "+format(asid)+" at "+format(range.getVirtualAddress()));
						}
					}
				}
			}
		} catch (IOException ex) {
			// EOF => we are done
		}
		log.fine("Read "+ranges.size()+" address ranges");
		return ranges;
	}

	protected MemoryRange readRecord(long pos) throws IOException
	{
		stream.seek(pos);
		int signature = stream.readInt();
		if (signature != DR1 && signature != DR2) {
			throw new IOException("Unrecognized dump record");
		}
		// Note: we used to ignore records if the 'type' field (2 bytes following the signature) 
		// was not 0xc3e5 (ebcdic 'CV'). We need to process these now to find shared memory ASIDs.

		long address;
		int asid;

		stream.seek(pos + 12);
		asid = stream.readInt();
		stream.seek(pos + 20);
		if (signature == DR2) {
			address = stream.readLong();
		} else {
			address = stream.readUnsignedInt();
		}
		
		// Not sure where the share/read/execute flags are stored, so do not specified any values
		return new MemoryRange(address, pos+RECORD_HEADER_LEN, RECORD_BODY_LEN, asid);
		//return new MemoryRange(address, pos+RECORD_HEADER_LEN, RECORD_BODY_LEN, asid, false,false,true);
	}

	public IAbstractAddressSpace getAddressSpace()
	{
		return _space;
	}
	
	public boolean isTruncated() {
		return false;
	}

	public void releaseResources() throws IOException {
		if(stream instanceof ClosingFileReader) {
			((ClosingFileReader)stream).releaseResources();
		}
	}
}
