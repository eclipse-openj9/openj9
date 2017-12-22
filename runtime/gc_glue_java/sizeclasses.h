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


#ifndef SIZECLASSES_H_
#define SIZECLASSES_H_

/* @ddr_namespace: default */
#include "j9consts.h"

#define OMR_SIZECLASSES_NUM_SMALL J9VMGC_SIZECLASSES_NUM_SMALL

#define OMR_SIZECLASSES_MIN_SMALL 0x01
#define OMR_SIZECLASSES_MAX_SMALL OMR_SIZECLASSES_NUM_SMALL
#define OMR_SIZECLASSES_ARRAYLET (OMR_SIZECLASSES_NUM_SMALL + 1)
#define OMR_SIZECLASSES_LARGE (OMR_SIZECLASSES_NUM_SMALL + 2)

#define OMR_SIZECLASSES_LOG_SMALLEST J9VMGC_SIZECLASSES_LOG_SMALLEST
#define OMR_SIZECLASSES_LOG_LARGEST J9VMGC_SIZECLASSES_LOG_LARGEST
#define OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES (1 << OMR_SIZECLASSES_LOG_LARGEST)

/* The initial number of size classes and their sizes.
 *
 * If we are configured to support RTSJ, then force 8 byte alignment
 * of all objects. (SizeClass.java, force8)
 * Enables cheap scope vs. heap check based on address: 000 ==> heap, 100 ==> scope
 *
 * Because of alignment considerations, we must ensure there
 * are never two adjacent size classes both of which are not multiples of 8.
 * Size classes computed by SizeClass.java using parameters rho = 1/8.
 * Note that this array must be of size OMR_SIZECLASSES_NUM_SMALL+1. Note that
 * the 0 size class isn't used since there are no 0-size objects.
 */
#define SMALL_SIZECLASSES	{0, 16, 24, 32, 40, 48, 56, 64, 72, 88,\
							 104, 120, 136, 160, 184, 208, 240, 272, 312, 352,\
							 400, 456, 520, 592, 672, 760, 856, 968, 1096, 1240,\
							 1400, 1576, 1776, 2000, 2256, 2544, 2864, 3224, 3632, 4088,\
							 4600, 5176, 5824, 6552, 7376, 8304, 9344, 10512, 11832, 13312,\
							 14976, 16848, 18960, 21336, 24008, 27016, 30400, 34200, 38480, 43296,\
							 48712, 54808, 61664, 65536 }

typedef struct OMR_SizeClasses {
    UDATA smallCellSizes[OMR_SIZECLASSES_MAX_SMALL + 1];
    UDATA smallNumCells[OMR_SIZECLASSES_MAX_SMALL + 1];
    UDATA sizeClassIndex[OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES >> 2];
} OMR_SizeClasses;

#endif /* SIZECLASSES_H_ */
