#/*******************************************************************************
# * Copyright (c) 2016, 2017 IBM Corp. and others
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
# * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
# *******************************************************************************/

ifndef JVM_VERSION
$(error JVM_VERSION is undefined)
endif

ifndef SPEC_LEVEL
$(error SPEC_LEVEL is undefined)
else ifeq (,$(findstring $(SPEC_LEVEL),1.8 8))
$(error Unsupported SPEC_LEVEL: $(SPEC_LEVEL))
endif

ifndef WORKSPACE
$(error WORKSPACE is undefined)
endif

define \n


endef

# JVM setting
JAVA_BIN   ?= $(JAVA_HOME)/bin
JAVAC      := $(JAVA_BIN)/javac
JAVA       := $(JAVA_BIN)/java
JAR        := $(JAVA_BIN)/jar

# set up build path
J9_JCL     := $(WORKSPACE)/jcl

JAVAC_ARGS := -verbose -g -Xlint:unchecked -source 1.8 -target 1.8

JPP_CONFIG := CUDA4J
JPP_ARGS   := \
	-verdict \
	-baseDir "" \
	-xml $(J9_JCL)/jpp_configuration.xml \
	-config $(JPP_CONFIG) \
	-srcRoot $(J9_JCL) \
	-dest $(J9_JCL)/src.preprocessed/$(JPP_CONFIG)

CUDA4J_BIN := bin_j8
OUTPUT_JAR := cuda4j_j8.jar

# recursively find files in directory $1 matching the pattern $2
findAllFiles = \
	$(foreach i,$(wildcard $1/*),$(call findAllFiles,$i,$2)) \
	$(wildcard $1/$2)

.PHONY : clean check-env preprocess-cuda compile-cuda build-jar

all : build-jar

preprocess-cuda : check-env
	$(JAVA) -cp "$(WORKSPACE)/sourcetools/lib/jpp.jar" -Dfile.encoding=US-ASCII \
		com.ibm.jpp.commandline.CommandlineBuilder $(JPP_ARGS)

compile-cuda : preprocess-cuda
	mkdir -p $(J9_JCL)/$(CUDA4J_BIN)/$(JPP_CONFIG)
	$(JAVAC) $(JAVAC_ARGS) -d $(J9_JCL)/$(CUDA4J_BIN)/$(JPP_CONFIG) \
		$(sort $(strip $(call findAllFiles,src.preprocessed/$(JPP_CONFIG),*.java)))

build-jar : compile-cuda
	$(JAR) cvfM $(J9_JCL)/$(OUTPUT_JAR) -C $(J9_JCL)/$(CUDA4J_BIN)/$(JPP_CONFIG) .
	@echo "BUILD" "SUCCESSFUL"

check-env :
	@echo "Buildfile: cuda4j.mk"

clean : check-env
	rm -f $(OUTPUT_JAR)
