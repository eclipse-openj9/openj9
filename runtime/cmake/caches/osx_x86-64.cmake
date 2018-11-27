################################################################################
# Copyright (c) 2018, 2018 IBM Corp. and others
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

# We are going to base this cache file off of linux_x86-64_cmprssptrs
# Here we explicitly disable the bits we dont want from that file

set(OMR_OPT_CUDA OFF CACHE BOOL "") # cuda not supported on osx

# The remaining flags are all related to compressed references
# (since this is a non compressed ref build)
set(OMR_GC_COMPRESSED_POINTERS OFF CACHE BOOL "")
set(OMR_INTERP_COMPRESSED_OBJECT_HEADER OFF CACHE BOOL "")
set(OMR_INTERP_SMALL_MONITOR_SLOT OFF CACHE BOOL "")
set(J9VM_GC_CLASSES_ON_HEAP OFF CACHE BOOL "")
set(J9VM_GC_COMPRESSED_POINTERS OFF CACHE BOOL "")
set(J9VM_INTERP_COMPRESSED_OBJECT_HEADER OFF CACHE BOOL "")
set(J9VM_INTERP_SMALL_MONITOR_SLOT OFF CACHE BOOL "")

#TODO: this should be based off of the linux_x86-64 (non compressed refs) once it exists
include(${CMAKE_CURRENT_LIST_DIR}/linux_x86-64_cmprssptrs.cmake)
