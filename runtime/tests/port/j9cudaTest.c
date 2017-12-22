/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

/**
 * @file
 * @ingroup j9cuda
 * @brief Verify CUDA operations.
 *
 * Exercise the j9cuda API, found in the file @ref j9cuda.cpp.
 */

#include "j9cfg.h"
#include "j9port.h"

#include "testHelpers.h"

#if defined(OMR_OPT_CUDA)

static const J9CudaCacheConfig
cacheConfigs[] = {
	J9CUDA_CACHE_CONFIG_PREFER_EQUAL,
	J9CUDA_CACHE_CONFIG_PREFER_L1,
	J9CUDA_CACHE_CONFIG_PREFER_NONE,
	J9CUDA_CACHE_CONFIG_PREFER_SHARED
};

typedef struct Attribute {
	const char            * name;
	J9CudaDeviceAttribute   attribute;
	int32_t                 minValue;
	int32_t                 maxValue;
} Attribute;

#define ATTRIBUTE_ENTRY(name, minValue, maxValue) \
		{ # name, J9CUDA_DEVICE_ATTRIBUTE_ ## name, minValue, maxValue }

static const Attribute
deviceAttributes[] = {
	ATTRIBUTE_ENTRY(ASYNC_ENGINE_COUNT,                    0, 2),
	ATTRIBUTE_ENTRY(CAN_MAP_HOST_MEMORY,                   0, 1),
	ATTRIBUTE_ENTRY(CLOCK_RATE /*kHz*/,                    0, 5 * 1024 * 1024),
	ATTRIBUTE_ENTRY(COMPUTE_CAPABILITY,                    0, 70),
	ATTRIBUTE_ENTRY(COMPUTE_CAPABILITY_MAJOR,              0, 7),
	ATTRIBUTE_ENTRY(COMPUTE_CAPABILITY_MINOR,              0, 5),
	ATTRIBUTE_ENTRY(COMPUTE_MODE,                          0, 3),
	ATTRIBUTE_ENTRY(CONCURRENT_KERNELS,                    0, 1),
	ATTRIBUTE_ENTRY(ECC_ENABLED,                           0, 1),
	ATTRIBUTE_ENTRY(GLOBAL_MEMORY_BUS_WIDTH,               128, 384),
	ATTRIBUTE_ENTRY(INTEGRATED,                            0, 1),
	ATTRIBUTE_ENTRY(KERNEL_EXEC_TIMEOUT,                   0, 1),
	ATTRIBUTE_ENTRY(L2_CACHE_SIZE,                         0, 4 * 1024 * 1024),
	ATTRIBUTE_ENTRY(MAX_BLOCK_DIM_X,                       512, 1024),
	ATTRIBUTE_ENTRY(MAX_BLOCK_DIM_Y,                       512, 1024),
	ATTRIBUTE_ENTRY(MAX_BLOCK_DIM_Z,                       64, 64),
	ATTRIBUTE_ENTRY(MAX_GRID_DIM_X,                        65535, 2147483647),
	ATTRIBUTE_ENTRY(MAX_GRID_DIM_Y,                        65535, 65535),
	ATTRIBUTE_ENTRY(MAX_GRID_DIM_Z,                        1, 65535),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE1D_LAYERED_LAYERS,      0, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE1D_LAYERED_WIDTH,       0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE1D_WIDTH,               0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_HEIGHT,              0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_LAYERED_HEIGHT,      0, 32 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_LAYERED_LAYERS,      0, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_LAYERED_WIDTH,       0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_WIDTH,               0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE3D_DEPTH,               0, 4 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE3D_HEIGHT,              0, 32 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE3D_WIDTH,               0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS, 0, 2046),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH,  0, 32 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACECUBEMAP_WIDTH,          0, 32 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_LAYERED_LAYERS,      512, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_LAYERED_WIDTH,       8 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_LINEAR_WIDTH,        128 * 1024 * 1024, 128 * 1024 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH,     8 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_WIDTH,               8 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_GATHER_HEIGHT,       0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_GATHER_WIDTH,        0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_HEIGHT,              32 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LAYERED_HEIGHT,      8 * 1024, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LAYERED_LAYERS,      512, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LAYERED_WIDTH,       8 * 1024, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LINEAR_HEIGHT,       65000, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LINEAR_PITCH,        1 * 1024, 1 * 1024 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LINEAR_WIDTH,        65000, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT,    8 * 1024, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH,     8 * 1024, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_WIDTH,               64 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_DEPTH,               2 * 1024, 4 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE,     0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_HEIGHT,              2 * 1024, 4 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE,    0, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_WIDTH,               2 * 1024, 4 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE,     0, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS, 0, 2046),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH,  0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURECUBEMAP_WIDTH,          0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAX_PITCH,                             2147483647, 2147483647),
	ATTRIBUTE_ENTRY(MAX_REGISTERS_PER_BLOCK,               16 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAX_SHARED_MEMORY_PER_BLOCK,           16 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAX_THREADS_PER_BLOCK,                 512, 1 * 1024),
	ATTRIBUTE_ENTRY(MAX_THREADS_PER_MULTIPROCESSOR,        512, 2 * 1024),
	ATTRIBUTE_ENTRY(MEMORY_CLOCK_RATE /*kHz*/,             500 * 1024, 6 * 1024 * 1024),
	ATTRIBUTE_ENTRY(MULTIPROCESSOR_COUNT,                  1, 64),
	ATTRIBUTE_ENTRY(PCI_BUS_ID,                            0, 255),
	ATTRIBUTE_ENTRY(PCI_DEVICE_ID,                         0, 31),
	ATTRIBUTE_ENTRY(PCI_DOMAIN_ID,                         0, 255),
	ATTRIBUTE_ENTRY(STREAM_PRIORITIES_SUPPORTED,           0, 1),
	ATTRIBUTE_ENTRY(SURFACE_ALIGNMENT,                     1, 1 * 1024),
	ATTRIBUTE_ENTRY(TCC_DRIVER,                            0, 1),
	ATTRIBUTE_ENTRY(TEXTURE_ALIGNMENT,                     256, 1 * 1024),
	ATTRIBUTE_ENTRY(TEXTURE_PITCH_ALIGNMENT,               32, 1 * 1024),
	ATTRIBUTE_ENTRY(TOTAL_CONSTANT_MEMORY,                 64 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(UNIFIED_ADDRESSING,                    0, 1),
	ATTRIBUTE_ENTRY(WARP_SIZE,                             32, 32),
	{ NULL, (J9CudaDeviceAttribute)0, 0, 0 } /* terminator */
};

typedef struct Limit {
	const char        * name;
	J9CudaDeviceLimit   limit;
	int32_t             minComputeCapability;
	uint32_t            minValue;
	uint32_t            maxValue;
} Limit;

