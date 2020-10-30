################################################################################
# Copyright (c) 2018, 2020 IBM Corp. and others
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

cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

project(openj9)

if("${BOOT_JDK}" STREQUAL "")
	message(FATAL_ERROR "BOOT_JDK must be defined")
endif()

message(STATUS "Using BOOT_JDK: '${BOOT_JDK}'")

# These variables are used by "Java" language support.
find_program(CMAKE_Java_COMPILER NAMES javac PATHS ${BOOT_JDK}/bin NO_DEFAULT_PATH)
find_program(CMAKE_Java_RUNTIME  NAMES java  PATHS ${BOOT_JDK}/bin NO_DEFAULT_PATH)
find_program(CMAKE_Java_AR       NAMES jar   PATHS ${BOOT_JDK}/bin NO_DEFAULT_PATH)

# Variables used by the "UseJava" module (predates "Java" language support)
set(Java_JAVA_EXECUTABLE ${CMAKE_Java_RUNTIME})
set(Java_JAVAC_EXECUTABLE ${CMAKE_Java_COMPILER})
set(Java_JAR_EXECUTABLE ${CMAKE_Java_AR})

# Native tooling
if(CMAKE_CROSSCOMPILING)
	if(NOT OMR_EXE_LAUNCHER)
		message(FATAL_ERROR "OMR_EXE_LAUNCHER must be specified when cross-compiling")
	endif()
endif()

add_subdirectory(sourcetools)
add_subdirectory(runtime)
