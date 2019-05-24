/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
package com.ibm.gpu.spi;

/**
 * GPUAssist expresses the ability to use one or more GPUs
 * to assist the various sort methods of java.util.Arrays.
 */
public interface GPUAssist {

	/**
	 * GPUAssist.Provider enables discovery of a implementation of GPUAssist
	 * via the ServiceLoader.load method.
	 */
	interface Provider {

		/**
		 * Answer a GPUAssist implementation if one is available and enabled
		 * or null otherwise.
		 * 
		 * @return a GPUAssist implementation or null
		 */
		GPUAssist getGPUAssist();

	}

	/**
	 * A default implementation of GPUAssist that always defers to using the CPU.
	 */
	GPUAssist NONE = new GPUAssist() {

		@Override
		public boolean trySort(double[] array, int fromIndex, int toIndex) {
			return false;
		}

		@Override
		public boolean trySort(float[] array, int fromIndex, int toIndex) {
			return false;
		}

		@Override
		public boolean trySort(int[] array, int fromIndex, int toIndex) {
			return false;
		}

		@Override
		public boolean trySort(long[] array, int fromIndex, int toIndex) {
			return false;
		}

	};

	/**
	 * If sort is enabled on the GPU, and the array slice has a reasonable
	 * size for using a GPU, sort the region of the given array of values
	 * into ascending order, on the first available device.
	 * 
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @return true if the sort was successful, false otherwise
	 * @throws ArrayIndexOutOfBoundsException
	 *          if fromIndex is negative or toIndex is larger than
	 *          the length of the array 
	 * @throws IllegalArgumentException if fromIndex &gt; toIndex
	 */
	boolean trySort(double[] array, int fromIndex, int toIndex);

	/**
	 * If sort is enabled on the GPU, and the array slice has a reasonable
	 * size for using a GPU, sort the region of the given array of values
	 * into ascending order, on the first available device.
	 * 
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @return true if the sort was successful, false otherwise
	 * @throws ArrayIndexOutOfBoundsException
	 *          if fromIndex is negative or toIndex is larger than
	 *          the length of the array 
	 * @throws IllegalArgumentException if fromIndex &gt; toIndex
	 */
	boolean trySort(float[] array, int fromIndex, int toIndex);

	/**
	 * If sort is enabled on the GPU, and the array slice has a reasonable
	 * size for using a GPU, sort the region of the given array of values
	 * into ascending order, on the first available device.
	 * 
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @return true if the sort was successful, false otherwise
	 * @throws ArrayIndexOutOfBoundsException
	 *          if fromIndex is negative or toIndex is larger than
	 *          the length of the array 
	 * @throws IllegalArgumentException if fromIndex &gt; toIndex
	 */
	boolean trySort(int[] array, int fromIndex, int toIndex);

	/**
	 * If sort is enabled on the GPU, and the array slice has a reasonable
	 * size for using a GPU, sort the region of the given array of values
	 * into ascending order, on the first available device.
	 * 
	 * @param array
	 *          the array that will be sorted
	 * @param fromIndex
	 *          the range starting index (inclusive)
	 * @param toIndex
	 *          the range ending index (exclusive)
	 * @return true if the sort was successful, false otherwise
	 * @throws ArrayIndexOutOfBoundsException
	 *          if fromIndex is negative or toIndex is larger than
	 *          the length of the array 
	 * @throws IllegalArgumentException if fromIndex &gt; toIndex
	 */
	boolean trySort(long[] array, int fromIndex, int toIndex);

}
