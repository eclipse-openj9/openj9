##############################################################################
#  Copyright (c) 2016, 2019 IBM Corp. and others
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
	@echo "required:"
	@echo "    TEST_JDK_HOME=<path to JDK home directory that you wish to test>"
	@echo "    BUILD_LIST=<comma separated projects to be compiled and executed> (default to all projects)"
	@echo "optional:"
	@echo "    SPEC=[linux_x86-64|linux_x86-64_cmprssptrs|...] (platform on which to test, could be auto detected)"
	@echo "    JDK_VERSION=[8|9|10|11|12|Panama|Valhalla] (default to 8, could be auto detected)"
	@echo "    JDK_IMPL=[openj9|ibm|hotspot|sap] (default to openj9, could be auto detected)"
	@echo "    NATIVE_TEST_LIBS=<path to native test libraries> (default to native-test-libs folder at the same level as TEST_JDK_HOME)"

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
SQ='
P=:
AND_IF_SUCCESS=&&
PROPS_DIR=props_unix

-include $(TEST_ROOT)$(D)TestConfig$(D)autoGenEnv.mk
include $(TEST_ROOT)$(D)TestConfig$(D)envSettings.mk
include $(TEST_ROOT)$(D)TestConfig$(D)utils.mk
include $(TEST_ROOT)$(D)TestConfig$(D)testEnv.mk
include $(TEST_ROOT)$(D)TestConfig$(D)featureSettings.mk

# temporarily support both JAVA_VERSION and JDK_VERSION
ifeq ($(JAVA_VERSION), SE80)
	JDK_VERSION:=8
endif
ifeq ($(JAVA_VERSION), SE90)
	JDK_VERSION:=9
endif
ifeq ($(JAVA_VERSION), SE100)
	JDK_VERSION:=10
endif
ifeq ($(JAVA_VERSION), SE110)
	JDK_VERSION:=11
endif
ifeq ($(JAVA_VERSION), SE120)
	JDK_VERSION:=12
endif
ifeq ($(JAVA_VERSION), SE130)
	JDK_VERSION:=13
endif

ifndef JDK_VERSION
	export JDK_VERSION:=8
else
	export JDK_VERSION:=$(JDK_VERSION)
endif

ifndef TEST_JDK_HOME
$(error Please provide TEST_JDK_HOME value.)
else
export TEST_JDK_HOME := $(subst \,/,$(TEST_JDK_HOME))
endif

ifeq ($(JDK_VERSION), 8)
export JAVA_BIN := $(TEST_JDK_HOME)/jre/bin
else
export JAVA_BIN := $(TEST_JDK_HOME)/bin
endif

OLD_JAVA_HOME := $(JAVA_HOME)
export JAVA_HOME := $(TEST_JDK_HOME)

ifndef SPEC
$(error Please provide SPEC that matches the current platform (e.g. SPEC=linux_x86-64))
else
export SPEC:=$(SPEC)
endif

# temporarily support both JAVA_IMPL and JDK_IMPL
ifndef JDK_IMPL
	ifndef JAVA_IMPL
		export JDK_IMPL:=openj9
	else
		export JDK_IMPL:=$(JAVA_IMPL)
	endif
else
	export JDK_IMPL:=$(JDK_IMPL)
endif


ifndef JVM_VERSION
	OPENJDK_VERSION = openjdk$(JDK_VERSION)

ifeq (hotspot, $(JDK_IMPL))
	JVM_VERSION = $(OPENJDK_VERSION)
else 
	JVM_VERSION = $(OPENJDK_VERSION)-$(JDK_IMPL)
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

JAVA_COMMAND:=$(Q)$(JAVA_BIN)$(D)java$(Q)

#######################################
# common dir and jars
#######################################
LIB_DIR=$(JVM_TEST_ROOT)$(D)TestConfig$(D)lib
TESTNG=$(LIB_DIR)$(D)testng.jar$(P)$(LIB_DIR)$(D)jcommander.jar
RESOURCES_DIR=$(JVM_TEST_ROOT)$(D)TestConfig$(D)resources

#######################################
# cmdlinetester jars
#######################################
CMDLINETESTER_JAR   =$(Q)$(JVM_TEST_ROOT)$(D)functional$(D)cmdline_options_tester$(D)cmdlinetester.jar$(Q)
CMDLINETESTER_RESJAR=$(Q)$(JVM_TEST_ROOT)$(D)functional$(D)cmdline_options_testresources$(D)cmdlinetestresources.jar$(Q)

#######################################
# testng report dir
#######################################
ifndef UNIQUEID
	GETID := $(TEST_ROOT)$(D)TestConfig$(D)scripts$(D)testKitGen$(D)resultsSummary$(D)getUniqueId.pl
	export UNIQUEID := $(shell perl $(GETID) -v)
endif
TESTOUTPUT := $(TEST_ROOT)$(D)TestConfig$(D)test_output_$(UNIQUEID)
ifeq ($(TEST_ITERATIONS), 1)
	REPORTDIR = $(Q)$(TESTOUTPUT)$(D)$@$(Q)
else
	REPORTDIR = $(Q)$(TESTOUTPUT)$(D)$@_ITER_$$itercnt$(Q)
endif

#######################################
# TEST_STATUS
#######################################
RM_REPORTDIR=
KEEP_REPORTDIR?=true
ifeq ($(KEEP_REPORTDIR), false)
	RM_REPORTDIR=$(RM) -r $(REPORTDIR);
