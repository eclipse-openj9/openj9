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

add_library(j9vm_gc_glue INTERFACE)
target_sources(j9vm_gc_glue
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/ArrayletObjectModel.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ArrayletObjectModelBase.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/CollectorLanguageInterfaceImpl.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/CompactDelegate.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/CompactSchemeFixupObject.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/CompactSchemeFixupRoots.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ConcurrentMarkingDelegate.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ConcurrentSafepointCallbackJava.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/EnvironmentDelegate.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/GlobalCollectorDelegate.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/JNICriticalRegion.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/MarkingDelegate.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/MarkingSchemeRootClearer.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/MarkingSchemeRootMarker.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/MetronomeDelegate.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/MixedObjectModel.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ObjectModel.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ObjectModelDelegate.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ScavengerBackOutScanner.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ScavengerDelegate.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ScavengerRootClearer.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/ScavengerRootScanner.cpp
)
target_link_libraries(j9vm_gc_glue
	INTERFACE
		j9vm_interface
		j9vm_gc_includes
)
target_include_directories(j9vm_gc_glue
	INTERFACE
		${j9vm_SOURCE_DIR}/gc_modron_standard
		${j9vm_SOURCE_DIR}/gc_realtime
		${j9vm_SOURCE_DIR}/gc_trace
		${j9vm_SOURCE_DIR}/gc_vlhgc
)
add_library(j9vm_util_glue INTERFACE)
target_sources(j9vm_util_glue
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/UtilGlue.c
)
target_link_libraries(j9vm_util_glue
	INTERFACE
		j9vm_interface
)

add_library(j9vm_core_glue INTERFACE)
target_sources(j9vm_core_glue
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/LanguageVMGlue.c
)
target_link_libraries(j9vm_core_glue
	INTERFACE
		j9vm_interface
		j9vm_gc_includes
)

# Note: we dont actually need to add any glue for the vm or ras components
add_library(j9vm_vm_glue INTERFACE)
add_library(j9vm_ras_glue INTERFACE)