#define LIMIT_ENTRY(name, minComputeCapability, minValue, maxValue) \
		{ # name, J9CUDA_DEVICE_LIMIT_ ## name, minComputeCapability, minValue, maxValue }

static const Limit
deviceLimits[] = {
	LIMIT_ENTRY(DEV_RUNTIME_PENDING_LAUNCH_COUNT, 35, 4, 4 * 1024),
	LIMIT_ENTRY(DEV_RUNTIME_SYNC_DEPTH,           35, 0, 16),
	LIMIT_ENTRY(MALLOC_HEAP_SIZE,                 20, 4 * 1024, 16 * 1024 * 1024),
	LIMIT_ENTRY(PRINTF_FIFO_SIZE,                 20, 4 * 1024, 8 * 1024 * 1024),
	LIMIT_ENTRY(STACK_SIZE,                       20, 1 * 1024, 256 * 1024),
	{ NULL, (J9CudaDeviceLimit)0, 0, 0 } /* terminator */
};

static const J9CudaError errors[] = {
	J9CUDA_NO_ERROR,
	J9CUDA_ERROR_MISSING_CONFIGURATION,
	J9CUDA_ERROR_MEMORY_ALLOCATION,
	J9CUDA_ERROR_INITIALIZATION_ERROR,
	J9CUDA_ERROR_LAUNCH_FAILURE,
	J9CUDA_ERROR_LAUNCH_TIMEOUT,
	J9CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES,
	J9CUDA_ERROR_INVALID_DEVICE_FUNCTION,
	J9CUDA_ERROR_INVALID_CONFIGURATION,
	J9CUDA_ERROR_INVALID_DEVICE,
	J9CUDA_ERROR_INVALID_VALUE,
	J9CUDA_ERROR_INVALID_PITCH_VALUE,
	J9CUDA_ERROR_INVALID_SYMBOL,
	J9CUDA_ERROR_MAP_BUFFER_OBJECT_FAILED,
	J9CUDA_ERROR_UNMAP_BUFFER_OBJECT_FAILED,
	J9CUDA_ERROR_INVALID_HOST_POINTER,
	J9CUDA_ERROR_INVALID_DEVICE_POINTER,
	J9CUDA_ERROR_INVALID_TEXTURE,
	J9CUDA_ERROR_INVALID_TEXTURE_BINDING,
	J9CUDA_ERROR_INVALID_CHANNEL_DESCRIPTOR,
	J9CUDA_ERROR_INVALID_MEMCPY_DIRECTION,
	J9CUDA_ERROR_INVALID_FILTER_SETTING,
	J9CUDA_ERROR_INVALID_NORM_SETTING,
	J9CUDA_ERROR_CUDART_UNLOADING,
	J9CUDA_ERROR_UNKNOWN,
	J9CUDA_ERROR_INVALID_RESOURCE_HANDLE,
	J9CUDA_ERROR_NOT_READY,
	J9CUDA_ERROR_INSUFFICIENT_DRIVER,
	J9CUDA_ERROR_SET_ON_ACTIVE_PROCESS,
	J9CUDA_ERROR_INVALID_SURFACE,
	J9CUDA_ERROR_NO_DEVICE,
	J9CUDA_ERROR_ECCUNCORRECTABLE,
	J9CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND,
	J9CUDA_ERROR_SHARED_OBJECT_INIT_FAILED,
	J9CUDA_ERROR_UNSUPPORTED_LIMIT,
	J9CUDA_ERROR_DUPLICATE_VARIABLE_NAME,
	J9CUDA_ERROR_DUPLICATE_TEXTURE_NAME,
	J9CUDA_ERROR_DUPLICATE_SURFACE_NAME,
	J9CUDA_ERROR_DEVICES_UNAVAILABLE,
	J9CUDA_ERROR_INVALID_KERNEL_IMAGE,
	J9CUDA_ERROR_NO_KERNEL_IMAGE_FOR_DEVICE,
	J9CUDA_ERROR_INCOMPATIBLE_DRIVER_CONTEXT,
	J9CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED,
	J9CUDA_ERROR_PEER_ACCESS_NOT_ENABLED,
	J9CUDA_ERROR_DEVICE_ALREADY_IN_USE,
	J9CUDA_ERROR_PROFILER_DISABLED,
	J9CUDA_ERROR_ASSERT,
	J9CUDA_ERROR_TOO_MANY_PEERS,
	J9CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED,
	J9CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED,
	J9CUDA_ERROR_OPERATING_SYSTEM,
	J9CUDA_ERROR_PEER_ACCESS_UNSUPPORTED,
	J9CUDA_ERROR_LAUNCH_MAX_DEPTH_EXCEEDED,
	J9CUDA_ERROR_LAUNCH_FILE_SCOPED_TEX,
	J9CUDA_ERROR_LAUNCH_FILE_SCOPED_SURF,
	J9CUDA_ERROR_SYNC_DEPTH_EXCEEDED,
	J9CUDA_ERROR_LAUNCH_PENDING_COUNT_EXCEEDED,
	J9CUDA_ERROR_NOT_PERMITTED,
	J9CUDA_ERROR_NOT_SUPPORTED,
	J9CUDA_ERROR_STARTUP_FAILURE,
	J9CUDA_ERROR_NOT_FOUND
};

static const J9CudaFunctionAttribute functionAttributes[] = {
	J9CUDA_FUNCTION_ATTRIBUTE_BINARY_VERSION,
	J9CUDA_FUNCTION_ATTRIBUTE_CONST_SIZE_BYTES,
	J9CUDA_FUNCTION_ATTRIBUTE_LOCAL_SIZE_BYTES,
	J9CUDA_FUNCTION_ATTRIBUTE_MAX_THREADS_PER_BLOCK,
	J9CUDA_FUNCTION_ATTRIBUTE_NUM_REGS,
	J9CUDA_FUNCTION_ATTRIBUTE_PTX_VERSION,
	J9CUDA_FUNCTION_ATTRIBUTE_SHARED_SIZE_BYTES
};

static const J9CudaSharedMemConfig
sharedMemConfigs[] = {
	J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE,
	J9CUDA_SHARED_MEM_CONFIG_4_BYTE_BANK_SIZE,
	J9CUDA_SHARED_MEM_CONFIG_8_BYTE_BANK_SIZE
};

#define LENGTH_OF(array) (sizeof(array) / sizeof((array)[0]))

/* Parameters of our linear congruential generator */
#define MULTIPLIER 25214903917
#define INCREMENT  11

/**
 * Fill memory with a semi-random pattern.
 */
static void
fillPattern(void * buffer, uintptr_t size, uint32_t seed)
{
	uint8_t * bufferBytes = (uint8_t *)buffer;
	uint64_t value = seed << 16;
	uint64_t i = 0;

	for (i = 0; i < seed; ++i) {
		value = (value * MULTIPLIER) + INCREMENT;
	}

	for (i = 0; i < size; ++i) {
		value = (value * MULTIPLIER) + INCREMENT;
		bufferBytes[i] = (uint8_t)(value >> 16);
	}
}

/**
 * Test whether memory contains a semi-random pattern.
 */
static BOOLEAN
verifyPattern(const void * buffer, uintptr_t size, uint32_t seed)
{
	const uint8_t * bufferBytes = (const uint8_t *)buffer;
	uint64_t value = seed << 16;
	uint64_t i = 0;

	for (i = 0; i < seed; ++i) {
		value = (value * MULTIPLIER) + INCREMENT;
	}

	for (i = 0; i < size; ++i) {
		value = (value * MULTIPLIER) + INCREMENT;

		if ((uint8_t)(value >> 16) != bufferBytes[i]) {
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * Test whether memory contains a repeating pattern.
 */
static BOOLEAN
verifyFill(const void * buffer, uintptr_t size, const void * fill, uintptr_t fillSize)
{
	const uint8_t * bufferBytes = (const uint8_t *)buffer;
	const uint8_t * fillBytes = (const uint8_t *)fill;
	uint64_t bufferIndex = 0;
	uint64_t fillIndex = 0;

	for (bufferIndex = 0; bufferIndex < size; ++bufferIndex) {
		if (bufferBytes[bufferIndex] != fillBytes[fillIndex]) {
			return FALSE;
		}

		fillIndex += 1;
		if (fillIndex >= fillSize) {
			fillIndex = 0;
		}
	}

	return TRUE;
}

/**
 * Normalize an API version number encoded as (1000 * major + 10 * minor)
 * to (10 * major + minor) for consistency with other scaled values like
 * compute capability. For example, runtime version 5050 maps to 55.
 *
 * @param[in] apiVersion  the API version number
 * @return a normalized version number
 */
static uint32_t
normalizeVersion(uint32_t apiVersion)
{
	uint32_t major = (apiVersion / 1000);
	uint32_t minor = (apiVersion /   10) % 10;

	return (major * 10) + minor;
}

/**
 * Verify basic functions.
 *
 * Ensure that the number of devices present can be determined.
 * If there are any devices, verify that we can query the driver
 * and runtime API versions.
 *
 * @param[in]  portLibrary    the port library under test
 * @param[out] deviceCountOut the number of devices available to this process
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testBasic(struct J9PortLibrary * portLibrary, uint32_t * deviceCountOut)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "basic";
	int32_t rc = 0;
	uint32_t deviceCount = 0;
	uint32_t deviceId = 0;
	uint32_t driverVersion = 0;
	uint32_t runtimeVersion = 0;

	reportTestEntry(portLibrary, testName);

	rc = j9cuda_deviceGetCount(&deviceCount);

	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not determine the number of available devices\n");
		deviceCount = 0;
	}

	*deviceCountOut = deviceCount;
	outputComment(PORTLIB, "j9cuda: found %d device%s\n", deviceCount, (1 == deviceCount) ? "" : "s");

	if (0 != deviceCount) {
		rc = j9cuda_driverGetVersion(&driverVersion);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not determine the CUDA driver version\n");
		} else {
			outputComment(PORTLIB, "j9cuda: driver version: %d\n", driverVersion);
		}

		rc = j9cuda_runtimeGetVersion(&runtimeVersion);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not determine the CUDA runtime version\n");
		} else {
			outputComment(PORTLIB, "j9cuda: runtime version: %d\n", runtimeVersion);
		}
	}

	{
		J9CudaConfig * config = OMRPORT_FROM_J9PORT(PORTLIB)->cuda_configData;

		if (NULL == config) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Config data is missing\n");
		} else {
			if (NULL == config->getSummaryData) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Config: getSummaryData is missing\n");
			} else {
				J9CudaSummaryDescriptor summary;

				rc = config->getSummaryData(OMRPORT_FROM_J9PORT(PORTLIB), &summary);

				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Config: getSummaryData returned %d\n", rc);
				} else {
					if (deviceCount != summary.deviceCount) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "Config device count: expected %u, found %u\n",
								deviceCount, summary.deviceCount);
					}

					if (normalizeVersion(driverVersion) != summary.driverVersion) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "Config driver version: expected %u, found %u\n",
								normalizeVersion(driverVersion), summary.driverVersion);
					}

					if (normalizeVersion(runtimeVersion) != summary.runtimeVersion) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "Config runtime version: expected %u, found %u\n",
								normalizeVersion(runtimeVersion), summary.runtimeVersion);
					}
				}
			}

			if (NULL == config->getDeviceData) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Config: getDeviceData is missing\n");
			} else {
				for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
					J9CudaDeviceDescriptor detail;

					rc = config->getDeviceData(OMRPORT_FROM_J9PORT(PORTLIB), deviceId, &detail);

					if (0 != rc) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "Config: getDeviceData returned %d\n", rc);
					}
				}
			}
		}
	}

	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		 rc = j9cuda_deviceReset(deviceId);

		 if (0 != rc) {
			 outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not reset device %u\n", deviceId);
		 }
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Is a message for the given error code required when
 * the specified number of devices are available?
 *
 * @param[in] error        the error code
 * @param[in] deviceCount  the number of available devices
 */
static BOOLEAN
isErrorStringRequired(int32_t error, uint32_t deviceCount)
{
	if (0 != deviceCount) {
		return TRUE;
	}

	if ((J9CUDA_NO_ERROR == error) || (J9CUDA_ERROR_NO_DEVICE == error)) {
		return TRUE;
	}

	return FALSE;
}

/**
 * Verify errors API.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testErrors(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "errors";
	uint32_t index = 0;

	reportTestEntry(portLibrary, testName);

	for (index = 0; index < LENGTH_OF(errors); ++index) {
		int32_t error = errors[index];
		const char * text = j9cuda_getErrorString(error);

		if (NULL != text) {
			outputComment(PORTLIB, "error %d: %s\n", error, text);
		} else if (isErrorStringRequired(error, deviceCount)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "No text for error %d\n", error);
		}
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify event API.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testEvents(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "events";
	uint32_t deviceId = 0;

	reportTestEntry(portLibrary, testName);

	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		J9CudaEvent event1 = NULL;
		J9CudaEvent event2 = NULL;
		float elapsedMillis = 0;
		int32_t rc = 0;

		rc = j9cuda_eventCreate(deviceId, J9CUDA_EVENT_FLAG_DEFAULT, &event1);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create event: %d\n", rc);
			goto deviceCleanup;
		} else if (NULL == event1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Created null event\n");
			goto deviceCleanup;
		}

		rc = j9cuda_eventCreate(deviceId, J9CUDA_EVENT_FLAG_DEFAULT, &event2);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create event: %d\n", rc);
			goto deviceCleanup;
		} else if (NULL == event2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Created null event\n");
			goto deviceCleanup;
		}

		rc = j9cuda_eventRecord(deviceId, event1, NULL);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to record event: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_eventRecord(deviceId, event2, NULL);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to record event: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_eventSynchronize(event2);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to synchronize with event: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_eventQuery(event1);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Event should be ready: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_eventQuery(event2);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Event should be ready: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_eventElapsedTime(event1, event2, &elapsedMillis);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to query elapsed time: %d\n", rc);
			goto deviceCleanup;
		}

		if (0 > elapsedMillis) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Elapsed time should be non-negative: %f\n", elapsedMillis);
		}

deviceCleanup:
		if (NULL != event1) {
			rc = j9cuda_eventDestroy(deviceId, event1);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to destroy event: %d\n", rc);
			}
		}

		if (NULL != event2) {
			rc = j9cuda_eventDestroy(deviceId, event2);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to destroy event: %d\n", rc);
			}
		}
	}

	return reportTestExit(portLibrary, testName);
}

#include "j9cuda_ptx.h"

/**
 * Verify linker API.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testLinker(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "linker";
	uint32_t deviceId = 0;

	reportTestEntry(portLibrary, testName);

	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		int32_t computeCapability = 0;
		void * image = NULL;
		uintptr_t imageSize;
		J9CudaLinker linker = NULL;
		int32_t rc = 0;

		rc = j9cuda_deviceGetAttribute(deviceId, J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY, &computeCapability);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to query compute capability: %d\n", rc);
			continue;
		}

		if (20 > computeCapability) {
			/* separate compilation requires compute capability 2.0 or above */
			continue;
		}

		rc = j9cuda_linkerCreate(deviceId, NULL, &linker);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create linker: %d\n", rc);
			continue;
		} else if (NULL == linker) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Created null linker\n");
			goto cleanup;
		}

		rc = j9cuda_linkerAddData(deviceId, linker, J9CUDA_JIT_INPUT_TYPE_PTX,
		        fragment1_ptx, sizeof(fragment1_ptx), "fragment1", NULL);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to add link fragment: %d\n", rc);
			goto cleanup;
		}

		rc = j9cuda_linkerAddData(deviceId, linker, J9CUDA_JIT_INPUT_TYPE_PTX,
				fragment2_ptx, sizeof(fragment2_ptx), "fragment2", NULL);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to add link fragment: %d\n", rc);
			goto cleanup;
		}

		rc = j9cuda_linkerComplete(deviceId, linker, &image, &imageSize);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to complete link: %d\n", rc);
		} else {
			if (NULL == image) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "null linker image\n");
			}

			if (0 == imageSize) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "empty linker image\n");
			}
		}

