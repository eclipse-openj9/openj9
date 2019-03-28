################################################################################
# Copyright (c) 2018, 2019 IBM Corp. and others
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

if(NOT $ENV{BOOT_JDK} STREQUAL "")
    set(JAVA_HOME $ENV{BOOT_JDK})

    # These variables are consumed when the "Java" language is enabled
    set(CMAKE_Java_COMPILER ${JAVA_HOME}/bin/javac)
    set(CMAKE_Java_RUNTIME ${JAVA_HOME}/bin/java)
    set(CMAKE_Java_AR ${JAVA_HOME}/bin/jar)

    set(Java_JAVA_EXECUTABLE ${JAVA_HOME}/bin/java)
else()
    message(STATUS "BOOT_JDK is not set, default jdk will be used")
    include(FindJava)
endif()

# Note: this is a temporary transition variable
set(J9VM_IS_NON_STAGING TRUE)


add_subdirectory(sourcetools)
add_subdirectory(runtime)
