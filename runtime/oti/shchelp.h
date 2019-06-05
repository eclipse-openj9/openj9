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
#ifndef SHCHELP_H
#define SHCHELP_H

/* @ddr_namespace: default */
#include "j9cfg.h"
#include "j9comp.h"
#include "j9port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define J9SH_MODLEVEL_JAVA5  1
#define J9SH_MODLEVEL_JAVA6  2
#define J9SH_MODLEVEL_JAVA7  3
#define J9SH_MODLEVEL_JAVA8  4
#define J9SH_MODLEVEL_JAVA9  5
/*
 * From Java 10, modLevel equals to java version number. But there might be Java 10 shared cache that has modLevel 6
 * created before this change. Define J9SH_MODLEVEL_JAVA10 to handle them
 */
#define J9SH_MODLEVEL_JAVA10  6

/*	JVM feature bit flag(s)	*/
/*
 * if there is a need for double digit features such as 10
 * version string length is going to increase by 1
 * J9SH_VERSION_STRING_LEN need to be changed accordingly *
 */
#define J9SH_FEATURE_DEFAULT 0x0
#define J9SH_FEATURE_COMPRESSED_POINTERS 0x1
#define J9SH_FEATURE_NON_COMPRESSED_POINTERS 0x2
#define J9SH_FEATURE_MAX_VALUE 0x2

#define J9SH_ADDRMODE_32 32
#define J9SH_ADDRMODE_64 64

#ifdef OMR_ENV_DATA64
#define J9SH_ADDRMODE J9SH_ADDRMODE_64
#else
#define J9SH_ADDRMODE J9SH_ADDRMODE_32
#endif

#define J9SH_VERSION_PREFIX_CHAR 'C'
#define J9SH_MODLEVEL_G07ANDLOWER_CHAR 'D'
#define J9SH_MODLEVEL_PREFIX_CHAR 'M'
#define J9SH_FEATURE_PREFIX_CHAR 'F'
#define J9SH_ADDRMODE_PREFIX_CHAR 'A'
#define J9SH_PERSISTENT_PREFIX_CHAR 'P'
#define J9SH_SNAPSHOT_PREFIX_CHAR 'S'
#define J9SH_PREFIX_SEPARATOR_CHAR '_'

#define J9SH_GENERATION_29		29
#define J9SH_GENERATION_07		7

#define J9SH_VERSION_STRING_G07ANDLOWER_SPEC "C%dD%dA%d"
#define J9SH_VERSION_STRING_G07TO29_SPEC "C%dM%dA%d"
#define J9SH_VERSION_STRING_SPEC "C%dM%dF%xA%d"
#define J9SH_VERSION_STRING_LEN 13
#define J9SH_VERSTRLEN_INCREASED_SINCEG29 2
#define J9SH_VERSTRLEN_INCREASED_SINCEJAVA10 1

#define J9SH_MODLEVEL_PREFIX_CHAR_OFFSET 4

typedef struct J9PortShcVersion {
    uint32_t esVersionMajor;
    uint32_t esVersionMinor;
    uint32_t modlevel;
    uint32_t addrmode;
    uint32_t cacheType;
    uint32_t feature;
} J9PortShcVersion;

#define J9PORT_SHR_CACHE_TYPE_PERSISTENT  1
#define J9PORT_SHR_CACHE_TYPE_NONPERSISTENT  2
#define J9PORT_SHR_CACHE_TYPE_VMEM  3
#define J9PORT_SHR_CACHE_TYPE_CROSSGUEST  4
#define J9PORT_SHR_CACHE_TYPE_SNAPSHOT  5

uintptr_t
getValuesFromShcFilePrefix(struct J9PortLibrary* portLibrary, const char* filename, struct J9PortShcVersion* versionData);

uint32_t
getShcModlevelForJCL(uintptr_t j2seVersion);

uint32_t
getJCLForShcModlevel(uintptr_t modlevel);

uintptr_t
isCompatibleShcFilePrefix(J9PortLibrary* portlib, uint32_t javaVersion, uint32_t feature, const char* filename);

void
getStringForShcModlevel(J9PortLibrary* portlib, uint32_t modlevel, char* buffer, uint32_t buffSize);

void
getStringForShcAddrmode(J9PortLibrary* portlib, uint32_t addrmode, char* buffer);

uintptr_t
isCacheFileName(J9PortLibrary* portlib, const char* nameToTest, uintptr_t expectPersistent, const char* optionalExtraID);

intptr_t getModLevelFromName(const char* cacheNameWithVGen);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* SHCHELP_H */
