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

#TODO alot of this should really just be inherited from the OMR config
#TODO we are assuming Linux at the moment
if(CMAKE_CXX_COMPILER_ID STREQUAL "XL")
    set(OMR_TOOLCONFIG "xlc")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -q64 -qalias=noansi")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -q64 -qalias=noansi -qnortti -qnoeh -qsuppress=1540-1087:1540-1088:1540-1090")
    set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -qpic=large -q64")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS " -g -fno-rtti -fno-exceptions ${CMAKE_CXX_FLAGS}")
    set(CMAKE_C_FLAGS "-g ${CMAKE_C_FLAGS}")
    set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-z,defs ${CMAKE_SHARED_LINKER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -O3 -fno-strict-aliasing")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -O3 -fno-strict-aliasing -fno-exceptions -fno-rtti -fno-threadsafe-statics")
endif()

include(OmrPlatform)
omr_platform_global_setup()

if(OMR_ARCH_POWER)
    #TODO do based on toolchain stuff
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_definitions(-DOMR_ENV_GCC)
    endif()
    #TODO this is a hack
    set(J9VM_JIT_RUNTIME_INSTRUMENTATION ON CACHE BOOL "")
    set(J9VM_PORT_RUNTIME_INSTRUMENTATION ON CACHE BOOL "")
endif()

add_definitions(
	-DIPv6_FUNCTION_SUPPORT
	-DUT_DIRECT_TRACE_REGISTRATION
)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1")
