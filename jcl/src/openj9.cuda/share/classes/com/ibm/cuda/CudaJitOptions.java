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

import java.util.concurrent.atomic.AtomicLong;

/**
 * The {@code CudaJitOptions} class represents a set of options that influence
 * the behavior of linking and loading modules.
 */
public final class CudaJitOptions implements Cloneable {

	/**
	 * {@code CacheMode} identifies the cache management choices.
	 */
	public static enum CacheMode {

		/** Compile with no -dlcm flag specified. */
		DEFAULT(1),

		/** Compile with L1 cache disabled. */
		L1_DISABLED(2),

		/** Compile with L1 cache enabled. */
		L1_ENABLED(3);

		final int nativeMode;

		CacheMode(int nativeMode) {
			this.nativeMode = nativeMode;
		}
	}

	/**
	 * {@code Fallback} identifies the available fall-back strategies
	 * when an exactly matching object is not available.
	 */
	public static enum Fallback {

		/** Prefer to fall back to compatible binary code if exact match not found. */
		PreferBinary(1),

		/** Prefer to compile ptx if exact binary match not found. */
		PreferPtx(2);

		final int nativeStrategy;

		Fallback(int nativeStrategy) {
			this.nativeStrategy = nativeStrategy;
		}
	}

	// The range of OPT_* constants.
	private static final int NUM_OPT_CODES = 15;

	private static final int OPT_CACHE_MODE = 14;

	private static final int OPT_ERROR_LOG_BUFFER_SIZE_BYTES = 6;

	private static final int OPT_FALLBACK_STRATEGY = 10;

	private static final int OPT_GENERATE_DEBUG_INFO = 11;

	private static final int OPT_GENERATE_LINE_INFO = 13;

	private static final int OPT_INFO_LOG_BUFFER_SIZE_BYTES = 4;

	private static final int OPT_LOG_VERBOSE = 12;

	private static final int OPT_MAX_REGISTERS = 0;

	private static final int OPT_OPTIMIZATION_LEVEL = 7;

	private static final int OPT_TARGET = 9;

	private static final int OPT_TARGET_FROM_CUCONTEXT = 8;

	private static final int OPT_THREADS_PER_BLOCK = 1;

	private static final int OPT_WALL_TIME = 2;

	private static native long create(int[] keyValuePairs) throws CudaException;

	private static native void destroy(long handle);

	private static native String getErrorLogBuffer(long handle);

	private static native String getInfoLogBuffer(long handle);

	private static native int getThreadsPerBlock(long handle);

	private static native float getWallTime(long handle);

	private String errorLogBuffer;

	private String infoLogBuffer;

	private final AtomicLong nativeHandle;

	private int optionMask;

	private final int[] optionValue;

	private int threadsPerBlock;

	private float wallTime;

	/**
	 * Creates a new options object.
	 */
	public CudaJitOptions() {
		super();
		this.errorLogBuffer = ""; //$NON-NLS-1$
		this.infoLogBuffer = ""; //$NON-NLS-1$
		this.nativeHandle = new AtomicLong();
		this.optionMask = 0;
		this.optionValue = new int[NUM_OPT_CODES];
		this.threadsPerBlock = 0;
		this.wallTime = 0.0f;
	}

	/**
	 * Creates a new options object with the same state as this object.
	 */
	@Override
	protected CudaJitOptions clone() {
		CudaJitOptions clone = new CudaJitOptions();

		clone.optionMask = optionMask;
		System.arraycopy(optionValue, 0, clone.optionValue, 0, NUM_OPT_CODES);

		return clone;
	}

	/**
	 * Returns the contents of the error log.
	 * <p>
	 * The result will be empty unless {@link #setErrorLogBufferSize(int)}
	 * was called with a positive value, this object was used in connection
	 * with a {@link CudaModule} or a {@link CudaLinker}, and errors were
	 * reported.
	 *
	 * @return
	 *          the contents of the error log
	 */
	public String getErrorLogBuffer() {
		return errorLogBuffer;
	}

