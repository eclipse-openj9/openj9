/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
package com.ibm.gpu.internal;

import com.ibm.gpu.CUDAManager;
import com.ibm.gpu.GPUConfigurationException;
import com.ibm.gpu.GPUSortException;
import com.ibm.gpu.Maths;
import com.ibm.gpu.spi.GPUAssist;

/**
 * The implementation of GPU sort assist using one or more CUDA devices.
 */
final class CudaGPUAssist implements GPUAssist {

	@SuppressWarnings({ "boxing", "nls" })
	private static void verboseBelowThreshold(String type, int length, int threshold) {
		String format = "%s%s array of %,d elements will be sorted on the CPU as it does not"
				+ " meet the minimum length requirement (must contain at least %,d elements)%n";

		System.out.printf(format, CUDAManager.getOutputHeader(), type, length, threshold);
	}

	private final CUDAManager manager;

	CudaGPUAssist(CUDAManager manager) {
		super();
		this.manager = manager;
	}

	@Override
	public boolean trySort(double[] array, int fromIndex, int toIndex) {
		int deviceId = manager.acquireFreeDevice();

		if (deviceId < 0) {
			return false;
		}

		try {
			// Check length if not using enforce option.
			if (!manager.isSortEnforcedOnGPU()) {
				int length = toIndex - fromIndex;
				// What's the threshold for the device?
				int threshold = manager.getDevice(deviceId).getDoubleThreshold();

				if (length < threshold) {
					if (manager.getVerboseGPUOutput()) {
						verboseBelowThreshold("Double", length, threshold); //$NON-NLS-1$
					}

					return false;
				}
			}

			Maths.sortArray(deviceId, array, fromIndex, toIndex);
			return true;
		} catch (GPUSortException | GPUConfigurationException e) {
			return false;
		} finally {
			manager.releaseDevice(deviceId);
		}
	}

	@Override
	public boolean trySort(float[] array, int fromIndex, int toIndex) {
		int deviceId = manager.acquireFreeDevice();

		if (deviceId < 0) {
			return false;
		}

		try {
			// Check length if not using enforce option.
			if (!manager.isSortEnforcedOnGPU()) {
				int length = toIndex - fromIndex;
				// What's the threshold for the device?
				int threshold = manager.getDevice(deviceId).getFloatThreshold();

				if (length < threshold) {
					if (manager.getVerboseGPUOutput()) {
						verboseBelowThreshold("Float", length, threshold); //$NON-NLS-1$
					}

					return false;
				}
			}

			Maths.sortArray(deviceId, array, fromIndex, toIndex);
			return true;
		} catch (GPUSortException | GPUConfigurationException e) {
			return false;
		} finally {
			manager.releaseDevice(deviceId);
		}
	}

	@Override
	public boolean trySort(int[] array, int fromIndex, int toIndex) {
		int deviceId = manager.acquireFreeDevice();

		if (deviceId < 0) {
			return false;
		}

		try {
			// Check length if not using enforce option.
			if (!manager.isSortEnforcedOnGPU()) {
				int length = toIndex - fromIndex;
				// What's the threshold for the device?
				int threshold = manager.getDevice(deviceId).getIntThreshold();

				if (length < threshold) {
					if (manager.getVerboseGPUOutput()) {
						verboseBelowThreshold("Integer", length, threshold); //$NON-NLS-1$
					}

					return false;
				}
			}

			Maths.sortArray(deviceId, array, fromIndex, toIndex);
			return true;
		} catch (GPUSortException | GPUConfigurationException e) {
			return false;
		} finally {
			manager.releaseDevice(deviceId);
		}
	}

	@Override
	public boolean trySort(long[] array, int fromIndex, int toIndex) {
		int deviceId = manager.acquireFreeDevice();

		if (deviceId < 0) {
			return false;
		}

		try {
			// Check length if not using enforce option.
			if (!manager.isSortEnforcedOnGPU()) {
				int length = toIndex - fromIndex;
				// What's the threshold for the device?
				int threshold = manager.getDevice(deviceId).getLongThreshold();

				if (length < threshold) {
					if (manager.getVerboseGPUOutput()) {
						verboseBelowThreshold("Long", length, threshold); //$NON-NLS-1$
					}

					return false;
				}
			}

			Maths.sortArray(deviceId, array, fromIndex, toIndex);
			return true;
		} catch (GPUSortException | GPUConfigurationException e) {
			return false;
		} finally {
			manager.releaseDevice(deviceId);
		}
	}

}
