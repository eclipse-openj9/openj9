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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cfreader.h"
#include "j9.h"

#include "jbcmap.h"
#include "pcstack.h" 
#include "bcnames.h" 
#include "j9port.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j9cp.h"
#include "rommeth.h"
#include "stackmap_internal.h"
#include "vrfytbl.h"
#include "bytecodewalk.h"
#include "ut_map.h"


#define DEFAULT_SCRATCH_SIZE 8192

#define ENCODED_INDEX	0x04
#define	ENCODED_MASK	0x03
#define	WIDE_INDEX		0x08

#define	DOUBLE_ACCESS	0x020
#define	SINGLE_ACCESS	0x040
#define	OBJECT_ACCESS	0x080
#define	WRITE_ACCESS	0x010

/* PARALLEL_TYPE max size is U_32 without changing the unwalked branch stack size and logic (it holds seenLocals and pc) */
#define PARALLEL_TYPE		U_32
#define	PARALLEL_BYTES		(sizeof(PARALLEL_TYPE))
#define	PARALLEL_WRITES		((UDATA) (sizeof (U_32) / PARALLEL_BYTES))
#define	PARALLEL_BITS		(PARALLEL_BYTES * 8)

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARALLEL_START_INDEX	(0)
#define	PARALLEL_INCREMENT		(1)
#else
#define PARALLEL_START_INDEX	(PARALLEL_WRITES - 1)
#define	PARALLEL_INCREMENT		(-1)
#endif

#define PARAM_8(index, offset) ((index) [offset])

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARAM_16(index, offset)				\
	( ( ((U_16) (index)[offset])		)	\
	| ( ((U_16) (index)[offset + 1]) << 8)	\
	)
#else
#define PARAM_16(index, offset)			\
	( ( ((U_16) (index)[offset]) << 8)	\
	| ( ((U_16) (index)[offset + 1]) )	\
	)
#endif

#ifdef J9VM_ENV_LITTLE_ENDIAN
#define PARAM_32(index, offset)				\
	( ( ((U_32) (index)[offset])		  )	\
	| ( ((U_32) (index)[offset + 1]) << 8 )	\
	| ( ((U_32) (index)[offset + 2]) << 16)	\
	| ( ((U_32) (index)[offset + 3]) << 24)	\
	)
#else
#define PARAM_32(index, offset)				\
	( ( ((U_32) (index)[offset])	 << 24)	\
	| ( ((U_32) (index)[offset + 1]) << 16)	\
	| ( ((U_32) (index)[offset + 2]) << 8 )	\
	| ( ((U_32) (index)[offset + 3])	  ) \
	)
#endif

static void mapAllLocals(J9PortLibrary * portLibrary, J9ROMMethod * romMethod, PARALLEL_TYPE * unknownsByPC, UDATA startPC, U_32 * resultArrayBase);
static IDATA mapLocalSet(J9PortLibrary * portLibrary, J9ROMMethod * romMethod, PARALLEL_TYPE * unknownsByPC, UDATA startPC, UDATA localIndexBase, PARALLEL_TYPE * knownLocals, PARALLEL_TYPE * knownObjects, BOOLEAN* unknownsWereUpdated);


/**
 * Construct a locals map size_of(PARALLEL_TYPE) * 8 slots at a time from 0 to remainingLocals-1.
 * @param portLibrary
 * @param romMethod Method whose bytecodes should be walked.
 * @param unknownsByPC Buffer used to store per-PC metadata (1 PARALLEL_TYPE per PC) + branch stack.
 * @param startPC The PC at which to start mapping.
 * @param resultArrayBase Memory into which the result should be stored.
*/
static void
mapAllLocals(J9PortLibrary * portLibrary, J9ROMMethod * romMethod, PARALLEL_TYPE * unknownsByPC, UDATA startPC, U_32 * resultArrayBase) 
{

	UDATA exceptionCount = 0, localIndexBase = 0;
	UDATA writeIndex = PARALLEL_START_INDEX;
	J9ExceptionHandler *handler;
	J9ExceptionInfo *exceptionData;
	UDATA remainingLocals = J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod) + J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);
	PARALLEL_TYPE knownLocals, knownObjects;
	PARALLEL_TYPE *parallelResultArrayBase = (PARALLEL_TYPE *) resultArrayBase;

#if defined(DEBUG)
	PORT_ACCESS_FROM_PORT(portLibrary);
