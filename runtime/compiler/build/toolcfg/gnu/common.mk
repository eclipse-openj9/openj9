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
ifeq ($(OS),osx)
SOSUFF=.dylib
else
SOSUFF=.so
endif
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
M4?=m4
SED?=sed
PERL?=perl

#
# z/Architecture arch and tune level
#

ARCHLEVEL?=z9-109
TUNELEVEL?=z10

ifeq ($(C_COMPILER),gcc)
    ifeq (default,$(origin CC))
        CC=gcc
    endif
    # Use default CXX=g++
endif

ifeq ($(C_COMPILER),clang)
    ifeq (default,$(origin CC))
        CC=clang
    endif
    ifeq (default,$(origin CXX))
        CXX=clang++
    endif
endif

# This is the script to preprocess ARM assembly files
ARMASM_SCRIPT?=$(JIT_SCRIPT_DIR)/armasm2gas.sed

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
    SUPPORTS_THREAD_LOCAL \
    _LONG_LONG

# Explicitly set the C standard we compile against
# Clang defaults to gnu99 (vs GCC's gnu89) unless explicitly overridden
C_FLAGS+=\
    -std=gnu89

CX_FLAGS+=\
    -pthread \
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
CX_FLAGS_DEBUG+=-ggdb3

CX_DEFAULTOPT=-O3
CX_OPTFLAG?=$(CX_DEFAULTOPT)
CX_FLAGS_PROD+=$(CX_OPTFLAG)

ifeq ($(HOST_ARCH),x)
    CX_FLAGS+=-mfpmath=sse -msse -msse2 -fno-math-errno -fno-trapping-math

    ifeq ($(HOST_BITS),32)
        CX_FLAGS+=-m32 -fpic
    endif

    ifeq ($(HOST_BITS),64)
        CX_DEFINES+=J9HAMMER
        CX_FLAGS+=-m64 -fPIC
    endif
endif

ifeq ($(HOST_ARCH),p)
    CX_DEFAULTOPT=-O2
    CX_FLAGS+=-fpic

    ifeq ($(HOST_BITS),32)
        CX_DEFINES+=LINUXPPC USING_ANSI
        CX_FLAGS+=-m32
        CX_FLAGS_PROD+=-mcpu=powerpc
    endif

    ifeq ($(HOST_BITS),64)
        CX_DEFINES+=LINUXPPC LINUXPPC64 USING_ANSI
        CX_FLAGS_PROD+=-mcpu=powerpc64
    endif

    ifdef ENABLE_SIMD_LIB
        CX_DEFINES+=ENABLE_SPMD_SIMD
        CX_FLAGS+=-qaltivec -qarch=pwr7 -qtune=pwr7
    endif
endif

ifeq ($(HOST_ARCH),z)
    ifeq ($(HOST_BITS),32)
        CX_DEFINES+=J9VM_TIERED_CODE_CACHE MAXMOVE S390 FULL_ANSI
        CX_FLAGS+=-m31 -fPIC -march=$(ARCHLEVEL) -mtune=$(TUNELEVEL) -mzarch
    endif

    ifeq ($(HOST_BITS),64)
        CX_DEFINES+=S390 S39064 FULL_ANSI MAXMOVE J9VM_TIERED_CODE_CACHE
        CX_FLAGS+=-fPIC -march=$(ARCHLEVEL) -mtune=$(TUNELEVEL) -mzarch
    endif

    CX_FLAGS_DEBUG+=-gdwarf-2
endif

ifeq ($(HOST_ARCH),arm)
    CX_DEFINES+=ARMGNU ARMGNUEABI FIXUP_UNALIGNED HARDHAT
    CX_FLAGS+=-fPIC -mfloat-abi=hard -mfpu=vfp -march=armv6 -marm
endif

ifeq ($(HOST_ARCH),aarch64)
    CX_FLAGS+=-fPIC
endif

ifeq ($(C_COMPILER),clang)
    CX_FLAGS+=-Wno-parentheses -Werror=header-guard
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

