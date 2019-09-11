###############################################################################
# Copyright (c) 2016, 2019 IBM Corp. and others
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

# Notes (on disabled flags):
# OMR_THR_SPIN_WAKE_CONTROL flag requires OMR_THR_THREE_TIER_LOCKING flag. 
# OMR_THR_SPIN_WAKE_CONTROL flag is disabled on AIX since 
# OMR_THR_THREE_TIER_LOCKING flag is unavailable on AIX.

ifeq (aix_ppc-64_cmprssptrs, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_AIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_GC_CONCURRENT_SCAVENGER \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (aix_ppc-64_cmprssptrs_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_AIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_COMPRESSED_POINTERS \
		--enable-OMR_INTERP_COMPRESSED_OBJECT_HEADER \
		--enable-OMR_INTERP_SMALL_MONITOR_SLOT \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (aix_ppc-64_codecov, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_AIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (aix_ppc-64, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_AIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_GC_CONCURRENT_SCAVENGER \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (aix_ppc-64_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_AIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (aix_ppc_codecov, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_AIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (aix_ppc, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_AIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

ifeq (aix_ppc_purec, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_AIX \
		--enable-OMR_ARCH_POWER \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS
endif

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

ifeq (default,$(origin CC))
	CC = xlC_r
endif
ifeq (default,$(origin CXX))
	CXX = $(CC)
endif

CONFIGURE_ARGS += 'AS=$(AS)'
CONFIGURE_ARGS += 'CC=$(CC)'
CONFIGURE_ARGS += 'CXX=$(CXX)'
CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=ld'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CXX)'
CONFIGURE_ARGS += 'CXXLINKSHARED=makeC++SharedLib_r'
CONFIGURE_ARGS += 'AR=$(AR)'

CONFIGURE_ARGS += 'OMR_HOST_OS=aix'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=ppc'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=xlc'
