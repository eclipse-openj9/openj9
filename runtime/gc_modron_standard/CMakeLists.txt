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

set(gc_modron_standard_sources
	ConcurrentSweepGC.cpp
	ContinuationObjectBufferStandard.cpp
	OwnableSynchronizerObjectBufferStandard.cpp
	ReadBarrierVerifier.cpp
	ReferenceObjectBufferStandard.cpp
	RootScannerReadBarrierVerifier.cpp
	StandardAccessBarrier.cpp
	UnfinalizedObjectBufferStandard.cpp
)

j9vm_add_library(j9modronstandard STATIC
	${gc_modron_standard_sources}
)

target_link_libraries(j9modronstandard
	PRIVATE
		j9vm_interface
		j9vm_gc_includes

		omrgc
		j9gcbase
)

if(OMR_MIXED_REFERENCES_MODE_STATIC)
	j9vm_add_library(j9modronstandard_full STATIC
		${gc_modron_standard_sources}
	)

	target_link_libraries(j9modronstandard_full
		PRIVATE
			j9vm_interface
			j9vm_gc_includes

			omrgc_full
			j9gcbase_full
	)
endif()
