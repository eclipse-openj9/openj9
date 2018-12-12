/*[INCLUDE-IF Sidecar17]*/
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
package com.ibm.gpu;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Queue;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

import com.ibm.cuda.CudaBuffer;
import com.ibm.cuda.CudaDevice;
import com.ibm.cuda.CudaException;
import com.ibm.cuda.CudaGrid;
import com.ibm.cuda.CudaKernel;
import com.ibm.cuda.CudaModule;
import com.ibm.cuda.CudaStream;
import com.ibm.cuda.Dim3;

/**
 * This class provides access to a GPU implementation of the bitonic sort
 * network.
 */
final class SortNetwork {

	private static final class LoadKey {

		final int deviceId;

		final char type;

		LoadKey(int deviceId, char type) {
			super();
			this.deviceId = deviceId;
			this.type = type;
		}

		@Override
		public boolean equals(Object object) {
			if (object instanceof LoadKey) {
				LoadKey that = (LoadKey) object;

				if (this.deviceId == that.deviceId && this.type == that.type) {
					return true;
				}
			}

			return false;
		}

		@Override
		public int hashCode() {
			return (deviceId << 4) ^ type;
		}

	}

	private static final class LoadResult {

		static LoadResult create(LoadKey key) {
			try {
				CudaDevice device = new CudaDevice(key.deviceId);
				int capability = device.getAttribute(CudaDevice.ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR);

				if (capability < 2) {
					return failure("Unsupported device"); //$NON-NLS-1$
				}

				// Allocate a sufficiently large buffer up front to avoid the need
				// for it to grow (all current variants are less than 48kB).
				ByteArrayOutputStream ptxBuffer = new ByteArrayOutputStream(48 * 1024);

				PtxKernelGenerator.writeTo(capability, key.type, ptxBuffer);

				// PTX code must be NUL-terminated.
				ptxBuffer.write(0);

				byte[] ptxCode = ptxBuffer.toByteArray();

				PrivilegedAction<LoadResult> loader = () -> load(device, ptxCode);

				// we assert privilege to load a module
				return AccessController.doPrivileged(loader);
			} catch (CudaException | IOException e) {
				return failure(e);
			}
		}

		private static LoadResult failure(Exception exception) {
			return new LoadResult(exception);
		}

		private static LoadResult failure(String problem) {
			return new LoadResult(problem);
		}

		private static LoadResult load(CudaDevice device, byte[] ptxCode) {
			LoadResult result;

			try {
				CudaModule module = null;

				try {
					module = new CudaModule(device, ptxCode);
					result = success(new SortNetwork(device, module));
					ShutdownHook.unloadOnShutdown(module);
					module = null;
				} finally {
					if (module != null) {
						module.unload();
					}
				}
			} catch (CudaException e) {
				result = failure(e);
			}

			return result;
		}

		private static LoadResult success(SortNetwork network) {
			return new LoadResult(network);
		}

		private final SortNetwork network;

		private final String problem;

		private LoadResult(Exception exception) {
			this(exception.getLocalizedMessage());
		}

		private LoadResult(SortNetwork network) {
			super();
			this.network = network;
			this.problem = null;
		}

		private LoadResult(String problem) {
			super();
			this.network = null;
			this.problem = problem;
		}

		SortNetwork get() throws GPUConfigurationException {
			if (problem != null) {
				throw new GPUConfigurationException(problem);
			}

			return network;
		}

	}

	/**
	 * Unloads modules on VM shutdown.
	 */
	private static final class ShutdownHook extends Thread {

		private static final Queue<CudaModule> modules;

