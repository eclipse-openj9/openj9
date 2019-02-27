/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
#include <string.h>
#include "j9cfg.h"
#include "shchelp.h"
#include "omrutil.h"
#include "j9argscan.h"
#include "j9port.h"
#include "j2sever.h"

/**
 * Given a cache filename, populates a J9PortShcVersion struct with the version information
 * 
 * @param [in] portLibrary  A port library
 * @param [in] filename  The filename to parse
 * @param [out] result  The result struct to populate
 * 
 * @return 1 if the parsing succeeded, 0 otherwise
 */
uintptr_t
getValuesFromShcFilePrefix(struct J9PortLibrary* portLibrary, const char* filename, struct J9PortShcVersion* result) 
{
	if (filename != NULL) {
		uintptr_t temp;
		char* cursor = (char*)filename;

		if (*cursor == J9SH_VERSION_PREFIX_CHAR) {
			++cursor;
			if (scan_udata(&cursor, &temp) == 0) {
				result->esVersionMinor = (uint32_t)(temp % 100);
				result->esVersionMajor = (uint32_t)((temp - result->esVersionMinor) / 100);
			} else {
				return 0;
			}
		} else {
			return 0;
		}
		if ((*cursor == J9SH_MODLEVEL_PREFIX_CHAR) || (*cursor == J9SH_MODLEVEL_G07ANDLOWER_CHAR)) {
			++cursor;
			if (scan_udata(&cursor, &temp) == 0) {
				result->modlevel = (uint32_t)temp;
			} else {
				return 0;
			}
		} else {
			return 0;
		}
		if (*cursor == J9SH_FEATURE_PREFIX_CHAR) {
			++cursor;
			if (scan_hex_caseflag(&cursor, FALSE, &temp) == 0) {
				result->feature = (uint32_t)temp;
			} else {
				return 0;
			}
		} else {
			result->feature = 0;
		}
		if (*cursor == J9SH_ADDRMODE_PREFIX_CHAR) {
			++cursor;
			if (scan_udata(&cursor, &temp) == 0) {
				result->addrmode = (uint32_t)temp;
			} else {
				return 0;
			}
		} else {
			return 0;
		}
		if (*cursor == J9SH_PERSISTENT_PREFIX_CHAR) {
			result->cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
			++cursor;
		} else if (J9SH_SNAPSHOT_PREFIX_CHAR == *cursor) {
			result->cacheType = J9PORT_SHR_CACHE_TYPE_SNAPSHOT;
			++cursor;
		} else {
			result->cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
		}
		if (*cursor != J9SH_PREFIX_SEPARATOR_CHAR) {
			return 0;
		}
	} else {
		return 0;
	}
	return 1;
}

/**
 * Return the value of the generation from the cache filename
 *
 * @param [in] cacheNameWithVGen  the cache name with generation number
 *
 * @return the generation number or 0 if an error occurred
 */
uintptr_t
getGenerationFromName(const char* cacheNameWithVGen)
{
	char* cursor = (char*)cacheNameWithVGen;
	uintptr_t genValue;

	if ((cursor = strrchr(cursor, J9SH_PREFIX_SEPARATOR_CHAR)) == NULL) {
		return 0;
	}
	if (cursor[1] != 'G') {
		return 0;
	}
	cursor += strlen("_G");
	if (scan_udata(&cursor, &genValue) == 0) {
		return genValue;
	} else {
		return	0;
	}
}

/**
 * Return the value of the modLevel from the cache filename
 *
 * @param [in] cacheNameWithVGen  the cache file name
 *
 * @return the modeLevel number,
 * 		   -1 if it is an old cache name that does not have modlLevel, or an error occurred.
 */
intptr_t
getModLevelFromName(const char* cacheNameWithVGen)
{
	char* cursor = (char*)cacheNameWithVGen;
	intptr_t modLevel = -1;

	if ((strlen(cacheNameWithVGen) > (J9SH_MODLEVEL_PREFIX_CHAR_OFFSET + 2))
		 && (J9SH_MODLEVEL_PREFIX_CHAR == cursor[J9SH_MODLEVEL_PREFIX_CHAR_OFFSET])
	) {
		cursor += (J9SH_MODLEVEL_PREFIX_CHAR_OFFSET + 1);
		if (0 != scan_idata(&cursor, &modLevel)) {
			modLevel = -1;
		}
	}
	return modLevel;
}

