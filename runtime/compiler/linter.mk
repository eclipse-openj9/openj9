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

# To lint: 
#
# PLATFORM=foo-bar-clang make -f linter.mk 
#

.PHONY: linter
linter::

# Handy macro to check to make sure variables are set
REQUIRE_VARS=$(foreach VAR,$(1),$(if $($(VAR)),,$(error $(VAR) must be set)))
$(call REQUIRE_VARS,J9SRC)


ifeq ($(PLATFORM),ppc64-linux64-clangLinter)
    export LLVM_CONFIG?=/tr/llvm_checker/ppc-64/sles11/bin/llvm-config
    export CC_PATH?=/tr/llvm_checker/ppc-64/sles11/bin/clang
    export CXX_PATH?=/tr/llvm_checker/ppc-64/sles11/bin/clang++
else 
    #default paths, unless overridden 
    export LLVM_CONFIG?=llvm-config
    export CC_PATH?=clang
    export CXX_PATH?=clang++
endif

#
# First setup some important paths
# Personally, I feel it's best to default to out-of-tree build but who knows, there may be
# differing opinions on that.
#
JIT_SRCBASE?=..
JIT_OBJBASE?=$(J9SRC)/objs/compiler_$(BUILD_CONFIG)
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

# Add include directories to find j9cfg.h and omrcfg.h if needed.
#
# If the linter is being run after a CMake build, it
# results in j9cfg.h and omrcfg.h to be in a location that
# is different from an automake build.
#
ifneq ("$(CMAKE_BUILD_DIR)","")
CXX_INCLUDES+=$(CMAKE_BUILD_DIR)/runtime $(CMAKE_BUILD_DIR)/runtime/omr $(CMAKE_BUILD_DIR)/runtime/nls $(CMAKE_BUILD_DIR)/runtime/include
endif

#
# Add OMRChecker targets
#
# This likely ought to be using the OMRChecker fragment that 
# exists in that repo, but in the mean time, this is fine. 
# 

OMRCHECKER_DIR?=$(JIT_SRCBASE)/omr/tools/compiler/OMRChecker

OMRCHECKER_OBJECT=$(OMRCHECKER_DIR)/OMRChecker.so

OMRCHECKER_LDFLAGS=`$(LLVM_CONFIG) --ldflags`
OMRCHECKER_CXXFLAGS=`$(LLVM_CONFIG) --cxxflags`

$(OMRCHECKER_OBJECT):
	cd $(OMRCHECKER_DIR); sh smartmake.sh

.PHONY: omrchecker omrchecker_test omrchecker_clean omrchecker_cleandll omrchecker_cleanall
omrchecker: $(OMRCHECKER_OBJECT)

omrchecker_test: $(OMRCHECKER_OBJECT)
	cd $(OMRCHECKER_DIR); make test

omrchecker_clean:
	cd $(OMRCHECKER_DIR); make clean

omrchecker_cleandll:
	cd $(OMRCHECKER_DIR); make cleandll

omrchecker_cleanall:
	cd $(OMRCHECKER_DIR); make cleanall


# The core linter bits.  
# 
# linter:: is the default target, and we construct a pre-req for each
#  .cpp file.
#
linter:: omrchecker 


# The clang invocation magic line.
LINTER_EXTRA=-Xclang -load -Xclang $(OMRCHECKER_OBJECT) -Xclang -add-plugin -Xclang omr-checker 
LINTER_FLAGS=-std=c++0x -w -fsyntax-only -ferror-limit=0 $(LINTER_FLAGS_EXTRA)

define DEF_RULE.linter
.PHONY: $(1).linted 

$(1).linted: $(1) omrchecker
	$$(CXX_CMD) $(LINTER_FLAGS) $(LINTER_EXTRA)  $$(patsubst %,-D%,$$(CXX_DEFINES)) $$(patsubst %,-I'%',$$(CXX_INCLUDES)) -o $$@ -c $$<

linter:: $(1).linted 

endef # DEF_RULE.linter

RULE.linter=$(eval $(DEF_RULE.linter))

# The list of sources. 
JIT_CPP_FILES=$(filter %.cpp,$(JIT_PRODUCT_SOURCE_FILES) $(JIT_PRODUCT_BACKEND_SOURCES))

# Construct lint dependencies. 
$(foreach SRCFILE,$(JIT_CPP_FILES),\
   $(call RULE.linter,$(FIXED_SRCBASE)/$(SRCFILE)))

