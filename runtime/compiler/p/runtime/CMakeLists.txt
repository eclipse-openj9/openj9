################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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

if(OMR_HOST_OS STREQUAL "aix")
	set(PPC_HW_PROFILER PPCHWProfilerAIX.cpp)
	set(PPC_LM_GUARDED_STORAGE PPCLMGuardedStorageAIX.cpp)
#
#	ifeq ($(I5_VERSION),I5_V6R1)
#		PPC_HW_PROFILER=
#		PPC_LM_GUARDED_STORAGE=
#	endif
#	ifeq ($(I5_VERSION),I5_V7R2)
#		PPC_HW_PROFILER=
#		PPC_LM_GUARDED_STORAGE=
#	endif
endif()

if(OMR_HOST_OS STREQUAL "linux")
	set(PPC_HW_PROFILER PPCHWProfilerLinux.cpp)
	set(PPC_LM_GUARDED_STORAGE PPCLMGuardedStorageLinux.cpp)
endif()


j9jit_files(
	p/runtime/${PPC_HW_PROFILER}
	p/runtime/${PPC_LM_GUARDED_STORAGE}
	${omr_SOURCE_DIR}/compiler/p/runtime/VirtualGuardRuntime.spp
	${omr_SOURCE_DIR}/compiler/p/runtime/OMRCodeCacheConfig.cpp
	p/runtime/J9PPCArrayCopy.spp
	p/runtime/J9PPCArrayCmp.spp
	p/runtime/J9PPCArrayTranslate.spp
	p/runtime/J9PPCEncodeUTF16.spp
	p/runtime/J9PPCCompressString.spp
	p/runtime/J9PPCCRC32.spp
	p/runtime/J9PPCCRC32_wrapper.c
	p/runtime/CodeSync.cpp
	p/runtime/Math.spp
	p/runtime/PicBuilder.spp
	p/runtime/PPCRelocationTarget.cpp
	p/runtime/Recomp.cpp
	p/runtime/Recompilation.spp
	p/runtime/PPCHWProfiler.cpp
	p/runtime/PPCLMGuardedStorage.cpp
	p/runtime/ebb.spp
	p/runtime/Emulation.c
)
