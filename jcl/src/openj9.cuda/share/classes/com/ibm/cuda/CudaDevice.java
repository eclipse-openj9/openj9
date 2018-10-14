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

import java.util.Objects;

/**
 * The {@code CudaDevice} class represents a CUDA-capable device.
 */
public final class CudaDevice {

	/**
	 * {@code CacheConfig} identifies the cache configuration choices for
	 * a device.
	 */
	public static enum CacheConfig {

		/** prefer equal sized L1 cache and shared memory */
		PREFER_EQUAL(0),

		/** prefer larger L1 cache and smaller shared memory */
		PREFER_L1(1),

		/** no preference for shared memory or L1 (default) */
		PREFER_NONE(2),

		/** prefer larger shared memory and smaller L1 cache */
		PREFER_SHARED(3);

		final int nativeValue;

		CacheConfig(int value) {
			this.nativeValue = value;
		}
	}

	/**
	 * {@code Limit} identifies device limits that may be queried or configured.
	 */
	public static enum Limit {

		/** maximum number of outstanding device runtime launches that can be made from this context */
		DEV_RUNTIME_PENDING_LAUNCH_COUNT(0),

		/** maximum grid depth at which a thread can issue the device runtime call ::cudaDeviceSynchronize() to wait on child grid launches to complete */
		DEV_RUNTIME_SYNC_DEPTH(1),

		/** size in bytes of the heap used by the ::malloc() and ::free() device system calls */
		MALLOC_HEAP_SIZE(2),

		/** size in bytes of the FIFO used by the ::printf() device system call */
		PRINTF_FIFO_SIZE(3),

		/** stack size in bytes of each GPU thread */
		STACK_SIZE(4);

		final int nativeValue;

		private Limit(int value) {
			this.nativeValue = value;
		}
	}

	public static enum SharedMemConfig {

		/**
		 * default shared memory bank size
		 */
		DEFAULT_BANK_SIZE(0),

		/**
		 * eight byte shared memory bank width
		 */
		EIGHT_BYTE_BANK_SIZE(2),

		/**
		 * four byte shared memory bank width
		 */
		FOUR_BYTE_BANK_SIZE(1);

		final int nativeValue;

		SharedMemConfig(int value) {
			this.nativeValue = value;
		}
	}

	/** Number of asynchronous engines. */
	public static final int ATTRIBUTE_ASYNC_ENGINE_COUNT = 40;

	/** Device can map host memory into CUDA address space. */
	public static final int ATTRIBUTE_CAN_MAP_HOST_MEMORY = 19;

	/** Typical clock frequency in kilohertz. */
	public static final int ATTRIBUTE_CLOCK_RATE = 13;

	/**
	 * Compute capability version number. This value is the major compute
	 * capability version * 10 + the minor compute capability version, so
	 * a compute capability version 3.5 function would return the value 35.
	 */
	public static final int ATTRIBUTE_COMPUTE_CAPABILITY = -1;

	/** Major compute capability version number. */
	public static final int ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR = 75;

	/** Minor compute capability version number. */
	public static final int ATTRIBUTE_COMPUTE_CAPABILITY_MINOR = 76;

	/** Compute mode (see COMPUTE_MODE_XXX for details). */
	public static final int ATTRIBUTE_COMPUTE_MODE = 20;

	/** Device can possibly execute multiple kernels concurrently. */
	public static final int ATTRIBUTE_CONCURRENT_KERNELS = 31;

	/** Device has ECC support enabled. */
	public static final int ATTRIBUTE_ECC_ENABLED = 32;

	/** Global memory bus width in bits. */
	public static final int ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH = 37;

	/** Device is integrated with host memory. */
	public static final int ATTRIBUTE_INTEGRATED = 18;

	/** Specifies whether there is a run time limit on kernels. */
	public static final int ATTRIBUTE_KERNEL_EXEC_TIMEOUT = 17;

	/** Size of L2 cache in bytes. */
	public static final int ATTRIBUTE_L2_CACHE_SIZE = 38;

	/** Maximum block dimension X. */
	public static final int ATTRIBUTE_MAX_BLOCK_DIM_X = 2;

	/** Maximum block dimension Y. */
	public static final int ATTRIBUTE_MAX_BLOCK_DIM_Y = 3;

