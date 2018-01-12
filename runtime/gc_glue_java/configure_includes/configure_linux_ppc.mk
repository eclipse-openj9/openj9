###############################################################################
# Copyright (c) 2016, 2017 IBM Corp. and others
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

GCC_TOOLS:=\
'OMR_TOOLCHAIN=gcc' \
'AS=as' \
'CC=gcc' \
'CXX=c++' \
'CCLINKEXE=$$(CC)' \
'CCLINKSHARED=$$(CC)' \
'CXXLINKEXE=$$(CXX)' \
'CXXLINKSHARED=$$(CXX)'
# CPP is unused: 'CPP=cpp' 

XLC_TOOLS:=\
'OMR_TOOLCHAIN=xlc' \
'AS=xlC_r' \
'CC=xlC_r' \
'CXX=$$(CC)' \
'CCLINKEXE=xlc_r' \
'CCLINKSHARED=xlc_r' \
'CXXLINKEXE=$$(CC)' \
'CXXLINKSHARED=xlc_r'
# CPP is unused: 'CPP=cpp' 

# All specs except buildspecs named "_gcc" currently use XLC 
# buildspecs named "_gcc" use gcc
ifneq (,$(findstring _gcc,$(SPEC)))
	TEMP_TOOLS=$(GCC_TOOLS)
else
	TEMP_TOOLS=$(XLC_TOOLS)
endif

CONFIGURE_ARGS += \
	--enable-OMR_THR_THREE_TIER_LOCKING \
	--enable-OMR_THR_YIELD_ALG \
	--enable-OMR_THR_SPIN_WAKE_CONTROL

ifeq (linux_ppc-64_cmprssptrs_le, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc-64_cmprssptrs_le_gcc, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_ENV_GCC \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc-64_cmprssptrs_le_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc-64_cmprssptrs, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc-64_cmprssptrs_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc-64_le, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc-64_le_gcc, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_ENV_GCC \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc-64_le_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc-64_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc-64, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

ifeq (linux_ppc_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		--enable-OMR_PORT_NUMA_SUPPORT
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

ifndef TEMP_TOOLS
  $(error No toolchain selected)
endif

CONFIGURE_ARGS += $(TEMP_TOOLS)
CONFIGURE_ARGS += 'AR=ar'

CONFIGURE_ARGS += 'OMR_HOST_OS=linux'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=ppc'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
# OMR_TOOLCHAIN is set as part of TEMP_TOOLS above.
