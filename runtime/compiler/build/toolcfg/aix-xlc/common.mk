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
# These are the prefixes and suffixes that all AIX tools use for things
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
ifeq (default,$(origin CC))
    CC=xlc_r
endif
ifeq (default,$(origin CXX))
    CXX=xlC_r
endif
SED?=sed
PERL?=perl
SHAREDLIB?=makeC++SharedLib_r

#
# First setup C and C++ compilers. 
#
#     Note: "CX" means both C and C++
#

CX_DEFINES+=\
    $(PRODUCT_DEFINES) \
    $(HOST_DEFINES) \
    $(TARGET_DEFINES) \
    AIXPPC \
    RS6000 \
    _XOPEN_SOURCE_EXTENDED=1 \
    _ALL_SOURCE \
    SUPPORTS_THREAD_LOCAL

CX_FLAGS+=\
    -qarch=$(CX_ARCH) \
    -qtls \
    -qnotempinc \
    -qenum=small \
    -qmbcs \
    -qlanglvl=extended0x \
    -qfuncsect \
    -qsuppress=1540-1087:1540-1088:1540-1090:1540-029:1500-029 \
    -qdebug=nscrep

CX_DEFINES_DEBUG+=DEBUG
CX_FLAGS_DEBUG+=-g -qfullpath

CX_DEFAULTOPT=-O3
CX_OPTFLAG?=$(CX_DEFAULTOPT)
CX_FLAGS_PROD+=$(CX_OPTFLAG)
    
ifdef ENABLE_SIMD_LIB
    CX_DEFINES+=ENABLE_SPMD_SIMD
    CX_ARCH?=pwr7
    CX_FLAGS+=-qaltivec -qtune=pwr7
endif

ifeq ($(HOST_BITS),64)
    CX_DEFINES+=PPC64
    CX_FLAGS+=-q64
    # On AIX we don't expect to see any 64-bit POWER processors that don't support Graphics (gr) and Square Root (sq)
    # instructions, so we can use this more aggressive option rather than the plain -qarch=ppc64.
    CX_ARCH?=ppc64grsq
endif

ifeq ($(HOST_BITS),32)
    CX_ARCH?=ppc
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
# Now setup Assembler
#
S_CMD?=$(AS)

ifeq ($(HOST_BITS),32)
    S_FLAGS+=-mppc
endif

ifeq ($(HOST_BITS),64)
    S_FLAGS+=-mppc64 -a64
endif

ifeq ($(BUILD_CONFIG),debug)
    S_FLAGS+=$(S_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    S_FLAGS+=$(S_FLAGS_PROD)
endif

S_FLAGS+=$(S_FLAGS_EXTRA)

#
# Now setup SPP
#
SPP_CMD?=$(CC)

SPP_INCLUDES=$(PRODUCT_INCLUDES)

SPP_DEFINES+=\
    $(PRODUCT_DEFINES) \
    $(HOST_DEFINES) \
    $(TARGET_DEFINES) \
    AIXPPC \
    RS6000 \
    _XOPEN_SOURCE_EXTENDED=1 \
    _ALL_SOURCE \
    SUPPORTS_THREAD_LOCAL \
    $(SPP_DEFINES_EXTRA)

ifeq ($(BUILD_CONFIG),debug)
    SPP_FLAGS+=$(SPP_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    SPP_FLAGS+=$(SPP_FLAGS_PROD)
endif
    
SPP_FLAGS+=$(SPP_FLAGS_EXTRA)

# Now setup IPP
IPP_CMD?=$(SED)

ifeq ($(BUILD_CONFIG),debug)
    IPP_FLAGS+=$(IPP_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    IPP_FLAGS+=$(IPP_FLAGS_PROD)
endif

IPP_FLAGS+=$(IPP_FLAGS_EXTRA)

#
# Finally setup the linker
#
SOLINK_CMD?=$(SHAREDLIB)

SOLINK_FLAGS+=-p0 -bloadmap:lmap -brtl -bnoentry

ifeq ($(HOST_BITS),64)
    SOLINK_FLAGS+=-X64
endif

SOLINK_SLINK+=$(PRODUCT_SLINK) m j9thr$(J9_VERSION) j9hookable$(J9_VERSION)

SOLINK_LIBPATH+=$(PRODUCT_LIBPATH)

SOLINK_EXTRA_ARGS+=-E $(JIT_SCRIPT_DIR)/j9jit.aix.exp

ifeq ($(BUILD_CONFIG),debug)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_PROD)
endif

SOLINK_FLAGS+=$(SOLINK_FLAGS_EXTRA)
