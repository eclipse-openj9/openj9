################################################################################
# Copyright (c) 2017, 2021 IBM Corp. and others
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

set(gc_verbose_old_sources
	VerboseEventStream.cpp
	VerboseFileLoggingOutput.cpp
	VerboseManagerOld.cpp
	VerboseOutputAgent.cpp
	VerboseStandardStreamOutput.cpp
	VerboseTraceOutput.cpp
)

j9vm_add_library(j9gcvrbold STATIC
	${gc_verbose_old_sources}
)

target_include_directories(j9gcvrbold
	PRIVATE
		${j9vm_SOURCE_DIR}/gc_verbose_old_events
)

target_link_libraries(j9gcvrbold
	PRIVATE
		j9vm_interface

		omrgc
)

if(OMR_MIXED_REFERENCES_MODE_STATIC)
	j9vm_add_library(j9gcvrbold_full STATIC
		${gc_verbose_old_sources}
	)

	target_include_directories(j9gcvrbold_full
		PRIVATE
			${j9vm_SOURCE_DIR}/gc_verbose_old_events
	)

	target_link_libraries(j9gcvrbold_full
		PRIVATE
			j9vm_interface

			omrgc_full
	)
endif()
