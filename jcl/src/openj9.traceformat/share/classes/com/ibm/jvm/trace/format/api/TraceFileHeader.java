/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
package com.ibm.jvm.trace.format.api;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * Encapsulates the header for a trace file.
 * 
 * @author Tim Preece
 */
final public class TraceFileHeader {
	TraceContext context;
	String textSummary;

	int recordSize;
	int endianSignature;
	ByteOrder byteOrder;
	boolean ascii = true;

	protected DataHeader dataHeader;
	protected TraceSection traceSection;
	protected ServiceSection serviceSection;
	protected StartupSection startupSection;
	protected ActiveSection activeSection;
	protected ProcessorSection processorSection;

	private static final int BIG_ENDIAN_SIG = 0x12345678;
	private static final int LITTLE_ENDIAN_SIG = 0x78563412;

	private static final int eyecatcherASCII = 0x55545448;
	private static final int eyecatcherEBCDIC = 0xe4e3e3c8;

	/*
	 * internal constructor for testing
	 */
	TraceFileHeader(int recordSize, ByteOrder byteOrder) {
		this.recordSize = recordSize;
		this.byteOrder = byteOrder;
	}

	protected TraceFileHeader(TraceContext context, ByteBuffer data) throws IllegalArgumentException {
		int traceFileHeaderStart = data.position();

		/*
		 * we set ourselves in context so that context can be used in
		 * the constructor
		 */
		context.metadata = this;

		/*
		 * hop over the header and buffer size and set up the
		 * endian-ness. This is the only check we have for a valid trace
		 * file header given we can't depend on the eyecatcher yet
		 */
		try {
			endianSignature = data.getInt(traceFileHeaderStart + DataHeader.DATAHEADER_SIZE + 4);
			switch (endianSignature) {
			case BIG_ENDIAN_SIG:
				byteOrder = ByteOrder.BIG_ENDIAN;
				break;
			case LITTLE_ENDIAN_SIG:
				byteOrder = ByteOrder.LITTLE_ENDIAN;
				break;
			default:
				throw new IllegalArgumentException("Invalid endian signature in trace file header: " + Long.toString(endianSignature, 16));
			}
	
			context.debug(this, 1, "Data is " + byteOrder.toString());
	
			/* set up the byte order for the rest of our accesses */
			data.order(byteOrder);
	
			/* set up the basic header for this structure */
			dataHeader = new DataHeader(context, data, "UTTH");
	
			/*
			 * Check the eyecatcher to make sure this is a valid trace file.
			 * Written as characters so no need to worry about endian-ness
			 */
			if (dataHeader.ascii) {
				context.debug(this, 1, "Eyecatchers are in ASCII");
				ascii = true;
			} else  {
				context.debug(this, 1, "Eyecatchers are in EBCDIC");
				ascii = false;
			}
	
			recordSize = data.getInt();
			endianSignature = data.getInt();
	
			/*
			 * get the offsets for the various sections within our data
			 * buffer
			 */
			int traceStart = data.getInt();
			int serviceStart = data.getInt();
			int startupStart = data.getInt();
			int activeStart = data.getInt();
			int processorStart = data.getInt();
	
			if (context.debugStream != null) {
				context.debug(this, 1, summary());
			}
	
			/* some of the other sections format time stamps which depends on the processor type so we read this first */
			processorSection = new ProcessorSection(context, (ByteBuffer) data.position(processorStart + traceFileHeaderStart));
			traceSection = new TraceSection(context, (ByteBuffer) data.position(traceStart + traceFileHeaderStart));
			serviceSection = new ServiceSection(context, (ByteBuffer) data.position(serviceStart + traceFileHeaderStart));
			startupSection = new StartupSection(context, (ByteBuffer) data.position(startupStart + traceFileHeaderStart));
			activeSection = new ActiveSection(context, (ByteBuffer) data.position(activeStart + traceFileHeaderStart));
	
			/* now set up the context variables for other people to use */
			context.version = (float) dataHeader.version + (((float) dataHeader.modification) / 10);
		} catch (IndexOutOfBoundsException e) {
			throw new IllegalArgumentException("Truncated trace file header"); 
		}
	}

	public String toString() {
		return "Trace file header";
	}
	
	public String summary() {
		if (textSummary == null) {
			StringBuilder s = new StringBuilder(toString()+":"+System.getProperty("line.separator"));

			s.append("recordSize:     ").append(recordSize).append(System.getProperty("line.separator"));
			s.append("endianSignature: 0x").append(Long.toString(endianSignature, 16)).append(System.getProperty("line.separator"));

			textSummary = s.toString();
		}
		
		return textSummary;
	}
}
