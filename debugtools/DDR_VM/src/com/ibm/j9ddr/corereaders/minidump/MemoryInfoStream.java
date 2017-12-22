/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.minidump;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map.Entry;
import java.util.Properties;

import com.ibm.j9ddr.corereaders.memory.DetailedDumpMemorySource;
import com.ibm.j9ddr.corereaders.memory.DumpMemorySource;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.UnbackedMemorySource;
import static com.ibm.j9ddr.corereaders.memory.IDetailedMemoryRange.READABLE;
import static com.ibm.j9ddr.corereaders.memory.IDetailedMemoryRange.WRITABLE;
import static com.ibm.j9ddr.corereaders.memory.IDetailedMemoryRange.EXECUTABLE;

/*
 * Reads MINIDUMP_MEMORY_INFO  structures from the MINIDUMP_MEMORY_INFO_LIST 
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms680385(v=vs.85).aspx
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms680386(v=vs.85).aspx
 */
public class MemoryInfoStream extends Stream {

	private static final int STATE_MEM_COMMIT = 0x1000;
	private static final int STATE_MEM_FREE = 0x10000;
	private static final int STATE_MEM_RESERVE = 0x2000;
	
	private static final int TYPE_MEM_IMAGE = 0x1000000;
	private static final int TYPE_MEM_MAPPED = 0x40000;
	private static final int TYPE_MEM_PRIVATE = 0x20000;
	
	private static final int PROTECT_PAGE_NOACCESS			= 0x1;
	private static final int PROTECT_PAGE_READONLY 			= 0x2;
	private static final int PROTECT_PAGE_READWRITE 		= 0x4;
	private static final int PROTECT_PAGE_WRITECOPY 		= 0x8;
	private static final int PROTECT_PAGE_EXECUTE 			= 0x10;
	private static final int PROTECT_PAGE_EXECUTE_READ 		= 0x20;
	private static final int PROTECT_PAGE_EXECUTE_READWRITE = 0x40;
	private static final int PROTECT_PAGE_EXECUTE_WRITECOPY = 0x80;
	// Protection modifiers
	private static final int PROTECT_PAGE_GUARD 			= 0x100;
	private static final int PROTECT_PAGE_NOCACHE 			= 0x200;
	private static final int PROTECT_PAGE_WRITECOMBINE 		= 0x400;
	
	private final String TRUE = Boolean.TRUE.toString();
	private final String FALSE = Boolean.FALSE.toString();
	
	protected MemoryInfoStream(int dataSize, long location) {
		super(dataSize, location);
	}

