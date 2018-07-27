/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

#define J9PORT_RI_ENABLED ((uint32_t) 0x1)
#define J9PORT_RI_INITIALIZED ((uint32_t) 0x2)
/**
 * Define datastructures for runtime instrumentation control blocks
 */
typedef struct riControlBlock {
#if defined(S390) || defined(J9ZOS390)

   /* Word 0/1 - Run-time Instrumentation Program Buffer Current Address */
   uint64_t currentAddr;
   /* Word 2/3 - Run-time Instrumentation Program Buffer Origin Address */
   uint64_t originAddr;
   /* Word 4/5 - Run-time Instrumentation Program Buffer Limit Address */
   uint64_t limitAddr;

   /* Word 6 - Flags */
   uint32_t padAtWord6Bit0:29;
   uint32_t reportingGroupSize:3;

   /* Word 7 */

   /* Mode - 4 bits unsigned int:
    *  0 - Counting CPU Cycles
    *  1 - Counting Instructions
    *  2 - Directed Only
    */
   uint32_t mode:4;
   /* N - RINext Control */
   uint32_t RInext:1;
   /* MAE - Max Address exceeded bit */
   uint32_t MAE:1;
   uint32_t padAtWord7Bit6:2;
   /* Branch Controls */
   uint32_t callTypeBranches:1;
   uint32_t returnTypeBranches:1;
   uint32_t otherTypeBranches:1;
   uint32_t branchOnCondAsOtherTypeBrranches:1;
   uint32_t RIEmit:1;
   uint32_t TXAbort:1;

   uint32_t guardedStorage:1;
   uint32_t padAtWord7Bit15:1;
   /* Branch Prediction Controls */
   uint32_t bp_notTakenButPredictedTaken:1;
   uint32_t bp_takenButPredictedNotTaken:1;
   uint32_t bp_takenButWrongTarget:1;
   uint32_t bp_notTAkenButWrongTarget:1;

   /* Suppression Controls */
   uint32_t suppressPrimaryCPU:1;
   uint32_t suppressSecondaryCPU:1;

   uint32_t DCacheMissExtra:1;
   uint32_t cacheLatencyLevelOverride:1;
   uint32_t ICacheLatencyLevel:4;
   uint32_t DCacheLatencyLevel:4;

/* Word 8 - Not currently used */
   uint32_t notUsed8;

/* Word 9 - Not currently used */
   uint32_t notUsed9;

/* Word 10/11 - Scaling Factor  - Settable only when K = 0 */
   uint64_t scalingFactor;

/* Word 12/13 - Remaining Sample Interval Count - Settable only when K = 0 */
   uint64_t rsic;

/* Word 14 - Not currently used */
   uint32_t notUsed14;
/* Word 15 - Not currently used */
   uint32_t notUsed15;
#else
	uint32_t placeHolder;
#endif
} riControlBlock;