	/** Maximum block dimension Z. */
	public static final int ATTRIBUTE_MAX_BLOCK_DIM_Z = 4;

	/** Maximum grid dimension X. */
	public static final int ATTRIBUTE_MAX_GRID_DIM_X = 5;

	/** Maximum grid dimension Y. */
	public static final int ATTRIBUTE_MAX_GRID_DIM_Y = 6;

	/** Maximum grid dimension Z. */
	public static final int ATTRIBUTE_MAX_GRID_DIM_Z = 7;

	/** Maximum pitch in bytes allowed by memory copies. */
	public static final int ATTRIBUTE_MAX_PITCH = 11;

	/** Maximum number of 32-bit registers available per block. */
	public static final int ATTRIBUTE_MAX_REGISTERS_PER_BLOCK = 12;

	/** Maximum shared memory available per block in bytes. */
	public static final int ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK = 8;

	/** Maximum number of threads per block. */
	public static final int ATTRIBUTE_MAX_THREADS_PER_BLOCK = 1;

	/** Maximum resident threads per multiprocessor. */
	public static final int ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR = 39;

	/** Maximum layers in a 1D layered surface. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_LAYERS = 62;

	/** Maximum 1D layered surface width. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_WIDTH = 61;

	/** Maximum 1D surface width. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE1D_WIDTH = 55;

	/** Maximum 2D surface height. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE2D_HEIGHT = 57;

	/** Maximum 2D layered surface height. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_HEIGHT = 64;

	/** Maximum layers in a 2D layered surface. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_LAYERS = 65;

	/** Maximum 2D layered surface width. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_WIDTH = 63;

	/** Maximum 2D surface width. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE2D_WIDTH = 56;

	/** Maximum 3D surface depth. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE3D_DEPTH = 60;

	/** Maximum 3D surface height. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE3D_HEIGHT = 59;

	/** Maximum 3D surface width. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACE3D_WIDTH = 58;

	/** Maximum layers in a cubemap layered surface. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS = 68;

	/** Maximum cubemap layered surface width. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH = 67;

	/** Maximum cubemap surface width. */
	public static final int ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_WIDTH = 66;

	/** Maximum layers in a 1D layered texture. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_LAYERS = 43;

	/** Maximum 1D layered texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_WIDTH = 42;

	/** Maximum 1D linear texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE1D_LINEAR_WIDTH = 69;

	/** Maximum mipmapped 1D texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH = 77;

	/** Maximum 1D texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE1D_WIDTH = 21;

	/** Maximum 2D texture height if CUDA_ARRAY3D_TEXTURE_GATHER is set. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_HEIGHT = 46;

	/** Maximum 2D texture width if CUDA_ARRAY3D_TEXTURE_GATHER is set. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_WIDTH = 45;

	/** Maximum 2D texture height. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT = 23;

	/** Maximum 2D layered texture height. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_HEIGHT = 28;

	/** Maximum layers in a 2D layered texture. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_LAYERS = 29;

	/** Maximum 2D layered texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_WIDTH = 27;

	/** Maximum 2D linear texture height. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_HEIGHT = 71;

	/** Maximum 2D linear texture pitch in bytes. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_PITCH = 72;

	/** Maximum 2D linear texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_WIDTH = 70;

	/** Maximum mipmapped 2D texture height. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT = 74;

	/** Maximum mipmapped 2D texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH = 73;

	/** Maximum 2D texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH = 22;

	/** Maximum 3D texture depth. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH = 26;

	/** Alternate maximum 3D texture depth. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE = 49;

	/** Maximum 3D texture height. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT = 25;

	/** Alternate maximum 3D texture height. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE = 48;

	/** Maximum 3D texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH = 24;

	/** Alternate maximum 3D texture width. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE = 47;

	/** Maximum layers in a cubemap layered texture. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS = 54;

	/** Maximum cubemap layered texture width/height. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH = 53;

	/** Maximum cubemap texture width/height. */
	public static final int ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_WIDTH = 52;

	/** Peak memory clock frequency in kilohertz. */
	public static final int ATTRIBUTE_MEMORY_CLOCK_RATE = 36;

	/** Number of multiprocessors on device. */
	public static final int ATTRIBUTE_MULTIPROCESSOR_COUNT = 16;

