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

TEMP_TARGET_DATASIZE:=64

CONFIGURE_ARGS += \
	--enable-OMR_THR_THREE_TIER_LOCKING \
	--enable-OMR_THR_YIELD_ALG \
	--enable-OMR_THR_SPIN_WAKE_CONTROL \
	--enable-OMRTHREAD_LIB_UNIX \
	--enable-OMR_ARCH_S390 \
	--enable-OMR_ENV_DATA64 \
	--enable-OMR_GC_CONCURRENT_SCAVENGER \
	--enable-OMR_GC_SPARSE_HEAP_ALLOCATION \
	OMR_GC_POINTER_MODE=full


CONFIGURE_ARGS += libprefix=lib exeext= solibext=.so arlibext=.a objext=.o

ifeq (default,$(origin CC))
	CC=tpf-gcc
endif
ifeq (default,$(origin CXX))
	CXX=tpf-g++
endif
ifeq (default,$(origin AS))
	AS=tpf-as
endif

CONFIGURE_ARGS += --host=s390x-ibm-tpf
CONFIGURE_ARGS += --build=s390x-ibm-linux-gnu
CONFIGURE_ARGS += 'AS=$(AS)'
CONFIGURE_ARGS += 'CC=$(CC)'
CONFIGURE_ARGS += 'CXX=$(CXX)'
CONFIGURE_ARGS += 'CCLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CCLINKSHARED=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKSHARED=$$(CC)'
CONFIGURE_ARGS += 'AR=$(AR)'
CONFIGURE_ARGS += 'RM=$(RM)'
CONFIGURE_ARGS += 'OMR_HOST_OS=linux_ztpf'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=s390'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_BUILD_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_ENV_DATA64=1'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=gcc'
CONFIGURE_ARGS += 'OMR_BUILD_TOOLCHAIN=gcc'
CONFIGURE_ARGS += 'OMR_TOOLS_CC=gcc'
CONFIGURE_ARGS += 'OMR_TOOLS_CXX=g++'

