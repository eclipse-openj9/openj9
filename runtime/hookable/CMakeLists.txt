################################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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
################################################################################

add_library(j9hookable SHARED
	# We just pull in the hookable objects built as part of OMR
	# TODO: Do we want to just make omr build a shared lib and use that?
	$<TARGET_OBJECTS:j9hook_obj>
)

target_link_libraries(j9hookable
	PRIVATE
		j9pool
		j9prt
		j9thr
		j9utilcore
)

# OMR keeps track of the exported symbols, and attaches them to j9hook_obj
get_target_property(hookable_exports j9hook_obj EXPORTED_SYMBOLS)
omr_add_exports(j9hookable ${hookable_exports})

install(
	TARGETS j9hookable
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
