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

add_library(j9gc SHARED
	dllinit.c
	gctable.c

)

if(OMR_TOOLCONFIG STREQUAL "msvc")
	target_compile_options(j9gc PRIVATE /w34189)
endif()

target_link_libraries(j9gc
	PRIVATE
		j9vm_interface

		j9util
		j9utilcore
		j9avl
		j9thr
		j9stackmap
		j9pool
		omrgc #LINKING HACK
		j9hookable
		j9gcbase
		j9gcstructs
		j9gcstats
		j9gcapi
		j9modronstartup
		j9gctrc
		j9gctrcstandard
		j9gctrcvlhgc
		j9realtime
		j9gcvlhgc
		omrgc

		#TODO: this is hack to make linking work.
		# Need to figure out why we need this in CMake but not UMA
		j9modronstandard
		j9gcbase
		j9hashtable
		j9gcbase
		j9modronstandard
		j9realtime
		j9gcvlhgc
		j9gcbase

		j9thr
)

if(OMR_NEED_LIBRT)
	target_link_libraries(j9gc PRIVATE rt)
endif()

if(OMR_HOST_OS STREQUAL "linux")
	target_link_libraries(j9gc PRIVATE pthread)
endif()

target_include_directories(j9gc
	PUBLIC
		$<TARGET_PROPERTY:omrgc,INTERFACE_INCLUDE_DIRECTORIES>
)

omr_add_exports(j9gc
	JVM_OnLoad
	J9VMDllMain
)

target_enable_ddr(j9gc j9ddr)

install(
	TARGETS j9gc
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
