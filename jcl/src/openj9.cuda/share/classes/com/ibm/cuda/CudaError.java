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
 * The {@code CudaError} interface defines the possible values of {@link CudaException#code}.
 */
public interface CudaError {

	/**
	 * The API call returned with no errors. In the case of query calls, this
	 * can also mean that the operation being queried is complete (see
	 * ::cudaEventQuery() and ::cudaStreamQuery()).
	 */
	int Success = 0;

	/**
	 * The device function being invoked (usually via ::cudaLaunch()) was not
	 * previously configured via the ::cudaConfigureCall() function.
	 */
	int MissingConfiguration = 1;

	/**
	 * The API call failed because it was unable to allocate enough memory to
	 * perform the requested operation.
	 */
	int MemoryAllocation = 2;

	/**
	 * The API call failed because the CUDA driver and runtime could not be
	 * initialized.
	 */
	int InitializationError = 3;

	/**
	 * An exception occurred on the device while executing a kernel. Common
	 * causes include dereferencing an invalid device pointer and accessing
	 * out of bounds shared memory. The device cannot be used until
	 * ::cudaThreadExit() is called. All existing device memory allocations
	 * are invalid and must be reconstructed if the program is to continue
	 * using CUDA.
	 */
	int LaunchFailure = 4;

	/**
	 * This indicates that the device kernel took too long to execute. This can
	 * only occur if timeouts are enabled - see the device property
	 * \ref ::cudaDeviceProp::kernelExecTimeoutEnabled "kernelExecTimeoutEnabled"
	 * for more information. The device cannot be used until ::cudaThreadExit()
	 * is called. All existing device memory allocations are invalid and must be
	 * reconstructed if the program is to continue using CUDA.
	 */
	int LaunchTimeout = 6;

	/**
	 * This indicates that a launch did not occur because it did not have
	 * appropriate resources. Although this error is similar to
	 * ::InvalidConfiguration, this error usually indicates that the
	 * user has attempted to pass too many arguments to the device kernel, or the
	 * kernel launch specifies too many threads for the kernel's register count.
	 */
	int LaunchOutOfResources = 7;

	/**
	 * The requested device function does not exist or is not compiled for the
	 * proper device architecture.
	 */
	int InvalidDeviceFunction = 8;

	/**
	 * This indicates that a kernel launch is requesting resources that can
	 * never be satisfied by the current device. Requesting more shared memory
	 * per block than the device supports will trigger this error, as will
	 * requesting too many threads or blocks. See ::cudaDeviceProp for more
	 * device limitations.
	 */
	int InvalidConfiguration = 9;

	/**
	 * This indicates that the device ordinal supplied by the user does not
	 * correspond to a valid CUDA device.
	 */
	int InvalidDevice = 10;

	/**
	 * This indicates that one or more of the parameters passed to the API call
	 * is not within an acceptable range of values.
	 */
	int InvalidValue = 11;

	/**
	 * This indicates that one or more of the pitch-related parameters passed
	 * to the API call is not within the acceptable range for pitch.
	 */
	int InvalidPitchValue = 12;

	/**
	 * This indicates that the symbol name/identifier passed to the API call
	 * is not a valid name or identifier.
	 */
	int InvalidSymbol = 13;

	/**
	 * This indicates that the buffer object could not be mapped.
	 */
	int MapBufferObjectFailed = 14;

	/**
	 * This indicates that the buffer object could not be unmapped.
	 */
	int UnmapBufferObjectFailed = 15;

	/**
	 * This indicates that at least one host pointer passed to the API call is
	 * not a valid host pointer.
	 */
	int InvalidHostPointer = 16;

	/**
	 * This indicates that at least one device pointer passed to the API call is
	 * not a valid device pointer.
	 */
	int InvalidDevicePointer = 17;

	/**
	 * This indicates that the texture passed to the API call is not a valid
	 * texture.
	 */
	int InvalidTexture = 18;

	/**
	 * This indicates that the texture binding is not valid. This occurs if you
	 * call ::cudaGetTextureAlignmentOffset() with an unbound texture.
	 */
	int InvalidTextureBinding = 19;

	/**
	 * This indicates that the channel descriptor passed to the API call is not
	 * valid. This occurs if the format is not one of the formats specified by
	 * ::cudaChannelFormatKind, or if one of the dimensions is invalid.
	 */
	int InvalidChannelDescriptor = 20;

	/**
	 * This indicates that the direction of the memcpy passed to the API call is
	 * not one of the types specified by ::cudaMemcpyKind.
	 */
	int InvalidMemcpyDirection = 21;

	/**
	 * This indicates that a non-float texture was being accessed with linear
	 * filtering. This is not supported by CUDA.
	 */
	int InvalidFilterSetting = 26;

	/**
	 * This indicates that an attempt was made to read a non-float texture as a
	 * normalized float. This is not supported by CUDA.
	 */
	int InvalidNormSetting = 27;

	/**
	 * This indicates that a CUDA Runtime API call cannot be executed because
	 * it is being called during process shut down, at a point in time after
	 * CUDA driver has been unloaded.
	 */
	int CudartUnloading = 29;

	/**
	 * This indicates that an unknown internal error has occurred.
	 */
	int Unknown = 30;

	/**
	 * This indicates that a resource handle passed to the API call was not
	 * valid. Resource handles are opaque types like ::cudaStream_t and
	 * ::cudaEvent_t.
	 */
	int InvalidResourceHandle = 33;

	/**
	 * This indicates that asynchronous operations issued previously have not
	 * completed yet. This result is not actually an error, but must be indicated
	 * differently than ::cudaSuccess (which indicates completion). Calls that
	 * may return this value include ::cudaEventQuery() and ::cudaStreamQuery().
	 */
	int NotReady = 34;

	/**
	 * This indicates that the installed NVIDIA CUDA driver is older than the
	 * CUDA runtime library. This is not a supported configuration. Users should
	 * install an updated NVIDIA display driver to allow the application to run.
	 */
	int InsufficientDriver = 35;

	/**
	 * This indicates that the user has called ::cudaSetValidDevices(),
	 * ::cudaSetDeviceFlags(), ::cudaD3D9SetDirect3DDevice(),
	 * ::cudaD3D10SetDirect3DDevice, ::cudaD3D11SetDirect3DDevice(), or
	 * ::cudaVDPAUSetVDPAUDevice() after initializing the CUDA runtime by
	 * calling non-device management operations (allocating memory and
	 * launching kernels are examples of non-device management operations).
	 * This error can also be returned if using runtime/driver
	 * interoperability and there is an existing ::CUcontext active on the
	 * host thread.
	 */
	int SetOnActiveProcess = 36;

	/**
	 * This indicates that the surface passed to the API call is not a valid
	 * surface.
	 */
	int InvalidSurface = 37;

	/**
	 * This indicates that no CUDA-capable devices were detected by the installed
	 * CUDA driver.
	 */
	int NoDevice = 38;

	/**
	 * This indicates that an uncorrectable ECC error was detected during
	 * execution.
	 */
	int ECCUncorrectable = 39;

	/**
	 * This indicates that a link to a shared object failed to resolve.
	 */
	int SharedObjectSymbolNotFound = 40;

	/**
	 * This indicates that initialization of a shared object failed.
	 */
	int SharedObjectInitFailed = 41;

	/**
	 * This indicates that the ::cudaLimit passed to the API call is not
	 * supported by the active device.
	 */
	int UnsupportedLimit = 42;

	/**
	 * This indicates that multiple global or constant variables (across separate
	 * CUDA source files in the application) share the same string name.
	 */
	int DuplicateVariableName = 43;

	/**
	 * This indicates that multiple textures (across separate CUDA source
	 * files in the application) share the same string name.
	 */
	int DuplicateTextureName = 44;

	/**
	 * This indicates that multiple surfaces (across separate CUDA source
	 * files in the application) share the same string name.
	 */
	int DuplicateSurfaceName = 45;

	/**
	 * This indicates that all CUDA devices are busy or unavailable at the current
	 * time. Devices are often busy/unavailable due to use of
	 * ::cudaComputeModeExclusive, ::cudaComputeModeProhibited or when long
	 * running CUDA kernels have filled up the GPU and are blocking new work
	 * from starting. They can also be unavailable due to memory constraints
	 * on a device that already has active CUDA work being performed.
	 */
	int DevicesUnavailable = 46;

	/**
	 * This indicates that the device kernel image is invalid.
	 */
	int InvalidKernelImage = 47;

	/**
	 * This indicates that there is no kernel image available that is suitable
	 * for the device. This can occur when a user specifies code generation
	 * options for a particular CUDA source file that do not include the
	 * corresponding device configuration.
	 */
	int NoKernelImageForDevice = 48;

	/**
	 * This indicates that the current context is not compatible with this
	 * the CUDA Runtime. This can only occur if you are using CUDA
	 * Runtime/Driver interoperability and have created an existing Driver
	 * context using the driver API. The Driver context may be incompatible
	 * either because the Driver context was created using an older version
	 * of the API, because the Runtime API call expects a primary driver
	 * context and the Driver context is not primary, or because the Driver
	 * context has been destroyed. Please see \ref CUDART_DRIVER "Interactions
	 * with the CUDA Driver API" for more information.
	 */
	int IncompatibleDriverContext = 49;

	/**
	 * This error indicates that a call to ::cudaDeviceEnablePeerAccess() is
	 * trying to re-enable peer addressing on from a context which has already
	 * had peer addressing enabled.
	 */
	int PeerAccessAlreadyEnabled = 50;

	/**
	 * This error indicates that ::cudaDeviceDisablePeerAccess() is trying to
	 * disable peer addressing which has not been enabled yet via
	 * ::cudaDeviceEnablePeerAccess().
	 */
	int PeerAccessNotEnabled = 51;

	/**
	 * This indicates that a call tried to access an exclusive-thread device that
	 * is already in use by a different thread.
	 */
	int DeviceAlreadyInUse = 54;

	/**
	 * This indicates profiler is not initialized for this run. This can
	 * happen when the application is running with external profiling tools
	 * like visual profiler.
	 */
	int ProfilerDisabled = 55;

	/**
	 * An assert triggered in device code during kernel execution. The device
	 * cannot be used again until ::cudaThreadExit() is called. All existing
	 * allocations are invalid and must be reconstructed if the program is to
	 * continue using CUDA.
	 */
	int Assert = 59;

	/**
	 * This error indicates that the hardware resources required to enable
	 * peer access have been exhausted for one or more of the devices
	 * passed to ::cudaEnablePeerAccess().
	 */
	int TooManyPeers = 60;

	/**
	 * This error indicates that the memory range passed to ::cudaHostRegister()
	 * has already been registered.
	 */
	int HostMemoryAlreadyRegistered = 61;

	/**
	 * This error indicates that the pointer passed to ::cudaHostUnregister()
	 * does not correspond to any currently registered memory region.
	 */
	int HostMemoryNotRegistered = 62;

	/**
	 * This error indicates that an OS call failed.
	 */
	int OperatingSystem = 63;

	/**
	 * This error indicates that P2P access is not supported across the given
	 * devices.
	 */
	int PeerAccessUnsupported = 64;

	/**
	 * This error indicates that a device runtime grid launch did not occur
	 * because the depth of the child grid would exceed the maximum supported
	 * number of nested grid launches.
	 */
	int LaunchMaxDepthExceeded = 65;

	/**
	 * This error indicates that a grid launch did not occur because the kernel
	 * uses file-scoped textures which are unsupported by the device runtime.
	 * Kernels launched via the device runtime only support textures created with
	 * the Texture Object API's.
	 */
	int LaunchFileScopedTex = 66;

	/**
	 * This error indicates that a grid launch did not occur because the kernel
	 * uses file-scoped surfaces which are unsupported by the device runtime.
	 * Kernels launched via the device runtime only support surfaces created with
	 * the Surface Object API's.
	 */
	int LaunchFileScopedSurf = 67;

	/**
	 * This error indicates that a call to ::cudaDeviceSynchronize made from
	 * the device runtime failed because the call was made at grid depth greater
	 * than than either the default (2 levels of grids) or user specified device
	 * limit ::cudaLimitDevRuntimeSyncDepth. To be able to synchronize on
	 * launched grids at a greater depth successfully, the maximum nested
	 * depth at which ::cudaDeviceSynchronize will be called must be specified
	 * with the ::cudaLimitDevRuntimeSyncDepth limit to the ::cudaDeviceSetLimit
	 * api before the host-side launch of a kernel using the device runtime.
	 * Keep in mind that additional levels of sync depth require the runtime
	 * to reserve large amounts of device memory that cannot be used for
	 * user allocations.
	 */
	int SyncDepthExceeded = 68;

	/**
	 * This error indicates that a device runtime grid launch failed because
	 * the launch would exceed the limit ::cudaLimitDevRuntimePendingLaunchCount.
	 * For this launch to proceed successfully, ::cudaDeviceSetLimit must be
	 * called to set the ::cudaLimitDevRuntimePendingLaunchCount to be higher
	 * than the upper bound of outstanding launches that can be issued to the
	 * device runtime. Keep in mind that raising the limit of pending device
	 * runtime launches will require the runtime to reserve device memory that
	 * cannot be used for user allocations.
	 */
	int LaunchPendingCountExceeded = 69;

	/**
	 * This error indicates the attempted operation is not permitted.
	 */
	int NotPermitted = 70;

	/**
	 * This error indicates the attempted operation is not supported
	 * on the current system or device.
	 */
	int NotSupported = 71;

	/**
	 * This indicates an internal startup failure in the CUDA runtime.
	 */
	int StartupFailure = 0x7f;
}
