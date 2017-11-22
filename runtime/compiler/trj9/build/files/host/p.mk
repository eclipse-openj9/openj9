# Copyright (c) 2000, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

JIT_PRODUCT_BACKEND_SOURCES+= \
    omr/compiler/p/runtime/VirtualGuardRuntime.spp

JIT_PRODUCT_SOURCE_FILES+=\
    compiler/trj9/p/runtime/J9PPCArrayCopy.spp \
    compiler/trj9/p/runtime/J9PPCArrayCmp.spp \
    compiler/trj9/p/runtime/J9PPCArrayTranslate.spp \
    compiler/trj9/p/runtime/J9PPCEncodeUTF16.spp \
    compiler/trj9/p/runtime/J9PPCCompressString.spp \
    compiler/trj9/p/runtime/J9PPCCRC32.spp \
    compiler/trj9/p/runtime/J9PPCCRC32_wrapper.c \
    compiler/trj9/p/runtime/CodeSync.cpp \
    compiler/trj9/p/runtime/Math.spp \
    compiler/trj9/p/runtime/PicBuilder.spp \
    compiler/trj9/p/runtime/PPCRelocationTarget.cpp \
    compiler/trj9/p/runtime/Recomp.cpp \
    compiler/trj9/p/runtime/Recompilation.spp \
    compiler/trj9/p/runtime/PPCHWProfiler.cpp \
    compiler/trj9/p/runtime/PPCLMGuardedStorage.cpp \
    compiler/trj9/p/runtime/ebb.spp \
    compiler/trj9/p/runtime/Emulation.c \
    omr/compiler/p/runtime/OMRCodeCacheConfig.cpp

ifeq ($(OS),aix)
    PPC_HW_PROFILER=compiler/trj9/p/runtime/PPCHWProfilerAIX.cpp
    PPC_LM_GUARDED_STORAGE=compiler/trj9/p/runtime/PPCLMGuardedStorageAIX.cpp

    ifeq ($(I5_VERSION),I5_V6R1)
        PPC_HW_PROFILER=
        PPC_LM_GUARDED_STORAGE=
    endif
    ifeq ($(I5_VERSION),I5_V7R2)
        PPC_HW_PROFILER=
        PPC_LM_GUARDED_STORAGE=
    endif
endif

ifeq ($(OS),linux)
    PPC_HW_PROFILER=compiler/trj9/p/runtime/PPCHWProfilerLinux.cpp
    PPC_LM_GUARDED_STORAGE=compiler/trj9/p/runtime/PPCLMGuardedStorageLinux.cpp
endif

JIT_PRODUCT_SOURCE_FILES+=$(PPC_HW_PROFILER)
JIT_PRODUCT_SOURCE_FILES+=$(PPC_LM_GUARDED_STORAGE)
