/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#ifndef j9arch_h
#define j9arch_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(RS6000) || defined(LINUXPPC)
#ifdef PPC64
#ifdef J9VM_ENV_LITTLE_ENDIAN
#define OPENJ9_ARCH_DIR "ppc64le"
#else /* J9VM_ENV_LITTLE_ENDIAN */
#define OPENJ9_ARCH_DIR "ppc64"
#endif /* J9VM_ENV_LITTLE_ENDIAN */
#else
#define OPENJ9_ARCH_DIR "ppc"
#endif /* PPC64*/
#elif defined(J9X86) || defined(WIN32)
#define OPENJ9_ARCH_DIR "i386"
#elif defined(S390) || defined(J9ZOS390)
#if defined(S39064) || defined(J9ZOS39064)
#define OPENJ9_ARCH_DIR "s390x"
#else
#define OPENJ9_ARCH_DIR "s390"
#endif /* S39064 || J9ZOS39064 */
#elif defined(J9HAMMER)
#define OPENJ9_ARCH_DIR "amd64"
#elif defined(J9ARM)
#define OPENJ9_ARCH_DIR "arm"
#elif defined(J9AARCH64)
#define OPENJ9_ARCH_DIR "aarch64"
#elif defined(RISCV64)
#define OPENJ9_ARCH_DIR "riscv64"
#else
#error "Must define an architecture"
#endif

#define OPENJ9_CR_JVM_DIR "compressedrefs"
#define OPENJ9_NOCR_JVM_DIR "default"

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* j9arch_h */
