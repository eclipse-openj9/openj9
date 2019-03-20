# Copyright (c) 2000, 2019 IBM Corp. and others
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

#
# Explicitly set shell
#
SHELL=/bin/sh

#
# These are the prefixes and suffixes that all GNU tools use for things
#
OBJSUFF=.o
ARSUFF=.a
SOSUFF=.so
EXESUFF=
LIBPREFIX=lib
DEPSUFF=.depend.mk

#
# Paths for default programs on the platform
# Most rules will use these default programs, but they can be overwritten individually if,
# for example, you want to compile .spp files with a different C++ compiler than you use
# to compile .cpp files
#

# Use default AR=ar
# Use default AS=as
SED?=sed
PERL?=perl

ifeq (default,$(origin CC))
    CC=xlc_r
endif
ifeq (default,$(origin CXX))
    CXX=xlC_r
endif

#
# First setup C and C++ compilers.
#
#     Note: "CX" means both C and C++
#

CX_DEFINES+=\
    $(PRODUCT_DEFINES) \
    $(HOST_DEFINES) \
    $(TARGET_DEFINES) \
    SUPPORTS_THREAD_LOCAL \
    LINUXPPC \
    USING_ANSI

CX_FLAGS+=\
    -qtls \
    -qpic=large \
    -qxflag=selinux \
    -qalias=noansi \
    -qfuncsect \
    -qxflag=LTOL:LTOL0 \
    -qarch=$(CX_ARCH)

CXX_FLAGS+=\
    -qlanglvl=extended0x \
    -qnortti \
    -w

CX_DEFINES_DEBUG+=DEBUG
CX_FLAGS_DEBUG+=-g -qfullpath

CX_DEFAULTOPT=-O3
CX_OPTFLAG?=$(CX_DEFAULTOPT)
CX_FLAGS_PROD+=$(CX_OPTFLAG) -qdebug=nscrep

ifdef ENABLE_SIMD_LIB
    CX_DEFINES+=ENABLE_SPMD_SIMD
    CX_ARCH?=pwr7
    CX_FLAGS+=-qaltivec -qtune=pwr7
endif

ifeq ($(C_COMPILER),xlc)
    CX_FLAGS+=-qsuppress=1540-1087:1540-1088:1540-1090:1540-029
endif

ifeq ($(C_COMPILER),clangtana)
    CX_FLAGS+=-qnoxlcompatmacros -Wno-c++11-narrowing
endif

