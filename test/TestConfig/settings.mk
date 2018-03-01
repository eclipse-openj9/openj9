##############################################################################
#  Copyright (c) 2016, 2018 IBM Corp. and others
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

.PHONY: help rmResultFile resultsSummary

help:
	@echo "This makefile is used to build and execute JVM tests. You should specify the following"
	@echo "variables before using this script:"
	@echo "JAVA_BIN  Path to java bin dir which will be used to built and executed JVM tests"
	@echo "SPEC      Should match the current platform (e.g. SPEC=linux_x86-64, SPEC=linux_x86)"

CD        = cd
ECHO      = echo
MKDIR     = mkdir
MKTREE    = mkdir -p
PWD       = pwd
EXECUTABLE_SUFFIX =
RUN_SCRIPT = sh
RUN_SCRIPT_STRING = "sh -c"
SCRIPT_SUFFIX=.sh
Q="
P=:
PROPS_DIR=props_unix

include $(TEST_ROOT)$(D)TestConfig$(D)utils.mk

ifndef JAVA_BIN
$(error Please provide JAVA_BIN value.)
else
export JAVA_BIN:=$(JAVA_BIN)
endif

ifndef SPEC
$(error Please provide SPEC that matches the current platform (e.g. SPEC=linux_x86-64))
else
export SPEC:=$(SPEC)
endif

ifndef JAVA_VERSION
export JAVA_VERSION:=SE90
else
export JAVA_VERSION:=$(JAVA_VERSION)
endif

ifndef JAVA_IMPL
export JAVA_IMPL:=openj9
endif

ifndef JVM_VERSION
ifeq ($(JAVA_VERSION), SE80)
	JDK_VERSION = openjdk8
else
	ifeq ($(JAVA_VERSION), SE90)
		JDK_VERSION = openjdk9
	else
		ifeq ($(JAVA_VERSION), SE100)
			JDK_VERSION = openjdk10
		endif
	endif
endif
ifneq (, $(findstring openj9, $(JAVA_IMPL)))
	JVM_VERSION = $(JDK_VERSION)-openj9
else 
	ifeq (, $(findstring sap, $(JAVA_IMPL)))
		JVM_VERSION = $(JDK_VERSION)-sap
	else 
		JVM_VERSION = $(JDK_VERSION)
	endif
endif
export JVM_VERSION:=$(JVM_VERSION)
endif

ifneq (,$(findstring win,$(SPEC)))
P=;
D=\\
EXECUTABLE_SUFFIX=.exe
RUN_SCRIPT="cmd /c" 
RUN_SCRIPT_STRING=$(RUN_SCRIPT)
SCRIPT_SUFFIX=.bat
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

ifeq ($(CYGWIN),1)
	TEST_ROOT := $(shell cygpath -w $(TEST_ROOT))
endif
TEST_ROOT := $(subst \,/,$(TEST_ROOT))

ifndef BUILD_ROOT
BUILD_ROOT := $(TEST_ROOT)$(D)..$(D)jvmtest
BUILD_ROOT := $(subst \,/,$(BUILD_ROOT))
endif

JVM_TEST_ROOT = $(BUILD_ROOT)
TEST_GROUP=level.*

