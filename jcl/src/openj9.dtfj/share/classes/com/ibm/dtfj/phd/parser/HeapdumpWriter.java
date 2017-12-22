/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd.parser;

import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.util.Vector;

public class HeapdumpWriter extends Base {

	String filename;
	DataOutputStream dos;
	Vector strings = new Vector();
	int lastAddress;
	int[] classAddressCache = new int[4];
	int classAddressCacheIndex;
	public int totalObjects;
	public int totalRefs;
	public int version;
	int[] counts = new int[256];
	int biggestGap;
	int totalGap;
	int where;
	// body tags
	public static final int START_OF_HEADER = 1;
	public static final int START_OF_DUMP = 2;
	public static final int END_OF_DUMP = 3;
	public static final int LONG_OBJECT_RECORD = 4;
	public static final int OBJECT_ARRAY_RECORD = 5;
	public static final int CLASS_RECORD = 6;
	public static final int PRIMITIVE_ARRAY_RECORD = 7;
	public static final int NEW_OBJECT_ARRAY_RECORD = 8;
	// header tags
	public static final int TOTALS = 1;
	public static final int END_OF_HEADER = 2;
	public static final int HASHCODE_RANDOMS = 3;
	public static final int FULL_VERSION = 4;

	public HeapdumpWriter(String filename) {
		this.filename = filename;
		try {
			FileOutputStream fos = new FileOutputStream(filename);
			BufferedOutputStream bos = new BufferedOutputStream(fos);
			dos = new DataOutputStream(bos);
			dos.writeUTF("portable heap dump");
			dos.writeInt(4);        // version
			dos.writeInt(0);        // flags
			dos.writeByte(START_OF_HEADER);
			dos.writeByte(TOTALS);
			dos.writeInt(totalObjects);
			dos.writeInt(totalRefs);
			dos.writeByte(END_OF_HEADER);
			dos.writeByte(START_OF_DUMP);
		} catch (Exception e) {
			throw new Error("unexpected error: " + e);
		}
	}

	public void close() {
		try {
			dos.writeByte(END_OF_DUMP);   // end of dump
			dos.close();
			RandomAccessFile raf = new RandomAccessFile(filename, "rw");
			raf.readUTF();      // skip header
			raf.readInt();      // skip version
			raf.readInt();      // skip flags
			raf.readByte();     // skip start of header
			raf.readByte();     // skip totals record tag
			raf.writeInt(totalObjects);
			raf.writeInt(totalRefs);
			raf.close();
		} catch (Exception e) {
			throw new Error("unexpected error while closing file" + e);
		}
	}

	int refsSize(int address, int[] refs) {
		if (refs == null)
			return 0;
		int biggestGap = 0;
		for (int i = 0; i < refs.length; i++) {
			int gap = Math.abs(address - refs[i]) >> 3;
		if (gap > biggestGap) {
			biggestGap = gap;
		}
		}
		// use 0x80 because we need space for the sign bit
		if (biggestGap < 0x80)
			return 0;
		else if (biggestGap < 0x8000)
			return 1;
		else
			// add 64-bit support!
			return 2;
	}

	void writeRefs(int address, int[] refs, int refsSize) throws IOException {
		int numRefs = (refs == null ? 0 : refs.length);
		for (int i = 0; i < numRefs; i++) {
			int gap = (refs[i] - address) >> 3;
		if (refsSize == 0)
			dos.writeByte(gap);
		else if (refsSize == 1)
			dos.writeShort(gap);
		else if (refsSize == 2)
			dos.writeInt(gap);
		else if (refsSize == 3)
			dos.writeLong(gap);
		}
	}

	void addToCache(int classAddress) {
		/* May consider this as a future change to the spec...
        for (int i = 0; i < 4; i++) {
            if (classAddressCache[i] == classAddress)
                return;
        }
		 */
		classAddressCache[classAddressCacheIndex] = classAddress;
		classAddressCacheIndex = (classAddressCacheIndex + 1) % 4;
	}

