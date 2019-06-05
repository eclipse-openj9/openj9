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

add_library(j9zlib SHARED
	adler32.c
	compress.c
	crc32.c
	deflate.c
	infback.c
	inffast.c
	inflate.c
	inftrees.c
	trees.c
	uncompr.c
	zutil.c

)

target_include_directories(j9zlib
	PUBLIC
		.
)

target_link_libraries(j9zlib
	PRIVATE
		j9vm_interface
)

omr_add_exports(j9zlib
	j9zlib_adler32
	j9zlib_compress
	j9zlib_compress2
	j9zlib_crc32
	j9zlib_deflate
	j9zlib_deflateCopy
	j9zlib_deflateEnd
	j9zlib_deflateInit_
	j9zlib_deflateInit2_
	j9zlib_deflateParams
	j9zlib_deflateReset
	j9zlib_deflateSetDictionary
	j9zlib_get_crc_table
	j9zlib_inflate
	j9zlib_inflateCopy
	j9zlib_inflateEnd
	j9zlib_inflateInit_
	j9zlib_inflateInit2_
	j9zlib_inflateReset
	j9zlib_inflateSetDictionary
	j9zlib_inflateSync
	j9zlib_inflateSyncPoint
	j9zlib_uncompress
	j9zlib_zError
	j9zlib_zlibVersion
)

install(
	TARGETS j9zlib
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
