##############################################################################
#  Copyright (c) 2016, 2017 IBM Corp. and others
#
#  This program and the accompanying materials are made available under
#  the terms of the Eclipse Public License 2.0 which accompanies this
#  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
#  or the Apache License, Version 2.0 which accompanies this distribution and
#  is available at https://www.apache.org/licenses/LICENSE-2.0.
#
#  This Source Code may also be made available under the following
#  Secondary Licenses when the conditions for such availability set
#  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
#  General Public License, version 2 with the GNU Classpath
#  Exception [1] and GNU General Public License, version 2 with the
#  OpenJDK Assembly Exception [2].
#
#  [1] https://www.gnu.org/software/classpath/license.html
#  [2] http://openjdk.java.net/legal/assembly-exception.html
#
#  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
##############################################################################

#
# Makefile to run OpenJ9 tests
#
################################################################################
#
# usage: 
#	make [JAVA_BIN=/path/to/java/bin] [SPEC=linux_x86-64] [JTREG_DIR=/path/to/jtreg/harness] [all | sanity | extended | external | perf | openjdk]
#
# sanity tests only: 
#	make sanity [JAVA_BIN=/path/to/java/bin] [SPEC=linux_x86-64]
# extended tests only: 
#	make extended [JAVA_BIN=/path/to/java/bin] [SPEC=linux_x86-64]
#external tests only:
#	make external [JAVA_BIN=/path/to/java/bin] [SPEC=linux_x86-64]
#perf tests only:
#	make perf [JAVA_BIN=/path/to/java/bin] [SPEC=linux_x86-64]
# openjdk jtreg tests only:
#	make openjdk [JAVA_BIN=/path/to/java/bin] [SPEC=linux_x86-64] [JTREG_DIR=/path/to/jtreg/harness]
#################################################################################
## Configure J9 tests

MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))
TOP_DIR := $(dir $(strip $(patsubst %/, %, $(dir $(MAKEFILE_PATH)))))
SPEC_FILE := $(subst //,/,$(wildcard $(TOP_DIR)/*/*/spec.gmk))

ifdef SPEC_FILE
	include $(SPEC_FILE)

	JAVA_BIN ?= $(IMAGES_OUTPUTDIR)/sdk/bin
	J9_SPEC ?= $(OPENJDK_BUILD_OS)_$(OPENJDK_BUILD_CPU_ARCH)-$(OPENJDK_BUILD_CPU_BITS)_cmprssptrs
endif

JTREG_DIR ?= $(subst //,/,$(TOP_DIR)/jtreg)
JAVA_VERSION ?= SE90

ADD_CMD_ARGS := JAVA_BIN=$(JAVA_BIN) SPEC=$(J9_SPEC) JAVA_VERSION=$(JAVA_VERSION)

OPENJ9TEST_SRC_DIR := $(subst //,/,$(TOP_DIR)/test)
OPENJ9BINARIES_SRC_DIR := $(subst //,/,$(TOP_DIR)/binaries)

AUTOGEN_FILES := $(filter-out $(OPENJ9TEST_SRC_DIR)/TestConfig/run_configure.mk, $(wildcard $(OPENJ9TEST_SRC_DIR)/TestConfig/*.mk))
AUTOGEN_FILES += $(wildcard $(OPENJ9TEST_SRC_DIR)/*/autoGenTest.mk)
AUTOGEN_FILES := $(strip $(AUTOGEN_FILES))

# function to check if a variable is set or not
# requires two parameters:
#	1. variable name
# 	2. error message
#
define check_variable_defined
	$(if $(strip $(value $1)),,$(error Error: $1 is not set! $2))
endef


.PHONY: check_env check_jtreg stage configure sanity extended openjdk clean all
.NOTPARALLEL: sanity extended openjdk

default: test

test: sanity extended openjdk
	@echo All tests completed!

all: clean test

check_env:
	$(call check_variable_defined,JAVA_BIN,Please provide a JAVA_BIN value (path to the bin directory of an OpenJ9 java installation)!) 
	$(call check_variable_defined,J9_SPEC,Please provide the J9_SPEC value for your system (e.g. linux_x86-64_cmprssptrs)!) 

configure:
ifeq ($(AUTOGEN_FILES),)
	(cd $(OPENJ9TEST_SRC_DIR)/TestConfig && \
		$(MAKE) -f run_configure.mk $(ADD_CMD_ARGS) )
endif

compile: configure
ifeq ($(wildcard $(TOP_DIR)/jvmtest),)
	(cd $(OPENJ9TEST_SRC_DIR)/TestConfig && \
		$(MAKE) $(ADD_CMD_ARGS) compile )
endif

test_%: compile
	@echo Running $(patsubst test_%,%,$@) tests
	(cd $(OPENJ9TEST_SRC_DIR)/TestConfig && \
		$(MAKE) $(ADD_CMD_ARGS) $(patsubst test_%,_%,$@))

check_jtreg:
	$(call check_variable_defined,JTREG_DIR,Please provide a JTREG_DIR value (path to OpenJDK jtreg harness)!)

##############################################################################
# NB: for now JTREG_DIR must be provided. 
# In the future, it will be built from source.
# TODO: [Releng Team] Add clone jtreg to get_source.sh
# TODO: [FV Team] build jtreg from source with IBM patches
##############################################################################
jtreg: check_jtreg
	$(info JTREG_DIR is $(JTREG_DIR)) 

sanity: test_sanity

extended: test_extended

external: test_external

perf: test_perf

openjdk : override ADD_CMD_ARGS := $(ADD_CMD_ARGS) JTREG_DIR=$(JTREG_DIR) JTREG_TEST_DIR=$(subst //,/,$(TOP_DIR)/jdk/test)
openjdk: jtreg test_openjdk

clean_configure:
	(cd $(OPENJ9TEST_SRC_DIR)/TestConfig && \
		$(MAKE) -f run_configure.mk clean)

clean: check_env
	(cd $(OPENJ9TEST_SRC_DIR)/TestConfig && \
		$(MAKE) $(ADD_CMD_ARGS) clean)

