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

/**
 * The {@code CudaKernel} class represents a kernel {@link CudaFunction function}
 * in a loaded {@link CudaModule}.
 */
public class CudaKernel {

	/**
	 * The {@code Parameters} class represents the actual parameters in
	 * a {@link CudaKernel kernel} launch.
	 */
	public static final class Parameters implements Cloneable {

		/* A bit-mask of missing values (the bit (1<<i) is set if the values[i] is missing). */
		private long mask;

		final long[] values;

		/**
		 * Creates a new bundle of parameter values.
		 *
		 * @param count
		 *          the number of values to be passed when the kernel is launched
		 * @throws IllegalArgumentException
		 *          if the count is negative or greater than 64
		 */
		public Parameters(int count) {
			super();
			if (0 <= count && count <= Long.SIZE) {
				this.mask = count == Long.SIZE ? -1L : (1L << count) - 1;
				this.values = new long[count];
			} else {
				throw new IllegalArgumentException();
			}
		}

		/**
		 * Creates a new bundle of parameter values.
		 * <p>
		 * Each parameter value must be one of the following:
		 * <ul>
		 * <li>a boxed primitive value</li>
		 * <li>a CudaBuffer object</li>
		 * <li>null</li>
		 * </ul>
		 *
		 * @param values
		 *          the values to be passed when the kernel is launched
		 * @throws IllegalArgumentException
		 *          if {@code parameters} contains any unsupported types
		 */
		public Parameters(Object... values) {
			super();

			int count = values.length;

			this.mask = 0;
			this.values = new long[count];

			for (int i = 0; i < count; ++i) {
				this.values[i] = CudaFunction.nativeValueOf(values[i]);
			}
		}

		/**
		 * Creates a copy of the given parameter block.
		 *
		 * @param that
		 *          the parameter block to be copied
		 */
		public Parameters(Parameters that) {
			super();
			this.mask = that.mask;
			this.values = that.values.clone();
		}

		/**
		 * Appends a byte value to the list of parameter values.
		 *
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if all positions already have values defined
		 */
		public Parameters add(byte value) {
			return add((long) value);
		}

		/**
		 * Appends a character value to the list of parameter values.
		 *
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if all positions already have values defined
		 */
		public Parameters add(char value) {
			return add((long) value);
		}

		/**
		 * Appends a buffer address to the list of parameter values.
		 *
		 * @param value
		 *          the value to be passed when the kernel is launched,
		 *          or null to pass a null pointer
		 * @return
		 *          this parameter list
		 * @throws IllegalStateException
		 *          if the buffer has been closed (see {@link CudaBuffer#close()})
		 * @throws IndexOutOfBoundsException
		 *          if all positions already have values defined
		 */
		public Parameters add(CudaBuffer value) {
			return add(value == null ? 0 : value.getAddress());
		}

		/**
		 * Appends a double value to the list of parameter values.
		 *
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if all positions already have values defined
		 */
		public Parameters add(double value) {
			return add(Double.doubleToRawLongBits(value));
		}

		/**
		 * Appends a float value to the list of parameter values.
		 *
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if all positions already have values defined
		 */
		public Parameters add(float value) {
			return add((long) Float.floatToRawIntBits(value));
		}

		/**
		 * Appends a integer value to the list of parameter values.
		 *
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if all positions already have values defined
		 */
		public Parameters add(int value) {
			return add((long) value);
		}

		/**
		 * Appends a long value to the list of parameter values.
		 *
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if all positions already have values defined
		 */
		public Parameters add(long value) {
			if (isComplete()) {
				throw new IndexOutOfBoundsException();
			}

			int index = Long.numberOfTrailingZeros(mask);

			return set(index, value);
		}

		/**
		 * Appends a short value to the list of parameter values.
		 *
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if all positions already have values defined
		 */
		public Parameters add(short value) {
			return add((long) value);
		}

		/**
		 * Creates a copy of this parameter block.
		 */
		@Override
		public Parameters clone() {
			return new Parameters(this);
		}

		boolean isComplete() {
			return mask == 0;
		}

		/**
		 * Replaces the parameter at the specified index with the given byte value.
		 *
		 * @param index
		 *          the index of the parameter to be set
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if {@code index} &lt; 0 or {@code index} &gt;= the size of this parameter list
		 */
		public Parameters set(int index, byte value) {
			return set(index, (long) value);
		}

		/**
		 * Replaces the parameter at the specified index with the given character value.
		 *
		 * @param index
		 *          the index of the parameter to be set
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if {@code index} &lt; 0 or {@code index} &gt;= the size of this parameter list
		 */
		public Parameters set(int index, char value) {
			return set(index, (long) value);
		}

		/**
		 * Replaces the parameter at the specified index with the given buffer address.
		 *
		 * @param index
		 *          the index of the parameter to be set
		 * @param value
		 *          the value to be passed when the kernel is launched,
		 *          or null to pass a null pointer
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if {@code index} &lt; 0 or {@code index} &gt;= the size of this parameter list
		 */
		public Parameters set(int index, CudaBuffer value) {
			return set(index, value == null ? 0 : value.getAddress());
		}

