/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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
package com.ibm.cuda;

import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.CharBuffer;
import java.nio.DoubleBuffer;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;
import java.nio.LongBuffer;
import java.nio.ShortBuffer;
import java.util.concurrent.atomic.AtomicLong;

/**
 * The {@code CudaBuffer} class represents a region of memory on a specific
 * device.
 * <p>
 * Data may be transferred between the device and the Java host via the
 * various {@code copyTo} or {@code copyFrom} methods. A buffer may be filled
 * with a specific pattern through use of one of the {@code fillXxx} methods.
 * <p>
 * When no longer required, a buffer must be {@code close}d.
 */
public final class CudaBuffer implements AutoCloseable {

	private static final ByteOrder DeviceOrder = ByteOrder.LITTLE_ENDIAN;

	private static native long allocate(int deviceId, long byteCount)
			throws CudaException;

	private static native ByteBuffer allocateDirectBuffer(long capacity);

	private static int chunkBytes(long size) {
		return size <= Integer.MAX_VALUE ? (int) size : Integer.MAX_VALUE & ~7;
	}

	private static native void copyFromDevice(int deviceId, long devicePtr,
			int sourceDeviceId, long sourceAddress, long byteCount)
			throws CudaException;

	private static native void copyFromHostByte(int deviceId, long devicePtr,
			byte[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyFromHostChar(int deviceId, long devicePtr,
			char[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyFromHostDirect(int deviceId, long devicePtr,
			Buffer source, long fromOffset, long toOffset) throws CudaException;

	private static native void copyFromHostDouble(int deviceId, long devicePtr,
			double[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyFromHostFloat(int deviceId, long devicePtr,
			float[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyFromHostInt(int deviceId, long devicePtr,
			int[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyFromHostLong(int deviceId, long devicePtr,
			long[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyFromHostShort(int deviceId, long devicePtr,
			short[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyToHostByte(int deviceId, long devicePtr,
			byte[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyToHostChar(int deviceId, long devicePtr,
			char[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyToHostDirect(int deviceId, long devicePtr,
			Buffer target, long fromOffset, long toOffset) throws CudaException;

	private static native void copyToHostDouble(int deviceId, long devicePtr,
			double[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyToHostFloat(int deviceId, long devicePtr,
			float[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyToHostInt(int deviceId, long devicePtr,
			int[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyToHostLong(int deviceId, long devicePtr,
			long[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void copyToHostShort(int deviceId, long devicePtr,
			short[] array, int fromIndex, int toIndex) throws CudaException;

	private static native void fill(int deviceId, long devicePtr,
			int elementSize, int value, long count) throws CudaException;

	private static native void freeDirectBuffer(Buffer buffer);

	private static void rangeCheck(long length, long fromIndex, long toIndex) {
		if (fromIndex > toIndex) {
			throw new IllegalArgumentException(
					"fromIndex(" + fromIndex + ") > toIndex(" + toIndex + ')'); //$NON-NLS-1$ //$NON-NLS-2$
		}

		if (fromIndex < 0) {
			throw new IndexOutOfBoundsException(String.valueOf(fromIndex));
		}

		if (toIndex > length) {
			throw new IndexOutOfBoundsException(String.valueOf(toIndex));
		}
	}

	private static native void release(int deviceId, long devicePtr)
			throws CudaException;

	private final int deviceId;

	private final AtomicLong devicePtr;

	private final long length;

	private final CudaBuffer parent;

	private CudaBuffer(CudaBuffer parent, int deviceId, long devicePtr,
			long length) {
		super();
		this.deviceId = deviceId;
		this.devicePtr = new AtomicLong(devicePtr);
		this.length = length;
		this.parent = parent;
	}

	/**
	 * Allocates a new region on the specified {@code device} of size {@code byteCount} bytes.
	 *
	 * @param device
	 *          the device on which the region is to be allocated
	 * @param byteCount
	 *          the allocation size in bytes
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public CudaBuffer(CudaDevice device, long byteCount) throws CudaException {
		super();
		this.deviceId = device.getDeviceId();
		this.devicePtr = new AtomicLong(allocate(this.deviceId, byteCount));
		this.length = byteCount;
		this.parent = null;
	}

	/**
	 * Returns a sub-region of this buffer. The new buffer begins at the
	 * specified offset and extends to the end of this buffer.
	 *
	 * @param fromOffset
	 *          the byte offset of the sub-region within this buffer
	 * @return
	 *          the specified sub-region
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the specified offset is negative or larger than the
	 *          length of this buffer
	 */
	public CudaBuffer atOffset(long fromOffset) {
		return slice(fromOffset, length);
	}

	/**
	 * Releases the region of device memory backing this buffer.
	 * <p>
	 * Closing a buffer created via {@link #atOffset(long)} with a
	 * non-zero offset has no effect: the memory is still accessible via
	 * the parent buffer which must be closed separately.
	 *
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	@Override
	public void close() throws CudaException {
		long address = devicePtr.getAndSet(0);

		if (address != 0 && parent == null) {
			release(deviceId, address);
		}
	}

	/**
	 * Copies all data from the specified {@code array} (on the Java host) to
	 * this buffer (on the device). Equivalent to
	 * <pre>
	 * copyFrom(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the source array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(byte[] array) throws CudaException {
		copyFrom(array, 0, array.length);
	}

	/**
	 * Copies data from the specified {@code array} (on the Java host) to this
	 * buffer (on the device). Elements are read from {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}
	 * storing them in the same order in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param array
	 *          the source array
	 * @param fromIndex
	 *          the source starting offset (inclusive)
	 * @param toIndex
	 *          the source ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(byte[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 0);
		copyFromHostByte(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from the specified {@code source} buffer (on the Java host)
	 * to this buffer (on the device). Elements are read from {@code source}
	 * beginning at {@code position()} continuing up to, but excluding,
	 * {@code limit()} storing them in the same order in this buffer.
	 * The {@code source} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param source
	 *          the source buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(ByteBuffer source) throws CudaException {
		final int fromIndex = source.position();
		final int toIndex = source.limit();
		final int byteCount = toIndex - fromIndex;

		lengthCheck(byteCount, 0);

		if (source.isDirect()) {
			copyFromHostDirect(deviceId, getAddress(), // <br/>
					source, fromIndex, toIndex);
		} else if (source.hasArray()) {
			final int offset = source.arrayOffset();

			copyFrom(source.array(), fromIndex + offset, toIndex + offset);
		} else {
			ByteBuffer tmp = allocateDirectBuffer(byteCount).order(DeviceOrder);

			try {
				tmp.put(source);
				copyFromHostDirect(deviceId, getAddress(), // <br/>
						tmp, 0, byteCount);
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		source.position(toIndex);
	}

	/**
	 * Copies all data from the specified {@code array} (on the Java host) to
	 * this buffer (on the device). Equivalent to
	 * <pre>
	 * copyFrom(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the source array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(char[] array) throws CudaException {
		copyFrom(array, 0, array.length);
	}

	/**
	 * Copies data from the specified {@code array} (on the Java host) to this
	 * buffer (on the device). Elements are read from {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}
	 * storing them in the same order in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param array
	 *          the source array
	 * @param fromIndex
	 *          the source starting offset (inclusive)
	 * @param toIndex
	 *          the source ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(char[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 1);
		copyFromHostChar(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from the specified {@code source} buffer (on the Java host)
	 * to this buffer (on the device). Elements are read from {@code source}
	 * beginning at {@code position()} continuing up to, but excluding,
	 * {@code limit()} storing them in the same order in this buffer.
	 * The {@code source} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param source
	 *          the source buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(CharBuffer source) throws CudaException {
		final int fromIndex = source.position();
		final int toIndex = source.limit();

		lengthCheck(toIndex - fromIndex, 1);

		if (source.isDirect()) {
			copyFromHostDirect(deviceId, getAddress(), // <br/>
					source, (long) fromIndex << 1, (long) toIndex << 1);
		} else if (source.hasArray()) {
			final int offset = source.arrayOffset();

			copyFrom(source.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 1;
			final int chunkSize = chunkBytes(byteCount);
			final CharBuffer slice = source.slice();
			final CharBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asCharBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final long end = Math.min(start + chunkSize, byteCount);

					slice.limit((int) (end >> 1));
					tmp.clear();
					tmp.put(slice);
					copyFromHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, end - start);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		source.position(toIndex);
	}

	/**
	 * Copies data from the specified {@code source} buffer (on a device)
	 * to this buffer (on the device). Elements are read from {@code source}
	 * beginning at {@code position()} continuing up to, but excluding,
	 * {@code limit()} storing them in the same order in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param source
	 *          the source buffer
	 * @param fromOffset
	 *          the source starting offset (inclusive)
	 * @param toOffset
	 *          the source ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if either {@code fromOffset} or {@code toOffset} is not a legal
	 *          offset within the source buffer or the number of source bytes
	 *          is larger than the length of this buffer
	 */
	public void copyFrom(CudaBuffer source, long fromOffset, long toOffset)
			throws CudaException {
		final long byteCount = toOffset - fromOffset;

		rangeCheck(source.length, fromOffset, toOffset);
		lengthCheck(byteCount, 0);

		copyFromDevice(deviceId, getAddress(), // <br/>
				source.deviceId, source.getAddress() + fromOffset, // <br/>
				byteCount);
	}

	/**
	 * Copies all data from the specified {@code array} (on the Java host) to
	 * this buffer (on the device). Equivalent to
	 * <pre>
	 * copyFrom(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the source array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(double[] array) throws CudaException {
		copyFrom(array, 0, array.length);
	}

	/**
	 * Copies data from the specified {@code array} (on the Java host) to this
	 * buffer (on the device). Elements are read from {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}
	 * storing them in the same order in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param array
	 *          the source array
	 * @param fromIndex
	 *          the source starting offset (inclusive)
	 * @param toIndex
	 *          the source ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(double[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 3);
		copyFromHostDouble(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from the specified {@code source} buffer (on the Java host)
	 * to this buffer (on the device). Elements are read from {@code source}
	 * beginning at {@code position()} continuing up to, but excluding,
	 * {@code limit()} storing them in the same order in this buffer.
	 * The {@code source} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param source
	 *          the source buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(DoubleBuffer source) throws CudaException {
		final int fromIndex = source.position();
		final int toIndex = source.limit();

		lengthCheck(toIndex - fromIndex, 3);

		if (source.isDirect()) {
			copyFromHostDirect(deviceId, getAddress(), // <br/>
					source, (long) fromIndex << 3, (long) toIndex << 3);
		} else if (source.hasArray()) {
			final int offset = source.arrayOffset();

			copyFrom(source.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 3;
			final int chunkSize = chunkBytes(byteCount);
			final DoubleBuffer slice = source.slice();
			final DoubleBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asDoubleBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final long end = Math.min(start + chunkSize, byteCount);

					slice.limit((int) (end >> 3));
					tmp.clear();
					tmp.put(slice);
					copyFromHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, end - start);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		source.position(toIndex);
	}

	/**
	 * Copies all data from the specified {@code array} (on the Java host) to
	 * this buffer (on the device). Equivalent to
	 * <pre>
	 * copyFrom(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the source array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(float[] array) throws CudaException {
		copyFrom(array, 0, array.length);
	}

	/**
	 * Copies data from the specified {@code array} (on the Java host) to this
	 * buffer (on the device). Elements are read from {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}
	 * storing them in the same order in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param array
	 *          the source array
	 * @param fromIndex
	 *          the source starting offset (inclusive)
	 * @param toIndex
	 *          the source ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(float[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 2);
		copyFromHostFloat(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from the specified {@code source} buffer (on the Java host)
	 * to this buffer (on the device). Elements are read from {@code source}
	 * beginning at {@code position()} continuing up to, but excluding,
	 * {@code limit()} storing them in the same order in this buffer.
	 * The {@code source} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param source
	 *          the source buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(FloatBuffer source) throws CudaException {
		final int fromIndex = source.position();
		final int toIndex = source.limit();

		lengthCheck(toIndex - fromIndex, 2);

		if (source.isDirect()) {
			copyFromHostDirect(deviceId, getAddress(), // <br/>
					source, (long) fromIndex << 2, (long) toIndex << 2);
		} else if (source.hasArray()) {
			final int offset = source.arrayOffset();

			copyFrom(source.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 2;
			final int chunkSize = chunkBytes(byteCount);
			final FloatBuffer slice = source.slice();
			final FloatBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asFloatBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final long end = Math.min(start + chunkSize, byteCount);

					slice.limit((int) (end >> 2));
					tmp.clear();
					tmp.put(slice);
					copyFromHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, end - start);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		source.position(toIndex);
	}

	/**
	 * Copies all data from the specified {@code array} (on the Java host) to
	 * this buffer (on the device). Equivalent to
	 * <pre>
	 * copyFrom(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the source array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(int[] array) throws CudaException {
		copyFrom(array, 0, array.length);
	}

	/**
	 * Copies data from the specified {@code array} (on the Java host) to this
	 * buffer (on the device). Elements are read from {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}
	 * storing them in the same order in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param array
	 *          the source array
	 * @param fromIndex
	 *          the source starting offset (inclusive)
	 * @param toIndex
	 *          the source ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(int[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 2);
		copyFromHostInt(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from the specified {@code source} buffer (on the Java host)
	 * to this buffer (on the device). Elements are read from {@code source}
	 * beginning at {@code position()} continuing up to, but excluding,
	 * {@code limit()} storing them in the same order in this buffer.
	 * The {@code source} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param source
	 *          the source buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(IntBuffer source) throws CudaException {
		final int fromIndex = source.position();
		final int toIndex = source.limit();

		lengthCheck(toIndex - fromIndex, 2);

		if (source.isDirect()) {
			copyFromHostDirect(deviceId, getAddress(), // <br/>
					source, (long) fromIndex << 2, (long) toIndex << 2);
		} else if (source.hasArray()) {
			final int offset = source.arrayOffset();

			copyFrom(source.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 2;
			final int chunkSize = chunkBytes(byteCount);
			final IntBuffer slice = source.slice();
			final IntBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asIntBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final long end = Math.min(start + chunkSize, byteCount);

					slice.limit((int) (end >> 2));
					tmp.clear();
					tmp.put(slice);
					copyFromHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, end - start);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		source.position(toIndex);
	}

	/**
	 * Copies all data from the specified {@code array} (on the Java host) to
	 * this buffer (on the device). Equivalent to
	 * <pre>
	 * copyFrom(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the source array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(long[] array) throws CudaException {
		copyFrom(array, 0, array.length);
	}

	/**
	 * Copies data from the specified {@code array} (on the Java host) to this
	 * buffer (on the device). Elements are read from {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}
	 * storing them in the same order in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param array
	 *          the source array
	 * @param fromIndex
	 *          the source starting offset (inclusive)
	 * @param toIndex
	 *          the source ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(long[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 3);
		copyFromHostLong(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from the specified {@code source} buffer (on the Java host)
	 * to this buffer (on the device). Elements are read from {@code source}
	 * beginning at {@code position()} continuing up to, but excluding,
	 * {@code limit()} storing them in the same order in this buffer.
	 * The {@code source} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param source
	 *          the source buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(LongBuffer source) throws CudaException {
		final int fromIndex = source.position();
		final int toIndex = source.limit();

		lengthCheck(toIndex - fromIndex, 3);

		if (source.isDirect()) {
			copyFromHostDirect(deviceId, getAddress(), // <br/>
					source, (long) fromIndex << 3, (long) toIndex << 3);
		} else if (source.hasArray()) {
			final int offset = source.arrayOffset();

			copyFrom(source.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 3;
			final int chunkSize = chunkBytes(byteCount);
			final LongBuffer slice = source.slice();
			final LongBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asLongBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final long end = Math.min(start + chunkSize, byteCount);

					slice.limit((int) (end >> 3));
					tmp.clear();
					tmp.put(slice);
					copyFromHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, end - start);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		source.position(toIndex);
	}

	/**
	 * Copies all data from the specified {@code array} (on the Java host) to
	 * this buffer (on the device). Equivalent to
	 * <pre>
	 * copyFrom(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the source array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(short[] array) throws CudaException {
		copyFrom(array, 0, array.length);
	}

	/**
	 * Copies data from the specified {@code array} (on the Java host) to this
	 * buffer (on the device). Elements are read from {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}
	 * storing them in the same order in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param array
	 *          the source array
	 * @param fromIndex
	 *          the source starting offset (inclusive)
	 * @param toIndex
	 *          the source ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(short[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 1);
		copyFromHostShort(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from the specified {@code source} buffer (on the Java host)
	 * to this buffer (on the device). Elements are read from {@code source}
	 * beginning at {@code position()} continuing up to, but excluding,
	 * {@code limit()} storing them in the same order in this buffer.
	 * The {@code source} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the data
	 * are to be copied somewhere other than the beginning of this buffer.
	 *
	 * @param source
	 *          the source buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyFrom(ShortBuffer source) throws CudaException {
		final int fromIndex = source.position();
		final int toIndex = source.limit();

		lengthCheck(toIndex - fromIndex, 1);

		if (source.isDirect()) {
			copyFromHostDirect(deviceId, getAddress(), // <br/>
					source, (long) fromIndex << 1, (long) toIndex << 1);
		} else if (source.hasArray()) {
			final int offset = source.arrayOffset();

			copyFrom(source.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 1;
			final int chunkSize = chunkBytes(byteCount);
			final ShortBuffer slice = source.slice();
			final ShortBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asShortBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final long end = Math.min(start + chunkSize, byteCount);

					slice.limit((int) (end >> 1));
					tmp.clear();
					tmp.put(slice);
					copyFromHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, end - start);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		source.position(toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Equivalent to
	 * <pre>
	 * copyTo(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(byte[] array) throws CudaException {
		copyTo(array, 0, array.length);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Elements are read starting at the
	 * beginning of this buffer and stored in {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param array
	 *          the destination array
	 * @param fromIndex
	 *          the destination starting offset (inclusive)
	 * @param toIndex
	 *          the destination ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(byte[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 0);
		copyToHostByte(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code target} buffer (on the Java host). Elements are read starting at
	 * the beginning of this buffer and stored in {@code target} beginning
	 * at {@code position()} continuing up to, but excluding, {@code limit()}.
	 * The {@code target} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param target
	 *          the destination buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(ByteBuffer target) throws CudaException {
		final int fromIndex = target.position();
		final int toIndex = target.limit();
		final int byteCount = toIndex - fromIndex;

		lengthCheck(byteCount, 0);

		if (target.isDirect()) {
			copyToHostDirect(deviceId, getAddress(), // <br/>
					target, fromIndex, toIndex);
		} else if (target.hasArray()) {
			final int offset = target.arrayOffset();

			copyTo(target.array(), fromIndex + offset, toIndex + offset);
		} else {
			ByteBuffer tmp = allocateDirectBuffer(byteCount).order(DeviceOrder);

			try {
				copyToHostDirect(deviceId, getAddress(), // <br/>
						tmp, 0, byteCount);
				target.put(tmp);
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		target.position(toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Equivalent to
	 * <pre>
	 * copyTo(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(char[] array) throws CudaException {
		copyTo(array, 0, array.length);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Elements are read starting at the
	 * beginning of this buffer and stored in {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param array
	 *          the destination array
	 * @param fromIndex
	 *          the destination starting offset (inclusive)
	 * @param toIndex
	 *          the destination ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(char[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 1);
		copyToHostChar(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code target} buffer (on the Java host). Elements are read starting at
	 * the beginning of this buffer and stored in {@code target} beginning
	 * at {@code position()} continuing up to, but excluding, {@code limit()}.
	 * The {@code target} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param target
	 *          the destination buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(CharBuffer target) throws CudaException {
		final int fromIndex = target.position();
		final int toIndex = target.limit();

		lengthCheck(toIndex - fromIndex, 1);

		if (target.isDirect()) {
			copyToHostDirect(deviceId, getAddress(), // <br/>
					target, (long) fromIndex << 1, (long) toIndex << 1);
		} else if (target.hasArray()) {
			final int offset = target.arrayOffset();

			copyTo(target.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 1;
			final int chunkSize = chunkBytes(byteCount);
			final CharBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asCharBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final int chunk = (int) Math.min(byteCount - start,
							chunkSize);

					tmp.position(0).limit(chunk >> 1);
					copyToHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, chunk);
					target.put(tmp);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		target.position(toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Equivalent to
	 * <pre>
	 * copyTo(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(double[] array) throws CudaException {
		copyTo(array, 0, array.length);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Elements are read starting at the
	 * beginning of this buffer and stored in {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param array
	 *          the destination array
	 * @param fromIndex
	 *          the destination starting offset (inclusive)
	 * @param toIndex
	 *          the destination ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(double[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 3);
		copyToHostDouble(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code target} buffer (on the Java host). Elements are read starting at
	 * the beginning of this buffer and stored in {@code target} beginning
	 * at {@code position()} continuing up to, but excluding, {@code limit()}.
	 * The {@code target} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param target
	 *          the destination buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(DoubleBuffer target) throws CudaException {
		final int fromIndex = target.position();
		final int toIndex = target.limit();

		lengthCheck(toIndex - fromIndex, 3);

		if (target.isDirect()) {
			copyToHostDirect(deviceId, getAddress(), // <br/>
					target, (long) fromIndex << 3, (long) toIndex << 3);
		} else if (target.hasArray()) {
			final int offset = target.arrayOffset();

			copyTo(target.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 3;
			final int chunkSize = chunkBytes(byteCount);
			final DoubleBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asDoubleBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final int chunk = (int) Math.min(byteCount - start,
							chunkSize);

					tmp.position(0).limit(chunk >> 3);
					copyToHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, chunk);
					target.put(tmp);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		target.position(toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Equivalent to
	 * <pre>
	 * copyTo(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(float[] array) throws CudaException {
		copyTo(array, 0, array.length);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Elements are read starting at the
	 * beginning of this buffer and stored in {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param array
	 *          the destination array
	 * @param fromIndex
	 *          the destination starting offset (inclusive)
	 * @param toIndex
	 *          the destination ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(float[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 2);
		copyToHostFloat(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code target} buffer (on the Java host). Elements are read starting at
	 * the beginning of this buffer and stored in {@code target} beginning
	 * at {@code position()} continuing up to, but excluding, {@code limit()}.
	 * The {@code target} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param target
	 *          the destination buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(FloatBuffer target) throws CudaException {
		final int fromIndex = target.position();
		final int toIndex = target.limit();

		lengthCheck(toIndex - fromIndex, 2);

		if (target.isDirect()) {
			copyToHostDirect(deviceId, getAddress(), // <br/>
					target, (long) fromIndex << 2, (long) toIndex << 2);
		} else if (target.hasArray()) {
			final int offset = target.arrayOffset();

			copyTo(target.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 2;
			final int chunkSize = chunkBytes(byteCount);
			final FloatBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asFloatBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final int chunk = (int) Math.min(byteCount - start,
							chunkSize);

					tmp.position(0).limit(chunk >> 2);
					copyToHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, chunk);
					target.put(tmp);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		target.position(toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Equivalent to
	 * <pre>
	 * copyTo(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(int[] array) throws CudaException {
		copyTo(array, 0, array.length);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Elements are read starting at the
	 * beginning of this buffer and stored in {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param array
	 *          the destination array
	 * @param fromIndex
	 *          the destination starting offset (inclusive)
	 * @param toIndex
	 *          the destination ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(int[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 2);
		copyToHostInt(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code target} buffer (on the Java host). Elements are read starting at
	 * the beginning of this buffer and stored in {@code target} beginning
	 * at {@code position()} continuing up to, but excluding, {@code limit()}.
	 * The {@code target} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param target
	 *          the destination buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(IntBuffer target) throws CudaException {
		final int fromIndex = target.position();
		final int toIndex = target.limit();

		lengthCheck(toIndex - fromIndex, 2);

		if (target.isDirect()) {
			copyToHostDirect(deviceId, getAddress(), // <br/>
					target, (long) fromIndex << 2, (long) toIndex << 2);
		} else if (target.hasArray()) {
			final int offset = target.arrayOffset();

			copyTo(target.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 2;
			final int chunkSize = chunkBytes(byteCount);
			final IntBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asIntBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final int chunk = (int) Math.min(byteCount - start,
							chunkSize);

					tmp.position(0).limit(chunk >> 2);
					copyToHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, chunk);
					target.put(tmp);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		target.position(toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Equivalent to
	 * <pre>
	 * copyTo(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(long[] array) throws CudaException {
		copyTo(array, 0, array.length);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Elements are read starting at the
	 * beginning of this buffer and stored in {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param array
	 *          the destination array
	 * @param fromIndex
	 *          the destination starting offset (inclusive)
	 * @param toIndex
	 *          the destination ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(long[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 3);
		copyToHostLong(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code target} buffer (on the Java host). Elements are read starting at
	 * the beginning of this buffer and stored in {@code target} beginning
	 * at {@code position()} continuing up to, but excluding, {@code limit()}.
	 * The {@code target} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param target
	 *          the destination buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(LongBuffer target) throws CudaException {
		final int fromIndex = target.position();
		final int toIndex = target.limit();

		lengthCheck(toIndex - fromIndex, 3);

		if (target.isDirect()) {
			copyToHostDirect(deviceId, getAddress(), // <br/>
					target, (long) fromIndex << 3, (long) toIndex << 3);
		} else if (target.hasArray()) {
			final int offset = target.arrayOffset();

			copyTo(target.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 3;
			final int chunkSize = chunkBytes(byteCount);
			final LongBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asLongBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final int chunk = (int) Math.min(byteCount - start,
							chunkSize);

					tmp.position(0).limit(chunk >> 3);
					copyToHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, chunk);
					target.put(tmp);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		target.position(toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Equivalent to
	 * <pre>
	 * copyTo(array, 0, array.length);
	 * </pre>
	 *
	 * @param array
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(short[] array) throws CudaException {
		copyTo(array, 0, array.length);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code array} (on the Java host). Elements are read starting at the
	 * beginning of this buffer and stored in {@code array} beginning
	 * at {@code fromIndex} continuing up to, but excluding, {@code toIndex}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param array
	 *          the destination array
	 * @param fromIndex
	 *          the destination starting offset (inclusive)
	 * @param toIndex
	 *          the destination ending offset (exclusive)
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code fromIndex > toIndex}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromIndex} is negative, {@code toIndex > array.length},
	 *          or the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(short[] array, int fromIndex, int toIndex)
			throws CudaException {
		rangeCheck(array.length, fromIndex, toIndex);
		lengthCheck(toIndex - fromIndex, 1);
		copyToHostShort(deviceId, getAddress(), // <br/>
				array, fromIndex, toIndex);
	}

	/**
	 * Copies data from this buffer (on the device) to the specified
	 * {@code target} buffer (on the Java host). Elements are read starting at
	 * the beginning of this buffer and stored in {@code target} beginning
	 * at {@code position()} continuing up to, but excluding, {@code limit()}.
	 * The {@code target} buffer position is set to {@code limit()}.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the source
	 * data are not located at the beginning of this buffer.
	 *
	 * @param target
	 *          the destination buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the number of required source bytes is larger than the length
	 *          of this buffer
	 */
	public void copyTo(ShortBuffer target) throws CudaException {
		final int fromIndex = target.position();
		final int toIndex = target.limit();

		lengthCheck(toIndex - fromIndex, 1);

		if (target.isDirect()) {
			copyToHostDirect(deviceId, getAddress(), // <br/>
					target, (long) fromIndex << 1, (long) toIndex << 1);
		} else if (target.hasArray()) {
			final int offset = target.arrayOffset();

			copyTo(target.array(), fromIndex + offset, toIndex + offset);
		} else {
			final long byteCount = (long) (toIndex - fromIndex) << 1;
			final int chunkSize = chunkBytes(byteCount);
			final ShortBuffer tmp = allocateDirectBuffer(chunkSize) // <br/>
					.order(DeviceOrder).asShortBuffer();

			try {
				for (long start = 0; start < byteCount; start += chunkSize) {
					final int chunk = (int) Math.min(byteCount - start,
							chunkSize);

					tmp.position(0).limit(chunk >> 1);
					copyToHostDirect(deviceId, getAddress() + start, // <br/>
							tmp, 0, chunk);
					target.put(tmp);
				}
			} finally {
				freeDirectBuffer(tmp);
			}
		}

		target.position(toIndex);
	}

	/**
	 * Stores {@code count} copies of {@code value} in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the values
	 * are to be stored somewhere other than the beginning of this buffer.
	 *
	 * @param value
	 *          the destination array
	 * @param count
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the space required is larger than the length of this buffer
	 */
	public void fillByte(byte value, long count) throws CudaException {
		lengthCheck(count, 0);
		fill(deviceId, getAddress(), Byte.SIZE / 8, value, count);
	}

	/**
	 * Stores {@code count} copies of {@code value} in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the values
	 * are to be stored somewhere other than the beginning of this buffer.
	 *
	 * @param value
	 *          the destination array
	 * @param count
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the space required is larger than the length of this buffer
	 */
	public void fillChar(char value, long count) throws CudaException {
		lengthCheck(count, 1);
		fill(deviceId, getAddress(), Character.SIZE / 8, value, count);
	}

	/**
	 * Stores {@code count} copies of {@code value} in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the values
	 * are to be stored somewhere other than the beginning of this buffer.
	 *
	 * @param value
	 *          the destination array
	 * @param count
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the space required is larger than the length of this buffer
	 */
	public void fillFloat(float value, long count) throws CudaException {
		fillInt(Float.floatToRawIntBits(value), count);
	}

	/**
	 * Stores {@code count} copies of {@code value} in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the values
	 * are to be stored somewhere other than the beginning of this buffer.
	 *
	 * @param value
	 *          the destination array
	 * @param count
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the space required is larger than the length of this buffer
	 */
	public void fillInt(int value, long count) throws CudaException {
		lengthCheck(count, 2);
		fill(deviceId, getAddress(), Integer.SIZE / 8, value, count);
	}

	/**
	 * Stores {@code count} copies of {@code value} in this buffer.
	 * <p>
	 * A sub-buffer may be created (see {@link #atOffset(long)}) when the values
	 * are to be stored somewhere other than the beginning of this buffer.
	 *
	 * @param value
	 *          the destination array
	 * @param count
	 *          the destination array
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if the space required is larger than the length of this buffer
	 */
	public void fillShort(short value, long count) throws CudaException {
		lengthCheck(count, 1);
		fill(deviceId, getAddress(), Short.SIZE / 8, value, count);
	}

	long getAddress() {
		long address = devicePtr.get();

		if (address != 0) {
			if (parent == null || parent.devicePtr.get() != 0) {
				return address;
			}

			// the parent buffer has been closed making this buffer invalid
			devicePtr.set(0);
		}

		throw new IllegalStateException();
	}

	/**
	 * Returns the length in bytes of this buffer.
	 *
	 * @return
	 *          the length in bytes of this buffer
	 */
	public long getLength() {
		return length;
	}

	private void lengthCheck(long elementCount, int logUnitSize) {
		if (!(0 <= elementCount && elementCount <= (length >> logUnitSize))) {
			throw new IndexOutOfBoundsException(String.valueOf(elementCount));
		}
	}

	/**
	 * Returns a sub-region of this buffer. The new buffer begins at the
	 * specified fromOffset and extends to the specified toOffset (exclusive).
	 *
	 * @param fromOffset
	 *          the byte offset of the start of the sub-region within this buffer
	 * @param toOffset
	 *          the byte offset of the end of the sub-region within this buffer
	 * @return
	 *          the specified sub-region
	 * @throws IllegalArgumentException
	 *          if {@code fromOffset > toOffset}
	 * @throws IllegalStateException
	 *          if this buffer has been closed (see {@link #close()})
	 * @throws IndexOutOfBoundsException
	 *          if {@code fromOffset} is negative, {@code toOffset > length},
	 *          or the number of source bytes is larger than the length
	 *          of this buffer
	 */
	public CudaBuffer slice(long fromOffset, long toOffset) {
		if (fromOffset == 0 && toOffset == length) {
			return this;
		} else {
			rangeCheck(length, fromOffset, toOffset);

			return new CudaBuffer( // <br/>
					parent != null ? parent : this, // <br/>
					deviceId, // <br/>
					getAddress() + fromOffset, // <br/>
					toOffset - fromOffset);
		}
	}
}
