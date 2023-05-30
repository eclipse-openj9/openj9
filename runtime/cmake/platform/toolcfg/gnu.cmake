################################################################################
# Copyright IBM Corp. and others 2020
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
################################################################################

list(APPEND OMR_PLATFORM_COMPILE_OPTIONS -O3 -g -fstack-protector)
if(OMR_DDR AND NOT (CMAKE_C_COMPILER_VERSION VERSION_LESS 11))
	# In gcc 11+ the default is to use DWARF version 5 which is not yet
	# supported by ddrgen: tell the compiler to use DWARF version 4.
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS -gdwarf-4)
endif()
list(APPEND OMR_PLATFORM_C_COMPILE_OPTIONS -Wimplicit -Wreturn-type)
list(APPEND OMR_PLATFORM_CXX_COMPILE_OPTIONS -fno-threadsafe-statics)

# OMR_PLATFORM_CXX_COMPILE_OPTIONS gets applied to the jit (which needs exceptions),
# so we put these in the CMAKE_CXX_FLAGS instead
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")

# Raise an error if a shared library has any unresolved symbols.
# This flag isn't supported on OSX, but it has this behaviour by default
if(NOT OMR_OS_OSX)
	set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-z,defs ${CMAKE_SHARED_LINKER_FLAGS}")
endif()

# add -U_FORTIFY_SOURCE and -D_FORTIFY_SOURCE=1
omr_append_flags(CMAKE_C_FLAGS ${OMR_STRNCPY_FORTIFY_OPTIONS})
omr_append_flags(CMAKE_CXX_FLAGS ${OMR_STRNCPY_FORTIFY_OPTIONS})
if(J9VM_USE_RDYNAMIC AND OMR_OS_LINUX)
	omr_append_flags(CMAKE_SHARED_LINKER_FLAGS "-rdynamic")
endif()
