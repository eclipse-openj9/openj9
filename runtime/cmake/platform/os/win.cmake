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

# Note: we need to inject WIN32 et al, as OMR no longer uses them

list(APPEND OMR_PLATFORM_DEFINITIONS
    -DWIN32
    -D_WIN32
)
if(OMR_ENV_DATA64)
    list(APPEND OMR_PLATFORM_DEFINITIONS
        -DWIN64
        -D_WIN64
    )
endif()
list(APPEND OMR_PLATFORM_DEFINITIONS -DWINDOWS)

# Set flags we use to build the interpreter
omr_stringify(CMAKE_J9VM_CXX_FLAGS
    -O3
    -fno-rtti
    -fno-threadsafe-statics
    -fno-strict-aliasing
    -fno-exceptions
    -fno-asynchronous-unwind-tables
    -std=c++0x
    -D_CRT_SUPPRESS_RESTRICT
    -DVS12AndHigher
    ${OMR_PLATFORM_DEFINITIONS}
)

if(OMR_ENV_DATA32)
    set(CMAKE_J9VM_CXX_FLAGS "${CMAKE_J9VM_CXX_FLAGS} -m32")
endif()
