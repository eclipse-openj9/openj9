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
    omr/compiler/arm/runtime/FlushICache.armasm \
    omr/compiler/arm/runtime/CodeSync.cpp \
    omr/compiler/arm/runtime/VirtualGuardRuntime.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    compiler/trj9/arm/runtime/MathHelp.c \
    compiler/trj9/arm/runtime/PicBuilder.armasm \
    compiler/trj9/arm/runtime/ThunkHelpersARM.c \
    compiler/trj9/arm/runtime/Math.armasm \
    compiler/trj9/arm/runtime/ArrayCopy.armasm \
    compiler/trj9/arm/runtime/Recomp.cpp \
    compiler/trj9/arm/runtime/Recompilation.armasm \
    compiler/trj9/arm/runtime/MTHelpersARM.cpp \
    compiler/trj9/arm/runtime/ARMRelocationTarget.cpp
