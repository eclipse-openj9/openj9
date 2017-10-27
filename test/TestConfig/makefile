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
# Makefile to run various JVM tests
#

$(info Running make $(MAKE_VERSION))

.PHONY: test clean compile getdependency help runtest rmResultFile resultsSummary

.DEFAULT_GOAL := test

help:
	@echo "This makefile is used to build and execute JVM tests. You should specify the following"
	@echo "variables before using this script:"
	@echo "JAVA_BIN  Path to java bin dir which will be used to built and executed JVM tests"
	@echo "SPEC      Should match the current platform (e.g. SPEC=linux_x86-64, SPEC=linux_x86, SPEC=linux_x86-64)"

CD        = cd
ECHO      = echo
MKDIR     = mkdir
PWD       = pwd
EXECUTABLE_SUFFIX =
RUN_SCRIPT = sh
SCRIPT_SUFFIX=.sh
Q="
D=/
P=:
PROPS_DIR=props_unix

ifndef JAVA_BIN
$(error Please provide JAVA_BIN value.)
else
export JAVA_BIN:=$(JAVA_BIN)
$(info set JAVA_BIN to $(JAVA_BIN))
endif

ifndef SPEC
$(error Please provide SPEC that matches the current platform (e.g. SPEC=linux_x86-64))
else
export SPEC:=$(SPEC)
$(info set SPEC to $(SPEC))
endif

ifndef JAVA_VERSION
export JAVA_VERSION:=SE90
else
export JAVA_VERSION:=$(JAVA_VERSION)
endif
$(info set JAVA_VERSION to $(JAVA_VERSION))



ifneq (,$(findstring win,$(SPEC)))
P=;
EXECUTABLE_SUFFIX=.exe
RUN_SCRIPT="cmd /c" 
SCRIPT_SUFFIX=.bat
D=\\
PROPS_DIR=props_win
endif

# Environment variable OSTYPE is set to cygwin if running under cygwin.
ifndef CYGWIN
	OSTYPE?=$(shell echo $$OSTYPE)
	ifeq ($(OSTYPE),cygwin)
		CYGWIN:=1
	else
		CYGWIN:=0
	endif
endif

ifndef SRC_ROOT
	SRC_ROOT := $(shell $(PWD))$(D)..
	ifeq ($(CYGWIN),1)
		SRC_ROOT := $(shell cygpath -w $(SRC_ROOT))
	endif
	SRC_ROOT := $(subst \,/,$(SRC_ROOT))
endif
$(info set SRC_ROOT to $(SRC_ROOT))

ifndef BUILD_ROOT
BUILD_ROOT := $(SRC_ROOT)$(D)..$(D)jvmtest
BUILD_ROOT := $(subst \,/,$(BUILD_ROOT))
endif
$(info set BUILD_ROOT to $(BUILD_ROOT))

# set default values
JVM_TEST_ROOT := $(SRC_ROOT)
TEST_GROUP=level.*

