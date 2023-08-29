###############################################################################
# Copyright IBM Corp. and others 2016
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
		--enable-OMR_GC_SPARSE_HEAP_ALLOCATION \
		--enable-OMR_GC_CONCURRENT_SCAVENGER \
		--enable-OMR_GC_IDLE_HEAP_MANAGER \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		OMR_GC_POINTER_MODE=compressed
ifeq (8,$(VERSION_MAJOR))
	CONFIGURE_ARGS += \
		OMR_ZOS_COMPILE_ARCHITECTURE=8 \
		OMR_ZOS_COMPILE_TUNE=10 \
		OMR_ZOS_COMPILE_TARGET=zOSV1R13 \
		OMR_ZOS_LINK_COMPAT=ZOSV1R13
else
	CONFIGURE_ARGS += \
		OMR_ZOS_COMPILE_ARCHITECTURE=10 \
		OMR_ZOS_COMPILE_TUNE=12 \
		OMR_ZOS_COMPILE_TARGET=zOSV2R3 \
		OMR_ZOS_LINK_COMPAT=ZOSV2R3
endif
endif

ifeq (zos_390-64, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_ZOS \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_SPARSE_HEAP_ALLOCATION \
		--enable-OMR_GC_CONCURRENT_SCAVENGER \
		--enable-OMR_GC_IDLE_HEAP_MANAGER \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		OMR_GC_POINTER_MODE=full
ifeq (8,$(VERSION_MAJOR))
	CONFIGURE_ARGS += \
		OMR_ZOS_COMPILE_ARCHITECTURE=8 \
		OMR_ZOS_COMPILE_TUNE=10 \
		OMR_ZOS_COMPILE_TARGET=zOSV1R13 \
		OMR_ZOS_LINK_COMPAT=ZOSV1R13
else
	CONFIGURE_ARGS += \
		OMR_ZOS_COMPILE_ARCHITECTURE=10 \
		OMR_ZOS_COMPILE_TUNE=12 \
		OMR_ZOS_COMPILE_TARGET=zOSV2R3 \
		OMR_ZOS_LINK_COMPAT=ZOSV2R3
endif
endif

ifeq (zos_390, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_ZOS \
		--enable-OMR_ARCH_S390 \
		--enable-OMR_PORT_ZOS_CEEHDLRSUPPORT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		OMR_GC_POINTER_MODE=full
ifeq (8,$(VERSION_MAJOR))
	CONFIGURE_ARGS += \
		OMR_ZOS_COMPILE_ARCHITECTURE=8 \
		OMR_ZOS_COMPILE_TUNE=10 \
		OMR_ZOS_COMPILE_TARGET=zOSV1R13 \
		OMR_ZOS_LINK_COMPAT=ZOSV1R13
endif
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

ifeq (default,$(origin AS))
	AS=c89
endif
ifeq (default,$(origin CC))
	CC=c89
endif
ifeq (default,$(origin CXX))
	CXX=cxx
endif

CONFIGURE_ARGS += 'AS=$(AS)'
CONFIGURE_ARGS += 'CC=$(CC)'
CONFIGURE_ARGS += 'CXX=$(CXX)'
CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=cc'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CXX)' # plus additional flags set by makefile
CONFIGURE_ARGS += 'CXXLINKSHARED=$$(CXX)' # plus additional flags set by makefile
CONFIGURE_ARGS += 'AR=$(AR)'

CONFIGURE_ARGS += 'OMR_HOST_OS=zos'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=s390'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=xlc'

CONFIGURE_ARGS += 'GLOBAL_CXXFLAGS=-Wc,"SERVICE(j${uma.buildinfo.build_date})"'
CONFIGURE_ARGS += 'GLOBAL_CFLAGS=-Wc,"SERVICE(j${uma.buildinfo.build_date})"'

# Some code (e.g. ddrgen test samples) uses native (EBCDIC) encoding.
CONFIGURE_ARGS += --enable-native-encoding