#endif

	/* set up data to walk exceptions */
	if (J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)) {
		exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
		exceptionCount = (UDATA) exceptionData->catchCount;
	}

	while (remainingLocals) {
		BOOLEAN unknownsChanged;
		
		/* clear the beenHere for each local set */
		memset(unknownsByPC, 0, J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod) * PARALLEL_BYTES);
		knownLocals = 0;

		if (remainingLocals > PARALLEL_BITS) {
			remainingLocals -= PARALLEL_BITS;
		} else {
			if (remainingLocals < PARALLEL_BITS) {
				knownLocals = (PARALLEL_TYPE) -1;
				knownLocals <<= remainingLocals;
			}
			remainingLocals = 0;
		}

		knownObjects = 0;
		mapLocalSet(portLibrary, romMethod, unknownsByPC, startPC, localIndexBase, &knownLocals, &knownObjects, &unknownsChanged);

		if ((knownLocals != (PARALLEL_TYPE) -1) && exceptionCount) {
			/* walk the exceptions */
			UDATA e, j, keepLooking = TRUE;
			PARALLEL_TYPE unknownLocals;

			while (keepLooking) {
				/* since each exception handler may walk through the start/end range of another handler,
					 continue until all handlers are walked or the remaining handlers could not have been
					 called (no foot prints passed through their protection range) */
				keepLooking = FALSE;
				handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);

				for (e = 0; e < exceptionCount; e++) {
					UDATA rawUnknowns;
					
					/* Collect up any locals that were unknown in the covered exception range. */
					unknownLocals = 0;
					for (j = handler->startPC; j < handler->endPC; j++) {
						unknownLocals |= unknownsByPC[j];
					}
					
					/* Don't bother looking for locals we know about definitively. */
					rawUnknowns = unknownLocals;
					unknownLocals &= ~knownLocals;

#if defined(DEBUG)
				{
					UDATA bitIndex;
					j9file_printf(PORTLIB, J9PORT_TTY_OUT, "\n");
					j9file_printf(PORTLIB, J9PORT_TTY_OUT, "handler[%d] unknowns raw=0x%X masked=0x%X [", e, rawUnknowns, unknownLocals);
					for (bitIndex=0; bitIndex < 32; bitIndex++) {
						UDATA bit = 1 << bitIndex;
						if (unknownLocals & bit) {
							j9file_printf(PORTLIB, J9PORT_TTY_OUT, "%d ", bitIndex + localIndexBase);
						}
					}
					j9file_printf(PORTLIB, J9PORT_TTY_OUT, "]\n");
				}
#endif
					
					if ((~unknownsByPC[handler->handlerPC] & unknownLocals) != 0) {
						PARALLEL_TYPE exceptionKnownLocals = ~unknownLocals;
						PARALLEL_TYPE localsBeforeWalk = exceptionKnownLocals;
						BOOLEAN unknownsChangedWalkingHandler;
						
						/* walk the exception handler */
						mapLocalSet(portLibrary, romMethod, unknownsByPC, handler->handlerPC, localIndexBase, &exceptionKnownLocals, &knownObjects, &unknownsChangedWalkingHandler);

						/* keepLooking if found new stuff, or updated per-PC metadata */
						keepLooking = keepLooking || (exceptionKnownLocals != localsBeforeWalk);
						keepLooking = keepLooking || unknownsChangedWalkingHandler;

						/* Note all new locals found */
						knownLocals |= (exceptionKnownLocals & ~localsBeforeWalk);
					}

					handler++;
				}
			}
		} 

		/* save the local set result */
		parallelResultArrayBase[writeIndex] = knownObjects;
		writeIndex += PARALLEL_INCREMENT;
		/* relies on wrapping of unsigned subtract for big endian */
		if (writeIndex >= PARALLEL_WRITES) {
			writeIndex = PARALLEL_START_INDEX;
			parallelResultArrayBase += PARALLEL_WRITES;
		}
		localIndexBase += PARALLEL_BITS;

	}
	return;
}



/* 
	Walk the bytecodes in romMethod starting at startPC.  For each 
	@param romMethod Method containing the bytecodes to walk.
	@param unknownsByPC Buffer used to store per-PC metadata (1 PARALLEL_TYPE per PC) + branch stack.
	@param startPC Bytecode index to begin the walk.
	@param localIndexBase Offset of the first local, usually zero unless method has > PARALLEL_TYPE bits. 
	@param knownLocals Bitfield of locals whose types are known definitively.
	@param knownObject Bitfield of locals which are definitively objects.
	@param unknownsUpdated Pointer to a boolean value updated iff. any per-PC metadata was updated.
*/

