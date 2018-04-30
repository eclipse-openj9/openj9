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

omr_add_tracegen(j9bcu.tdf)

# We define an interface library containing all the sources here
# this is to avoid having to retype the entire sources list for test/recompiled
add_library(j9dyn_sources INTERFACE)
target_sources(j9dyn_sources
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/bcutil.c
		${CMAKE_CURRENT_SOURCE_DIR}/BufferManager.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/cfreader.c
		${CMAKE_CURRENT_SOURCE_DIR}/ClassFileOracle.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ClassFileParser.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ClassFileWriter.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ComparingCursor.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ConstantPoolMap.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/defineclass.c
		${CMAKE_CURRENT_SOURCE_DIR}/dynload.c
		${CMAKE_CURRENT_SOURCE_DIR}/J9PortAllocationStrategy.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/jimageintf.c
		${CMAKE_CURRENT_SOURCE_DIR}/jimagereader.c
		${CMAKE_CURRENT_SOURCE_DIR}/jsrinliner.c
		${CMAKE_CURRENT_SOURCE_DIR}/ROMClassBuilder.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ROMClassCreationContext.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ROMClassHashTable.c
		${CMAKE_CURRENT_SOURCE_DIR}/ROMClassSegmentAllocationStrategy.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ROMClassStringInternManager.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ROMClassWriter.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/SRPKeyProducer.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/SRPOffsetTable.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/StringInternTable.cpp
		# Excluded by module.xml. Why?
		#${CMAKE_CURRENT_SOURCE_DIR}/TSFUtil.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/verifyerrstring.c
		${CMAKE_CURRENT_SOURCE_DIR}/WritingCursor.cpp

		${CMAKE_CURRENT_BINARY_DIR}/ut_j9bcu.c
)
# Add current binarydir to include path in order to get the tracegen headers
target_include_directories(j9dyn_sources INTERFACE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(j9dyn_sources
	INTERFACE
	j9vm_interface
	j9vm_gc_includes

	j9shrcommon
)
# also add explicit dependency on running tracegen, because cmake wont pick this up outside of this dir
add_dependencies(j9dyn_sources trc_j9bcu)

add_library(j9dyn STATIC)
target_compile_definitions(j9dyn PRIVATE J9_INTERNAL_TO_VM)

target_include_directories(j9dyn
	PRIVATE
		${j9vm_SOURCE_DIR}/shared_common/include
		${omr_SOURCE_DIR}/gc/include
		${j9vm_SOURCE_DIR}/gc_include
)

target_link_libraries(j9dyn
	PRIVATE
		j9vm_interface
		j9dyn_sources

		j9hashtable
)

add_subdirectory(test/dyntest)
add_subdirectory(test/natives)
add_subdirectory(test/recompiled)