	public void readFrom(MiniDumpReader dump, boolean is64Bit, Collection<? extends IMemorySource> ranges) throws IOException {
		
		dump.seek(getLocation());
		int sizeOfHeader = 0;
		int sizeOfEntry = 0;
		long numberOfEntries = 0;
		
		HashMap<Long, Properties> memoryInfo = new HashMap<Long, Properties>();
		
		// The header is 2 ULONGs, which are 32 bit and a ULONG64
		sizeOfHeader = dump.readInt();
		sizeOfEntry = dump.readInt();
		numberOfEntries = dump.readLong();
		
		// The thread info structures follow the header.
		/* Each structure is:
			ULONG64 BaseAddress;
			ULONG64 AllocationBase;
			ULONG32 AllocationProtect;
			ULONG32 __alignment1;
			ULONG64 RegionSize;
			ULONG32 State;
			ULONG32 Protect;
			ULONG32 Type;
			ULONG32 __alignment2;
		 */
		
		// We should be exactly here already but to be safe.
		dump.seek(getLocation()+sizeOfHeader);
		
		for( int i = 0; i < numberOfEntries; i++) {
			Properties props = new Properties();
			long baseAddress = dump.readLong();
			long allocationBase = dump.readLong();
			int allocationProtect = dump.readInt();
			dump.readInt(); // alignment1
			long regionSize = dump.readLong();

			int state = dump.readInt();
			int protect = dump.readInt();
			int type = dump.readInt();
			dump.readInt(); // alignment2
			
			props.put("state", decodeStateFlags(state));
			props.put("state_flags", String.format("0x%X", state));
			
			props.put("size", String.format(is64Bit?"0x%016X":"0x%08X", regionSize));
			
			if( state != STATE_MEM_FREE ) {
				// For free memory allocation base, allocation protect, protect and type are undefined.
				props.put("allocationBase", String.format(is64Bit?"0x%016X":"0x%08X", allocationBase));
				
				props.put("allocationProtect", decodeProtectFlags(allocationProtect));
				props.put("allocationProtect_flags", String.format("0x%X", allocationProtect));
				
				props.put("protect", decodeProtectFlags(protect));
				setReadWriteExecProperties(protect, props);
				props.put("protect_flags", String.format("0x%X", protect));

				props.put("type", decodeTypeFlags(type));
				props.put("type_flags", String.format("0x%X", allocationProtect));

			} else {
				// Free memory is not accessible.
				props.setProperty(READABLE, FALSE);
				props.setProperty(WRITABLE, FALSE);
				props.setProperty(EXECUTABLE, FALSE);
			}
			
			memoryInfo.put(new Long(baseAddress), props);
		}
		
		/* Merge the extra properties into the memory info. */
		
		Iterator<? extends IMemorySource> memoryIterator = ranges.iterator();
		List<IMemorySource> newRanges = new ArrayList<IMemorySource>(memoryInfo.size());
		while( memoryIterator.hasNext() ) {
			IMemorySource m = memoryIterator.next();
			Properties p = memoryInfo.remove(m.getBaseAddress() ); 
			if( p != null && m instanceof DumpMemorySource) {
				DetailedDumpMemorySource newSource = new DetailedDumpMemorySource((DumpMemorySource)m, p);
				newSource.getProperties().putAll(p);
				newRanges.add(newSource);
			} else {
				newRanges.add(m);
			}
		}
		// Add sections we didn't find as unbacked memory!
		// (There should be at least one huge one in the middle if this is 64 bit.)
		Iterator<Entry<Long, Properties>> propsIterator = memoryInfo.entrySet().iterator();
		while( propsIterator.hasNext()) {
			Entry<Long, Properties> e = propsIterator.next();
			long size = Long.parseLong(((String)e.getValue().get("size")).substring(2), 16);
			String explanation = "Windows MINIDUMP_MEMORY_INFO entry included but data not included in dump";
			if( "MEM_FREE".equals(e.getValue().get("state")) ) {
				explanation = "Free memory, unallocated";
			}
			UnbackedMemorySource m = new UnbackedMemorySource(e.getKey(), size, explanation);
			m.getProperties().putAll(e.getValue());
			newRanges.add(m);
		}
		
		Collections.sort(newRanges, new Comparator<IMemorySource>() {

			public int compare(IMemorySource o1, IMemorySource o2) {
				if( o1.getBaseAddress() > o2.getBaseAddress() ) {
					return 1;
				} else if( o1.getBaseAddress() < o2.getBaseAddress() ) {
					return -1;
				}
				return 0;
			}
		});
		dump.setMemorySources(newRanges);
	}