static IDATA
mapLocalSet(J9PortLibrary * portLibrary, J9ROMMethod * romMethod, PARALLEL_TYPE * unknownsByPC, UDATA startPC, UDATA localIndexBase, PARALLEL_TYPE * knownLocals, PARALLEL_TYPE * knownObjects, BOOLEAN* unknownsWereUpdated) 
{

#define PUSH_BRANCH(programCounter)					 \
	*nextUnwalkedBranch++ = (U_32) (programCounter); \
	*nextUnwalkedBranch++ = (U_32) seekLocals;

#define POP_BRANCH(programCounter) 						\
	seekLocals = (PARALLEL_TYPE) *--nextUnwalkedBranch; \
	programCounter = *--nextUnwalkedBranch;
	
	IDATA pc;
	I_32 offset, low, high;
	U_32 npairs, setBit;
	UDATA index;
	U_8 * bcStart, *bcIndex, *bcEnd;
	UDATA bc, size, temp1, target;
	U_32 *unwalkedBranches = (U_32 *) (unknownsByPC + J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod));
	/* initialize the stack of unwalked PCs - empty */
	U_32 *nextUnwalkedBranch = unwalkedBranches;
	PARALLEL_TYPE seekLocals = ~(*knownLocals);

#if defined(DEBUG)
	PORT_ACCESS_FROM_PORT(portLibrary);
#endif
	
	/* Assume nothing changed */
	*unknownsWereUpdated = FALSE;
	
	bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	bcEnd = bcStart + J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	bcIndex = bcStart + startPC;

	while (bcIndex < bcEnd) {
		pc = bcIndex - bcStart;

#if defined(DEBUG)
		{
			UDATA shouldBeWalked = ~unknownsByPC[pc] & seekLocals;
			UDATA bitIndex;
			j9file_printf(PORTLIB, J9PORT_TTY_OUT, "pc=%d walk=%s seekLocals=0x%X [", pc, (shouldBeWalked ? "true" : "false"), seekLocals);
	
			for (bitIndex=0; bitIndex < 32; bitIndex++) {
				UDATA bit = 1 << bitIndex;
				if (seekLocals & bit) {
					j9file_printf(PORTLIB, J9PORT_TTY_OUT, "%d ", bitIndex);
				}
			}
	
			j9file_printf(PORTLIB, J9PORT_TTY_OUT, "]\n");
		}
#endif
		
		/* Continue if any seekLocals is not in the beenHere set */
		if (~unknownsByPC[pc] & seekLocals) {

			/* leave footprints */
			*unknownsWereUpdated = TRUE;
			unknownsByPC[pc] |= seekLocals;

			bc = (UDATA) *bcIndex;
			size = (UDATA) J9JavaInstructionSizeAndBranchActionTable[bc];

			switch (size >> 4) {

			case 0:
				temp1 = (UDATA) J9BytecodeSlotUseTable[bc];
				if (temp1) {

					if (temp1 & ENCODED_INDEX) {
						/* Get encoded index */
						index = temp1 & ENCODED_MASK;
					} else {
						/* Get parameter byte index */
						index = PARAM_8(bcIndex, 1);
						if (temp1 & WIDE_INDEX) {
							/* get word index */
							index = PARAM_16(bcIndex, 1);
						}
					}

					/* Trace only those locals in the range of interest */
					index -= localIndexBase;

_doubleAccess:
					if (index < PARALLEL_BITS) {

						setBit = 1 << index;

						/* First encounter with this local? */
						if (seekLocals & setBit) {
							/* Stop looking for this local */
							seekLocals &= (~setBit);
#if defined(DEBUG)
							j9file_printf(PORTLIB, J9PORT_TTY_OUT, "  stop looking for local=%d\n", index);
#endif

							/* Know what the local is if it is read */
							if ((temp1 & WRITE_ACCESS) == 0) {
								*knownLocals |= setBit;
#if defined(DEBUG)
								j9file_printf(PORTLIB, J9PORT_TTY_OUT, "  add local=%d to known locals.\n", index);
#endif

								/* Is it an Object */
								if (temp1 & OBJECT_ACCESS) {
									*knownObjects |= setBit;
#if defined(DEBUG)
									j9file_printf(PORTLIB, J9PORT_TTY_OUT, "  add local=%d to known objects.\n", index);
#endif
								}
							}
						} else {
#if defined(DEBUG)
							j9file_printf(PORTLIB, J9PORT_TTY_OUT, "  skipping reference to local=%d\n", index);
#endif
						}
					}

					if (temp1 & DOUBLE_ACCESS) {
						temp1 &= ~DOUBLE_ACCESS;
						index++;
						goto _doubleAccess;
					}
				}
				bcIndex += size;
				break;

			case 1:			/* ifs */
				offset = (I_16) PARAM_16(bcIndex, 1);
				PUSH_BRANCH((U_32) (pc + (I_16) offset));
				/* fall through */

			case 6:		/* invokes */
				bcIndex += (size & 7);
				break;

			case 2:			/* gotos */
				if (bc == JBgoto) {
					offset = (I_16) PARAM_16(bcIndex, 1);
					target = (UDATA) (pc + (I_16) offset);
				} else {
					offset = (I_32) PARAM_32(bcIndex, 1);
					target = (UDATA) (pc + offset);
				}
				bcIndex = bcStart + target;
				break;

			case 4:			/* returns/athrow */
				goto _nextBranch;
				break;

			case 5:			/* switches */

				bcIndex = bcIndex + (4 - (pc & 3));
				offset = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;
				target = (UDATA) (pc + offset);
				low = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;

				if (bc == JBtableswitch) {
					high = (I_32) PARAM_32(bcIndex, 0);
					bcIndex += 4;
					npairs = (U_32) (high - low + 1);
					temp1 = 0;
				} else {
					npairs = (U_32) low;
					/* used to skip over the tableswitch key entry */
					temp1 = 4;
				}

				for (; npairs > 0; npairs--) {
					bcIndex += temp1;
					offset = (I_32) PARAM_32(bcIndex, 0);
					bcIndex += 4;
					PUSH_BRANCH((U_32) (pc + offset));
				}

				/* finally continue at the default switch case */
				bcIndex = bcStart + target;
				break;
			}

		} else {
_nextBranch:
			/* No further possible paths - finished */
			if (nextUnwalkedBranch == unwalkedBranches) {
				return 0;
			}
			POP_BRANCH(pc);
			/* Don't look for already known locals */
			seekLocals &= ~(*knownLocals);
			bcIndex = bcStart + pc;
		}
	}
	Trc_Map_mapLocalSet_WalkOffEndOfBytecodeArray();
	return -1;					/* Should never get here */
}