		static {
			modules = new ConcurrentLinkedQueue<>();

			AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
				Runtime.getRuntime().addShutdownHook(new ShutdownHook());
				return null;
			});
		}

		public static void unloadOnShutdown(CudaModule module) {
			modules.add(module);
		}

		private ShutdownHook() {
			super("GPU sort shutdown helper"); //$NON-NLS-1$
		}

		@Override
		public void run() {
			for (;;) {
				CudaModule module = modules.poll();

				if (module == null) {
					break;
				}

				try {
					module.unload();
				} catch (CudaException e) {
					// ignore
				}
			}
		}

	}

	private static final Integer[] powersOf2;

	private static final ConcurrentHashMap<LoadKey, LoadResult> resultsMap;

	static {
		final int phaseCount = 31;
		final Integer[] powers = new Integer[phaseCount];

		for (int i = 0; i < phaseCount; ++i) {
			powers[i] = Integer.valueOf(1 << i);
		}

		powersOf2 = powers;
		resultsMap = new ConcurrentHashMap<>();
	}

	private static void checkIndices(int length, int fromIndex, int toIndex) {
		if (fromIndex > toIndex) {
			throw new IllegalArgumentException();
		}

		if (fromIndex < 0) {
			throw new ArrayIndexOutOfBoundsException(fromIndex);
		}

		if (toIndex > length) {
			throw new ArrayIndexOutOfBoundsException(toIndex);
		}
	}

	private static SortNetwork load(int deviceId, char type) throws GPUConfigurationException {
		LoadKey key = new LoadKey(deviceId, type);

		return resultsMap.computeIfAbsent(key, LoadResult::create).get();
	}

	private static int roundUp(int value, int unit) {
		assert value > 0;
		assert unit > 0;

		int remainder = value % unit;

		return remainder == 0 ? value : (value + (unit - remainder));
	}

	private static int significantBits(int value) {
		return 32 - Integer.numberOfLeadingZeros(Math.max(1, value));
	}

	/**
	 * Sort a specified portion of the given array of doubles into ascending
	 * order, using the CUDA device with the given device number
	 *
	 * @param deviceId  device number associated with a CUDA device
	 * @param array  the array to be sorted
	 * @param fromIndex  starting index of the sort
	 * @param toIndex  ending index of the sort
	 * @throws GPUConfigurationException
	 * @throws GPUSortException
	 */
	static void sortArray(int deviceId, double[] array, int fromIndex,
			int toIndex) throws GPUConfigurationException, GPUSortException {
		CUDAManager manager = traceStart(deviceId, "double", fromIndex, toIndex); //$NON-NLS-1$

		try {
			SortNetwork network = load(deviceId, 'D');

			network.sort(array, fromIndex, toIndex);
		} catch (GPUConfigurationException | GPUSortException e) {
			traceFailure(manager, e);
			throw e;
		}

		traceSuccess(manager, deviceId, "double"); //$NON-NLS-1$
	}

	/**
	 * Sort a specified portion of the given array of floats into ascending
	 * order, using the CUDA device with the given device number
	 *
	 * @param deviceId  device number associated with a CUDA device
	 * @param array  the array to be sorted
	 * @param fromIndex  starting index of the sort
	 * @param toIndex  ending index of the sort
	 * @throws GPUConfigurationException
	 * @throws GPUSortException
	 */
	static void sortArray(int deviceId, float[] array, int fromIndex,
			int toIndex) throws GPUConfigurationException, GPUSortException {
		CUDAManager manager = traceStart(deviceId, "float", fromIndex, toIndex); //$NON-NLS-1$

		try {
			SortNetwork network = load(deviceId, 'F');

			network.sort(array, fromIndex, toIndex);
		} catch (GPUConfigurationException | GPUSortException e) {
			traceFailure(manager, e);
			throw e;
		}

		traceSuccess(manager, deviceId, "float"); //$NON-NLS-1$
	}

	/**
	 * Sort a specified portion of the given array of integers into ascending
	 * order, using the CUDA device with the given device number
	 *
	 * @param deviceId  device number associated with a CUDA device
	 * @param array  the array to be sorted
	 * @param fromIndex  starting index of the sort
	 * @param toIndex  ending index of the sort
	 * @throws GPUConfigurationException
	 * @throws GPUSortException
	 */
	static void sortArray(int deviceId, int[] array, int fromIndex,
			int toIndex) throws GPUConfigurationException, GPUSortException {
		CUDAManager manager = traceStart(deviceId, "int", fromIndex, toIndex); //$NON-NLS-1$

		try {
			SortNetwork network = load(deviceId, 'I');

			network.sort(array, fromIndex, toIndex);
		} catch (GPUConfigurationException | GPUSortException e) {
			traceFailure(manager, e);
			throw e;
		}

		traceSuccess(manager, deviceId, "int"); //$NON-NLS-1$
	}

	/**
	 * Sort a specified portion of the given array of longs into ascending
	 * order, using the CUDA device with the given device number
	 *
	 * @param deviceId  device number associated with a CUDA device
	 * @param array  the array to be sorted
	 * @param fromIndex  starting index of the sort
	 * @param toIndex  ending index of the sort
	 * @throws GPUConfigurationException
	 * @throws GPUSortException
	 */
	static void sortArray(int deviceId, long[] array, int fromIndex,
			int toIndex) throws GPUConfigurationException, GPUSortException {
		CUDAManager manager = traceStart(deviceId, "long", fromIndex, toIndex); //$NON-NLS-1$

		try {
			SortNetwork network = load(deviceId, 'J');

			network.sort(array, fromIndex, toIndex);
		} catch (GPUConfigurationException | GPUSortException e) {
			traceFailure(manager, e);
			throw e;
		}

		traceSuccess(manager, deviceId, "long"); //$NON-NLS-1$
	}

	private static void traceFailure(CUDAManager manager, Exception exception) {
		manager.outputIfVerbose(exception.getLocalizedMessage());
	}

	@SuppressWarnings("nls")
	private static CUDAManager traceStart(int deviceId, String type, int fromIndex, int toIndex) {
		CUDAManager manager = CUDAManager.instanceInternal();

		if (manager.getVerboseGPUOutput()) {
			manager.outputIfVerbose("Using device: " + deviceId + " to sort " + type
					+ " array; elements " + fromIndex + " to " + toIndex);
		}

		return manager;
	}

	@SuppressWarnings("nls")
	private static void traceSuccess(CUDAManager manager, int deviceId, String type) {
		if (manager.getVerboseGPUOutput()) {
			manager.outputIfVerbose("Sorted " + type + "s on device " + deviceId + " successfully");
		}
	}

	private final CudaDevice device;

	private final int maxGridDimX;

	private final CudaKernel sortFirst4;

	private final CudaKernel sortOther1;

	private final CudaKernel sortOther2;

	private final CudaKernel sortOther3;

	private final CudaKernel sortOther4;

	private final CudaKernel sortPhase9;

	/**
	 * Initialize a new SortNetwork for the specified device.
	 *
	 * @param deviceId
	 * @param elementType
	 * @throws CudaException
	 */
	SortNetwork(CudaDevice device, CudaModule module) throws CudaException {
		super();
		this.device = device;
		this.maxGridDimX = device.getAttribute(CudaDevice.ATTRIBUTE_MAX_GRID_DIM_X);
		this.sortFirst4 = new CudaKernel(module, "first4"); //$NON-NLS-1$
		this.sortOther1 = new CudaKernel(module, "other1"); //$NON-NLS-1$
		this.sortOther2 = new CudaKernel(module, "other2"); //$NON-NLS-1$
		this.sortOther3 = new CudaKernel(module, "other3"); //$NON-NLS-1$
		this.sortOther4 = new CudaKernel(module, "other4"); //$NON-NLS-1$
		this.sortPhase9 = new CudaKernel(module, "phase9"); //$NON-NLS-1$
	}

	private CudaGrid makeGrid(int threadCount, int blockSize, CudaStream stream) {
		int blockCount = Math.max(1, (threadCount + blockSize - 1) / blockSize);

		return new CudaGrid(makeGridDim(blockCount), new Dim3(blockSize), stream);
	}

	private Dim3 makeGridDim(int blockCount) {
		// This method maintains invariant: (blockDimX * blockDimY >= blockCount).
		int blockDimX = Math.max(1, blockCount);
		int blockDimY = 1;

		// This loop is only entered for devices with compute capability < 3.0
		// (where maxGridDimX is 0xFFFF).
		// Because blockCount is at most 0x40_0000 (Integer.MAX_INT / 512 + 1),
		// the loop body will execute at most 7 times leaving (blockDimY <= 0x80).
		while (blockDimX > maxGridDimX) {
			if ((blockDimX & 1) != 0) {
				blockDimX += 1; // round up
			}

			blockDimX >>= 1;
			blockDimY <<= 1;
		}

		return new Dim3(blockDimX, blockDimY);
	}

	private void sort(double[] array, int fromIndex, int toIndex)
			throws GPUSortException {
		final int length = toIndex - fromIndex;

		if (length < 2) {
			checkIndices(array.length, fromIndex, toIndex);
			return;
		}

		try (CudaBuffer gpuBuffer = new CudaBuffer(device, length * (long) Double.BYTES)) {
			gpuBuffer.copyFrom(array, fromIndex, toIndex);
			sortBuffer(gpuBuffer, length);
			gpuBuffer.copyTo(array, fromIndex, toIndex);
		} catch (CudaException e) {
			throw new GPUSortException(e.getLocalizedMessage(), e);
		}
	}

	private void sort(float[] array, int fromIndex, int toIndex)
			throws GPUSortException {
		final int length = toIndex - fromIndex;

		if (length < 2) {
			checkIndices(array.length, fromIndex, toIndex);
			return;
		}

		try (CudaBuffer gpuBuffer = new CudaBuffer(device, length * (long) Float.BYTES)) {
			gpuBuffer.copyFrom(array, fromIndex, toIndex);
			sortBuffer(gpuBuffer, length);
			gpuBuffer.copyTo(array, fromIndex, toIndex);
		} catch (CudaException e) {
			throw new GPUSortException(e.getLocalizedMessage(), e);
		}
	}

	private void sort(int[] array, int fromIndex, int toIndex)
			throws GPUSortException {
		final int length = toIndex - fromIndex;

		if (length < 2) {
			checkIndices(array.length, fromIndex, toIndex);
			return;
		}

		try (CudaBuffer gpuBuffer = new CudaBuffer(device, length * (long) Integer.BYTES)) {
			gpuBuffer.copyFrom(array, fromIndex, toIndex);
			sortBuffer(gpuBuffer, length);
			gpuBuffer.copyTo(array, fromIndex, toIndex);
		} catch (CudaException e) {
			throw new GPUSortException(e.getLocalizedMessage(), e);
		}
	}

	/**
	 * Sets up and runs the kernels used to sort LONG arrays
	 *
	 * @param array  array to be sorted
	 * @param fromIndex  starting index of sort
	 * @param toIndex  ending index of sort
	 * @throws GPUSortException
	 */
	private void sort(long[] array, int fromIndex, int toIndex)
			throws GPUSortException {
		final int length = toIndex - fromIndex;

		if (length < 2) {
			checkIndices(array.length, fromIndex, toIndex);
			return;
		}

		try (CudaBuffer gpuBuffer = new CudaBuffer(device, length * (long) Long.BYTES)) {
			gpuBuffer.copyFrom(array, fromIndex, toIndex);
			sortBuffer(gpuBuffer, length);
			gpuBuffer.copyTo(array, fromIndex, toIndex);
		} catch (CudaException e) {
			throw new GPUSortException(e.getLocalizedMessage(), e);
		}
	}

	/**
	 * Determines grid dimensions for each kernel based on the length
	 * of the array, and then launches kernels in a bitonic sort
	 * pattern.
	 *
	 * @param buffer  array to be sorted
	 * @param length  the number of elements to be sorted
	 * @throws CudaException
	 */
	private void sortBuffer(CudaBuffer buffer, int length) throws CudaException {
		try (CudaStream stream = new CudaStream(device)) {
			final Integer boxLength = Integer.valueOf(length);

			// launch one kernel for phases 0-8 regardless of size
			{
				/*
				 * In this kernel, each block of threads processes 512 (= 1<<9) elements of data.
				 * Because each thread currently only processes one pair of elements each, we want
				 * 256 threads per block.
				 *
				 * TODO Experiment to discover an optimal thread block size.
				 */
				final int phaseCount = 9;
				final int inputSize = 1 << phaseCount;
				final int blockSize = inputSize >> 1;
				final CudaGrid grid = makeGrid(length >> 1, blockSize, stream);

				sortPhase9.launch(grid, buffer, boxLength);
			}

			final int phaseCount = significantBits(length - 1);

			if (phaseCount <= 9) {
				return;
			}

			/*
			 * The remaining kernels take three parameters:
			 *   0: array to be sorted
			 *   1: length of array
			 *   2: stride
			 */

			final int blockSize = 256;
			final CudaGrid gridOther = makeGrid(length >> 1, blockSize, stream);

			for (int phase = 9; phase < phaseCount; ++phase) {
				// phases 9 and later all begin with sortFirst4
				{
					/*
					 * The number of threads required here is a multiple of (1 << phase)
					 * to process the two input blocks each of which consists
					 * of (1 << phase) elements.
					 */
					final int granule = 1 << phase;
					final int grains = roundUp(length, granule);
					final CudaGrid grid = makeGrid(grains >> 1, blockSize, stream);

					sortFirst4.launch(grid, buffer, boxLength, powersOf2[phase]);
				}

				// execute as many sortOther4 as appropriate
				for (int step = phase; (step -= 4) >= 3;) {
					sortOther4.launch(gridOther, buffer, boxLength, powersOf2[step]);
				}

				// there are 3 or fewer steps remaining; finish with the appropriate kernel
				switch (phase & 3) {
				case 2:
					sortOther3.launch(gridOther, buffer, boxLength, powersOf2[2]);
					break;
				case 1:
					sortOther2.launch(gridOther, buffer, boxLength, powersOf2[1]);
					break;
				case 0:
					sortOther1.launch(gridOther, buffer, boxLength, powersOf2[0]);
					break;
				default:
					break;
				}
			}
		}
	}

}
