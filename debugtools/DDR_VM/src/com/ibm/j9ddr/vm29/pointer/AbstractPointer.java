/*
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.vm29.pointer;

import java.io.ByteArrayOutputStream;
import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.NullPointerDereference;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectContiguousPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectDiscontiguousPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectWithDataAddressContiguousPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectWithDataAddressDiscontiguousPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMFieldShapeHelper;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.UDATA;

public abstract class AbstractPointer extends DataType {
	private static int cacheSize = 32;
	private static long[] keys;
	private static J9ClassPointer[] values;
	private static int[] counts;
	private static long probes;
	private static long hits;
	static {
		initializeCache();
	}

	protected long address;

	protected AbstractPointer(long address) {
		super();
		this.address = address;

		if (4 == DataType.getProcess().bytesPerPointer()) {
			this.address = 0xFFFFFFFFL & address;
		}
	}

	public abstract AbstractPointer add(long count);

	public abstract AbstractPointer add(Scalar count);

	public abstract AbstractPointer addOffset(long offset);
	public abstract AbstractPointer addOffset(Scalar offset);

	public abstract AbstractPointer sub(long count);
	public abstract AbstractPointer sub(Scalar count);

	public abstract AbstractPointer subOffset(long offset);
	public abstract AbstractPointer subOffset(Scalar offset);

	public abstract AbstractPointer untag(long tagBits);
	public abstract AbstractPointer untag();

	public final boolean allBitsIn(long bitmask) {
		if (0 == bitmask) {
			/*
			 * In the vast majority of situations, the caller supplies a non-zero
			 * argument. However, in backwards-compatibility cases the argument
			 * may be derived from a constant which has not always been present.
			 * The behavior we want is for such tests to return false (such a bit
			 * could never have been set in core dumps generated before the constant
			 * existed).
			 */
			return false;
		}
		return bitmask == (address & bitmask);
	}

	public boolean anyBitsIn(long bitmask) {
		return 0 != (address & bitmask);
	}

	public long longValue() throws CorruptDataException {
		return address;
	}

	public abstract DataType at(long index) throws CorruptDataException;
	public abstract DataType at(Scalar index) throws CorruptDataException;

	public boolean isNull() {
		return address == 0;
	}

	public boolean notNull() {
		return address != 0;
	}

	public boolean eq(Object obj) {
		return equals(obj);
	}

	@Override
	public boolean equals(Object obj) {
		if (obj == null) {
			return false;
		}

		if (!(obj instanceof AbstractPointer)) {
			return false;
		}

		return address == ((AbstractPointer) obj).address;
	}

	@Override
	public int hashCode() {
		return (int) ((0xFFFFFFFFL & address) ^ ((0xFFFFFFFF00000000L & address) >> 32));
	}

	public long getAddress() {
		return address;
	}

	public final long nonNullAddress() throws NullPointerDereference {
		if (address == 0) {
			throw new NullPointerDereference();
		}

		return address;
	}

	public String getHexAddress() {
		return String.format("0x%0" + (UDATA.SIZEOF * 2) + "X", address);
	}

	/**
	 * This method returns the memory values at the given index.
	 * For StructurePointer instance objects, UDATA.sizeof amount of bytes are read.
	 * Otherwise, amount of bytes to be read depend on the object type.
	 * For instance, if it is I16Pointer, then 2 bytes are read.
	 *
	 * Note that this method returns bytes as they appear in the memory. It ignores endian-ization.
	 * For instance I16 value 0xABCD lays in the memory as CDAB on Little Endian platforms.
	 * And this method returns for this I16Pointer 0xCDAB and does not reverse the bytes.
	 *
	 *
	 * @param index Offset of the memory to be read.
	 * @return String representation of the value at the given index.
	 * @throws CorruptDataException
	 */
	public String hexAt(long index) throws CorruptDataException {
		int bufferSize;
		String result = "0x";
		if (this instanceof StructurePointer) {
			bufferSize = UDATA.SIZEOF;
		} else {
			bufferSize = (int)sizeOfBaseType();
		}
		byte[] buffer = new byte[bufferSize];
		getBytesAtOffset(index, buffer);
		for (int i = 0; i < buffer.length; i++) {
			result += String.format("%02X", buffer[i]);
		}
		return result;
	}

	/**
	 * This method returns the memory values at the given index.
	 * For StructurePointer instance objects, UDATA.sizeof amount of bytes are read.
	 * Otherwise, amount of bytes to be read depend on the object type.
	 * For instance, if it is I16Pointer, then 2 bytes are read.
	 *
	 *
	 * Note that this method returns bytes as they appear in the memory. It ignores endian-ization.
	 * For instance I16 value 0xABCD lays in the memory as CDAB on Little Endian platforms.
	 * And this method returns for this I16Pointer 0xCDAB and does not reverse the bytes.
	 *
	 *
	 * @param index Offset of the memory to be read.
	 * @return String representation of the value at the given index.
	 * @throws CorruptDataException
	 */
	public String hexAt(Scalar index) throws CorruptDataException {
		return hexAt(index.longValue());
	}

	/**
	 * This method reads number of the bytes depending on the pointers' base size.
	 * And if the platform is little endian, it reverses the bytes read and returns it as hex string.
	 * If the platform is big endian, then it returns the read bytes as it is as an hex string.
	 *
	 * @return hex string of the value of this pointer points.
	 * @throws CorruptDataException
	 */
	public String getHexValue() throws CorruptDataException {
		int bufferSize;
		String result = "0x";
		if (this instanceof StructurePointer) {
			bufferSize = UDATA.SIZEOF;
		} else {
			bufferSize = (int) sizeOfBaseType();
		}
		byte[] buffer = new byte[bufferSize];
		getBytesAtOffset(0, buffer);
		/* If it is little endian, then swap the bytes since memory is written in reverse order */
		if (J9BuildFlags.J9VM_ENV_LITTLE_ENDIAN) {
			for (int i = buffer.length - 1; i >= 0; i--) {
				result += String.format("%02X", buffer[i]);
			}
		} else {
			for (int i = 0; i < buffer.length; i++) {
				result += String.format("%02X", buffer[i]);
			}
		}
		return result;
	}

	protected static IProcess getAddressSpace() {
		return process;
	}

	public boolean lt(AbstractPointer pointer) {
		if (J9BuildFlags.J9VM_ENV_DATA64) {
			return new U64(address).lt(new U64(pointer.address));
		} else {
			return address < pointer.address;
		}
	}

	public boolean lte(AbstractPointer pointer) {
		if (J9BuildFlags.J9VM_ENV_DATA64) {
			return new U64(address).lte(new U64(pointer.address));
		} else {
			return address <= pointer.address;
		}
	}

	public boolean gt(AbstractPointer pointer) {
		if (J9BuildFlags.J9VM_ENV_DATA64) {
			return new U64(address).gt(new U64(pointer.address));
		} else {
			return address > pointer.address;
		}
	}

	public boolean gte(AbstractPointer pointer) {
		if (J9BuildFlags.J9VM_ENV_DATA64) {
			return new U64(address).gte(new U64(pointer.address));
		} else {
			return address >= pointer.address;
		}
	}

	public IDATA sub(AbstractPointer pointer)
	{
		if (this.getClass() != pointer.getClass()) {
			throw new UnsupportedOperationException("Cannot subtract different pointer types. This type = " + this.getClass() + ", parameter type = " + pointer.getClass());
		}

		IDATA diff = new IDATA(this.address).sub(new IDATA(pointer.address));

		return new IDATA(diff.longValue() / sizeOfBaseType());
	}

	protected abstract long sizeOfBaseType();

	public int compare(AbstractPointer pointer) {
		return address == pointer.address ? 0 : lt(pointer) ? -1 : 1;
	}

	public String toString() {
		String pointerType = getClass().getName();
		try {
<<<<<<< Upstream, based on Upstream/master
			if ((this instanceof J9ObjectPointer)
				|| (this instanceof J9IndexableObjectPointer)
				|| (this instanceof J9IndexableObjectContiguousPointer)
				|| (this instanceof J9IndexableObjectDiscontiguousPointer)
				|| (this instanceof J9IndexableObjectWithDataAddressContiguousPointer)
				|| (this instanceof J9IndexableObjectWithDataAddressDiscontiguousPointer)
			) {
=======
			if ((this instanceof J9ObjectPointer)||(this instanceof J9IndexableObjectPointer) ||
				(this instanceof J9IndexableObjectContiguousPointer) || (this instanceof J9IndexableObjectDiscontiguousPointer) ||
				(this instanceof J9IndexableObjectWithDataAddressContiguousPointer) || (this instanceof J9IndexableObjectWithDataAddressDiscontiguousPointer) ) {
>>>>>>> 10b2a40 1, DDR Changes for Off-Heap
				pointerType = J9ObjectHelper.getClassName(J9ObjectPointer.cast(this));
			}
			if (this instanceof J9ROMFieldShapePointer) {
				return J9ROMFieldShapeHelper.toString(J9ROMFieldShapePointer.cast(this));
			}
			if (address == 0) {
				return String.format("%s @ 00000", pointerType);
			} else {
				return String.format("%s @ 0x%08X %n%s", pointerType, address, getMemString(16));
			}
		} catch (CorruptDataException e) {
			return super.toString();
		}
	}

	private String getMemString(int words) {
		ByteArrayOutputStream bos = new ByteArrayOutputStream();
		PrintStream stream = new PrintStream(bos);
		dumpHex(words, stream);
		return bos.toString();
	}

	/**
	 * Debug assist method.  Display words from the core file start at this pointer.
	 * @param words
	 */
	private void dumpHex(int words, PrintStream stream) {
		if (isNull()) {
			// Prevent gratuitous MemoryFaults when debugging
			return;
		}
		stream.println();
		for (int i = 0; i < words; i++) {
			int word;
			try {
				word = getAddressSpace().getIntAt(address + (i * 4));
				if (i % 4 == 0) {
					stream.print(String.format("%08X : ", getAddress() + (i * 4)));
				}
				stream.print(String.format("%08X ", word));
				if (i % 4 == 3) {
					stream.println();
				}
			} catch (CorruptDataException e) {
				stream.print(" FAULT ");
			}
		}
		stream.println();
	}

	// Probably Nobody outside of generated code should call this
	protected long getPointerAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		return getAddressSpace().getPointerAt(address + offset);
	}

	protected int getIntAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		return getAddressSpace().getIntAt(address + offset);
	}

	protected double getDoubleAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		// On all the platforms we are about floating point bits are endian-ized to match the platform.
		long bits = getAddressSpace().getLongAt(address + offset);
		return Double.longBitsToDouble(bits);
	}

	protected float getFloatAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		// On all the platforms we are about floating point bits are endian-ized to match the platform.
		int bits = getAddressSpace().getIntAt(address + offset);
		return Float.intBitsToFloat(bits);
	}

	protected boolean getBoolAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}

		switch (SIZEOF_BOOL) {
		case 1:
			return 0 != getAddressSpace().getByteAt(address + offset);
		case 2:
			return 0 != getAddressSpace().getShortAt(address + offset);
		case 4:
			return 0 != getAddressSpace().getIntAt(address + offset);
		case 8:
			return 0 != getAddressSpace().getLongAt(address + offset);
		default:
			byte[] buffer = new byte[SIZEOF_BOOL];
			getAddressSpace().getBytesAt(address + offset, buffer);
			for (int i = 0; i < SIZEOF_BOOL; i++) {
				if (0 != buffer[i]) {
					return true;
				}
			}
			return false;
		}
	}

	protected UDATA getUDATAAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		if (J9BuildFlags.J9VM_ENV_DATA64) {
			return new UDATA(getAddressSpace().getLongAt(address + offset));
		} else {
			return new UDATA(getAddressSpace().getIntAt(address + offset));
		}
	}

	protected IDATA getIDATAAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		if (J9BuildFlags.J9VM_ENV_DATA64) {
			return new IDATA(getAddressSpace().getLongAt(address + offset));
		} else {
			return new IDATA(getAddressSpace().getIntAt(address + offset));
		}
	}

	protected short getShortAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		return getAddressSpace().getShortAt(address + offset);
	}

	protected byte getByteAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		return getAddressSpace().getByteAt(address + offset);
	}

	public int getBytesAtOffset(long offset, byte[] data) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		return getAddressSpace().getBytesAt(address + offset, data);
	}

	protected char getBaseCharAtOffset(long offset) throws CorruptDataException {
		return (char) ((int) getShortAtOffset(offset) & 0xFFFF);
	}

	protected long getLongAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		return getAddressSpace().getLongAt(address + offset);
	}

	protected J9ObjectPointer getObjectReferenceAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		if (J9ObjectHelper.compressObjectReferences) {
			return ObjectAccessBarrier.convertPointerFromToken(getAddressSpace().getIntAt(address + offset));
		} else {
			return J9ObjectPointer.cast(getAddressSpace().getPointerAt(address + offset));
		}
	}

	protected J9ClassPointer getObjectClassAtOffset(long offset) throws CorruptDataException {
		long location = address + offset;
		long classPointer;
		if (J9ObjectHelper.compressObjectReferences) {
			classPointer = (long) getAddressSpace().getIntAt(location) & 0xFFFFFFFFL;
		} else {
			classPointer = getAddressSpace().getPointerAt(location);
		}
		if (classPointer == 0L) {
			throw new MemoryFault(location, "Invalid class address found in object");
		}
		J9ClassPointer cp = checkClassCache(classPointer);
		if (cp == null) {
			cp = J9ClassPointer.cast(classPointer);
			setClassCache(classPointer, cp);
		}
		return cp;
	}

	private static J9ClassPointer checkClassCache(long pointer)
	{
		probes++;
		for (int i = 0; i < cacheSize; i++) {
			if (keys[i] == pointer) {
				hits++;
				counts[i]++;
				return values[i];
			}
		}
		return null;
	}

	private static void setClassCache(long pointer, J9ClassPointer cp)
	{
		int min = counts[0];
		int minIndex = 0;
		for (int i = 1; i < cacheSize; i++) {
			if (counts[i] < min) {
				min = counts[i];
				minIndex = i;
			}
		}
		keys[minIndex] = pointer;
		values[minIndex] = cp;
		counts[minIndex] = 1;
	}

	protected J9ObjectMonitorPointer getObjectMonitorAtOffset(long offset) throws CorruptDataException {
		if (address == 0) {
			throw new NullPointerDereference();
		}
		if (J9ObjectHelper.compressObjectReferences) {
			return J9ObjectMonitorPointer.cast(0xFFFFFFFFL & (long)(getAddressSpace().getIntAt(address + offset)));
		} else {
			return J9ObjectMonitorPointer.cast(getAddressSpace().getPointerAt(address + offset));
		}
	}

	/**
	 *
	 * @return Full DDR-interactive formatting
	 */
	public String formatFullInteractive()
	{
		return this.toString();
	}

	public String getTargetName()
	{
		String name = this.getClass().getSimpleName();

		if (name.endsWith("Pointer")) {
			name = name.substring(0, name.length() - "Pointer".length());
		}

		return name;
	}

	@Override
	public String formatShortInteractive()
	{
		String name = getTargetName();

		name = name.toLowerCase();

		return "!" + name + " 0x" + Long.toHexString(this.getAddress());
	}

	private static void initializeCache()
	{
		keys = new long[cacheSize];
		values = new J9ClassPointer[cacheSize];
		counts = new int[cacheSize];
		probes = 0;
		hits = 0;
	}

	public static void reportClassCacheStats()
	{
		double hitRate = (double)hits / (double)probes * 100.0;
		System.out.println("AbstractPointer probes: " + probes + " hit rate: " + hitRate + "%");
		initializeCache();
	}
}
