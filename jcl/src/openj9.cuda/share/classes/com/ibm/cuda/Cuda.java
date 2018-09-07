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

import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.lang.reflect.Method;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * The {@code Cuda} class provides general CUDA utilities.
 */
public final class Cuda {

	private static final class Cleaner implements Runnable {

		static final Cleaner instance;

		/**
		 * Counts the number of pinned buffers released by the cleaner thread
		 * (for use in junit tests).
		 */
		private static long releaseCount;

		static {
			instance = new Cleaner();

			Thread daemon = new Thread(instance, "CUDA pinned buffer cleaner"); //$NON-NLS-1$

			daemon.setDaemon(true);
			daemon.start();
		}

		private static native void releasePinnedBuffer(long address)
				throws CudaException;

		private final Map<Object, Long> inuse;

		private final ReferenceQueue<Object> queue;

		private Cleaner() {
			super();
			inuse = new ConcurrentHashMap<>();
			queue = new ReferenceQueue<>();
		}

		ByteBuffer insert(ByteBuffer buffer, long address) {
			inuse.put(new WeakReference<>(buffer, queue), Long.valueOf(address));

			return buffer;
		}

		@Override
		public void run() {
			for (;;) {
				try {
					Long address = inuse.remove(queue.remove());

					if (address != null) {
						releasePinnedBuffer(address.longValue());
						releaseCount = releaseCount + 1;
					}
				} catch (CudaException | InterruptedException e) {
					// ignore
				}
			}
		}
	}

	static {
		AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
			/*[USEMACROS]*/
			String version = System.getProperty("com.ibm.oti.vm.library.version", "%%MACRO@com.ibm.oti.vm.library.version%%"); //$NON-NLS-1$ //$NON-NLS-2$

			System.loadLibrary("cuda4j".concat(version)); //$NON-NLS-1$
			return null;
		});

		Method runMethod;

		try {
			runMethod = Runnable.class.getMethod("run"); //$NON-NLS-1$
		} catch (NoSuchMethodException | SecurityException e) {
			throw new RuntimeException(e);
		}

		int status = initialize(CudaException.class, runMethod);

		if (status != CudaError.Success) {
			throw new RuntimeException(getErrorMessage(status));
		}
	}

	private static native long allocatePinnedBuffer(long byteCount)
			throws CudaException;

	/**
	 * Allocates a new direct byte buffer, backed by page-locked host memory;
	 * enabling optimal performance of transfers to and from device memory.
	 * <p>
	 * The position of the returned buffer will be zero; its limit and
	 * capacity will be {@code capacity}; its order will be
	 * {@link ByteOrder#LITTLE_ENDIAN LITTLE_ENDIAN}.
	 * <p>
	 * Notes:
	 * <ul>
	 * <li>The capacity of a ByteBuffer is in general limited to {@link Integer#MAX_VALUE}
	 * - that limit also applies and is enforced here.</li>
	 * <li>Even though the result is backed by host memory, a {@code CudaException}
	 * will be thrown if the driver is not installed because registration of that
	 * host memory with the driver is integral to the behavior of this method.</li>
	 * </ul>
	 *
	 * @param capacity
	 *          the desired capacity, in bytes, of the buffer
	 * @return
	 *          the new buffer
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code capacity} is negative or larger than {@code Integer.MAX_VALUE}
	 */
	public static ByteBuffer allocatePinnedHostBuffer(long capacity)
			throws CudaException {
		if (0 <= capacity && capacity <= Integer.MAX_VALUE) {
			long address = allocatePinnedBuffer(capacity);
			ByteBuffer buffer = wrapDirectBuffer(address, capacity);

			return Cleaner.instance // <br/>
					.insert(buffer, address) // <br/>
					.order(ByteOrder.LITTLE_ENDIAN);
		}

		throw new IllegalArgumentException(String.valueOf(capacity));
	}

	/**
	 * Returns the number of CUDA-capable devices available to the Java host.
	 *
	 * @return the number of available CUDA-capable devices
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public static native int getDeviceCount() throws CudaException;

	/**
	 * Returns a number identifying the driver version.
	 * A {@code CudaException} will be thrown if the CUDA driver is not installed.
	 *
	 * @return
	 *          the driver version number
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public static native int getDriverVersion() throws CudaException;

	static native String getErrorMessage(int code);

	/**
	 * Returns a number identifying the runtime version.
	 * A {@code CudaException} will be thrown if the CUDA driver is not installed.
	 *
	 * @return
	 *          the runtime version number
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public static native int getRuntimeVersion() throws CudaException;

	private static native int initialize(Class<CudaException> exceptionClass, Method runMethod);

	static void loadNatives() {
		return;
	}

	private static native ByteBuffer wrapDirectBuffer(long address, long capacity);

	private Cuda() {
		super();
	}

}
