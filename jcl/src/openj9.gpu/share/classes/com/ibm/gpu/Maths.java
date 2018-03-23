/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

import java.util.Objects;

/**
 * This class is used to perform perform sorting operations of
 * primitive arrays of type int, long, float, double
 * on any connected CUDA GPU. A successful sort operation
 * results in the array being sorted in ascending order.
 */
public class Maths {

	private static int getDefaultDevice() {
		return CUDAManager.instanceInternal().getDefaultDevice();
	}

	/**
	 * Sort the given array of doubles into ascending order, using the default CUDA device.
	 *
	 * @param array
	 *          the array that will be sorted
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(double[] array)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(getDefaultDevice(), array, 0, array.length);
	}

	/**
	 * Sort the specified range of the array of doubles into ascending order, using the default CUDA device.
	 *
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(double[] array, int fromIndex, int toIndex)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(getDefaultDevice(), array, fromIndex, toIndex);
	}

	/**
	 * Sort the given array of floats into ascending order, using the default CUDA device.
	 *
	 * @param array
	 *          the array that will be sorted
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(float[] array) throws GPUSortException,
			GPUConfigurationException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(getDefaultDevice(), array, 0, array.length);
	}

	/**
	 * Sort the specified range of the array of floats into ascending order, using the default CUDA device.
	 *
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(float[] array, int fromIndex, int toIndex)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		sortArray(getDefaultDevice(), array, fromIndex, toIndex);
	}

	/**
	 * Sort the given array of doubles into ascending order, using the specified CUDA device.
	 *
	 * @param deviceId
	 *          the CUDA device to be used
	 * @param array
	 *          the array that will be sorted
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int deviceId, double[] array)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(deviceId, array, 0, array.length);
	}

	/**
	 * Sort the specified range of the array of doubles into ascending order, using the specified CUDA device.
	 *
	 * @param deviceId
	 *          the CUDA device to be used
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int deviceId, double[] array, int fromIndex,
			int toIndex) throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(deviceId, array, fromIndex, toIndex);
	}

	/**
	 * Sort the given array of floats into ascending order, using the specified CUDA device.
	 *
	 * @param deviceId
	 *          the CUDA device to be used
	 * @param array
	 *          the array that will be sorted
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int deviceId, float[] array)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(deviceId, array, 0, array.length);
	}

	/**
	 * Sort the specified range of the array of floats into ascending order, using the specified CUDA device.
	 *
	 * @param deviceId
	 *          the CUDA device to be used
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int deviceId, float[] array, int fromIndex,
			int toIndex) throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(deviceId, array, fromIndex, toIndex);
	}

	/**
	 * Sort the given array of integers into ascending order, using the specified CUDA device.
	 *
	 * @param deviceId
	 *          the CUDA device to be used
	 * @param array
	 *          the array that will be sorted
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int deviceId, int[] array)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(deviceId, array, 0, array.length);
	}

	/**
	 * Sort the specified range of the array of integers into ascending order, using the specified CUDA device.
	 *
	 * @param deviceId
	 *          the CUDA device to be used
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int deviceId, int[] array, int fromIndex,
			int toIndex) throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(deviceId, array, fromIndex, toIndex);
	}

	/**
	 * Sort the given array of longs into ascending order, using the specified CUDA device.
	 *
	 * @param deviceId
	 *          the CUDA device to be used
	 * @param array
	 *          the array that will be sorted
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int deviceId, long[] array)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(deviceId, array, 0, array.length);
	}

	/**
	 * Sort the specified range of the array of longs into ascending order, using the specified CUDA device.
	 *
	 * @param deviceId
	 *          the CUDA device to be used
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int deviceId, long[] array, int fromIndex,
			int toIndex) throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(deviceId, array, fromIndex, toIndex);
	}

	/**
	 * Sort the given array of integers into ascending order, using the default CUDA device.
	 *
	 * @param array
	 *          the array that will be sorted
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int[] array) throws GPUConfigurationException,
			GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(getDefaultDevice(), array, 0, array.length);
	}

	/**
	 * Sort the specified range of the array of integers into ascending order, using the default CUDA device.
	 *
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(int[] array, int fromIndex, int toIndex)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(getDefaultDevice(), array, fromIndex, toIndex);
	}

	/**
	 * Sort the given array of longs into ascending order, using the default CUDA device.
	 *
	 * @param array
	 *          the array that will be sorted
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(long[] array)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(getDefaultDevice(), array, 0, array.length);
	}

	/**
	 * Sort the specified range of the array of longs into ascending order, using the default CUDA device.
	 *
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @throws GPUConfigurationException
	 *          if an issue has occurred with the CUDA environment
	 * @throws GPUSortException
	 *          if any of the following happens:
	 * <ul>
	 * <li>the device is not available</li>
	 * <li>insufficient device memory is available</li>
	 * <li>an error occurs transferring the data to or from the device</li>
	 * <li>a device execution error occurs</li>
	 * </ul>
	 */
	public static void sortArray(long[] array, int fromIndex, int toIndex)
			throws GPUConfigurationException, GPUSortException {
		Objects.requireNonNull(array);
		SortNetwork.sortArray(getDefaultDevice(), array, fromIndex, toIndex);
	}

}
