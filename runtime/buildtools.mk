# buildtools Makefile

###############################################################################
# Copyright (c) 1998, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

# Note: This is a Windows makefile. These tools are intended to be run
# in a Windows environment before the source is uploaded to a build
# machine for compilation. It may or may not work on other platforms.

define \n


endef

ifndef SPEC
$(error $(\n)ERROR: SPEC is not set.$(\n)USAGE: make -f buildtools.mk SPEC=<value> [TRC_THRESHOLD=<value>] [BUILD_ID=<value>]$(\n)SPEC is the name of the spec file without .spec. Be careful to use the right one, especially for default/compressed.$(\n)TRC_THRESHOLD is optional. Default value is 1.$(\n)BUILD_ID is optional. Default value is 000000.$(\n))
endif

BUILD_ID      ?= 000000
OS            := $(shell uname)
TRC_THRESHOLD ?= 1
FREEMARKER_JAR ?= $(CURDIR)/buildtools/freemarker.jar

ifneq (,$(or $(findstring Windows,$(OS)),$(findstring CYGWIN,$(OS))))
	PATHSEP := ;
else
	PATHSEP := :
	J9_ROOT := $(shell pwd)
endif

ifdef BOOT_JDK
	JAVA := $(subst //,/,$(subst \,/,$(BOOT_JDK)/bin/java))
else
	JAVA := $(if $(J9_ROOT),java8,$(DEV_TOOLS)\ibm-jdk-1.8.0\bin\java)
endif

default : all
tools : copya2e configure tracing nls hooktool constantpool
all : tools ddr

# OMRTODO
# A JIT makefile has a hardcoded path to a2e/headers.
# Until this dependency is removed, we make a copy of the headers in the expected location.
copya2e :
ifneq (,$(findstring zos,$(SPEC)))
	mkdir -p a2e
	cp -r omr/util/a2e/headers a2e/headers
endif

intrel : all makeLibDir

checkSpec :

# build java tools
buildtools :
	$(MAKE) -C sourcetools -f buildj9tools.mk FREEMARKER_JAR=$(FREEMARKER_JAR)

buildtrace : configure
ifneq ($(CALLED_BY_SOURCE_ZIP),yes)
	+ $(MAKE) -C omr -f GNUmakefile ENABLE_TRACEGEN=yes tools/tracegen tools/tracemerge
endif

buildhook : configure
ifneq ($(CALLED_BY_SOURCE_ZIP),yes)
	+ $(MAKE) -C omr -f GNUmakefile tools/hookgen
endif

# recursively find files in directory $1 matching the pattern $2
findAllFiles = \
	$(foreach i,$(wildcard $1/*),$(call findAllFiles,$i,$2)) \
	$(wildcard $1/$2)

TRACEGEN_DEFINITION_TDF_FILES := $(strip $(call findAllFiles,.,*.tdf))
TRACEGEN_DEFINITION_SENTINELS := $(patsubst %.tdf,%.tracesentinel,$(TRACEGEN_DEFINITION_TDF_FILES))
%.tracesentinel : %.tdf
	./tracegen -treatWarningAsError -generatecfiles -force -threshold $(TRC_THRESHOLD) -file $<
	touch $@

# process TDF files to generate RAS tracing headers and C files
trace_merge : buildtrace
	@$(MAKE) -f buildtools.mk $(TRACEGEN_DEFINITION_SENTINELS)
	./tracemerge -majorversion 5 -minorversion 1 -root .
	touch $@

tracing : trace_merge

NLS_OPTIONS :=
NLS_NLSTOOL := $(JAVA) -cp sourcetools/lib/j9nls.jar com.ibm.oti.NLSTool.J9NLS

# process NLS files to generate java.properties and NLS header files
nls : buildtools
	$(NLS_NLSTOOL) $(NLS_OPTIONS)

HOOK_DEFINITION_FILES := $(strip $(call findAllFiles,.,*.hdf))
HOOK_DEFINITION_SENTINELS := $(patsubst %.hdf,%.hooksentinel, $(HOOK_DEFINITION_FILES))
# process HDF files to generate hook header files
%.hooksentinel: %.hdf
	./hookgen $<
	touch $@

hooktool : buildhook
	@$(MAKE) -f buildtools.mk $(HOOK_DEFINITION_SENTINELS)

# run configure to generate makefile
OMRGLUE = ../gc_glue_java
CONFIG_INCL_DIR = ../gc_glue_java/configure_includes
OMRGLUE_INCLUDES = \
  ../oti \
  ../include \
  ../gc_base \
  ../gc_include \
  ../gc_stats \
  ../gc_structs \
  ../gc_base \
  ../include \
  ../oti \
  ../nls \
  ../gc_include \
  ../gc_structs \
  ../gc_stats \
  ../gc_modron_standard \
  ../gc_staccato \
  ../gc_realtime \
  ../gc_trace \
  ../gc_vlhgc

configure : uma
	$(MAKE) -C omr -f run_configure.mk 'SPEC=$(SPEC)' 'OMRGLUE=$(OMRGLUE)' 'CONFIG_INCL_DIR=$(CONFIG_INCL_DIR)' 'OMRGLUE_INCLUDES=$(OMRGLUE_INCLUDES)' 'EXTRA_CONFIGURE_ARGS=$(EXTRA_CONFIGURE_ARGS)'

# run UMA to generate makefile
J9VM_GIT_DIR := $(firstword $(wildcard $(J9_ROOT)/.git) $(wildcard $(J9_ROOT)/workspace/.git))
J9VM_SHA     := $(if $(J9VM_GIT_DIR),$(shell git -C $(dir $(J9VM_GIT_DIR)) rev-parse --short HEAD),developer.compile)
SPEC_DIR     := buildspecs
UMA_TOOL     := $(JAVA) -cp "sourcetools/lib/om.jar$(PATHSEP)$(FREEMARKER_JAR)$(PATHSEP)sourcetools/lib/uma.jar" com.ibm.j9.uma.Main
UMA_OPTIONS  := -rootDir . -configDir $(SPEC_DIR) -buildSpecId $(SPEC)
UMA_OPTIONS  += -buildId $(BUILD_ID) -buildTag $(J9VM_SHA) -jvf tr.source/jit.version
UMA_OPTIONS  += $(UMA_OPTIONS_EXTRA)
# JAZZ 90097 Don't build executables in OMR because, on Windows, UMA generates .rc files for these executables
# that require j9version.h, a JVM header file. j9version.h is not available in the include path for OMR modules.
# This is a temporary workaround. It can be removed after UMA is no longer used to generate files for OMR, and the
# module.xml files in the OMR directories have been deleted. The JVM doesn't require any of these executables.
UMA_OPTIONS += -ea tracegen,tracemerge
uma : buildtools copya2e
	@echo J9VM version: $(J9VM_SHA)
	$(UMA_TOOL) $(UMA_OPTIONS)

# process constant pool definition file to generate jcl constant pool definitions and header file
CONSTANTPOOL_TOOL    := $(JAVA) -cp "sourcetools/lib/om.jar$(PATHSEP)sourcetools/lib/j9vmcp.jar" com.ibm.oti.VMCPTool.Main
CONSTANTPOOL_OPTIONS := -rootDir . -buildSpecId $(SPEC) -configDir $(SPEC_DIR) -jcls se7_basic,se9_before_b165,se9
constantpool : buildtools
	$(CONSTANTPOOL_TOOL) $(CONSTANTPOOL_OPTIONS)

#Backslashes need replacing on Windows/Cygwin
ESCAPED_JAVA := $(subst \,/,$(JAVA))
PLATFORM	 := $(if $(wildcard buildtools/j9ddr-autoblob.jar),$(shell $(JAVA) -cp buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GetNativeDirectory))
SUPERSET     := superset.$(SPEC).dat

#For z/TPF change platform because of cross-build
ifeq (linux_ztpf_390-64, $(SPEC))
	PLATFORM := linux_ztpf_390-64
	OS := ztpf
endif

#Generate DDR structure blob C files - only functions on module.ddr build specs
ddr : configure
	@echo "Building DDR"
	@echo "Java     = $(ESCAPED_JAVA)"
	@echo "Platform = $(PLATFORM)"
	@echo "OS       = $(OS)"
ifneq (,$(filter linux_x86 win_x86,$(PLATFORM)))
	@echo "Building $(PLATFORM)"
	( \
	if [ -e ddr/j9ddrgen.mk ] ; then \
		$(MAKE) -C ddr -f ddr_buildtools.mk JAVA=$(ESCAPED_JAVA) PLATFORM=$(PLATFORM) generate_ddr_blob_c ; \
		echo "Generating updated superset ready for code generation" ; \
		$(ESCAPED_JAVA) -cp buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateSpecSuperset -d ddr/superset -s ddr,gc_ddr -f $(SUPERSET) ; \
	fi ; \
	)
endif

# Create the lib directory
makeLibDir :
	mkdir -p lib

.PHONY : buildtools checkSpec copya2e uma tracing nls hooktool constantpool makeLibDir ddr configure tools
