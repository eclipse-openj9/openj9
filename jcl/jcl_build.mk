#/*******************************************************************************
# * Copyright (c) 2016, 2018 IBM Corp. and others
# *
# * This program and the accompanying materials are made available under
# * the terms of the Eclipse Public License 2.0 which accompanies this
# * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# * or the Apache License, Version 2.0 which accompanies this distribution and
# * is available at https://www.apache.org/licenses/LICENSE-2.0.
# *
# * This Source Code may also be made available under the following
# * Secondary Licenses when the conditions for such availability set
# * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# * General Public License, version 2 with the GNU Classpath
# * Exception [1] and GNU General Public License, version 2 with the
# * OpenJDK Assembly Exception [2].
# *
# * [1] https://www.gnu.org/software/classpath/license.html
# * [2] http://openjdk.java.net/legal/assembly-exception.html
# *
# * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
# *******************************************************************************/

#
# JCL build java Makefile
#
# Example call:
#   make -f <path to file>/jcl_build.mk SPEC_LEVEL=8 JPP_CONFIG=SIDECAR18-SE JPP_DIRNAME=jclSC18 \
#       JAVA_BIN=<JAVA SDK bin path> BUILD_ID=<build-id>

define \n


endef

ifndef BUILD_ROOT
$(error BUILD_ROOT is undefined)
endif

ifndef JPP_CONFIG
$(error JPP_CONFIG is undefined)
endif

ifndef JPP_DIRNAME
$(error JPP_DIRNAME is undefined)
endif

ifndef SPEC_LEVEL
$(error SPEC_LEVEL is undefined)
else ifeq (,$(findstring $(SPEC_LEVEL),1.8 8))
$(error Unsupported SPEC_LEVEL: $(SPEC_LEVEL))
endif

SPEC_VERSION := 8

# build specification
BUILD_ID   ?= 000000

# OS dependent setting
OS         ?= $(shell uname)

ifneq (,$(findstring $(OS),Windows))
  TEMP     ?= /tmp
  PATH_SEP := ;
  AND_THEN := &
else
  TEMP     ?= /tmp
  PATH_SEP := :
  AND_THEN := ;
endif

SPACE :=
SPACE +=

# JVM setting
JAVA_BIN ?= $(JAVA_HOME)/bin
JAVA     := $(JAVA_BIN)/java
JAVAC    := $(JAVA_BIN)/javac

# set up build path

ROOT_DIR  := $(shell cd $(BUILD_ROOT); pwd)
WORKSPACE := $(ROOT_DIR)/workspace
PCONFIG   := $(TEMP)/bld_$(BUILD_ID)/pConfig

# Module directories
JCL_DIR        := $(WORKSPACE)/jcl
JCL_BUILD_PATH := $(WORKSPACE)/sourcetools/J9_JCL_buildpath
JCL_TOOLS_DIR  := $(WORKSPACE)/sourcetools/J9_JCL_Build_Tools
PCONFIG_DIR    := $(PCONFIG)/pConfig_$(JPP_CONFIG)
DEST_DIR       := $(ROOT_DIR)/build/j9jcl/source/ive/lib/$(JPP_DIRNAME)

SOURCETOOLS_LIB  := $(WORKSPACE)/sourcetools/lib
IBMJZOS_JAR      := $(SOURCETOOLS_LIB)/ibmjzos.jar
RAS_BINARIES_DIR := $(ROOT_DIR)/build/RAS_Binaries

# compiler arguments
PREPROCESS_ARGS := \
	-verdict \
	-baseDir "" \
	-config "$(JPP_CONFIG)" \
	-srcRoot "$(JCL_DIR)" \
	-xml "$(JCL_DIR)/jpp_configuration.xml" \
	-dest "$(PCONFIG_DIR)/src" \
	-macro:define "com.ibm.oti.vm.library.version=29"

JCL_ARGS  := -verbose -g -Xlint:unchecked -source $(SPEC_VERSION) -target $(SPEC_VERSION)
JCL_CP    := $(IBMJZOS_JAR)

# Add Boot class path
ifneq ($(COMPILER_BCP),)
JCL_ARGS += -bootclasspath "$(subst .jar$(SPACE),.jar$(PATH_SEP),$(strip $(wildcard $(JCL_BUILD_PATH)/$(COMPILER_BCP)/*.jar)))"
endif

# functions #

