/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef ROMCOOKIE_H
#define ROMCOOKIE_H

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C" {
#endif

#include "j9comp.h"

#define J9_ROM_CLASS_COOKIE_SIG { 0x4A, 0x39, 0x52, 0x4F, 0x4D, 0x43, 0x4C, 0x41, 0x53, 0x53, 0x43, 0x4F, 0x4F, 0x4B, 0x49, 0x45 }
/* CMVC 123277: create obfuscated magic number which can be used to prevent cookies being created in the java space */
#if defined(J9VM_OPT_SHARED_CLASSES)
#define J9_ROM_CLASS_COOKIE_MAGIC(javaVM, romClass) ((UDATA)((romClass->className << 8) | romClass->superclassName) ^ ~(UDATA)javaVM->sharedClassConfig->jclStringFarm)
#endif

#define COOKIE_SIG_LENGTH 16

typedef struct J9ROMClassCookie {
    U_8 signature[COOKIE_SIG_LENGTH];
    U_32 version;
    U_32 type;
} J9ROMClassCookie;


#define J9_ROM_CLASS_COOKIE_SIG_LENGTH  COOKIE_SIG_LENGTH
#define J9_ROM_CLASS_COOKIE_TYPE_SHARED_CLASS  5
#define J9_ROM_CLASS_COOKIE_VERSION  2

typedef struct J9ROMClassCookieSharedClass {
    U_8 signature[16];
    U_32 version;
    U_32 type;
    struct J9ROMClass* romClass;
    UDATA magic;
} J9ROMClassCookieSharedClass;

#ifdef __cplusplus
}
#endif

#endif /* ROMCOOKIE_H */
