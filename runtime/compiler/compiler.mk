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

.PHONY: all clean cleanobjs cleandeps cleandll
all:
clean:
cleanobjs:
cleandeps:
cleandll:

# This is the logic right now for locating Clang and LLVM-config
# There's probably a nicer way to do all of this... it's pretty bad

#
# Detect the VM's build SPEC for compiling ARM.  This should eventually
# be done cleaner.
#
ifeq ($(SPEC),linux_arm)
    PLATFORM=arm-linux-gcc-cross
    $(warning ARM SPEC detected)
endif

ifeq ($(PLATFORM),amd64-linux64-clang)
    # Luckily we can just use the default path for Clang :)
endif

ifeq ($(PLATFORM),ppc64-linux64-clang)
    ifeq (default,$(origin CC))
        export CC=/tr/llvm_checker/ppc-64/sles11/bin/clang
    endif
    ifeq (default,$(origin CXX))
        export CXX=/tr/llvm_checker/ppc-64/sles11/bin/clang++
    endif
endif

ifeq ($(PLATFORM),s390-zos64-vacpp)
    ifeq (default,$(origin CC))
        export CC=/usr/lpp/cbclib/xlc/bin/xlc
    endif
    ifeq (default,$(origin CXX))
        export CXX=/usr/lpp/cbclib/xlc/bin/xlC
    endif
    export A2E_INCLUDE_PATH?=/usr/lpp/cbclib/include
endif

ifeq ($(PLATFORM),arm-linux-gcc-cross)
    OPENJ9_CC_PREFIX ?= arm-bcm2708hardfp-linux-gnueabi
    ifeq (default,$(origin CC))
        export CC=$(OPENJ9_CC_PREFIX)-gcc
    endif
    ifeq (default,$(origin CXX))
        export CXX=$(OPENJ9_CC_PREFIX)-g++
    endif
    ifeq (default,$(origin AS))
        export AS=$(OPENJ9_CC_PREFIX)-as
    endif
    PLATFORM=arm-linux-gcc
endif

ifeq ($(PLATFORM),aarch64-linux-gcc)
    ifeq (default,$(origin CC))
        export CC=$(OPENJ9_CC_PREFIX)-gcc
    endif
    ifeq (default,$(origin CXX))
        export CXX=$(OPENJ9_CC_PREFIX)-g++
    endif
    ifeq (default,$(origin AS))
        export AS=$(OPENJ9_CC_PREFIX)-as
    endif
endif

ifneq ($(findstring DPROD_WITH_ASSUMES, $(USERCFLAGS)),)
    ASSUMES=1
endif

#
# "all" should be the first target to appear so it's the default
#
.PHONY: all clean cleanobjs cleandeps cleandll
all: ; @echo SUCCESS - All files are up-to-date
clean: ; @echo SUCCESS - All files are cleaned
cleanobjs: ; @echo SUCCESS - All objects are cleaned
cleandeps: ; @echo SUCCESS - All dependencies are cleaned
cleandll: ; @echo SUCCESS - All shared libraries are cleaned

# Handy macro to check to make sure variables are set
REQUIRE_VARS=$(foreach VAR,$(1),$(if $($(VAR)),,$(error $(VAR) must be set)))

# Verify SDK pointer for non-cleaning targets
ifeq (,$(filter clean cleandeps cleandll,$(MAKECMDGOALS)))
    $(call REQUIRE_VARS,J9SRC)
endif

#
# First setup some important paths
# Personally, I feel it's best to default to out-of-tree build but who knows, there may be
# differing opinions on that.
#
JIT_SRCBASE?=..
JIT_OBJBASE?=../objs/compiler_$(BUILD_CONFIG)
JIT_DLL_DIR?=$(JIT_OBJBASE)

#
# Windows users will likely use backslashes, but Make tends to not like that so much
#
FIXED_SRCBASE=$(subst \,/,$(JIT_SRCBASE))
FIXED_OBJBASE=$(subst \,/,$(JIT_OBJBASE))
FIXED_DLL_DIR=$(subst \,/,$(JIT_DLL_DIR))

# TODO - "debug" as default?
BUILD_CONFIG?=prod

#
# Dirs used internally by the makefiles
#
JIT_MAKE_DIR?=$(FIXED_SRCBASE)/compiler/build
JIT_SCRIPT_DIR?=$(JIT_MAKE_DIR)/scripts

#
# First we set a bunch of tokens about the platform that the rest of the
# makefile will use as conditionals
#
include $(JIT_MAKE_DIR)/platform/common.mk

#
# Now we include the names of all the files that will go into building the JIT
# Will automatically include files needed from HOST and TARGET platform
#
include $(JIT_MAKE_DIR)/files/common.mk

#
# Now we configure all the tooling we will use to build the files
#
# There is quite a bit of shared stuff, but the overwhelming majority of this
# is toolchain-dependent.
#
# That makes sense - You can't expect XLC and GCC to take the same arguments
#
include $(JIT_MAKE_DIR)/toolcfg/common.mk

#
# Here's where everything has been setup and we lay down the actual targets and
# recipes that Make will use to build them
#
include $(JIT_MAKE_DIR)/rules/common.mk
