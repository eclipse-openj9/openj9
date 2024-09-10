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

CONFIGURE_ARGS += \
	--enable-debug \
	--enable-OMR_THR_THREE_TIER_LOCKING \
	--enable-OMR_THR_SPIN_WAKE_CONTROL

ifeq (win_x86-64_cmprssptrs, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_WIN32 \
		--enable-OMR_ARCH_X86 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_GC_CONCURRENT_SCAVENGER \
		--enable-OMR_GC_SPARSE_HEAP_ALLOCATION \
		--enable-OMR_GC_TLH_PREFETCH_FTA \
		--enable-OMR_PORT_ALLOCATE_TOP_DOWN \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		OMR_GC_POINTER_MODE=compressed
endif

ifeq (win_x86-64, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_WIN32 \
		--enable-OMR_ARCH_X86 \
		--enable-OMR_ENV_DATA64 \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_GC_CONCURRENT_SCAVENGER \
		--enable-OMR_GC_SPARSE_HEAP_ALLOCATION \
		--enable-OMR_GC_TLH_PREFETCH_FTA \
		--enable-OMR_PORT_ALLOCATE_TOP_DOWN \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		OMR_GC_POINTER_MODE=full
endif

ifeq (win_x86, $(SPEC))
	CONFIGURE_ARGS += \
		--enable-OMRTHREAD_LIB_WIN32 \
		--enable-OMR_ARCH_X86 \
		--enable-OMR_ENV_LITTLE_ENDIAN \
		--enable-OMR_GC_TLH_PREFETCH_FTA \
		--enable-OMR_PORT_ALLOCATE_TOP_DOWN \
		--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
		OMR_GC_POINTER_MODE=full
endif

CONFIGURE_ARGS += libprefix= exeext=.exe solibext=.dll arlibext=.lib objext=.obj

ifeq (default,$(origin AS))
	ifeq (64,$(TEMP_TARGET_DATASIZE))
		AS=ml64
	else
		AS=ml
	endif
endif
ifeq (default,$(origin CC))
	CC=cl
endif
ifeq (default,$(origin CXX))
	CXX=$(CC)
endif
ifeq (default,$(origin AR))
	AR=lib
endif

CONFIGURE_ARGS += 'AS=$(AS)'
CONFIGURE_ARGS += 'CC=$(CC)'
CONFIGURE_ARGS += 'CXX=$(CXX)'
CONFIGURE_ARGS += 'CCLINK=link'
CONFIGURE_ARGS += 'CXXLINK=link'
CONFIGURE_ARGS += 'AR=$(AR)'

CONFIGURE_ARGS += 'OMR_HOST_OS=win'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=x86'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=msvc'

CONFIGURE_ARGS += 'OMR_COMPANY_NAME=International Business Machines Corporation'
CONFIGURE_ARGS += 'OMR_COMPANY_COPYRIGHT=Copyright (c) 1991, $(shell date "+%Y") IBM Corp. and others'
CONFIGURE_ARGS += 'OMR_PRODUCT_NAME=IBM SDK, Java(tm) 2 Technology Edition'
CONFIGURE_ARGS += 'OMR_PRODUCT_DESCRIPTION=J9 Virtual Machine Runtime'
CONFIGURE_ARGS += 'OMR_PRODUCT_VERSION=$(OPENJDK_VERSION_NUMBER_FOUR_POSITIONS)'