	/** PCI bus ID of the device. */
	public static final int ATTRIBUTE_PCI_BUS_ID = 33;

	/** PCI device ID of the device. */
	public static final int ATTRIBUTE_PCI_DEVICE_ID = 34;

	/** PCI domain ID of the device. */
	public static final int ATTRIBUTE_PCI_DOMAIN_ID = 50;

	/** Device supports stream priorities. */
	public static final int ATTRIBUTE_STREAM_PRIORITIES_SUPPORTED = 78;

	/** Alignment requirement for surfaces. */
	public static final int ATTRIBUTE_SURFACE_ALIGNMENT = 30;

	/** Device is using TCC driver model. */
	public static final int ATTRIBUTE_TCC_DRIVER = 35;

	/** Alignment requirement for textures. */
	public static final int ATTRIBUTE_TEXTURE_ALIGNMENT = 14;

	/** Pitch alignment requirement for textures. */
	public static final int ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT = 51;

	/** Memory available on device for __constant__ variables in a kernel in bytes. */
	public static final int ATTRIBUTE_TOTAL_CONSTANT_MEMORY = 9;

	/** Device shares a unified address space with the host. */
	public static final int ATTRIBUTE_UNIFIED_ADDRESSING = 41;

	/** Warp size in threads. */
	public static final int ATTRIBUTE_WARP_SIZE = 10;

	/** Default compute mode (multiple contexts allowed per device). */
	public static final int COMPUTE_MODE_DEFAULT = 0;

	/**
	 * Compute exclusive process mode (at most one context used by a single process
	 * can be present on this device at a time).
	 */
	public static final int COMPUTE_MODE_PROCESS_EXCLUSIVE = 3;

	/** Compute prohibited mode (no contexts can be created on this device at this time). */
	public static final int COMPUTE_MODE_PROHIBITED = 2;

	/**
	 * Exclusive thread mode (at most one context, used by a single thread,
	 * can be present on this device at a time).
	 */
	public static final int COMPUTE_MODE_THREAD_EXCLUSIVE = 1;

	/** Keep local memory allocation after launch. */
	public static final int FLAG_LMEM_RESIZE_TO_MAX = 0x10;

	/** Support mapped pinned allocations. */
	public static final int FLAG_MAP_HOST = 0x08;

	/** Automatic scheduling. */
	public static final int FLAG_SCHED_AUTO = 0x00;

	/** Set blocking synchronization as default scheduling. */
	public static final int FLAG_SCHED_BLOCKING_SYNC = 0x04;

	/** Set spin as default scheduling. */
	public static final int FLAG_SCHED_SPIN = 0x01;

	/** Set yield as default scheduling. */
	public static final int FLAG_SCHED_YIELD = 0x02;

	public static final int MASK_SCHED = 0x07;

	static native void addCallback(int deviceId, long streamHandle,
			Runnable callback) throws CudaException;

	private static native boolean canAccessPeer(int deviceId, int peerDeviceId)
			throws CudaException;

	private static native void disablePeerAccess(int deviceId, int peerDeviceId)
			throws CudaException;

	private static native void enablePeerAccess(int deviceId, int peerDeviceId)
			throws CudaException;

	private static native int getAttribute(int deviceId, int attribute)
			throws CudaException;

	private static native int getCacheConfig(int deviceId) throws CudaException;

	/**
	 * Returns the number of CUDA-capable devices available to the Java host.
	 *
	 * @return the number of available CUDA-capable devices
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public static int getCount() throws CudaException {
		return Cuda.getDeviceCount();
	}

	/**
	 * Returns a number identifying the driver version.
	 *
	 * @return
	 *          the driver version number
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @deprecated
	 *         Use Cuda.getDriverVersion() instead.
	 */
	@Deprecated
	public static int getDriverVersion() throws CudaException {
		return Cuda.getDriverVersion();
	}

	private static native long getFreeMemory(int deviceId) throws CudaException;

	private static native int getGreatestStreamPriority(int deviceId)
			throws CudaException;

	private static native int getLeastStreamPriority(int deviceId)
			throws CudaException;

	private static native long getLimit(int deviceId, int limit)
			throws CudaException;

	private static native String getName(int deviceId) throws CudaException;

