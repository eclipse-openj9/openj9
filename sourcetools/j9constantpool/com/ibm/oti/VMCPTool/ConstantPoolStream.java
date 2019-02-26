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
package com.ibm.oti.VMCPTool;

import java.io.PrintWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

// The ConstantPoolStream class is a little ugly because it captures and writes state in 2 passes.
// The two passes should consist of exactly the same sequence of calls to write*().  The first pass
// captures offsets that may be "forward references".  The second pass then provides correct offsets
// of ConstantPoolItems from getOffset(ConstantPoolItem).
//
// The size of the constant pool is captured from the offset when open() is called.  The offset is
// then reset to 0.  Buffers are flushed and the output stream is attached.
//
// Thus a correct call sequence would be something like:
//  ConstantPoolStream s = new ConstantPoolStream("jcl", constantPool, itemCount);
//  /* First sequence of s.writeByte() and friends */
//  s.open(out);
//  /* Second sequence of s.writeByte() and friends */
//  s.close();

@SuppressWarnings("nls")
public class ConstantPoolStream {
	public final int version;
	public final Set<String> flags;
	private List<byte[]> outputQueue = new ArrayList<byte[]>();
	private ConstantPool constantPool;
	private int[] cpDescription;
	private PrintWriter out = null;
	private int offset = 0;
	private int itemCount;
	private boolean even = true;

	// These constants are taken from j9.h
	private static final int J9CPTYPE_UNUSED = 0;
	private static final int J9CPTYPE_CLASS = 1;
	private static final int J9CPTYPE_INSTANCE_FIELD = 7;
	private static final int J9CPTYPE_STATIC_FIELD = 8;
	private static final int J9CPTYPE_VIRTUAL_METHOD = 9;
	private static final int J9CPTYPE_STATIC_METHOD = 10;
	private static final int J9CPTYPE_SPECIAL_METHOD = 11;
	private static final int J9CPTYPE_INTERFACE_METHOD = 12;
	private static final int J9_CP_BITS_PER_DESCRIPTION = 8;
	private static final int J9_CP_DESCRIPTION_MASK = 0xff;
	private static final int J9_CP_DESCRIPTIONS_PER_U32 = 4;

	public static final int ITEM_SIZE = 8; // Bytes per Constant Pool Item

	public ConstantPoolStream(int version, Set<String> flags, ConstantPool constantPool, int itemCount) {
		this.version = version;
		this.flags = Collections.unmodifiableSet(flags);
		this.constantPool = constantPool;
		this.itemCount = itemCount;
		this.cpDescription = new int[(itemCount + J9_CP_DESCRIPTIONS_PER_U32 - 1) / J9_CP_DESCRIPTIONS_PER_U32];
		
		// Initialize constant pool descriptors
		for (int i = 0; i < itemCount; i++) {
			mark(i, J9CPTYPE_UNUSED);
		}
	}

	private void mark(int cpIndex, int type) {
		int index = cpIndex / J9_CP_DESCRIPTIONS_PER_U32;
		int shift = (cpIndex % J9_CP_DESCRIPTIONS_PER_U32) * J9_CP_BITS_PER_DESCRIPTION;
		cpDescription[index] |= (type & J9_CP_DESCRIPTION_MASK) << shift;
	}

	private void mark(int type) {
		if (0 != offset % ITEM_SIZE) {
			throw new Error("Constant Pool offset is not Item-aligned");
		}

		mark(offset / ITEM_SIZE, type);
	}

	public void markInstanceField() {
		mark(J9CPTYPE_INSTANCE_FIELD);
	}
	
	public void markStaticField() {
		mark(J9CPTYPE_STATIC_FIELD);
	}
	
	public void markStaticMethod() {
		mark(J9CPTYPE_STATIC_METHOD);
	}
	
	public void markVirtualMethod() {
		mark(J9CPTYPE_VIRTUAL_METHOD);
	}

	public void markSpecialMethod() {
		mark(J9CPTYPE_SPECIAL_METHOD);
	}

	public void markInterfaceMethod() {
		mark(J9CPTYPE_INTERFACE_METHOD);
	}

	public void markClass() {
		mark(J9CPTYPE_CLASS);
	}

	public void writeBoolean(boolean arg0) {
		writeByte(arg0 ? 1 : 0);
	}

