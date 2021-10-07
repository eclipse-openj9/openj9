################################################################################
# Copyright (c) 2021, 2021 IBM Corp. and others
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

set(OMR_ENHANCED_WARNINGS OFF)
j9vm_add_library(clinkerffitests SHARED
	downcall.c
)
target_link_libraries(clinkerffitests
	PRIVATE
		j9vm_interface
		jvm
)

omr_add_exports(clinkerffitests
	add2Ints
	add3Ints
	addIntAndChar
	add2IntsReturnVoid
	add2BoolsWithOr
	createNewCharFrom2Chars
	add2Bytes
	add2Shorts
	add2Longs
	add2Floats
	add2Doubles
	addIntsFromVaList
	addLongsFromVaList
	addDoublesFromVaList
	addIntAndIntFromPointer
	addBoolAndBoolFromPointerWithOr
	createNewCharFromCharAndCharFromPointer
	addByteAndByteFromPointer
	addShortAndShortFromPointer
	addLongAndLongFromPointer
	addFloatAndFloatFromPointer
	addDoubleAndDoubleFromPointer
)

install(
	TARGETS clinkerffitests
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