/* 
	Return only the Args part of the Locals.
*/

void
j9localmap_ArgBitsForPC0 (J9ROMClass * romClass, J9ROMMethod * romMethod, U_32 * resultArrayBase) 
{
	argBitsFromSignature(
		J9UTF8_DATA(J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod)),
		resultArrayBase,
		(J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + 31) >> 5,
		(romMethod->modifiers & J9AccStatic) != 0);
}



IDATA
j9localmap_LocalBitsForPC(J9PortLibrary * portLib, J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA pc, U_32 * resultArrayBase, 
							void * userData, 
							UDATA * (* getBuffer) (void * userData), 
							void (* releaseBuffer) (void * userData)) 
{

	PORT_ACCESS_FROM_PORT(portLib);
	UDATA mapWords = (UDATA) ((J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod) + J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + 31) >> 5);
	UDATA length, scratchSize;
	UDATA *scratch;
	UDATA accurateGuess = FALSE;
	U_32 tempResultArray = 0;
#define LOCAL_SCRATCH 2048
	UDATA localScratch[LOCAL_SCRATCH / sizeof(UDATA)];
	UDATA *allocScratch = NULL;
	UDATA *globalScratch = NULL;

	Trc_Map_j9localmap_LocalBitsForPC_Method(J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod) + J9_ARG_COUNT_FROM_ROM_METHOD(romMethod), pc, 
												(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)),
												(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_GET_NAME(romClass, romMethod)), J9UTF8_DATA(J9ROMMETHOD_GET_NAME(romClass, romMethod)),
												(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod)), J9UTF8_DATA(J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod)));
	
	/* clear the result map as we may not write all of it */
	memset ((U_8 *)resultArrayBase, 0, mapWords * sizeof (U_32));

	length = (UDATA) J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

	scratchSize = (romClass->maxBranchCount * (2 * sizeof(U_32))) + (length * sizeof(U_32));

	if(scratchSize < LOCAL_SCRATCH) {
		scratch = localScratch;
	} else {
		allocScratch = j9mem_allocate_memory(scratchSize, OMRMEM_CATEGORY_VM);
		if (allocScratch) {
			scratch = allocScratch;
		} else if (getBuffer != NULL) {
			globalScratch = (getBuffer) (userData);
			scratch = globalScratch;
			if (!scratch) {
				Trc_Map_j9localmap_LocalBitsForPC_GlobalBufferFailure(scratchSize);
				return BCT_ERR_OUT_OF_MEMORY;
			}
		} else {
			Trc_Map_j9localmap_LocalBitsForPC_AllocationFailure(scratchSize);
			return BCT_ERR_OUT_OF_MEMORY;
		}
	}

	mapAllLocals(portLib, romMethod, (PARALLEL_TYPE *) scratch, pc, resultArrayBase);

	/* Ensure that the receiver is marked for empty j.l.Object.<init>()V */
	if (J9ROMMETHOD_IS_OBJECT_CONSTRUCTOR(romMethod) && J9ROMMETHOD_IS_EMPTY(romMethod)) {
		*resultArrayBase |= 1;
	}

 	if (globalScratch) {
		(releaseBuffer) (userData);
 	}

	j9mem_free_memory(allocScratch);

	return 0;
}

#undef LOCAL_SCRATCH