/**
 * Return the modification level for a given j2se version
 * 
 * @param [in] j2seVersion  The j2se version
 * 
 * @return the modification level or 0 if one cannot be found
 */
uint32_t
getShcModlevelForJCL(uintptr_t j2seVersion)
{
	uint32_t modLevel = 0;
	switch (j2seVersion) {
	case J2SE_18 :
		modLevel = J9SH_MODLEVEL_JAVA8;
		break;
	default: 
		modLevel = JAVA_SPEC_VERSION_FROM_J2SE(j2seVersion);
		break;
	}
	return modLevel;
}

/**
 * Return the java spec version for a given modification level
 * @param [in] modlevel  The modification level
 * 
 * @return the j2se version or 0 if one cannot be found
 */
uint32_t
getJCLForShcModlevel(uintptr_t modlevel)
{
	uint32_t javaVersion = 0;
	switch (modlevel) {
	case J9SH_MODLEVEL_JAVA5 :
		javaVersion = 5;
		break;
	case J9SH_MODLEVEL_JAVA6 :
		javaVersion = 6;
		break;
	case J9SH_MODLEVEL_JAVA7 :
		javaVersion = 7;
		break;
	case J9SH_MODLEVEL_JAVA8 :
		javaVersion = 8;
		break;
	case J9SH_MODLEVEL_JAVA9 :
		javaVersion = 9;
		break;
	case J9SH_MODLEVEL_JAVA10:
		/* From Java 10, modLevel equals to java version number. But there might be Java 10 shared cache that has modLevel 6
		 * created before this change.
		 */
		javaVersion = 10;
		break;
	default:
		if (modlevel >= 10) {
			/* J9SH_MODLEVEL_JAVA9 is 5. Does not have modlevel that is 7,8,9 */
			javaVersion = modlevel;
		}
		break;
	}
	return javaVersion;
}

/**
 * Given a cache filename and a j2se version, is the filename compatible with this running JVM
 * 
 * @param [in] portlib  A port library
 * @param [in] javaVersion  The java version the JVM is running
 * @param [in] feature  this running JVM feature
 * @param [in] filename  The cache filename
 * 
 * @return 1 if the cache is compatible or 0 otherwise
 */
uintptr_t 
isCompatibleShcFilePrefix(J9PortLibrary* portlib, uint32_t javaVersion, uint32_t feature, const char* filename)
{
	J9PortShcVersion versionData;
	uint32_t jclLevel;
	
	getValuesFromShcFilePrefix(portlib, filename, &versionData);
	jclLevel = getJCLForShcModlevel(versionData.modlevel);

	if ((versionData.esVersionMajor == EsVersionMajor) &&
			(versionData.esVersionMinor == EsVersionMinor) &&
			(jclLevel == javaVersion) &&
			(versionData.addrmode == J9SH_ADDRMODE) &&
			(versionData.feature == feature)) {
		return 1;
	}
	return 0;
}

/**
 * Get the string for a given j2se version
 * 
 * @param [in] portlib  A port library
 * @param [in] modlevel  The modification level
 * @param [out] buffer  The buffer to copy the string into
 */
