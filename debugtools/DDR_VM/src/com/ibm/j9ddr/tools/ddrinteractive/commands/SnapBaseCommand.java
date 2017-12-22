/*******************************************************************************
 * Copyright (c) 2013, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.IOException;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.nio.ByteOrder;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.StructureCommandUtil;

/**
 * Debug extension superclass to extract trace buffers from a core dump
 * so they can be dumped or formatted.
 * 
 * @author Howard Hellyer
 *
 */
public abstract class SnapBaseCommand extends Command {	private String fileName = null;
	
	protected abstract void writeHeaderBytesToTrace(Context context, byte[] headerBytes, PrintStream out);
	
	protected abstract void writeBytesToTrace(Context context, long address, int bufferSize, PrintStream out);

	/**
	 * Used by subclasses to actually walk the trace data in the core.
	 * Calls writeHeaderBytesToTrace and writeBytesToTrace to actually process the
	 * trace data.
	 * @param context
	 * @param out
	 * @throws DDRInteractiveCommandException
	 */
	protected void extractTraceData(Context context, PrintStream out)
			throws DDRInteractiveCommandException {
		long vmAddress = context.vmAddress;
		try {
			
			// Path to trace header is:
			// vm->j9rasglobalstorage->utglobaldata->traceheader
			long j9rasglobalstorageAddress = CommandUtils.followPointerFromStructure(context, "J9JavaVM", vmAddress, "j9rasGlobalStorage");
			long utglobaldataAddress = CommandUtils.followPointerFromStructure(context, "RasGlobalStorage", j9rasglobalstorageAddress, "utGlobalData");
			if (utglobaldataAddress == 0) {
				out.println("Encountered null J9JavaVM->j9rasGlobalStorage->utGlobalData pointer. Trace was not initialized, check dump VM options for -Xtrace:none");
				return;
			}
			long traceheaderAddress = CommandUtils.followPointerFromStructure(context, "UtGlobalData", utglobaldataAddress, "traceHeader");
			long bufferSizeOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtGlobalData", context), "bufferSize");
			int bufferSize = context.process.getIntAt(utglobaldataAddress + bufferSizeOffset);

			if( traceheaderAddress != 0 ) {
				long headerOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtTraceFileHdr", context), "header");
				long lengthOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtDataHeader", context), "length");
				int headerLength = context.process.getIntAt(traceheaderAddress + headerOffset + lengthOffset);
				out.println("traceHeader: !uttracefilehdr 0x"+(Long.toHexString(traceheaderAddress)));
				out.println("traceHeader: length     = "+ headerLength);
				byte[] headerBytes = new byte[headerLength];
				context.process.getBytesAt(traceheaderAddress, headerBytes);
				writeHeaderBytesToTrace(context, headerBytes, out);
			} else {
				// Un-comment if you need to check whether we are finding the header in the dump or creating it here.
				// out.println("TraceFileHeader missing. Creating header from dump, some header information may be missing.");
				createAndWriteTraceFileHeader(context, utglobaldataAddress, bufferSize, out);
			}
			long nextTraceBufferAddress = CommandUtils.followPointerFromStructure(context, "UtGlobalData", utglobaldataAddress, "traceGlobal");
			out.println("Buffer size = "+ bufferSize);
			out.println("firstBuffer: !uttracebuffer 0x" + Long.toHexString(nextTraceBufferAddress));
			int count = 0;
			long recordOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtTraceBuffer", context), "record");
			while( nextTraceBufferAddress != 0) {
				count++;
				writeBytesToTrace(context, nextTraceBufferAddress + recordOffset, bufferSize, out);
				nextTraceBufferAddress = CommandUtils.followPointerFromStructure(context, "UtTraceBuffer", nextTraceBufferAddress, "globalNext");
				if( count < 10 && nextTraceBufferAddress != 0 ) {
					out.println("nextBuffer:  !uttracebuffer 0x" + Long.toHexString(nextTraceBufferAddress));
				} else if ( count == 10 && nextTraceBufferAddress != 0) {
					// Avoid printing out thousands of lines if there are lots of buffers.
					// (This is likely with Health Center monitoring WAS.)
					out.println("...");
				} 
			}
			if( count > 0 ) {
				out.println((count) + " trace buffers extracted from the dump.");
			} else {
				out.println("There were no trace buffers found in the dump");
			}
		} catch (MemoryFault e) {
			e.printStackTrace(out);
		} catch (com.ibm.j9ddr.NoSuchFieldException e){
			e.printStackTrace(out);				
		} catch (CorruptDataException e) {
			e.printStackTrace(out);
		}
	}

