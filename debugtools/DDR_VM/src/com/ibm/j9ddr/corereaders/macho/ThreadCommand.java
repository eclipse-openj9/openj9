/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
/*******************************************************************************
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc. All Rights
 * Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *******************************************************************************/
package com.ibm.j9ddr.corereaders.macho;

import java.io.IOException;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

import javax.imageio.stream.ImageInputStream;

public class ThreadCommand extends LoadCommand
{

	// thread flavors
	public static final int x86_THREAD_STATE32 = 1;
	public static final int x86_FLOAT_STATE32 = 2;
	public static final int x86_EXCEPTION_STATE32 = 3;
	public static final int x86_THREAD_STATE64 = 4;
	public static final int x86_FLOAT_STATE64 = 5;
	public static final int x86_EXCEPTION_STATE64 = 6;
	public static final int x86_THREAD_STATE = 7;
	public static final int x86_FLOAT_STATE = 8;
	public static final int x86_EXCEPTION_STATE = 9;
	public static final int x86_DEBUG_STATE32 = 10;
	public static final int x86_DEBUG_STATE64 = 11;
	public static final int x86_DEBUG_STATE = 12;
	public static final int THREAD_STATE_NONE = 13;
	public static final int x86_AVX_STATE32 = 16;
	public static final int x86_AVX_STATE64 = (x86_AVX_STATE32 + 1);
	public static final int x86_AVX_STATE = (x86_AVX_STATE32 + 2);
	public static final int x86_AVX512_STATE32 = 19;
	public static final int x86_AVX512_STATE64 = (x86_AVX512_STATE32 + 1);
	public static final int x86_AVX512_STATE =(x86_AVX512_STATE32 + 2);

	public static class ThreadState
	{
		int flavor;
		int sizeInUInts;
		byte stateBytes[];
		ByteOrder endianness;
		Map<String, Number> registers;

		public ThreadState(int flavor, int size, byte state[], ByteOrder endian)
		{
			this.flavor = flavor;
			this.sizeInUInts = size;
			this.stateBytes = state;
			this.endianness = endian;
			registers = new TreeMap<>();
			switch (flavor) {
				case x86_THREAD_STATE:
					fillX86ThreadRegisters();
					break;
				case x86_EXCEPTION_STATE:
					fillX86ExceptionData();
					break;
				default:
					break;
			}
		}

		public long readLong(int start)
		{
			long ret = (0xFF00000000000000L & (((long) stateBytes[start + 7]) << 56))
					| (0x00FF000000000000L & (((long) stateBytes[start + 6]) << 48))
					| (0x0000FF0000000000L & (((long) stateBytes[start + 5]) << 40))
					| (0x000000FF00000000L & (((long) stateBytes[start + 4]) << 32))
					| (0x00000000FF000000L & (((long) stateBytes[start + 3]) << 24))
					| (0x0000000000FF0000L & (((long) stateBytes[start + 2]) << 16))
					| (0x000000000000FF00L & (((long) stateBytes[start + 1]) << 8))
					| (0x00000000000000FFL & (stateBytes[start + 0]));
			return (endianness == ByteOrder.LITTLE_ENDIAN) ? ret : Long.reverseBytes(ret);
		}

		public int readInt(int start)
		{
			int ret = (0xFF000000 & ((stateBytes[start + 3]) << 24))
					| (0x00FF0000 & ((stateBytes[start + 2]) << 16))
					| (0x0000FF00 & ((stateBytes[start + 1]) << 8))
					| (0x000000FF & (stateBytes[start + 0]));
			return (endianness == ByteOrder.LITTLE_ENDIAN) ? ret : Integer.reverseBytes(ret);
		}

		public short readShort(int start)
		{
			short ret = (short) ((0xFF00 & ((stateBytes[start + 1]) << 8))
					| (0x00FF & ((short) stateBytes[start + 0])));
			return (endianness == ByteOrder.LITTLE_ENDIAN) ? ret : Short.reverseBytes(ret);
		}

		public void fillX86ThreadRegisters()
		{
			registers.put("rax", readLong(0));
			registers.put("rbx", readLong(8));
			registers.put("rcx", readLong(16));
			registers.put("rdx", readLong(24));
			registers.put("rdi", readLong(32));
			registers.put("rsi", readLong(40));
			registers.put("rbp", readLong(48));
			registers.put("rsp", readLong(56));
			registers.put("r8", readLong(64));
			registers.put("r9", readLong(72));
			registers.put("r10", readLong(80));
			registers.put("r11", readLong(88));
			registers.put("r12", readLong(96));
			registers.put("r13", readLong(104));
			registers.put("r14", readLong(112));
			registers.put("r15", readLong(120));
			registers.put("rip", readLong(128));
			registers.put("rflags", readLong(136));
			registers.put("cs", readLong(144));
			registers.put("fs", readLong(152));
			registers.put("gs", readLong(160));
		}

		public void fillX86ExceptionData()
		{
			registers.put("trapno", readShort(0));
			registers.put("cpu", readShort(2));
			registers.put("err", readInt(4));
			registers.put("faultvaddr", readLong(8));
		}
	}

	public List<ThreadState> states;

	public ThreadCommand()
	{
		states = new ArrayList<>();
	}

	public ThreadCommand(int type, long size, long offset)
	{
		super(type, size, offset);
		states = new ArrayList<>();
	}

	public ThreadCommand readCommand(ImageInputStream stream, long streamSegmentOffset) throws IOException
	{
		super.readCommand(stream, streamSegmentOffset);
		long structOffset = 8;
		while (structOffset < cmdSize) {
			int flavor = stream.readInt();
			int size = stream.readInt();
			byte data[] = new byte[size * 4];
			stream.readFully(data);
			states.add(new ThreadCommand.ThreadState(flavor, size, data, stream.getByteOrder()));
			structOffset += size * 4 + 8;
		}
		return this;
	}

}
