# Copyright (c) 2017, 2017 IBM Corp. and others
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

AS_PATH?=tpf-as
M4_PATH?=m4
SED_PATH?=sed
AR_PATH?=ar
PERL_PATH?=perl

#
# z/Architecture arch and tune level
# z/TPF uses z10 as minminum supported z/Architecture.
# mtune of z9-109 allows builtin versions of memcpy, etc.
# instead of calling "arch-optimized" glibc functions.
#

ARCHLEVEL?=z10
TUNELEVEL?=z9-109

CC_PATH?=tpf-gcc
CXX_PATH?=tpf-g++

# This is the script that's used to generate TRBuildName.cpp
GENERATE_VERSION_SCRIPT?=$(JIT_SCRIPT_DIR)/generateVersion.pl

# This is the command to check Z assembly files
ZASM_SCRIPT?=$(JIT_SCRIPT_DIR)/s390m4check.pl

#
# First setup C and C++ compilers. 
#
#     Note: "CX" means both C and C++
#


CX_DEFINES+=\
    $(PRODUCT_DEFINES) \
    $(HOST_DEFINES) \
    $(TARGET_DEFINES) \
    _LONG_LONG

# Next, setup the z/TPF include directories (-isystem and -I)
# commit will always be the base root includes.
# On top of that, the buildtype will either be an svt or user defined.
# Note, the CX_FLAGS:  -isystem $(ZTPF_INCL) -isystem $(ZTPF_ROOT) are
# used for the a2e's library calls to opensource functions
ZTPF_ROOT?=/ztpf/commit
BUILDTYPE?=user

ifeq ($(BUILDTYPE),svt)
    ZTPF_INCL=/ztpf/svtcur/redhat/all
endif

ifeq ($(BUILDTYPE),user)
    ZTPF_INCL=/projects/jvmport/userfiles
endif

CX_FLAGS+=\
     -I$(ZTPF_INCL)/base/a2e/headers -I$(ZTPF_ROOT)/base/a2e/headers \
     -isystem $(ZTPF_INCL)/opensource/include -isystem $(ZTPF_ROOT)/opensource/include \
     -isystem $(ZTPF_INCL)/opensource/include46/g++ -isystem $(ZTPF_ROOT)/opensource/include46/g++ \
     -isystem $(ZTPF_INCL)/opensource/include46/g++/backward -isystem $(ZTPF_ROOT)/opensource/include46/g++/backward \
     -isystem $(ZTPF_INCL) -isystem $(ZTPF_ROOT) \
     -isystem $(ZTPF_INCL)/base/include -isystem $(ZTPF_ROOT)/base/include \
     -isystem $(ZTPF_INCL)/noship/include -isystem $(ZTPF_ROOT)/noship/include \
     -I$(ZTPF_INCL)/base/include -I$(ZTPF_ROOT)/base/include \
     -I$(ZTPF_INCL)/opensource/include -I$(ZTPF_ROOT)/opensource/include \
     -I$(ZTPF_INCL)/opensource/include46/g++ -I$(ZTPF_ROOT)/opensource/include46/g++ \
     -I$(ZTPF_INCL)/opensource/include46/g++/backward -I$(ZTPF_ROOT)/opensource/include46/g++/backward \
     -I$(ZTPF_INCL)/base/include -I$(ZTPF_ROOT)/base/include

# Explicitly set the C standard we compile against
C_FLAGS+=\
    -std=gnu89 \
    -fPIC \
    -fexec-charset=ISO-8859-1 \
    -fmessage-length=0 \
    -funsigned-char \
    -Wno-format-extra-args \
    -fverbose-asm \
    -fno-builtin-abort \
    -fno-builtin-exit \
    -ffloat-store \
    -Wreturn-type \
    -Wno-unused \
    -Wno-uninitialized \
    -Wno-parentheses

