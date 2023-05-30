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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
################################################################################

# set JAVA_HOME to guide FindJava
set(JAVA_HOME ${BOOT_JDK})

include(UseJava)

project(j9_sourcetools VERSION 1.0)

set(CMAKE_JAVA_TARGET_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# CMake generates the java classpath with a leading path separator
# which fixpath.sh in JDK11+ will not handle: Add a dummy prefix.
if(WIN32 AND (NOT CMAKE_HOST_WIN32) AND (NOT JAVA_SPEC_VERSION LESS 11))
	set(CMAKE_JAVA_INCLUDE_PATH_FINAL "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

set(CMAKE_JAVA_COMPILE_FLAGS -nowarn)

# Note add_jar isn't as good at tracking target dependencies as regular cmake.
# In particular it will not let you link against a target which will be defined later.
add_subdirectory(objectmodel)
add_subdirectory(j9constantpool)
add_subdirectory(j9nls)