# removing " 
JAVA_BIN_TMP := $(subst ",,$(JAVA_BIN))
JDK_HOME := $(JAVA_BIN_TMP)$(D)..
ifeq ($(JAVA_VERSION),SE80)
JDK_HOME := $(JAVA_BIN_TMP)$(D)..$(D)..
endif

ifdef JTREG_DIR
# removing "
JTREG_DIR := $(subst ",,$(JTREG_DIR))
endif

ifdef JTREG_TEST_DIR
# removing "
JTREG_TEST_DIR := $(subst ",,$(JTREG_TEST_DIR))
endif

#######################################
# Set OS, ARCH and BITS based on SPEC
#######################################
WORD_LIST:= $(subst _, ,$(SPEC))
OS:=os.$(word 1,$(WORD_LIST))
ARCH_INFO:=$(word 2,$(WORD_LIST))

BITS=bits.32
ARCH=arch.$(ARCH_INFO)
ifneq (,$(findstring -64,$(ARCH_INFO)))
	BITS=bits.64
	ARCH:=arch.$(subst -64,,$(ARCH_INFO))
else
	ifneq (,$(findstring 390,$(ARCH_INFO)))
		BITS=bits.31
	endif
endif

ifneq ($(DEBUG),)
$(info OS is set to $(OS), ARCH is set to $(ARCH) and BITS is set to $(BITS))
endif

DEFAULT_EXCLUDE=d.*.$(SPEC),d.*.$(ARCH),d.*.$(OS),d.*.$(BITS),d.*.generic-all
ifneq ($(DEBUG),)
$(info DEFAULT_EXCLUDE is set to $(DEFAULT_EXCLUDE))
endif

JAVA_COMMAND:=$(Q)$(JAVA_BIN_TMP)$(D)java$(Q)

#######################################
# common dir and jars
#######################################
#download all dependent jars
getdependency:
	perl $(SRC_ROOT)$(D)TestConfig$(D)scripts$(D)tools$(D)getDependencies.pl -path '$(SRC_ROOT)$(D)TestConfig$(D)lib' -task default

LIB_DIR=$(JVM_TEST_ROOT)$(D)TestConfig$(D)lib
TESTNG=$(LIB_DIR)$(D)testng.jar$(P)$(LIB_DIR)$(D)jcommander.jar
RESOURCES_DIR=$(JVM_TEST_ROOT)$(D)TestConfig$(D)resources

#######################################
# cmdlinetester jars
#######################################
CMDLINETESTER_JAR   =$(Q)$(JVM_TEST_ROOT)$(D)cmdline_options_tester$(D)cmdlinetester.jar$(Q)
CMDLINETESTER_RESJAR=$(Q)$(JVM_TEST_ROOT)$(D)cmdline_options_testresources$(D)cmdlinetestresources.jar$(Q)

#######################################
# testng report dir
#######################################
TIMESTAMP := $(shell perl $(JVM_TEST_ROOT)$(D)TestConfig$(D)scripts$(D)testKitGen$(D)resultsSummary$(D)getTimeStamp.pl -v)
TESTOUTPUT = $(shell $(PWD))$(D)test_output_$(TIMESTAMP)
REPORTDIR = $(Q)$(TESTOUTPUT)$(D)$@$(Q)

#######################################
# TEST_STATUS
#######################################
TEST_STATUS=if [ $$? -eq 0 ] ; then $(ECHO) $(Q)$@$(Q)$(Q)_PASSED$(Q); else $(ECHO) $(Q)$@$(Q)$(Q)_FAILED$(Q); fi
ifneq ($(DEBUG),)
$(info TEST_STATUS is $(TEST_STATUS))
endif
TEST_SKIP_STATUS=@$(ECHO) $@_SKIPPED

#######################################
# include configure makefile
#######################################
ifndef MACHINE_MK
-include $(JVM_TEST_ROOT)$(D)TestConfig$(D)machineConfigure.mk
else
-include $(MACHINE_MK)$(D)machineConfigure.mk
endif
#######################################
# include sub makefile
#######################################
-include $(JVM_TEST_ROOT)$(D)TestConfig$(D)extraSettings
include $(JVM_TEST_ROOT)$(D)TestConfig$(D)specToPlat.mk
include $(JVM_TEST_ROOT)$(D)TestConfig$(D)jvmTest.mk
include $(JVM_TEST_ROOT)$(D)autoGenTest.mk

#######################################
# generic target
#######################################
_TESTTARGET = $(firstword $(MAKECMDGOALS))
TESTTARGET = $(patsubst _%,%,$(_TESTTARGET))
ifneq ($(DEBUG),)
$(info _TESTTARGET is $(_TESTTARGET))
$(info TESTTARGET is $(TESTTARGET))
endif

.PHONY: _$(TESTTARGET) $(TESTTARGET)
$(TESTTARGET): JVM_TEST_ROOT := $(BUILD_ROOT)
_$(TESTTARGET): setup_$(TESTTARGET) rmResultFile $(TESTTARGET) resultsSummary
	@$(ECHO) $@ done

setup_%:
	@$(ECHO)
	@$(ECHO) Running $(TESTTARGET)
	$(JAVA_COMMAND) -version

#######################################
# test_dir target
#######################################
TEST_DIRTARGET = $(firstword $(MAKECMDGOALS))
DIRTARGET = $(patsubst test_%,%,$(_TESTTARGET))
ifneq ($(DEBUG),)
$(info _TESTTARGET is $(_TESTTARGET))
$(info TESTTARGET is $(TESTTARGET))
endif

.PHONY: test_$(DIRTARGET)
$(DIRTARGET): JVM_TEST_ROOT := $(BUILD_ROOT)
test_$(DIRTARGET): setup_$(DIRTARGET) rmResultFile $(DIRTARGET)_$(JAVA_VERSION) resultsSummary

test: compile runtest
	@$(ECHO) "All Tests Completed"

ifndef JCL_VERSION
export JCL_VERSION:=latest
else
export JCL_VERSION:=$(JCL_VERSION)
endif
$(info set JAVA_VERSION to $(JAVA_VERSION))

# Define the EXCLUDE_FILE to be used for temporarily excluding failed tests.
# This macro is used in /test/Utils/src/org/openj9/test/util/IncludeExcludeTestAnnotationTransformer
ifndef EXCLUDE_FILE
	ifeq ($(JCL_VERSION),latest)
		export EXCLUDE_FILE:=$(JVM_TEST_ROOT)$(D)TestConfig$(D)resources$(D)excludes$(D)openj9_exclude.txt
	else
		export EXCLUDE_FILE:=$(JVM_TEST_ROOT)$(D)TestConfig$(D)resources$(D)excludes$(D)default_exclude.txt
	endif
endif

# compile all tests under $(SRC_ROOT)
compile: getdependency
	ant -f $(SRC_ROOT)$(D)TestConfig$(D)scripts$(D)build_test.xml -DSRC_ROOT=$(SRC_ROOT) -DBUILD_ROOT=$(BUILD_ROOT) -DJAVA_BIN=$(JAVA_BIN) -DJAVA_VERSION=$(JAVA_VERSION) -DJCL_VERSION=$(JCL_VERSION) -DBUILD_LIST=${BUILD_LIST} -DRESOURCES_DIR=${RESOURCES_DIR}

#######################################
# failed target
#######################################
FAILEDTAGETS = $(shell $(PWD))$(D)failedtargets.mk

-include $(FAILEDTAGETS)

#######################################
# result Summary
#######################################
TEMPRESULTFILE=TestTargetResult
TAPRESULTFILE=TestTargetResult.tap

rmResultFile:
	$(RM) $(Q)$(TEMPRESULTFILE)$(Q)

resultsSummary:
	@perl $(JVM_TEST_ROOT)$(D)TestConfig$(D)scripts$(D)testKitGen$(D)resultsSummary$(D)resultsSum.pl --failuremk=$(Q)$(FAILEDTAGETS)$(Q) --resultFile=$(Q)$(TEMPRESULTFILE)$(Q) --tapFile=$(Q)$(TAPRESULTFILE)$(Q) --diagnostic=failure

sanity: JVM_TEST_ROOT := $(BUILD_ROOT)
sanity: setup_sanity rmResultFile sanity_$(JAVA_VERSION) resultsSummary

extended: JVM_TEST_ROOT := $(BUILD_ROOT)
extended: setup_extended rmResultFile extended_$(JAVA_VERSION) resultsSummary

openjdk: JVM_TEST_ROOT := $(BUILD_ROOT)
openjdk: setup_openjdk rmResultFile openjdk_$(JAVA_VERSION) resultsSummary

external: JVM_TEST_ROOT := $(BUILD_ROOT)
external: setup_external rmResultFile external_$(JAVA_VERSION) resultsSummary

perf: JVM_TEST_ROOT := $(BUILD_ROOT)
perf: setup_sanity rmResultFile perf_$(JAVA_VERSION) resultsSummary

runtest: setup_perf rmResultFile sanity_$(JAVA_VERSION) extended_$(JAVA_VERSION) openjdk_$(JAVA_VERSION) external_$(JAVA_VERSION) perf_$(JAVA_VERSION) resultsSummary
.NOTPARALLEL: extended sanity openjdk external perf runtest rmResultFile resultsSummary

# cleanup
clean:
	$(RM) -r $(BUILD_ROOT)
	$(RM) -r $(SRC_ROOT)$(D)TestConfig$(D)test_output_*
	$(RM) $(TEMPRESULTFILE)
	$(RM) $(TAPRESULTFILE)
	$(RM) $(FAILEDTAGETS)