endif
ifeq ($(TEST_ITERATIONS), 1) 
	TEST_STATUS=if [ $$? -eq 0 ] ; then $(ECHO) $(Q)$(Q); $(ECHO) $(Q)$@$(Q)$(Q)_PASSED$(Q); $(ECHO) $(Q)$(Q); $(CD) $(TEST_ROOT); $(RM_REPORTDIR) else $(ECHO) $(Q)$(Q); $(ECHO) $(Q)$@$(Q)$(Q)_FAILED$(Q); $(ECHO) $(Q)$(Q); fi
else
	TEST_STATUS=if [ $$? -eq 0 ] ; then $(ECHO) $(Q)$(Q); $(ECHO) $(Q)$@$(Q)$(Q)_PASSED(ITER_$$itercnt)$(Q); $(ECHO) $(Q)$(Q); $(CD) $(TEST_ROOT); $(RM_REPORTDIR) if [ $$itercnt -eq $(TEST_ITERATIONS) ] ; then $(ECHO) $(Q)$(Q); $(ECHO) $(Q)$@$(Q)$(Q)_PASSED$(Q); $(ECHO) $(Q)$(Q); fi else $(ECHO) $(Q)$(Q); $(ECHO) $(Q)$@$(Q)$(Q)_FAILED(ITER_$$itercnt)$(Q); $(ECHO) $(Q)$(Q); $(ECHO) $(Q)$(Q); $(ECHO) $(Q)$@$(Q)$(Q)_FAILED$(Q); $(ECHO) $(Q)$(Q); exit 1; fi
endif

ifneq ($(DEBUG),)
$(info TEST_STATUS is $(TEST_STATUS))
endif
TEST_SKIP_STATUS=$@_SKIPPED

#######################################
# TEST_SETUP
#######################################
TEST_SETUP=@echo "Nothing to be done for setup."
ifeq ($(JDK_IMPL), $(filter $(JDK_IMPL),openj9 ibm))
	TEST_SETUP=$(JAVA_COMMAND) -Xshareclasses:destroyAll; $(JAVA_COMMAND) -Xshareclasses:groupAccess,destroyAll; echo "cache cleanup done"
endif

#######################################
# TEST_TEARDOWN
#######################################
TEST_TEARDOWN=@echo "Nothing to be done for teardown."
ifeq ($(JDK_IMPL), $(filter $(JDK_IMPL),openj9 ibm))
	TEST_TEARDOWN=$(JAVA_COMMAND) -Xshareclasses:destroyAll; $(JAVA_COMMAND) -Xshareclasses:groupAccess,destroyAll; echo "cache cleanup done"
endif

#######################################
# include configure makefile
#######################################
ifndef MACHINE_MK
-include $(JVM_TEST_ROOT)$(D)TestConfig$(D)machineConfigure.mk
else
-include $(MACHINE_MK)$(D)machineConfigure.mk
endif

#######################################
# include openj9 specific settings
#######################################
ifeq ($(JDK_IMPL), $(filter $(JDK_IMPL),openj9 ibm))
	include $(TEST_ROOT)$(D)TestConfig$(D)openj9Settings.mk
endif

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

_$(TESTTARGET): setup_$(TESTTARGET) rmResultFile $(TESTTARGET) resultsSummary teardown_$(TESTTARGET)
	@$(ECHO) $@ done

.PHONY: _$(TESTTARGET) $(TESTTARGET) $(SUBDIRS) $(SUBDIRS_TESTTARGET)

.NOTPARALLEL: _$(TESTTARGET) $(TESTTARGET) $(SUBDIRS) $(SUBDIRS_TESTTARGET)

TOTALCOUNT := 0

setup_%: testEnvSetup
	@$(ECHO)
	@$(ECHO) Running make $(MAKE_VERSION)
	@$(ECHO) set TEST_ROOT to $(TEST_ROOT)
	@$(ECHO) set JDK_VERSION to $(JDK_VERSION)
	@$(ECHO) set JDK_IMPL to $(JDK_IMPL)
	@$(ECHO) set JVM_VERSION to $(JVM_VERSION)
	@$(ECHO) set JCL_VERSION to $(JCL_VERSION)
	@if [ $(OLD_JAVA_HOME) ]; then \
		$(ECHO) JAVA_HOME was originally set to $(OLD_JAVA_HOME); \
	fi
	@$(ECHO) set JAVA_HOME to $(JAVA_HOME)
	@$(ECHO) set JAVA_BIN to $(JAVA_BIN)
	@$(ECHO) set SPEC to $(SPEC)
	@$(MKTREE) $(Q)$(TESTOUTPUT)$(Q)
	@$(ECHO) Running $(TESTTARGET) ...
	@if [ $(TOTALCOUNT) -ne 0 ]; then \
		$(ECHO) There are $(TOTALCOUNT) test targets in $(TESTTARGET).; \
	fi
	$(JAVA_COMMAND) -version

teardown_%: testEnvTeardown
	@$(ECHO)

ifndef JCL_VERSION
export JCL_VERSION:=latest
else
export JCL_VERSION:=$(JCL_VERSION)
endif

# Define the EXCLUDE_FILE to be used for temporarily excluding failed tests.
# This macro is used in /test/Utils/src/org/openj9/test/util/IncludeExcludeTestAnnotationTransformer
ifndef EXCLUDE_FILE
	export EXCLUDE_FILE:=$(JVM_TEST_ROOT)$(D)TestConfig$(D)resources$(D)excludes$(D)$(JCL_VERSION)_exclude_$(JDK_VERSION).txt
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

