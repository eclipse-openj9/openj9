/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

#ifndef J9CFG_BUILDER_H
#define J9CFG_BUILDER_H

#include "j9cfg.h"

/* The following define specifies what type of locks are used by this VM */
#define J9VM_TASUKI_LOCKS_SINGLE_SLOT

/* The following define specifies whether or not an instruction to sample the CPU timestamp is supported */
#if defined(J9VM_ARCH_X86) || defined(J9VM_ARCH_POWER)
#define J9VM_CPU_TIMESTAMP_SUPPORT
#endif /* J9VM_ARCH_X86 || J9VM_ARCH_POWER */

/* The default memory sizes */

#define J9_GC_TLH_SIZE (4 * 1024)
#define J9_GC_TLH_THRESHOLD 1024
#define J9_GC_JNI_ARRAY_CACHE_SIZE (128 * 1024)
#define J9_MEMORY_MAX (256 * 1024 * 1024)
#define J9_FINALIZABLE_INTERVAL -2

#define J9_ALLOCATION_INCREMENT_NUMERATOR 125
#define J9_ALLOCATION_INCREMENT_DENOMINATOR 1000
#define J9_ALLOCATION_INCREMENT_MIN (1 * 1024 * 1024)
#define J9_ALLOCATION_INCREMENT_MAX (8 * 1024 * 1024)

#define J9_RAM_CLASS_ALLOCATION_INCREMENT_NUMERATOR 1
#define J9_RAM_CLASS_ALLOCATION_INCREMENT_DENOMINATOR 1024
#define J9_RAM_CLASS_ALLOCATION_INCREMENT_MIN (16 * 1024)
#define J9_RAM_CLASS_ALLOCATION_INCREMENT_MAX (32 * 1024)
#define J9_RAM_CLASS_ALLOCATION_INCREMENT_ROUND_TO 1024

#define J9_ROM_CLASS_ALLOCATION_INCREMENT_NUMERATOR 1
#define J9_ROM_CLASS_ALLOCATION_INCREMENT_DENOMINATOR 256
#define J9_ROM_CLASS_ALLOCATION_INCREMENT_MIN (64 * 1024)
#define J9_ROM_CLASS_ALLOCATION_INCREMENT_MAX (128 * 1024)
#define J9_ROM_CLASS_ALLOCATION_INCREMENT_ROUND_TO 1024

#define J9_INVARIANT_INTERN_TABLE_NODE_COUNT 2345
#define J9_SHARED_CLASS_CACHE_MIN_SIZE (4 * 1024)
#define J9_SHARED_CLASS_CACHE_MAX_SIZE I_32_MAX
/* Default shared class cache size on 64-bit platforms (if OS allows)
 * Otherwise,
 * 1. For non-persistent cache, if SHMMAX < J9_SHARED_CLASS_CACHE_DEFAULT_SIZE_64BIT_PLATFORM (300MB), default cache size is set to SHMMAX
 * 2. For persistent cache, if free disk space is < SHRINIT_LOW_FREE_DISK_SIZE (6GB), default cache size is set to J9_SHARED_CLASS_CACHE_DEFAULT_SOFTMAX_SIZE_64BIT_PLATFORM (64MB)
 */
#define J9_SHARED_CLASS_CACHE_DEFAULT_SIZE_64BIT_PLATFORM (300 * 1024 * 1024)
/* Default shared class soft max size on 64-bit platforms. This value is only set if the OS allows default cache size to be greater than J9_SHARED_CLASS_CACHE_MIN_DEFAULT_CACHE_SIZE_FOR_SOFTMAX */
#define J9_SHARED_CLASS_CACHE_DEFAULT_SOFTMAX_SIZE_64BIT_PLATFORM (64 * 1024 * 1024)
/* The minimum default shared class cache size to set a default soft max, on 64-bit platforms only. */
#define J9_SHARED_CLASS_CACHE_MIN_DEFAULT_CACHE_SIZE_FOR_SOFTMAX (80 * 1024 *1024)
/* Default shared class cache size on 32-bit platforms */
#define J9_SHARED_CLASS_CACHE_DEFAULT_SIZE (16 * 1024 * 1024)

#define J9_FIXED_SPACE_SIZE_NUMERATOR 0
#define J9_FIXED_SPACE_SIZE_DENOMINATOR 100
#define J9_FIXED_SPACE_SIZE_MIN 0
#define J9_FIXED_SPACE_SIZE_MAX 0

