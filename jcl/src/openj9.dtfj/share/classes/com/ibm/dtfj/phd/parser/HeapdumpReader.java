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

import java.io.*;
import java.util.zip.GZIPInputStream;
import java.util.Vector;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.phd.PHDImage;
import com.ibm.dtfj.phd.util.*;
import com.ibm.dtfj.phd.PHDJavaObject;

/**
 *  This class parses a PHD format heapdump file. To use it, first create the object 
 *  (see {@link #HeapdumpReader}) passing the file name and then parse it (see {@link #parse})
 *  passing it an object that obeys the {@link com.ibm.dtfj.phd.parser.PortableHeapDumpListener} interface.
 */
public class HeapdumpReader extends Base {

	private static final long MAX_UNSIGNED_INT_AS_LONG = 0xffffffffL;
	String filename;
	DataStreamAdapter dis;
	long lastAddress;
	long[] classAddressCache = new long[4];
	int classAddressCacheIndex;
	int totalObjects;
	int totalRefs;
	int version;
	int random1;
	int random2;
	int totalLong;
	int totalShort;
	int totalMedium;
	int totalPrim;
	int totalLongPrim;
	int totalClass;
	int totalArray;
	int totalHash;
	int totalActualRefs;
	int totalNegGaps;
	Vector classNames = new Vector();
	int dumpFlags;
	String full_version = "unknown";
	boolean dbg = debug;
	boolean pre78432;
	int gapShift = 3;
	boolean j9;
	NumberStream refStream = new BufferedNumberStream();
	RefEnum refEnum = new RefEnum(refStream);
	private static final boolean nomangle = Boolean.getBoolean("findroots.nomangle");
	boolean continueParse;
	PHDImage image = null;

	/**
	 * Create a new HeapdumpReader object from the given file. The file must be in Phd format.
	 * Image must be supplied to allow us to clean up streams when it is closed.
	 * @throws IOException 
	 */
	public HeapdumpReader( File file, PHDImage image ) throws IOException {
		this(file.getAbsolutePath());
		this.image = image;
		// If image is null we will throw an NPE.
		// This is intentional, this constructor should not be called without an image or
		// there is no way to close streams when Image.close() is called and we leak file
		// handles and block files from being closed. 
		if( this.image == null ) {
			// Throw NPE manually to provide an explanation.
			throw new NullPointerException("PHDImage must be provided to allow streams to be cleaned up.");
		}
		this.image.registerReader(this);
	}
	
	public HeapdumpReader(ImageInputStream stream, PHDImage image) throws IOException {
		this(stream);
		this.image = image;
		// If image is null we will throw an NPE.
		// This is intentional, this constructor should not be called without an image or
		// there is no way to close streams when Image.close() is called and we leak file
		// handles and block files from being closed. 
		if( this.image == null ) {
			// Throw NPE manually to provide an explanation.
			throw new NullPointerException("PHDImage must be provided to allow streams to be cleaned up.");
		}
		this.image.registerReader(this);
	}
	
	/**
	 * Create a new HeapdumpReader object from the given stream. The file must be in Phd format.
	 * @throws IOException 
	 */
	protected HeapdumpReader(ImageInputStream stream) throws IOException {
		this.filename="[data stream]";		//indicate that this came from a data stream not a file
		stream.seek(0);		//make sure the stream is at the beginning
		dis = new DataStreamAdapter(stream);
		processData();
	}
	
	
	/**
	 * Create a new HeapdumpReader object from the given file. The file must be in Phd format.
	 * @throws IOException 
	 */
	protected HeapdumpReader(String filename) throws IOException {
		if (dbg) System.out.println("opening file " + filename);
		this.filename = filename;
		InputStream is = null;
		try {
			if (filename.endsWith(".gz")) {
				is = new BufferedInputStream(new GZIPInputStream(new FileInputStream(filename)));
			} else {			
				FileInputStream fis = new FileInputStream(filename);
				is = new BufferedInputStream(fis);
			}
			dis = new DataStreamAdapter(new DataInputStream(is));
			processData();
		} catch (java.io.UTFDataFormatException e) {
			try {
				if (filename.endsWith(".gz")) {
					is = new GZIPInputStream(new FileInputStream(filename));
				} else {			
					FileInputStream fis = new FileInputStream(filename);
					is = new BufferedInputStream(fis);
				}
				dis = new DataStreamAdapter(new DataInputStream(is));
				long l = dis.readLong();
				if (l == 0x12d73f94b12fdfL) {
					UTFDataFormatException e1 = new UTFDataFormatException(
							"Warning!\nThis PHD file has been ftp'd in ascii mode and is consequently unusable.\nPlease go back and ftp the original file again but this time specifying binary ftp mode.");
					e1.initCause(e);
					e = e1;
					System.err.println(e.getMessage());
				}
				throw e;
			} catch (Exception ee) {
				IOException ioe = new IOException("Error parsing PHD file");
				ioe.initCause(ee);
				throw ioe;
			}
		} catch (Exception e) {
			IOException ioe = new IOException("Error parsing PHD file");
			ioe.initCause(e);
			throw ioe;
		}
	}

