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

import com.ibm.cuda.CudaDevice.CacheConfig;
import com.ibm.cuda.CudaDevice.SharedMemConfig;
import com.ibm.cuda.CudaKernel.Parameters;

/**
 * The {@code CudaFunction} class represents a kernel entry point found in
 * a specific {@code CudaModule} loaded on a CUDA-capable device.
 */
public final class CudaFunction {

	/**
	 * The binary architecture version for which the function was compiled.
	 * This value is the major binary version * 10 + the minor binary version,
	 * so a binary version 1.3 function would return the value 13. Note that
	 * this will return a value of 10 for legacy cubins that do not have a
	 * properly-encoded binary architecture version.
	 */
	public static final int ATTRIBUTE_BINARY_VERSION = 6;

	/**
	 * The size in bytes of user-allocated constant memory required by this
	 * function.
	 */
	public static final int ATTRIBUTE_CONST_SIZE_BYTES = 2;

	/**
	 * The size in bytes of local memory used by each thread of this function.
	 */
	public static final int ATTRIBUTE_LOCAL_SIZE_BYTES = 3;

	/**
	 * The maximum number of threads per block, beyond which a launch of the
	 * function would fail. This number depends on both the function and the
	 * device on which the function is currently loaded.
	 */
	public static final int ATTRIBUTE_MAX_THREADS_PER_BLOCK = 0;

	/**
	 * The number of registers used by each thread of this function.
	 */
	public static final int ATTRIBUTE_NUM_REGS = 4;

	/**
	 * The PTX virtual architecture version for which the function was
	 * compiled. This value is the major PTX version * 10 + the minor PTX
	 * version, so a PTX version 1.3 function would return the value 13.
	 * Note that this may return the undefined value of 0 for cubins
	 * compiled prior to CUDA 3.0.
	 */
	public static final int ATTRIBUTE_PTX_VERSION = 5;

	/**
	 * The size in bytes of statically-allocated shared memory required by
	 * this function. This does not include dynamically-allocated shared
	 * memory requested by the user at runtime.
	 */
	public static final int ATTRIBUTE_SHARED_SIZE_BYTES = 1;

	private static native int getAttribute(int deviceId, long nativeHandle,
			int attribute) throws CudaException;

	private static native void launch(int deviceId, long functionPtr, // <br/>
			int gridDimX, int gridDimY, int gridDimZ, // <br/>
			int blockDimX, int blockDimY, int blockDimZ, // <br/>
			int sharedMemBytes, long stream, // <br/>
			long[] parameterValues) throws CudaException;

	static long nativeValueOf(Object parameter) {
		long value = 0;

		if (parameter != null) {
			Class<? extends Object> type = parameter.getClass();

			// type tests are sorted in order of decreasing (expected) likelihood
			if (type == CudaBuffer.class) {
				value = ((CudaBuffer) parameter).getAddress();
			} else if (type == Integer.class) {
				value = ((Integer) parameter).intValue();
			} else if (type == Long.class) {
				value = ((Long) parameter).longValue();
			} else if (type == Double.class) {
				value = Double.doubleToRawLongBits( // <br/>
						((Double) parameter).doubleValue());
			} else if (type == Float.class) {
				value = Float.floatToRawIntBits( // <br/>
						((Float) parameter).floatValue());
			} else if (type == Short.class) {
				value = ((Short) parameter).shortValue();
			} else if (type == Byte.class) {
				value = ((Byte) parameter).byteValue();
			} else if (type == Character.class) {
				value = ((Character) parameter).charValue();
			} else {
				throw new IllegalArgumentException();
			}
		}

		return value;
	}

	private static native void setCacheConfig(int deviceId, long nativeHandle,
			int config) throws CudaException;

	private static native void setSharedMemConfig(int deviceId,
			long nativeHandle, int config) throws CudaException;

	final int deviceId;

	private final long nativeHandle;

	CudaFunction(int deviceId, long nativeHandle) {
		super();
		this.deviceId = deviceId;
		this.nativeHandle = nativeHandle;
	}

	/**
	 * Returns the value of the specified @{code attribute}.
	 *
	 * @param attribute
	 *          the attribute to be queried (see ATTRIBUTE_XXX)
	 * @return
	 *          the attribute value
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public int getAttribute(int attribute) throws CudaException {
		return getAttribute(deviceId, nativeHandle, attribute);
	}

	void launch(CudaGrid grid, Object... parameters) throws CudaException {
		int parameterCount = parameters.length;
		long[] nativeValues = new long[parameterCount];

		for (int i = 0; i < parameterCount; ++i) {
			nativeValues[i] = nativeValueOf(parameters[i]);
		}

		CudaStream stream = grid.stream;

		launch(deviceId, nativeHandle, // <br/>
				grid.gridDimX, grid.gridDimY, grid.gridDimZ, // <br/>
				grid.blockDimX, grid.blockDimY, grid.blockDimZ, // <br/>
				grid.sharedMemBytes, // <br/>
				stream != null ? stream.getHandle() : 0, // <br/>
				nativeValues);
	}

	void launch(CudaGrid grid, Parameters parameters) throws CudaException {
		if (!parameters.isComplete()) {
			throw new IllegalArgumentException();
		}

		CudaStream stream = grid.stream;

		launch(deviceId, nativeHandle, // <br/>
				grid.gridDimX, grid.gridDimY, grid.gridDimZ, // <br/>
				grid.blockDimX, grid.blockDimY, grid.blockDimZ, // <br/>
				grid.sharedMemBytes, // <br/>
				stream != null ? stream.getHandle() : 0, // <br/>
				parameters.values);
	}

	/**
	 * Configures the cache for this function.
	 *
	 * @param config
	 *          the desired cache configuration
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public void setCacheConfig(CacheConfig config) throws CudaException {
		setCacheConfig(deviceId, nativeHandle, config.nativeValue);
	}

	/**
	 * Configures the shared memory of this function.
	 *
	 * @param config
	 *          the desired shared memory configuration
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public void setSharedMemConfig(SharedMemConfig config) throws CudaException {
		setSharedMemConfig(deviceId, nativeHandle, config.nativeValue);
	}
}
