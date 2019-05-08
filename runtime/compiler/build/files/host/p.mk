# Copyright (c) 2000, 2019 IBM Corp. and others
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

JIT_PRODUCT_BACKEND_SOURCES+= \
    omr/compiler/p/runtime/VirtualGuardRuntime.spp

JIT_PRODUCT_SOURCE_FILES+=\
    compiler/p/runtime/CodeSync.cpp \
    compiler/p/runtime/ebb.spp \
    compiler/p/runtime/Emulation.c \
    compiler/p/runtime/J9PPCArrayCmp.spp \
    compiler/p/runtime/J9PPCArrayCopy.spp \
    compiler/p/runtime/J9PPCArrayTranslate.spp \
    compiler/p/runtime/J9PPCCRC32.spp \
    compiler/p/runtime/J9PPCCRC32_wrapper.c \
    compiler/p/runtime/J9PPCCompressString.spp \
    compiler/p/runtime/J9PPCEncodeUTF16.spp \
    compiler/p/runtime/Math.spp \
    compiler/p/runtime/PPCHWProfiler.cpp \
    compiler/p/runtime/PPCLMGuardedStorage.cpp \
    compiler/p/runtime/PPCRelocationTarget.cpp \
    compiler/p/runtime/PicBuilder.spp \
    compiler/p/runtime/Recomp.cpp \
    compiler/p/runtime/Recompilation.spp \
    omr/compiler/p/runtime/OMRCodeCacheConfig.cpp

ifeq ($(OS),aix)
    PPC_HW_PROFILER=compiler/p/runtime/PPCHWProfilerAIX.cpp
    PPC_LM_GUARDED_STORAGE=compiler/p/runtime/PPCLMGuardedStorageAIX.cpp

    ifneq ($(findstring $(I5_VERSION),I5_V6R1 I5_V7R2 I5_V7R3 I5_V7R4),)
        PPC_HW_PROFILER=
        PPC_LM_GUARDED_STORAGE=
    endif
endif

ifeq ($(OS),linux)
    PPC_HW_PROFILER=compiler/p/runtime/PPCHWProfilerLinux.cpp
    PPC_LM_GUARDED_STORAGE=compiler/p/runtime/PPCLMGuardedStorageLinux.cpp
endif

JIT_PRODUCT_SOURCE_FILES+=$(PPC_HW_PROFILER)
JIT_PRODUCT_SOURCE_FILES+=$(PPC_LM_GUARDED_STORAGE)
