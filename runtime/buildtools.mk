# buildtools Makefile

###############################################################################
# Copyright (c) 1998, 2019 IBM Corp. and others
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

define \n


endef

ifndef SPEC
$(error $(\n)\
ERROR: SPEC is not set.$(\n)\
USAGE: make -f buildtools.mk SPEC=<value> [TRC_THRESHOLD=<value>] [BUILD_ID=<value>]$(\n)\
SPEC is the name of the spec file without .spec. Be careful to use the right one, especially for default/compressed.$(\n)\
TRC_THRESHOLD is optional. Default value is 1.$(\n)\
BUILD_ID is optional. Default value is 000000.$(\n))
endif

BUILD_ID      ?= 000000
OS            := $(shell uname)
TRC_THRESHOLD ?= 1
FREEMARKER_JAR ?= $(CURDIR)/buildtools/freemarker.jar
CMAKE_ARGS :=

# We grab the C/C++ compilers detected by autoconf or provided by user, not
# the CC/CXX variables defined by the makefiles, which potentially include
# the ccache command which will throw off cmake
ifneq (,$(OPENJ9_CC))
  CMAKE_ARGS += "-DCMAKE_C_COMPILER=$(OPENJ9_CC)"
else
  CMAKE_ARGS += "-DCMAKE_C_COMPILER=$(ac_cv_prog_CC)"
endif

ifneq (,$(OPENJ9_CXX))
  CMAKE_ARGS += "-DCMAKE_CXX_COMPILER=$(OPENJ9_CXX)"
else
  CMAKE_ARGS += "-DCMAKE_CXX_COMPILER=$(ac_cv_prog_CXX)"
endif

ifneq (,$(CCACHE))
  # Open jdk makefiles add a  bunch of environment variables to the ccache command
  # cmake will not parse this properly, so we wrap the whole thing in the env command
  # We also need to add semicolons between arguments or else cmake will treat the whole
  # thing as one long command name

  # Note: we remove the CCACHE_COMPRESS option that openjdk adds, because it significantly
  # slows down the build (to the point of erasing any gains from using ccache)
  CCACHE_NOCOMPRESS := $(filter-out CCACHE_COMPRESS=1,$(CCACHE))
  ESCAPED_CCACHE :=env$(shell printf ";%s" $(CCACHE_NOCOMPRESS))

  CMAKE_ARGS += "-DCMAKE_CXX_COMPILER_LAUNCHER=$(ESCAPED_CCACHE)"
  CMAKE_ARGS += "-DCMAKE_C_COMPILER_LAUNCHER=$(ESCAPED_CCACHE)"
endif

ifneq (,$(or $(findstring Windows,$(OS)),$(findstring CYGWIN,$(OS))))
	EXEEXT := .exe
	PATHSEP := ;
else
	EXEEXT :=
	PATHSEP := :
	J9_ROOT := $(shell pwd)
endif

