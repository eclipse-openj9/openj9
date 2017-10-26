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

# Notes (on disabled flags):
# OMR_THR_SPIN_WAKE_CONTROL flag is disabled on zOS due to poor performance.

CONFIGURE_ARGS += \
	--enable-OMR_THR_THREE_TIER_LOCKING

ifeq (zos_390-64_cmprssptrs, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_ZOS \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_GC_CONCURRENT_SCAVENGER \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (zos_390-64_cmprssptrs_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_ZOS \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (zos_390-64, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_ZOS \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_CONCURRENT_SCAVENGER \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (zos_390-64_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_ZOS \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (zos_390, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_ZOS \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_PORT_ZOS_CEEHDLRSUPPORT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (zos_390_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_ZOS \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_PORT_ZOS_CEEHDLRSUPPORT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

CONFIGURE_ARGS += 'AS=c89'
CONFIGURE_ARGS += 'CC=c89'
CONFIGURE_ARGS += 'CXX=cxx'
CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=cc'
CONFIGURE_ARGS += 'CXXLINKEXE=cxx' # plus additional flags set by makefile
CONFIGURE_ARGS += 'CXXLINKSHARED=cxx' # plus additional flags set by makefile
CONFIGURE_ARGS += 'AR=ar'

CONFIGURE_ARGS += 'OMR_HOST_OS=zos'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=s390'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=xlc'

CONFIGURE_ARGS += 'GLOBAL_CXXFLAGS=-W "c,SERVICE(j${uma.buildinfo.build_date})"'
CONFIGURE_ARGS += 'GLOBAL_CFLAGS=-W "c,SERVICE(j${uma.buildinfo.build_date})"'
