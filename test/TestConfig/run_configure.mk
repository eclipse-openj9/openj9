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

ifndef SPEC
$(error Please provide SPEC that matches the current platform (e.g. SPEC=linux_x86-64))
else
export SPEC:=$(SPEC)
$(info set SPEC to $(SPEC))
endif

autoconfig:
	perl configure.pl

autogen: autoconfig
	cd $(CURRENT_DIR)$(D)scripts$(D)testKitGen; \
	perl testKitGen.pl --graphSpecs=$(SPEC) $(OPTS); \
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