	long getHandle() throws CudaException {
		long handle;

		while ((handle = nativeHandle.get()) == 0) {
			int mask = optionMask;
			int[] keyValuePairs = new int[Integer.bitCount(mask) << 1];
			int index = 0;

			for (int code = 0; mask != 0; ++code, mask >>= 1) {
				if ((mask & 1) != 0) {
					keyValuePairs[index++] = code;
					keyValuePairs[index++] = optionValue[code];
				}
			}

			handle = create(keyValuePairs);

			if (nativeHandle.compareAndSet(0, handle)) {
				break;
			}

			// forget our work and look for results from a thread that finished earlier
			destroy(handle);
		}

		return handle;
	}

	/**
	 * Returns the contents of the information log.
	 * <p>
	 * The result will be empty unless {@link #setInfoLogBufferSize(int)}
	 * was called with a positive value, this object was used in connection
	 * with a {@link CudaModule} or a {@link CudaLinker}, and informational
	 * messages were reported.
	 *
	 * @return
	 *          the contents of the information log
	 */
	public String getInfoLogBuffer() {
		return infoLogBuffer;
	}

	/**
	 * Returns the maximum number of threads per block.
	 * <p>
	 * The result will only be meaningful if {@link #setThreadsPerBlock(int)} was
	 * called with a positive value, and this object was used in connection
	 * with a {@link CudaModule} or a {@link CudaLinker} involving PTX code.
	 *
	 * @return
	 *          the maximum number of threads per block
	 */
	public int getThreadsPerBlock() {
		return threadsPerBlock;
	}

	/**
	 * Returns the total elapsed time, in milliseconds,
	 * spent in the compiler and linker.
	 * <p>
	 * Applies to: compiler and linker.
	 *
	 * @return
	 *          the total elapsed time, in milliseconds, spent in the compiler and linker
	 */
	public float getWallTime() {
		return wallTime;
	}

	/**
	 * Requests recording of the total wall clock time,
	 * in milliseconds, spent in the compiler and linker.
	 * <p>
	 * Applies to: compiler and linker.
	 *
	 * @return
	 *          this options object
	 */
	public CudaJitOptions recordWallTime() {
		return setOption(OPT_WALL_TIME, true);
	}

	void releaseHandle(boolean update) {
		long handle = nativeHandle.getAndSet(0);

		if (handle != 0) {
			if (update) {
				update(handle);
			}

			destroy(handle);
		}
	}