cleanup:
		rc = j9cuda_linkerDestroy(deviceId, linker);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to destroy linker: %d\n", rc);
		}
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify function API.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceId     the current device
 * @param[in] function     the function to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testFunction(struct J9PortLibrary * portLibrary, uint32_t deviceId, J9CudaFunction function)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	uint32_t index = 0;
	int32_t rc = TEST_FAIL;
	int32_t value = 0;

	for (index = 0; index < LENGTH_OF(functionAttributes); ++index) {
		rc = j9cuda_funcGetAttribute(deviceId, function, functionAttributes[index], &value);

		if (0 != rc) {
			outputComment(PORTLIB, "Failed to get function attribute(%d): %d\n", functionAttributes[index], rc);
			goto done;
		}
	}

	for (index = 0; index < LENGTH_OF(cacheConfigs); ++index) {
		rc = j9cuda_funcSetCacheConfig(deviceId, function, cacheConfigs[index]);

		if (0 != rc) {
			outputComment(PORTLIB, "Failed to set function cache config(%d): %d\n", cacheConfigs[index], rc);
			goto done;
		}
	}

	for (index = 0; index < LENGTH_OF(sharedMemConfigs); ++index) {
		rc = j9cuda_funcSetSharedMemConfig(deviceId, function, sharedMemConfigs[index]);

		if (0 != rc) {
			outputComment(PORTLIB, "Failed to set function shared memory config(%d): %d\n", sharedMemConfigs[index], rc);
			goto done;
		}
	}

	 rc = TEST_PASS;

