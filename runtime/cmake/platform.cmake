################################################################################
# Copyright IBM Corp. and others 2017
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
################################################################################

# Get OMR's platform config
include(OmrPlatform)

# If we are building on windows, we want ASM_MASM not ASM
# Note: We need enable ASM before applying our platform config options so that our custom options don't get clobbered.
if(OMR_OS_WINDOWS)
	enable_language(ASM_MASM)
else()
	enable_language(ASM)
endif()

# Add our own platform specific config if we have any
include("${CMAKE_CURRENT_LIST_DIR}/platform/os/${OMR_HOST_OS}.cmake" OPTIONAL)
include("${CMAKE_CURRENT_LIST_DIR}/platform/arch/${OMR_HOST_ARCH}.cmake" OPTIONAL)
include("${CMAKE_CURRENT_LIST_DIR}/platform/toolcfg/${OMR_TOOLCONFIG}.cmake" OPTIONAL)


# Apply the combined platform config
omr_platform_global_setup()

if(NOT OMR_OS_OSX)
	add_definitions(-DIPv6_FUNCTION_SUPPORT)
endif()