	private void processData() throws IOException {
		try {
			// Remember the first two bytes in case the header is corrupt
			dis.mark(2);
			int len = dis.readUnsignedShort();
			dis.reset();
			//String header = dis.readUTF();
			String header = readUTF();
			if (dbg) System.out.println("read header: " + header);
			if (header.equals("portable heap dump")) {
				version = dis.readInt();
				if (version != 4 && version != 5 && version != 6) throw new IOException("unexpected version: " + version);
			} else {
				// Reconstruct length bytes in case the header is not a UTF string
				throw new IOException("bad header in '" + filename+"' : "+(char)(len >> 8 & 0xff)+(char)(len & 0xff)+header.substring(0, Math.min(header.length(), 50)));
			}
			if (dbg) System.out.println("phd version: " + version);
			if (version == 5) {
				gapShift = 2;
			}
			dumpFlags = dis.readInt();
			if ((dumpFlags & 4) != 0) {
				gapShift = 2;
				j9 = true;
			}
			if (dbg) System.out.println("read dump flags 0x" + hex(dumpFlags));
			int tag = dis.readUnsignedByte();
			Assert(tag == HeapdumpWriter.START_OF_HEADER);
			if (dbg) System.out.println("read start of header tag");
			do {
				tag = dis.readUnsignedByte();
				switch (tag) {
				case HeapdumpWriter.TOTALS:
					if (dbg) System.out.println("read totals tag");
					totalObjects = dis.readInt();
					totalRefs = dis.readInt();
					break;
				case HeapdumpWriter.END_OF_HEADER:
					if (dbg) System.out.println("read end of header tag");
					break;
				case HeapdumpWriter.HASHCODE_RANDOMS:
					if (dbg) System.out.println("read hashcodes tag");
					random1 = dis.readInt();
					random2 = dis.readInt();
					break;
				case HeapdumpWriter.FULL_VERSION:
					//full_version = dis.readUTF();
					full_version = readUTF();
					if (full_version.endsWith("0917")) {
						// before defect 78432 went in, primitive arrays incorrectly used ints
						pre78432 = true;
					}
					if (dbg) System.out.println("read full version tag, version = " + full_version);
					break;
				default:
					throw new IOException("unrecognized tag: " + tag);
				}
			} while (tag != HeapdumpWriter.END_OF_HEADER);
			tag = dis.readUnsignedByte();
			Assert(tag == HeapdumpWriter.START_OF_DUMP);
			if (dbg) System.out.println("read start of dump tag");
		} catch (Exception e) {
			IOException ioe = new IOException("Error parsing PHD file");
			ioe.initCause(e);
			throw ioe;
		}	
	}
	
	/**
	 *  Returns the full version for the JVM that created this heapdump.
	 */
	public String full_version() {
		return full_version;
	}

	/**
	 *  Returns the Phd version number. Recognized values are 4 and 5.
	 */
	public int version() {
		return version;
	}

	/**
	 *  Returns true if this is a 64-bit heap dump.
	 */
	public boolean is64Bit() {
		return (dumpFlags & 1) != 0;
	}

