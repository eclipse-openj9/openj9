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

list(APPEND OMR_PLATFORM_COMPILE_OPTIONS -O3)

list(APPEND OMR_PLATFORM_CXX_COMPILE_OPTIONS -qnortti)

if(NOT OMR_OS_ZOS)
	list(APPEND OMR_PLATFORM_CXX_COMPILE_OPTIONS -qsuppress=1540-1087:1540-1088:1540-1090)
endif()

# OMR_PLATFORM_CXX_COMPILE_OPTIONS gets applied to the jit (which needs exceptions),
# so we put these in the CMAKE_CXX_FLAGS instead
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -qnoeh")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -qpic=large")

if(CMAKE_C_COMPILER_IS_XLCLANG)
	# xlclang/xlclang++ options
	# OMR_PLATFORM_COMPILE_OPTIONS gets applied to the jit (which doesn't compile with -g),
	# so we put -g in the CMAKE_C_FLAGS and CMAKE_CXX_FLAGS instead
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -g")
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS -fno-rtti)
else()
	# xlc/xlc++ options
	list(APPEND OMR_PLATFORM_COMPILE_OPTIONS -g)
endif()