void
getStringForShcModlevel(J9PortLibrary* portlib, uint32_t modlevel, char* buffer, uint32_t buffSize)
{
	PORT_ACCESS_FROM_PORT(portlib);
	switch (modlevel) {
	case J9SH_MODLEVEL_JAVA5 :
		strncpy(buffer, "Java5", buffSize);
		break;
	case J9SH_MODLEVEL_JAVA6 :
		strncpy(buffer, "Java6", buffSize);
		break;
	case J9SH_MODLEVEL_JAVA7 :
		strncpy(buffer, "Java7", buffSize);
		break;
	case J9SH_MODLEVEL_JAVA8 :
		strncpy(buffer, "Java8", buffSize);
		break;
	case J9SH_MODLEVEL_JAVA9 :
		strncpy(buffer, "Java9", buffSize);
		break;
	case J9SH_MODLEVEL_JAVA10 :
		/* From Java 10, modLevel equals to java version number. But there might be Java 10 shared cache that has modLevel 6
		 * created before this change.
		 */
		strncpy(buffer, "Java10", buffSize);
		break;
	default :
		if (modlevel >= 10) {
			j9str_printf(portlib, buffer, buffSize, "%s%u", "Java", modlevel);
		} else {
			/* J9SH_MODLEVEL_JAVA9 is 5. Does not have modlevel that is 7,8,9 */
			strncpy(buffer, "Unknown", buffSize);
		}
		break;
	}
}

/**
 * Get a string describing the address mode of the JVM 
 * 
 * @param [in] portlib  A port library
 * @param [in] addrmode  The address mode of the JVM
 * @param [out] buffer  The buffer to copy the string into
 */
void
getStringForShcAddrmode(J9PortLibrary* portlib, uint32_t addrmode, char* buffer)
{
	switch (addrmode) {
	case J9SH_ADDRMODE_32 :
		strcpy(buffer, "32-bit");
		break;
	case J9SH_ADDRMODE_64 :
		strcpy(buffer, "64-bit");
		break;
	default :
		strcpy(buffer, "Unknown");
		break;
	}
}

/**
 * Test to see whether a given filename string is a cache file or not
 * 
 * @param [in] portlib  A port library
 * @param [in] nameToTest  The cache name to test
 * @param [in] expectCacheType  Cache type expected
 * @param [in] optionalExtraID  Some control files have an extra ID - if this parameter is not NULL, this is also checked
 * 
 * @return 1 if the filename is a cache, or 0 otherwise 
 */
uintptr_t
isCacheFileName(J9PortLibrary* portlib, const char* nameToTest, uintptr_t expectCacheType, const char* optionalExtraID)
{
	J9PortShcVersion versionData;
	intptr_t nameToTestLen = 0;
	intptr_t expectedVersionLen = ((expectCacheType == J9PORT_SHR_CACHE_TYPE_PERSISTENT) || (J9PORT_SHR_CACHE_TYPE_SNAPSHOT == expectCacheType)) ? J9SH_VERSION_STRING_LEN + 1 : J9SH_VERSION_STRING_LEN;
	uintptr_t generation = getGenerationFromName(nameToTest);
	intptr_t modLevel = 0;

	/*
	 * cache names generated by earlier JVMs (generation <=29) don't have feature prefix char 'F' and the value
	 * adjust versionLen to parse cacheNameWithVGen generated by earlier JVMs according to generation number
	 */
	if (generation <= J9SH_GENERATION_29) {
		expectedVersionLen -= J9SH_VERSTRLEN_INCREASED_SINCEG29;
	}
	if (nameToTest == NULL) {
		return 0;
	}
	modLevel = getModLevelFromName(nameToTest);

	/* modLevel becomes 2 digits from Java 10 */
	if (modLevel < 10) {
		expectedVersionLen -= J9SH_VERSTRLEN_INCREASED_SINCEJAVA10;
	}
	if (optionalExtraID != NULL) {
		const char* temp1 = strstr(nameToTest, optionalExtraID);
		const char* temp2 = nameToTest + (expectedVersionLen - 1);
		if (temp1 != temp2) {
			return 0;
		}
	}
	nameToTestLen = strlen(nameToTest);
	if ((nameToTest[nameToTestLen-3] != 'G') && (nameToTest[nameToTestLen-4] != '_')) {
		return 0;
	}
	if (getValuesFromShcFilePrefix(portlib, nameToTest, &versionData) != 0) {
		if (versionData.feature > J9SH_FEATURE_MAX_VALUE) {
			return 0;
		}
		if (versionData.cacheType == expectCacheType) {
			return 1;
		}
	}
	return 0;	
}
