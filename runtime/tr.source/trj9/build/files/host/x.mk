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
    omr/compiler/x/runtime/VirtualGuardRuntime.cpp

JIT_PRODUCT_SOURCE_FILES+=\
    tr.source/trj9/x/runtime/X86LockReservation.asm \
    tr.source/trj9/x/runtime/X86ArrayTranslate.asm \
    tr.source/trj9/x/runtime/X86Codert.asm \
    tr.source/trj9/x/runtime/X86PicBuilder.pasm \
    tr.source/trj9/x/runtime/X86PicBuilderC.cpp \
    tr.source/trj9/x/runtime/X86RelocationTarget.cpp \
    tr.source/trj9/x/runtime/X86Unresolveds.pasm \
    tr.source/trj9/x/runtime/X86EncodeUTF16.asm \
    tr.source/trj9/x/runtime/Recomp.cpp

include $(JIT_MAKE_DIR)/files/host/$(HOST_SUBARCH).mk
