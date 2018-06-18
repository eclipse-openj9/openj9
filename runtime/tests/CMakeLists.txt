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

# We dont want to suffix vm version to shared libs
set(CMAKE_SHARED_LIBRARY_SUFFIX ${J9VM_OLD_SHARED_SUFFIX})

#add_subdirectory(aixerrmsg)
add_subdirectory(algorithm)
add_subdirectory(bcprof)
add_subdirectory(bcverify)
add_subdirectory(cutest)
add_subdirectory(gp)
add_subdirectory(j9vm)
add_subdirectory(jni)
add_subdirectory(jniarg)
add_subdirectory(jsig)
add_subdirectory(jvmtitests)
add_subdirectory(lazyclassload)
add_subdirectory(port)
add_subdirectory(props)
add_subdirectory(redirector)
add_subdirectory(shared)
#add_subdirectory(shared_service)
add_subdirectory(thread)
#add_subdirectory(unresolved)
add_subdirectory(vm)
add_subdirectory(vm_lifecycle)
#add_subdirectory(zos)
