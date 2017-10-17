###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

TEMP_TARGET_DATASIZE:=64

include $(CONFIG_INCL_DIR)/configure_common.mk

CONFIGURE_ARGS += \
  --enable-OMR_EXAMPLE \
  --enable-OMR_GC \
  --enable-OMR_JITBUILDER \
  --enable-OMR_PORT \
  --enable-OMR_THREAD \
  --enable-OMR_OMRSIG \
  --enable-OMRTHREAD_LIB_UNIX \
  --enable-OMR_ARCH_X86 \
  --enable-OMR_ENV_DATA64 \
  --enable-OMR_ENV_LITTLE_ENDIAN \
  --enable-OMR_GC_TLH_PREFETCH_FTA \
  --enable-OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS \
  --enable-OMR_TEST_COMPILER \
  --enable-OMR_THR_FORK_SUPPORT \
  --enable-OMR_THR_THREE_TIER_LOCKING \
  --enable-OMR_THR_YIELD_ALG \
  --enable-OMR_GC_ARRAYLETS \
  --enable-OMR_THR_SPIN_WAKE_CONTROL


CONFIGURE_ARGS += libprefix=lib exeext= solibext=.dylib arlibext=.a objext=.o

CONFIGURE_ARGS += 'AS=as'
CONFIGURE_ARGS += 'CC=cc'
CONFIGURE_ARGS += 'CXX=c++'
CONFIGURE_ARGS += 'CCLINK=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKSHARED=$$(CC)'
CONFIGURE_ARGS += 'CXXLINKEXE=$$(CXX)'
CONFIGURE_ARGS += 'AR=ar'

CONFIGURE_ARGS += 'OMR_HOST_OS=osx'
CONFIGURE_ARGS += 'OMR_HOST_ARCH=x86'
CONFIGURE_ARGS += 'OMR_TARGET_DATASIZE=$(TEMP_TARGET_DATASIZE)'
CONFIGURE_ARGS += 'OMR_TOOLCHAIN=gcc'