ifeq ($(BUILD_CONFIG),debug)
    CX_DEFINES+=$(CX_DEFINES_DEBUG)
    CX_FLAGS+=$(CX_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    CX_DEFINES+=$(CX_DEFINES_PROD)
    CX_FLAGS+=$(CX_FLAGS_PROD)
endif

C_CMD?=$(CC)
C_INCLUDES=$(PRODUCT_INCLUDES)
C_DEFINES+=$(CX_DEFINES) $(CX_DEFINES_EXTRA) $(C_DEFINES_EXTRA)
C_FLAGS+=$(CX_FLAGS) $(CX_FLAGS_EXTRA) $(C_FLAGS_EXTRA)

CXX_CMD?=$(CXX)
CXX_INCLUDES=$(PRODUCT_INCLUDES)
CXX_DEFINES+=$(CX_DEFINES) $(CX_DEFINES_EXTRA) $(CXX_DEFINES_EXTRA)
CXX_FLAGS+=$(CX_FLAGS) $(CX_FLAGS_EXTRA) $(CXX_FLAGS_EXTRA)

#
# Now setup GAS
#
S_CMD?=$(AS)

S_FLAGS+=-maltivec

ifeq ($(BUILD_CONFIG),debug)
    S_DEFINES+=$(S_DEFINES_DEBUG)
    S_FLAGS+=$(S_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    S_DEFINES+=$(S_DEFINES_PROD)
    S_FLAGS+=$(S_FLAGS_PROD)
endif

S_DEFINES+=$(S_DEFINES_EXTRA)
S_FLAGS+=$(S_FLAGS_EXTRA)

#
# Setup IPP
#
IPP_CMD?=$(SED)

#
#
# Setup SPP
SPP_DEFINES+=\
    $(PRODUCT_DEFINES) \
    $(HOST_DEFINES) \
    $(TARGET_DEFINES) \
    SUPPORTS_THREAD_LOCAL \
    LINUXPPC \
    USING_ANSI

SPP_FLAGS+=\
    -qtls \
    -qpic=large \
    -qxflag=selinux \
    -qalias=noansi \
    -qfuncsect \
    -qxflag=LTOL:LTOL0 \
    -qarch=$(CX_ARCH)

SPP_DEFINES_DEBUG+=DEBUG
SPP_FLAGS_DEBUG+=-g -qfullpath

SPP_DEFAULTOPT=-O3
SPP_OPTFLAG?=$(SPP_DEFAULTOPT)
SPP_FLAGS_PROD+=$(SPP_OPTFLAG) -qdebug=nscrep

ifdef ENABLE_SIMD_LIB
    SPP_DEFINES+=ENABLE_SPMD_SIMD
    SPP_FLAGS+=-qaltivec -qarch=pwr7 -qtune=pwr7
endif

ifeq ($(C_COMPILER),xlc)
    SPP_FLAGS+=-qsuppress=1540-1087:1540-1088:1540-1090:1540-029
endif

ifeq ($(C_COMPILER),clangtana)
    SPP_FLAGS+=-qnoxlcompatmacros -Wno-c++11-narrowing
endif

ifeq ($(BUILD_CONFIG),debug)
    SPP_DEFINES+=$(SPP_DEFINES_DEBUG)
    SPP_FLAGS+=$(SPP_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    SPP_DEFINES+=$(SPP_DEFINES_PROD)
    SPP_FLAGS+=$(SPP_FLAGS_PROD)
endif

SPP_CMD?=$(CC)
SPP_INCLUDES=$(PRODUCT_INCLUDES)
SPP_DEFINES+=$(SPP_DEFINES_EXTRA)
SPP_FLAGS+=$(SPP_FLAGS_EXTRA)

#
# Finally setup the linker
#
SOLINK_CMD?=$(CC)

SOLINK_FLAGS+=-qmkshrobj -qxflag=selinux
SOLINK_LIBPATH+=$(PRODUCT_LIBPATH)
SOLINK_SLINK+=\
    $(PRODUCT_SLINK) \
    j9thr$(J9_VERSION)\
    j9hookable$(J9_VERSION) \
    dl

#
# Adjust for 64bit
#
ifeq ($(HOST_BITS),64)
    CX_DEFINES+=LINUXPPC64
    CX_FLAGS+=-q64
    CX_ARCH?=ppc64
    SPP_DEFINES+=LINUXPPC64
    SPP_FLAGS+=-q64
    S_FLAGS+=-a64 -mppc64
    SOLINK_FLAGS+=-q64
endif

ifeq ($(HOST_BITS),32)
    CX_ARCH?=ppc
    S_FLAGS+=-a32 -mppc
endif

SOLINK_EXTRA_ARGS+=-Wl,-static -libmc++ -Wl,-call_shared -lstdc++

# Determine which export script to use depending on whether debug symbols are on
SOLINK_VERSION_SCRIPT=$(JIT_SCRIPT_DIR)/j9jit.linux.exp
ifeq ($(ASSUMES),1)
    SOLINK_VERSION_SCRIPT=$(JIT_SCRIPT_DIR)/j9jit.linux.debug.exp
endif
ifeq ($(BUILD_CONFIG),debug)
    SOLINK_VERSION_SCRIPT=$(JIT_SCRIPT_DIR)/j9jit.linux.debug.exp
endif

SOLINK_EXTRA_ARGS+=-Wl,--version-script=$(SOLINK_VERSION_SCRIPT)

ifeq ($(BUILD_CONFIG),debug)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_PROD)
endif

SOLINK_FLAGS+=$(SOLINK_FLAGS_EXTRA)
