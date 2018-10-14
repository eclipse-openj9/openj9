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

import java.util.concurrent.atomic.AtomicLong;

/**
 * The {@code CudaEvent} class represents an event that can be queued in a
 * stream on a CUDA-capable device.
 * <p>
 * When no longer required, an event must be {@code close}d.
 */
public final class CudaEvent implements AutoCloseable {

	/** Default event creation flag. */
	public static final int FLAG_DEFAULT = 0;

	/** Use blocking synchronization. */
	public static final int FLAG_BLOCKING_SYNC = 1;

	/** Do not record timing data. */
	public static final int FLAG_DISABLE_TIMING = 2;

	/** Event is suitable for interprocess use. FLAG_DISABLE_TIMING must be set. */
	public static final int FLAG_INTERPROCESS = 4;

	private static native long create(int deviceId, int flags)
			throws CudaException;

	private static native void destroy(int deviceId, long eventHandle)
			throws CudaException;

	private static native float elapsedTimeSince(long thisHandle,
			long priorEventHandle) throws CudaException;

	private static native int query(long eventHandle);

	private static native void record(int deviceId, long streamHandle,
			long eventHandle) throws CudaException;

	private static native void synchronize(long eventHandle)
			throws CudaException;

	private final int deviceId;

	private final AtomicLong nativeHandle;

	/**
	 * Creates a new event on the specified device with default flags.
	 *
	 * @param device
	 *          the specified device
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public CudaEvent(CudaDevice device) throws CudaException {
		this(device, FLAG_DEFAULT);
	}

	/**
	 * Creates a new event on the specified device with the specified {@code flags}.
	 *
	 * @param device
	 *          the specified device
	 * @param flags
	 *          the desired flags
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public CudaEvent(CudaDevice device, int flags) throws CudaException {
		super();
		this.deviceId = device.getDeviceId();
		this.nativeHandle = new AtomicLong(create(deviceId, flags));
	}

	/**
	 * Releases resources associated with this event.
	 *
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
	 * Returns the elapsed time (in milliseconds) relative to the specified
	 * {@code priorEvent}.
	 *
	 * @param priorEvent
	 *          the prior event
	 * @return
	 *          the elapsed time (in milliseconds) between the occurrence of
	 *          the prior event and this event
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this event or the prior event has been closed (see {@link #close()})
	 */
	public float elapsedTimeSince(CudaEvent priorEvent) throws CudaException {
		return elapsedTimeSince(getHandle(), priorEvent.getHandle());
	}

	long getHandle() {
		long handle = nativeHandle.get();

		if (handle == 0) {
			throw new IllegalStateException();
		}

		return handle;
	}

	/**
	 * Queries the state of this event.
	 * The common normally occurring states are:
	 * <ul>
	 * <li>CudaError.Success - event has occurred</li>
	 * <li>CudaError.NotReady - event has not occurred</li>
	 * </ul>
	 *
	 * @return
	 *          the state of this event
	 * @throws IllegalStateException
	 *          if this event has been closed (see {@link #close()})
	 * @see CudaError
	 */
	public int query() {
		return query(getHandle());
	}

	/**
	 * Records this event on the default stream of the specified device.
	 *
	 * @param device
	 *          the specified device
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this event has been closed (see {@link #close()})
	 */
	public void record(CudaDevice device) throws CudaException {
		record(device.getDeviceId(), 0, getHandle());
	}

	/**
	 * Records this event on the specified stream.
	 *
	 * @param stream
	 *          the specified stream
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this event has been closed (see {@link #close()}),
	 *          or the stream has been closed (see {@link CudaStream#close()})
	 */
	public void record(CudaStream stream) throws CudaException {
		record(stream.deviceId, stream.getHandle(), getHandle());
	}

	/**
	 * Synchronizes on this event.
	 * This method blocks until the associated event has occurred.
	 *
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this event has been closed (see {@link #close()})
	 */
	public void synchronize() throws CudaException {
		synchronize(getHandle());
	}
}