	/**
	 * Returns true if this is a J9 heap dump.
	 */
	public boolean isJ9() {
		return j9;
	}
	
	/**
	 * Returns true all objects in this heap dump
	 * will have hashcodes set, regardless of the
	 * flags on the record.
	 */
	public boolean allObjectsHashed() {
		return (dumpFlags & 2) != 0;
	}
	
	/**
	 * Returns the total number of objects in the dump.
	 */
	public int totalObjects() {
		return totalObjects;
	}

	/**
	 * Returns the total number of references in the dump.
	 */
	public int totalRefs() {
		return totalRefs;
	}

	int Handle2Hash(long address) {
		return (int)(address >>> 3);
	}

	int ROTATE(int value, int n) {
		return ((value >>> n)|(value << (8*4-n)));
	}

	int MANGLE(int hashCode) {
		return ((ROTATE((hashCode^random1) , 17) ^ random2) >>> 1);
	}

	int getHashCode(long address, int flags) throws Exception {
		if (j9 && allObjectsHashed() ) {
			// j9 dumps always store the hashcode in a short
			if (dbg) System.out.println("request to getHashCode (j9)");
			// All objects have been hashed with a 16 bit hashcode.
			// (J9 R23, R24, R25)
			// J9 converts a 15 bit hashcode to 32 bits
			int r = (dis.readShort() & 0x7fff);
			return (r << 16 | r);
		} else if (flags != 0) {
			// Sovereign and J9 R26+ use a 32 bit hashcode,
			// it will or will not be set according to the flags.
			if (dbg) System.out.println("request to getHashCode (sov/J9 R26+)");
			totalHash++;
			return dis.readInt();
		} else if( !j9 ){
			// For sovereign we can generate a hashcode if one wasn't
			// supplied.
			if (dbg) System.out.println("request to getHashCode (sov mangle)");
			return nomangle ? 0 : MANGLE(Handle2Hash(address));
		} else {
			// For J9 we cannot generate a hashcode if one wasn't
			// supplied.
			if (dbg) System.out.println("request to getHashCode (J9 unset)");
			return 0;
		}
	}

	/**
	 * Read the instance size from the stream if version 6 PHD or beyond.
	 * <p>
	 * In version 6 PHD and beyond, the instance size is present in the datastream for 
	 * primitive and object arrays as an unsigned int with the size measured in 32-bit words, 
	 * i.e. the actual length divided by 4 (allows us to encode a length up to 16GB in an unsigned int)
	 * <p>
	 * For an object array, the instance size follows the count of number of elements.  
	 * For a primitive array, the instance size follows the hash code if present.
	 * <p>
	 * Should the encoding for the instance size ever change, then this method should change and
	 * so should the code in two other places:
	 * 1. In the vm itself in heapdump.cpp
	 * 2. In the code that generates a heapdump in dtfjview 
	 * @return the instance size or PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE
	 * @throws IOException
	 */
	private long getInstanceSize() throws IOException {
		// instance size is only present for version 6 or above
		if (version >= 6) {
			int val = dis.readInt();
			// instance size is held in the PHD since PHD version 6     
			// it is encoded in the datastream as an unsigned int with the value divided by 4 
			// to make it possible to encode up to 16GB, so multiply by 4
			return (MAX_UNSIGNED_INT_AS_LONG & (long)val) * 4;
		} else {
			return PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE;
		}
	}

	long readWord() throws Exception {
		if (is64Bit())
			return dis.readLong();
		else
			return dis.readInt();
	}

	long readUnsignedWord() throws Exception {
		if (is64Bit())
			return dis.readLong();
		else {
			int val = dis.readInt();
			// Zero out the top 32 bits. Without this the top 32 bits become 0xffffffff when val is a "-"vs int
			return MAX_UNSIGNED_INT_AS_LONG & (long)val;
		}
	}

