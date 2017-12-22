/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef jimage_reader_h
#define jimage_reader_h

#define ROUND_UP_TO(granularity, number) 			((((number) % (granularity)) ? ((number) + (granularity) - ((number) % (granularity))) : (number)))

#define J9JIMAGE_READ_U8(value, cursor)				((value = *(U_8 *)cursor), cursor += sizeof(U_8), value)

#define J9JIMAGE_READ_U16_BIGENDIAN(value, cursor)	((value = ((U_16)(cursor)[0] << 8) | ((U_16)(cursor)[1])), cursor += 2, value)
#define J9JIMAGE_READ_U24_BIGENDIAN(value, cursor)	((value = ((U_32)(cursor)[0] << 16) | ((U_32)(cursor)[1] << 8) | (U_32)(cursor)[2]), cursor += 3, value)
#define J9JIMAGE_READ_U32_BIGENDIAN(value, cursor)	((value = ((U_32)(cursor)[0] << 24) | ((U_32)(cursor)[1] << 16) | ((U_32)(cursor)[2] << 8) | (U_32)(cursor)[3]), cursor += 4, value)

#define J9JIMAGE_READ_BYTES_BIGENDIAN(value, cursor, numBytes) \
	do { \
		I_32 i = 0; \
		value = 0; \
		for (i = 0; i < numBytes; i++) { \
			value = value | (cursor[i] << (numBytes - (8 * (i + 1)))); \
		} \
		cursor += numBytes; \
	} while (0);


/**
 * Error codes used by functions in jimagereader.c
 */
#define J9JIMAGE_NO_ERROR 0

/* File based error(s) -1 to -10 */
#define J9JIMAGE_FILE_OPEN_ERROR -1
#define J9JIMAGE_FILE_LENGTH_ERROR -2
#define J9JIMAGE_FILE_READ_ERROR -3
#define J9JIMAGE_FILE_SEEK_ERROR -4

/* Memory allocation error(s) -11 to -20 */
#define J9JIMAGE_OUT_OF_MEMORY -11

/* Resource lookup error(s) -21 to -30 */
#define J9JIMAGE_RESOURCE_NOT_FOUND -21
#define J9JIMAGE_INVALID_LOT_INDEX -22
#define J9JIMAGE_INVALID_LOCATION_OFFSET -23
#define J9JIMAGE_MODULE_METAINFO_LOOKUP_FAILED -24
#define J9JIMAGE_MODULE_METAINFO_RESOURCE_FAILED -25
#define J9JIMAGE_RESOURCE_TRUNCATED -26

/* Invalid jimage structure error(s) -31 to -40 */
#define J9JIMAGE_INVALID_HEADER -31
#define J9JIMAGE_INVALID_MODULE_OFFSET -32
#define J9JIMAGE_INVALID_PARENT_OFFSET -33
#define J9JIMAGE_INVALID_BASE_OFFSET -34
#define J9JIMAGE_INVALID_EXTENSION_OFFSET -35
#define J9JIMAGE_INVALID_RESOURCE_OFFSET -36
#define J9JIMAGE_INVALID_COMPRESSED_SIZE -37
#define J9JIMAGE_INVALID_UNCOMPRESSED_SIZE -38
#define J9JIMAGE_BAD_MAGIC -39
#define J9JIMAGE_BAD_VERSION -40

/* ImageLocation verification error(s) -41 to -50 */
#define J9JIMAGE_LOCATION_VERIFICATION_FAIL -41

/* Resource lookup error in module metadata -51 to -60 */
#define J9JIMAGE_PACKAGE_NOT_FOUND -51
#define J9JIMAGE_INVALID_P2M_TABLE_INDEX -52
#define J9JIMAGE_PACKAGE_NAME_MISMATCH -53
#define J9JIMAGE_MODULE_NOT_FOUND -54
#define J9JIMAGE_INVALID_M2P_TABLE_INDEX -55
#define J9JIMAGE_MODULE_NAME_MISMATCH -56

/* Decompressor Errors -61 to -80 */
#define J9JIMAGE_DECOMPRESSOR_INVALID -61
#define J9JIMAGE_DECOMPRESSOR_NOT_SUPPORTED -62

/* Miscellaneous error(s) -91 onwards*/
#define J9JIMAGE_INVALID_PARAMETER -91
#define J9JIMAGE_MAP_FAILED -92
#define J9JIMAGE_CORRUPTED -93
#define J9JIMAGE_INTERNAL_ERROR -94
#define J9JIMAGE_LIBJIMAGE_LOAD_FAILED -95

#define J9JIMAGE_UNKNOWN_ERROR -999

#endif /* jimage_reader_h */
