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

J9_VERSION?=29
J9LIBS = j9jit_vm j9codert_vm j9util j9utilcore j9pool j9avl j9stackmap j9hashtable

OMR_DIR ?= $(J9SRC)/omr
-include $(OMR_DIR)/omrmakefiles/jitinclude.mk

PRODUCT_INCLUDES=\
    $(OMR_INCLUDES_FOR_JIT) \
    $(FIXED_SRCBASE)/compiler/$(TARGET_ARCH)/$(TARGET_SUBARCH) \
    $(FIXED_SRCBASE)/compiler/$(TARGET_ARCH) \
    $(FIXED_SRCBASE)/compiler \
    $(FIXED_SRCBASE)/omr/compiler/$(TARGET_ARCH)/$(TARGET_SUBARCH) \
    $(FIXED_SRCBASE)/omr/compiler/$(TARGET_ARCH) \
    $(FIXED_SRCBASE)/omr/compiler \
    $(FIXED_SRCBASE)/omr \
    $(FIXED_SRCBASE) \
    $(J9SRC)/codert_vm \
    $(J9SRC)/gc_include \
    $(J9SRC)/include \
    $(J9SRC)/gc_glue_java \
    $(J9SRC)/jit_vm \
    $(J9SRC)/nls \
    $(J9SRC)/oti \
    $(J9SRC)/util

PRODUCT_DEFINES+=\
    BITVECTOR_BIT_NUMBERING_MSB \
    UT_DIRECT_TRACE_REGISTRATION \
    J9_PROJECT_SPECIFIC

ifdef ASSUMES
    PRODUCT_DEFINES+=PROD_WITH_ASSUMES
endif

ifdef PUBLIC_BUILD
    PRODUCT_DEFINES+=PUBLIC_BUILD
endif

ifdef ENABLE_GPU
    ifeq (,$(CUDA_HOME))
        $(error You must set CUDA_HOME if ENABLE_GPU is set)
    endif

    ifeq (,$(GDK_HOME))
        $(error You must set GDK_HOME if ENABLE_GPU is set)
    endif

    PRODUCT_INCLUDES+=$(CUDA_HOME)/include $(CUDA_HOME)/nvvm/include $(GDK_HOME)
    PRODUCT_DEFINES+=ENABLE_GPU
endif

PRODUCT_RELEASE?=tr.open.java

PRODUCT_NAME?=j9jit$(J9_VERSION)

PRODUCT_LIBPATH=$(J9SRC) $(J9SRC)/lib
PRODUCT_SLINK=$(J9LIBS) $(J9LIBS)

# Optional project-specific settings
-include $(JIT_MAKE_DIR)/toolcfg/common-extra.mk

#
# Now we include the host and target tool config
# These don't really do much generally... They set a few defines but there really
# isn't a lot of stuff that's host/target dependent that isn't also dependent
# on what tools you're using
#
include $(JIT_MAKE_DIR)/toolcfg/host/$(HOST_ARCH).mk
include $(JIT_MAKE_DIR)/toolcfg/host/$(HOST_BITS).mk
include $(JIT_MAKE_DIR)/toolcfg/host/$(OS).mk
include $(JIT_MAKE_DIR)/toolcfg/target/$(TARGET_ARCH).mk
include $(JIT_MAKE_DIR)/toolcfg/target/$(TARGET_BITS).mk

# The script used to generate TRBuildName.cpp.
GENERATE_VERSION_SCRIPT ?= $(OMR_DIR)/tools/compiler/scripts/generateVersion.pl

#
# Now this is the big tool config file. This is where all the includes and defines
# get turned into flags, and where all the flags get setup for the different
# tools and file types
#
include $(JIT_MAKE_DIR)/toolcfg/$(TOOLCHAIN)/common.mk
