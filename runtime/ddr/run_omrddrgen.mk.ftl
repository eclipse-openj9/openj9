###############################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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
###############################################################################

TOP_DIR := ../

include $(TOP_DIR)makelib/mkconstants.mk
include $(TOP_DIR)makelib/uma_macros.mk

ifeq (8,$(VERSION_MAJOR))
  DDR_JCL_MODULE := jclse7b_
else ifneq (,$(filter 9 10 11,$(VERSION_MAJOR)))
  DDR_JCL_MODULE := jclse$(VERSION_MAJOR)_
else
  $(error Unsupported version: '$(VERSION_MAJOR)')
endif

DDR_INPUT_MODULES := j9ddr_misc j9gc j9jvmti j9prt j9shr j9thr j9trc j9vm $(DDR_JCL_MODULE)
DDR_INPUT_DEPENDS := $(addprefix $(TOP_DIR),$(foreach module,$(DDR_INPUT_MODULES),$($(module)_depend)))

<#if uma.spec.type.windows>
DDR_INPUT_FILES := $(addprefix $(TOP_DIR),$(foreach module,$(DDR_INPUT_MODULES),$($(module)_pdb)))
<#elseif uma.spec.flags.uma_gnuDebugSymbols.enabled>
DDR_INPUT_FILES := $(addsuffix .dbg,$(DDR_INPUT_DEPENDS))
</#if>

# The primary goals of this makefile.
DDR_BLOB := $(TOP_DIR)j9ddr.dat
DDR_SUPERSET_FILE := $(TOP_DIR)superset.dat

# Intermediate artifacts produced by this makefile.
DDR_MACRO_LIST := $(TOP_DIR)macroList

# Command-line options for ddrgen.
DDR_OPTIONS := \
	--blob $(DDR_BLOB) \
	--superset $(DDR_SUPERSET_FILE) \
	--blacklist blacklist \
	--macrolist $(DDR_MACRO_LIST) \
	--overrides overrides \
	--show-empty \
	#

# All artifacts produced by this makefile.
DDR_PRODUCTS := \
	$(DDR_BLOB) \
	$(DDR_MACRO_LIST) \
	$(DDR_SUPERSET_FILE) \
	#

.PHONY : all clean

all : $(DDR_BLOB)

clean :
	rm -f $(DDR_PRODUCTS)

$(DDR_BLOB) : $(TOP_DIR)ddrgen $(DDR_MACRO_LIST)
	$(TOP_DIR)ddrgen $(DDR_OPTIONS) \
		$(DDR_INPUT_FILES)

$(DDR_MACRO_LIST) : $(DDR_INPUT_DEPENDS)
	@echo Running getmacros for constant discovery
	@rm -f $@
	bash $(TOP_DIR)omr/ddr/tools/getmacros $(TOP_DIR)
