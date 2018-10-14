# Copyright (c) 2000, 2018 IBM Corp. and others
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
    omr/compiler/x/runtime/VirtualGuardRuntime.cpp

ifeq ($(OS),osx)
    JIT_PRODUCT_SOURCE_FILES+=\
    compiler/x/runtime/Recomp.cpp \
    compiler/x/runtime/X86ArrayTranslate.nasm \
    compiler/x/runtime/X86Codert.nasm \
    compiler/x/runtime/X86EncodeUTF16.nasm \
    compiler/x/runtime/X86LockReservation.nasm \
    compiler/x/runtime/X86PicBuilder.nasm \
    compiler/x/runtime/X86PicBuilderC.cpp \
    compiler/x/runtime/X86RelocationTarget.cpp \
    compiler/x/runtime/X86Unresolveds.nasm

else

JIT_PRODUCT_SOURCE_FILES+=\
    compiler/x/runtime/Recomp.cpp \
    compiler/x/runtime/X86ArrayTranslate.asm \
    compiler/x/runtime/X86Codert.asm \
    compiler/x/runtime/X86EncodeUTF16.asm \
    compiler/x/runtime/X86LockReservation.asm \
    compiler/x/runtime/X86PicBuilder.pasm \
    compiler/x/runtime/X86PicBuilderC.cpp \
    compiler/x/runtime/X86RelocationTarget.cpp \
    compiler/x/runtime/X86Unresolveds.pasm

endif # OS == osx

include $(JIT_MAKE_DIR)/files/host/$(HOST_SUBARCH).mk