		/**
		 * Replaces the parameter at the specified index with the given double value.
		 *
		 * @param index
		 *          the index of the parameter to be set
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if {@code index} &lt; 0 or {@code index} &gt;= the size of this parameter list
		 */
		public Parameters set(int index, double value) {
			return set(index, Double.doubleToRawLongBits(value));
		}

		/**
		 * Replaces the parameter at the specified index with the given float value.
		 *
		 * @param index
		 *          the index of the parameter to be set
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if {@code index} &lt; 0 or {@code index} &gt;= the size of this parameter list
		 */
		public Parameters set(int index, float value) {
			return set(index, (long) Float.floatToRawIntBits(value));
		}

		/**
		 * Replaces the parameter at the specified index with the given int value.
		 *
		 * @param index
		 *          the index of the parameter to be set
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if {@code index} &lt; 0 or {@code index} &gt;= the size of this parameter list
		 */
		public Parameters set(int index, int value) {
			return set(index, (long) value);
		}

		/**
		 * Replaces the parameter at the specified index with the given long value.
		 *
		 * @param index
		 *          the index of the parameter to be set
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if {@code index} &lt; 0 or {@code index} &gt;= the size of this parameter list
		 */
		public Parameters set(int index, long value) {
			if (0 <= index && index < values.length) {
				mask &= ~(1L << index);
				values[index] = value;
				return this;
			} else {
				throw new IndexOutOfBoundsException(Integer.toString(index));
			}
		}

		/**
		 * Replaces the parameter at the specified index with a short value.
		 *
		 * @param index
		 *          the index of the parameter to be set
		 * @param value
		 *          the value to be passed when the kernel is launched
		 * @return
		 *          this parameter list
		 * @throws IndexOutOfBoundsException
		 *          if {@code index} &lt; 0 or {@code index} &gt;= the size of this parameter list
		 */
		public Parameters set(int index, short value) {
			return set(index, (long) value);
		}
	}

	private final CudaFunction function;

	/**
	 * Creates a new kernel object in the given module whose entry point
	 * is the specified function.
	 *
	 * @param module
	 *          the module containing the kernel code
	 * @param function
	 *          the entry point of the kernel
	 */
	public CudaKernel(CudaModule module, CudaFunction function) {
		super();

		if (function.deviceId != module.deviceId) {
			throw new IllegalArgumentException();
		}

		this.function = function;
	}

	/**
	 * Creates a new kernel object in the given module whose entry point
	 * is the function with the specified name.
	 *
	 * @param module
	 *          the module containing the kernel code
	 * @param functionName
	 *          the name of the entry point of the kernel
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public CudaKernel(CudaModule module, String functionName)
			throws CudaException {
		this(module, module.getFunction(functionName));
	}

	/**
	 * Returns the value of the specified @{code attribute} for the
	 * {@link CudaFunction function} associated with this kernel.
	 *
	 * @param attribute
	 *          the attribute to be queried (see CudaFunction.ATTRIBUTE_XXX)
	 * @return
	 *          the attribute value
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public final int getAttribute(int attribute) throws CudaException {
		return function.getAttribute(attribute);
	}

	/**
	 * Launches this kernel. The launch configuration is given by {@code grid}
	 * and the actual parameter values are specified by {@code parameters}.
	 * <p>
	 * Each parameter value must be one of the following:
	 * <ul>
	 * <li>a boxed primitive value</li>
	 * <li>a CudaBuffer object</li>
	 * <li>null</li>
	 * </ul>
	 *
	 * @param grid
	 *          the launch configuration
	 * @param parameters
	 *          the actual parameter values
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code parameters} contains any unsupported types
	 */
	public final void launch(CudaGrid grid, Object... parameters)
			throws CudaException {
		function.launch(grid, parameters);
	}

	/**
	 * Launches this kernel. The launch configuration is given by {@code grid}
	 * and the actual parameter values are specified by {@code parameters}.
	 *
	 * @param grid
	 *          the launch configuration
	 * @param parameters
	 *          the actual parameter values
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws IllegalArgumentException
	 *          if {@code parameters} does not contain the correct number of values
	 */
	public final void launch(CudaGrid grid, Parameters parameters)
			throws CudaException {
		function.launch(grid, parameters);
	}

	/**
	 * Configures the cache for the {@link CudaFunction function} associated
	 * with this kernel.
	 *
	 * @param config
	 *          the desired cache configuration
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public final void setCacheConfig(CacheConfig config) throws CudaException {
		function.setCacheConfig(config);
	}

	/**
	 * Configures the shared memory of the {@link CudaFunction function}
	 * associated with this kernel.
	 *
	 * @param config
	 *          the desired shared memory configuration
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public final void setSharedMemConfig(SharedMemConfig config)
			throws CudaException {
		function.setSharedMemConfig(config);
	}
}