	private void createAndWriteTraceFileHeader(Context context,
			long utglobaldataAddress, int bufferSize, PrintStream out)
			throws CorruptDataException, DDRInteractiveCommandException {
		IProcess process = context.process;
		
		boolean isBigEndian = process.getByteOrder().equals(ByteOrder.BIG_ENDIAN);
		
		long j9rasAddress = CommandUtils.followPointerFromStructure(context, "J9JavaVM", context.vmAddress, "j9ras");
		
		long cpuOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("J9RAS", context), "cpus");
		int cpuCount = context.process.getIntAt(j9rasAddress + cpuOffset);
		
		long archOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("J9RAS", context), "osarch");
		String arch = getCStringAtAddress(process, j9rasAddress + archOffset, 16);

		String processorSubType = ""; // We can't actually work this out.
		
		long serviceInfoOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtGlobalData", context), "serviceInfo");
		long serviceInfoAddress = process.getPointerAt(utglobaldataAddress + serviceInfoOffset);
		String serviceLevel = getCStringAtAddress(process, serviceInfoAddress);

		long propertiesOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtGlobalData", context), "properties");
		long propertiesAddress = process.getPointerAt(utglobaldataAddress + propertiesOffset);
		String startupOptions = getCStringAtAddress(process, propertiesAddress);
		
		int wordSize = process.bytesPerPointer()*8;
		
		// The trace configuration options are in a list.
		String traceConfig = "";
		long commandOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtTraceCfg", context), "command");
		long nextConfigAddress = CommandUtils.followPointerFromStructure(context, "UtGlobalData", utglobaldataAddress, "config");
		while( nextConfigAddress != 0 ) {
			traceConfig += getCStringAtAddress(process, nextConfigAddress + commandOffset);
			nextConfigAddress = CommandUtils.followPointerFromStructure(context, "UtTraceCfg", nextConfigAddress, "next");
			if( nextConfigAddress != 0 ) {
				traceConfig += "\n";
			}
		}
		
		long startPlatformOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtGlobalData", context), "startPlatform");
		long startPlatform = context.process.getLongAt(utglobaldataAddress + startPlatformOffset);
		
		long startSystemOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtGlobalData", context), "startSystem");
		long startSystem = context.process.getLongAt(utglobaldataAddress + startSystemOffset);
		
		long externalTraceOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtGlobalData", context), "externalTrace");
		int type = context.process.getIntAt(utglobaldataAddress + externalTraceOffset);

		long traceGenerationsOffset = CommandUtils.getOffsetForField(StructureCommandUtil.getStructureDescriptor("UtGlobalData", context), "traceGenerations");
		int generations = context.process.getIntAt(utglobaldataAddress + traceGenerationsOffset);
			
		TraceFileHeaderWriter headerWriter;
		try {
			headerWriter = new TraceFileHeaderWriter(fileName,
					isBigEndian, cpuCount, wordSize, bufferSize, arch, processorSubType,
					serviceLevel, startupOptions, traceConfig, startPlatform, startSystem, type, generations);
			byte[] headerBytes = headerWriter.createAndWriteTraceFileHeader();
			writeHeaderBytesToTrace(context, headerBytes, out);
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

	public String getCStringAtAddress(IProcess process, long address) throws CorruptDataException {
		return getCStringAtAddress(process, address, Long.MAX_VALUE);
	}
	
	public String getCStringAtAddress(IProcess process, long address, long maxLength) throws CorruptDataException {
		int length = 0;
		while(0 != process.getByteAt(address + length) && length < maxLength) {
			length++;
		}
		byte[] buffer = new byte[length];
		process.getBytesAt(address, buffer);
		
		try {
			return new String(buffer,"UTF-8");
		} catch (UnsupportedEncodingException e) {
			throw new RuntimeException(e);
		}
	}
}