CX_FLAGS+=\
    -D_TPF_SOURCE \
    -DJ9ZTPF \
    -DOMRZTPF \
    -DLINUX \
    -DZTPF_POSIX_SOCKET \
    -DTR_HOST_S390 \
    -DTR_TARGET_S390 \
    -DTR_HOST_64BIT \
    -DTR_TARGET_64BIT \
    -DBITVECTOR_64BIT \
    -DFULL_ANSI \
    -DMAXMOVE \
    -DIBM_ATOE \
    -D_GNU_SOURCE \
    -D_PORTABLE_TPF_SIGINFO \
    -fomit-frame-pointer \
    -fasynchronous-unwind-tables \
    -Wreturn-type \
    -fno-dollars-in-identifiers

CXX_FLAGS+=\
    -DSUPPORTS_THREAD_LOCAL \
    -D_ZTPF_WANTS_ATOE_FUNCTIONALITY \
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

   #
   # z/TPF is a 64 bit host.
   #
   CX_DEFINES+=S390 S39064 FULL_ANSI MAXMOVE J9VM_TIERED_CODE_CACHE
   CX_FLAGS+=-fPIC -fno-strict-aliasing -march=$(ARCHLEVEL) -mtune=$(TUNELEVEL) -mzarch

ifeq ($(BUILD_CONFIG),debug)
    CX_DEFINES+=$(CX_DEFINES_DEBUG)
    CX_FLAGS+=$(CX_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    CX_DEFINES+=$(CX_DEFINES_PROD)
    CX_FLAGS+=$(CX_FLAGS_PROD)
endif

C_CMD?=$(CC_PATH)
C_INCLUDES=$(PRODUCT_INCLUDES)
C_DEFINES+=$(CX_DEFINES) $(CX_DEFINES_EXTRA) $(C_DEFINES_EXTRA)
C_FLAGS+=$(CX_FLAGS) $(CX_FLAGS_EXTRA) $(C_FLAGS_EXTRA)

CXX_CMD?=$(CXX_PATH)
CXX_INCLUDES=$(PRODUCT_INCLUDES)
CXX_DEFINES+=$(CX_DEFINES) $(CX_DEFINES_EXTRA) $(CXX_DEFINES_EXTRA)
CXX_FLAGS+=$(CX_FLAGS) $(CX_FLAGS_EXTRA) $(CXX_FLAGS_EXTRA)

#
# Now setup GAS
#
S_CMD?=$(AS_PATH)

S_INCLUDES=$(PRODUCT_INCLUDES)

S_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)
S_DEFINES_DEBUG+=DEBUG

S_FLAGS+=--noexecstack
S_FLAGS_DEBUG+=--gstabs

S_FLAGS+=-march=$(ARCHLEVEL) -mzarch

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
    M4_CMD?=$(M4_PATH)

    M4_INCLUDES=$(PRODUCT_INCLUDES)
    
    M4_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)
    M4_DEFINES+=J9VM_TIERED_CODE_CACHE
    M4_DEFINES+=OMRZTPF
    M4_DEFINES+=J9ZTPF
    
    ifeq ($(HOST_BITS),64)
        ifneq (,$(shell grep 'define J9VM_INTERP_COMPRESSED_OBJECT_HEADER' $(J9SRC)/include/j9cfg.h))
            M4_DEFINES+=J9VM_INTERP_COMPRESSED_OBJECT_HEADER
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
SOLINK_CMD?=$(CXX_PATH)

SOLINK_FLAGS+=
SOLINK_FLAGS_PROD+=-Wl,-S

SOLINK_LIBPATH+=$(PRODUCT_LIBPATH)
SOLINK_SLINK+=$(PRODUCT_SLINK) j9thr$(J9_VERSION) j9hookable$(J9_VERSION)

ifeq ($(HOST_ARCH),z)
    ifeq ($(HOST_BITS),32)
        SOLINK_FLAGS+=-m31
        SOLINK_SLINK+=stdc++
    endif
    
    ifeq ($(HOST_BITS),64)
        SOLINK_SLINK+=stdc++
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

ifeq ($(LIBCXX_STATIC),1)
    SOLINK_FLAGS+=-static-libstdc++
endif

SOLINK_FLAGS+=$(SOLINK_FLAGS_EXTRA)
