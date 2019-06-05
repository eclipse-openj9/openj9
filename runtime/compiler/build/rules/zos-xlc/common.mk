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
include $(JIT_MAKE_DIR)/rules/zos-xlc/filetypes.mk

# Convert the source file names to object file names
JIT_PRODUCT_BACKEND_OBJECTS=$(patsubst %,$(FIXED_OBJBASE)/%.o,$(basename $(JIT_PRODUCT_BACKEND_SOURCES)))
JIT_PRODUCT_OBJECTS=$(patsubst %,$(FIXED_OBJBASE)/%.o,$(basename $(JIT_PRODUCT_SOURCE_FILES)))

# On z/OS, we need to build a base library of all the non-frontend stuff
JIT_PRODUCT_BACKEND_LIBRARY=$(FIXED_DLL_DIR)/lib$(PRODUCT_NAME)_base.a

# Figure out the name of the .so file
JIT_PRODUCT_SONAME=$(FIXED_DLL_DIR)/lib$(PRODUCT_NAME).so

# Add build name to JIT
JIT_PRODUCT_BUILDNAME_SRC=$(FIXED_OBJBASE)/omr/compiler/env/TRBuildName.cpp
JIT_PRODUCT_BUILDNAME_OBJ=$(FIXED_OBJBASE)/omr/compiler/env/TRBuildName.o
JIT_PRODUCT_BACKEND_OBJECTS+=$(JIT_PRODUCT_BUILDNAME_OBJ)

jit: $(JIT_PRODUCT_SONAME)

$(JIT_PRODUCT_SONAME): $(JIT_PRODUCT_OBJECTS) $(JIT_PRODUCT_BACKEND_LIBRARY) | jit_createdirs
	$(SOLINK_CMD) $(SOLINK_FLAGS) $(patsubst %,-L%,$(SOLINK_LIBPATH)) -o $@ $(SOLINK_PRE_OBJECTS) $(JIT_PRODUCT_OBJECTS) $(SOLINK_POST_OBJECTS) $(JIT_PRODUCT_BACKEND_LIBRARY) $(patsubst %,-l%,$(SOLINK_SLINK)) $(SOLINK_EXTRA_ARGS) > $@.linkmap

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_SONAME))

jit_cleandll::
	rm -f $(JIT_PRODUCT_SONAME)

#
# The z/OS Archiver has a bug where it doesn't store the source directory
# of the files it contains.
#
# Therefore, it can't normally store 2 files with the same name
#
# We work around this by using the "q" option to append to the end of the
# archive without checking if it would replace an existing file
#
# Eventually I think a better solution would be to mangle the filenames
# with the directory prefix so updates would work properly
#
ALL_UNIQ_LIST=$(filter-out $(addprefix %,$(shell echo $(notdir $(1)) | tr " " "\n" | sort | uniq -d)),$(1))
ALL_DUP_LIST=$(filter $(addprefix %,$(shell echo $(notdir $(1)) | tr " " "\n" | sort | uniq -d)),$(1))

$(JIT_PRODUCT_BACKEND_LIBRARY): $(JIT_PRODUCT_BACKEND_OBJECTS) | jit_createdirs
	@rm -f $@
	$(AR_CMD) rcsv $@ $(call ALL_UNIQ_LIST,$(JIT_PRODUCT_BACKEND_OBJECTS))
	for files in $(call ALL_DUP_LIST,$(JIT_PRODUCT_BACKEND_OBJECTS)); do \
      $(AR_CMD) qcsv $@ $$files; \
    done

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_BACKEND_LIBRARY))

jit_cleanobjs::
	rm -f $(JIT_PRODUCT_BACKEND_LIBRARY)

$(call RULE.cpp,$(JIT_PRODUCT_BUILDNAME_OBJ),$(JIT_PRODUCT_BUILDNAME_SRC))

# If this target is marked 'phony', make will recompile the generated source, even if it doesn't change.
$(JIT_PRODUCT_BUILDNAME_SRC): jit_createdirs
	$(PERL) $(GENERATE_VERSION_SCRIPT) $(PRODUCT_RELEASE) $@

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_BUILDNAME_SRC))

jit_cleanobjs::
	rm -f $(JIT_PRODUCT_BUILDNAME_SRC)

#
# This part calls the "RULE.x" macros for each source file
#
$(foreach SRCFILE,$(JIT_PRODUCT_BACKEND_SOURCES),\
    $(call RULE$(suffix $(SRCFILE)),$(FIXED_OBJBASE)/$(basename $(SRCFILE))$(OBJSUFF),$(FIXED_SRCBASE)/$(SRCFILE)) \
 )

$(foreach SRCFILE,$(JIT_PRODUCT_SOURCE_FILES),\
    $(call RULE$(suffix $(SRCFILE)),$(FIXED_OBJBASE)/$(basename $(SRCFILE))$(OBJSUFF),$(FIXED_SRCBASE)/$(SRCFILE)) \
 )

#
# Generate a rule that will create every directory before the build starts
#
$(foreach JIT_DIR,$(sort $(JIT_DIR_LIST)),$(eval jit_createdirs:: ; mkdir -p $(JIT_DIR)))
