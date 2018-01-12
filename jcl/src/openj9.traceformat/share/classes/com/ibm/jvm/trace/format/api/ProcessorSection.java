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

import java.io.BufferedWriter;
import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * Processor section of a file header
 * 
 * @author Tim Preece
 */
public class ProcessorSection {
	final static String[] timerDesc = { "Sequence number    ", "Special            ", "Pentium TSC        ", "Time (UTC)         ", "MSPR               ", "MFTB               ", "Time (UTC)         ", "J9 timer(UTC)      " };

	protected static final int BYTE = 1; // length of a byte in bytes
	protected static final int INT = 4; // length of an int in bytes
	protected static final int LONG = 8; // length of a long in bytes
	protected static final String SUM_TAB = "        ";

	private final static String[] Archs = { "Unknown", "x86", "S390", "Power", "IA64", "S390X", "AMD64" };
	private final static String[] SubTypes = { "i486", "i586", "Pentium II", "Pentium III", "Merced", "McKinley", "PowerRS", "PowerPC", "GigaProcessor", "ESA", "Pentium IV", "T-Rex", "Opteron" };
	private final static String[] trCounter = { "Sequence Counter", "Special", "RDTSC Timer", "AIX Timer", "MFSPR Timer", "MFTB Timer", "STCK Timer", "J9 timer" };
	
	TraceContext context;

	int arch;
	boolean big;
	int word;
	int procs;
	int subType;
	int counter;
	
	String textSummary;

	public ProcessorSection(TraceContext context, ByteBuffer data) throws IllegalArgumentException {
		this.context = context;
		DataHeader dataHeader = new DataHeader(context, data, "UTPR");

		data.position(data.position() + 16); // Skip over HPI header information
		arch = data.getInt();
		big = (data.getInt() != 0);
		word = data.getInt();
		procs = data.getInt();
		data.position(data.position() + 16);	// Skip over HPI header information
		subType = data.getInt();
		counter = data.getInt();


		if (context.debugStream != null) {
			context.debug(this, 1, summary());
		}
	}

	public String toString() {
		return "Processor information";
	}
	
	public String summary() {
		if (textSummary == null) {
			StringBuilder s = new StringBuilder(toString()+":"+System.getProperty("line.separator"));
			s.append("        Arch family:         ").append(Archs[arch]).append(System.getProperty("line.separator"));
			s.append("        Processor Sub-type:  ").append(SubTypes[subType]).append(System.getProperty("line.separator"));
			s.append("        Num Processors:      ").append(procs).append(System.getProperty("line.separator"));
			s.append("        Big Endian:          ").append(big).append(System.getProperty("line.separator"));
			s.append("        Word size:           ").append(word).append(System.getProperty("line.separator"));
			s.append("        Using Trace Counter: ").append(trCounter[counter]).append(System.getProperty("line.separator"));

			textSummary = s.toString();
		}
		
		return textSummary;
	}
}
