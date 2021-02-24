################################################################################
# Copyright (c) 2020, 2021 IBM Corp. and others
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

list(APPEND OMR_PLATFORM_DEFINITIONS
	-D_CRT_NONSTDC_NO_WARNINGS
)

# Remove some default flags cmake gives us
set(flags_to_remove /O2 /Ob1 /DNDEBUG)
omr_remove_flags(CMAKE_C_FLAGS ${flags_to_remove})
omr_remove_flags(CMAKE_CXX_FLAGS ${flags_to_remove})
foreach(build_type IN ITEMS ${CMAKE_CONFIGURATION_TYPES} ${CMAKE_BUILD_TYPE})
	string(TOUPPER ${build_type} build_type)
	omr_remove_flags(CMAKE_C_FLAGS_${build_type} ${flags_to_remove})
	omr_remove_flags(CMAKE_CXX_FLAGS_${build_type} ${flags_to_remove})
endforeach()

# /Ox = enable most speed optimizations
# /Zi = produce separate pdb files
list(APPEND OMR_PLATFORM_COMPILE_OPTIONS  /Ox /Zi)

list(APPEND OMR_PLATFORM_EXE_LINKER_OPTIONS /debug /opt:icf /opt:ref)
list(APPEND OMR_PLATFORM_SHARED_LINKER_OPTIONS /debug /opt:icf /opt:ref)


# Assembler flags
#	/Cp Preserve case of user identifiers
#	/W3 Set warning level
#	/nologo Suppress copyright message
#	/coff generate COFF format object file
#	/Zm Enable MASM 5.10 compatibility
#	/Zd Add line number info
#	/Zi Add symbolic debug info
#	/Gd Use C calls (i.e. prepend underscored to symbols)
#	/safeseh Marks as containing exception handlers that are all declared with .SAFESEH (only available on x86-32)
set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /Cp /W3 /nologo /Zd /Zi")
if(OMR_ENV_DATA32)
	set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /safeseh /coff /Zm /Gd")
endif()