	public void writeByte(int arg0) {
		write(new byte[] { (byte)arg0 });
	}

	public void writeBytes(String arg0) {
		throw new UnsupportedOperationException("writeBytes not supported");
	}

	public void writeChar(int arg0) {
		writeShort((short)arg0);
	}

	public void writeChars(String arg0) {
		throw new UnsupportedOperationException("writeChars not supported");
	}

	public void writeDouble(double arg0) {
		writeLong(Double.doubleToLongBits(arg0));
	}

	public void writeFloat(float arg0) {
		writeInt(Float.floatToIntBits(arg0));
	}

	public void writeInt(int arg0) {
		write(new byte[] { (byte)(arg0 >> 24), (byte)(arg0 >> 16), (byte)(arg0 >> 8), (byte)arg0 } );
	}

	public void writeLong(long arg0) {
		write(new byte[] { (byte)(arg0 >> 56), (byte)(arg0 >> 48), (byte)(arg0 >> 40), (byte)(arg0 >> 32), (byte)(arg0 >> 24), (byte)(arg0 >> 16), (byte)(arg0 >> 8), (byte)arg0 } );
	}

	public void writeShort(int arg0) {
		write(new byte[] { (byte)(arg0 >> 8), (byte)arg0 } );
	}

	public void writeUTF(String arg0) {
		byte[] data = arg0.getBytes(StandardCharsets.UTF_8);
		for (int i = 0; i < data.length; i++) {
			writeByte(data[i]);
		}
	}

	/**
	 * Add the specified bytes to the constant pool.
	 * The bytes must be in big endian order. They will
	 * be reversed on little endian platforms.
	 */
	protected void write(byte[] data) {
		if (null != out) {
			outputQueue.add(data);
			if (data.length == 4) {
				flushQueue();
			}
		}
		
		offset += data.length;
	}
	
	public int getOffset() {
		return offset;
	}

	public void alignTo(int alignment) {
		while ((getOffset() % alignment) != 0) {
			writeByte(0);
		}
	}
	
	private void flushQueue() {
		for (Iterator<byte[]> iter = outputQueue.iterator(); iter.hasNext();) {
			if (even) {
				out.print("\t\t{");
			}
			
			byte[] r0 = iter.next();
			if (r0.length == 4) {
				out.print(hex(r0));
			} else {
				byte[] r1 = iter.next();
				if (r0.length == 2 && r1.length == 2) {
					out.print("WORD_WORD(" + hex(r0) + ", " + hex(r1) + ")");
				} else {
					byte[] r2 = iter.next();
					if (r0.length == 2 && r1.length == 1 && r2.length == 1) {
						out.print("WORD_BYTE_BYTE(" + hex(r0) + ", " + hex(r1) + ", " + hex(r2) + ")");
					} else if (r0.length == 1 && r1.length == 1 && r2.length == 2) {
						out.print("BYTE_BYTE_WORD(" + hex(r0) + ", " + hex(r1) + ", " + hex(r2) + ")");
					} else {
						byte[] r3 = iter.next();
						if (r0.length == 1 && r1.length == 1 && r2.length == 1 && r3.length == 1) {
							out.print("BYTE_BYTE_BYTE_BYTE(" + hex(r0) + ", " + hex(r1) + ", " + hex(r2) + ", " + hex(r3) + ")");
						} else {
							throw new Error();
						}
					}
				}
			}

			if (even) {
				out.print(", ");
			} else {
				out.println("},");
			}
			even = !even;
		}
		outputQueue.clear();
	}

	private static final String HEX = "0123456789ABCDEF";

	private static String hex(byte[] array) {
		StringBuilder buffer = new StringBuilder(array.length * 2 + 2);

		buffer.append("0x");

		for (int i = 0; i < array.length; i++) {
			buffer.append(HEX.charAt((array[i] >> 4) & 0x0F));
			buffer.append(HEX.charAt((array[i] >> 0) & 0x0F));
		}

		return buffer.toString();
	}

	private Map<ConstantPoolItem, Offset> secondaryItems = new HashMap<ConstantPoolItem, Offset>();
	
	private class Offset {
		int offset;
		boolean wrote;
		Offset(int offset, boolean wrote) {
			this.offset = offset;
			this.wrote = wrote;
		}
	}