# recursively find files in directory $1 matching the pattern $2
findAllFiles = \
	$(foreach i,$(wildcard $1/*),$(call findAllFiles,$i,$2)) \
	$(wildcard $1/$2)

# locate java source files
java_sources = $(sort $(strip $(call findAllFiles,$1,*.java)))

# run command $1 with arguments $2; unless argument list $2 is empty, then run command $3
ifelse = $(if $(strip $2),$1 $2,$3)

# JCL compile wrapper
# compile the .java source files
compile      = \
		$(JAVAC) $(JCL_ARGS) \
			-cp "$(JCL_CP)$(PATH_SEP)$(PCONFIG_DIR)/bin" \
			-d "$(PCONFIG_DIR)/bin" \
			$(call java_sources,$(PCONFIG_DIR)/src)

.PHONY : all init preprocess-jcl compile-jcl copy-files dist clean

all : dist clean

init :
	@ echo "Buildfile: jcl_build.mk"
	mkdir -p "$(TEMP)/bld_$(BUILD_ID)"
	mkdir -p "$(PCONFIG_DIR)/bin"
	mkdir -p "$(DEST_DIR)"

preprocess-jcl : init
	$(JAVA) -cp "$(WORKSPACE)/sourcetools/lib/jpp.jar" -Dfile.encoding=US-ASCII \
		com.ibm.jpp.commandline.CommandlineBuilder $(PREPROCESS_ARGS)

compile-jcl : preprocess-jcl
	$(call compile)

FILES_TO_COPY := \
	com/ibm/gpu/ibm_gpu_thresholds.properties \
	com/ibm/oti/util/ExternalMessages.properties

copy-files : compile-jcl
	@ $(foreach file,$(FILES_TO_COPY), cp $(PCONFIG_DIR)/src/$(file) $(PCONFIG_DIR)/bin/$(file) || echo $(PCONFIG_DIR)/src/$(file) not found $(\n))
ifeq ($(JPP_CONFIG), SIDECAR18-SE)
	$(JAVA) -cp "$(JCL_TOOLS_DIR)/lib/*" com.ibm.oti.build.utils.GenManifest "$(PCONFIG_DIR)/manifest" "Java Runtime Environment API" "2.9" "1.8"
endif

DASH_U ?= 
ifeq ($(JPP_CONFIG), SIDECAR18-SE)
DASH_U ?= -u
endif

dist : copy-files
	mkdir -p "$(DEST_DIR)/source"
	cd "$(PCONFIG_DIR)/src" $(AND_THEN) zip -r "$(DEST_DIR)/source/full.zip"   .
	cd "$(PCONFIG_DIR)/src" $(AND_THEN) zip -r "$(DEST_DIR)/source/source.zip" . -x "com/*"
ifeq ($(JPP_CONFIG), SIDECAR18-SE)
	cd "$(PCONFIG_DIR)/manifest" $(AND_THEN) zip -D "$(DEST_DIR)/classes-vm.zip" "META-INF/MANIFEST.MF"
endif
	cd "$(PCONFIG_DIR)/bin" $(AND_THEN) zip -r "$(DEST_DIR)/classes-vm.zip" $(DASH_U)  . -x "*/java/lang/System\$$Logger*" -x "com/ibm/tools/attach/attacher*"

	mkdir -p "$(RAS_BINARIES_DIR)"
ifeq ($(JPP_CONFIG), SIDECAR18-DTFJ)
	cd "$(PCONFIG_DIR)/bin" $(AND_THEN) zip -r "$(RAS_BINARIES_DIR)/dtfj.jar" com/ibm/dtfj com/ibm/jvm/j9/dump com/ibm/java/diagnostics/utils
else ifeq ($(JPP_CONFIG), SIDECAR18-DTFJVIEW)
	cd "$(PCONFIG_DIR)/bin" $(AND_THEN) zip -r "$(RAS_BINARIES_DIR)/dtfjview.jar" com/ibm/jvm/dtfjview
else ifeq ($(JPP_CONFIG), SIDECAR18-TRACEFORMAT)
	cd "$(PCONFIG_DIR)/bin" $(AND_THEN) zip -r "$(RAS_BINARIES_DIR)/traceformat.jar" \
		com/ibm/jvm/Debug* \
		com/ibm/jvm/FormatTimestamp* \
		com/ibm/jvm/Indent* \
		com/ibm/jvm/InputFile* \
		com/ibm/jvm/MessageFile* \
		com/ibm/jvm/OutputFile* \
		com/ibm/jvm/ProgramOption* \
		com/ibm/jvm/Statistics* \
		com/ibm/jvm/Summary* \
		com/ibm/jvm/Threads* \
		com/ibm/jvm/Timezone* \
		com/ibm/jvm/TraceFormat* \
		com/ibm/jvm/Verbose* \
		com/ibm/jvm/format \
		com/ibm/jvm/trace
endif
	@ echo "BUILD SUCCESSFUL"

clean :
	rm -rf $(TEMP)/bld_$(BUILD_ID)