	void readRefs(long address, long classAddress, int numRefs, int refsSize) throws IOException {
		refStream.clear();
		long gap = 0;
		if (dbg) System.out.println("readRefs, numRefs = " + numRefs + " refsSize = " + refsSize);
		for (int i = 0; i < numRefs; i++) {
			if (refsSize == 0)
				gap = dis.readByte() << gapShift;
			else if (refsSize == 1)
				gap = dis.readShort() << gapShift;
			else if (refsSize == 2)
				gap = (long)dis.readInt() << gapShift;
			else
				gap = dis.readLong() << gapShift;
			long ref = is64Bit() ? address + gap : (address + gap) & MAX_UNSIGNED_INT_AS_LONG;
			// Fix j9 bug - they currently output the class address as the first reference!
			if (j9 && i == 0 && ref == classAddress)
				continue;
			refStream.writeLong(ref);
			if (dbg) System.out.println("read ref " + hex(ref) + " with gap " + hex(gap));
			totalActualRefs++;
		}
		refStream.rewind();
	}

	/**
	 * Reverse the order of the elements for an object array.
	 * <p>
	 * In a portable heapdump from the vm, the array elements are in reverse order, 
	 * because for historical reasons that is the order in which the gc iterator returns them.
	 * <p> 
	 * Reverse them here so that they are in the other order i.e. index order.
	 * This means that they will come back from getReferences() in index order, which
	 * is the order in which they are returned by DDR implementations of DTFJJavaObject.
	 * <p>
	 * Even though there is no guarantee in getReferences() about the order in which 
	 * references will be returned, the jdmpview x/j command makes an implicit 
	 * assumption that they are in index order when it prints the elements. 
	 * So this reordering is necessary to make x/j show the array correctly. 
	 * See CMVC 193691
	 * 
	 */
	void reverseOrderOfRefs() {
		int count = refEnum.numberOfElements();
		long[] refs = new long[count];	
		for (int i = 0; i < count; i++) {
			refs[i] = (Long) refEnum.nextElement();			
		}
		refStream.clear();


		for (int i = count - 1; i >= 0; i--) {
			refStream.writeLong(refs[i]);
		}		
		refStream.rewind();
	}

	long getRelativeAddress(int flags) throws IOException {
		long gap = 0;
		switch (flags) {
		case 0:
			gap = (dis.readByte() << gapShift);
			if (dbg) System.out.println("relative address 0x" + hex(lastAddress + gap) + " = last address 0x" + hex(lastAddress) + " + byte gap 0x" + hex(gap));
			break;
		case 1:
			gap = (dis.readShort() << gapShift);
			if (dbg) System.out.println("relative address 0x" + hex(lastAddress + gap) + " = last address 0x" + hex(lastAddress) + " + short gap 0x" + hex(gap));
			break;
		case 2:
			gap = ((long)dis.readInt() << gapShift);
			if (dbg) System.out.println("relative address 0x" + hex(lastAddress + gap) + " = last address 0x" + hex(lastAddress) + " + int gap 0x" + hex(gap));
			break;
		case 3:
			gap = (dis.readLong() << gapShift);
			if (dbg) System.out.println("relative address 0x" + hex(lastAddress + gap) + " = last address 0x" + hex(lastAddress) + " + long gap 0x" + hex(gap));
			break;
		}
		return is64Bit() ? lastAddress + gap : (lastAddress + gap) & MAX_UNSIGNED_INT_AS_LONG;
	}

