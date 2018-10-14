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

import java.util.Objects;
import java.util.concurrent.atomic.AtomicLong;

/**
 * The {@code CudaStream} class represents an independent queue of work for a
 * specific {@link CudaDevice}.
 * <p>
 * When no longer required, a stream must be {@code close}d.
 */
public final class CudaStream implements AutoCloseable {

	/**
	 * Default stream creation flag.
	 */
	public static final int FLAG_DEFAULT = 0;

	/**
	 * Stream creation flag requesting no implicit synchronization with the
	 * default stream.
	 */
	public static final int FLAG_NON_BLOCKING = 1;

	private static native long create(int deviceId) throws CudaException;

	private static native long createWithPriority(int deviceId, int flags,
			int priority) throws CudaException;

	private static native void destroy(int deviceId, long streamHandle)
			throws CudaException;

	private static native int getFlags(int deviceId, long streamHandle)
			throws CudaException;

	private static native int getPriority(int deviceId, long streamHandle)
			throws CudaException;

	private static native int query(int deviceId, long streamHandle);

	private static native void synchronize(int deviceId, long streamHandle)
			throws CudaException;

	private static native void waitFor(int deviceId, long streamHandle,
			long eventHandle) throws CudaException;

	final int deviceId;

	private final AtomicLong nativeHandle;

	/**
	 * Creates a new stream on the specified device, with the default flags
	 * and the default priority.
	 *
	 * @param device
	 *          the specified device
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public CudaStream(CudaDevice device) throws CudaException {
		super();
		this.deviceId = device.getDeviceId();
		this.nativeHandle = new AtomicLong(create(this.deviceId));
	}

	/**
	 * Creates a new stream on the specified device, with the specified flags
	 * and priority.
	 *
	 * @param device
	 *          the specified device
	 * @param flags
	 *          the desired flags
	 * @param priority
	 *          the desired priority
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public CudaStream(CudaDevice device, int flags, int priority)
			throws CudaException {
		super();
		this.deviceId = device.getDeviceId();
		this.nativeHandle = new AtomicLong( // <br/>
				createWithPriority(this.deviceId, flags, priority));
	}

	/**
	 * Enqueues a callback to be run after all previous work on this stream
	 * has been completed.
	 * <p>
	 * Note that the callback will occur on a distinct thread. Further, any
	 * attempts to interact with CUDA devices will fail with a CudaException
	 * with code {@link CudaError#NotPermitted}.
	 *
	 * @param callback
	 *          the runnable to be run
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public void addCallback(Runnable callback) throws CudaException {
		Objects.requireNonNull(callback);
		CudaDevice.addCallback(deviceId, getHandle(), callback);
	}

	/**
	 * Closes this stream.
	 * Any work queued on this stream will be allowed to complete: this method
	 * does not wait for that work (if any) to complete.
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	@Override
	public void close() throws CudaException {
		long handle = nativeHandle.getAndSet(0);

		if (handle != 0) {
			destroy(deviceId, handle);
		}
	}

	/**
	 * Returns the flags of this stream.
	 * @return
	 *          the flags of this stream
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this stream has been closed (see {@link #close()})
	 */
	public int getFlags() throws CudaException {
		return getFlags(deviceId, getHandle());
	}

	long getHandle() {
		long handle = nativeHandle.get();

		if (handle == 0) {
			throw new IllegalStateException();
		}

		return handle;
	}

	/**
	 * Returns the priority of this stream.
	 * @return
	 *          the priority of this stream
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this stream has been closed (see {@link #close()})
	 */
	public int getPriority() throws CudaException {
		return getPriority(deviceId, getHandle());
	}

	/**
	 * Queries the state of this stream.
	 * The common normally occurring states are:
	 * <ul>
	 * <li>CudaError.Success - stream has no pending work</li>
	 * <li>CudaError.NotReady - stream has pending work</li>
	 * </ul>
	 *
	 * @return
	 *          the state of this stream
	 * @throws IllegalStateException
	 *          if this stream has been closed (see {@link #close()})
	 */
	public int query() {
		return query(deviceId, getHandle());
	}

	/**
	 * Synchronizes with this stream.
	 * This method blocks until all work queued on this stream has completed.
	 *
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this stream has been closed (see {@link #close()})
	 */
	public void synchronize() throws CudaException {
		synchronize(deviceId, getHandle());
	}

	/**
	 * Makes all future work submitted to this stream wait for the specified
	 * event to occur.
	 *
	 * @param event
	 *          the specified event
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this stream has been closed (see {@link #close()}),
	 *          or the event has been closed (see {@link CudaEvent#close()})
	 */
	public void waitFor(CudaEvent event) throws CudaException {
		waitFor(deviceId, getHandle(), event.getHandle());
	}
}
