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

import java.io.FileNotFoundException;
import java.io.InputStream;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Queue;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

import com.ibm.cuda.CudaBuffer;
import com.ibm.cuda.CudaDevice;
import com.ibm.cuda.CudaError;
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

	private static final class DelayedException extends RuntimeException {

		private static final long serialVersionUID = 6735593106826400878L;

		DelayedException(Exception exception) {
			super(exception.getLocalizedMessage(), exception);
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
				} catch (Exception e) {
					// ignore
				}
			}
		}

	}

	private static final class SortKernels {

		static SortKernels create(CudaDevice device) {
			// we assert privileges to read our resources and load a module
			PrivilegedAction<SortKernels> action = () -> {
				try {
					String code = "SortKernels.fatbin"; //$NON-NLS-1$
					CudaModule module = null;

					try (InputStream fatbin = CUDAManager.class.getResourceAsStream(code)) {
						if (fatbin == null) {
							throw new FileNotFoundException(code);
						}

						module = new CudaModule(device, fatbin);

						SortKernels kernels = new SortKernels(module);

						ShutdownHook.unloadOnShutdown(module);
						module = null;

						return kernels;
					} finally {
						if (module != null) {
							module.unload();
						}
					}
				} catch (Exception e) {
					throw new DelayedException(e);
				}
			};

			return AccessController.doPrivileged(action);
		}

		final CudaKernel doubleSortFirst4;
		final CudaKernel doubleSortOther1;
		final CudaKernel doubleSortOther2;
		final CudaKernel doubleSortOther3;
		final CudaKernel doubleSortOther4;
		final CudaKernel doubleSortPhase9;
		final CudaKernel floatSortFirst4;
		final CudaKernel floatSortOther1;
		final CudaKernel floatSortOther2;
		final CudaKernel floatSortOther3;
		final CudaKernel floatSortOther4;
		final CudaKernel floatSortPhase9;
		final CudaKernel intSortFirst4;
		final CudaKernel intSortOther1;
		final CudaKernel intSortOther2;
		final CudaKernel intSortOther3;
		final CudaKernel intSortOther4;
		final CudaKernel intSortPhase9;
		final CudaKernel longSortFirst4;
		final CudaKernel longSortOther1;
		final CudaKernel longSortOther2;
		final CudaKernel longSortOther3;
		final CudaKernel longSortOther4;
		final CudaKernel longSortPhase9;

		/**
		 * Finds kernels for later use.
		 *
		 * @param module  the module expected to contain the sort kernels
		 * @throws CudaException
		 */
		@SuppressWarnings("nls")
		private SortKernels(CudaModule module) throws Exception {
			super();

			doubleSortFirst4 = new CudaKernel(module, "DFirst4");
			doubleSortPhase9 = new CudaKernel(module, "DPhase9");
			doubleSortOther1 = new CudaKernel(module, "DOther1");
			doubleSortOther2 = new CudaKernel(module, "DOther2");
			doubleSortOther3 = new CudaKernel(module, "DOther3");
			doubleSortOther4 = new CudaKernel(module, "DOther4");

			floatSortFirst4 = new CudaKernel(module, "FFirst4");
			floatSortPhase9 = new CudaKernel(module, "FPhase9");
			floatSortOther1 = new CudaKernel(module, "FOther1");
			floatSortOther2 = new CudaKernel(module, "FOther2");
			floatSortOther3 = new CudaKernel(module, "FOther3");
			floatSortOther4 = new CudaKernel(module, "FOther4");

			intSortFirst4 = new CudaKernel(module, "IFirst4");
			intSortPhase9 = new CudaKernel(module, "IPhase9");
			intSortOther1 = new CudaKernel(module, "IOther1");
			intSortOther2 = new CudaKernel(module, "IOther2");
			intSortOther3 = new CudaKernel(module, "IOther3");
			intSortOther4 = new CudaKernel(module, "IOther4");

			longSortFirst4 = new CudaKernel(module, "JFirst4");
			longSortPhase9 = new CudaKernel(module, "JPhase9");
			longSortOther1 = new CudaKernel(module, "JOther1");
			longSortOther2 = new CudaKernel(module, "JOther2");
			longSortOther3 = new CudaKernel(module, "JOther3");
			longSortOther4 = new CudaKernel(module, "JOther4");
		}

	}

	private static final ConcurrentHashMap<CudaDevice, SortKernels> deviceMap;

	private static final Integer[] powersOf2;

	static {
		final int phaseCount = 31;
		final Integer[] powers = new Integer[phaseCount];

		for (int i = 0; i < phaseCount; ++i) {
			powers[i] = Integer.valueOf(1 << i);
		}

		deviceMap = new ConcurrentHashMap<>();
		powersOf2 = powers;
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
			SortNetwork network = new SortNetwork(deviceId);

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
			SortNetwork network = new SortNetwork(deviceId);

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
			SortNetwork network = new SortNetwork(deviceId);

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
			SortNetwork network = new SortNetwork(deviceId);

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

	private CudaKernel sortFirst4;

	private CudaKernel sortOther1;

	private CudaKernel sortOther2;

	private CudaKernel sortOther3;

	private CudaKernel sortOther4;

	private CudaKernel sortPhase9;

	/**
	 * Initialize a new SortNetwork for the specified device.
	 *
	 * @param deviceId
	 * @throws GPUConfigurationException
	 */
	private SortNetwork(int deviceId) throws GPUConfigurationException {
		super();
		this.device = new CudaDevice(deviceId);

		try {
			this.maxGridDimX = device.getAttribute(CudaDevice.ATTRIBUTE_MAX_GRID_DIM_X);
		} catch (Exception e) {
			throw new GPUConfigurationException(e.getLocalizedMessage(), e);
		}
	}

	/**
	 * Gets an existing SortKernels object for the current device,
	 * if none exists, create a new one, store it and return it.
	 *
	 * @return SortKernels
	 * @throws GPUConfigurationException
	 * @throws GPUSortException
	 */
	private SortKernels getKernels() throws GPUConfigurationException, GPUSortException {
		try {
			return deviceMap.computeIfAbsent(device, SortKernels::create);
		} catch (DelayedException e) {
			Throwable cause = e.getCause();

			/*
			 * CUDA 9.0 removed support for devices with compute capability 2
			 * so the fatbin resource won't have code for those devices when
			 * CUDA 9+ is used to compile the kernel code. We detect that here
			 * instead of checking compute capability in the constructor as was
			 * done originally.
			 */
			if (cause instanceof CudaException) {
				if (((CudaException) cause).code == CudaError.NoKernelImageForDevice) {
					throw new GPUConfigurationException("Unsupported device detected"); //$NON-NLS-1$
				}
			}

			throw new GPUSortException(e.getLocalizedMessage(), e);
		}
	}

	private CudaGrid makeGrid(int threadCount, int blockSize, CudaStream stream) {
		int blockCount = Math.max(1, (threadCount + blockSize - 1) / blockSize);

		return new CudaGrid(makeGridDim(blockCount), new Dim3(blockSize), stream);
	}

	private Dim3 makeGridDim(int blockCount) {
		int blockDimX = Math.max(1, blockCount);
		int blockDimY = 1;

		// Even for devices with compute capability < 3.0 (with maxGridDimX=65535),
		// this loop terminates with reasonable grid dimensions because
		// 65535*65535 > Integer.MAX_INT.
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
			throws GPUConfigurationException, GPUSortException {
		final int length = toIndex - fromIndex;

		if (length < 2) {
			checkIndices(array.length, fromIndex, toIndex);
			return;
		}

		SortKernels kernels = getKernels();

		sortFirst4 = kernels.doubleSortFirst4;
		sortOther1 = kernels.doubleSortOther1;
		sortOther2 = kernels.doubleSortOther2;
		sortOther3 = kernels.doubleSortOther3;
		sortOther4 = kernels.doubleSortOther4;
		sortPhase9 = kernels.doubleSortPhase9;

		try (CudaBuffer gpuBuffer = new CudaBuffer(device, length * (long) Double.BYTES)) {
			gpuBuffer.copyFrom(array, fromIndex, toIndex);
			sortBuffer(gpuBuffer, length);
			gpuBuffer.copyTo(array, fromIndex, toIndex);
		} catch (Exception e) {
			throw new GPUSortException(e.getLocalizedMessage(), e);
		}
	}

	private void sort(float[] array, int fromIndex, int toIndex)
			throws GPUConfigurationException, GPUSortException {
		final int length = toIndex - fromIndex;

		if (length < 2) {
			checkIndices(array.length, fromIndex, toIndex);
			return;
		}

		SortKernels kernels = getKernels();

		sortFirst4 = kernels.floatSortFirst4;
		sortOther1 = kernels.floatSortOther1;
		sortOther2 = kernels.floatSortOther2;
		sortOther3 = kernels.floatSortOther3;
		sortOther4 = kernels.floatSortOther4;
		sortPhase9 = kernels.floatSortPhase9;

		try (CudaBuffer gpuBuffer = new CudaBuffer(device, length * (long) Float.BYTES)) {
			gpuBuffer.copyFrom(array, fromIndex, toIndex);
			sortBuffer(gpuBuffer, length);
			gpuBuffer.copyTo(array, fromIndex, toIndex);
		} catch (Exception e) {
			throw new GPUSortException(e.getLocalizedMessage(), e);
		}
	}

	private void sort(int[] array, int fromIndex, int toIndex)
			throws GPUConfigurationException, GPUSortException {
		final int length = toIndex - fromIndex;

		if (length < 2) {
			checkIndices(array.length, fromIndex, toIndex);
			return;
		}

		SortKernels kernels = getKernels();

		sortFirst4 = kernels.intSortFirst4;
		sortOther1 = kernels.intSortOther1;
		sortOther2 = kernels.intSortOther2;
		sortOther3 = kernels.intSortOther3;
		sortOther4 = kernels.intSortOther4;
		sortPhase9 = kernels.intSortPhase9;

		try (CudaBuffer gpuBuffer = new CudaBuffer(device, length * (long) Integer.BYTES)) {
			gpuBuffer.copyFrom(array, fromIndex, toIndex);
			sortBuffer(gpuBuffer, length);
			gpuBuffer.copyTo(array, fromIndex, toIndex);
		} catch (Exception e) {
			throw new GPUSortException(e.getLocalizedMessage(), e);
		}
	}

	 /**
	 * Sets up and runs the kernels used to sort LONG arrays
	 *
	 * @param array  array to be sorted
	 * @param fromIndex  starting index of sort
	 * @param toIndex  ending index of sort
	 * @throws GPUConfigurationException
	 * @throws GPUSortException
	 */
	private void sort(long[] array, int fromIndex, int toIndex)
			throws GPUConfigurationException, GPUSortException {
		final int length = toIndex - fromIndex;

		if (length < 2) {
			checkIndices(array.length, fromIndex, toIndex);
			return;
		}

		SortKernels kernels = getKernels();

		sortFirst4 = kernels.longSortFirst4;
		sortOther1 = kernels.longSortOther1;
		sortOther2 = kernels.longSortOther2;
		sortOther3 = kernels.longSortOther3;
		sortOther4 = kernels.longSortOther4;
		sortPhase9 = kernels.longSortPhase9;

		try (CudaBuffer gpuBuffer = new CudaBuffer(device, length * (long) Long.BYTES)) {
			gpuBuffer.copyFrom(array, fromIndex, toIndex);
			sortBuffer(gpuBuffer, length);
			gpuBuffer.copyTo(array, fromIndex, toIndex);
		} catch (Exception e) {
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
	private void sortBuffer(CudaBuffer buffer, int length) throws Exception {
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