ifdef BOOT_JDK
	JAVA := $(subst //,/,$(subst \,/,$(BOOT_JDK)/bin/java))
else
	JAVA := $(if $(J9_ROOT),java8,$(DEV_TOOLS)\ibm-jdk-1.8.0\bin\java)
endif

ifneq (,$(VERSION_MAJOR))
	CMAKE_ARGS += -DJAVA_SPEC_VERSION=$(VERSION_MAJOR)
endif

default : all

all : ddr tools

tools : configure constantpool copya2e nls

# CMake builds don't need hooktool or tracing.
ifneq (true,$(OPENJ9_ENABLE_CMAKE))
tools : hooktool tracing
endif

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

TRACEGEN_DEFINITION_TDF_FILES := $(patsubst ./%,%,$(strip $(call findAllFiles,.,*.tdf)))
TRACEGEN_DEFINITION_SENTINELS := $(patsubst %.tdf,%.tracesentinel,$(TRACEGEN_DEFINITION_TDF_FILES))

%.tracesentinel : %.tdf
	./tracegen -treatWarningAsError -generatecfiles -force -threshold $(TRC_THRESHOLD) -file $<
	touch $@

# re-run tracegen if the executable has changed
$(TRACEGEN_DEFINITION_SENTINELS) : tracegen$(EXEEXT)

all_tracesentinels : $(TRACEGEN_DEFINITION_SENTINELS)

# process TDF files to generate RAS tracing headers and C files
trace_merge : tracemerge$(EXEEXT) $(TRACEGEN_DEFINITION_SENTINELS)
	./tracemerge -majorversion 5 -minorversion 1 -root .
	touch $@

tracing : buildtrace
	$(MAKE) -f buildtools.mk 'SPEC=$(SPEC)' all_tracesentinels
	$(MAKE) -f buildtools.mk 'SPEC=$(SPEC)' trace_merge

NLS_NLSTOOL := $(JAVA) -cp sourcetools/lib/j9nls.jar com.ibm.oti.NLSTool.J9NLS
NLS_OPTIONS :=

# process NLS files to generate java.properties and NLS header files
nls : buildtools
	$(NLS_NLSTOOL) $(NLS_OPTIONS)

HOOK_DEFINITION_FILES := $(strip $(call findAllFiles,.,*.hdf))
HOOK_DEFINITION_SENTINELS := $(patsubst %.hdf,%.hooksentinel, $(HOOK_DEFINITION_FILES))

# process HDF files to generate hook header files
%.hooksentinel : %.hdf
	./hookgen $<
	touch $@

# re-run hookgen if the executable has changed
$(HOOK_DEFINITION_SENTINELS) : hookgen$(EXEEXT)

all_hooksentinels : $(HOOK_DEFINITION_SENTINELS)

hooktool : buildhook
	$(MAKE) -f buildtools.mk 'SPEC=$(SPEC)' all_hooksentinels

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
  ../gc_realtime \
  ../gc_trace \
  ../gc_vlhgc

ifeq (true,$(OPENJ9_ENABLE_CMAKE))
# If we are doing a cmake build, configure won't depend on UMA.
# However we need to run constantpool and nls tools before invoking cmake.
configure : constantpool nls
	mkdir -p build && cd build && $(CMAKE) -C ../cmake/caches/$(SPEC).cmake  $(CMAKE_ARGS) $(EXTRA_CMAKE_ARGS) ..
else
.PHONY : j9includegen

j9includegen : uma
	$(MAKE) -C include j9include_generate

configure : j9includegen
	$(MAKE) -C omr -f run_configure.mk 'SPEC=$(SPEC)' 'OMRGLUE=$(OMRGLUE)' 'CONFIG_INCL_DIR=$(CONFIG_INCL_DIR)' 'OMRGLUE_INCLUDES=$(OMRGLUE_INCLUDES)' 'EXTRA_CONFIGURE_ARGS=$(EXTRA_CONFIGURE_ARGS)'
endif

# run UMA to generate makefiles
J9VM_GIT_DIR := $(firstword $(wildcard $(J9_ROOT)/.git) $(wildcard $(J9_ROOT)/workspace/.git))
J9VM_SHA     := $(if $(J9VM_GIT_DIR),$(shell git -C $(dir $(J9VM_GIT_DIR)) rev-parse --short HEAD),developer.compile)
SPEC_DIR     := buildspecs
UMA_TOOL     := $(JAVA) -cp "sourcetools/lib/om.jar$(PATHSEP)$(FREEMARKER_JAR)$(PATHSEP)sourcetools/lib/uma.jar" com.ibm.j9.uma.Main
UMA_OPTIONS  := -rootDir . -configDir $(SPEC_DIR) -buildSpecId $(SPEC)
UMA_OPTIONS  += -buildId $(BUILD_ID) -buildTag $(J9VM_SHA) -jvf compiler/jit.version
UMA_OPTIONS  += $(UMA_OPTIONS_EXTRA)
ifneq (,$(VERSION_MAJOR))
UMA_OPTIONS  += -M JAVA_SPEC_VERSION=$(VERSION_MAJOR)
endif
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
CONSTANTPOOL_OPTIONS := \
	-rootDir . \
	-buildSpecId $(SPEC) \
	-configDir $(SPEC_DIR) \
	-version $(VERSION_MAJOR)

constantpool : buildtools
	$(CONSTANTPOOL_TOOL) $(CONSTANTPOOL_OPTIONS)

# Backslashes need replacing on Windows/Cygwin
ESCAPED_JAVA := $(subst \,/,$(JAVA))
PLATFORM     := $(if $(wildcard buildtools/j9ddr-autoblob.jar),$(shell $(JAVA) -cp buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GetNativeDirectory))
SUPERSET     := superset.$(SPEC).dat

# Trigger cross-compilation & use linux_x86 DDR configuration
ifeq (linux_ztpf_390-64, $(SPEC))
	PLATFORM := linux_x86
	OS := ztpf
endif

# Generate DDR structure blob C files - only functions on module.ddr build specs.
ddr :
ifneq (,$(filter linux_x86 win_x86,$(PLATFORM)))
# On platforms where ddr_buildtools.mk is actually run, generated header files must be produced first.
ddr : constantpool hooktool nls tracing
	@echo "Building DDR"
	@echo "Java     = $(ESCAPED_JAVA)"
	@echo "Platform = $(PLATFORM)"
	@echo "OS       = $(OS)"
	@echo "Building $(PLATFORM)"
	( \
	if [ -e ddr/j9ddrgen.mk ] ; then \
		$(MAKE) -C ddr -f ddr_buildtools.mk JAVA=$(ESCAPED_JAVA) PLATFORM=$(PLATFORM) generate_ddr_blob_c ; \
		echo "Generating updated superset ready for code generation" ; \
		$(ESCAPED_JAVA) -cp buildtools/j9ddr-autoblob.jar com.ibm.j9ddr.autoblob.GenerateSpecSuperset -d ddr/superset -s ddr,gc_ddr -f $(SUPERSET) ; \
	fi \
	)
endif

# Create the lib directory
makeLibDir :
	mkdir -p lib

.PHONY : all_hooksentinels all_tracesentinels buildtools checkSpec configure constantpool copya2e ddr hooktool makeLibDir nls tools tracing uma
