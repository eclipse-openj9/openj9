################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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

# Set up an interface library to keep track of the various compiler defines
# used by the jit components.

# TODO should probably rename to j9vm_jit_defines for less ambiguity
add_library(j9vm_compiler_defines INTERFACE)

if(OMR_ENV_DATA64)
	target_compile_definitions(j9vm_compiler_defines INTERFACE TR_64bit TR_HOST_64BIT TR_TARGET_64BIT)
else()
	target_compile_definitions(j9vm_compiler_defines INTERFACE TR_HOST_32BIT TR_TARGET_32BIT)
endif()

if(OMR_ARCH_X86)
	target_compile_definitions(j9vm_compiler_defines INTERFACE TR_TARGET_X86 TR_HOST_X86)
elseif(OMR_ARCH_POWER)
	target_compile_definitions(j9vm_compiler_defines INTERFACE TR_TARGET_POWER TR_HOST_POWER)
else()
	message(FATAL_ERROR "Currently only x86 and ppc are supported under CMake")
endif() #TODO OTHER PLATFORMS
