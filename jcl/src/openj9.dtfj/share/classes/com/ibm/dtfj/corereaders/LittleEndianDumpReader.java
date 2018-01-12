/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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

import java.io.IOException;

import javax.imageio.stream.ImageInputStream;


public class LittleEndianDumpReader extends DumpReader {
	public LittleEndianDumpReader(ImageInputStream stream, boolean is64Bit)
	{
		super(stream, is64Bit);
	}

	public short readShort() throws IOException {
		return byteSwap(super.readShort());
	}

	public int readInt() throws IOException {
		return byteSwap(super.readInt());
	}

	public long readLong() throws IOException {
		return byteSwap(super.readLong());
	}

	private short byteSwap(short s) {
		return (short)(((s >> 8) & 0x00ff) | ((s << 8) & 0xff00));
	}

	private int byteSwap(int i) {
		return
			((i >> 24) & 0x000000ff) |
			((i >>  8) & 0x0000ff00) |
			((i <<  8) & 0x00ff0000) |
			((i << 24) & 0xff000000);
	}

	private long byteSwap(long l) {
		return
			((l >> 56) & 0x00000000000000ffL) |
			((l >> 40) & 0x000000000000ff00L) |
			((l >> 24) & 0x0000000000ff0000L) |
			((l >>  8) & 0x00000000ff000000L) |
			((l <<  8) & 0x000000ff00000000L) |
			((l << 24) & 0x0000ff0000000000L) |
			((l << 40) & 0x00ff000000000000L) |
			((l << 56) & 0xff00000000000000L);
	}
}
