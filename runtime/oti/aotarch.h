/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#ifndef AOTARCH_H
#define AOTARCH_H

#ifdef __cplusplus
extern "C" {
#endif

#define J9_AOT_ARCHITECTURE_TYPE_MASK 0xFFFF0000
#define J9_AOT_ARCHITECTURE_TYPE_IA32 0x00010000
#define J9_AOT_ARCHITECTURE_TYPE_PPC  0x00020000
#define J9_AOT_ARCHITECTURE_TYPE_ARM  0x00040000
#define J9_AOT_ARCHITECTURE_TYPE_S390 0x00200000
#define J9_AOT_OS_TYPE_MASK 	0x0000FFFF
#define J9_AOT_OS_TYPE_WIN 		0x00000001
#define J9_AOT_OS_TYPE_LINUX 	0x00000002
#define J9_AOT_OS_TYPE_BREW 	0x00000004
#define J9_AOT_OS_TYPE_AIX 		0x00000010
#define J9_AOT_OS_TYPE_OSE 		0x00000020
#define J9_AOT_OS_TYPE_ZOS 		0x00000080

/* Determine the supported architecture */
#if defined(J9VM_ARCH_X86)
#define J9_AOT_ARCHITECTURE_HOST J9_AOT_ARCHITECTURE_TYPE_IA32
#endif
#if defined(J9VM_ARCH_POWER)
#define J9_AOT_ARCHITECTURE_HOST J9_AOT_ARCHITECTURE_TYPE_PPC
#endif
#if defined(J9VM_ARCH_ARM)
#define J9_AOT_ARCHITECTURE_HOST J9_AOT_ARCHITECTURE_TYPE_ARM
#endif
#if defined(J9VM_ARCH_S390)
#define J9_AOT_ARCHITECTURE_HOST J9_AOT_ARCHITECTURE_TYPE_S390
#endif

/* Determine the supported OS */
#if defined(WIN32)
#define J9_AOT_OS_HOST J9_AOT_OS_TYPE_WIN
#endif
#if defined(AIXPPC)
#define J9_AOT_OS_HOST J9_AOT_OS_TYPE_AIX
#endif
#if defined(LINUX)
#define J9_AOT_OS_HOST J9_AOT_OS_TYPE_LINUX
#endif
#if defined(BREW)
#define J9_AOT_OS_HOST J9_AOT_OS_TYPE_BREW
#endif
#if defined(J9ZOS390)
#define J9_AOT_OS_HOST J9_AOT_OS_TYPE_ZOS
#endif


#ifdef __cplusplus
}
#endif

#endif /* AOTARCH_H */
