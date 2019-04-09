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
SHELL=cmd.exe
.SHELLFLAGS=/c

#
# Commands for this shell
#
FIXPATH=$(patsubst %,"%",$(subst /,\,$(1)))
MKDIR=if not exist $(call FIXPATH,$(dir $(1))) mkdir $(call FIXPATH,$(dir $(1)))
RM=if exist $(call FIXPATH,$(1)) del /f $(call FIXPATH,$(1))

#
# These are the prefixes and suffixes that all GNU tools use for things
#
OBJSUFF=.obj
ARSUFF=.lib
SOSUFF=.dll
EXESUFF=.exe
LIBPREFIX=
DEPSUFF=.depend.mk

#
# Paths for default programs on the platform
# Most rules will use these default programs, but they can be overwritten individually if,
# for example, you want to compile .spp files with a different C++ compiler than you use
# to compile .cpp files
#

ifeq ($(HOST_BITS),32)
    ML?=ml
endif

ifeq ($(HOST_BITS),64)
    ML?=ml64
endif

RC?=rc
PERL?=perl
LINK?=link
ifeq (default,$(origin CC))
    CC=cl
endif
ifeq (default,$(origin CXX))
    CXX=cl
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
    CRTAPI1=_cdecl \
    CRTAPI2=_cdecl \
    _WIN32_WINDOWS=0x601 \
    _WIN32_WINNT=0x0601 \
    WINVER=0x601\
    _WIN32 \
    WIN32 \
    _CRT_SECURE_NO_WARNINGS \
    SUPPORTS_THREAD_LOCAL


CX_FLAGS+=\
    -nologo \
    -c \
    -W3 \
    -wd4099 \
    -wd4355 \
    -wd4345 \
    -wd4309 \
    -wd4307 \
    -wd4102 \
    -wd4068 \
    -wd4065 \
    -wd4800 \
    -wd4390 \
    -wd4101 \
    -wd4267 \
    -wd4244 \
    -wd4018 \
    -we4700 \
    -MD

CXX_FLAGS+=\
    -EHsc \
    -wd4291

CX_DEFINES_DEBUG+=DEBUG
CX_FLAGS_DEBUG+=-Zi

CX_FLAGS_PROD+=-Ox -Zi

ifeq ($(HOST_BITS),32)
    CX_FLAGS+=-arch:SSE2
endif

ifeq ($(HOST_BITS),64)
    CX_DEFINES+=\
        _WINSOCKAPI_ \
        J9HAMMER
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
# Setup NASM
#

NASM_CMD?=nasm

NASM_DEFINES=\
    TR_HOST_X86 \
    TR_TARGET_X86 \
    WINDOWS

NASM_INCLUDES=\
    ../oti \
    ../compiler \
    ../compiler/x/runtime

ifeq ($(HOST_BITS),32)
    NASM_OBJ_FORMAT=-fwin32

    NASM_DEFINES+=\
        TR_HOST_32BIT \
        TR_TARGET_32BIT

    NASM_INCLUDES+=\
        ../compiler/x/i386/runtime
else
    NASM_OBJ_FORMAT=-fwin64

    NASM_DEFINES+=\
        TR_HOST_64BIT \
        TR_TARGET_64BIT

    NASM_INCLUDES+=\
        ../compiler/x/amd64/runtime
endif

#
# Setup RC
#
RC_CMD?=$(RC)

RC_INCLUDES=$(PRODUCT_INCLUDES)

ifeq ($(BUILD_CONFIG),debug)
    RC_DEFINES+=$(RC_DEFINES_DEBUG)
    RC_FLAGS+=$(RC_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    RC_DEFINES+=$(RC_DEFINES_PROD)
    RC_FLAGS+=$(RC_FLAGS_PROD)
endif

RC_DEFINES+=$(RC_DEFINES_EXTRA)
RC_FLAGS+=$(RC_FLAGS_EXTRA)

#
# Finally setup the linker
#
SOLINK_CMD?=$(LINK)

SOLINK_FLAGS+=-nologo -nodefaultlib -incremental:no -debug
SOLINK_LIBPATH+=$(PRODUCT_LIBPATH)
SOLINK_SLINK+=$(PRODUCT_SLINK) j9thr j9hookable kernel32 oldnames msvcrt msvcprt ws2_32

ifeq ($(origin MSVC_VERSION), undefined)
    ifneq (,$(filter 14.0 15.0, $(VisualStudioVersion)))
        SOLINK_SLINK+=ucrt vcruntime
    endif
else
    ifneq (,$(filter 2015 2017, $(MSVC_VERSION)))
        SOLINK_SLINK+=ucrt vcruntime
    endif
endif

SOLINK_DEF?=$(JIT_SCRIPT_DIR)/j9jit.def
SOLINK_ORG?=0x12900000

ifeq ($(HOST_BITS),32)
    SOLINK_FLAGS+=-machine:i386 -safeseh
endif

ifeq ($(HOST_BITS),64)
    SOLINK_FLAGS+=-machine:AMD64
endif

ifeq ($(BUILD_CONFIG),debug)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_DEBUG)
endif

ifeq ($(BUILD_CONFIG),prod)
    SOLINK_FLAGS+=$(SOLINK_FLAGS_PROD)
endif

SOLINK_FLAGS+=$(SOLINK_FLAGS_EXTRA)
