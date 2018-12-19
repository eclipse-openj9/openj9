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

add_executable(shrtest
	AOTDataMinMaxTest.cpp
	AttachedDataMinMaxTest.cpp
	AttachedDataTest.cpp
	ByteDataTest.cpp
	CacheDirPerm.cpp
	CacheFullTests.cpp
	CacheMapTest.cpp
	ClassDebugDataTests.cpp
	ClasspathCacheTest.cpp
	ClasspathItemTest.cpp
	CompiledMethodTest.cpp
	CompositeCacheSizesTests.cpp
	CompositeCacheTest.cpp
	CorruptCacheTest.cpp
	OpenCacheHelper.cpp
	OSCacheTest.cpp
	OSCacheTestMisc.cpp
	OSCacheTestMmap.cpp
	OSCacheTestSysv.cpp
	ProcessHelper.cpp
	ProtectNewROMClassDataTests.cpp
	ProtectSharedCacheDataTests.cpp
	SCStoreTransactionTests.cpp
	SCStoreWithBCITests.cpp
	SCStringTransactionTests.cpp
	SCTestCommon.cpp
	SharedCacheAPITest.cpp
	StartupHintsTest.cpp
	main.c
)

target_link_libraries(shrtest
	PRIVATE
		j9vm_interface
		j9vm_gc_includes
		j9vm_main_wrapper

		j9util
		j9utilcore
		j9avl
		j9hashtable
		j9pool
		j9simplepool
		j9thr
		j9prt
		j9shrcommon
		j9shrutil
		j9exelib
		j9util
		j9utilcore
		j9zip
		j9zlib
		j9hookable
)

target_compile_definitions(shrtest
	PRIVATE
		-DJ9VM_SHRTEST
)

add_library(SharedClassesNativeAgent SHARED
	jvmti/SharedClassesNativeAgent.c
)

target_link_libraries(SharedClassesNativeAgent 
	PRIVATE
		j9vm_interface

		j9prt
)

install(
	TARGETS shrtest SharedClassesNativeAgent
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
)
