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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

include $(CONFIG_INCL_DIR)/configure_common.mk

# Override datasize for 31-bit specs.
ifeq (,$(findstring -64,$(SPEC)))
  TEMP_TARGET_DATASIZE := 31
endif

CONFIGURE_ARGS += \
	--enable-OMR_THR_THREE_TIER_LOCKING \
	--enable-OMR_THR_YIELD_ALG \
	--enable-OMR_THR_SPIN_WAKE_CONTROL

ifeq (linux_ztpf_390-64_cmprssptrs_codecov, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT
#		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
#               --enable-OMR_RTTI
endif

ifeq (linux_ztpf_390-64_cmprssptrs, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_GC_CONCURRENT_SCAVENGER \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT
#		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
#               --enable-OMR_RTTI
endif

ifeq (linux_ztpf_390-64_cmprssptrs_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT
#		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
#                --enable-OMR_RTTI
endif

ifeq (linux_ztpf_390-64_codecov, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64
#		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
#                --enable-OMR_RTTI
endif

ifeq (linux_ztpf_390-64, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_CONCURRENT_SCAVENGER
#		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
#                --enable-OMR_RTTI
endif

ifeq (linux_ztpf_390-64_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64
#		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
#                --enable-OMR_RTTI
endif

ifeq (linux_ztpf_390, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_S390
#		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
#                --enable-OMR_RTTI
endif

ifeq (linux_ztpf_390_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_UNIX \
		--enable-OMR_ARCH_S390
#		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
#                --enable-OMR_RTTI
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

CONFIGURE_ARGS += --host=s390x-ibm-tpf
CONFIGURE_ARGS += --build=s390x-ibm-linux-gnu
CONFIGURE_ARGS += 'AS=as'
CONFIGURE_ARGS += 'CC=tpf-gcc'
CONFIGURE_ARGS += 'CXX=tpf-g++'
CONFIGURE_ARGS += 'CCLINKEXE=tpf-gcc'
CONFIGURE_ARGS += 'CCLINKSHARED=tpf-gcc'
CONFIGURE_ARGS += 'CXXLINKEXE=tpf-gcc'
CONFIGURE_ARGS += 'CXXLINKSHARED=tpf-gcc'
CONFIGURE_ARGS += 'AR=ar'
CONFIGURE_ARGS += 'RM=$(RM)'

CONFIGURE_ARGS += 'CFLAGS=-D_TPF_SOURCE'

CONFIGURE_ARGS += 'OMR_HOST_OS=linux_ztpf'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=s390x'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=gcc'