	/**
	 * Returns a number identifying the runtime version.
	 *
	 * @return
	 *          the runtime version number
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @deprecated
	 *         Use Cuda.getRuntimeVersion() instead.
	 */
	@Deprecated
	public static int getRuntimeVersion() throws CudaException {
		return Cuda.getRuntimeVersion();
	}

	private static native int getSharedMemConfig(int deviceId)
			throws CudaException;

	private static native long getTotalMemory(int deviceId)
			throws CudaException;

	private static native void setCacheConfig(int deviceId, int config)
			throws CudaException;

	private static native void setLimit(int deviceId, int limit, long value)
			throws CudaException;

	private static native void setSharedMemConfig(int deviceId, int config)
			throws CudaException;

	private static native void synchronize(int deviceId) throws CudaException;

	private final int deviceId;

	/**
	 * Creates a device handle corresponding to {@code deviceId}.
	 * <p>
	 * No checking is done on {@code deviceId}, but it must be non-negative
	 * and less than the value returned {@link #getCount()} to be useful.
	 *
	 * @param deviceId
	 *          an integer identifying the device of interest
	 */
	public CudaDevice(int deviceId) {
		super();
		this.deviceId = deviceId;
		Cuda.loadNatives();
	}

	/**
	 * Queues the given {@code callback} to be executed when the associated
	 * device has completed all previous actions in the default stream.
	 *
	 * @param callback
	 *          code to run
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public void addCallback(Runnable callback) throws CudaException {
		Objects.requireNonNull(callback);
		addCallback(deviceId, 0, callback);
	}

	/**
	 * Returns whether this device can access memory of the specified
	 * {@code peerDevice}.
	 *
	 * @param peerDevice
	 *          the peer device
	 * @return
	 *          true if this device can access memory of {@code peerDevice},
	 *          false otherwise
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public boolean canAccessPeer(CudaDevice peerDevice) throws CudaException {
		return canAccessPeer(deviceId, peerDevice.deviceId);
	}

	/**
	 * Disable access to memory of {@code peerDevice} by this device.
	 *
	 * @param peerDevice
	 *          the peer device
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws SecurityException
	 *          if a security manager exists and the calling thread
	 *          does not have permission to disable peer access
	 */
	public void disablePeerAccess(CudaDevice peerDevice) throws CudaException {
		SecurityManager security = System.getSecurityManager();

		if (security != null) {
			security.checkPermission(CudaPermission.DisablePeerAccess);
		}

		disablePeerAccess(deviceId, peerDevice.deviceId);
	}

	/**
	 * Enable access to memory of {@code peerDevice} by this device.
	 *
	 * @param peerDevice
	 *          the peer device
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws SecurityException
	 *          if a security manager exists and the calling thread
	 *          does not have permission to enable peer access
	 */
	public void enablePeerAccess(CudaDevice peerDevice) throws CudaException {
		SecurityManager security = System.getSecurityManager();

		if (security != null) {
			security.checkPermission(CudaPermission.EnablePeerAccess);
		}

		enablePeerAccess(deviceId, peerDevice.deviceId);
	}

