# Copyright (c) 2017, 2019 IBM Corp. and others
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

# z/TPF only has one s390x configuration:
#  - HOST_ARCH=z, HOST_BITS=64, OS=ztpf, C_COMPILER=tpf-gcc
# Applicable conditionals to the above fields have been left in this file to
# maintain similarity with Linux, while other architecture-dependent flags
# have been removed for simplicity.

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
ifeq (default,$(origin AS))
    AS=tpf-as
endif
M4?=m4
SED?=sed
PERL?=perl

#
# z/Architecture arch and tune level
# z/TPF uses z10 as minimum supported z/Architecture.
# mtune of z9-109 allows builtin versions of memcpy, etc.
# instead of calling "arch-optimized" glibc functions.
#

ARCHLEVEL?=z10
TUNELEVEL?=z9-109

ifeq (default,$(origin CC))
    CC=tpf-gcc
endif
ifeq (default,$(origin CXX))
    CXX=tpf-g++
endif

# This is the command to check Z assembly files
ZASM_SCRIPT?=$(JIT_SCRIPT_DIR)/s390m4check.pl

# Set up z/TPF directories and flags
# Note: -isystem $(TPF_ROOT) is used for a2e calls to opensource functions
TPF_ROOT ?= /ztpf/java/bld/jvm/userfiles /zbld/svtcur/gnu/all /ztpf/commit

TPF_INCLUDES := $(foreach d,$(TPF_ROOT),-I$d/base/a2e/headers)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/base/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/opensource/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/opensource/include46/g++)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/opensource/include46/g++/backward)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/noship/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-isystem $d/opensource/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-isystem $d/noship/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-isystem $d)

TPF_FLAGS += -fexec-charset=ISO-8859-1 -fmessage-length=0 -funsigned-char -fverbose-asm -fno-builtin-abort -fno-builtin-exit -fno-builtin-sprintf -ffloat-store -gdwarf-2 -Wno-format-extra-args -Wno-int-to-pointer-cast -Wno-unused-but-set-variable -Wno-write-strings

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
    _LONG_LONG

# Explicitly set the C standard we compile against
C_FLAGS+=\
    -std=gnu89

CX_FLAGS+=\
    $(TPF_INCLUDES) \
    $(TPF_FLAGS) \
    -fomit-frame-pointer \
    -fasynchronous-unwind-tables \
    -Wreturn-type \
    -fno-strict-aliasing

CXX_FLAGS+=\
    -std=c++0x \
    -fno-rtti \
    -fno-threadsafe-statics \
    -Wno-deprecated \
    -Wno-enum-compare \
    -Wno-invalid-offsetof \
    -Wno-write-strings

CX_DEFINES_DEBUG+=DEBUG

CX_DEFAULTOPT=-O3
CX_OPTFLAG?=$(CX_DEFAULTOPT)
CX_FLAGS_PROD+=$(CX_OPTFLAG)

ifeq ($(HOST_ARCH),z)
    ifeq ($(HOST_BITS),64)
        CX_DEFINES+=S390 S39064 FULL_ANSI MAXMOVE J9VM_TIERED_CODE_CACHE
        CX_DEFINES+=_GNU_SOURCE IBM_ATOE _TPF_SOURCE ZTPF_POSIX_SOCKET J9ZTPF OMRZTPF
        CX_FLAGS+=-fPIC -march=$(ARCHLEVEL) -mtune=$(TUNELEVEL) -mzarch
    endif
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

S_INCLUDES=$(PRODUCT_INCLUDES)

S_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)
S_DEFINES_DEBUG+=DEBUG

S_FLAGS+=--noexecstack
S_FLAGS_DEBUG+=--gstabs

ifeq ($(HOST_ARCH),z)
    ifeq ($(HOST_BITS),64)
        S_FLAGS+=-march=$(ARCHLEVEL) -mzarch
    endif