	// According to http://msdn.microsoft.com/en-us/library/windows/desktop/ms680386(v=vs.85).aspx
	// only one of the values below will be set.
	private void setReadWriteExecProperties(int protect, Properties props) {

		if( (protect & PROTECT_PAGE_NOACCESS) != 0 ) {
			props.setProperty(READABLE, FALSE);
			props.setProperty(WRITABLE, FALSE);
			props.setProperty(EXECUTABLE, FALSE);
		} else if( (protect & PROTECT_PAGE_READONLY) != 0 ) {
			props.setProperty(READABLE, TRUE);
			props.setProperty(WRITABLE, FALSE);
			props.setProperty(EXECUTABLE, FALSE);
		} else if( (protect & PROTECT_PAGE_READWRITE) != 0 ) {
			props.setProperty(READABLE, TRUE);
			props.setProperty(WRITABLE, TRUE);
			props.setProperty(EXECUTABLE, FALSE);
		} else if( (protect & PROTECT_PAGE_WRITECOPY) != 0 ) {
			props.setProperty(READABLE, TRUE);
			props.setProperty(WRITABLE, TRUE);
			props.setProperty(EXECUTABLE, FALSE);
		} else if( (protect & PROTECT_PAGE_EXECUTE) != 0 ) {
			props.setProperty(READABLE, FALSE);
			props.setProperty(WRITABLE, FALSE);
			props.setProperty(EXECUTABLE, TRUE);
		} else if( (protect & PROTECT_PAGE_EXECUTE_READ) != 0 ) {
			props.setProperty(READABLE, TRUE);
			props.setProperty(WRITABLE, FALSE);
			props.setProperty(EXECUTABLE, TRUE);
		} else if( (protect & PROTECT_PAGE_EXECUTE_READWRITE) != 0 ) {
			props.setProperty(READABLE, TRUE);
			props.setProperty(WRITABLE, TRUE);
			props.setProperty(EXECUTABLE, TRUE);
		} else if( (protect & PROTECT_PAGE_EXECUTE_WRITECOPY) != 0 ) {
			props.setProperty(READABLE, TRUE);
			props.setProperty(WRITABLE, TRUE);
			props.setProperty(EXECUTABLE, TRUE);
		}		
	}
	
	private String decodeProtectFlags(int protect) {
		String flagString = "";
		if( (protect & PROTECT_PAGE_NOACCESS) != 0 ) {
			flagString += "PAGE_NOACCESS|";
		}
		if( (protect & PROTECT_PAGE_READONLY) != 0 ) {
			flagString += "PAGE_READONLY|";
		}
		if( (protect & PROTECT_PAGE_READWRITE) != 0 ) {
			flagString += "PAGE_READWRITE|";
		}
		if( (protect & PROTECT_PAGE_WRITECOPY) != 0 ) {
			flagString += "PAGE_WRITECOPY|";
		}
		if( (protect & PROTECT_PAGE_EXECUTE) != 0 ) {
			flagString += "PAGE_EXECUTE|";
		}
		if( (protect & PROTECT_PAGE_EXECUTE_READ) != 0 ) {
			flagString += "PAGE_EXECUTE_READ|";
		}
		if( (protect & PROTECT_PAGE_EXECUTE_READWRITE) != 0 ) {
			flagString += "PAGE_EXECUTE_READWRITE|";
		}
		if( (protect & PROTECT_PAGE_EXECUTE_WRITECOPY) != 0 ) {
			flagString += "PAGE_EXECUTE_WRITECOPY|";
		}
		// Modifiers, mutually exclusive so no need for another |
		if( (protect & PROTECT_PAGE_GUARD) != 0 ) {
			flagString += "PAGE_GUARD";
		}
		if( (protect & PROTECT_PAGE_NOCACHE) != 0 ) {
			flagString += "PAGE_NOCACHE";
		}
		if( (protect & PROTECT_PAGE_WRITECOMBINE) != 0 ) {
			flagString += "PAGE_WRITECOMBINE";
		}

		if( flagString.endsWith("|") ) {
			flagString = flagString.substring(0, flagString.length() - 1);
		}
		return flagString;
		
	}

	private String decodeStateFlags(int state) {
		switch(state) {
		case STATE_MEM_COMMIT:
			return "MEM_COMMIT";
		case STATE_MEM_FREE:
			return "MEM_FREE";
		case STATE_MEM_RESERVE:
			return "MEM_RESERVE";
		default:
			return "UNKNOWN";
		}
	}
	
	private String decodeTypeFlags(int type) {
		switch(type) {
		case TYPE_MEM_IMAGE:
			return "MEM_IMAGE";
		case TYPE_MEM_MAPPED:
			return "MEM_MAPPED";
		case TYPE_MEM_PRIVATE:
			return "MEM_PRIVATE";
		default:
			return "UNKNOWN";
		}
	}
}