# removing " 
JAVA_BIN_TMP := $(subst ",,$(JAVA_BIN))
JDK_HOME := $(JAVA_BIN_TMP)$(D)..
ifeq ($(JAVA_VERSION),SE80)
JDK_HOME := $(JAVA_BIN_TMP)$(D)..$(D)..
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
ifndef UNIQUEID
	GETID := $(TEST_ROOT)$(D)TestConfig$(D)scripts$(D)testKitGen$(D)resultsSummary$(D)getUniqueId.pl
	export UNIQUEID := $(shell perl $(GETID) -v)
endif
TESTOUTPUT := $(TEST_ROOT)$(D)TestConfig$(D)test_output_$(UNIQUEID)
REPORTDIR = $(Q)$(TESTOUTPUT)$(D)$@$(Q)

#######################################
# TEST_STATUS
#######################################
TEST_STATUS=if [ $$? -eq 0 ] ; then $(ECHO) $(Q)$@$(Q)$(Q)_PASSED$(Q); else $(ECHO) -e $(Q)\n$@$(Q)$(Q)_FAILED\n$(Q); fi
ifneq ($(DEBUG),)
$(info TEST_STATUS is $(TEST_STATUS))
endif
TEST_SKIP_STATUS=$@_SKIPPED

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
-include $(JVM_TEST_ROOT)$(D)TestConfig$(D)extraSettings.mk

#######################################
# generic target
#######################################
_TESTTARGET = $(firstword $(MAKECMDGOALS))
TESTTARGET = $(patsubst _%,%,$(_TESTTARGET))

SUBDIRS_TESTTARGET = $(SUBDIRS:%=$(TESTTARGET)-%)

$(SUBDIRS_TESTTARGET):
	@if [ -d $(@:$(TESTTARGET)-%=%) ]; then \
		$(MAKE) -C $(@:$(TESTTARGET)-%=%) -f autoGen.mk $(TESTTARGET); \
	fi

$(TESTTARGET): $(SUBDIRS_TESTTARGET)

_$(TESTTARGET): setup_$(TESTTARGET) rmResultFile $(TESTTARGET) resultsSummary
	@$(ECHO) $@ done

.PHONY: _$(TESTTARGET) $(TESTTARGET) $(SUBDIRS) $(SUBDIRS_TESTTARGET)

.NOTPARALLEL: _$(TESTTARGET) $(TESTTARGET) $(SUBDIRS) $(SUBDIRS_TESTTARGET)

TOTALCOUNT := 0

setup_%:
	@$(ECHO)
	@$(ECHO) Running make $(MAKE_VERSION)
	@$(ECHO) set TEST_ROOT to $(TEST_ROOT)
	@$(ECHO) set JAVA_VERSION to $(JAVA_VERSION)
	@$(ECHO) set JAVA_IMPL to $(JAVA_IMPL)
	@$(ECHO) set JVM_VERSION to $(JVM_VERSION)
	@$(ECHO) set JCL_VERSION to $(JCL_VERSION)
	@$(ECHO) set JAVA_BIN to $(JAVA_BIN)
	@$(ECHO) set SPEC to $(SPEC)
	@$(ECHO) Running $(TESTTARGET) ...
	@if [ $(TOTALCOUNT) -ne 0 ]; then \
		$(ECHO) There are $(TOTALCOUNT) test targets in $(TESTTARGET).; \
	fi
	$(JAVA_COMMAND) -version

ifndef JCL_VERSION
export JCL_VERSION:=latest
else
export JCL_VERSION:=$(JCL_VERSION)
endif

# Define the EXCLUDE_FILE to be used for temporarily excluding failed tests.
# This macro is used in /test/Utils/src/org/openj9/test/util/IncludeExcludeTestAnnotationTransformer
ifndef EXCLUDE_FILE
	export EXCLUDE_FILE:=$(JVM_TEST_ROOT)$(D)TestConfig$(D)resources$(D)excludes$(D)$(JCL_VERSION)_exclude_$(JAVA_VERSION).txt
endif

#######################################
# failed target
#######################################
FAILEDTARGETS = $(TEST_ROOT)$(D)TestConfig$(D)failedtargets.mk

#######################################
# result Summary
#######################################
TEMPRESULTFILE=$(TESTOUTPUT)$(D)TestTargetResult
TAPRESULTFILE=$(TESTOUTPUT)$(D)TestTargetResult.tap

ifndef DIAGNOSTICLEVEL
export DIAGNOSTICLEVEL:=failure
endif

rmResultFile:
	@$(RM) $(Q)$(TEMPRESULTFILE)$(Q)

resultsSummary:
	@perl $(Q)$(TEST_ROOT)$(D)TestConfig$(D)scripts$(D)testKitGen$(D)resultsSummary$(D)resultsSum.pl$(Q) --failuremk=$(Q)$(FAILEDTARGETS)$(Q) --resultFile=$(Q)$(TEMPRESULTFILE)$(Q) --tapFile=$(Q)$(TAPRESULTFILE)$(Q) --diagnostic=$(DIAGNOSTICLEVEL)

