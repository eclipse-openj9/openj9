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
# Helper Macro because cmd.exe has such a tiny size for its maximum command line
#
# Essentially implements this recursive behavior:
#
# def GenList(object_list, out_file):
#   if(len(object_list) == 0):
#     return
#
#   generate_echo(object_list[0], out_file)
#
#   GenList(object_list[1:], out_file)
#
define GENLIST_HELPER
	@echo $(call FIXPATH,$(2)) >> $(call FIXPATH,$(1))

endef

GENLIST = \
	$(if $(2), \
		$(call GENLIST_HELPER,$(1),$(word 1,$(2))) \
		$(call GENLIST,$(1),$(wordlist 2,$(words $(2)),$(2))))
