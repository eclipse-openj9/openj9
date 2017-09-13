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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.jvm.trace.format.api;

import java.math.BigInteger;
import java.nio.ByteBuffer;

/**
 * Trace section of a file header.
 * 
 * @author Tim Preece
 */
public class TraceSection {

	BigInteger startPlatform;
	BigInteger startSystem;
	int type;
	int generations;
	int pointerSize = 4;

	TraceContext context;
	String textSummary;

	public TraceSection(TraceContext context, ByteBuffer data) throws IllegalArgumentException {
		this.context = context;
		DataHeader dataHeader = new DataHeader(context, data, "UTTS");

		byte bytes[] = new byte[16];
		data.get(bytes);
		ByteStream stream = context.createByteStream(bytes);
		startPlatform = stream.getBigInteger(8);
		startSystem = stream.getBigInteger(8);
		type = data.getInt();
		generations = data.getInt();
		pointerSize = data.getInt();

		if (context.debugStream != null) {
			context.debug(this, 1, summary());
		}
	}
	
	public String toString() {
		return "Trace file header";
	}
	
	public String summary() {
		if (textSummary == null) {
			StringBuilder s = new StringBuilder(toString()+":"+System.getProperty("line.separator"));

			s.append("        JVM start time: " + context.getFormattedTime(startPlatform)).append(System.getProperty("line.separator"));
			/* note: 'startSystem' (high res clock) and 'type' (internal/external trace flag) are of no interest to users, so omit them */
			s.append("        Generations:    " + generations).append(System.getProperty("line.separator"));
			s.append("        Pointer size:   " + pointerSize).append(System.getProperty("line.separator"));

			textSummary = s.toString();
		}

		return textSummary;
	}
}
