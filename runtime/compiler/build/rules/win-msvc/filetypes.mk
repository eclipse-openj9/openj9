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
# These are the rules to compile files of type ".x" into object files
# as well as to generate clean and cleandeps rules
#

#
# Compile .c file into .obj file
#
define DEF_RULE.c

$(1): $(2) | jit_createdirs
	$$(C_CMD) $$(C_FLAGS) $$(patsubst %,-D"%",$$(C_DEFINES)) $$(patsubst %,-I%,$$(call FIXPATH,$$(C_INCLUDES))) -Fd$$(call FIXPATH,$$@.pdb) -Fo$$(call FIXPATH,$$@) $$(call FIXPATH,$$<)

JIT_DIR_LIST+=$(dir $(1))

jit_cleanobjs::
	$$(call RM,$(1))

endef # DEF_RULE.c

RULE.c=$(eval $(DEF_RULE.c))

#
# Compile .cpp file into .obj file
#
define DEF_RULE.cpp
$(1): $(2) | jit_createdirs
	$$(CXX_CMD) $$(CXX_FLAGS) $$(patsubst %,-D"%",$$(CXX_DEFINES)) $$(patsubst %,-I%,$$(call FIXPATH,$$(CXX_INCLUDES))) -Fd$$(call FIXPATH,$$@.pdb) -Fo$$(call FIXPATH,$$@) $$(call FIXPATH,$$<)

JIT_DIR_LIST+=$(dir $(1))

jit_cleanobjs::
	$$(call RM,$(1))

endef # DEF_RULE.cpp

RULE.cpp=$(eval $(DEF_RULE.cpp))

#
# Compile .nasm file into .obj file
#
define DEF_RULE.nasm
$(1): $(2) | jit_createdirs
	$$(NASM_CMD) $$(NASM_OBJ_FORMAT) $$(patsubst %,-D%=1,$$(NASM_DEFINES)) $$(patsubst %,-I%/,$$(subst \,/,$$(NASM_INCLUDES))) -o $$(subst \,/,"$$@") $$(subst \,/,"$$<")

JIT_DIR_LIST+=$(dir $(1))

jit_cleanobjs::
	$$(call RM,$(1))

endef # DEF_RULE.nasm

RULE.nasm=$(eval $(DEF_RULE.nasm))

#
# Compile .rc file into .res file
#
define DEF_RULE.rc
$(1): $(2) | jit_createdirs
	$(RC_CMD) $$(RC_FLAGS) $$(patsubst %,-D"%",$$(RC_DEFINES)) $$(patsubst %,-I%,$$(call FIXPATH,$$(RC_INCLUDES))) -fo $$(call FIXPATH,$$@) $$(call FIXPATH,$$<)

JIT_DIR_LIST+=$(dir $(1))

jit_cleanobjs::
	$$(call RM,$(1))

endef # DEF_RULE.rc

RULE.rc=$(eval $(DEF_RULE.rc))
