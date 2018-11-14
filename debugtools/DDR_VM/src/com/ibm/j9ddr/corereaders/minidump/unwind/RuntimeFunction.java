/*******************************************************************************
 * Copyright (c) 2012, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.minidump.unwind;

public class RuntimeFunction implements Comparable<RuntimeFunction> {

	private long startOffset, endOffset, unwindInfo;

	public RuntimeFunction(long startOffset, long endOffset, long unwindInfo) {
		this.startOffset = startOffset;
		this.endOffset = endOffset;
		this.unwindInfo = unwindInfo;
	}

	public String toString() {
		return String.format("start: 0x%08X end: 0x%08X UnwindInfoAddress 0x%08X", startOffset, endOffset, unwindInfo);
	}

	public long getUnwindInfoAddress() {
		return unwindInfo;
	}

	public long getStartOffset() {
		return this.startOffset;
	}

	public boolean contains(long address) {
		// System.err.println(String.format("Checking 0x%08x <= 0x%08x < 0x%08x", startOffset, address, endOffset ));
		if (address >= startOffset && address < endOffset) {
			return true;
		}
		return false;
	}

	/*
	 * We assume that two areas don't overlap.
	 */
	@Override
	public int compareTo(RuntimeFunction rf) {
		return Long.compare(this.startOffset, rf.startOffset);
	}

}
