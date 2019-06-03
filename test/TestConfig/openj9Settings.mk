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
# This makefile contains openJ9 specific settings
#

#######################################
# For 64 bits, if SPEC contains _cmprssptrs, add -Xcompressedrefs.
# If not, add -Xnocompressedrefs.
#######################################
ifneq (,$(findstring bits.64,$(BITS)))
	ifneq (,$(findstring _cmprssptrs,$(SPEC)))
		RESERVED_OPTIONS+= -Xcompressedrefs
	else
		RESERVED_OPTIONS+= -Xnocompressedrefs
	endif
endif


#######################################
# Set JAVA_SHARED_LIBRARIES_DIR  and VM_SUBDIR for tests which need native library.
# Set ADD_JVM_LIB_DIR_TO_LIBPATH as tests on some platforms need LIBPATH containing VM directory
#######################################
JAVA_LIB_DIR:=$(JAVA_BIN)$(D)..$(D)lib
VM_SUBDIR=default
ifneq (,$(findstring cmprssptrs,$(SPEC)))
VM_SUBDIR=compressedrefs
endif

ifneq (,$(findstring le,$(SPEC)))
	ARCH_DIR=ppc64le
else ifneq (,$(findstring ppc-64,$(SPEC)))
	ARCH_DIR=ppc64
else ifneq (,$(findstring ppc,$(SPEC)))
	ARCH_DIR=ppc
else ifneq (,$(findstring arm,$(SPEC)))
	ARCH_DIR=arm
else ifneq (,$(findstring 390-64,$(SPEC)))
	ARCH_DIR=s390x
else ifneq (,$(findstring 390,$(SPEC)))
	ARCH_DIR=s390
else ifneq (,$(findstring linux_x86-64,$(SPEC)))
	ARCH_DIR=amd64
else ifneq (,$(findstring linux_x86,$(SPEC)))
	ARCH_DIR=i386
endif

ifndef NATIVE_TEST_LIBS
	NATIVE_TEST_LIBS=$(TEST_JDK_HOME)$(D)..$(D)native-test-libs$(D)
endif

# if JCL_VERSION is current check for default locations for native test libs
# otherwise, native test libs are under NATIVE_TEST_LIBS
ifneq (, $(findstring current, $(JCL_VERSION)))
	ifneq (,$(findstring win,$(SPEC)))
		JAVA_SHARED_LIBRARIES_DIR:=$(JAVA_BIN)$(D)$(VM_SUBDIR)
		J9VM_PATH=$(JAVA_BIN)$(D)j9vm
	else
		JAVA_SHARED_LIBRARIES_DIR:=$(JAVA_LIB_DIR)$(D)$(ARCH_DIR)$(D)$(VM_SUBDIR)
		J9VM_PATH=$(JAVA_LIB_DIR)$(D)$(ARCH_DIR)$(D)j9vm
	endif
	ADD_JVM_LIB_DIR_TO_LIBPATH:=export LIBPATH=$(Q)$(LIBPATH)$(P)$(JAVA_LIB_DIR)$(D)$(VM_SUBDIR)$(P)$(JAVA_SHARED_LIBRARIES_DIR)$(P)$(JAVA_BIN)$(D)j9vm$(Q);
else
	ifneq (, $(findstring 8, $(JDK_VERSION)))
		ifneq (,$(findstring win,$(SPEC)))
			VM_SUBDIR_PATH=$(JAVA_BIN)$(D)$(VM_SUBDIR)
			J9VM_PATH=$(JAVA_BIN)$(D)j9vm
		else
			VM_SUBDIR_PATH=$(JAVA_LIB_DIR)$(D)$(ARCH_DIR)$(D)$(VM_SUBDIR)
			J9VM_PATH=$(JAVA_LIB_DIR)$(D)$(ARCH_DIR)$(D)j9vm
		endif
	else
		ifneq (,$(findstring win,$(SPEC))) 
			VM_SUBDIR_PATH=$(JAVA_BIN)$(D)$(VM_SUBDIR)
			J9VM_PATH=$(JAVA_BIN)$(D)j9vm
		else
			VM_SUBDIR_PATH=$(JAVA_LIB_DIR)$(D)$(VM_SUBDIR)
			J9VM_PATH=$(JAVA_LIB_DIR)$(D)j9vm
		endif
	endif

	PS=:
	ifneq (,$(findstring win,$(SPEC)))
		ifeq ($(CYGWIN),1)
			NATIVE_TEST_LIBS := $(shell cygpath -u $(NATIVE_TEST_LIBS))
			VM_SUBDIR_PATH := $(shell cygpath -u $(VM_SUBDIR_PATH))
			J9VM_PATH := $(shell cygpath -u $(J9VM_PATH))
		else
			PS=;
		endif
	endif
	
	TEST_LIB_PATH_VALUE:=$(NATIVE_TEST_LIBS)$(PS)$(VM_SUBDIR_PATH)$(PS)$(J9VM_PATH)
		
	ifneq (,$(findstring win,$(SPEC)))
		TEST_LIB_PATH:=PATH=$(Q)$(TEST_LIB_PATH_VALUE)$(PS)$(PATH)$(Q)
	else ifneq (,$(findstring aix,$(SPEC)))
		TEST_LIB_PATH:=LIBPATH=$(Q)$(LIBPATH)$(PS)$(TEST_LIB_PATH_VALUE)$(Q)
	else ifneq (,$(findstring zos,$(SPEC)))
		TEST_LIB_PATH:=LIBPATH=$(Q)$(LIBPATH)$(PS)$(TEST_LIB_PATH_VALUE)$(Q)
	else ifneq (,$(findstring osx,$(SPEC)))
		TEST_LIB_PATH:=DYLD_LIBRARY_PATH=$(Q)$(TEST_LIB_PATH_VALUE)$(PS)$(DYLD_LIBRARY_PATH)$(Q)
	else
		TEST_LIB_PATH:=LD_LIBRARY_PATH=$(Q)$(TEST_LIB_PATH_VALUE)$(PS)$(LD_LIBRARY_PATH)$(Q)
	endif
	
	JAVA_SHARED_LIBRARIES_DIR:=$(NATIVE_TEST_LIBS)
	ADD_JVM_LIB_DIR_TO_LIBPATH:=export $(TEST_LIB_PATH);