done:
	return rc;
}

/**
 * A structure populated by a device through use of parameters_ptx/store.
 */
typedef struct Packed {
	double   doubleValue;
	int64_t  longValue;
	uint64_t pointer;
	float    floatValue;
	int32_t  intValue;
	uint16_t charValue;
	int16_t  shortValue;
	int8_t   byteValue;
} Packed;

/**
 * Verify launch API.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testLaunch(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "launch";
	uint32_t deviceId = 0;

	reportTestEntry(portLibrary, testName);

	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		uintptr_t bufferSize = sizeof(Packed);
		void * deviceBuf = NULL;
		Packed expected;
		J9CudaFunction function = NULL;
		void * kernelParms[9];
		J9CudaModule module = NULL;
		int32_t rc = 0;
		Packed result;

		rc = j9cuda_moduleLoad(deviceId, parameters_ptx, NULL, &module);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to load module: %d\n", rc);
			continue;
		} else if (NULL == module) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Created null module\n");
			goto cleanup;
		}

		rc = j9cuda_moduleGetFunction(deviceId, module, "store", &function);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to find function: %d\n", rc);
			goto cleanup;
		} else if (NULL == function) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "null function address\n");
			goto cleanup;
		}

		rc = j9cuda_deviceAlloc(deviceId, bufferSize, &deviceBuf);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate device memory: %d\n", rc);
			goto cleanup;
		} else if (NULL == deviceBuf) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "allocated null device address\n");
			goto cleanup;
		}

		expected.byteValue   = 85;
		expected.charValue   = 33;
		expected.doubleValue = 3.14159265358979323846; /* pi */
		expected.floatValue  = 2.71828182845904523536f; /* e */
		expected.intValue    = 28;
		expected.longValue   = 42;
		expected.pointer     = (uintptr_t)deviceBuf;
		expected.shortValue  = 85;

		kernelParms[0] = &deviceBuf;
		kernelParms[1] = &bufferSize;
		kernelParms[2] = &expected.byteValue;
		kernelParms[3] = &expected.charValue;
		kernelParms[4] = &expected.doubleValue;
		kernelParms[5] = &expected.floatValue;
		kernelParms[6] = &expected.intValue;
		kernelParms[7] = &expected.longValue;
		kernelParms[8] = &expected.shortValue;

		rc = j9cuda_launchKernel(deviceId, function, 1, 1, 1, 32, 1, 1, 0, NULL, kernelParms);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to lauch kernel: %d\n", rc);
			goto cleanup;
		}

		rc = j9cuda_deviceSynchronize(deviceId);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to synchronize with device: %d\n", rc);
			goto cleanup;
		}

		rc = j9cuda_memcpyDeviceToHost(deviceId, &result, deviceBuf, bufferSize);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Device to host copy failed: %d\n", rc);
			goto cleanup;
		}

		if (result.doubleValue != expected.doubleValue) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Launch doubleValue result: %f, expected %f\n",
					result.doubleValue, expected.doubleValue);
		}

		if (result.longValue != expected.longValue) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Launch longValue result: %lld, expected %lld\n",
					result.longValue, expected.longValue);
		}

		if (result.pointer != expected.pointer) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Launch pointer result: %p, expected %p\n",
					result.pointer, expected.pointer);
		}

		if (result.floatValue != expected.floatValue) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Launch floatValue result: %f, expected %f\n",
					result.floatValue, expected.floatValue);
		}

		if (result.intValue != expected.intValue) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Launch intValue result: %d, expected %d\n",
					result.intValue, expected.intValue);
		}

		if (result.charValue != expected.charValue) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Launch charValue result: %u, expected %u\n",
					result.charValue, expected.charValue);
		}

		if (result.shortValue != expected.shortValue) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Launch shortValue result: %d, expected %f\n",
					result.shortValue, expected.shortValue);
		}

		if (result.byteValue != expected.byteValue) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Launch byteValue result: %d, expected %d\n",
					result.byteValue, expected.byteValue);
		}

