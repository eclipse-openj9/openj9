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

import java.io.IOException;
import java.io.InputStream;
import java.util.concurrent.atomic.AtomicLong;

import com.ibm.cuda.internal.CudaUtil;

/**
 * The {@code CudaLinker} class supports combining one or more code fragments
 * to form a module that can be then loaded on a CUDA-capable device.
 * <p>
 * When no longer required, a linker must be destroyed (see {@link #destroy()}).
 */
public final class CudaLinker {

	private static native void add(int deviceId, long linkStateHandle,
			int inputType, byte[] data, String name, long optionsHandle)
			throws CudaException;

	private static native byte[] complete(int deviceId, long linkStateHandle)
			throws CudaException;

	private static native long create(int deviceId, long optionsHandle)
			throws CudaException;

	private static native void destroy(int deviceId, long linkStateHandle)
			throws CudaException;

	private final CudaJitOptions createOptions;

	private final int deviceId;

	private final AtomicLong nativeHandle;

	/**
	 * Creates a new linker for the specified {@code device}
	 * using default options.
	 *
	 * @param device
	 *          the device on which the resulting module is to be loaded
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public CudaLinker(CudaDevice device) throws CudaException {
		this(device, null);
	}

	/**
	 * Creates a new linker for the specified {@code device}
	 * using the specified {@code options}.
	 *
	 * @param device
	 *          the device on which the resulting module is to be loaded
	 * @param options
	 *          the desired options, or null for the default options
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public CudaLinker(CudaDevice device, CudaJitOptions options)
			throws CudaException {
		super();
		this.createOptions = options;
		this.deviceId = device.getDeviceId();
		this.nativeHandle = new AtomicLong( // <br/>
				create(deviceId, options == null ? 0 : options.getHandle()));
	}

	/**
	 * Adds a new code fragment to be linked into the module under construction
	 * using the default options.
	 *
	 * @param type
	 *          the type of input data
	 * @param data
	 *          the content of the new code fragment
	 * @param name
	 *          the name to be used in log messages in reference to this code fragment
	 * @return
	 *          this linker object
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this linker has been destroyed (see {@link #destroy()})
	 */
	public CudaLinker add(CudaJitInputType type, byte[] data, String name)
			throws CudaException {
		return add(type, data, name, null);
	}

	/**
	 * Adds a new code fragment to be linked into the module under construction
	 * using the specified options.
	 *
	 * @param type
	 *          the type of input data
	 * @param data
	 *          the content of the new code fragment
	 * @param name
	 *          the name to be used in log messages in reference to this code fragment
	 * @param options
	 *          the desired options
	 * @return
	 *          this linker object
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this linker has been destroyed (see {@link #destroy()})
	 */
	public CudaLinker add(CudaJitInputType type, byte[] data, String name,
			CudaJitOptions options) throws CudaException {
		if (type == null) {
			throw new NullPointerException();
		}

		if (data == null) {
			throw new NullPointerException();
		}

		long handle = getHandle();

		if (options == null) {
			add(deviceId, handle, type.nativeValue, data, name, 0);
		} else {
			try {
				add(deviceId, handle, type.nativeValue, data, name, options.getHandle());
			} finally {
				options.releaseHandle(true);
			}
		}

		if (createOptions != null) {
			createOptions.update();
		}

		return this;
	}

	/**
	 * Adds a new code fragment to be linked into the module under construction
	 * using the default options.
	 *
	 * @param type
	 *          the type of input data
	 * @param input
	 *          the content of the new code fragment
	 * @param name
	 *          the name to be used in log messages in reference to this code fragment
	 * @return
	 *          this linker object
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this linker has been destroyed (see {@link #destroy()})
	 * @throws IOException
	 *          if an I/O error occurs reading {@code input}
	 */
	public CudaLinker add(CudaJitInputType type, InputStream input, String name)
			throws CudaException, IOException {
		return add(type, input, name, null);
	}

	/**
	 * Adds a new code fragment to be linked into the module under construction
	 * using the default options.
	 *
	 * @param type
	 *          the type of input data
	 * @param input
	 *          the content of the new code fragment
	 * @param name
	 *          the name to be used in log messages in reference to this code fragment
	 * @param options
	 *          the desired options
	 * @return
	 *          this linker object
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this linker has been destroyed (see {@link #destroy()})
	 * @throws IOException
	 *          if an I/O error occurs reading {@code input}
	 */
	public CudaLinker add(CudaJitInputType type, InputStream input,
			String name, CudaJitOptions options) throws CudaException,
			IOException {
		return add(type, CudaUtil.read(input, true), name, options);
	}

	/**
	 * Completes the module under construction and return an image suitable
	 * for loading.
	 *
	 * @return
	 *          the image suitable for loading
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalStateException
	 *          if this linker has been destroyed (see {@link #destroy()})
	 */
	public byte[] complete() throws CudaException {
		return complete(deviceId, getHandle());
	}

	/**
	 * Destroys this linker, releasing associated resources.
	 *
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public void destroy() throws CudaException {
		long handle = nativeHandle.getAndSet(0);

		if (handle != 0) {
			destroy(deviceId, handle);
		}

		if (createOptions != null) {
			createOptions.releaseHandle(false);
		}
	}

	/**
	 * Returns the contents of the error log.
	 * <p>
	 * The result will be empty unless this linker was created with options
	 * which specified a positive error log buffer size
	 * (see {@link CudaJitOptions#setErrorLogBufferSize(int)})
	 * and errors were reported.
	 *
	 * @return
	 *          the contents of the error log
	 */
	public String getErrorLogBuffer() {
		if (createOptions == null) {
			return ""; //$NON-NLS-1$
		} else {
			return createOptions.getErrorLogBuffer();
		}
	}

	private long getHandle() {
		long handle = nativeHandle.get();

		if (handle == 0) {
			throw new IllegalStateException();
		}

		return handle;
	}

	/**
	 * Returns the contents of the information log.
	 * <p>
	 * The result will be empty unless this linker was created with options
	 * which specified a positive information log buffer size
	 * (see {@link CudaJitOptions#setInfoLogBufferSize(int)})
	 * and informational messages were reported.
	 *
	 * @return
	 *          the contents of the information log
	 */
	public String getInfoLogBuffer() {
		if (createOptions == null) {
			return ""; //$NON-NLS-1$
		} else {
			return createOptions.getInfoLogBuffer();
		}
	}

	/**
	 * Answers the total elapsed time, in milliseconds,
	 * spent in the compiler and linker.
	 * <p>
	 * Applies to: compiler and linker.
	 *
	 * @return
	 *          the total elapsed time, in milliseconds, spent in the compiler and linker
	 */
	public float getWallTime() {
		if (createOptions == null) {
			return 0.0f;
		} else {
			return createOptions.getWallTime();
		}
	}
}
