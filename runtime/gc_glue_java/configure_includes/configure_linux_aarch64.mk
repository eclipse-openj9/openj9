###############################################################################
# Copyright IBM Corp. and others 2019
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
	--enable-OMR_THR_THREE_TIER_LOCKING \
	--enable-OMR_THR_YIELD_ALG \
	--enable-OMR_THR_SPIN_WAKE_CONTROL

OPENJ9_CC_PREFIX ?= aarch64-linux-gnu

CONFIGURE_ARGS += \
	--enable-OMRTHREAD_LIB_UNIX \
	--enable-OMR_ARCH_AARCH64 \
	--enable-OMR_ENV_DATA64 \
	--enable-OMR_GC_SPARSE_HEAP_ALLOCATION \
	--enable-OMR_ENV_LITTLE_ENDIAN \
	--enable-OMR_GC_CONCURRENT_SCAVENGER \
	--enable-OMR_GC_IDLE_HEAP_MANAGER \
	--enable-OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD \
	--enable-OMR_GC_TLH_PREFETCH_FTA \
	--enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS

ifneq (,$(findstring _cmprssptrs,$(SPEC)))
	CONFIGURE_ARGS += \
		OMR_GC_POINTER_MODE=compressed
else
	CONFIGURE_ARGS += \
		OMR_GC_POINTER_MODE=full
endif

ifneq (,$(findstring _cross,$(SPEC)))
	CONFIGURE_ARGS += \
	--host=$(OPENJ9_CC_PREFIX) \
	--build=x86_64-pc-linux-gnu \
	'OMR_CROSS_CONFIGURE=yes'

	ifeq (default,$(origin AS))
		AS = $(OPENJ9_CC_PREFIX)-as
	endif
	ifeq (default,$(origin CC))
		CC = $(OPENJ9_CC_PREFIX)-gcc
	endif
	ifeq (default,$(origin CXX))
		CXX = $(OPENJ9_CC_PREFIX)-g++
	endif
	ifeq (default,$(origin AR))
		AR = $(OPENJ9_CC_PREFIX)-ar
	endif
	OBJCOPY = $(OPENJ9_CC_PREFIX)-objcopy
else
	OBJCOPY ?= objcopy
endif

CONFIGURE_ARGS += 'AS=$(AS)'
CONFIGURE_ARGS += 'CC=$(CC)'
CONFIGURE_ARGS += 'CXX=$(CXX)'
CONFIGURE_ARGS += 'OBJCOPY=$(OBJCOPY)'

CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CXX)'
CONFIGURE_ARGS += 'CXXLINKSHARED=$$(CXX)'
CONFIGURE_ARGS += 'AR=$(AR)'

CONFIGURE_ARGS += 'OMR_HOST_OS=linux'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=aarch64'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=64'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=gcc'

ifeq (OMR_CROSS_CONFIGURE=yes,$(findstring OMR_CROSS_CONFIGURE=yes,$(CONFIGURE_ARGS)))
CONFIGURE_ARGS += 'OMR_TOOLS_CC=gcc'
CONFIGURE_ARGS += 'OMR_TOOLS_CXX=g++'
CONFIGURE_ARGS += 'OMR_BUILD_DATASIZE=64'
CONFIGURE_ARGS += 'OMR_BUILD_TOOLCHAIN=gcc'
endif

CONFIGURE_ARGS+= 'GLOBAL_CFLAGS=-fstack-protector'
CONFIGURE_ARGS+= 'GLOBAL_CXXFLAGS=-fstack-protector'