ifeq ($(HOST_ARCH),p)
    S_FLAGS+=-maltivec
    ifeq ($(HOST_BITS),32)
        S_FLAGS+=-a32 -mppc
    endif

    ifeq ($(HOST_BITS),64)
        S_FLAGS+=-a64 -mppc64
    endif
endif

ifeq ($(HOST_ARCH),z)
    ifeq ($(HOST_BITS),32)
        S_FLAGS+=-m31 -mzarch -march=$(ARCHLEVEL)
    endif

    ifeq ($(HOST_BITS),64)
        S_FLAGS+=-march=$(ARCHLEVEL) -mzarch
    endif
endif

ifeq ($(HOST_ARCH),arm)
    S_FLAGS+=-mfloat-abi=hard -mfpu=vfp
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

ifeq ($(HOST_ARCH),x)
#
# Setup NASM
#

    NASM_CMD?=nasm

    NASM_DEFINES=\
        TR_HOST_X86 \
        TR_TARGET_X86

    ifeq ($(OS),osx)
        NASM_DEFINES+=\
            OSX
    else
        NASM_DEFINES+=\
            LINUX
    endif

    NASM_INCLUDES=\
        $(J9SRC)/oti \
        $(J9SRC)/compiler \
        $(J9SRC)/compiler/x/runtime

    ifeq ($(HOST_BITS),32)
        NASM_OBJ_FORMAT=-felf32

        NASM_DEFINES+=\
            TR_HOST_32BIT \
            TR_TARGET_32BIT

        NASM_INCLUDES+=\
            $(J9SRC)/compiler/x/i386/runtime
    else
        ifeq ($(OS),osx)
            NASM_OBJ_FORMAT=-fmacho64
        else
            NASM_OBJ_FORMAT=-felf64
        endif

        NASM_DEFINES+=\
            TR_HOST_64BIT \
            TR_TARGET_64BIT

        NASM_INCLUDES+=\
            $(J9SRC)/compiler/x/amd64/runtime
    endif

endif # HOST_ARCH == x

