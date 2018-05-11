/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2002, 2018 IBM Corp. and others
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

import com.ibm.dtfj.phd.PHDJavaObject;
import com.ibm.dtfj.phd.util.LongEnumeration;

import java.util.*;
import java.io.*;

/**
 *  This class simply prints the contents of the heap dump.
 */
public class PrintHeapdump extends Base {

	boolean hash;
	String[] types = {"bool", "char", "float", "double", "byte", "short", "int", "long"};
	int objectCount;
	int objectArrayCount;
	int classCount;
	int primitiveArrayCount;
	int totalObjectCount;
	int refCount;
	int minimumInstanceSize = 100;
	PrintStream out;
	static boolean is64Bit;

	public static void main(String args[]) throws Exception {
		new PrintHeapdump(args);
	}

	public PrintHeapdump(String args[]) throws Exception {
		parseOptions(args);
		out = System.out;
		parse(args[args.length - 1]);
	}

	public PrintHeapdump(String filename) throws Exception {
		out = System.out;
		parse(filename);
	}

	public PrintHeapdump(String filename, PrintStream out) throws Exception {
		this.out = out;
		parse(filename);
	}

	int roundup(int size) {
		return (size + 7) & ~7;
	}

	void printRefs(LongEnumeration refs) {
		out.print("\t");
		while (refs.hasMoreElements()) {
			out.print("0x" + hex(refs.nextLong()) + " ");
			refCount++;
		}
		out.println("");
	}

	public void parse(String filename) throws Exception {
		HeapdumpReader reader = new HeapdumpReader(filename);
		out.println("// Version: " + reader.full_version());
		reader.parse(new PortableHeapDumpListener() {
			public void objectDump(long address, long classAddress, int flags, int hashCode, LongEnumeration refs, long instanceSize) {
				DumpClass.foundClass(classAddress);
			}
			public void objectArrayDump(long address, long classAddress, int flags, int hashCode, LongEnumeration refs, int length, long instanceSize) {
				DumpClass.foundClass(classAddress);
			}
			public void classDump(long address, long superAddress, String name, int instanceSize, int flags, int hashCode, LongEnumeration refs) {
				DumpClass.put(address, name, instanceSize);
				if (instanceSize >= 0 && instanceSize < minimumInstanceSize)
					minimumInstanceSize = instanceSize;
			}
			public void primitiveArrayDump(long address, int type, int length, int flags, int hashCode, long instanceSize) {
			}
		});
		reader.close();
		reader = new HeapdumpReader(filename);
		is64Bit = reader.is64Bit();
		/*
		 * Note the sizes printed are just approximate.
		 */
		reader.parse(new PortableHeapDumpListener() {
			public void objectDump(long address, long classAddress, int flags, int hashCode, LongEnumeration refs, long instanceSize) {
				DumpClass cl = DumpClass.get(classAddress);
				long size = cl.instanceSize;
				out.println("0x" + hexpad(address).toUpperCase() + " [" + size + "] " + cl.name + (hash ? " [hashcode = " + hex(hashCode) + "]" : "") );
				printRefs(refs);
				objectCount++;
				totalObjectCount++;
			}
			public void objectArrayDump(long address, long classAddress, int flags, int hashCode, LongEnumeration refs, int length, long instanceSize) {
				DumpClass cl = DumpClass.get(classAddress);
				if (instanceSize == PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE) {
					// calculate as best we can
					instanceSize = (length + 4) << (is64Bit ? 3 : 2);
					instanceSize = roundup((int)instanceSize);
				}
				out.println("0x" + hexpad(address).toUpperCase() + " [" + instanceSize + "] array of " + cl.name + (hash ? " [hashcode = " + hex(hashCode) + "]" : ""));
				printRefs(refs);
				objectArrayCount++;
				totalObjectCount++;
			}
			public void classDump(long address, long superAddress, String name, int instanceSize, int flags, int hashCode, LongEnumeration refs) {
				out.println("0x" + hexpad(address).toUpperCase() + " [" + (is64Bit ? "552" : "304") + "] class " + name + (hash ? " [hashcode = " + hex(hashCode) + "]" : ""));
				printRefs(refs);
				classCount++;
				totalObjectCount++;
			}
			public void primitiveArrayDump(long address, int type, int length, int flags, int hashCode, long instanceSize) {
				if (instanceSize == PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE) {
					// calculate as best we can
					instanceSize = (length << (type & 3)) + (is64Bit ? 24 : 12);
					instanceSize = roundup((int)instanceSize);
				}
				if (type >= types.length) {
					out.println("Warning! bad type " + type + " found in following object");
					type = 0;
				}
				out.println("0x" + hexpad(address).toUpperCase() + " [" + instanceSize + "] " + types[type] + "[]" + (hash ? " [hashcode = " + hex(hashCode) + "]" : ""));
				primitiveArrayCount++;
				totalObjectCount++;
			}
		});
		reader.close();
		out.println("");
		out.println("// Breakdown - Classes: " + classCount + ", Objects: " + objectCount + ", ObjectArrays: " + objectArrayCount + ", PrimitiveArrays: " + primitiveArrayCount);
		out.println("// EOF:  Total 'Objects',Refs(null) : " + totalObjectCount + "," + refCount + "(0)");
	}

	String[] options() {
		return new String[] {"-hash"};
	}

	String[] optionDescriptions() {
		return new String[] {
				"\tInclude the hash codes"
		};
	}

	public boolean parseOption(String arg, String opt) {
		if ("-hash".equals(arg)) {
			hash = true;
			return true;
		}
		return super.parseOption(arg, opt);
	}

	String className() {
		return "PrintHeapdump";
	}

	public static String hexpad(long word) {
		String s = is64Bit ? Long.toHexString(word) : Integer.toHexString((int)word);

		for (int i = s.length(); i < (is64Bit ? 16 : 8); i++)
			s = "0" + s;
		return s;
	}

	public static String hex(long word) {
		String s = is64Bit ? String.format("%016X",word) : String.format("%08X",word);
		return s;
	}
	
}

class DumpClass {
	long address;
	String name;
	int instanceSize;
	static HashMap classes = new HashMap();

	DumpClass(long address, String name, int instanceSize) {
		this.address = address;
		this.name = name;
		this.instanceSize = instanceSize;
	}

	static void put(long address, String name, int instanceSize) {
		classes.put(Long.valueOf(address), new DumpClass(address, name, instanceSize));
	}

	static void foundClass(long address) {
		DumpClass cl = (DumpClass)classes.get(Long.valueOf(address));
		if (cl == null) {
			cl = new DumpClass(address, "unknown class 0x" + Long.toHexString(address), 0);
			classes.put(Long.valueOf(address), cl);
		}
	}

	static DumpClass get(long address) {
		return (DumpClass)classes.get(Long.valueOf(address));
	}
}
