################################################################################
# Copyright (c) 2019, 2019 IBM Corp. and others
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

set(m4_defs)
if(OMR_ENV_DATA64)
	set(m4_defs TR_HOST_64BIT)
endif()

j9vm_gen_asm(
	Math.m4
	PicBuilder.m4
	Recompilation.m4
	ValueProf.m4
	DEFINES ${m4_defs}
	INCLUDE_DIRECTORIES
		${j9vm_SOURCE_DIR}/compiler
		${omr_SOURCE_DIR}/compiler
)

# Note we have to define a custom target here to ensure the assembly files are made before building
# j9jit. This would happen automatically if we called j9vm_gen_asm in the same directory as we defined
# the j9jit target. However, due to the way the jit builds up their source list, this is impractical.
add_custom_target(j9jit_gen_z_runtime_asm
	DEPENDS
	Math.s
	PicBuilder.s
	Recompilation.s
	ValueProf.s
)
add_dependencies(j9jit_generate j9jit_gen_z_runtime_asm)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/Math.s
	${CMAKE_CURRENT_BINARY_DIR}/PicBuilder.s
	${CMAKE_CURRENT_BINARY_DIR}/Recompilation.s
	${CMAKE_CURRENT_BINARY_DIR}/ValueProf.s
	PROPERTIES
	GENERATED TRUE
)

j9jit_files(
	z/runtime/Recomp.cpp
	z/runtime/S390RelocationTarget.cpp
	${CMAKE_CURRENT_BINARY_DIR}/Math.s
	${CMAKE_CURRENT_BINARY_DIR}/PicBuilder.s
	${CMAKE_CURRENT_BINARY_DIR}/Recompilation.s
	${CMAKE_CURRENT_BINARY_DIR}/ValueProf.s
)