#
# Setup CPP and SED to preprocess PowerPC Assembly Files
#
ifeq ($(HOST_ARCH),p)
    IPP_CMD=$(SED)

    ifeq ($(BUILD_CONFIG),debug)
        IPP_FLAGS+=$(IPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        IPP_FLAGS+=$(IPP_FLAGS_PROD)
    endif

    IPP_FLAGS+=$(IPP_FLAGS_EXTRA)

    SPP_CMD=$(CC)

    SPP_INCLUDES=$(PRODUCT_INCLUDES)
    SPP_DEFINES+=$(CX_DEFINES) $(SPP_DEFINES_EXTRA)
    SPP_FLAGS+=$(CX_FLAGS)

    ifeq ($(BUILD_CONFIG),debug)
        SPP_FLAGS+=$(SPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        SPP_FLAGS+=$(SPP_FLAGS_PROD)
    endif

    SPP_FLAGS+=$(SPP_FLAGS_EXTRA)
endif

#
# Now we setup M4 to preprocess Z assembly files
#
ifeq ($(HOST_ARCH),z)
    M4_CMD?=$(M4)

    M4_INCLUDES=$(PRODUCT_INCLUDES)

    M4_DEFINES+=$(HOST_DEFINES) $(TARGET_DEFINES)
    M4_DEFINES+=J9VM_TIERED_CODE_CACHE

    ifeq ($(HOST_BITS),32)
        ifneq (,$(shell grep 'define J9VM_JIT_32BIT_USES64BIT_REGISTERS' $(J9SRC)/include/j9cfg.h))
            M4_DEFINES+=J9VM_JIT_32BIT_USES64BIT_REGISTERS
        endif
    endif

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
# Now setup stuff for ARM assembly
#
ifeq ($(HOST_ARCH),arm)
    ARMASM_CMD?=$(SED)

    SPP_CMD?=$(CC)

    SPP_INCLUDES=$(PRODUCT_INCLUDES)
    SPP_DEFINES+=$(CX_DEFINES)
    SPP_FLAGS+=$(CX_FLAGS)

    ifeq ($(BUILD_CONFIG),debug)
        SPP_DEFINES+=$(SPP_DEFINES_DEBUG)
        SPP_FLAGS+=$(SPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        SPP_DEFINES+=$(SPP_DEFINES_PROD)
        SPP_FLAGS+=$(SPP_FLAGS_PROD)
    endif

    SPP_DEFINES+=$(SPP_DEFINES_EXTRA)
    SPP_FLAGS+=$(SPP_FLAGS_EXTRA)
endif # HOST_ARCH == arm

#
# Setup CPP to preprocess AArch64 Assembly Files
#
ifeq ($(HOST_ARCH),aarch64)
    SPP_CMD=$(CC)

    SPP_INCLUDES=$(PRODUCT_INCLUDES)
    SPP_DEFINES+=$(CX_DEFINES)
    SPP_FLAGS+=$(CX_FLAGS)

    ifeq ($(BUILD_CONFIG),debug)
        SPP_FLAGS+=$(SPP_FLAGS_DEBUG)
    endif

    ifeq ($(BUILD_CONFIG),prod)
        SPP_FLAGS+=$(SPP_FLAGS_PROD)
    endif

    SPP_DEFINES+=$(SPP_DEFINES_EXTRA)
    SPP_FLAGS+=$(SPP_FLAGS_EXTRA)
endif # HOST_ARCH == aarch64

#
# Finally setup the linker
#
SOLINK_CMD?=$(CXX)

SOLINK_FLAGS+=
SOLINK_FLAGS_PROD+=-Wl,-S

SOLINK_LIBPATH+=$(PRODUCT_LIBPATH)
SOLINK_SLINK+=$(PRODUCT_SLINK) j9thr$(J9_VERSION) j9hookable$(J9_VERSION)

ifeq ($(HOST_ARCH),x)
    ifeq ($(HOST_BITS),32)
        SOLINK_FLAGS+=-m32
        SOLINK_SLINK+=dl m omrsig
    endif

    ifeq ($(HOST_BITS),64)
        SOLINK_FLAGS+=-m64
        SOLINK_SLINK+=dl m
    endif

    # Using the linker option -static-libgcc results in an error on OSX. The option -static-libstdc++ is unused. Therefore
    # these options have been excluded from OSX
    ifneq ($(OS),osx)
        SUPPORT_STATIC_LIBCXX = $(shell $(SOLINK_CMD) -static-libstdc++ 2>&1 | grep "unrecognized option" > /dev/null; echo $$?)
        ifneq ($(SUPPORT_STATIC_LIBCXX),0)
            SOLINK_FLAGS+=-static-libgcc -static-libstdc++
        endif
    endif
endif

ifeq ($(HOST_ARCH),p)
    ifeq ($(HOST_BITS),32)
        SOLINK_FLAGS+=-m32 -fpic
        SOLINK_SLINK+=dl m
    endif

    ifeq ($(HOST_BITS),64)
        SOLINK_FLAGS+=-m64 -fpic
        SOLINK_SLINK+=dl m pthread
    endif

    SUPPORT_STATIC_LIBCXX = $(shell $(SOLINK_CMD) -static-libstdc++ 2>&1 | grep "unrecognized option" > /dev/null; echo $$?)
    ifneq ($(SUPPORT_STATIC_LIBCXX),0)
        SOLINK_FLAGS+=-static-libgcc -static-libstdc++
    endif
endif

ifeq ($(HOST_ARCH),z)
    ifeq ($(HOST_BITS),32)
        SOLINK_FLAGS+=-m31
    endif

    SUPPORT_STATIC_LIBCXX = $(shell $(SOLINK_CMD) -static-libstdc++ 2>&1 | grep "unrecognized option" > /dev/null; echo $$?)
    ifneq ($(SUPPORT_STATIC_LIBCXX),0)
        SOLINK_FLAGS+=-static-libgcc -static-libstdc++
    endif
endif

ifeq ($(HOST_ARCH),arm)
    SOLINK_SLINK+=c m gcc_s
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

ifeq ($(OS),linux)
SOLINK_EXTRA_ARGS+=-Wl,--version-script=$(SOLINK_VERSION_SCRIPT)
endif

SOLINK_FLAGS+=$(SOLINK_FLAGS_EXTRA)