	public void exitParse() {
		continueParse = false;
	}
	/**
	 *  Parse the heapdump. This uses callbacks via the PortableHeapDumpListener interface. Any
	 *  exceptions that the listener raises are propagated back.
	 *  @return true if there is more data to parse
	 */
	public boolean parse(PortableHeapDumpListener listener) throws Exception {
		long address = 0;
		for (continueParse = true; continueParse;) {
			int tag = dis.readUnsignedByte();
			if (dbg) System.out.println("read tag " + hex(tag));
			if ((tag & 0x80) != 0) {
				// short object
				tag &= 0x7f;
				long classAddress = classAddressCache[tag >> 5];
				int numRefs = (tag >> 3) & 3;
				int refsSize = tag & 3;
				address = getRelativeAddress((tag >> 2) & 1);
				int hashCode = getHashCode(address, 0);
				int objFlags = j9 || allObjectsHashed() ? 1 : 0;
				readRefs(address, classAddress, numRefs, refsSize);
				if (dbg) System.out.println(hex(address) + ": short object, class = " + hex(classAddress));
				totalShort++;
				lastAddress = address;
				listener.objectDump(address, classAddress, objFlags, hashCode, refEnum,  PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE);
			} else if ((tag & 0x40) != 0) {
				// medium object
				tag &= 0x3f;
				int numRefs = tag >> 3;
				int refsSize = tag & 3;
				address = getRelativeAddress((tag >> 2) & 1);
				long classAddress = readUnsignedWord();
				classAddressCache[classAddressCacheIndex] = classAddress;
				classAddressCacheIndex = (classAddressCacheIndex + 1) % 4;
				int hashCode = getHashCode(address, 0);
				int objFlags = j9 || allObjectsHashed() ? 1 : 0;
				readRefs(address, classAddress, numRefs, refsSize);
				if (dbg) System.out.println(hex(address) + ": medium object, class = " + hex(classAddress));
				totalMedium++;
				lastAddress = address;
				listener.objectDump(address, classAddress, objFlags, hashCode, refEnum, PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE);
			} else if ((tag & 0x20) != 0) {
				// primitive array
				int type = (tag >> 2) & 7;
				int size = tag & 3;
				int length = 0;
				if (pre78432 && size == 3) {
					address = lastAddress + (dis.readInt() << gapShift);
					length = (int)dis.readInt();
					if (dbg) System.out.println("warning! bad primitive array");
				} else {
					address = getRelativeAddress(size);
					if (size == 0) {
						length = dis.readUnsignedByte();
					} else if (size == 1) {
						length = dis.readUnsignedShort();
					} else if (size == 2) {
						length = dis.readInt();
					} else {
						length = (int)dis.readLong();
					}
				}
				// For J9, pre R2.6, primitives always have short hashcodes.
				// For sov and J9 R26+ primitives don't ever have hashcodes.
				// (For J9 R26 primitive arrays with hashcode set get a long
				// primitive array record.)
				int hashCode = getHashCode(address, 0);
				long instanceSize = getInstanceSize(); // will read an unsigned int * 4 from stream if version >= 6
				int objFlags = j9 || allObjectsHashed() ? 1 : 0;
				if (dbg) System.out.println(hex(address) + ": primitive array, short record, length " + length + ", instance size = " + instanceSize);
				totalPrim++;
				lastAddress = address;
				listener.primitiveArrayDump(address, type, length, objFlags, hashCode, instanceSize);
			} else switch (tag) {
			case HeapdumpWriter.END_OF_DUMP:
				if (nomangle) System.out.println("totalLong = " + totalLong + " totalMedium = " + totalMedium + " totalShort = " + totalShort + " totalPrim = " + totalPrim + " totalHash = " + totalHash);
				if (nomangle) System.out.println("totalLongPrim = " + totalLongPrim + " totalClass = " + totalClass + " totalArray = " + totalArray + " totalRefs = " + totalActualRefs);
				if (nomangle) System.out.println("version = " + version);
				return false;
			case HeapdumpWriter.LONG_OBJECT_RECORD: {
				int flags = dis.readUnsignedByte();
				address = getRelativeAddress((flags >> 6) & 3);
				long classAddress = readUnsignedWord();
				classAddressCache[classAddressCacheIndex] = classAddress;
				classAddressCacheIndex = (classAddressCacheIndex + 1) % 4;
				int hashCode = getHashCode(address, flags & 2);
				int objFlags = (j9 || allObjectsHashed() ? 1 : 0) | flags & 0x3;
				int numRefs = dis.readInt();
				int refsSize = (flags >> 4) & 3;
				readRefs(address, classAddress, numRefs, refsSize);
				long instanceSize = PHDJavaObject.UNSPECIFIED_INSTANCE_SIZE;
				if (dbg) {
					System.out.println(hex(address) + ": long object, hash code = " + hex(hashCode) + " flags = " + hex(flags) + " class = " + hex(classAddress) + " numRefs = " + numRefs + " instance size = " + instanceSize);
				}
				totalLong++;
				lastAddress = address;
				listener.objectDump(address, classAddress, objFlags, hashCode, refEnum, instanceSize);
				break;
			}
			case HeapdumpWriter.NEW_OBJECT_ARRAY_RECORD:
				totalArray++;
			case HeapdumpWriter.OBJECT_ARRAY_RECORD: {
				int flags = dis.readUnsignedByte();
				address = getRelativeAddress((flags >> 6) & 3);
				long classAddress = readUnsignedWord();
				int hashCode = getHashCode(address, flags & 2);
				int objFlags = (j9 || allObjectsHashed() ? 1 : 0) | flags & 0x3;
				int numRefs = dis.readInt();
				int refsSize = (flags >> 4) & 3;
				readRefs(address, classAddress, numRefs, refsSize);
				reverseOrderOfRefs(); // see method javadoc for why we reverse the order of the references
				int length = numRefs;
				if (tag == HeapdumpWriter.NEW_OBJECT_ARRAY_RECORD) {
					length = dis.readInt();
				}
				long instanceSize = getInstanceSize(); // will read an unsigned int * 4 from stream if version >= 6
				if (dbg) System.out.println(hex(address) + ": object array, hash code = " + hex(hashCode) + " flags = " + hex(flags)+" num refs = " + numRefs + " length = " + length + " instance size = " + instanceSize);
				lastAddress = address;
				listener.objectArrayDump(address, classAddress, objFlags, hashCode, refEnum, length, instanceSize);
				break;
			}
			case HeapdumpWriter.CLASS_RECORD: {
				int flags = dis.readUnsignedByte();
				address = getRelativeAddress((flags >> 6) & 3);
				int instanceSize = dis.readInt();
				int hashCode = getHashCode(address, flags & 8);
				// Do classes always have a persistent hashcode?
				int objFlags = (j9 || allObjectsHashed() ? 1 : 0) | ((flags & 8) != 0 ? 0x3 : 0);
				long superAddress = readUnsignedWord();
				if (dbg) System.out.println(hex(address) + ": class flags = " + hex(flags)  + " hashCode = " + hex(hashCode) + " instanceSize = " + instanceSize + " superAddress = " + hex(superAddress));
				//String className = dis.readUTF();
				String className = readUTF();
				if (dbg) System.out.println(hex(address) + ": class " + className);
				int numRefs = dis.readInt();
				int refsSize = (flags >> 4) & 3;
				readRefs(address, -1, numRefs, refsSize);
				totalClass++;
				lastAddress = address;
				listener.classDump(address, superAddress, className, instanceSize, objFlags, hashCode, refEnum);
				break;
			}
			case HeapdumpWriter.PRIMITIVE_ARRAY_RECORD: {
				int flags = dis.readUnsignedByte();
				int type = flags >>> 5;
				int length = 0;
				if ((flags & 0x10) == 0) {
					address = lastAddress + (dis.readByte() << gapShift);
					length = dis.readUnsignedByte();
				} else {
					address = lastAddress + (readWord() << gapShift);
					length = (int)readUnsignedWord();
				}
				int hashCode = getHashCode(address, flags & 2);
				long instanceSize = getInstanceSize(); // will read an unsigned int from stream if version >= 6
				int objFlags = (j9 || allObjectsHashed() ? 1 : 0) | flags & 0x3;
				if (dbg) System.out.println(hex(address) + ": primitive array, long record, length = " + length + " hash code = " + hex(hashCode) + " flags = " + hex(flags) + " instance size = " + instanceSize);
				totalLongPrim++;
				lastAddress = address;
				listener.primitiveArrayDump(address, type, length, objFlags, hashCode, instanceSize);
				break;
			}
			default:
				//System.out.println("unexpected tag: " + tag +" "+dis.available());
				throw new Exception("unexpected tag: " + tag);
			}
			// Important - don't include any code here in case callback throws an exception and wants to resume by restarting parse
		}
		return true;
	}

