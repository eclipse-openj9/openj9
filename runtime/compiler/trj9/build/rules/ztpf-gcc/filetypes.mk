# Copyright (c) 2017, 2017 IBM Corp. and others
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

#
# These are the rules to compile files of type ".x" into object files
# as well as to generate clean and cleandeps rules
#

#
# Compile .c file into .o file
#
define DEF_RULE.c

$(1): $(2) $(1)$(DEPSUFF) | jit_createdirs
	$$(C_CMD) $$(C_FLAGS) $$(patsubst %,-D%,$$(C_DEFINES)) $$(patsubst %,-I'%',$$(C_INCLUDES)) -MMD -MF $$@$$(DEPSUFF) -MT $$@ -MP -o $$@ -c $$<

$(1)$(DEPSUFF): ;

-include $(1)$(DEPSUFF)

JIT_DIR_LIST+=$(dir $(1))

jit_cleanobjs::
	rm -f $(1)

jit_cleandeps::
	rm -f $(1)$(DEPSUFF)

endef # DEF_RULE.c

RULE.c=$(eval $(DEF_RULE.c))

#
# Compile .cpp file into .o file
#
define DEF_RULE.cpp
$(1): $(2) $(1)$(DEPSUFF) | jit_createdirs
	$$(CXX_CMD) $$(CXX_FLAGS) $$(patsubst %,-D%,$$(CXX_DEFINES)) $$(patsubst %,-I'%',$$(CXX_INCLUDES)) -MMD -MF $$@$$(DEPSUFF) -MT $$@ -MP -o $$@ -c $$<

$(1)$(DEPSUFF): ;

-include $(1)$(DEPSUFF)

JIT_DIR_LIST+=$(dir $(1))

jit_cleanobjs::
	rm -f $(1)

jit_cleandeps::
	rm -f $(1)$(DEPSUFF)

endef # DEF_RULE.cpp

RULE.cpp=$(eval $(DEF_RULE.cpp))

#
# Compile .s file into .o file
#
define DEF_RULE.s
$(1): $(2) | jit_createdirs
	$$(S_CMD) $$(S_FLAGS) $$(patsubst %,--defsym %=1,$$(S_DEFINES)) $$(patsubst %,-I'%',$$(S_INCLUDES)) -o $$@ $$<

JIT_DIR_LIST+=$(dir $(1))

jit_cleanobjs::
	rm -f $(1)

endef # DEF_RULE.s

RULE.s=$(eval $(DEF_RULE.s))

##### START Z SPECIFIC RULES #####
ifeq ($(HOST_ARCH),z)

#
# Compile .m4 file into .o file
#
define DEF_RULE.m4
$(1).s: $(2) | jit_createdirs
	$$(PERL_PATH) $$(ZASM_SCRIPT) $$<
	$$(M4_CMD) $$(M4_FLAGS) $$(patsubst %,-I'%',$$(M4_INCLUDES)) $$(patsubst %,-D%,$$(M4_DEFINES)) $$< > $$@
	$$(PERL_PATH) $$(ZASM_SCRIPT) $$@

JIT_DIR_LIST+=$(dir $(1))

jit_cleanobjs::
	rm -f $(1).s

$(call RULE.s,$(1),$(1).s)
endef # DEF_RULE.m4

RULE.m4=$(eval $(DEF_RULE.m4))

endif # ($(HOST_ARCH),z)
##### END Z SPECIFIC RULES #####
