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
#  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
##############################################################################

#
# Makefile to run various JVM tests
#

.DEFAULT_GOAL := test

D = /

SUBDIRS = ..

ifndef TEST_ROOT
	TEST_ROOT := $(shell pwd)$(D)..
endif

include settings.mk

include count.mk

#######################################
# run all tests under $(TEST_ROOT)
#######################################
runtest:
	@$(MAKE) -C $(TEST_ROOT) -f autoGen.mk _all

.PHONY: runtest

test: compile runtest
	@$(ECHO) "All Tests Completed"

.PHONY: test

.NOTPARALLEL: test

failed:
	@$(MAKE) -f failedtargets.mk failed

.PHONY: failed

.NOTPARALLEL: failed

cleanBuild:
	$(RM) -r $(BUILD_ROOT)

clean: cleanBuild
	$(RM) -r $(TEST_ROOT)$(D)TestConfig$(D)test_output_*
	$(RM) $(FAILEDTARGETS)

.PHONY: cleanBuild clean

#######################################
# download all dependent jars
#######################################
getdependency:
	perl scripts$(D)tools$(D)getDependencies.pl -path '$(TEST_ROOT)$(D)TestConfig$(D)lib' -task default -os $(OS)

#######################################
# compile all tests under $(TEST_ROOT)
#######################################
compile: getdependency
	ant -f scripts$(D)build_test.xml -DTEST_ROOT=$(TEST_ROOT) -DBUILD_ROOT=$(BUILD_ROOT) -DJAVA_BIN=$(JAVA_BIN) -DJDK_VERSION=$(JDK_VERSION) -DJCL_VERSION=$(JCL_VERSION) -DBUILD_LIST=${BUILD_LIST} -DRESOURCES_DIR=${RESOURCES_DIR} -DPLATFORM=${PLATFORM} -DTEST_JDK_HOME=${TEST_JDK_HOME} -DJVM_VERSION=$(JVM_VERSION)
