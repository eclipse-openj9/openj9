/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(METRONOME_HPP_)
#define METRONOME_HPP_

/* @ddr_namespace: default */
#include "omr.h"
#include "omrcfg.h"

#include <assert.h>
#define ASSERT_LEVEL 0
#define assert1(expr) assert((ASSERT_LEVEL < 1) || (expr));
#define assert2(expr) assert((ASSERT_LEVEL < 2) || (expr));

#define MAX_UINT ((uintptr_t) (-1))

#define CLOCK_SWITCH_TICK_THRESHOLD 1000000
#define INTER_YIELD_WARNING_THRESHOLD_NS 80000
/* INTER_YIELD_WARNING_THRESHOLD_NS indicates that 80 usec for inter-yield checks is already considered large.
 * The largest interval between yield checks should be bounded by 500 usec, which is a HardRT quanta.
 */
#define INTER_YIELD_MAX_NS 500000

#define INTER_YIELD_WARNING_THRESHOLD_NS 80000
#define UTILIZATION_WINDOW_SIZE 100

#define ROOT_GRANULARITY 100

/* 
 *	NOTE: Since we are using safe points, any information recorded by the
 *	GC when it is known that threads are stopped, can be accessed by the threads 
 *	without a critical section. 
 */
#define	GC_PHASE_IDLE		0x00000000
#define	GC_PHASE_ROOT		0x00000001
#define	GC_PHASE_TRACE		0x00000002
#define	GC_PHASE_SWEEP		0x00000004
#define	GC_PHASE_CONCURRENT_TRACE 0x00000008
#define	GC_PHASE_CONCURRENT_SWEEP 0x00000010
#define GC_PHASE_UNLOADING_CLASS_LOADERS  0x00000020

#define MINIMUM_FREE_CHUNK_SIZE 64

#endif /* METRONOME_HPP_ */
