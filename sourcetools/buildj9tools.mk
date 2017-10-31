################################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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
#
################################################################################
# Makefile to compile and archive the J9 Java tools.
#
# Usage:
#   make -f buildj9tools.mk FREEMARKER_JAR=/path/to/freemarker.jar
#
# Optional parameters:
#   BOOT_JDK: path to Java 8 or 9 installation directory
#   DEST_DIR: the location to copy the J9 Java tools generated binaries
################################################################################

DEST_DIR    := lib
EMPTY       :=
LIB_DIR     := lib
OS          := $(shell uname)
PATH_SEP    := $(if $(or $(findstring Windows,$(OS)),$(findstring CYGWIN,$(OS))),;,:)
SPACE       := $(EMPTY) $(EMPTY)
WORK_PFX    := build-

# compiler settings
ifdef BOOT_JDK
  JAVA_BIN  ?= $(BOOT_JDK)/bin
else
ifdef DEV_TOOLS
  JAVA_BIN  ?= $(DEV_TOOLS)/ibm-jdk-1.8.0/bin
else
  # requires Java 8 or 9
  JAVA      := java8
  JAVA_BIN  ?= $(dir $(realpath $(shell which $(JAVA))))
endif
endif

ifneq (,$(JAVA_BIN))
  JAVA_BIN  := $(subst //,/,$(subst \,/,$(JAVA_BIN)/))
endif
JAVAC       := $(JAVA_BIN)javac
JAR         := $(JAVA_BIN)jar

SPEC_LEVEL  ?= 7
JAVAC_FLAGS := -nowarn -source $(SPEC_LEVEL) -target $(SPEC_LEVEL)

JAR_TARGETS :=

.PHONY : all clean default distclean

default : all

all : # prerequisites are contributed via macros below

clean :
	rm -rf $(WORK_PFX)*

distclean : clean
	rm -f $(JAR_TARGETS)

# FindAllFiles
# ------------
# param 1 = the directory to search (recursively)
# param 2 = the pattern to match
#
FindAllFiles       = $(strip $(call FindAllFiles_impl,$1,$2))
FindAllFiles_impl  = $(wildcard $1/$2) $(foreach i,$(wildcard $1/*),$(call FindAllFiles_impl,$i,$2))

# BuildJar_template
# -----------------
# param 1 = the base name of the jar to create
# param 2 = the source directory
# parma 3 = classpath elements (optional)
#
define BuildJar_template

$1_OTHER_FILES  := $(call FindAllFiles,$2,*.properties)

ifeq ($1,uma)
  $1_OTHER_FILES += $(call FindAllFiles,$2,*.xsd)
endif

$1_SOURCES      := $(call FindAllFiles,$2,*.java)
$1_TARGET       := $(DEST_DIR)/$1.jar
$1_WORK_DIR     := $(WORK_PFX)$1

JAR_TARGETS     += $$($1_TARGET)

all : $$($1_TARGET)

$(DEST_DIR)/$1.jar : $$($1_SOURCES) $$($1_OTHER_FILES)
	@echo Building $$@
	@rm -rf $$($1_WORK_DIR)
	@mkdir -p $$($1_WORK_DIR)
	@$(JAVAC) -d $$($1_WORK_DIR) $(JAVAC_FLAGS) $(if $3,-classpath "$(subst $(SPACE),$(PATH_SEP),$(strip $3))") $$($1_SOURCES)
	@$(JAR) cf $$@ -C $$($1_WORK_DIR) . \
		$$(patsubst $2/%,-C $2 %,$$($1_OTHER_FILES)) \
		$$(if $$(wildcard $2/schema),-C $2/schema .)
	@rm -rf $$($1_WORK_DIR)

endef # BuildJar_template

$(eval $(call BuildJar_template,j9nls,j9nls))
$(eval $(call BuildJar_template,j9vmcp,j9constantpool,$(DEST_DIR)/om.jar))
$(eval $(call BuildJar_template,om,objectmodel))
$(eval $(call BuildJar_template,jpp,com.ibm.jpp.preprocessor))
$(eval $(call BuildJar_template,uma,com.ibm.uma,$(DEST_DIR)/om.jar $(FREEMARKER_JAR)))

$(DEST_DIR)/j9vmcp.jar : $(DEST_DIR)/om.jar
$(DEST_DIR)/uma.jar : $(DEST_DIR)/om.jar
