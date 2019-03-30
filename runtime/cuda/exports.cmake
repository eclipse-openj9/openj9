################################################################################
# Copyright (c) 2019, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
################################################################################

omr_add_exports(cuda4j
	Java_com_ibm_cuda_Cuda_allocatePinnedBuffer
	Java_com_ibm_cuda_Cuda_getDeviceCount
	Java_com_ibm_cuda_Cuda_getDriverVersion
	Java_com_ibm_cuda_Cuda_getErrorMessage
	Java_com_ibm_cuda_Cuda_getRuntimeVersion
	Java_com_ibm_cuda_Cuda_initialize

	Java_com_ibm_cuda_CudaBuffer_allocate

	Java_com_ibm_cuda_CudaDevice_addCallback
	Java_com_ibm_cuda_CudaDevice_canAccessPeer
	Java_com_ibm_cuda_CudaDevice_disablePeerAccess
	Java_com_ibm_cuda_CudaDevice_enablePeerAccess
	Java_com_ibm_cuda_CudaDevice_getAttribute
	Java_com_ibm_cuda_CudaDevice_getCacheConfig
	Java_com_ibm_cuda_CudaDevice_getFreeMemory
	Java_com_ibm_cuda_CudaDevice_getGreatestStreamPriority
	Java_com_ibm_cuda_CudaDevice_getLeastStreamPriority
	Java_com_ibm_cuda_CudaDevice_getLimit
	Java_com_ibm_cuda_CudaDevice_getName
	Java_com_ibm_cuda_CudaDevice_getSharedMemConfig
	Java_com_ibm_cuda_CudaDevice_getTotalMemory
	Java_com_ibm_cuda_CudaDevice_setCacheConfig
	Java_com_ibm_cuda_CudaDevice_setLimit
	Java_com_ibm_cuda_CudaDevice_setSharedMemConfig
	Java_com_ibm_cuda_CudaDevice_synchronize

	Java_com_ibm_cuda_CudaEvent_create

	Java_com_ibm_cuda_CudaJitOptions_create
	Java_com_ibm_cuda_CudaJitOptions_destroy
	Java_com_ibm_cuda_CudaJitOptions_getErrorLogBuffer
	Java_com_ibm_cuda_CudaJitOptions_getInfoLogBuffer
	Java_com_ibm_cuda_CudaJitOptions_getThreadsPerBlock
	Java_com_ibm_cuda_CudaJitOptions_getWallTime

	Java_com_ibm_cuda_CudaLinker_create

	Java_com_ibm_cuda_CudaModule_load

	Java_com_ibm_cuda_CudaStream_create
	Java_com_ibm_cuda_CudaStream_createWithPriority

	JNI_OnUnload
)

if(J9VM_OPT_CUDA)
	omr_add_exports(cuda4j
		Java_com_ibm_cuda_Cuda_wrapDirectBuffer

		Java_com_ibm_cuda_Cuda_00024Cleaner_releasePinnedBuffer

		Java_com_ibm_cuda_CudaBuffer_allocateDirectBuffer
		Java_com_ibm_cuda_CudaBuffer_copyFromDevice
		Java_com_ibm_cuda_CudaBuffer_copyFromHostByte
		Java_com_ibm_cuda_CudaBuffer_copyFromHostChar
		Java_com_ibm_cuda_CudaBuffer_copyFromHostDirect
		Java_com_ibm_cuda_CudaBuffer_copyFromHostDouble
		Java_com_ibm_cuda_CudaBuffer_copyFromHostFloat
		Java_com_ibm_cuda_CudaBuffer_copyFromHostInt
		Java_com_ibm_cuda_CudaBuffer_copyFromHostLong
		Java_com_ibm_cuda_CudaBuffer_copyFromHostShort
		Java_com_ibm_cuda_CudaBuffer_copyToHostByte
		Java_com_ibm_cuda_CudaBuffer_copyToHostChar
		Java_com_ibm_cuda_CudaBuffer_copyToHostDirect
		Java_com_ibm_cuda_CudaBuffer_copyToHostDouble
		Java_com_ibm_cuda_CudaBuffer_copyToHostFloat
		Java_com_ibm_cuda_CudaBuffer_copyToHostInt
		Java_com_ibm_cuda_CudaBuffer_copyToHostLong
		Java_com_ibm_cuda_CudaBuffer_copyToHostShort
		Java_com_ibm_cuda_CudaBuffer_fill
		Java_com_ibm_cuda_CudaBuffer_freeDirectBuffer
		Java_com_ibm_cuda_CudaBuffer_release

		Java_com_ibm_cuda_CudaEvent_destroy
		Java_com_ibm_cuda_CudaEvent_elapsedTimeSince
		Java_com_ibm_cuda_CudaEvent_query
		Java_com_ibm_cuda_CudaEvent_record
		Java_com_ibm_cuda_CudaEvent_synchronize

		Java_com_ibm_cuda_CudaFunction_getAttribute
		Java_com_ibm_cuda_CudaFunction_launch
		Java_com_ibm_cuda_CudaFunction_setCacheConfig
		Java_com_ibm_cuda_CudaFunction_setSharedMemConfig

		Java_com_ibm_cuda_CudaLinker_add
		Java_com_ibm_cuda_CudaLinker_complete
		Java_com_ibm_cuda_CudaLinker_destroy

		Java_com_ibm_cuda_CudaModule_getFunction
		Java_com_ibm_cuda_CudaModule_getGlobal
		Java_com_ibm_cuda_CudaModule_getSurface
		Java_com_ibm_cuda_CudaModule_getTexture
		Java_com_ibm_cuda_CudaModule_unload

		Java_com_ibm_cuda_CudaStream_destroy
		Java_com_ibm_cuda_CudaStream_getFlags
		Java_com_ibm_cuda_CudaStream_getPriority
		Java_com_ibm_cuda_CudaStream_query
		Java_com_ibm_cuda_CudaStream_synchronize
		Java_com_ibm_cuda_CudaStream_waitFor
	)
endif()