	String className() {
		return "HeapdumpReader";
	}

	private String readUTF() throws IOException {
		int length = dis.readUnsignedShort();
		byte[] buf = new byte[length];
		dis.readFully(buf);
		return new String(buf, "UTF-8");//AJ
	}

	class RefEnum implements LongEnumeration {

		NumberStream stream;

		RefEnum(NumberStream stream) {
			this.stream = stream;
		}

		public boolean hasMoreElements() {
			return stream.hasMore();
		}

		public boolean hasNumberOfElements() {
			return true;
		}

		public int numberOfElements() {
			return stream.elementCount();
		}

		public Object nextElement() {
			return Long.valueOf(nextLong());
		}

		public long nextLong() {
			return stream.readLong();
		}

		public void reset() {
			stream.clear();
		}
	}

	public void close() {
		if( dis != null ) {
			try {
				dis.close();
			} catch (IOException e) {
				// Not a lot that we can do.
			}
		}
	}
	
	public void releaseResources() {
		if( dis != null ) {
			try {
				dis.releaseResources();
			} catch (IOException e) {
				// Not a lot that we can do.
			}
		}
	}

	/**
	 * This method just prints out the following info from the header:
	 * <ul>
	 * <li>is64Bit
	 * <li>phd version
	 * <li>full version
	 * <li>total objects
	 * <li>total refs
	 * </ul>
	 * @throws IOException 
	 */
	public static void main(String[] args) throws IOException {
		HeapdumpReader reader = new HeapdumpReader(args[0]);
		System.out.println("is64Bit = " + (reader.is64Bit() ? "true" : "false"));
		System.out.println("phd version = " + reader.version());
		System.out.println("full version = " + reader.full_version());
		System.out.println("total objects = " + reader.totalObjects());
		System.out.println("total refs = " + reader.totalRefs());
		reader.close();
	}
	
