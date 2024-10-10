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

set(gcchk_sources
	gcchk.cpp
)

j9vm_add_library(j9gcchk SHARED
	OUTPUT_NAME j9gcchk${J9VM_VERSION_SUFFIX}
	${gcchk_sources}
)

if(DEFINED OMR_OS_AIX AND DEFINED CMAKE_C_COMPILER_IS_OPENXL)
	target_link_libraries(j9gcchk
		PRIVATE
			j9vm_interface
			j9util
			j9utilcore
			j9avl
			j9hashtable
			j9thr
			j9stackmap
			j9pool
			j9hookable
			j9modronstartup

			#TODO do we just want to link against j9gc?
			j9gcbase
			j9gcstructs
			j9gcstats
			j9gccheck
			omrgc
			j9gctrc
			j9gctrcstandard
			j9gctrcvlhgc
			j9gc
	)
else()
	target_link_libraries(j9gcchk
		PRIVATE
			j9vm_interface
			j9util
			j9utilcore
			j9avl
			j9hashtable
			j9thr
			j9stackmap
			j9pool
			j9hookable

			#TODO do we just want to link against j9gc?
			j9gcbase
			j9gcstructs
			j9gcstats
			j9gccheck
			omrgc
	)
endif()

omr_add_exports(j9gcchk J9VMDllMain)

install(
	TARGETS j9gcchk
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)

if(OMR_MIXED_REFERENCES_MODE_STATIC)
	j9vm_add_library(j9gcchk_full SHARED
		OUTPUT_NAME j9gcchk_full${J9VM_VERSION_SUFFIX}
		${gcchk_sources}
	)

	if(DEFINED OMR_OS_AIX AND DEFINED CMAKE_C_COMPILER_IS_OPENXL)
		target_link_libraries(j9gcchk_full
			PRIVATE
				j9vm_interface
				j9util
				j9utilcore
				j9avl
				j9hashtable
				j9thr
				j9stackmap
				j9pool
				j9hookable
				j9modronstartup

				#TODO do we just want to link against j9gc?
				j9gcbase_full
				j9gcstructs_full
				j9gcstats_full
				j9gccheck_full
				omrgc_full
				j9gctrc_full
				j9gctrcstandard_full
				j9gctrcvlhgc_full
				j9gc_full
		)
	else()
		target_link_libraries(j9gcchk_full
			PRIVATE
				j9vm_interface
				j9util
				j9utilcore
				j9avl
				j9hashtable
				j9thr
				j9stackmap
				j9pool
				j9hookable

				#TODO do we just want to link against j9gc?
				j9gcbase_full
				j9gcstructs_full
				j9gcstats_full
				j9gccheck_full
				omrgc_full
		)
	endif()

	omr_add_exports(j9gcchk_full J9VMDllMain)

	install(
		TARGETS j9gcchk_full
		LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
		RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
	)
endif()
