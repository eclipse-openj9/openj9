###############################################################################
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
###############################################################################

TOP_DIR := ..

include $(TOP_DIR)/makelib/mkconstants.mk
include $(TOP_DIR)/makelib/uma_macros.mk

# recursively find files in directory $1 matching the pattern $2
FindAllFiles = \
	$(wildcard $1/$2) \
	$(foreach i,$(wildcard $1/*),$(call FindAllFiles,$i,$2))

DDR_INPUT_MODULES := j9ddr_misc j9gc j9jvmti j9prt j9shr j9thr j9trc j9vm jclse
DDR_INPUT_DEPENDS := $(addprefix $(TOP_DIR)/,$(foreach module,$(DDR_INPUT_MODULES),$($(module)_depend)))

<#if uma.spec.type.windows>
DDR_INPUT_FILES := $(addprefix $(TOP_DIR)/,$(foreach module,$(DDR_INPUT_MODULES),$($(module)_pdb)))
<#elseif uma.spec.type.zos>

# Exclude debug information that is only relevant to tools or test code.
DDR_EXCLUDED_FOLDERS := $(addsuffix /%, $(addprefix $(TOP_DIR)/, \
	bcutil/test \
	gc_tests \
	jilgen \
	omr/ddr \
	omr/third_party \
	omr/tools \
	runtimetools \
	tests \
	))

DDR_INPUT_FILES := $(sort $(filter-out $(DDR_EXCLUDED_FOLDERS), $(call FindAllFiles,$(TOP_DIR),*.dbg)))

<#elseif uma.spec.flags.uma_gnuDebugSymbols.enabled>
DDR_INPUT_FILES := $(addsuffix .dbg,$(DDR_INPUT_DEPENDS))
<#if uma.spec.type.osx>
# workaround for OSX not keeping anonymous enum symbols in shared library
# so get it directly from object file instead
DDR_INPUT_FILES += $(TOP_DIR)/omr/gc/base/standard/CompactScheme$(UMA_DOT_O)
</#if>
</#if>

# help ddrgen find required libraries
<#if uma.spec.type.aix || uma.spec.type.zos>
DDR_LIB_PATH := LIBPATH=$(TOP_DIR)$(if $(LIBPATH),:$(LIBPATH))
<#else>
DDR_LIB_PATH :=
</#if>

# The primary goals of this makefile.
DDR_BLOB := $(TOP_DIR)/j9ddr.dat
DDR_SUPERSET_FILE := $(TOP_DIR)/superset.dat

# Intermediate artifacts produced by this makefile.
DDR_MACRO_LIST := $(TOP_DIR)/macroList

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

$(DDR_BLOB) : $(TOP_DIR)/ddrgen$(UMA_DOT_EXE) $(DDR_MACRO_LIST) blacklist $(wildcard overrides*)
	@echo "Running ddrgen to generate $(notdir $@) and $(notdir $(DDR_SUPERSET_FILE))"
	@$(DDR_LIB_PATH) $(TOP_DIR)/ddrgen $(DDR_OPTIONS) \
		$(DDR_INPUT_FILES)
<#if uma.spec.type.zos>
	chtag -t -c ISO8859-1 $(DDR_SUPERSET_FILE)
</#if>

$(DDR_MACRO_LIST) : $(DDR_INPUT_DEPENDS)
	@echo Running getmacros for constant discovery
	@rm -f $@
	bash $(TOP_DIR)/omr/ddr/tools/getmacros $(TOP_DIR)
<#if uma.spec.type.zos>
	iconv -f ISO8859-1 -t IBM-1047 $@ > $@.tmp
	mv -f $@.tmp $@
	chtag -t -c IBM-1047 $@
</#if>
