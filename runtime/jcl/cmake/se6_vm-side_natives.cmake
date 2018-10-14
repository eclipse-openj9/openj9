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

add_library(j9vm_jcl_se6_vm-side_natives INTERFACE)
target_sources(j9vm_jcl_se6_vm-side_natives
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/common/java_lang_J9VMInternals.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/common/java_lang_ref_Finalizer.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/java_lang_ref_Reference.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/common/reflecthelp.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/acccont.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/annparser.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/jniidcacheinit.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/bootstrp.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/bpinit.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/clsldr.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/common/compiler.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/dump.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/exhelp
		${CMAKE_CURRENT_SOURCE_DIR}/common/getstacktrace
		${CMAKE_CURRENT_SOURCE_DIR}/common/iohelp.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/jclcinit.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/jcldefine.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/jclexception.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/jclglob.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/jclreflect.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/jclvm.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/log.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtclassloading.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtcompilation.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtgc.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtinit.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtmemmgr.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtmemory.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtmempool.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtos.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtosext.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtruntime.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtthread.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/proxy.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/shared.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/sigquit.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/stdinit
		${CMAKE_CURRENT_SOURCE_DIR}/common/system.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/thread.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/common/jcltrace.c
		#ut_j9jcl.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/vm_scar.c
		${CMAKE_CURRENT_SOURCE_DIR}/filesys/vmfilesys.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/vminternals.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/sun_reflect_ConstantPool.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/java_lang_Class.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/common/java_lang_Access.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/com_ibm_oti_vm_VM.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmthypervisor.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/mgmtprocessor.c
		${CMAKE_CURRENT_SOURCE_DIR}/common/com_ibm_jvm_Stats.c

		#TODO platform specific stuff here
		${CMAKE_CURRENT_SOURCE_DIR}/unix/syshelp.c
)

target_include_directories(j9vm_jcl_se6_vm-side_natives
INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    #TODO fix
    ${CMAKE_CURRENT_SOURCE_DIR}/unix
)
