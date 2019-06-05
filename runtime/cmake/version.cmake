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

# Set up some default version variables.
# Note as cache variables these can be overridden on the command line when invoking cmake
# Using the syntax `-D<VAR_NAME>=<VALUE>`

set(JAVA_SPEC_VERSION "9" CACHE STRING "Version of Java to build")
# Limit `JAVA_SPEC_VERSION` to reasonable values
# TODO: this is only a gui thing. It doesn't actually do proper enforcement
set_property(CACHE JAVA_SPEC_VERSION PROPERTY STRINGS "8" "9" "10" "11" "12" "13")

set(J9VM_VERSION_MAJOR 2 CACHE STRING "")
set(J9VM_VERSION_MINOR 9 CACHE INTERNAL "")
set(J9VM_VERSION ${J9VM_VERSION_MAJOR}.${J9VM_VERSION_MINOR})