cleanup:
		if (NULL != deviceBuf) {
			rc = j9cuda_deviceFree(deviceId, deviceBuf);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to free device memory: %d\n", rc);
			}
		}

		rc = j9cuda_moduleUnload(deviceId, module);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to unload module: %d\n", rc);
		}
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify module API.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testModules(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "modules";
	uint32_t deviceId = 0;

	reportTestEntry(portLibrary, testName);

	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		uintptr_t address;
		J9CudaFunction function = NULL;
		J9CudaModule module = NULL;
		int32_t rc = 0;
		uintptr_t size;

		rc = j9cuda_moduleLoad(deviceId, module_ptx, NULL, &module);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to load module: %d\n", rc);
			continue;
		}

		rc = j9cuda_moduleGetFunction(deviceId, module, "stepFirst", &function);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to find function: %d\n", rc);
		} else {
			if (NULL == function) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "null function address\n");
			} else {
				rc = testFunction(portLibrary, deviceId, function);

				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Function test(s) failed\n");
				}
			}
		}

		rc = j9cuda_moduleGetFunction(deviceId, module, "swapCount", &function);

		if (0 == rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Function should not be found\n");
		}

		rc = j9cuda_moduleGetGlobal(deviceId, module, "swapCount", &address, &size);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to find global: %d\n", rc);
		} else {
			if (0 == address) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "null global symbol address\n");
			}

			if (0 == size) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "global symbol too small\n");
			}
		}

		rc = j9cuda_moduleGetSurfaceRef(deviceId, module, "surfaceHandle", &address);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to find surface: %d\n", rc);
		} else if (0 == address) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "null surface address\n");
		}

		rc = j9cuda_moduleGetTextureRef(deviceId, module, "textureHandle", &address);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to find texture: %d\n", rc);
		} else if (0 == address) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "null texture address\n");
		}

		rc = j9cuda_moduleUnload(deviceId, module);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to unload module: %d\n", rc);
		}
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify peer transfer API.
 *
 * @param[in] portLibrary   the port library under test
 * @param[in] deviceId      the current device
 * @param[in] peerDeviceId  the peer device
 */
static void
testPeerTransfer(struct J9PortLibrary * portLibrary, uint32_t deviceId, uint32_t peerDeviceId)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	uintptr_t const BufferBytes = 2 * 1024 * 1024;
	const char * const testName = "peer transfer";
	void * deviceBuf = NULL;
	void * peerBuf = NULL;
	void * hostBuf = NULL;
	int32_t rc = 0;

	rc = j9cuda_hostAlloc(BufferBytes, J9CUDA_HOST_ALLOC_DEFAULT, &hostBuf);

	if ((0 != rc) || (NULL == hostBuf)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not allocate host memory\n");
		goto cleanup;
	}

	rc = j9cuda_deviceAlloc(deviceId, BufferBytes, &deviceBuf);

	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate device memory: %d\n", rc);
		goto cleanup;
	} else if (NULL == deviceBuf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "allocated null device address\n");
		goto cleanup;
	}

	rc = j9cuda_deviceAlloc(peerDeviceId, BufferBytes, &peerBuf);

	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate device memory: %d\n", rc);
		goto cleanup;
	} else if (NULL == peerBuf) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "allocated null device address\n");
		goto cleanup;
	}

	fillPattern(hostBuf, BufferBytes, deviceId);

	rc = j9cuda_memcpyHostToDevice(deviceId, deviceBuf, hostBuf, BufferBytes);

	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Host to device copy failed: %d\n", rc);
		goto cleanup;
	}

	rc = j9cuda_memcpyPeer(peerDeviceId, peerBuf, deviceId, deviceBuf, BufferBytes);

	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Device to peer copy failed: %d\n", rc);
		goto cleanup;
	}

	memset(hostBuf, 0, BufferBytes);

	rc = j9cuda_memcpyDeviceToHost(peerDeviceId, hostBuf, peerBuf, BufferBytes);

	if (0 != rc) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Device to host copy failed: %d\n", rc);
		goto cleanup;
	}

	if (!verifyPattern(hostBuf, BufferBytes, deviceId)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Data transferred does not match expected pattern\n", rc);
	}

cleanup:
	if (NULL != hostBuf) {
		rc = j9cuda_hostFree(hostBuf);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to free host memory: %d\n", rc);
		}
	}

	if (NULL != deviceBuf) {
		rc = j9cuda_deviceFree(deviceId, deviceBuf);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to free device memory: %d\n", rc);
		}
	}

	if (NULL != peerBuf) {
		rc = j9cuda_deviceFree(peerDeviceId, peerBuf);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to free device memory: %d\n", rc);
		}
	}
}

