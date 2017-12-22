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
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.TimeZone;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;

/*
 * Reads MINIDUMP_THREAD_INFO structures from the MINIDUMP_THREAD_INFO_LIST 
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms680506%28v=vs.85%29.aspx
 * http://msdn.microsoft.com/en-us/library/windows/desktop/ms680510%28v=vs.85%29.aspx
 */
public class ThreadInfoStream extends Stream {

	private final long baseMillisOffset;
	private final DateFormat dateFormat = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss");
	
	protected ThreadInfoStream(int dataSize, long location) {
		super(dataSize, location);
		Calendar baseTime = new GregorianCalendar(TimeZone.getTimeZone("UTC"));
		baseTime.clear();
		baseTime.set(1601, 0, 1, 0, 0, 0);
		baseMillisOffset = baseTime.getTimeInMillis();
	}

	public void readFrom(MiniDumpReader dump, IAddressSpace addressSpace,
			boolean is64Bit, List<IOSThread> threads) throws CorruptDataException, IOException {
		
		dump.seek(getLocation());
		
		int sizeOfHeader = 0;
		int sizeOfEntry = 0;
		int numberOfEntries = 0;
		
		HashMap<Long, Properties> threadInfo = new HashMap<Long, Properties>();
		
		// The header is 3 ULONGs, which are 32 bit.
		sizeOfHeader = dump.readInt();
		sizeOfEntry = dump.readInt();
		numberOfEntries = dump.readInt();
			
		// The thread info structures follow the header.
		/* Each structure is:
		 *  ULONG32 ThreadId;
  			ULONG32 DumpFlags;
  			ULONG32 DumpError;
  			ULONG32 ExitStatus;
  			ULONG64 CreateTime;
  			ULONG64 ExitTime;
  			ULONG64 KernelTime;
  			ULONG64 UserTime;
  			ULONG64 StartAddress;
  			ULONG64 Affinity;
		 */
		// We should be exactly here already but to be safe.
		dump.seek(getLocation()+sizeOfHeader);
		
		for( int i = 0; i < numberOfEntries; i++) {
			Properties props = new Properties();
			int threadId = dump.readInt();
			int dumpFlags = dump.readInt();
			int dumpError = dump.readInt();
			int exitStatus = dump.readInt();
			// Times stamps are in 100 nano second intervals since January 1, 1601 (UTC).
			long createTime = dump.readLong();
			long exitTime = dump.readLong();
			
			// Durations (also in 100 nano second intervals).
			long kernelTime = dump.readLong();
			long userTime = dump.readLong();
			long startAddress = dump.readLong();
			long affinity = dump.readLong();
			
			props.put("DumpFlags", String.format("0x%08X",dumpFlags));
			props.put("DumpError", String.format("0x%08X",dumpError));
			props.put("ExitStatus", String.format("0x%08X",exitStatus));
			props.put("CreateTime", Long.toString(createTime) );
			props.put("ExitTime", Long.toString(exitTime) );
			props.put("KernelTime", Long.toString(kernelTime) );
			props.put("UserTime", Long.toString(userTime) );
			props.put("StartAddress", String.format(is64Bit?"0x%016X":"0x%08X", startAddress) );
			// CPU affinity mask, enabled bits show enabled CPUs.
			props.put("Affinity", String.format("0x%016X", affinity));
			
			long createTimeMillis = (createTime/10000l) + baseMillisOffset;
			String createTimeStr = dateFormat.format(new Date(createTimeMillis));
			props.put("CreateTime_Formatted", createTimeStr);

			if( exitTime != 0 ) {
				long exitTimeMillis = (exitTime/10000l) + baseMillisOffset;
				String exitTimeStr = dateFormat.format(new Date(exitTimeMillis));
				props.put("ExitTime_Formatted", exitTimeStr);
			}
			
			threadInfo.put(new Long(threadId), props);
		}
		
		/* Merge the extra properties into the thread info. */
		Iterator<IOSThread> threadIterator = threads.iterator();
		while( threadIterator.hasNext() ) {
			IOSThread t = threadIterator.next();
			Properties p = threadInfo.get(t.getThreadId() ); 
			if( p != null ) {
				t.getProperties().putAll(p);
			}
		}
	}
	
}
