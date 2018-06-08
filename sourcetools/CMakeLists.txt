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
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
include(FindJava)
include(UseJava)
project(j9_sourcetools VERSION 1.0)

set(CMAKE_JAVA_TARGET_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

#TODO this needs to be named and handled better
set(SPEC_LEVEL 8 CACHE STRING "")
set(CMAKE_JAVA_COMPILE_FLAGS -nowarn -source ${SPEC_LEVEL} -target ${SPEC_LEVEL} )

# Note add_jar isn't as good at tracking target dependencies as regular cmake
# In particular it will not let you link against a target which will be defined later.
add_subdirectory(com.ibm.jpp.preprocessor)
add_subdirectory(objectmodel)
add_subdirectory(j9constantpool)
add_subdirectory(j9nls)
