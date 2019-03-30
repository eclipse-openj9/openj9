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


# We dont want to suffix vm version to bcuwhite_test
set(CMAKE_SHARED_LIBRARY_SUFFIX ${J9VM_OLD_SHARED_SUFFIX})

add_library(bcuwhite_test SHARED
	bcunatives.c
)

target_link_libraries(bcuwhite_test
	PRIVATE
		j9vm_interface

		j9dyn_generic_test
		j9verutil
		j9util
		j9utilcore
		j9avl
		j9hashtable
		j9thr
		j9stackmap
		j9simplepool
		j9pool

		#IF opt_zip_support
		j9hookable
		j9vmi
		#endif
		j9bcv
)

omr_add_exports(bcuwhite_test Java_com_ibm_j9_test_bcutil_TestNatives_verifyCanonisizeAndCopyUTF8)