	/**
	 * Used to adapt data retrieval calls between two incompatible streams
	 * DataInputStream and ImageInputStream
	 * @author adam
	 *
	 */
	private class DataStreamAdapter {
		private final DataInputStream dis;
		private final ImageInputStream iis;
		
		public DataStreamAdapter(ImageInputStream iis) {
			this.iis = iis;
			dis = null;
		}
		
		public DataStreamAdapter(DataInputStream dis) {
			this.dis = dis;
			iis = null;
		}
		
		public int readInt() throws IOException {
			if(dis == null) {
				return iis.readInt();
			} else {
				return dis.readInt();
			}
		}
		
		public int readUnsignedShort() throws IOException {
			if(dis == null) {
				return iis.readUnsignedShort();
			} else {
				return dis.readUnsignedShort();
			}
		}
		
		public int readUnsignedByte() throws IOException {
			if(dis == null) {
				return iis.readUnsignedByte();
			} else {
				return dis.readUnsignedByte();
			}
		}
		
		public void mark(int readlimit) {
			if(dis == null) {
				iis.mark();		//iis mark doesn't take a parameter
			} else {
				dis.mark(readlimit);
			}
		}
		
		public void reset() throws IOException {
			if(dis == null) {
				iis.reset();
			} else {
				dis.reset();
			}
		}
		
		public long readLong() throws IOException {
			if(dis == null) {
				return iis.readLong();
			} else {
				return dis.readLong();
			}
		}
		
		public short readShort() throws IOException {
			if(dis == null) {
				return iis.readShort();
			} else {
				return dis.readShort();
			}
		}
		
		public byte readByte() throws IOException {
			if(dis == null) {
				return iis.readByte();
			} else {
				return dis.readByte();
			}
		}
		
		public void readFully(byte[] buffer) throws IOException {
			if(dis == null) {
				iis.readFully(buffer);
			} else {
				dis.readFully(buffer);
			}
		}
		
		public void close() throws IOException {
			if(dis == null) {
				//ignore and do not close the image input stream as this will be handled by the PHD Image
			} else {
				dis.close();
			}
		}
		
		//allows all input sources to be closed and is used to signal the final closing
		public void releaseResources() throws IOException {
			if(dis == null) {
				iis.close();
			} else {
				dis.close();
			}
		}
	}
}