	public void writeSecondaryItem(ConstantPoolItem item) {
		// We "write" the secondary item if it hasn't been previously seen or written.
		// The secondary item can only have been written if there is an output stream.
		Offset offset = secondaryItems.get(item);
		if (null == offset || (null != out && !offset.wrote)) {
			item.write(this);
		}
	}
	
	public void setOffset(ConstantPoolItem item) {
		Offset offset = secondaryItems.get(item);
		if (null != offset && getOffset() != offset.offset) {
			throw new Error("Mismatched offset in secondary item");
		}
		secondaryItems.put(item, new Offset(getOffset(), null != out));
	}

	public int getOffset(ConstantPoolItem item) {
		Offset offset = secondaryItems.get(item);
		return null == offset ? -1 : offset.offset;
	}

	public void open(PrintWriter out) {
		int size = offset;
		offset = 0;
		outputQueue.clear();
		this.out = out;
		writeHeader(size);
	}

	public void close() {
		flushQueue();
		writeFooter();
	}

	private void writeFooter() {
		out.println("\t},");
		out.println("#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)");
		writeUnsplitDescription();
		out.println("#else");
		writeDescription();
		out.println("#endif");
		out.println("};");
		out.println();
		out.println("const J9ROMClass * jclROMClass = &_jclROMClass.romClass;");
	}

	private void writeHeader(int size) {
		if (null != out) {
			if (0 == size || 0 != size % ConstantPoolStream.ITEM_SIZE) {
				throw new Error("Constant Pool size is not Item-aligned");
			}

			int totalCPSize = size / ConstantPoolStream.ITEM_SIZE;

			out.println("static const struct {");
			out.println("\tJ9ROMClass romClass;");
			out.println("\tJ9ROMConstantPoolItem romConstantPool[" + totalCPSize + "];");
			out.println("\tU_32 cpDescription[" + cpDescription.length + "];");
			out.println("} _jclROMClass = {");

			String shapeDescriptionOffset = "sizeof(J9ROMClass) - offsetof(J9ROMClass, cpShapeDescription) + " + size;
			out.println("\t{0,0,0,0,0,J9AccClassUnsafe, 0,0,0,0,0,0,0,0, " + itemCount + ", " + itemCount + ", 0,0,0, " + shapeDescriptionOffset + ", 0,},");
			out.println("\t{");
		}
	}

	private void writeUnsplitDescription() {
		final int J9CPTYPE_FIELD = 7;
		final int J9CPTYPE_INSTANCE_METHOD = 9;

		out.print("\t{");
		for (int i = 0; i < cpDescription.length; i++) {
			int descriptionWord = cpDescription[i];
			for (int j = 0; j < J9_CP_DESCRIPTIONS_PER_U32; j++) {
				int shift = j * J9_CP_BITS_PER_DESCRIPTION;
				int oldNibble = (descriptionWord >>> shift) & J9_CP_DESCRIPTION_MASK;
				int newNibble = oldNibble;
				if ((J9CPTYPE_INSTANCE_FIELD == oldNibble) || (J9CPTYPE_STATIC_FIELD == oldNibble)) {
					newNibble = J9CPTYPE_FIELD;
				} else if ((J9CPTYPE_VIRTUAL_METHOD == oldNibble) || (J9CPTYPE_SPECIAL_METHOD == oldNibble)) {
					newNibble = J9CPTYPE_INSTANCE_METHOD;
				}
				if (newNibble != oldNibble) {
					descriptionWord &= ~(J9_CP_DESCRIPTION_MASK << shift);
					descriptionWord |= (newNibble & J9_CP_DESCRIPTION_MASK) << shift;
				}
			}
			out.print("0x" + Integer.toHexString(descriptionWord) + ", ");
		}
		out.println("}");
	}

	private void writeDescription() {
		out.print("\t{");
		for (int i = 0; i < cpDescription.length; i++) {
			out.print("0x" + Integer.toHexString(cpDescription[i]) + ", ");
		}
		out.println("}");
	}

	public int getIndex(PrimaryItem item) {
		return constantPool.getIndex(item);
	}

	public PrimaryItem findPrimaryItem(Object obj) {
		return constantPool.findPrimaryItem(obj);
	}

	public void comment(String string) {
		if (null != out) {
			out.print("\t/* " + string + " */");
		}
	}
}
