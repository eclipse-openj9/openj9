##############################################################################
#  Copyright (c) 2019, 2019 IBM Corp. and others
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
#  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
##############################################################################

.PHONY: autoconfig autogen clean help

.DEFAULT_GOAL := autogen

help:
	@echo "This makefile is used to run perl script which generates makefiles for JVM tests before being built and executed."
	@echo "OPTS=help     Display help information for more options."

CURRENT_DIR := $(shell pwd)
OPTS=

D=/

ifneq (,$(findstring Win,$(OS)))
CURRENT_DIR := $(subst \,/,$(CURRENT_DIR))
endif
include $(CURRENT_DIR)$(D)featureSettings.mk
-include $(CURRENT_DIR)$(D)autoGenEnv.mk
include $(CURRENT_DIR)$(D)envSettings.mk

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

autoconfig:
	perl configure.pl

autogen: autoconfig
	cd $(CURRENT_DIR)$(D)scripts$(D)testKitGen; \
	perl testKitGen.pl --graphSpecs=$(SPEC) --jdkVersion=$(JDK_VERSION) --impl=$(JDK_IMPL) --buildList=${BUILD_LIST} --iterations=$(TEST_ITERATIONS) --testFlag=$(TEST_FLAG) $(OPTS); \
	cd $(CURRENT_DIR);

AUTOGEN_FILES = $(wildcard $(CURRENT_DIR)$(D)jvmTest.mk)
AUTOGEN_FILES += $(wildcard $(CURRENT_DIR)$(D)machineConfigure.mk)
AUTOGEN_FILES += $(wildcard $(CURRENT_DIR)$(D)..$(D)*$(D)autoGenTest.mk)

clean:
ifneq (,$(findstring .mk,$(AUTOGEN_FILES)))
	$(RM) $(AUTOGEN_FILES);
else
	@echo "Nothing to clean";
endif