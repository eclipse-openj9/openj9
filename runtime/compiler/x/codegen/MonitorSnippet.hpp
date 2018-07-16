/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef X86MONITORSNIPPET_INCL
#define X86MONITORSNIPPET_INCL

// Format of the monitor slot for tasuki locks is:
//    Bit 0: Fat lock bit
//    Bit 1: Reserved for FLC bit
//    Bits 2-7: Nesting count
//    Bits 8-31: Thread pointer or fat monitor id
//
#define INC_DEC_VALUE        OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT
#define NON_INC_DEC_MASK     ((~OBJECT_HEADER_LOCK_BITS_MASK) | OBJECT_HEADER_LOCK_INFLATED | OBJECT_HEADER_LOCK_FLC | OBJECT_HEADER_LOCK_RESERVED)
#define NON_INC_DEC_FLC_MASK ((~OBJECT_HEADER_LOCK_BITS_MASK) | OBJECT_HEADER_LOCK_INFLATED)
#define THREAD_PTR_MASK      (~OBJECT_HEADER_LOCK_BITS_MASK)
#define MAX_INL_INC_DEC_MASK ((~OBJECT_HEADER_LOCK_BITS_MASK) | OBJECT_HEADER_LOCK_INFLATED | OBJECT_HEADER_LOCK_FLC | OBJECT_HEADER_LOCK_RESERVED | OBJECT_HEADER_LOCK_LAST_RECURSION_BIT)
#define FLC_BIT              OBJECT_HEADER_LOCK_FLC
#define RES_BIT              OBJECT_HEADER_LOCK_RESERVED
#define RES_BIT_POSITION     2
#define REC_BIT              OBJECT_HEADER_LOCK_FIRST_RECURSION_BIT
#define FLAGS_MASK           (OBJECT_HEADER_LOCK_INFLATED | OBJECT_HEADER_LOCK_FLC | OBJECT_HEADER_LOCK_RESERVED)
#define REC_MASK             OBJECT_HEADER_LOCK_RECURSION_MASK

static_assert(RES_BIT == (1 << RES_BIT_POSITION), "RES_BIT_POSITION mismatch");

#endif
