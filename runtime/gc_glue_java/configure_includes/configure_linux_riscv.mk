###############################################################################
# Copyright (c) 2019, 2020 IBM Corp. and others
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
###############################################################################

include $(CONFIG_INCL_DIR)/configure_common.mk

# The riscv64 cross toolchain is used for both 32bit and 64bit
RISCV_CROSSTOOLS_PREFIX ?= riscv64-unknown-linux-gnu

CONFIGURE_ARGS += \
    --enable-debug \
    --enable-OMR_THR_THREE_TIER_LOCKING \
    --enable-OMR_THR_YIELD_ALG \
    --enable-OMR_THR_SPIN_WAKE_CONTROL \
    --enable-OMRTHREAD_LIB_UNIX \
    --enable-OMR_ARCH_RISCV \
    --enable-OMR_ENV_LITTLE_ENDIAN \
    --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
    --enable-OMR_GC_IDLE_HEAP_MANAGER \
    --enable-OMR_GC_TLH_PREFETCH_FTA \
    --enable-OMR_GC_CONCURRENT_SCAVENGER \
    --enable-OMR_GC_ARRAYLETS

ifneq (,$(findstring _riscv64, $(SPEC)))
    CONFIGURE_ARGS += \
        --host=$(RISCV_CROSSTOOLS_PREFIX) \
        --enable-OMR_ENV_DATA64 \
        'OMR_TARGET_DATASIZE=64'
endif

ifneq (,$(findstring _cmprssptrs, $(SPEC)))
	CONFIGURE_ARGS += \
		OMR_GC_POINTER_MODE=compressed
else
	CONFIGURE_ARGS += \
		OMR_GC_POINTER_MODE=full
endif

ifneq (,$(findstring _cross, $(SPEC)))
    CONFIGURE_ARGS += \
        --build=x86_64-pc-linux-gnu \
        'OMR_CROSS_CONFIGURE=yes'

    AR=$(RISCV_CROSSTOOLS_PREFIX)-ar
    AS=$(RISCV_CROSSTOOLS_PREFIX)-as
    CC=$(RISCV_CROSSTOOLS_PREFIX)-gcc $(SYSROOT_CFLAGS)
    CXX=$(RISCV_CROSSTOOLS_PREFIX)-g++ $(SYSROOT_CFLAGS)
    OBJCOPY=$(RISCV_CROSSTOOLS_PREFIX)-objcopy
else
    CONFIGURE_ARGS += \
        --build=$(RISCV_CROSSTOOLS_PREFIX)

    ifeq (default,$(origin AR))
        AR=ar
    endif
    ifeq (default,$(origin AS))
        AS=as
    endif
    ifeq (default,$(origin CC))
        CC=gcc
    endif
    ifeq (default,$(origin CXX))
        CXX=g++
    endif
    OBJCOPY=objcopy
endif

CONFIGURE_ARGS += 'AR=$(AR)'
CONFIGURE_ARGS += 'AS=$(AS)'
CONFIGURE_ARGS += 'CC=$(CC)'
CONFIGURE_ARGS += 'CXX=$(CXX)'
CONFIGURE_ARGS += 'OBJCOPY=$(OBJCOPY)'

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CXX)'
CONFIGURE_ARGS += 'CXXLINKSHARED=$$(CXX)'

CONFIGURE_ARGS += 'OMR_HOST_OS=linux'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=riscv'

CONFIGURE_ARGS += 'OMR_TOOLCHAIN=gcc'
CONFIGURE_ARGS += 'OMR_BUILD_TOOLCHAIN=gcc'
CONFIGURE_ARGS += 'OMR_TOOLS_CC=gcc'
CONFIGURE_ARGS += 'OMR_TOOLS_CXX=g++'
CONFIGURE_ARGS += 'OMR_BUILD_DATASIZE=64'