/**
 * Verify device peering API.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testPeerAccess(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "peer access";
	uint32_t deviceId = 0;

	reportTestEntry(portLibrary, testName);

	// heading
	outputComment(PORTLIB, "       |");
	outputComment(PORTLIB, "Peer\n");

	outputComment(PORTLIB, "Device |");
	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		outputComment(PORTLIB, "%4d", deviceId);
	}
	outputComment(PORTLIB, "\n");

	outputComment(PORTLIB, "------ |");
	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		outputComment(PORTLIB, "----");
	}
	outputComment(PORTLIB, "\n");

	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		uint32_t peerDeviceId = 0;

		outputComment(PORTLIB, "%6d |", deviceId);

		for (peerDeviceId = 0; peerDeviceId < deviceCount; ++peerDeviceId) {
			BOOLEAN canAccess = FALSE;
			int32_t rc = 0;

			if (peerDeviceId == deviceId) {
				outputComment(PORTLIB, "%4c", '-');
				continue;
			}

			rc = j9cuda_deviceCanAccessPeer(deviceId, peerDeviceId, &canAccess);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS,
						"Could not query peer access from device %d to device %d\n",
						deviceId, peerDeviceId);
				continue;
			} else {
				outputComment(PORTLIB, "%4c", canAccess ? 'Y' : 'N');
			}

			/*
			 * Execute two of the three following blocks to toggle peer access
			 * on then off or, off then on.
			 */
			if (!canAccess) {
				rc = j9cuda_deviceEnablePeerAccess(deviceId, peerDeviceId);

				if (J9CUDA_ERROR_PEER_ACCESS_UNSUPPORTED == rc) {
					continue;
				} else if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS,
							"Unexpected error enabling peer access from device %d to device %d\n",
							deviceId, peerDeviceId);
					continue;
				}
			}

			{
				rc = j9cuda_deviceDisablePeerAccess(deviceId, peerDeviceId);

				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS,
							"Unexpected error disabling peer access from device %d to device %d\n",
							deviceId, peerDeviceId);
					continue;
				}
			}

			testPeerTransfer(portLibrary, deviceId, peerDeviceId);

			if (canAccess) {
				rc = j9cuda_deviceEnablePeerAccess(deviceId, peerDeviceId);

				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS,
							"Unexpected error restoring peer access from device %d to device %d\n",
							deviceId, peerDeviceId);
				}
			}
		}

		outputComment(PORTLIB, "\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify querying device properties.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testProperties(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "properties";
	uint32_t deviceId = 0;

	reportTestEntry(portLibrary, testName);

	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		uint32_t index = 0;
		int32_t rc = 0;

		{ // name
			char deviceName[200];

			rc = j9cuda_deviceGetName(deviceId, sizeof(deviceName), deviceName);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not get name of device %u\n", deviceId);
			} else {
				outputComment(PORTLIB, "Device %u: %s\n", deviceId, deviceName);
			}
		}

		{ // global memory info
			uintptr_t freeBytes = 0;
			uintptr_t totalBytes = 0;

			rc = j9cuda_deviceGetMemInfo(deviceId, &freeBytes, &totalBytes);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not get memory info for device %u\n", deviceId);
			} else if (freeBytes > totalBytes) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Free memory %pu exceeds total %pu\n", freeBytes, totalBytes);
			} else {
				outputComment(PORTLIB, "  %zu bytes free of %zu total bytes\n", freeBytes, totalBytes);
			}
		}

		{ // cache config
			J9CudaCacheConfig cacheConfig = J9CUDA_CACHE_CONFIG_PREFER_EQUAL;

			rc = j9cuda_deviceGetCacheConfig(deviceId, &cacheConfig);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not query cache config\n");
			} else {
				outputComment(PORTLIB, "  Cache config: %d\n", cacheConfig);
			}

			for (index = 0; index < LENGTH_OF(cacheConfigs); ++index) {
				rc = j9cuda_deviceSetCacheConfig(deviceId, cacheConfigs[index]);

				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set device cache config(%d): %d\n",
							cacheConfigs[index], rc);
				}
			}
		}

		{ // shared memory config
			J9CudaSharedMemConfig sharedMemConfig = J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;

			rc = j9cuda_deviceGetSharedMemConfig(deviceId, &sharedMemConfig);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not query shared memory config\n");
			} else {
				outputComment(PORTLIB, "  Shared memory config: %d\n", sharedMemConfig);
			}

			for (index = 0; index < LENGTH_OF(sharedMemConfigs); ++index) {
				rc = j9cuda_deviceSetSharedMemConfig(deviceId, sharedMemConfigs[index]);

				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to set device shared memory config(%d): %d\n",
							sharedMemConfigs[index], rc);
				}
			}
		}

		{ // stream priority range
			int32_t leastPriority = 0;
			int32_t greatestPriority = 0;

			rc = j9cuda_deviceGetStreamPriorityRange(deviceId, &leastPriority, &greatestPriority);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not query stream priority range\n");
			} else {
				outputComment(PORTLIB, "  Stream priority range: [%d,%d]\n", greatestPriority, leastPriority);
			}
		}

		{ // limits
			const Limit * entry = deviceLimits;
			int32_t computeCapability = 0;

			j9cuda_deviceGetAttribute(deviceId, J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY, &computeCapability);

			outputComment(PORTLIB, "  Limits:\n", deviceId);

			for (; NULL != entry->name; ++entry) {
				uintptr_t value = 0;
				int32_t rc = j9cuda_deviceGetLimit(deviceId, entry->limit, &value);

				if (computeCapability < entry->minComputeCapability) {
					if (0 == rc) {
						outputErrorMessage(PORTTEST_ERROR_ARGS,
								"Limit %s should not be supported on device with compute capability %d.%d\n",
								entry->name, computeCapability / 10, computeCapability % 10 );
					}
				} else {
					if (0 != rc) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not query limit %s\n", entry->name);
					} else if (entry->minValue <= value && value <= entry->maxValue) {
						outputComment(PORTLIB, "    %s: %d\n", entry->name, value);

						/* compute the median of the legal range */
						value = entry->minValue + ((entry->maxValue - entry->minValue) / 2);

						rc = j9cuda_deviceSetLimit(deviceId, entry->limit, value);

						if (0 != rc) {
							outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not set limit %s to %zu: %d\n",
							        entry->name, value, rc);
						}
					} else {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "%s: %d not in expected range [%d,%d]\n",
								entry->name, value, entry->minValue, entry->maxValue);
					}
				}
			}
		}

		{ // attributes
			const Attribute * entry = deviceAttributes;

			outputComment(PORTLIB, "  Attributes:\n", deviceId);

			for (; NULL != entry->name; ++entry) {
				int32_t value = 0;

				rc = j9cuda_deviceGetAttribute(deviceId, entry->attribute, &value);

				if (0 != rc) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not query attribute %s\n", entry->name);
				} else if (entry->minValue <= value && value <= entry->maxValue) {
					outputComment(PORTLIB, "    %s: %d\n", entry->name, value);
				} else {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "%s: %d not in expected range [%d,%d]\n",
							entry->name, value, entry->minValue, entry->maxValue);
				}
			}
		}
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify device global memory API.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testGlobalMemory(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	uintptr_t const BufferBytes = 2 * 1024 * 1024;
	const char * const testName = "global memory";
	uint32_t deviceId = 0;
	int32_t rc = 0;
	void * hostBuf1 = NULL;
	void * hostBuf2 = NULL;

	reportTestEntry(portLibrary, testName);

	rc = j9cuda_hostAlloc(BufferBytes, J9CUDA_HOST_ALLOC_DEFAULT, &hostBuf1);

	if ((0 != rc) || (NULL == hostBuf1)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not allocate host memory\n");
		goto hostCleanup;
	}

	rc = j9cuda_hostAlloc(BufferBytes, J9CUDA_HOST_ALLOC_DEFAULT, &hostBuf2);

	if ((0 != rc) || (NULL == hostBuf2)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Could not allocate host memory\n");
		goto hostCleanup;
	}

	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		void * deviceBuf1 = NULL;
		void * deviceBuf2 = NULL;
		uint32_t fillValue = 0;

		rc = j9cuda_deviceAlloc(deviceId, BufferBytes, &deviceBuf1);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate device memory: %d\n", rc);
			goto deviceCleanup;
		} else if (NULL == deviceBuf1) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "allocated null device address\n");
			goto deviceCleanup;
		}

		rc = j9cuda_deviceAlloc(deviceId, BufferBytes, &deviceBuf2);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate device memory: %d\n", rc);
			goto deviceCleanup;
		} else if (NULL == deviceBuf2) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "allocated null device address\n");
			goto deviceCleanup;
		}

		fillPattern(hostBuf1, BufferBytes, deviceId);

		rc = j9cuda_memcpyHostToDevice(deviceId, deviceBuf1, hostBuf1, BufferBytes);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Host to device copy failed: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_memcpyDeviceToDevice(deviceId, deviceBuf2, deviceBuf1, BufferBytes);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Intra-device copy failed: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_memcpyDeviceToHost(deviceId, hostBuf2, deviceBuf2, BufferBytes);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Device to host copy failed: %d\n", rc);
			goto deviceCleanup;
		}

		if (!verifyPattern(hostBuf2, BufferBytes, deviceId)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Data transferred does not match expected pattern\n", rc);
		}

		fillValue = 0x23232323;
		rc = j9cuda_memset8(deviceId, deviceBuf1, 0x23, BufferBytes);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Device memset8 failed: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_memcpyDeviceToHost(deviceId, hostBuf1, deviceBuf1, BufferBytes);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Device to host copy failed: %d\n", rc);
			goto deviceCleanup;
		}

		if (!verifyFill(hostBuf1, BufferBytes, &fillValue, 1)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Data transferred does not match expected byte pattern\n", rc);
		}

		fillValue = 0x45674567;
		rc = j9cuda_memset16(deviceId, deviceBuf1, 0x4567, BufferBytes / 2);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Device memset16 failed: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_memcpyDeviceToHost(deviceId, hostBuf1, deviceBuf1, BufferBytes);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Device to host copy failed: %d\n", rc);
			goto deviceCleanup;
		}

		if (!verifyFill(hostBuf1, BufferBytes, &fillValue, 2)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Data transferred does not match expected byte pattern\n", rc);
		}

		fillValue = 0xabcddcba;
		rc = j9cuda_memset32(deviceId, deviceBuf1, fillValue, BufferBytes / 4);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Device memset32 failed: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_memcpyDeviceToHost(deviceId, hostBuf1, deviceBuf1, BufferBytes);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Device to host copy failed: %d\n", rc);
			goto deviceCleanup;
		}

		if (!verifyFill(hostBuf1, BufferBytes, &fillValue, 4)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Data transferred does not match expected byte pattern\n", rc);
		}

