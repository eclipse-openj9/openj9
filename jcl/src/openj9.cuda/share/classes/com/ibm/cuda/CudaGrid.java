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

/**
 * The {@code CudaGrid} class represents a kernel launch configuration.
 */
public final class CudaGrid {

	/**
	 * The size of the thread block in the x dimension.
	 */
	public final int blockDimX;

	/**
	 * The size of the thread block in the y dimension.
	 */
	public final int blockDimY;

	/**
	 * The size of the thread block in the z dimension.
	 */
	public final int blockDimZ;

	/**
	 * The size of the grid in the x dimension.
	 */
	public final int gridDimX;

	/**
	 * The size of the grid in the y dimension.
	 */
	public final int gridDimY;

	/**
	 * The size of the grid in the z dimension.
	 */
	public final int gridDimZ;

	/**
	 * The number of bytes of shared memory to allocate to each thread block.
	 */
	public final int sharedMemBytes;

	/**
	 * The stream on which the kernel should be queued
	 * (or null for the default stream).
	 */
	public final CudaStream stream;

	/**
	 * Creates a grid with the specified dimensions, with no shared memory
	 * on the default stream.
	 *
	 * @param gridDim
	 *          the dimensions of the grid
	 * @param blockDim
	 *          the dimensions of the thread block
	 */
	public CudaGrid(Dim3 gridDim, Dim3 blockDim) {
		this(gridDim, blockDim, 0, null);
	}

	/**
	 * Creates a grid with the specified dimensions with no shared memory
	 * on the specified stream.
	 *
	 * @param gridDim
	 *          the dimensions of the grid
	 * @param blockDim
	 *          the dimensions of the thread block
	 * @param stream
	 *          the stream on which the kernel should be queued
	 *          (or null for the default stream)
	 */
	public CudaGrid(Dim3 gridDim, Dim3 blockDim, CudaStream stream) {
		this(gridDim, blockDim, 0, stream);
	}

	/**
	 * Creates a grid with the specified dimensions and shared memory size
	 * on the default stream.
	 *
	 * @param gridDim
	 *          the dimensions of the grid
	 * @param blockDim
	 *          the dimensions of the thread block
	 * @param sharedMemBytes
	 *          the number of bytes of shared memory to allocate to each thread block
	 */
	public CudaGrid(Dim3 gridDim, Dim3 blockDim, int sharedMemBytes) {
		this(gridDim, blockDim, sharedMemBytes, null);
	}

	/**
	 * Creates a grid with the specified dimensions and shared memory size
	 * on the specified stream.
	 *
	 * @param gridDim
	 *          the dimensions of the grid
	 * @param blockDim
	 *          the dimensions of the thread block
	 * @param sharedMemBytes
	 *          the number of bytes of shared memory to allocate to each thread block
	 * @param stream
	 *          the stream on which the kernel should be queued
	 *          (or null for the default stream)
	 */
	public CudaGrid(Dim3 gridDim, Dim3 blockDim, int sharedMemBytes,
			CudaStream stream) {
		super();
		this.blockDimX = blockDim.x;
		this.blockDimY = blockDim.y;
		this.blockDimZ = blockDim.z;
		this.gridDimX = gridDim.x;
		this.gridDimY = gridDim.y;
		this.gridDimZ = gridDim.z;
		this.sharedMemBytes = sharedMemBytes;
		this.stream = stream;
	}

	/**
	 * Creates a grid with the specified x dimensions with no shared memory
	 * on the default stream. The y and z dimensions are set to 1.
	 *
	 * @param gridDim
	 *          the x dimension of the grid
	 * @param blockDim
	 *          the x dimension of the thread block
	 */
	public CudaGrid(int gridDim, int blockDim) {
		this(gridDim, blockDim, 0, null);
	}

	/**
	 * Creates a grid with the specified x dimensions with no shared memory
	 * on the specified stream. The y and z dimensions are set to 1.
	 *
	 * @param gridDim
	 *          the x dimension of the grid
	 * @param blockDim
	 *          the x dimension of the thread block
	 * @param stream
	 *          the stream on which the kernel should be queued
	 *          (or null for the default stream)
	 */
	public CudaGrid(int gridDim, int blockDim, CudaStream stream) {
		this(gridDim, blockDim, 0, stream);
	}

	/**
	 * Creates a grid with the specified x dimensions and shared memory size
	 * on the default stream. The y and z dimensions are set to 1.
	 *
	 * @param gridDim
	 *          the x dimension of the grid
	 * @param blockDim
	 *          the x dimension of the thread block
	 * @param sharedMemBytes
	 *          the number of bytes of shared memory to allocate to each thread block
	 */
	public CudaGrid(int gridDim, int blockDim, int sharedMemBytes) {
		this(gridDim, blockDim, sharedMemBytes, null);
	}

	/**
	 * Creates a grid with the specified x dimensions and shared memory size
	 * on the specified stream. The y and z dimensions are set to 1.
	 *
	 * @param gridDim
	 *          the x dimension of the grid
	 * @param blockDim
	 *          the x dimension of the thread block
	 * @param sharedMemBytes
	 *          the number of bytes of shared memory to allocate to each thread block
	 * @param stream
	 *          the stream on which the kernel should be queued
	 *          (or null for the default stream)
	 */
	public CudaGrid(int gridDim, int blockDim, int sharedMemBytes,
			CudaStream stream) {
		super();
		this.blockDimX = blockDim;
		this.blockDimY = 1;
		this.blockDimZ = 1;
		this.gridDimX = gridDim;
		this.gridDimY = 1;
		this.gridDimZ = 1;
		this.sharedMemBytes = sharedMemBytes;
		this.stream = stream;
	}
}