endif

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
# Now we setup M4 to preprocess Z assembly files
#
ifeq ($(HOST_ARCH),z)
    M4_CMD?=$(M4)

    M4_INCLUDES=$(PRODUCT_INCLUDES)

    M4_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)
    M4_DEFINES+=J9VM_TIERED_CODE_CACHE
    M4_DEFINES+=OMRZTPF
    M4_DEFINES+=J9ZTPF

    ifeq ($(HOST_BITS),64)
        ifneq (,$(shell grep 'define J9VM_INTERP_COMPRESSED_OBJECT_HEADER' $(J9SRC)/include/j9cfg.h))
            M4_DEFINES+=J9VM_INTERP_COMPRESSED_OBJECT_HEADER
        endif
        ifneq (,$(shell grep 'define J9VM_GC_COMPRESSED_POINTERS' $(J9SRC)/include/j9cfg.h))
            M4_DEFINES+=OMR_GC_COMPRESSED_POINTERS
        endif
    endif

    ifeq ($(BUILD_CONFIG),debug)
        M4_DEFINES+=$(M4_DEFINES_DEBUG)
        M4_FLAGS+=$(M4_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        M4_DEFINES+=$(M4_DEFINES_PROD)
        M4_FLAGS+=$(M4_FLAGS_PROD)
    endif

    M4_DEFINES+=$(M4_DEFINES_EXTRA)
    M4_FLAGS+=$(M4_FLAGS_EXTRA)
endif

#
# Finally setup the linker
#
SOLINK_CMD?=$(CXX)

SOLINK_FLAGS+=
SOLINK_FLAGS_PROD+=-Wl,-S

SOLINK_LIBPATH+=$(PRODUCT_LIBPATH)
SOLINK_SLINK+=$(PRODUCT_SLINK) j9thr$(J9_VERSION) j9hookable$(J9_VERSION)

# CTIS needs to be linked before CISO from sysroot for gettimeofday
ifeq ($(HOST_ARCH),z)
    ifeq ($(HOST_BITS),64)
        SOLINK_SLINK+=gcc
        SOLINK_SLINK+=CTOE
        SOLINK_SLINK+=CTIS
    endif
endif

ifeq ($(BUILD_CONFIG),debug)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_PROD)
endif

# Determine which export script to use depending on whether debug symbols are on
SOLINK_VERSION_SCRIPT=$(JIT_SCRIPT_DIR)/j9jit.linux.exp
ifeq ($(ASSUMES),1)
    SOLINK_VERSION_SCRIPT=$(JIT_SCRIPT_DIR)/j9jit.linux.debug.exp
endif
ifeq ($(BUILD_CONFIG),debug)
    SOLINK_VERSION_SCRIPT=$(JIT_SCRIPT_DIR)/j9jit.linux.debug.exp
endif

SOLINK_EXTRA_ARGS+=-Wl,--version-script=$(SOLINK_VERSION_SCRIPT)
SOLINK_EXTRA_ARGS+=-Wl,-Map=j9jit.map
SOLINK_EXTRA_ARGS+=-Wl,-entry=0
SOLINK_EXTRA_ARGS+=-Wl,-script=$(word 1,$(wildcard $(foreach d,$(TPF_ROOT),$d/base/util/tools/tpfscript)))
SOLINK_EXTRA_ARGS+=-Wl,-soname=libj9jit29.so
SOLINK_EXTRA_ARGS+=-Wl,--as-needed
SOLINK_EXTRA_ARGS+=-Wl,--eh-frame-hdr
SOLINK_EXTRA_ARGS+=$(foreach d,$(TPF_ROOT),-L$d/base/lib)
SOLINK_EXTRA_ARGS+=$(foreach d,$(TPF_ROOT),-L$d/base/stdlib)
SOLINK_EXTRA_ARGS+=$(foreach d,$(TPF_ROOT),-L$d/opensource/stdlib)

SOLINK_FLAGS+=$(SOLINK_FLAGS_EXTRA)
