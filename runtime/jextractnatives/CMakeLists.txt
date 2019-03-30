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

# Don't append version suffix to j9jextract
set(CMAKE_SHARED_LIBRARY_SUFFIX ${J9VM_OLD_SHARED_SUFFIX})

add_library(j9jextract SHARED
	jextractglue.c
	jextractnatives.c
	
)

target_link_libraries(j9jextract
	PRIVATE
		j9vm_interface

		j9dbgext
		j9util
		j9utilcore
		j9pool
)

omr_add_exports(j9jextract
	Java_com_ibm_jvm_j9_dump_extract_Main_doCommand
	Java_com_ibm_jvm_j9_dump_extract_Main_getEnvironmentPointer
)

install(
	TARGETS j9jextract
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