deviceCleanup:
		if (NULL != deviceBuf1) {
			rc = j9cuda_deviceFree(deviceId, deviceBuf1);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to free device memory: %d\n", rc);
			}
		}

		if (NULL != deviceBuf2) {
			rc = j9cuda_deviceFree(deviceId, deviceBuf2);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to free device memory: %d\n", rc);
			}
		}
	}

hostCleanup:
	if (NULL != hostBuf1) {
		rc = j9cuda_hostFree(hostBuf1);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to free host memory: %d\n", rc);
		}
	}

	if (NULL != hostBuf2) {
		rc = j9cuda_hostFree(hostBuf2);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to free host memory: %d\n", rc);
		}
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify proper behavior using invalid device identifiers.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testInvalidDevices(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "invalid device identifiers";
	int32_t const expectedError = (0 == deviceCount) ? J9CUDA_ERROR_NO_DEVICE : J9CUDA_ERROR_INVALID_DEVICE;
	uint32_t deviceId = deviceCount;
	uint32_t deviceIdBit = 1;
	J9CudaDeviceAttribute deviceAttribute = J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY;
	J9CudaCacheConfig cacheConfig = J9CUDA_CACHE_CONFIG_PREFER_EQUAL;
	J9CudaStreamCallback callback = NULL;
	J9CudaEvent event = NULL;
	J9CudaFunction function = NULL;
	J9CudaFunctionAttribute functionAttribute = J9CUDA_FUNCTION_ATTRIBUTE_BINARY_VERSION;
	J9CudaDeviceLimit limit = J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT;
	J9CudaLinker linker = NULL;
	J9CudaModule module = NULL;
	J9CudaSharedMemConfig sharedMemConfig = J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;
	J9CudaStream stream = NULL;
	J9CudaJitInputType type = J9CUDA_JIT_INPUT_TYPE_CUBIN;
	BOOLEAN boolVal = FALSE;
	int32_t i32Val = 0;
	uint32_t u32Val = 0;
	uintptr_t ptrVal = 0;
	void * address = NULL;
	char name[40];

	reportTestEntry(portLibrary, testName);

	for (;;) {
		int32_t rc = 0;

#define DO_BAD_CALL(func, args)                                              \
		do {                                                                 \
			rc = j9cuda_ ## func args;                                       \
			if (expectedError != rc) {                                       \
				outputErrorMessage(PORTTEST_ERROR_ARGS,                      \
						"Unexpected return value from %s: %d\n", #func, rc); \
			}                                                                \
		} while (0)

		DO_BAD_CALL(deviceAlloc,                  (deviceId, ptrVal, &address));
		DO_BAD_CALL(deviceCanAccessPeer,          (deviceId, u32Val, &boolVal));
		DO_BAD_CALL(deviceDisablePeerAccess,      (deviceId, u32Val));
		DO_BAD_CALL(deviceEnablePeerAccess,       (deviceId, u32Val));
		DO_BAD_CALL(deviceFree,                   (deviceId, address));
		DO_BAD_CALL(deviceGetAttribute,           (deviceId, deviceAttribute, &i32Val));
		DO_BAD_CALL(deviceGetCacheConfig,         (deviceId, &cacheConfig));
		DO_BAD_CALL(deviceGetLimit,               (deviceId, limit, &ptrVal));
		DO_BAD_CALL(deviceGetMemInfo,             (deviceId, &ptrVal, &ptrVal));
		DO_BAD_CALL(deviceGetName,                (deviceId, sizeof(name), name));
		DO_BAD_CALL(deviceGetSharedMemConfig,     (deviceId, &sharedMemConfig));
		DO_BAD_CALL(deviceGetStreamPriorityRange, (deviceId, &i32Val, &i32Val));
		DO_BAD_CALL(deviceReset,                  (deviceId));
		DO_BAD_CALL(deviceSetCacheConfig,         (deviceId, cacheConfig));
		DO_BAD_CALL(deviceSetLimit,               (deviceId, limit, i32Val));
		DO_BAD_CALL(deviceSetSharedMemConfig,     (deviceId, sharedMemConfig));
		DO_BAD_CALL(deviceSynchronize,            (deviceId));
		DO_BAD_CALL(eventCreate,                  (deviceId, i32Val, &event));
		DO_BAD_CALL(eventDestroy,                 (deviceId, event));
		DO_BAD_CALL(eventRecord,                  (deviceId, event, stream));
		DO_BAD_CALL(funcGetAttribute,             (deviceId, function, functionAttribute, &i32Val));
		DO_BAD_CALL(funcSetCacheConfig,           (deviceId, function, cacheConfig));
		DO_BAD_CALL(funcSetSharedMemConfig,       (deviceId, function, sharedMemConfig));
		DO_BAD_CALL(launchKernel,                 (deviceId, function, 1, 1, 1, 1, 1, 1, 0, NULL, NULL));
		DO_BAD_CALL(linkerAddData,                (deviceId, linker, type, address, ptrVal, name, NULL));
		DO_BAD_CALL(linkerComplete,               (deviceId, linker, &address, &ptrVal));
		DO_BAD_CALL(linkerCreate,                 (deviceId, NULL, &linker));
		DO_BAD_CALL(linkerDestroy,                (deviceId, linker));
		DO_BAD_CALL(memcpyDeviceToDevice,         (deviceId, NULL, NULL, ptrVal));
		DO_BAD_CALL(memcpyDeviceToHost,           (deviceId, NULL, NULL, ptrVal));
		DO_BAD_CALL(memcpyHostToDevice,           (deviceId, NULL, NULL, ptrVal));
		DO_BAD_CALL(memcpyPeer,                   (deviceId, NULL, u32Val, NULL, ptrVal));
		DO_BAD_CALL(memset8,                      (deviceId, address, i32Val, ptrVal));
		DO_BAD_CALL(memset16,                     (deviceId, address, i32Val, ptrVal));
		DO_BAD_CALL(memset32,                     (deviceId, address, i32Val, ptrVal));
		DO_BAD_CALL(moduleGetFunction,            (deviceId, module, name, &function));
		DO_BAD_CALL(moduleGetGlobal,              (deviceId, module, name, &ptrVal, &ptrVal));
		DO_BAD_CALL(moduleGetSurfaceRef,          (deviceId, module, name, &ptrVal));
		DO_BAD_CALL(moduleGetTextureRef,          (deviceId, module, name, &ptrVal));
		DO_BAD_CALL(moduleLoad,                   (deviceId, address, NULL, &module));
		DO_BAD_CALL(moduleUnload,                 (deviceId, module));
		DO_BAD_CALL(streamAddCallback,            (deviceId, stream, callback, 0));
		DO_BAD_CALL(streamCreate,                 (deviceId, &stream));
		DO_BAD_CALL(streamCreateWithPriority,     (deviceId, i32Val, i32Val, &stream));
		DO_BAD_CALL(streamDestroy,                (deviceId, stream));
		DO_BAD_CALL(streamGetFlags,               (deviceId, stream, &u32Val));
		DO_BAD_CALL(streamGetPriority,            (deviceId, stream, &i32Val));
		DO_BAD_CALL(streamQuery,                  (deviceId, stream));
		DO_BAD_CALL(streamSynchronize,            (deviceId, stream));
		DO_BAD_CALL(streamWaitEvent,              (deviceId, stream, event));

#undef DO_BAD_CALL

		while (0 != (deviceId & deviceIdBit)) {
			deviceIdBit <<= 1;
		}

		if (0 == deviceIdBit) {
			break;
		}

		deviceId |= deviceIdBit;
		deviceIdBit <<= 1;
	}

	return reportTestExit(portLibrary, testName);
}

static void
streamCallback(
		J9CudaStream stream,
		int32_t      error,
		uintptr_t    userData)
{
	*(J9CudaStream *)userData = stream;
}

/**
 * Verify stream API.
 *
 * @param[in] portLibrary  the port library under test
 * @param[in] deviceCount  the number of devices to test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
static int32_t
testStreams(struct J9PortLibrary * portLibrary, uint32_t deviceCount)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	const char * const testName = "streams";
	uint32_t deviceId = 0;

	reportTestEntry(portLibrary, testName);

	for (deviceId = 0; deviceId < deviceCount; ++deviceId) {
		J9CudaStream callbackData = NULL;
		J9CudaEvent event = NULL;
		int32_t rc = 0;
		J9CudaStream stream1 = NULL;
		J9CudaStream stream2 = NULL;
		uint32_t streamFlags = 0;
		int32_t streamPriority = 0;

		rc = j9cuda_streamCreate(deviceId, &stream1);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create stream: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_streamCreateWithPriority(deviceId, -1, J9CUDA_STREAM_FLAG_NON_BLOCKING, &stream2);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create stream: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_eventCreate(deviceId, J9CUDA_EVENT_FLAG_DEFAULT, &event);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create event: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_eventRecord(deviceId, event, stream1);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to record event: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_streamWaitEvent(deviceId, stream2, event);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to make stream wait for event: %d\n", rc);
			goto deviceCleanup;
		}

		rc = j9cuda_streamAddCallback(deviceId, stream2, streamCallback, (uintptr_t)&callbackData);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to add stream callback: %d\n", rc);
		}

		rc = j9cuda_streamGetFlags(deviceId, stream2, &streamFlags);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to get stream flags: %d\n", rc);
		}

		rc = j9cuda_streamGetPriority(deviceId, stream2, &streamPriority);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to get stream priority: %d\n", rc);
		}

		rc = j9cuda_streamSynchronize(deviceId, stream2);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to synchronize with stream: %d\n", rc);
		}

		rc = j9cuda_streamQuery(deviceId, stream2);

		if (0 != rc) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Stream should be ready: %d\n", rc);
		}

		if (stream2 != callbackData) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Callback did not execute correctly\n");
		}

deviceCleanup:
		if (NULL != stream1) {
			rc = j9cuda_streamDestroy(deviceId, stream1);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to destroy stream: %d\n", rc);
			}
		}

		if (NULL != stream2) {
			rc = j9cuda_streamDestroy(deviceId, stream2);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to destroy stream: %d\n", rc);
			}
		}

		if (NULL != event) {
			rc = j9cuda_eventDestroy(deviceId, event);

			if (0 != rc) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to destroy event: %d\n", rc);
			}
		}
	}

	return reportTestExit(portLibrary, testName);
}

#endif /* OMR_OPT_CUDA */

int32_t
j9cuda_runTests(struct J9PortLibrary * portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);

	uint32_t deviceCount = 0;
	int32_t rc = TEST_PASS;

	/* display unit under test */
	HEADING(portLibrary, "j9cuda tests");

#if defined(OMR_OPT_CUDA)
	rc = testBasic(portLibrary, &deviceCount);

	if (TEST_PASS == rc) {
		rc |= testErrors(portLibrary, deviceCount);
		rc |= testInvalidDevices(portLibrary, deviceCount);

		if (0 < deviceCount) {
			rc |= testEvents(portLibrary, deviceCount);
			rc |= testGlobalMemory(portLibrary, deviceCount);
			rc |= testLaunch(portLibrary, deviceCount);
			rc |= testLinker(portLibrary, deviceCount);
			rc |= testModules(portLibrary, deviceCount);
			rc |= testProperties(portLibrary, deviceCount);
			rc |= testStreams(portLibrary, deviceCount);
		}

		if (1 < deviceCount) {
			rc |= testPeerAccess(portLibrary, deviceCount);
		}
	}
#endif /* OMR_OPT_CUDA */

	/* output results */
	j9tty_printf(PORTLIB, "\nj9cuda tests done%s.\n\n", (rc == TEST_PASS) ? "" : ", failures detected");

	return rc;
}