	/**
	 * Does the argument represent the same device as this?
	 */
	@Override
	public boolean equals(Object other) {
		if (this == other) {
			return true;
		}

		if (other instanceof CudaDevice) {
			CudaDevice that = (CudaDevice) other;

			if (this.deviceId == that.deviceId) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Returns the value of the specified {@code attribute}.
	 *
	 * @param attribute
	 *          the attribute to be queried (see ATTRIBUTE_XXX)
	 * @return
	 *          the attribute value
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public int getAttribute(int attribute) throws CudaException {
		return getAttribute(deviceId, attribute);
	}

	/**
	 * Returns the current cache configuration of this device.
	 *
	 * @return
	 *          the current cache configuration
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public CacheConfig getCacheConfig() throws CudaException {
		switch (getCacheConfig(deviceId)) {
		default:
		case 0:
			return CacheConfig.PREFER_NONE;
		case 1:
			return CacheConfig.PREFER_SHARED;
		case 2:
			return CacheConfig.PREFER_L1;
		case 3:
			return CacheConfig.PREFER_EQUAL;
		}
	}

	/**
	 * Returns an integer identifying this device (the value provided when
	 * this object was constructed).
	 *
	 * @return an integer identifying this device
	 */
	public int getDeviceId() {
		return deviceId;
	}

	/**
	 * Returns the amount of free device memory in bytes.
	 *
	 * @return
	 *          the number of bytes of free device memory
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public long getFreeMemory() throws CudaException {
		return getFreeMemory(deviceId);
	}

	/**
	 * Returns the greatest possible priority of a stream on this device.
	 * Note that stream priorities follow a convention where lower numbers imply
	 * greater priorities.
	 *
	 * @return
	 *          the greatest possible priority of a stream on this device
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public int getGreatestStreamPriority() throws CudaException {
		return getGreatestStreamPriority(deviceId);
	}

	/**
	 * Returns the least possible priority of a stream on this device.
	 * Note that stream priorities follow a convention where lower numbers imply
	 * greater priorities.
	 *
	 * @return
	 *          the greatest possible priority of a stream on this device
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public int getLeastStreamPriority() throws CudaException {
		return getLeastStreamPriority(deviceId);
	}

	/**
	 * Returns the value of the specified {@code limit}.
	 *
	 * @param limit
	 *          the limit to be queried
	 * @return
	 *          the value of the specified limit
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public long getLimit(Limit limit) throws CudaException {
		return getLimit(deviceId, limit.nativeValue);
	}

	/**
	 * Returns the name of this device.
	 *
	 * @return
	 *          the name of this device
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public String getName() throws CudaException {
		return getName(deviceId);
	}

	/**
	 * Returns the current shared memory configuration of this device.
	 *
	 * @return
	 *          the current shared memory configuration
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public SharedMemConfig getSharedMemConfig() throws CudaException {
		switch (getSharedMemConfig(deviceId)) {
		default:
		case 0:
			return SharedMemConfig.DEFAULT_BANK_SIZE;
		case 1:
			return SharedMemConfig.FOUR_BYTE_BANK_SIZE;
		case 2:
			return SharedMemConfig.EIGHT_BYTE_BANK_SIZE;
		}
	}

	/**
	 * Returns the total amount of memory on this device in bytes.
	 *
	 * @return
	 *          the number of bytes of device memory
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public long getTotalMemory() throws CudaException {
		return getTotalMemory(deviceId);
	}

	@Override
	public int hashCode() {
		return deviceId;
	}

	/**
	 * Configures the cache of this device.
	 *
	 * @param config
	 *          the desired cache configuration
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws SecurityException
	 *          if a security manager exists and the calling thread
	 *          does not have permission to set device cache configurations
	 */
	public void setCacheConfig(CacheConfig config) throws CudaException {
		SecurityManager security = System.getSecurityManager();

		if (security != null) {
			security.checkPermission(CudaPermission.SetCacheConfig);
		}

		setCacheConfig(deviceId, config.nativeValue);
	}

	/**
	 * Configures the specified {@code limit}.
	 *
	 * @param limit
	 *          the limit to be configured
	 * @param value
	 *          the desired limit value
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws SecurityException
	 *          if a security manager exists and the calling thread
	 *          does not have permission to set device limits
	 */
	public void setLimit(Limit limit, long value) throws CudaException {
		SecurityManager security = System.getSecurityManager();

		if (security != null) {
			security.checkPermission(CudaPermission.SetLimit);
		}

		setLimit(deviceId, limit.nativeValue, value);
	}

	/**
	 * Configures the shared memory of this device.
	 *
	 * @param config
	 *          the desired shared memory configuration
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 * @throws SecurityException
	 *          if a security manager exists and the calling thread does
	 *          not have permission to set device shared memory configurations
	 */
	public void setSharedMemConfig(SharedMemConfig config) throws CudaException {
		SecurityManager security = System.getSecurityManager();

		if (security != null) {
			security.checkPermission(CudaPermission.SetSharedMemConfig);
		}

		setSharedMemConfig(deviceId, config.nativeValue);
	}

	/**
	 * Synchronizes on this device. This method blocks until the associated
	 * device has completed all previous actions in the default stream.
	 *
	 * @throws CudaException
	 *          if a CUDA exception occurs
	 */
	public void synchronize() throws CudaException {
		synchronize(deviceId);
	}
}
