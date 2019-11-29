/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.memory;

import java.io.PrintStream;

/**
 * A memory record.  
 * If the memory content is not null, it normally means the memory record is patched;
 * If the memory content is null, it normally means the memory record is not patched, and we need read the memory from the system core file.
 * <p>
 * @author Manqing LI, IBM Canada
 *
 */
public class MemoryRecord {
	
	private long start;
	private int length;
	private byte [] content;

	public MemoryRecord(long start, int length) {
		this.start = start;
		this.length = length;
		this.content = null;
	}
	
	public MemoryRecord(long start, byte [] content) {
		this.start = start;
		this.content = content;
		this.length = content.length;
	}

	public long getStart() {
		return start;
	}

	public byte[] getContent() {
		return content;
	}

	public long getEnd() {
		return start + content.length;
	}
	
	public int getLength() {
		return length;
	}

	public void setStart(long start) {
		this.start = start;
	}

	public void setContent(byte[] content) {
		this.content = content;
		this.length = content.length;
	}
	
	private static final int MAX_PRINT_BYTES = 20;
	public void print(PrintStream out, String prefix) {
		out.print(prefix + "start address : 0x" + Long.toHexString(start) + " : length 0x" + Long.toHexString(length) + " :");
		if (content == null) {
			out.println("<NOT INITIALIZED>");
			return;
		}
		for (int i = 0; i < content.length & i < MAX_PRINT_BYTES; i++) {
			out.print(" " + String.format("%02X",  (int) (content[i] & 0xff)));
		}
		if (content.length > MAX_PRINT_BYTES) {
			out.print(" ......");
		}
		out.println();
	}
}
