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

add_tracegen(hshelp.tdf j9hshelp)
add_tracegen(util.tdf j9util)
add_tracegen(vmutil.tdf j9vmutil)
add_tracegen(module.tdf)
add_tracegen(srphashtable.tdf)

j9vm_gen_asm(fpusup.m4)

add_library(j9util STATIC
	alignedmemcpy.c
	annhelp.c
	argbits.c
	argcount.c
	bcdump.c
	bcnames.c
	binarysup.c
	checkcast.cpp
	cphelp.c
	cpplink.c
	defarg.c
	divhelp.c
	divrem.c
	eventframe.c
	extendedHCR.c
	extendedmethodblockaccess.c
	fieldutil.c
	filecache.c
	final.c
	fltconv.c
	fltdmath.c
	fltmath.c
	fltodd.c
	fltrem.c
	genericSignalHandler.c
	hshelp.c
	j9crc32.c
	j9fptr.c
	j9list.c
	jbcmap.c
	jitfloat.c
	jitlook.c
	jitregs.c
	jitresolveframe.c
	jlm.c
	jnierrhelp.c
	jniprotect.c
	modularityHelper.c
	moninfo.c
	mthutil.c
	ObjectHash.cpp
	optinfo.c
	pcstack.c
	pkgname.c
	propsfile.c
	rcdump.c
	resolvehelp.c
	returntype.c
	romclasswalk.c
	romhelp.c
	sendslot.c
	shchelp_j9.c
	sleephelp.c
	srphashtable.c
	strhelp.c
	subclass.c
	sunbcrel.c
	superclass.c
	thrhelp.c
	thrinfo.c
	thrname.c
	tracehelp.c
	utf8hash.c
	vmargs.c
	vmihelp.c
	vmstate.c
	VMThreadFlags.cpp
	vrfytbl.c
	wildcard.c

	${CMAKE_CURRENT_BINARY_DIR}/ut_j9hshelp.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9util.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9vmutil.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_module.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_srphashtable.c
)

if(OMR_OS_LINUX)
	target_sources(j9util PRIVATE linux/osinfo.c)
elseif(OMR_OS_WINDOWS)
	target_sources(j9util PRIVATE win32/openlibrary.c)
endif()

if(OMR_ARCH_X86)
	target_sources(j9util
		PRIVATE
			fpusup.s
	)
endif()

target_include_directories(j9util
	PUBLIC
	${CMAKE_CURRENT_BINARY_DIR}
	.
)

target_link_libraries(j9util
	PRIVATE
		j9vm_interface
		j9vm_gc_includes

		j9simplepool
		j9stackmap
)
