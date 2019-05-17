# Copyright (c) 2000, 2019 IBM Corp. and others
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

#
# Defines rules for compiling source files into object files
#
# Each of these rules will also add themselves to jit_cleanobjs and jit_cleandeps
# to clean build artifacts and dependency files, respectively
#
include $(JIT_MAKE_DIR)/rules/win-msvc/filetypes.mk

#
# Include special macro to help get around cmd.exe's tiny max command length
#
include $(JIT_MAKE_DIR)/rules/win-msvc/helper_macro.mk

# Convert the source file names to object file names
# GNU toolchain can just build backend and frontend together at same time
JIT_PRODUCT_SOURCE_FILES+=$(JIT_PRODUCT_BACKEND_SOURCES)
JIT_PRODUCT_OBJECTS=$(patsubst %,$(FIXED_OBJBASE)/%.obj,$(basename $(JIT_PRODUCT_SOURCE_FILES)))

# Figure out the name of the .so file
JIT_PRODUCT_SONAME=$(FIXED_DLL_DIR)/$(PRODUCT_NAME).dll

# Add build name to JIT
JIT_PRODUCT_BUILDNAME_SRC=$(FIXED_OBJBASE)/omr/compiler/env/TRBuildName.cpp
JIT_PRODUCT_BUILDNAME_OBJ=$(FIXED_OBJBASE)/omr/compiler/env/TRBuildName.obj
JIT_PRODUCT_OBJECTS+=$(JIT_PRODUCT_BUILDNAME_OBJ)

# Add resource file to JIT
JIT_PRODUCT_RES_SRC=$(JIT_SCRIPT_DIR)/j9jit.rc
JIT_PRODUCT_RES_OBJ=$(FIXED_OBJBASE)/j9jit.res
JIT_PRODUCT_OBJECTS+=$(JIT_PRODUCT_RES_OBJ)

jit: $(JIT_PRODUCT_SONAME)

$(JIT_PRODUCT_SONAME): $(JIT_PRODUCT_OBJECTS) | jit_createdirs
	$(call RM,$@.objlist)
	$(call GENLIST,$@.objlist,$(SOLINK_PRE_OBJECTS) $(JIT_PRODUCT_OBJECTS) $(SOLINK_POST_OBJECTS))
	$(SOLINK_CMD) -DLL -subsystem:windows $(SOLINK_FLAGS) $(patsubst %,-libpath:%,$(call FIXPATH,$(SOLINK_LIBPATH))) -OUT:$(call FIXPATH,$@) -MAP:$(call FIXPATH,$@.map) -BASE:$(SOLINK_ORG) -DEF:$(call FIXPATH,$(SOLINK_DEF)) @$(call FIXPATH,$@.objlist) $(call FIXPATH,$(patsubst %,%.lib,$(SOLINK_SLINK))) $(SOLINK_EXTRA_ARGS)
	$(call RM,$@.objlist)

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_SONAME))

jit_cleandll::
	$(call RM,$(JIT_PRODUCT_SONAME))

# Generate BuildName object from source
$(call RULE.cpp,$(JIT_PRODUCT_BUILDNAME_OBJ),$(JIT_PRODUCT_BUILDNAME_SRC))

# If this target is marked 'phony', make will recompile the generated source, even if it doesn't change.
$(JIT_PRODUCT_BUILDNAME_SRC): jit_createdirs
	$(PERL) $(call FIXPATH,$(GENERATE_VERSION_SCRIPT)) $(PRODUCT_RELEASE) $(call FIXPATH,$@)

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_BUILDNAME_SRC))

jit_cleanobjs::
	$(call RM,$(JIT_PRODUCT_BUILDNAME_SRC))

# Generate resource file from source
$(call RULE.rc,$(JIT_PRODUCT_RES_OBJ),$(JIT_PRODUCT_RES_SRC))

#
# This part calls the "RULE.x" macros for each source file
#
$(foreach SRCFILE,$(JIT_PRODUCT_SOURCE_FILES),\
    $(call RULE$(suffix $(SRCFILE)),$(FIXED_OBJBASE)/$(basename $(SRCFILE))$(OBJSUFF),$(FIXED_SRCBASE)/$(SRCFILE)) \
 )

#
# Generate a rule that will create every directory before the build starts
#
$(foreach JIT_DIR,$(sort $(JIT_DIR_LIST)),$(eval jit_createdirs:: ; $(call MKDIR,$(JIT_DIR))))