	public void objectDump(int address, int classAddress, int[] refs) {
		if ((address & 7) != 0 || (classAddress & 7) != 0)
			return;
		Assert((address & 7) == 0);
		Assert((classAddress & 7) == 0);
		int gap = (address - lastAddress) >> 3;
		int absgap = Math.abs(gap);
		int numRefs = (refs == null ? 0 : refs.length);
		int refsSize = refsSize(address, refs);
		totalObjects++;
		totalRefs += numRefs;
		try {
			if (absgap < 0x8000) {
				int cacheIndex = -1;
				if (classAddress == classAddressCache[0]) 
					cacheIndex = 0;
				else if (classAddress == classAddressCache[1]) 
					cacheIndex = 1;
				else if (classAddress == classAddressCache[2]) 
					cacheIndex = 2;
				else if (classAddress == classAddressCache[3]) 
					cacheIndex = 3;
				if (cacheIndex != -1 && numRefs < 4) {
					// short object record
					int tag = 0x80;
					tag |= (cacheIndex << 5);
					tag |= (numRefs << 3);
					if (absgap >= 0x80)
						tag |= 4;
					tag |= refsSize;
					dos.writeByte(tag);
					if (absgap >= 0x80)
						dos.writeShort(gap);
					else
						dos.writeByte(gap);
					writeRefs(address, refs, refsSize);
					lastAddress = address;
					return;
				}
				if (numRefs < 8) {
					// medium object record
					int tag = 0x40;
					tag |= (numRefs << 3);
					if (absgap >= 0x80)
						tag |= 4;
					tag |= refsSize;
					dos.writeByte(tag);
					if (absgap >= 0x80)
						dos.writeShort(gap);
					else
						dos.writeByte(gap);
					dos.writeInt(classAddress);
					writeRefs(address, refs, refsSize);
					addToCache(classAddress);
					lastAddress = address;
					return;
				}
			}
			// long object record
			dos.writeByte(LONG_OBJECT_RECORD);
			int flags = refsSize << 4;
			if (absgap >= 0x8000) {
				dos.writeByte(flags | 0x80);
				dos.writeInt(gap);
			} else if (absgap >= 0x80) {
				dos.writeByte(flags | 0x40);
				dos.writeShort(gap);
			} else {
				dos.writeByte(flags);
				dos.writeByte(gap);
			}
			dos.writeInt(classAddress);
			dos.writeInt(numRefs);
			writeRefs(address, refs, refsSize);
			addToCache(classAddress);
			lastAddress = address;
		} catch (IOException e) {
			throw new Error("unexpected error " + e);
		}
	}

	public void classDump(int address, int superAddress, String name, int instanceSize, int refs[]) {
		Assert((address & 7) == 0);
		int gap = (address - lastAddress) >> 3;
				int absgap = Math.abs(gap);
				int numRefs = (refs == null ? 0 : refs.length);
				int refsSize = refsSize(address, refs);
				try {
					dos.writeByte(CLASS_RECORD);
					int flags = refsSize << 4;
					if (absgap >= 0x8000) {
						dos.writeByte(flags | 0x80);
						dos.writeInt(gap);
					} else if (absgap >= 0x80) {
						dos.writeByte(flags | 0x40);
						dos.writeShort(gap);
					} else {
						dos.writeByte(flags);
						dos.writeByte(gap);
					}
					dos.writeInt(instanceSize);
					dos.writeInt(superAddress);
					dos.writeUTF(name);
					dos.writeInt(numRefs);
					writeRefs(address, refs, refsSize);
					lastAddress = address;
					totalObjects++;
					totalRefs += numRefs;
				} catch (IOException e) {
					throw new Error("unexpected error " + e);
				}
	}

	public void objectArrayDump(int address, int classAddress, int refs[]) {
		// use common code with long object record?
		Assert((address & 7) == 0);
		Assert((classAddress & 7) == 0);
		int gap = (address - lastAddress) >> 3;
					int absgap = Math.abs(gap);
					int numRefs = (refs == null ? 0 : refs.length);
					int refsSize = refsSize(address, refs);
					try {
						dos.writeByte(OBJECT_ARRAY_RECORD);
						int flags = refsSize << 4;
						if (absgap >= 0x8000) {
							dos.writeByte(flags | 0x80);
							dos.writeInt(gap);
						} else if (absgap >= 0x80) {
							dos.writeByte(flags | 0x40);
							dos.writeShort(gap);
						} else {
							dos.writeByte(flags);
							dos.writeByte(gap);
						}
						dos.writeInt(classAddress);
						dos.writeInt(numRefs);
						writeRefs(address, refs, refsSize);
						lastAddress = address;
						totalObjects++;
						totalRefs += numRefs;
					} catch (IOException e) {
						throw new Error("unexpected error " + e);
					}
	}

	public void primitiveArrayDump(int address, int type, int length) {
		Assert((address & 7) == 0);
		if (type >= 8) type -= 8;       // convert unsigned to signed types
		Assert(type < 8);
		Assert(type >= 0);
		int gap = (address - lastAddress) >> 3;
		int absgap = Math.abs(gap);
		try {
			if (absgap >= 0x8000 || length >= 0x8000) {
				dos.writeByte(0x20 | (type << 2) | 2);
				dos.writeInt(gap);
				dos.writeInt(length);
			} else if (absgap >= 0x80 || length >= 0x80) {
				dos.writeByte(0x20 | (type << 2) | 1);
				dos.writeShort(gap);
				dos.writeShort(length);
			} else {
				dos.writeByte(0x20 | (type << 2) | 0);
				dos.writeByte(gap);
				dos.writeByte(length);
			}
			lastAddress = address;
			totalObjects++;
		} catch (IOException e) {
			throw new Error("unexpected error " + e);
		}
	}

	String className() {
		return "HeapdumpWriter";
	}
}