#define J9_INITIAL_STACK_SIZE (2 * 1024)
#define J9_STACK_SIZE_INCREMENT (16 * 1024)
#if defined(J9VM_ENV_DATA64)
#define J9_STACK_SIZE (128 * 1024 * sizeof(UDATA))
#else /* J9VM_ENV_DATA64 */
#define J9_STACK_SIZE (80 * 1024 * sizeof(UDATA))
#endif /* J9VM_ENV_DATA64 */

#if defined(J9VM_ARCH_X86) && !defined(J9VM_ENV_DATA64)
#define J9_JIT_CODE_CACHE_SIZE (512 * 1024)
#define J9_JIT_DATA_CACHE_SIZE (512 * 1024)
#else /* J9VM_ARCH_X86 && !J9VM_ENV_DATA64 */
#define J9_JIT_CODE_CACHE_SIZE (8 * 1024 * 1024)
#define J9_JIT_DATA_CACHE_SIZE (8 * 1024 * 1024)
#endif /* J9VM_ARCH_X86 && !J9VM_ENV_DATA64 */

#if defined(J9ZOS390) && defined(J9VM_ENV_DATA64)
/* Use a 1MB OS stack on z/OS 64-bit as this is what the OS
 * allocates anyway, using IARV64 GETSTOR to allocate a segment.
 */
#define J9_OS_STACK_SIZE (1024 * 1024)
#else /* J9ZOS390 && J9VM_ENV_DATA64 */
#define J9_OS_STACK_SIZE (256 * 1024)
#endif

/* Unused constants, kept here in case the JCL compiles use them */

#define J9_INITIAL_MEMORY_SIZE_SCALE_NUMERATOR 225
#define J9_INITIAL_MEMORY_SIZE_MAX 12582912
#define J9_NEW_SPACE_SIZE_SCALE_NUMERATOR 100
#define J9_SUBALLOCHEAPMEM32_COMMITSIZE 52428800
#define J9_NEW_SPACE_SIZE_SCALE_DENOMINATOR 1000
#define J9_IGC_OVERFLOW_LIST_FRAGMENT_SIZE 32
#define J9_NEW_SPACE_SIZE_ROUND_TO 1024
#define J9_IGC_OBJECT_SCAN_QUEUE_SIZE_ROUND_TO 32
#define J9_PHYSICAL_MEMORY_SCALE_NUMERATOR 3
#define J9_GC_OWNABLE_SYNCHRONIZER_LIST_INCREMENT 4096
#define J9_IGC_OBJECT_SCAN_QUEUE_SIZE_MIN 256
#define J9_OLD_SPACE_SIZE_ROUND_TO 1024
#define J9_FINALIZABLE_LIST_MAX 0
#define J9_INVARIANT_INTERN_TABLE_SIZE 16384
#define J9_IGC_TRIGGER_RATIO 10
#define J9_IGC_OBJECT_SCAN_QUEUE_SIZE_DENOMINATOR 8192
#define J9_IGC_OVERFLOW_LIST_SIZE 16384
#define J9_IGC_OBJECT_SCAN_QUEUE_SIZE_NUMERATOR 1
#define J9VM_DESKTOP 1
#define J9_SUBALLOCHEAPMEM32_INITIALSIZE 209715200
#define J9_GC_REFERENCE_LIST_INCREMENT 4096
#define J9_SCV_TENURE_RATIO_LOW 10
#define J9_OLD_SPACE_SIZE_MIN 1048576
#define J9_OLD_SPACE_SIZE_SCALE_DENOMINATOR 1000
#define J9_IGC_OVERFLOW_LIST_MAX 16384
#define J9_SCV_TENURE_RATIO_HIGH 30
#define J9_IGC_OBJECT_SCAN_QUEUE_SIZE_MAX 16384
#define J9_INITIAL_MEMORY_SIZE_SCALE_DENOMINATOR 1000
#define J9_RESMAN_DEFAULT_MEMORY_SPACE_MAX_DIVISOR 8
#define J9_GC_UNFINALIZED_LIST_INCREMENT 4096
#define J9_OLD_SPACE_SIZE_SCALE_NUMERATOR 125
#define J9_FINALIZABLE_LIST_INCREMENT 8192
#define J9_NEW_SPACE_SIZE_MIN 262144
#define J9_IGC_MINIMUM_FREE_RATIO 25
#define J9_PHYSICAL_MEMORY_SCALE_DENOMINATOR 4
#define J9_OLD_SPACE_SIZE_MAX 8388608
#define J9_FIXED_SPACE_SIZE_ROUND_TO 1024
#define J9_ALLOCATION_INCREMENT_ROUND_TO 1024
#define J9_NEW_SPACE_SIZE_MAX 4194304

#endif /* J9CFG_BUILDER_H */