endif

ifneq ($(DEBUG),)
$(info JAVA_SHARED_LIBRARIES_DIR is set to $(JAVA_SHARED_LIBRARIES_DIR))
$(info VM_SUBDIR is set to $(VM_SUBDIR))
$(info ADD_JVM_LIB_DIR_TO_LIBPATH is set to $(ADD_JVM_LIB_DIR_TO_LIBPATH))
endif

#######################################
# TEST_STATUS
# if TEST_RESULTSTORE is enabled, failed test will uploaded to vmfarm result store
#######################################
ifneq ($(TEST_RESULTSTORE),)
ifndef BUILD_ID
$(error BUILD_ID is needed for uploading files to result store)
endif
ifndef JOB_ID
$(error JOB_ID is needed for uploading files to result store)
endif
ifndef JAVATEST_ROOT
$(error JAVATEST_ROOT is needed for uploading files to result store)
endif
AXXONRESULTSSERVER=vmfarm.ottawa.ibm.com:31
TEST_STATUS=if [ $$? -eq 0 ] ; then $(ECHO) $(Q)$(Q); $(ECHO) $(Q)$@$(Q)$(Q)_PASSED$(Q); $(ECHO) $(Q)$(Q); $(CD) $(TEST_ROOT); $(RM_REPORTDIR) else perl $(Q)-I$(JAVATEST_ROOT)$(D)lib$(D)perl$(Q) -mResultStore::Uploader -e $(Q)ResultStore::Uploader::upload('.',$(BUILD_ID),$(JOB_ID),'$(AXXONRESULTSSERVER)','results-$(JOB_ID)')$(Q); $(ECHO) $(Q)$(Q); $(ECHO) $(Q)$@$(Q)$(Q)_FAILED$(Q); $(ECHO) $(Q)$(Q); fi
endif

#######################################
# CONVERT_TO_EBCDIC_CMD
# convert ascii to ebcdic on zos
#######################################
TOEBCDIC_CMD= \
$(ECHO) $(Q)converting ascii files to ebcdic$(Q); \
find | grep \\.java$ | while read f; do echo $$f; iconv -f iso8859-1 -t ibm-1047 < $$f > $$f.ebcdic; rm $$f; mv $$f.ebcdic $$f; done; \
find | grep \\.txt$ | while read f; do echo $$f; iconv -f iso8859-1 -t ibm-1047 < $$f > $$f.ebcdic; rm $$f; mv $$f.ebcdic $$f; done; \
find | grep \\.mf$ | while read f; do echo $$f; iconv -f iso8859-1 -t ibm-1047 < $$f > $$f.ebcdic; rm $$f; mv $$f.ebcdic $$f; done; \
find | grep \\.policy$ | while read f; do echo $$f; iconv -f iso8859-1 -t ibm-1047 < $$f > $$f.ebcdic; rm $$f; mv $$f.ebcdic $$f; done; \
find | grep \\.props$ | while read f; do echo $$f; iconv -f iso8859-1 -t ibm-1047 < $$f > $$f.ebcdic; rm $$f; mv $$f.ebcdic $$f; done; \
find | grep \\.sh$ | while read f; do echo $$f; iconv -f iso8859-1 -t ibm-1047 < $$f > $$f.ebcdic; rm $$f; mv $$f.ebcdic $$f; done; \
find | grep \\.pl$ | while read f; do echo $$f; iconv -f iso8859-1 -t ibm-1047 < $$f > $$f.ebcdic; rm $$f; mv $$f.ebcdic $$f; done; \
find | grep \\.xml$ | while read f; do echo $$f; iconv -f iso8859-1 -t ibm-1047 < $$f > $$f.ebcdic; rm $$f; mv $$f.ebcdic $$f; done;

CONVERT_TO_EBCDIC_CMD=
ifneq (,$(findstring zos,$(SPEC)))
	CONVERT_TO_EBCDIC_CMD=$(TOEBCDIC_CMD)
endif