	/**
	 * Specifies the desired caching behavior (-dlcm).
	 * <p>
	 * Applies to compiler only.
	 *
	 * @param mode
	 *          the desired caching behavior
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setCacheMode(CacheMode mode) {
		if (mode == null) {
			mode = CacheMode.DEFAULT;
		}

		return setOption(OPT_CACHE_MODE, mode.nativeMode);
	}

	/**
	 * Specifies the size, in bytes, to allocate for capturing error messages.
	 * <p>
	 * Applies to compiler and linker.
	 *
	 * @param size
	 *          the size, in bytes, of the error log buffer
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setErrorLogBufferSize(int size) {
		return setOption(OPT_ERROR_LOG_BUFFER_SIZE_BYTES, size);
	}

	/**
	 * Specifies whether to generate debug information.
	 * <p>
	 * Applies to compiler and linker.
	 *
	 * @param enabled
	 *          whether debug information should be generated
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setGenerateDebugInfo(boolean enabled) {
		return setOption(OPT_GENERATE_DEBUG_INFO, enabled);
	}

	/**
	 * Specifies whether to generate line number information.
	 * <p>
	 * Applies to compiler only.
	 *
	 * @param enabled
	 *          whether line number information should be generated
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setGenerateLineInfo(boolean enabled) {
		return setOption(OPT_GENERATE_LINE_INFO, enabled);
	}

	/**
	 * Specifies the size, in bytes, to allocate for capturing informational
	 * messages.
	 * <p>
	 * Applies to compiler and linker.
	 *
	 * @param size
	 *          the size, in bytes, of the information log buffer
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setInfoLogBufferSize(int size) {
		return setOption(OPT_INFO_LOG_BUFFER_SIZE_BYTES, size);
	}

	/**
	 * Specifies the fallback strategy if an exactly matching
	 * binary object cannot be found.
	 * <p>
	 * Applies to: compiler only
	 *
	 * @param strategy
	 *          the desired fallback strategy
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setJitFallbackStrategy(Fallback strategy) {
		if (strategy == null) {
			strategy = Fallback.PreferPtx;
		}

		return setOption(OPT_FALLBACK_STRATEGY, strategy.nativeStrategy);
	}

	/**
	 * Specifies whether to generate verbose log messages.
	 * <p>
	 * Applies to: compiler and linker
	 *
	 * @param verbose
	 *          whether verbose log messages should be generated
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setLogVerbose(boolean verbose) {
		return setOption(OPT_LOG_VERBOSE, verbose);
	}

	/**
	 * Specifies the maximum number of registers that a thread may use.
	 * <p>
	 * Applies to: compiler only
	 *
	 * @param limit
	 *          the maximum number of registers a thread may use
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setMaxRegisters(int limit) {
		return setOption(OPT_MAX_REGISTERS, limit);
	}

	/**
	 * Specifies the level of optimization to be applied to generated code
	 * (0 - 4), with 4 being the default and highest level of optimization.
	 * <p>
	 * Applies to compiler only.
	 *
	 * @param level
	 *          the desired optimization level
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setOptimizationLevel(int level) {
		return setOption(OPT_OPTIMIZATION_LEVEL, level);
	}

	private CudaJitOptions setOption(int key, boolean value) {
		return setOption(key, value ? 1 : 0);
	}

	private CudaJitOptions setOption(int key, int value) {
		releaseHandle(false);

		optionMask |= 1 << key;
		optionValue[key] = value;

		return this;
	}

	/**
	 * Specifies the desired compute target.
	 * <p>
	 * Cannot be combined with {@link #setThreadsPerBlock(int)}.
	 * <p>
	 * Applies to compiler and linker.
	 *
	 * @param target
	 *          the desired compute target
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setTarget(CudaJitTarget target) {
		return setOption(OPT_TARGET, target.nativeValue);
	}

	/**
	 * Specifies that the target should be determined based on the current
	 * attached context.
	 * <p>
	 * Applies to compiler and linker.
	 *
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setTargetFromCuContext() {
		return setOption(OPT_TARGET_FROM_CUCONTEXT, true);
	}

	/**
	 * Specifies the minimum number of threads per block for compilation.
	 * <p>
	 * This restricts the resource utilization of the compiler (e.g. maximum
	 * registers) such that a block with the given number of threads should be
	 * able to launch based on register limitations. Note, this option does not
	 * currently take into account any other resource limitations, such as
	 * shared memory utilization.
	 * <p>
	 * Cannot be combined with {@link #setTarget(CudaJitTarget)}.
	 * <p>
	 * Applies to compiler only.
	 *
	 * @param limit
	 *          the desired minimum number of threads per block
	 * @return
	 *          this options object
	 */
	public CudaJitOptions setThreadsPerBlock(int limit) {
		return setOption(OPT_THREADS_PER_BLOCK, limit);
	}

	CudaJitOptions update() {
		long handle = nativeHandle.get();

		if (handle != 0) {
			update(handle);
		}

		return this;
	}

	private void update(long handle) {
		errorLogBuffer = getErrorLogBuffer(handle);
		infoLogBuffer = getInfoLogBuffer(handle);
		threadsPerBlock = getThreadsPerBlock(handle);
		wallTime = getWallTime(handle);
	}
}
