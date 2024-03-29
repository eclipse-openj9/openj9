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

j9vm_add_library(jsig SHARED
	jsig.c
)

target_link_libraries(jsig
	PRIVATE
		j9vm_interface

		omrutil
		${CMAKE_DL_LIBS}
)

target_include_directories(jsig PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

omr_add_exports(jsig
	signal
	jsig_handler
	jsig_primary_signal
)
if(NOT OMR_OS_WINDOWS)
	omr_add_exports(jsig
		sigaction
		sigset
		sigignore
		bsd_signal
		jsig_primary_sigaction
	)
	if(NOT OMR_OS_ZOS)
		omr_add_exports(jsig
			sysv_signal
		)
	endif()
endif()
if(OMR_OS_LINUX)
	omr_add_exports(jsig
		__sysv_signal
		ssignal
	)
endif()

install(
	TARGETS jsig
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
