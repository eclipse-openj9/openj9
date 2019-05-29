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

#define J9JSRI_DEBUG		0
#define XFUTURE_TEST		0
#define LARGE_METHOD_TEST	0
#define WIDE_BRANCH_TEST	0
#define TEST_PAD_LENGTH		(256*1024)

#include <string.h>
#include "cfreader.h"
#include "j9.h"

#include "jbcmap.h"
#include "pcstack.h" 
#include "bcnames.h"
#include "j9port.h"
#include "dbg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "rommeth.h"
#include "j9bcvnls.h"

#include "bcsizes.h"				/* Needed for the sunJavaByteCodeRelocation identifier */


#define JI_STACK_UNDERFLOW 0xFF
#define JI_STACK_OVERFLOW 0xFE
#define JI_LOCAL_OUT_OF_RANGE 0xFD
#define	JI_NO_RET_YET -1
#define JI_RET_ADDRESS 1
#define JI_USED_RET_ADDRESS 2

#define	J9JSRI_COLOUR_SWITCH_PAD_MASK 0x00000003
#define J9JSRI_COLOUR_FOR_UNWRITTEN 0x00000010
#define J9JSRI_COLOUR_FOR_JSR 0x00000020
#define J9JSRI_COLOUR_FOR_RET 0x00000040
#define J9JSRI_COLOUR_FOR_WIDE 0x00000080
#define J9JSRI_COLOUR_FOR_SWITCH 0x00000100
#define J9JSRI_COLOUR_FOR_BRANCH 0x00000200
#define J9JSRI_COLOUR_FOR_JSR_START 0x00000400
#define J9JSRI_COLOUR_FOR_JSR_END 0x00000800

#define J9JSRI_MAP_NOT_MARKED 0x00
#define J9JSRI_MAP_MARKED_END_BLOCK 0x01
#define J9JSRI_MAP_MARKED_EXCEPTION_HANDLER 0x02
#define J9JSRI_MAP_MARKED_JSR_COLLAPSIBLE 0x04

#define ROUND_TO(granularity, number) ((number) +		\
	(((number) % (granularity)) ? ((granularity) - ((number) % (granularity))) : 0))

#define CF_ALLOC_INCREMENT 4096
#define ROUNDING_GRANULARITY 1024

static UDATA isRetMultiplyUsed (UDATA pc, J9JSRIData * inlineBuffers);
static U_8 pushReturnOntoStack (U_16 constantIndex, J9CfrConstantPoolInfo * constantPool, J9JSRIJSRData * jsrData);
static void correctJumpOffsets (J9JSRIData * inlineBuffers);
static void markMapAtLocation (UDATA location, J9JSRICodeBlock * block, J9JSRIData * inlineBuffers);
static void releaseMap (J9JSRIData * inlineBuffers);
static void rewriteLineNumbers (J9JSRIData * inlineBuffers);
static void checkExceptionStart (J9JSRICodeBlock * thisBlock, J9JSRIJSRData * jsrData, J9JSRIData * inlineBuffers);
static void rewriteLocalVariables (J9JSRIData * inlineBuffers, U_8 tableType);
static void walkExceptions (I_32 hasRET, J9JSRICodeBlock * root, J9JSRIJSRData * jsrData, J9JSRIData * inlineBuffers);
static void dumpData (J9JSRIData * inlineBuffers);
static J9JSRIExceptionListEntry * addNewExceptionEntryToList (J9JSRIExceptionListEntry ** list, J9JSRIExceptionListEntry * entry, J9JSRIData * inlineBuffers);
static U_8 loadIntoVariable (J9JSRIData * inlineBuffers, J9JSRIJSRData * ourData, U_8 value, U_16 variable);
static UDATA mapJumpTargets (J9JSRIData * inlineBuffers);
static void copyOriginalExceptionList (J9JSRIData * inlineBuffers);
static UDATA areDataChainsEqual (J9JSRIJSRData *one, J9JSRIJSRData *two);
static J9JSRIJSRData * duplicateJSRDataChain (J9JSRIJSRData * original, J9JSRIData * inlineBuffers);
static U_8 pushZeroOntoStack (J9JSRIJSRData * ourData);
static J9JSRICodeBlock * getCodeBlockParentInChainWithVar (J9JSRIJSRData * root, U_16 variable, U_32 currentPC, J9JSRIData * inlineBuffers);
static J9JSRICodeBlock * getNextGreatestBlock (J9JSRIJSRData * jsrData, UDATA * startAddress, J9JSRIData * inlineBuffers);
static void evaluateCodeBlock (I_32 hasRET, J9JSRICodeBlock ** blockParentPointer, U_32 startingAddress, J9JSRIJSRData * jsrData, J9JSRIData * inlineBuffers);
static J9JSRICodeBlock * addSwitchChildToBlock (J9JSRICodeBlock * parent, J9JSRIData * inlineBuffers);
static J9JSRICodeBlock * isMapMarkedAtLocation (UDATA location, J9JSRIJSRData * parentJSR, J9JSRIData * inlineBuffers);
static void rewriteExceptionHandlers (J9JSRIData * inlineBuffers);
static void createNewMap (J9JSRIData * inlineBuffers);
static UDATA isJSRRecursive (J9JSRIJSRData * data, J9JSRIData * inlineBuffers);
static void allocateInlineBuffers (J9JSRIData * inlineBuffers);
static void flattenCodeBlockHeirarchyToList (J9JSRICodeBlock * structureRoot, J9JSRIData * inlineBuffers);
static U_8 pushOntoStack (J9JSRIJSRData * ourData, U_8 value);
static J9JSRIJSRData * createJSRData (J9JSRICodeBlock * parent, J9JSRIJSRData * previous, U_32 spawnPC, U_32 destinationPC, J9JSRIData * inlineBuffers);
static U_8 popStack (J9JSRIJSRData * ourData);
static void flattenCodeBlocksWide (J9JSRIData * inlineBuffers);


/*

Description:
Adds new entry to the END of the list given.

Args:
list - the list that we are adding the entry to

Return:
update the list pointer as an writeback parameter
return a pointer to the new list entry

Notes:

*/

static J9JSRIExceptionListEntry *
addNewExceptionEntryToList(J9JSRIExceptionListEntry ** list, J9JSRIExceptionListEntry * entry, 
																											J9JSRIData * inlineBuffers)
{
	J9JSRIExceptionListEntry *newEntry = pool_newElement(inlineBuffers->exceptionListEntryPool);

#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
	j9tty_printf(inlineBuffers->portLib, "addNewExceptionEntryToList %d to %d at %d\n", entry->startPC, entry->endPC, entry->handlerPC);
#endif

	if (newEntry) {
		newEntry->tableEntry = entry->tableEntry;
		
		if (*list) {
			J9JSRIExceptionListEntry *temp = *list;
			while (temp->nextInList) {
				temp = temp->nextInList;
			}
			temp->nextInList = newEntry;
		} else {
			*list = newEntry;
		}
	} else {
		inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
	}
	return newEntry;
}



/*
Description:
Adds the child code block to the parent as one of its switch children

Args:
parent - the code block that will accept the child
pool - allocation pool for CodeBlocks

Return:
void

Notes:
Child added to the END of the parent's list of switch children
*/

static J9JSRICodeBlock * 
addSwitchChildToBlock(J9JSRICodeBlock * parent, J9JSRIData * inlineBuffers)
{
	J9JSRICodeBlock *newBlock = pool_newElement(inlineBuffers->codeBlockPool);

	if (newBlock) {
		J9JSRICodeBlock * temp = parent->secondaryChild;
		if (temp) {
			while (temp->nextBlock) {
				temp = temp->nextBlock;
			}
			temp->nextBlock = newBlock;
		} else {
			parent->secondaryChild = newBlock;
		}
	} else
		inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
	return newBlock;
}



/*

Description:
Creates the inlining pools.

Args:
inlineBuffers - holds the various pool pointers

Return:
inlineBuffers->errorCode will indicate allocation failures

Notes:

*/

static void 
allocateInlineBuffers (J9JSRIData * inlineBuffers)
{
	/* Create buffers if first call */
	if (!inlineBuffers->codeBlockPool) {

		inlineBuffers->codeBlockPool = pool_new(sizeof(J9JSRICodeBlock), 0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(inlineBuffers->portLib));
		inlineBuffers->exceptionListEntryPool = pool_new(sizeof(J9JSRIExceptionListEntry), 0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(inlineBuffers->portLib));

	} else { /* reset buffers */
		pool_clear (inlineBuffers->codeBlockPool);
		pool_clear (inlineBuffers->exceptionListEntryPool);
	}
	pool_kill (inlineBuffers->jsrDataPool);

	inlineBuffers->jsrDataPool = pool_new((sizeof(J9JSRIJSRData) + inlineBuffers->maxStack + inlineBuffers->maxLocals + sizeof(UDATA)),
			0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(inlineBuffers->portLib));
	createNewMap(inlineBuffers);

	if (inlineBuffers->errorCode) {
		return;
	}

	if (!(inlineBuffers->codeBlockPool 
				&& inlineBuffers->exceptionListEntryPool 
				&& inlineBuffers->jsrDataPool 
				&& inlineBuffers->map)) {
		inlineBuffers->destBufferSize = 0;
		inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
		return;
	}

	inlineBuffers->destBuffer = inlineBuffers->freePointer;
	inlineBuffers->destBufferSize = (UDATA) (inlineBuffers->segmentEnd - inlineBuffers->freePointer);
	memset (inlineBuffers->destBuffer, 0, inlineBuffers->destBufferSize);

	inlineBuffers->destBufferIndex = 0;
	inlineBuffers->originalExceptionTable = NULL;
	inlineBuffers->exceptionTable = NULL;
	inlineBuffers->errorCode = 0;
	inlineBuffers->verifyError = 0;
	inlineBuffers->verifyErrorPC = -1;
	inlineBuffers->exceptionTableBufferSize = 0;
	inlineBuffers->exceptionTableBuffer = NULL;
	inlineBuffers->firstOutput = NULL;
	inlineBuffers->lastOutput = NULL;
}



/*
Description:
Compares the two given JSRDAta chains based on their scalar members

Args:
one - a JSRData chain to compare
two - a JSRData chain to compare

Return:
non-zero if the two given JSRData chains are equivalent

Notes:
*/

static UDATA 
areDataChainsEqual(J9JSRIJSRData *one, J9JSRIJSRData *two)
{
	while (one && two) {
		if (one->spawnPC != two->spawnPC) {
			return 0;
		}
		one = one->previousJSR;
		two = two->previousJSR;
	}
	return (one == two);
}



/*

Description:
Adds new entry to the END of the list given.

Args:
list - the list that we are adding the entry to

Return:
void

Notes:

*/

static void 
copyOriginalExceptionList(J9JSRIData * inlineBuffers)
{
	UDATA i;
	J9CfrExceptionTableEntry *entry;
	J9JSRIExceptionListEntry *newEntry;
	J9JSRIExceptionListEntry **last;

	last = &inlineBuffers->originalExceptionTable;

	for (i = 0; i < inlineBuffers->codeAttribute->exceptionTableLength; i++) {
		newEntry = pool_newElement(inlineBuffers->exceptionListEntryPool);

		if (newEntry) {
			entry = &(inlineBuffers->codeAttribute->exceptionTable[i]);

			newEntry->startPC = entry->startPC;
			newEntry->endPC = entry->endPC;
			newEntry->handlerPC = entry->handlerPC;
			newEntry->catchType = entry->catchType;
			newEntry->tableEntry = entry;
			*last = newEntry;
			last = &newEntry->nextInList;

			/* Handle exceptions catching themselves in the middle of an exception range */
			if ((newEntry->handlerPC > newEntry->startPC) && (newEntry->handlerPC < newEntry->endPC)) {
				/* Split the handler into two handlers - one up to the handlerPC and the other */
				/* from the handlerPC to the endPC */
				newEntry->endPC = newEntry->handlerPC;
				newEntry = pool_newElement(inlineBuffers->exceptionListEntryPool);

				if (newEntry) {

					newEntry->startPC = entry->handlerPC;
					newEntry->endPC = entry->endPC;
					newEntry->handlerPC = entry->handlerPC;
					newEntry->catchType = entry->catchType;
					newEntry->tableEntry = entry;
					*last = newEntry;
					last = &newEntry->nextInList;
				} else {
					inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
					return;
				}

			}
		} else {
			inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
			return;
		}
	}
}



/*
Description:
This will correct all offsets.
*/

static void 
correctJumpOffsets(J9JSRIData * inlineBuffers)
{
	U_8 * buffer = inlineBuffers->destBuffer;
	J9JSRICodeBlock *root = inlineBuffers->firstOutput;
	J9JSRICodeBlock *switchChildren;
	UDATA pc, bc, start, step;
	I_16 offset16;
	I_32 offset32;

	while (root) {
		pc = root->newPC;

		if (root->coloured & J9JSRI_COLOUR_FOR_BRANCH) {

			if (root->coloured & J9JSRI_COLOUR_FOR_WIDE) {
				pc += root->length - 5;
				offset32 = (I_32) (root->secondaryChild->newPC - pc);
				buffer[pc + 1] = (U_8) (offset32 >> 24);
				buffer[pc + 2] = (U_8) (offset32 >> 16);
				buffer[pc + 3] = (U_8) (offset32 >> 8);
				buffer[pc + 4] = (U_8) (offset32);
			} else {
				pc += root->length - 3;
				offset32 = (I_32) (root->secondaryChild->newPC - pc);
				if ((offset32 > (32 * 1024 - 1)) || (offset32 < (-32 * 1024))) {
					inlineBuffers->wideBranchesNeeded = TRUE;
					return;
				}
				offset16 = (I_16) (root->secondaryChild->newPC - pc);
				buffer[pc + 1] = (U_8) (offset16 >> 8);
				buffer[pc + 2] = (U_8) (offset16);
			}

		} else if (root->coloured & J9JSRI_COLOUR_FOR_RET) {

			/* rets have no primary children, but use the secondary to hold the spawning code block pointer */
			/* Note: ret blocks can be untagged in getNextGreatestBlock and would not get a goto_w */
			if (root->secondaryChild && root->secondaryChild->secondaryChild) {	
				/* All ret's are replaced with goto_w's */
				buffer[pc] = CFR_BC_goto_w;

				offset32 = (I_32) (root->secondaryChild->secondaryChild->newPC - pc);
				buffer[pc + 1] = (U_8) (offset32 >> 24);
				buffer[pc + 2] = (U_8) (offset32 >> 16);
				buffer[pc + 3] = (U_8) (offset32 >> 8);
				buffer[pc + 4] = (U_8) (offset32);
			}

		} else if (root->coloured & J9JSRI_COLOUR_FOR_SWITCH) {

			while (buffer[pc] == CFR_BC_nop) {
				pc++;
			}
			start = pc;
			bc = buffer[pc];

			pc += (4 - (pc & 3));

			offset32 = (I_32) (root->secondaryChild->secondaryChild->newPC - start);
			buffer[pc] = (U_8) (offset32 >> 24);
			buffer[pc + 1] = (U_8) (offset32 >> 16);
			buffer[pc + 2] = (U_8) (offset32 >> 8);
			buffer[pc + 3] = (U_8) (offset32);

			pc += 12;
			step = 8;
			if (bc == CFR_BC_tableswitch) {
				step = 4;
			}

			switchChildren = root->secondaryChild->nextBlock;
			while (switchChildren) {

				offset32 = (I_32) (switchChildren->secondaryChild->newPC - start);
				buffer[pc] = (U_8) (offset32 >> 24);
				buffer[pc + 1] = (U_8) (offset32 >> 16);
				buffer[pc + 2] = (U_8) (offset32 >> 8);
				buffer[pc + 3] = (U_8) (offset32);

				switchChildren = switchChildren->nextBlock;
				pc += step;
			}
		}
		root = root->primaryChild;
	}

	inlineBuffers->wideBranchesNeeded = FALSE;
	inlineBuffers->freePointer += (inlineBuffers->destBufferIndex + 3) & ~3;

	if (inlineBuffers->freePointer >= inlineBuffers->segmentEnd) {
		inlineBuffers->errorCode = BCT_ERR_OUT_OF_ROM;
		return;
	}

	inlineBuffers->bytesAddedByJSRInliner += (U_32) inlineBuffers->destBufferIndex - inlineBuffers->codeAttribute->codeLength;
	inlineBuffers->codeAttribute->codeLength = (U_32) inlineBuffers->destBufferIndex;
	inlineBuffers->codeAttribute->code = inlineBuffers->destBuffer;
	inlineBuffers->codeAttribute->originalCode = inlineBuffers->destBuffer;
}



/*
Description:
Creates and returns a new J9JSRIJSRData * initialized with the given information.

Args:
parent - the code block that is parent to the jsr opcode that asked for this to be created
previous - the previous JSRData structure in a chain
spawnPC - the address of the JSR opcode which corresponds to this instance
destinationPC - where the JSR originally wanted to jump to
pool - the J9Pool for allocating J9JSRIJSRData instances

Return:
The new instance

Notes:
Returns NULL on failure.
*/

static J9JSRIJSRData *
createJSRData(J9JSRICodeBlock * parent, J9JSRIJSRData * previous, U_32 spawnPC, U_32 destinationPC, J9JSRIData * inlineBuffers)
{
#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif
	J9JSRIJSRData *newData = pool_newElement(inlineBuffers->jsrDataPool);

	if (newData) {

		newData->parentBlock = parent;
		newData->previousJSR = previous;
		newData->stackBottom = ((U_8 *) newData) + sizeof(J9JSRIJSRData);
		newData->locals = newData->stackBottom + inlineBuffers->maxStack;
		newData->stack = newData->stackBottom;

		if (previous) {
			newData->stack += (previous->stack - previous->stackBottom);			
		}

		newData->spawnPC = spawnPC;
		newData->retPCPtr = &(newData->retPC);
		newData->retPC = JI_NO_RET_YET;
		newData->originalPC = destinationPC;
#if J9JSRI_DEBUG
		j9tty_printf(inlineBuffers->portLib, "	new jsrdata %p previous %p - jsr = %d, ret = %d\n", newData, newData->previousJSR, newData->spawnPC, newData->retPC);
#endif
	} else {
		inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
	}
	return newData;
}



/*
Description:
Creates and returns a new J9JSRIAddressMap * initialized with the given size

Args:
size - the length of the code block
portLib - the J9PortLibrary used to directly ask for memory (Address Maps don't use pools)

Return:
The new instance

Notes:
Returns NULL on failure.
*/

static void 
createNewMap(J9JSRIData * inlineBuffers)
{
	UDATA i, j, endPC;
	UDATA size = inlineBuffers->sourceBufferSize + 1;
	UDATA branchCount;
	J9CfrExceptionTableEntry *entry;
	J9CfrAttributeLineNumberTable *lineNumberTableAttribute;
	J9CfrAttributeLocalVariableTable *localVariableTableAttribute;

	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);

	if (inlineBuffers->mapSize < size) { /* Grow the buffer, too small to reuse */
		releaseMap (inlineBuffers);
		inlineBuffers->map = j9mem_allocate_memory ((UDATA) sizeof(J9JSRIAddressMap), J9MEM_CATEGORY_CLASSES);
#if J9JSRI_DEBUG
		j9tty_printf(inlineBuffers->portLib, "alloc map %p\n", inlineBuffers->map);
#endif

		if (inlineBuffers->map) {
			inlineBuffers->map->entries = j9mem_allocate_memory((UDATA) (size * sizeof(UDATA)), J9MEM_CATEGORY_CLASSES);
			inlineBuffers->map->reachable = j9mem_allocate_memory((UDATA) (size * sizeof(U_8)), J9MEM_CATEGORY_CLASSES);
			inlineBuffers->map->lineNumbers = j9mem_allocate_memory((UDATA) (size * sizeof(U_16)), J9MEM_CATEGORY_CLASSES);
		}

		if (!inlineBuffers->map || !inlineBuffers->map->entries || !inlineBuffers->map->reachable || !inlineBuffers->map->lineNumbers) {
			inlineBuffers->mapSize = 0;
			inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
			return;
		}

		inlineBuffers->mapSize = size;
	}

	memset(inlineBuffers->map->entries, 0, size * sizeof(UDATA));
	memset(inlineBuffers->map->reachable, J9JSRI_MAP_NOT_MARKED, size);
	memset(inlineBuffers->map->lineNumbers, 0, size * sizeof(U_16));

	branchCount = mapJumpTargets(inlineBuffers);

	/* Three elements are used to save a stack frame - pc, jsrData, and pointer to parent link slot */
	if (inlineBuffers->branchStack) {
		j9mem_free_memory(inlineBuffers->branchStack);
	}
	inlineBuffers->branchStack = j9mem_allocate_memory(branchCount * sizeof(J9JSRIBranch), J9MEM_CATEGORY_CLASSES);
	if (!inlineBuffers->branchStack) {
		inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
		return;
	}

	/* Used to mark block ends */
	for (i = 0; i < inlineBuffers->codeAttribute->exceptionTableLength; i++) {
		entry = &(inlineBuffers->codeAttribute->exceptionTable[i]);
		inlineBuffers->map->reachable[entry->startPC] |= J9JSRI_MAP_MARKED_END_BLOCK;
		inlineBuffers->map->reachable[entry->endPC] |= J9JSRI_MAP_MARKED_END_BLOCK;
		inlineBuffers->map->reachable[entry->handlerPC] |= J9JSRI_MAP_MARKED_END_BLOCK + J9JSRI_MAP_MARKED_EXCEPTION_HANDLER;
	}

	/* used to mark line number data points and local variable data points */
	if ((inlineBuffers->flags & BCT_StripDebugAttributes) == 0) {
		UDATA foundLineNumberTable = FALSE;

		for (i = 0; i < inlineBuffers->codeAttribute->attributesCount; i++) {
			if ((inlineBuffers->codeAttribute->attributes[i]->tag == CFR_ATTRIBUTE_LineNumberTable) 
					&& (((inlineBuffers->flags) & BCT_StripDebugLines) == 0)) {
				U_32 newPC;
				U_16 newLineNumber;
				/* Untag extra LineNumber tables - will output one merged table */
				if (foundLineNumberTable) {
					inlineBuffers->codeAttribute->attributes[i]->tag = CFR_ATTRIBUTE_Unknown;
				}
				foundLineNumberTable = TRUE;
				lineNumberTableAttribute = (J9CfrAttributeLineNumberTable *) inlineBuffers->codeAttribute->attributes[i];
				for (j = 0; j < lineNumberTableAttribute->lineNumberTableLength; j++) {
					newPC = lineNumberTableAttribute->lineNumberTable[j].startPC;
					newLineNumber = lineNumberTableAttribute->lineNumberTable[j].lineNumber;
					inlineBuffers->map->reachable[newPC] |= J9JSRI_MAP_MARKED_END_BLOCK;
					if (newLineNumber > inlineBuffers->map->lineNumbers[newPC]) {
						inlineBuffers->map->lineNumbers[newPC] = newLineNumber;
					}
				}
			}

			/* NB: local variable and local variable type tables are inclusive at the end point - either a valid bytecode or 1 past the end */
			if (((inlineBuffers->codeAttribute->attributes[i]->tag == CFR_ATTRIBUTE_LocalVariableTable)
						|| (inlineBuffers->codeAttribute->attributes[i]->tag == CFR_ATTRIBUTE_LocalVariableTypeTable))
					&& (((inlineBuffers->flags) & BCT_StripDebugVars) == 0)) {
				localVariableTableAttribute = (J9CfrAttributeLocalVariableTable *) inlineBuffers->codeAttribute->attributes[i];
				for (j = 0; j < localVariableTableAttribute->localVariableTableLength; j++) {
					inlineBuffers->map->reachable[localVariableTableAttribute->localVariableTable[j].startPC] |= J9JSRI_MAP_MARKED_END_BLOCK;
					endPC = localVariableTableAttribute->localVariableTable[j].startPC + localVariableTableAttribute->localVariableTable[j].length;
					/* reachable oversized by 1 to handle local variable end points one beyoind the code array */
					inlineBuffers->map->reachable[endPC] |= J9JSRI_MAP_MARKED_END_BLOCK;
				}
			}
		}
	}
}



/*
Description:
Creates and returns a new J9JSRICodeBlock * initialized with the data present in original

Args:
original - the instance to be duplicated
pool - the J9Pool for allocating J9JSRICodeBlock instances

Return:
The newly instantiated duplicate.

Notes:
Returns NULL on failure.
*/

static J9JSRIJSRData *
duplicateJSRDataChain(J9JSRIJSRData * original, J9JSRIData * inlineBuffers)
{
	if (original) {
		J9JSRIJSRData *copy = pool_newElement(inlineBuffers->jsrDataPool);

		if (copy) {
			copy->parentBlock = original->parentBlock;
			copy->previousJSR = duplicateJSRDataChain(original->previousJSR, inlineBuffers);
			copy->stackBottom = ((U_8 *) copy) + sizeof(J9JSRIJSRData);
			copy->locals = copy->stackBottom + inlineBuffers->maxStack;
			copy->spawnPC = original->spawnPC;
			copy->originalPC = original->originalPC;
			copy->retPCPtr = original->retPCPtr; /* Copy shares original's retPC */
			copy->retPC = JI_NO_RET_YET;

			memcpy(copy->stackBottom, original->stackBottom, inlineBuffers->maxStack);
			memcpy(copy->locals, original->locals, inlineBuffers->maxLocals);

			copy->stack = copy->stackBottom + (original->stack - original->stackBottom);
		} else 
			inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
		return copy;
	}
	return NULL;
}



/*
Description:
Walks the bytecodes in codeBlock starting with startingAddress until it reaches endOfBuffer or some 
instruction that would terminate this path of execution.  Creates a new J9JSRICodeBlock * and uses
the data it finds to describe it.
Marks the map as it is walking to describe what code is reachable and to determine if it is about to
loop.
Also updates information about the stack and variables to follow the movement of return addresses.

Args:
startingAddress - the address in the codeBlock buffer where this J9JSRICodeBlock is to start
jsrData - the stack, variable, and return address data from JSR subroutines that we are nested within
inlineBuffers - holds the buffers for inline operations

Return:
the code block created by iterating the buffer

Notes:
A return value of NULL indicates an error of some sort (recursive JSR, etc).
*/

static void
evaluateCodeBlock (I_32 hasRET, J9JSRICodeBlock ** blockParentPointer, U_32 startingAddress, J9JSRIJSRData * jsrData, J9JSRIData * inlineBuffers)
{
#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif

	U_32 destinationAddress;
	U_32 pc;
	U_32 step;
	U_8 bc, value, value2, value3, value4, variable8;
	U_8 *bcIndex, *args;
	U_16 constantIndex;
	U_16 variable16;
	I_16 offset;
	I_32 offset32, counter, defaultJump, npairs;
	U_32 start;
	U_8 stackError = 0;
	J9JSRIBranch *branchStack = inlineBuffers->branchStack;
	I_32 low, high;
	J9JSRICodeBlock *thisBlock;
	J9JSRICodeBlock *oldBlock;
	J9JSRICodeBlock *switchChildBlock;
	J9JSRIJSRData *newStack;
	J9CfrConstantPoolInfo *constantPool = inlineBuffers->constantPool;

_nextBranch:

	if (stackError) {
		goto _verifyError;
	}

	pc = startingAddress;
	start = pc;

	oldBlock = isMapMarkedAtLocation(startingAddress, jsrData, inlineBuffers);

	if (oldBlock) {
#if J9JSRI_DEBUG
		j9tty_printf(inlineBuffers->portLib, "existing block at %d - %p\n", pc, oldBlock);
#endif
		*blockParentPointer = oldBlock;
		goto _branchDone;
	}

	thisBlock = pool_newElement(inlineBuffers->codeBlockPool);

	if (thisBlock) {
		thisBlock->originalPC = pc;
		thisBlock->coloured = J9JSRI_COLOUR_FOR_UNWRITTEN;
#if J9JSRI_DEBUG
		j9tty_printf(inlineBuffers->portLib, "new block at %d - %p\n", pc, thisBlock);
#endif
		checkExceptionStart(thisBlock, jsrData, inlineBuffers);
		if (inlineBuffers->errorCode) {
			return;
		}

	} else {
		inlineBuffers->errorCode = BCT_ERR_OUT_OF_MEMORY;
		return;
	}

	*blockParentPointer = thisBlock;

	while (pc < inlineBuffers->sourceBufferSize) {
		start = pc;
		bc = inlineBuffers->sourceBuffer[pc];

		if (pc == startingAddress) {

			thisBlock->jsrData = jsrData;
			markMapAtLocation(pc, thisBlock, inlineBuffers);
			if (inlineBuffers->errorCode) {
				return;
			}

		} else {
			/* terminate blocks where exceptions start or end or at jump targets */
			if (inlineBuffers->map->reachable[pc] & J9JSRI_MAP_MARKED_END_BLOCK) {
				thisBlock->length = pc - startingAddress;
				if (isMapMarkedAtLocation(pc, jsrData, inlineBuffers)) {
					/* found a walked block at this location and jsrData matches */
					goto _branchDone;
				}
				/* block end found - start a new one */
				blockParentPointer = &thisBlock->primaryChild;
				startingAddress = pc;
				goto _nextBranch;
			}
		}

		switch (bc) {

		/*** Code to handle stack/var operations (start) ***/

		case CFR_BC_dconst_0:
		case CFR_BC_dconst_1:
		case CFR_BC_lconst_0:
		case CFR_BC_lconst_1:
		case CFR_BC_dload_0:
		case CFR_BC_dload_1:
		case CFR_BC_dload_2:
		case CFR_BC_dload_3:
		case CFR_BC_dload:
		case CFR_BC_lload_0:
		case CFR_BC_lload_1:
		case CFR_BC_lload_2:
		case CFR_BC_lload_3:
		case CFR_BC_lload:
		case CFR_BC_ldc2_w:
			pushZeroOntoStack(jsrData); /* fall through case */

		case CFR_BC_aconst_null:
		case CFR_BC_fconst_0:
		case CFR_BC_fconst_1:
		case CFR_BC_fconst_2:
		case CFR_BC_iconst_m1:
		case CFR_BC_iconst_0:
		case CFR_BC_iconst_1:
		case CFR_BC_iconst_2:
		case CFR_BC_iconst_3:
		case CFR_BC_iconst_4:
		case CFR_BC_iconst_5:
		case CFR_BC_fload_0:
		case CFR_BC_fload_1:
		case CFR_BC_fload_2:
		case CFR_BC_fload_3:
		case CFR_BC_fload:
		case CFR_BC_iload_0:
		case CFR_BC_iload_1:
		case CFR_BC_iload_2:
		case CFR_BC_iload_3:
		case CFR_BC_iload:
		case CFR_BC_bipush:
		case CFR_BC_sipush:
		case CFR_BC_ldc:
		case CFR_BC_ldc_w:
		case CFR_BC_new:
			stackError = pushZeroOntoStack(jsrData);
			break;

		case CFR_BC_aload_0:
		case CFR_BC_aload_1:
		case CFR_BC_aload_2:
		case CFR_BC_aload_3:
			variable8 = (U_8) (bc - CFR_BC_aload_0);
			if (jsrData && (variable8 < inlineBuffers->maxLocals)) {
				if (jsrData->locals[variable8]) {
					/* Jazz 82615: Set the verbose error type and the local variable index in the case of illegal load operation */
					inlineBuffers->verboseErrorType = BCV_ERR_JSR_ILLEGAL_LOAD_OPERATION;
					inlineBuffers->errorLocalIndex = (U_32)variable8;
					goto _verifyError;
				}
			}
			stackError = pushZeroOntoStack(jsrData);
			break;

		case CFR_BC_aload:
			bcIndex = inlineBuffers->sourceBuffer + pc + 1;
			NEXT_U8(variable8, bcIndex);
			if (jsrData && (variable8 < inlineBuffers->maxLocals)) {
				if (jsrData->locals[variable8]) {
					/* Jazz 82615: Set the verbose error type and the local variable index in the case of illegal load operation */
					inlineBuffers->verboseErrorType = BCV_ERR_JSR_ILLEGAL_LOAD_OPERATION;
					inlineBuffers->errorLocalIndex = (U_32)variable8;
					goto _verifyError;
				}
			}
			stackError = pushZeroOntoStack(jsrData);
			break;

		case CFR_BC_dcmpg:
		case CFR_BC_dcmpl:
		case CFR_BC_lcmp:
			stackError = popStack (jsrData);
			stackError |= popStack (jsrData); /* fall through */

		case CFR_BC_aaload:
		case CFR_BC_baload:
		case CFR_BC_caload:
		case CFR_BC_faload:
		case CFR_BC_iaload:
		case CFR_BC_saload:
		case CFR_BC_fadd:
		case CFR_BC_fsub:
		case CFR_BC_fmul:
		case CFR_BC_fdiv:
		case CFR_BC_frem:
		case CFR_BC_fcmpg:
		case CFR_BC_fcmpl:
		case CFR_BC_iadd:
		case CFR_BC_isub:
		case CFR_BC_imul:
		case CFR_BC_idiv:
		case CFR_BC_irem:
		case CFR_BC_ishl:
		case CFR_BC_ishr:
		case CFR_BC_iushr:
		case CFR_BC_iand:
		case CFR_BC_ior:
		case CFR_BC_ixor:
		case CFR_BC_d2f:
		case CFR_BC_d2i:
		case CFR_BC_l2f:
		case CFR_BC_l2i:
			stackError |= popStack (jsrData);
			stackError |= popStack (jsrData);
			pushZeroOntoStack(jsrData);
			break;

		case CFR_BC_f2d:
		case CFR_BC_f2l:
		case CFR_BC_i2d:
		case CFR_BC_i2l:
			stackError = popStack (jsrData);
			pushZeroOntoStack(jsrData);
			stackError |= pushZeroOntoStack(jsrData);
			break;

		case CFR_BC_dadd:
		case CFR_BC_dsub:
		case CFR_BC_dmul:
		case CFR_BC_ddiv:
		case CFR_BC_drem:
		case CFR_BC_ladd:
		case CFR_BC_lsub:
		case CFR_BC_lmul:
		case CFR_BC_ldiv:
		case CFR_BC_lrem:
		case CFR_BC_land:
		case CFR_BC_lor:
		case CFR_BC_lxor:
			stackError = popStack (jsrData);
			stackError |= popStack (jsrData);
			stackError |= popStack (jsrData);
			stackError |= popStack (jsrData);
			pushZeroOntoStack(jsrData);
			pushZeroOntoStack(jsrData);
			break;

		case CFR_BC_lshl:
		case CFR_BC_lshr:
		case CFR_BC_lushr:
			stackError = popStack (jsrData);
			stackError |= popStack (jsrData);
			stackError |= popStack (jsrData);
			pushZeroOntoStack(jsrData);
			pushZeroOntoStack(jsrData);
			break;

		case CFR_BC_daload:
		case CFR_BC_laload:
		case CFR_BC_dneg:
		case CFR_BC_lneg:
		case CFR_BC_l2d:
		case CFR_BC_d2l:
			stackError = popStack (jsrData);
			stackError |= popStack (jsrData);
			pushZeroOntoStack(jsrData);
			pushZeroOntoStack(jsrData);
			break;

		case CFR_BC_fneg:
		case CFR_BC_ineg:
		case CFR_BC_f2i:
		case CFR_BC_i2f:
		case CFR_BC_i2b:
		case CFR_BC_i2c:
		case CFR_BC_i2s:
		case CFR_BC_checkcast:
		case CFR_BC_instanceof:
		case CFR_BC_arraylength:
		case CFR_BC_newarray:
		case CFR_BC_anewarray:
			stackError = popStack (jsrData);
			pushZeroOntoStack(jsrData);
			break;

		case CFR_BC_astore:
			bcIndex = inlineBuffers->sourceBuffer + pc + 1;
			value = popStack(jsrData);
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
			NEXT_U8(variable8, bcIndex);
			loadIntoVariable(inlineBuffers, jsrData, value, variable8);
			break;

		case CFR_BC_astore_0:
		case CFR_BC_astore_1:
		case CFR_BC_astore_2:
		case CFR_BC_astore_3:
			value = popStack(jsrData);
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
			variable8 = (U_8) (bc - CFR_BC_astore_0);
			loadIntoVariable(inlineBuffers, jsrData, value, variable8);
			break;

		case CFR_BC_dastore:
		case CFR_BC_lastore:
			stackError = popStack (jsrData);
			/* fall through */

		case CFR_BC_aastore:
		case CFR_BC_bastore:
		case CFR_BC_castore:
		case CFR_BC_fastore:
		case CFR_BC_iastore:
		case CFR_BC_sastore:
			stackError |= popStack (jsrData);
			/* fall through */

		case CFR_BC_dstore_0:
		case CFR_BC_dstore_1:
		case CFR_BC_dstore_2:
		case CFR_BC_dstore_3:
		case CFR_BC_dstore:
		case CFR_BC_lstore_0:
		case CFR_BC_lstore_1:
		case CFR_BC_lstore_2:
		case CFR_BC_lstore_3:
		case CFR_BC_lstore:
			stackError |= popStack (jsrData);
			/* fall through */

		case CFR_BC_fstore_0:
		case CFR_BC_fstore_1:
		case CFR_BC_fstore_2:
		case CFR_BC_fstore_3:
		case CFR_BC_fstore:
		case CFR_BC_istore_0:
		case CFR_BC_istore_1:
		case CFR_BC_istore_2:
		case CFR_BC_istore_3:
		case CFR_BC_istore:
		case CFR_BC_monitorenter:
		case CFR_BC_monitorexit:
			stackError |= popStack (jsrData);
			break;

		/*	START OF DUP CODES */
		/* DUPS can legally duplicate return addresses */

		case CFR_BC_dup:
			value = popStack(jsrData);
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
			pushOntoStack(jsrData, value);
			stackError |= pushOntoStack(jsrData, value);
			break;

		case CFR_BC_dup_x1:
			value = popStack(jsrData);
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
			value2 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value2) ? JI_STACK_UNDERFLOW : 0;
			pushOntoStack(jsrData, value);
			pushOntoStack(jsrData, value2);
			stackError |= pushOntoStack(jsrData, value);
			break;

		case CFR_BC_dup_x2:
			value = popStack(jsrData);
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
			value2 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value2) ? JI_STACK_UNDERFLOW : 0;
			value3 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value3) ? JI_STACK_UNDERFLOW : 0;
			pushOntoStack(jsrData, value);
			pushOntoStack(jsrData, value3);
			pushOntoStack(jsrData, value2);
			stackError |= pushOntoStack(jsrData, value);
			break;

		case CFR_BC_dup2:
			value = popStack(jsrData);
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
			value2 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value2) ? JI_STACK_UNDERFLOW : 0;
			pushOntoStack(jsrData, value2);
			pushOntoStack(jsrData, value);
			stackError |= pushOntoStack(jsrData, value2);
			stackError |= pushOntoStack(jsrData, value);
			break;

		case CFR_BC_dup2_x1:
			value = popStack(jsrData);
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
			value2 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value2) ? JI_STACK_UNDERFLOW : 0;
			value3 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value3) ? JI_STACK_UNDERFLOW : 0;
			pushOntoStack(jsrData, value2);
			pushOntoStack(jsrData, value);
			pushOntoStack(jsrData, value3);
			stackError |= pushOntoStack(jsrData, value2);
			stackError |= pushOntoStack(jsrData, value);
			break;

		case CFR_BC_dup2_x2:
			value = popStack(jsrData);
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
			value2 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value2) ? JI_STACK_UNDERFLOW : 0;
			value3 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value3) ? JI_STACK_UNDERFLOW : 0;
			value4 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value4) ? JI_STACK_UNDERFLOW : 0;
			pushOntoStack(jsrData, value2);
			pushOntoStack(jsrData, value);
			pushOntoStack(jsrData, value4);
			pushOntoStack(jsrData, value3);
			stackError |= pushOntoStack(jsrData, value2);
			stackError |= pushOntoStack(jsrData, value);
			break;

		case CFR_BC_swap:
			value = popStack(jsrData);
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
			value2 = popStack(jsrData);
			stackError |= (JI_STACK_UNDERFLOW == value2) ? JI_STACK_UNDERFLOW : 0;
			pushOntoStack(jsrData, value);
			pushOntoStack(jsrData, value2);
			break;

		/*	END OF DUP CODES */

		case CFR_BC_getfield:
			stackError = popStack(jsrData);
			/* fall through */

		case CFR_BC_getstatic:
			bcIndex = inlineBuffers->sourceBuffer + pc + 1;
			NEXT_U16(constantIndex, bcIndex);
			variable8 = constantPool[constantPool[constantPool[constantIndex].slot2].slot2].bytes[0];
			pushZeroOntoStack(jsrData);
			if ((variable8 == 'J') || (variable8 == 'D')) {
				stackError |= pushZeroOntoStack(jsrData);
			}
			break;

		case CFR_BC_invokeinterface:
		case CFR_BC_invokespecial:
		case CFR_BC_invokevirtual:
		case CFR_BC_invokestatic:
			bcIndex = inlineBuffers->sourceBuffer + pc + 1;
			NEXT_U16(constantIndex, bcIndex);
			if (bc == CFR_BC_invokeinterface) {
				NEXT_U8(variable8, bcIndex);
				while (variable8--) {
					stackError |= popStack (jsrData);
				}
			} else {
				args = constantPool[constantPool[constantPool[constantIndex].slot2].slot2].bytes;
				for (step = 1; args[step] != ')'; step++) {
					stackError |= popStack (jsrData);
					if (args[step] == '[') {
						while (args[++step] == '[');
						if (args[step] != 'L') {
							continue;
						}
					}
					if (args[step] == 'L') {
						while (args[++step] != ';');
						continue;
					}
					if ((args[step] == 'D') || (args[step] == 'J')) {
						stackError |= popStack (jsrData);
					}
				}

				if (bc != CFR_BC_invokestatic) {
					stackError |= popStack (jsrData);
				}
			}
			stackError |= pushReturnOntoStack(constantIndex, constantPool, jsrData);
			break;

		case CFR_BC_multianewarray:
			bcIndex = inlineBuffers->sourceBuffer + pc + 3;
			NEXT_U8(variable8, bcIndex);
			while (variable8--) {
				stackError |= popStack (jsrData);
			}
			pushZeroOntoStack(jsrData);
			break;

		/* Pops can legally pop return addresses */

		case CFR_BC_pop2:
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError = (JI_STACK_UNDERFLOW == popStack(jsrData)) ? JI_STACK_UNDERFLOW : 0; /* fall through */

		case CFR_BC_pop:
			/* Jazz 82615: Set the error code in the case of stack underflow */
			stackError |= (JI_STACK_UNDERFLOW == popStack(jsrData)) ? JI_STACK_UNDERFLOW : 0;
			break;

		case CFR_BC_putfield:
			stackError = popStack(jsrData);
			/* fall through */

		case CFR_BC_putstatic:
			bcIndex = inlineBuffers->sourceBuffer + pc + 1;
			NEXT_U16(constantIndex, bcIndex);
			variable8 = constantPool[constantPool[constantPool[constantIndex].slot2].slot2].bytes[0];
			stackError |= popStack(jsrData);
			if ((variable8 == 'J') || (variable8 == 'D')) {
				stackError |= popStack(jsrData);
			}
			break;

		/*** Code to handle stack/var operations (end) ***/

		case CFR_BC_ret:
			/* Found a ret clause so terminate this branch */
			thisBlock->secondaryChild = getCodeBlockParentInChainWithVar(jsrData, inlineBuffers->sourceBuffer[pc + 1], pc, inlineBuffers);
			if (inlineBuffers->errorCode) {
				return;
			}
			if (isRetMultiplyUsed (startingAddress, inlineBuffers)) {
				goto _verifyError;
			}

			pc += 2;
			thisBlock->length = pc - startingAddress;
			thisBlock->coloured |= J9JSRI_COLOUR_FOR_RET;

			if (!thisBlock->secondaryChild->secondaryChild) {
				while (jsrData->spawnPC == thisBlock->secondaryChild->originalPC) {
#if J9JSRI_DEBUG
					j9tty_printf(inlineBuffers->portLib, "unroll jsrData\n");
#endif
					jsrData = jsrData->previousJSR;
				}

				blockParentPointer = &thisBlock->secondaryChild->secondaryChild;
				startingAddress = thisBlock->secondaryChild->originalPC + thisBlock->secondaryChild->length;
				goto _nextBranch;
			}
			goto _branchDone;
			break;

		case CFR_BC_lreturn:
		case CFR_BC_dreturn:
			stackError = popStack(jsrData);
			/* fall through */

		case CFR_BC_ireturn:
		case CFR_BC_freturn:
		case CFR_BC_areturn:
		case CFR_BC_athrow:
			stackError |= popStack(jsrData);
			/* fall through */

		case CFR_BC_return:
			/* Found a return/throw clause so terminate this branch */
			pc ++;
			thisBlock->length = pc - startingAddress;
			goto _branchDone;
			break;

		case CFR_BC_wide:
			switch (inlineBuffers->sourceBuffer[pc + 1]) {

			case CFR_BC_iinc:
				pc += 2; /* additional 3 added at wide switch end */
				break;

			case CFR_BC_lstore:
			case CFR_BC_dstore:
				stackError = popStack(jsrData);
				/* fall through */

			case CFR_BC_fstore:
			case CFR_BC_istore:
				stackError |= popStack(jsrData);
				break;

			case CFR_BC_astore:
				bcIndex = inlineBuffers->sourceBuffer + pc + 2;
				value = popStack(jsrData);
				/* Jazz 82615: Set the error code in the case of stack underflow */
				stackError = (JI_STACK_UNDERFLOW == value) ? JI_STACK_UNDERFLOW : 0;
				NEXT_U16(variable16, bcIndex);
				loadIntoVariable(inlineBuffers, jsrData, value, variable16);
				break;

			case CFR_BC_dload:
			case CFR_BC_lload:
				stackError = pushZeroOntoStack(jsrData); /* fall through */

			case CFR_BC_iload:
			case CFR_BC_fload:
				stackError |= pushZeroOntoStack(jsrData);
				break;

			case CFR_BC_aload:
				bcIndex = inlineBuffers->sourceBuffer + pc + 2;
				NEXT_U16(variable16, bcIndex);
				if (jsrData && ((UDATA) variable16 < inlineBuffers->maxLocals)) {
					if (jsrData->locals[variable16]) {
						/* Jazz 82615: Set the verbose error type and the local variable index in the case of illegal load operation */
						inlineBuffers->verboseErrorType = BCV_ERR_JSR_ILLEGAL_LOAD_OPERATION;
						inlineBuffers->errorLocalIndex = (U_32)variable16;
						goto _verifyError;
					}
				}
				stackError |= pushZeroOntoStack(jsrData);
				break;

			case CFR_BC_ret:
				/* Found a ret clause so terminate this branch */
				bcIndex = inlineBuffers->sourceBuffer + pc + 2;
				NEXT_U16(variable16, bcIndex);
				thisBlock->secondaryChild = getCodeBlockParentInChainWithVar(jsrData, variable16, pc+1, inlineBuffers);
				if (inlineBuffers->errorCode) {
					return;
				}
				if (isRetMultiplyUsed (startingAddress, inlineBuffers)) {
					goto _verifyError;
				}

				pc += 4;
				thisBlock->length = pc - startingAddress;
				thisBlock->coloured |= J9JSRI_COLOUR_FOR_RET | J9JSRI_COLOUR_FOR_WIDE;

				if (!thisBlock->secondaryChild->secondaryChild) {
					while (jsrData->spawnPC == thisBlock->secondaryChild->originalPC) {
#if J9JSRI_DEBUG
						j9tty_printf(inlineBuffers->portLib, "unroll jsrData\n");
#endif
						jsrData = jsrData->previousJSR;
					}

					blockParentPointer = &thisBlock->secondaryChild->secondaryChild;
					startingAddress = thisBlock->secondaryChild->originalPC + thisBlock->secondaryChild->length;
					goto _nextBranch;
				}
				goto _branchDone;
				break;

			}
			pc += 3; /* remaining 1 added at bc switch end */
			break;

		/* check for branches */

		case CFR_BC_if_icmpeq:
		case CFR_BC_if_icmpne:
		case CFR_BC_if_icmplt:
		case CFR_BC_if_icmpge:
		case CFR_BC_if_icmpgt:
		case CFR_BC_if_icmple:
		case CFR_BC_if_acmpeq:
		case CFR_BC_if_acmpne:
			stackError = popStack(jsrData);
			/* fall through */

		case CFR_BC_ifeq:
		case CFR_BC_ifne:
		case CFR_BC_iflt:
		case CFR_BC_ifge:
		case CFR_BC_ifgt:
		case CFR_BC_ifle:
		case CFR_BC_ifnull:
		case CFR_BC_ifnonnull:
			stackError |= popStack(jsrData);
			/* Found a branch so fork the structure */
			bcIndex = inlineBuffers->sourceBuffer + start + 1;

			newStack = duplicateJSRDataChain(jsrData, inlineBuffers);
			if (inlineBuffers->errorCode) {
				return;
			}

			pc += 3;
			thisBlock->length = pc - startingAddress;
			thisBlock->coloured |= J9JSRI_COLOUR_FOR_BRANCH;

			destinationAddress = start + NEXT_I16(offset, bcIndex);

			/* Push a branch for later walking */
			branchStack->startPC = pc;
			branchStack->jsrData = jsrData;
			branchStack->blockParentPointer = &thisBlock->primaryChild;
			branchStack++;

			blockParentPointer = &thisBlock->secondaryChild;
			jsrData = newStack;
			startingAddress = destinationAddress;
			goto _nextBranch;
			break;

		case CFR_BC_tableswitch:
		case CFR_BC_lookupswitch:
			stackError = popStack(jsrData);
			pc += (4 - (pc & 3));
			bcIndex = inlineBuffers->sourceBuffer + pc;
			NEXT_I32(defaultJump, bcIndex);
			NEXT_I32(low, bcIndex);
			pc += 8;
			high = low;
			step = 4;
			if (bc == CFR_BC_tableswitch) {
				NEXT_I32(high, bcIndex);
				pc += 4;
				step = 0;
				npairs = high - low + 1;
				pc += npairs * 4;
			} else {
				npairs = low;
				pc += npairs * 8;
			}

			thisBlock->length = pc - startingAddress;
			thisBlock->coloured |= J9JSRI_COLOUR_FOR_SWITCH;

			switchChildBlock = addSwitchChildToBlock(thisBlock, inlineBuffers);
			if (inlineBuffers->errorCode) {
				return;
			}
			blockParentPointer = &switchChildBlock->secondaryChild;
			startingAddress = start + defaultJump;

			for (counter = 0; counter < npairs; counter++) {
				bcIndex += step;
				NEXT_I32(offset32, bcIndex);
				newStack = duplicateJSRDataChain(jsrData, inlineBuffers);
				if (inlineBuffers->errorCode) {
					return;
				}

				switchChildBlock = addSwitchChildToBlock(thisBlock, inlineBuffers);
				if (inlineBuffers->errorCode) {
					return;
				}

				/* Push a branch for later walking */
				branchStack->startPC = start + offset32;
				branchStack->jsrData = newStack;
				branchStack->blockParentPointer = &switchChildBlock->secondaryChild;
				branchStack++;
			}
			goto _nextBranch;
			break;

		case CFR_BC_goto:
		case CFR_BC_goto_w:
			/* Found a new code block */
			bcIndex = inlineBuffers->sourceBuffer + start + 1;
			pc += 3;
			if (bc == CFR_BC_goto) {
				destinationAddress = start + NEXT_I16(offset, bcIndex);
			} else {
				destinationAddress = start + NEXT_I32(offset32, bcIndex);
				pc += 2;
				thisBlock->coloured |= J9JSRI_COLOUR_FOR_WIDE;
			}

			thisBlock->length = pc - startingAddress;
			thisBlock->coloured |= J9JSRI_COLOUR_FOR_BRANCH;

			blockParentPointer = &thisBlock->secondaryChild;
			startingAddress = destinationAddress;
			goto _nextBranch;
			break;

		case CFR_BC_jsr:
		case CFR_BC_jsr_w:
			thisBlock->coloured |= J9JSRI_COLOUR_FOR_JSR;
			bcIndex = inlineBuffers->sourceBuffer + start + 1;
			pc += 3;
			if (bc == CFR_BC_jsr) {
				destinationAddress = (U_32) (start + NEXT_I16(offset, bcIndex));
			} else {
				destinationAddress = (U_32) (start + NEXT_I32(offset32, bcIndex));
				pc += 2;
			}

			/* Will be replaced by an aconst_null */
			thisBlock->length = pc - startingAddress;

			if (hasRET) {
				/* Abandon original and make a new JSR Data for following flows - for reuse of a local to save a return address */
				jsrData = duplicateJSRDataChain(jsrData, inlineBuffers);
			}
			newStack = createJSRData(thisBlock, jsrData, start, destinationAddress, inlineBuffers);
			if (isJSRRecursive(newStack, inlineBuffers)) {
				goto _verifyError;
			}
			stackError = pushOntoStack(newStack, JI_RET_ADDRESS);

			blockParentPointer = &thisBlock->primaryChild;
			jsrData = newStack;
			startingAddress = destinationAddress;
			goto _nextBranch;
			break;

		default:
			if (bc > CFR_BC_jsr_w) {
				inlineBuffers->errorCode = BCT_ERR_INVALID_BYTECODE;
				inlineBuffers->verifyError = J9NLS_BCV_ERR_BC_UNKNOWN__ID;
				inlineBuffers->verifyErrorPC = start;
				goto _verifyError;
			}
		}
		if (stackError) {
			goto _verifyError;
		}
		pc += (sunJavaByteCodeRelocation[bc] & 7);
	}

	inlineBuffers->verifyError = J9NLS_BCV_ERR_UNEXPECTED_EOF__ID;
	goto _verifyError;

_branchDone:

	if (stackError) {
		goto _verifyError;
	}
	if (branchStack != inlineBuffers->branchStack) {
		/* Pop another branch to walk */
		branchStack--;
		startingAddress = (U_32)branchStack->startPC;
		jsrData = branchStack->jsrData;
		blockParentPointer = branchStack->blockParentPointer;
		goto _nextBranch;
	}

	return;

_verifyError:
	/* Jazz 82615: Set the verbose error type for the error message framework in the case of stack underflow/overflow */
	switch (stackError) {
	case JI_STACK_UNDERFLOW:
		inlineBuffers->verboseErrorType = BCV_ERR_JSR_STACK_UNDERFLOW;
		break;
	case JI_STACK_OVERFLOW:
		inlineBuffers->verboseErrorType = BCV_ERR_JSR_STACK_OVERFLOW;
		break;
	case JI_RET_ADDRESS:
		/* If JI_RET_ADDRESS is pushed onto the stack previously in the case of jsr/jsr_w (see case CFR_BC_jsr/jsr_w),
		 * then it triggers a verification error when JI_RET_ADDRESS gets popped up from the stack after that
		 * as the current opcode is expecting other data type (represented by zero) on the stack.
		 */
		inlineBuffers->verboseErrorType = BCV_ERR_JSR_RET_ADDRESS_ON_STACK;
		break;
	default:
		/* Zero means there is no stack-related error */
		break;
	}

	if (!inlineBuffers->errorCode) {
		inlineBuffers->errorCode = BCT_ERR_VERIFY_ERROR_INLINING;
		if (!inlineBuffers->verifyError) {
			inlineBuffers->verifyError = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID; 
		}
		if (inlineBuffers->verifyErrorPC == (UDATA) -1) {
			inlineBuffers->verifyErrorPC = start;
		}
	}
	return;
}


/*

Lays down the smallest source address element.

*/

static void 
flattenCodeBlockHeirarchyToList(J9JSRICodeBlock * root, J9JSRIData * inlineBuffers)
{
#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif

	J9JSRICodeBlock *previous;
	J9JSRIJSRData *jsrData;
	U_32 alignmentChange;
	UDATA startAddress;

	if (root) {

		startAddress = root->originalPC;

		jsrData = root->jsrData;

		previous = inlineBuffers->lastOutput;

		if (previous && (previous->coloured & J9JSRI_COLOUR_FOR_JSR)) {
			/* If first block in jsr starts with an "astore", */
			if (inlineBuffers->map->reachable[root->originalPC] & J9JSRI_MAP_MARKED_JSR_COLLAPSIBLE) {
				/* backup and remove the aconst_null and remove the astore */
				previous->coloured &= (~(J9JSRI_COLOUR_FOR_JSR | J9JSRI_COLOUR_FOR_WIDE));
				previous->length = 0;
				inlineBuffers->destBufferIndex -= 1;
				root->length = 0;
			}
			root->coloured |= J9JSRI_COLOUR_FOR_JSR_START;
		}

		while (root) {

			/* Check for room */
			if ((root->length + 5 + inlineBuffers->destBufferIndex) >= inlineBuffers->destBufferSize) {
				inlineBuffers->errorCode = BCT_ERR_OUT_OF_ROM;
				return;
			}

			root->newPC = (U_32) inlineBuffers->destBufferIndex;
			alignmentChange = 0;

			/* align switch blocks on same offset as the original */
			if (root->coloured & J9JSRI_COLOUR_FOR_SWITCH) {
				while ((inlineBuffers->destBufferIndex & 3) != (root->originalPC & 3)) {
					inlineBuffers->destBuffer[inlineBuffers->destBufferIndex++] = CFR_BC_nop;
					alignmentChange++;
				}
				root->coloured |= alignmentChange;
			}

#if J9JSRI_DEBUG
			j9tty_printf(inlineBuffers->portLib, "%p %d at %d\n", root, root->originalPC, inlineBuffers->destBufferIndex);
#endif

			if (root->coloured & J9JSRI_COLOUR_FOR_JSR) {
				/* Output the aconst_null to spoof the return address */
				inlineBuffers->destBuffer[inlineBuffers->destBufferIndex] = CFR_BC_aconst_null;
				root->length = 1;

			} else {
				memcpy (&(inlineBuffers->destBuffer)[inlineBuffers->destBufferIndex], &(inlineBuffers->sourceBuffer)[root->originalPC], root->length);
			}

			inlineBuffers->destBufferIndex += root->length;
			root->length += alignmentChange;

			if (inlineBuffers->lastOutput) {
				inlineBuffers->lastOutput->primaryChild = root;
			}
			inlineBuffers->lastOutput = root;

			/* leave room to put a goto_w over a ret */
			if (root->coloured & J9JSRI_COLOUR_FOR_RET) {
				if (root->coloured & J9JSRI_COLOUR_FOR_WIDE) {
					inlineBuffers->destBufferIndex += 1;
					root->length += 1;
				} else {
					inlineBuffers->destBufferIndex += 3;
					root->length += 3;
				}
			}

			root->coloured &= (~J9JSRI_COLOUR_FOR_UNWRITTEN);

			if (root->coloured & J9JSRI_COLOUR_FOR_JSR) {
				/* Write out jsr bodies as encountered */
				flattenCodeBlockHeirarchyToList(root->primaryChild, inlineBuffers);

				if (inlineBuffers->errorCode) {
					return;
				}
				inlineBuffers->lastOutput->coloured |= J9JSRI_COLOUR_FOR_JSR_END;
			}

			root = inlineBuffers->lastOutput->primaryChild;

			if (root) {
				/* Most common case - follow the primaryChild link */
				startAddress = root->originalPC;

			} else {
				root = getNextGreatestBlock(jsrData, &startAddress, inlineBuffers);

				if (root == NULL) {
					startAddress = 0;
					root = getNextGreatestBlock(jsrData, &startAddress, inlineBuffers);
				}

				previous = inlineBuffers->lastOutput;

				if (previous && (previous->coloured & J9JSRI_COLOUR_FOR_RET)) {
					/* If ret block will "goto" the following block then remove the goto_w space and RET/WIDE tags */

					if ((previous->secondaryChild) && (previous->secondaryChild->secondaryChild == root)) {
						previous->coloured &= (~(J9JSRI_COLOUR_FOR_RET | J9JSRI_COLOUR_FOR_WIDE));
						previous->length = 0;
						inlineBuffers->destBufferIndex -= 5;
					}
				}
			}
		}
	}
}



/*

Lays down the blocks again with wide jumps - reuse the output order linking.

*/

static void 
flattenCodeBlocksWide (J9JSRIData * inlineBuffers)
{
#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif

	J9JSRICodeBlock *root = inlineBuffers->firstOutput;
	UDATA alignmentChange = 0;
	U_8 *modifyByte;

	/* reset the previous output */
	inlineBuffers->destBufferIndex = 0;
	inlineBuffers->freePointer = inlineBuffers->destBuffer;

	while (root) {

		/* Check for room */
		if ((root->length + 5 + inlineBuffers->destBufferIndex) >= inlineBuffers->destBufferSize) {
			inlineBuffers->errorCode = BCT_ERR_OUT_OF_ROM;
			return;
		}

		alignmentChange = inlineBuffers->destBufferIndex;

		/* align switch blocks on same offset as the original */
		if (root->coloured & J9JSRI_COLOUR_FOR_SWITCH) {
			/* Remove the 0-3 nops padded into the normal flattening */
			alignmentChange = inlineBuffers->destBufferIndex;
			root->length -= (root->coloured & J9JSRI_COLOUR_SWITCH_PAD_MASK);
			while ((inlineBuffers->destBufferIndex & 3) != (root->originalPC & 3)) {
				inlineBuffers->destBuffer[inlineBuffers->destBufferIndex++] = CFR_BC_nop;
			}
		}

		alignmentChange = inlineBuffers->destBufferIndex - alignmentChange;

#if J9JSRI_DEBUG
		j9tty_printf(inlineBuffers->portLib, "%p %d at %d - wide\n", root, root->originalPC, inlineBuffers->destBufferIndex);
#endif

		root->newPC = (U_32) inlineBuffers->destBufferIndex;

		if (root->coloured & J9JSRI_COLOUR_FOR_JSR) {
			/* Output the aconst_null to spoof the return address */
			inlineBuffers->destBuffer[inlineBuffers->destBufferIndex] = CFR_BC_aconst_null;
			root->length = 1;

		} else {
			memcpy (&(inlineBuffers->destBuffer)[inlineBuffers->destBufferIndex], &(inlineBuffers->sourceBuffer)[root->originalPC], root->length);
		}

		inlineBuffers->destBufferIndex += root->length;

		/* adjust for the pre-block adjustments */
		root->newPC -= (U_32) alignmentChange;
		root->length += (U_32) alignmentChange;

		/* replace all short branches with long branches if flagged as necessary */
		if ((root->coloured & J9JSRI_COLOUR_FOR_BRANCH) && !(root->coloured & J9JSRI_COLOUR_FOR_WIDE)) {
			root->coloured |= J9JSRI_COLOUR_FOR_WIDE;
			alignmentChange = inlineBuffers->destBufferIndex;
			modifyByte = &(inlineBuffers->destBuffer[inlineBuffers->destBufferIndex - 3]);
			if (*modifyByte == CFR_BC_goto) {
				*modifyByte = CFR_BC_goto_w;
				inlineBuffers->destBufferIndex += 2;
			} else {
				/* Reverse if logic */
				if (*modifyByte < CFR_BC_ifnull) {
					/* ifs are in odd/even pairs */
					if (*modifyByte & 1) {
						*modifyByte += 1;
					} else {
						*modifyByte -= 1;
					}
				} else {
					/* if null/nonnull are in even/odd pair */
					if (*modifyByte == CFR_BC_ifnull) {
						*modifyByte = CFR_BC_ifnonnull;
					} else {
						*modifyByte = CFR_BC_ifnull;
					}
				}
				modifyByte++;
				/* Set the jump of 8 over the goto_w */
				*modifyByte++ = 0;
				*modifyByte = 8;
				inlineBuffers->destBuffer[inlineBuffers->destBufferIndex++] = CFR_BC_goto_w;
				inlineBuffers->destBufferIndex += 4;
			}

			/* add the post block adjustments */
			root->length += (U_32) (inlineBuffers->destBufferIndex - alignmentChange);
		}

		root = root->primaryChild;
	}
}



/*

Looks up the J9JSRICodeBlock that we need to return from by finding the first JSRData in rooted at root with a return address in variable number variable

sets the value in (*error) to non-zero if the jsr already had a ret

*/

static J9JSRICodeBlock *
getCodeBlockParentInChainWithVar(J9JSRIJSRData * root, U_16 variable, U_32 currentPC, J9JSRIData * inlineBuffers)
{
	J9JSRIJSRData * previousRoot;
#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif

	while (root) {
		if (root->locals[variable] == JI_RET_ADDRESS) {
			root->locals[variable] = JI_USED_RET_ADDRESS;
			if (*(root->retPCPtr) == (U_32) JI_NO_RET_YET) {
				/* set the ret at this pc */
				*(root->retPCPtr) = currentPC;
			}
			previousRoot = root;
			while (previousRoot->previousJSR) {
				previousRoot = previousRoot->previousJSR;
				if (previousRoot->locals[variable] == JI_RET_ADDRESS) {
					previousRoot->locals[variable] = JI_USED_RET_ADDRESS;
				}
			}
			inlineBuffers->errorCode =	(*(root->retPCPtr) != currentPC);
			if (inlineBuffers->errorCode) {
				inlineBuffers->errorCode = BCT_ERR_VERIFY_ERROR_INLINING;
				inlineBuffers->verifyError = J9NLS_BCV_ERR_MULTIPLE_RET__ID;
				/* Set the current PC value to show up in the error message framework */
				inlineBuffers->verifyErrorPC = currentPC;
#if J9JSRI_DEBUG
				j9tty_printf(inlineBuffers->portLib, "[][][] Verify Error multiple rets\n");
#endif
			}
			return root->parentBlock;
		}
		root = root->previousJSR;
	}
	inlineBuffers->errorCode =	BCT_ERR_VERIFY_ERROR_INLINING;
	inlineBuffers->verifyError = J9NLS_BCV_ERR_TEMP_NOT_RET_ADDRESS__ID;
	/* Set the current PC value to show up in the error message framework */
	inlineBuffers->verifyErrorPC = currentPC;
#if J9JSRI_DEBUG
	j9tty_printf(inlineBuffers->portLib, "[][][] Verify Error bad addr\n"); 
#endif
	return NULL;
}



/*

Returns the first block at or after the startingAddress with the equivalent jsrData chains.

*/

static J9JSRICodeBlock *
getNextGreatestBlock(J9JSRIJSRData * jsrData, UDATA * startAddress, J9JSRIData * inlineBuffers) {

	UDATA endAddress = inlineBuffers->sourceBufferSize;
	J9JSRICodeBlock *entry;

	while (*startAddress < endAddress) {
		(*startAddress)++;
		entry = inlineBuffers->map->entries[*startAddress - 1];

		while (entry) {
			if ((entry->coloured & J9JSRI_COLOUR_FOR_UNWRITTEN) && (areDataChainsEqual(jsrData, entry->jsrData))) {
				return entry;
			}
			entry = entry->nextBlock;
		}
	}
	return NULL;
}



/*
Description:
Returns non-zero if the given JSRData implies a recursive JSR exists.
This is done by checking to see if it has two destination addresses which are equal.

Args:
data - the list of JSRData that will be checked for recursion

Return:
0 if there is no recursive or non-zero (1) if there is.

Notes:
This method could probably be optimized since we will only be needing to check the most recently added element but that would require knowledge of the 
implementation which shouldn't be required.  Since this method is called infrequently, it will be implemented in an implementation-agnostic way.
*/


static UDATA 
isJSRRecursive(J9JSRIJSRData * data, J9JSRIData * inlineBuffers)
{
	while (data) {
		J9JSRIJSRData *temp = data->previousJSR;
		while (temp) {
			if (temp->originalPC == data->originalPC) {
				inlineBuffers->verifyError = J9NLS_BCV_ERR_MULTIPLE_JSR__ID;
				return 1;
			}
			temp = temp->previousJSR;
		}
		data = data->previousJSR;
	}
	return 0;
}



/* Returns a pointer to the J9JSRICodeBlock that marked this location or NULL if it is untouched */
/* Returns NULL if the location is out of the range of the given map */

static J9JSRICodeBlock *
isMapMarkedAtLocation(UDATA location, J9JSRIJSRData * parentJSR, J9JSRIData * inlineBuffers)
{
#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif

	J9JSRICodeBlock *entry = inlineBuffers->map->entries[location];

	if (entry && (entry->jsrData->spawnPC == (U_32) -1)) {
#if J9JSRI_DEBUG
		j9tty_printf(inlineBuffers->portLib, "	[][][] is unnested marked block at %d - entry %p - jsr = %d, ret = %d\n", location, entry, parentJSR->spawnPC, parentJSR->retPC);
#endif
		return entry;
	}

#if 0
	while (entry) {
#if J9JSRI_DEBUG
		j9tty_printf(inlineBuffers->portLib, "	check is equivalent block at %d - entry %p - jsr = %d, ret = %d\n", location, entry, parentJSR->spawnPC, parentJSR->retPC);
#endif
		if (entry->jsrData->spawnPC == (U_32) -1) {
#if J9JSRI_DEBUG
			j9tty_printf(inlineBuffers->portLib, "	[][][] is unnested marked block at %d - entry %p - jsr = %d, ret = %d\n", location, entry, parentJSR->spawnPC, parentJSR->retPC);
#endif
			return entry;
		}
		entry = entry->nextBlock;
	}

	entry = inlineBuffers->map->entries[location];
#endif

	while (entry) {
		if (areDataChainsEqual (entry->jsrData, parentJSR)) {
#if J9JSRI_DEBUG
			j9tty_printf(inlineBuffers->portLib, "	is marked block at %d - entry %p - jsr = %d, ret = %d\n", location, entry, parentJSR->spawnPC, parentJSR->retPC);
#endif
			return entry;
		}

		entry = entry->nextBlock;
	}

#if J9JSRI_DEBUG
	j9tty_printf(inlineBuffers->portLib, "	is not marked block at %d - entry %p\n", location, entry);
#endif
	return NULL;
}



/*
Description:
Sets a verify error if this ret is used by multiple jsr's starting at different points.
The first entry in the list is the newest, so only check it against the rest of the list.

Args:
pc - the pc of the ret address.

Return:
void

*/


static UDATA 
isRetMultiplyUsed(UDATA pc, J9JSRIData * inlineBuffers)
{
	J9JSRICodeBlock *entry1;
	J9JSRICodeBlock *entry2;

	entry1 = inlineBuffers->map->entries[pc];
	entry2 = entry1->nextBlock;
	while (entry2) {
		if ((entry1->jsrData) && (entry2->jsrData)) {
			if ((entry1->jsrData->retPC != JI_NO_RET_YET) && (entry2->jsrData->retPC != JI_NO_RET_YET)) {
				if (entry1->jsrData->originalPC != entry2->jsrData->originalPC) {
					inlineBuffers->verifyError = J9NLS_BCV_ERR_MULTIPLE_RET__ID;
					return 1;
				}
			}
		}
		entry2 = entry2->nextBlock;
	}
	return 0;
}



/*

Stores (value > 0) into the variable bit at index variable.

Modifies the structure.

Does nothing if the passed JSRData* is NULL
*/

static U_8 
loadIntoVariable(J9JSRIData * inlineBuffers, J9JSRIJSRData * ourData, U_8 value, U_16 variable)
{
	while (ourData) {
		if ((UDATA) variable < inlineBuffers->maxLocals) {
			ourData->locals[variable] = value;
		} else {
			return JI_LOCAL_OUT_OF_RANGE;
		}
		ourData = ourData->previousJSR;
	}
	return 0;
}
		


/* marks the location in the given map as "used" */

static void 
markMapAtLocation(UDATA location, J9JSRICodeBlock * block, J9JSRIData * inlineBuffers)
{
#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif

	J9JSRIAddressMap * map = inlineBuffers->map;
	J9JSRICodeBlock * firstBlock = map->entries[location];

#if J9JSRI_DEBUG
	j9tty_printf(inlineBuffers->portLib, "	mark block at %d - jsrdata %p - jsr = %d, ret = %d\n", location, block->jsrData, block->jsrData->spawnPC, block->jsrData->retPC);
#endif

	if (firstBlock) {
		if (firstBlock->jsrData->spawnPC == (U_32) -1) {
			/* Keep non-jsr blocks (spawnPC = -1) at front */
			block->nextBlock = firstBlock->nextBlock;
			firstBlock->nextBlock = block;
		} else {
			block->nextBlock = firstBlock;
			map->entries[location] = block;
		}
	} else {
		map->entries[location] = block;
	}
}



/*

Returns the byte at the top of the stack and decrements the stack pointer.

Modifies the structure.

*/

static U_8 
popStack(J9JSRIJSRData * ourData)
{
	U_8 result = 0;

	if (ourData) {
		if (ourData->stack > ourData->stackBottom) {
			result = *(ourData->stack - 1);
			/* Pop in this and nested frames */
			while (ourData) {
				ourData->stack--;
				ourData = ourData->previousJSR;
			}
		} else {
			result = JI_STACK_UNDERFLOW;
		}
	}
	return result;
}



/*

Pushes the value on the stack in ourData at its top and the height is incremented.

Modifies the structure.

*/

static U_8 
pushOntoStack(J9JSRIJSRData * ourData, U_8 value)
{
	U_8 result = 0;

	if (ourData) {
		if (ourData->stack < ourData->locals) {
			/* tell the other JSR structures to push */
			while (ourData) {
				*(ourData->stack) = value;
				ourData->stack++;
				value = 0;
				ourData = ourData->previousJSR;
			}
		} else {
			return JI_STACK_OVERFLOW;
		}
	}
	return result;
}



/*

Pushes the value on the stack in ourData at its top and the height is incremented.

Modifies the structure.

*/

static U_8 
pushReturnOntoStack(U_16 constantIndex, J9CfrConstantPoolInfo * constantPool, J9JSRIJSRData * jsrData)
{
	U_8 result = 0;
	U_8 *returnChar = constantPool[constantPool[constantPool[constantIndex].slot2].slot2].bytes +
										(constantPool[constantPool[constantPool[constantIndex].slot2].slot2].slot1 - 1);

	if (*returnChar == 'V') {
		return result;
	} else {
		result = pushZeroOntoStack(jsrData);
		if ((*returnChar == ';') || (*(returnChar - 1) == '[')) {
			return result;
		}
		if ((*returnChar == 'D') || (*returnChar == 'J')) {
			result = pushZeroOntoStack(jsrData);
		}
	}
	return result;
}



/*

Pushes the value on the stack in ourData at its top and the height is incremented.

Modifies the structure.

*/

static U_8 
pushZeroOntoStack(J9JSRIJSRData * ourData)
{
	if (ourData) {
		if (ourData->stack < ourData->locals) {
			/* tell the other JSR structures to push */
			while (ourData) {
				*(ourData->stack) = 0;
				ourData->stack++;
				ourData = ourData->previousJSR;
			}
		} else {
			return JI_STACK_OVERFLOW;
		}
	}
	return 0;
}



/*

Description:
Destroys the address map.

Args:
inlineBuffers - holds the map pointer

Return:
void

Notes:

*/

static void 
releaseMap (J9JSRIData * inlineBuffers)
{
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);

	if (inlineBuffers->branchStack) {
		j9mem_free_memory(inlineBuffers->branchStack);
		inlineBuffers->branchStack = NULL;
	}
	if (inlineBuffers->map) {
		if (inlineBuffers->map->entries) {
			j9mem_free_memory(inlineBuffers->map->entries);
		}
		if (inlineBuffers->map->reachable) {
			j9mem_free_memory(inlineBuffers->map->reachable);
		}
		if (inlineBuffers->map->lineNumbers) {
			j9mem_free_memory(inlineBuffers->map->lineNumbers);
		}
		j9mem_free_memory(inlineBuffers->map);
#if J9JSRI_DEBUG
		j9tty_printf(inlineBuffers->portLib, "free map %p\n", inlineBuffers->map);
#endif
	}
}



static void 
rewriteExceptionHandlers(J9JSRIData * inlineBuffers)
{
#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif

	J9JSRIExceptionListEntry *originalExceptions = inlineBuffers->originalExceptionTable;
	UDATA exceptionCount = 0;

	/* There can only be a new table if there was an old table */
	if (originalExceptions) {
		J9JSRICodeBlock *entry;
		J9JSRICodeBlock *exceptionEntry;
		J9JSRIJSRData *jsrData;
		UDATA exceptionRangeStarted = FALSE;
		J9CfrExceptionTableEntry *exceptionTableBuffer;

		inlineBuffers->exceptionTableBuffer = (J9CfrExceptionTableEntry *) inlineBuffers->freePointer;
		exceptionTableBuffer = inlineBuffers->exceptionTableBuffer;

		while (originalExceptions) {

			/* Walk all the blocks for each exception */
			entry = inlineBuffers->firstOutput;

			while (entry) {
				if ((entry->originalPC >= originalExceptions->startPC) && (entry->originalPC < originalExceptions->endPC)) {
					/* block is in exception range */

					/* if first block, start output exception */
					if (exceptionRangeStarted == FALSE) {
						/* add new exception */
						if (entry->length) {
							/* Ignore empty blocks - resulting from block shortening - jsrs, starts of subroutine, rets */
							exceptionRangeStarted = TRUE;

							inlineBuffers->freePointer += sizeof(J9CfrExceptionTableEntry);

							if (inlineBuffers->freePointer > inlineBuffers->segmentEnd) {
								inlineBuffers->errorCode = BCT_ERR_OUT_OF_ROM;
								return;
							}

							exceptionTableBuffer[exceptionCount].startPC = entry->newPC;
							exceptionTableBuffer[exceptionCount].endPC = entry->newPC + entry->length;
							exceptionTableBuffer[exceptionCount].catchType = originalExceptions->catchType;
							exceptionTableBuffer[exceptionCount].handlerPC = (U_32) -1;

							jsrData = entry->jsrData;

							/* Find the matching handler block for the exception range */
_tryOuterJsrData:
							exceptionEntry = inlineBuffers->map->entries[originalExceptions->handlerPC];
							while (exceptionEntry) {
								if (areDataChainsEqual (jsrData, exceptionEntry->jsrData)) {
									exceptionTableBuffer[exceptionCount].handlerPC = exceptionEntry->newPC;
									break;
								}
								exceptionEntry = exceptionEntry->nextBlock;
							}

							if (exceptionEntry == NULL) {
								if (jsrData->previousJSR) {
									jsrData = jsrData->previousJSR;
									goto _tryOuterJsrData;
								} else {
#if J9JSRI_DEBUG
									if (exceptionEntry == NULL) {
										j9tty_printf(inlineBuffers->portLib, "[][][] exception handler block not found\n");
									}
#endif
								}
							} else {
								exceptionCount++;
							}
						}
					} else {
						/* else extend current exception */
						exceptionTableBuffer[exceptionCount - 1].endPC = entry->newPC + entry->length;
					}

				} else {
					/* Remove duplicated exceptions */
					if (exceptionRangeStarted) {
						/* Just closing an exception - check if it's a duplicate */
						UDATA i;
						J9CfrExceptionTableEntry lastException = exceptionTableBuffer[exceptionCount - 1];

						for (i = exceptionCount; i > 1; i--) {
							J9CfrExceptionTableEntry earlierException = exceptionTableBuffer[i - 2];

							if ((lastException.startPC == earlierException.startPC)
								&& (lastException.endPC == earlierException.endPC)
								&& (lastException.handlerPC == earlierException.handlerPC)
								&& (lastException.catchType == earlierException.catchType)) {

								exceptionCount--;
								inlineBuffers->freePointer -= sizeof(J9CfrExceptionTableEntry);
								break;
							}
						}
					}
					exceptionRangeStarted = FALSE;
				}
				entry = entry->primaryChild;
			}
			originalExceptions = originalExceptions->nextInList;
		}
	}
	inlineBuffers->codeAttribute->exceptionTable = inlineBuffers->exceptionTableBuffer;
	if (exceptionCount >= ((64 * 1024) - 1)) {
		/* exception table too large - format limitation */
		inlineBuffers->errorCode = BCT_ERR_VERIFY_ERROR_INLINING;
		inlineBuffers->verifyError = J9NLS_BCV_ERR_TOO_MANY_JSRS__ID;
		/* Set the current PC value to show up in the error message framework */
		inlineBuffers->verifyErrorPC = 0;
	}
	inlineBuffers->bytesAddedByJSRInliner += ((U_16) exceptionCount - inlineBuffers->codeAttribute->exceptionTableLength) * sizeof(U_16) * 4;
	inlineBuffers->codeAttribute->exceptionTableLength = (U_16) exceptionCount;
}


/* 
	Walk the list of code blocks in output order and dump a new linenumber table if there was an old one
*/

static void 
rewriteLineNumbers(J9JSRIData * inlineBuffers)
{
	UDATA i;
	UDATA lineNumberTableLength = 0;
	J9JSRICodeBlock *block;
	J9CfrLineNumberTableEntry *tableStart;
	J9CfrLineNumberTableEntry *newLine = NULL;
	J9CfrAttributeLineNumberTable *lineNumberTableAttribute;

	if ((inlineBuffers->flags) & (BCT_StripDebugAttributes | BCT_StripDebugLines)) {
		/* Don't rewrite them - they will be stripped later */
		return;
	}

	/* Check for presence of a lineNumberTable - guaranteed max of one */
	for (i = 0; i < inlineBuffers->codeAttribute->attributesCount; i++) {

		if (inlineBuffers->codeAttribute->attributes[i]->tag == CFR_ATTRIBUTE_LineNumberTable) {
			IDATA lastPC = -1;

			lineNumberTableAttribute = (J9CfrAttributeLineNumberTable *) inlineBuffers->codeAttribute->attributes[i];

			/* replace the line number table */
			tableStart = (J9CfrLineNumberTableEntry *) inlineBuffers->freePointer;

			block = inlineBuffers->firstOutput;

			while (block) {
				U_16 lineNumber = inlineBuffers->map->lineNumbers[block->originalPC];
				/* Check for a line number - assumes line number 0 is invalid */
				if (lineNumber != 0) {
					if (block->newPC != (UDATA) lastPC) {
						/* Output a new line number table entry */
						newLine = (J9CfrLineNumberTableEntry *) inlineBuffers->freePointer;
						inlineBuffers->freePointer += sizeof(J9CfrLineNumberTableEntry);

						if (inlineBuffers->freePointer > inlineBuffers->segmentEnd) {
							inlineBuffers->errorCode = BCT_ERR_OUT_OF_ROM;
							return;
						}
						newLine->startPC = block->newPC;
						newLine->lineNumber = lineNumber;
						lastPC = block->newPC;
						lineNumberTableLength++;
					} else {
						/* Update the last output line number table entry */
						if ((NULL != newLine) && (newLine->lineNumber < lineNumber)) {
							newLine->lineNumber = lineNumber;
						}
					}
				}
				block = block->primaryChild;
			}

			if (lineNumberTableLength >= (64 * 1024)) {
				/* Output too large */
				lineNumberTableAttribute->tag = CFR_ATTRIBUTE_Unknown;
			} else {
				inlineBuffers->bytesAddedByJSRInliner += ((U_16) lineNumberTableLength - lineNumberTableAttribute->lineNumberTableLength) * sizeof(U_16) * 2;
				lineNumberTableAttribute->lineNumberTableLength = (U_16) lineNumberTableLength;
				lineNumberTableAttribute->lineNumberTable = tableStart;
			}
			break;
		}
	}
}

/*

Description:
Walks the exceptions that are partially or wholly outside jsrs

*/

static void 
walkExceptions(I_32 hasRET, J9JSRICodeBlock * root, J9JSRIJSRData * jsrData, J9JSRIData * inlineBuffers)
{
	J9JSRIExceptionListEntry *originalExceptions;
	UDATA uninitializedCount = 1;

#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif

	if (root) {
		while (uninitializedCount) {
			uninitializedCount = 0;

			originalExceptions = inlineBuffers->originalExceptionTable;
			while (originalExceptions) {
#if J9JSRI_DEBUG
				j9tty_printf(inlineBuffers->portLib, "Walk exception %p - %d to %d at %d\n", originalExceptions, originalExceptions->startPC, originalExceptions->endPC, originalExceptions->handlerPC);
#endif
				/* live exception - code range walked */
				if (originalExceptions->jsrData) {
					jsrData = duplicateJSRDataChain(originalExceptions->jsrData, inlineBuffers);

					/* set up the JSR stack for an exception */
					while (popStack (jsrData) != JI_STACK_UNDERFLOW);
					pushZeroOntoStack (jsrData);

					evaluateCodeBlock(hasRET, &originalExceptions->handlerBlock, originalExceptions->handlerPC, jsrData, inlineBuffers);

					if (inlineBuffers->errorCode) {
						return;
					}	
				} else {
					uninitializedCount++;
				}
				originalExceptions = originalExceptions->nextInList;
			}

			if (uninitializedCount) {
				/* Check for entries that were initialized and missed in the loop above */
				originalExceptions = inlineBuffers->originalExceptionTable;
				while (originalExceptions) {
					if (originalExceptions->jsrData == NULL) {
						uninitializedCount--;
					}
					originalExceptions = originalExceptions->nextInList;
				}
			}
		}
	}
}




/* inlines jsr instructions */
/* This method is the entry point to the jsr inlining functionality of the VM */

void 
inlineJsrs(I_32 hasRET, J9CfrClassFile * classfile, J9JSRIData * inlineBuffers)
{
	J9JSRICodeBlock *root;
	J9JSRIJSRData *jsrData;
	UDATA startAddress;
#if J9JSRI_DEBUG
	UDATA i;

	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib); /* Needs to be done here so that we can use j9tty */
#endif

	/* Keep only the flags of interest */
	inlineBuffers->flags &= (BCT_StripDebugAttributes | BCT_StripDebugLines | BCT_StripDebugVars | BCT_Xfuture);

#if XFUTURE_TEST
	inlineBuffers->flags |= BCT_Xfuture;
#endif

	startAddress = 0;

	/* Jazz 82615: Initialized by default when not specified in the error message framework */
	inlineBuffers->verboseErrorType = 0;
	inlineBuffers->errorLocalIndex = (U_32)-1;

	allocateInlineBuffers (inlineBuffers);
	if (inlineBuffers->errorCode) {
		return;
	}

	copyOriginalExceptionList(inlineBuffers);
	if (inlineBuffers->errorCode) {
		return;
	}

	jsrData = createJSRData(NULL, NULL, -1, -1, inlineBuffers);
	if (inlineBuffers->errorCode) {
		return;
	}

	/* Build the code block tree */
	evaluateCodeBlock(hasRET, &root, 0, jsrData, inlineBuffers);
	if (inlineBuffers->errorCode) {
		return;
	}

	walkExceptions(hasRET, root, jsrData, inlineBuffers);
	if (inlineBuffers->errorCode) {
		return;
	}

#if LARGE_METHOD_TEST
	if ((inlineBuffers->destBufferIndex + TEST_PAD_LENGTH) >= inlineBuffers->destBufferSize) {
		inlineBuffers->errorCode = BCT_ERR_OUT_OF_ROM;
		return;
	}

	memset((&inlineBuffers->destBuffer)[inlineBuffers->destBufferIndex], JBnop, TEST_PAD_LENGTH);
	inlineBuffers->destBufferIndex += TEST_PAD_LENGTH;
#endif

	inlineBuffers->firstOutput = root;

_inlineWide:

	/* Flatten the hierarchies */
	if (inlineBuffers->wideBranchesNeeded) {
		flattenCodeBlocksWide(inlineBuffers);
	} else {
		flattenCodeBlockHeirarchyToList(root, inlineBuffers);
#if WIDE_BRANCH_TEST
		if (inlineBuffers->errorCode) {
			return;
		}
		flattenCodeBlocksWide(inlineBuffers);
#endif
	}

	if (inlineBuffers->errorCode) {
		return;
	}

	if (inlineBuffers->destBufferIndex > (16 * 1024 * 1024)) {
		inlineBuffers->errorCode = BCT_ERR_VERIFY_ERROR_INLINING;
		inlineBuffers->verifyError = J9NLS_BCV_ERR_TOO_MANY_JSRS__ID;
		/* Set the current PC value to show up in the error message framework */
		inlineBuffers->verifyErrorPC = 0;
		return;
	}

	correctJumpOffsets(inlineBuffers);
	if (inlineBuffers->errorCode) {
		return;
	}

	if (inlineBuffers->wideBranchesNeeded) {
		goto _inlineWide;
	}

#if J9JSRI_DEBUG
	dumpData(inlineBuffers);
#endif

	rewriteExceptionHandlers(inlineBuffers);
	if (inlineBuffers->errorCode) {
		return;
	}

	rewriteLineNumbers(inlineBuffers);
	if (inlineBuffers->errorCode) {
		return;
	}

	rewriteLocalVariables(inlineBuffers, CFR_ATTRIBUTE_LocalVariableTable);
	if (inlineBuffers->errorCode) {
		return;
	}

	rewriteLocalVariables(inlineBuffers, CFR_ATTRIBUTE_LocalVariableTypeTable);
	if (inlineBuffers->errorCode) {
		return;
	}
}



/*

Description:
Destroys the inlining pools.

Args:
inlineBuffers - holds the various pool pointers

Return:
void

Notes:

*/

void 
releaseInlineBuffers (J9JSRIData * inlineBuffers)
{
	if (inlineBuffers->codeBlockPool) {
		pool_kill (inlineBuffers->codeBlockPool);
		pool_kill (inlineBuffers->exceptionListEntryPool);
		pool_kill (inlineBuffers->jsrDataPool);

		releaseMap (inlineBuffers);
	}
}





/* 
	Walk the list of code blocks in output order and dump a new local variable table if there was an old one.
	Works on both LocalVariableTables and LocalVariableTypeTables.
*/

static void 
rewriteLocalVariables(J9JSRIData * inlineBuffers, U_8 tableType)
{
	UDATA i, j;
	UDATA x = 0;
	UDATA depth = 0;
	UDATA foundTable = 0;
	UDATA isNewLocal;
	J9JSRICodeBlock *block;
	J9CfrLocalVariableTableEntry *tableStart, *local, *newLocal;
	J9CfrAttributeLocalVariableTable *localVariableTableAttribute, *firstLocalVariableTableAttribute;

	if ((inlineBuffers->flags) & (BCT_StripDebugAttributes | BCT_StripDebugVars)) {
		/* Don't rewrite them - they will be stripped later */
		return;
	}

	/* Check for presence of a localVariableTable */
	for (i = 0; i < inlineBuffers->codeAttribute->attributesCount; i++) {

		if (inlineBuffers->codeAttribute->attributes[i]->tag == tableType) {

			localVariableTableAttribute = (J9CfrAttributeLocalVariableTable *) inlineBuffers->codeAttribute->attributes[i];

			/* untag any extra tables */
			if (foundTable) {
				inlineBuffers->codeAttribute->attributes[i]->tag = CFR_ATTRIBUTE_Unknown;
			} else {
				/* replace the first local variable table */
				tableStart = (J9CfrLocalVariableTableEntry *) inlineBuffers->freePointer;
				firstLocalVariableTableAttribute = localVariableTableAttribute;
				foundTable = 1;
			}

			local = localVariableTableAttribute->localVariableTable;

			for (j = 0; j < localVariableTableAttribute->localVariableTableLength; j++, local++) {

				block = inlineBuffers->firstOutput;
				isNewLocal = 1;

				while (block) {

					if ((block->originalPC >= local->startPC) && (block->originalPC < (local->startPC + local->length))) {

						/* Start a new entry */
						if (isNewLocal) {
							if (depth || (block->coloured & J9JSRI_COLOUR_FOR_JSR_START)) {
								if (block->coloured & J9JSRI_COLOUR_FOR_JSR_START) {
									depth++;
								}
								if (block->coloured & J9JSRI_COLOUR_FOR_JSR_END) {
									depth--;
								}
							} else {
								isNewLocal = 0;
								newLocal = (J9CfrLocalVariableTableEntry *) inlineBuffers->freePointer;
								inlineBuffers->freePointer += sizeof(J9CfrLocalVariableTableEntry);
								if (inlineBuffers->freePointer > inlineBuffers->segmentEnd) {
									inlineBuffers->errorCode = BCT_ERR_OUT_OF_ROM;
									return;
								}
								newLocal->startPC = block->newPC;
								newLocal->length = block->length;
								newLocal->nameIndex = local->nameIndex;
								newLocal->descriptorIndex = local->descriptorIndex;
								newLocal->index = local->index;
								x++;
							}

						/* Update the existing entry */
						} else {
							newLocal->length += block->length;
						}

					/* Reset to not a new entry */
					} else {
						isNewLocal = 1;
					}
					block = block->primaryChild;
				}
			}
		}
	}
	if (foundTable) {
		inlineBuffers->bytesAddedByJSRInliner += ((U_16) x - firstLocalVariableTableAttribute->localVariableTableLength) * sizeof(U_16) * 5;
		firstLocalVariableTableAttribute->localVariableTableLength = (U_16) x;
		firstLocalVariableTableAttribute->localVariableTable = tableStart;
	}
}





static void
dumpData(J9JSRIData * inlineBuffers)
{
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib); /* Needs to be done here so that we can use j9tty */

	UDATA i, j;
	J9JSRICodeBlock *entry;
	J9JSRIJSRData *jsrDataDump;

	for (i = 0; i < inlineBuffers->sourceBufferSize; i++) {
		entry = inlineBuffers->map->entries[i];
		if (entry) {
			j9tty_printf(inlineBuffers->portLib, "Offset %d - 0x%x\n", i, i);
			while (entry) {
				j9tty_printf(inlineBuffers->portLib, "\tblk = %p, len = %d, colour = %p\n", entry, entry->length, entry->coloured);
				j9tty_printf(inlineBuffers->portLib, "\tpri = %p, sec = %p, jsrDat = %p\n", entry->primaryChild, entry->secondaryChild, entry->jsrData);
				j9tty_printf(inlineBuffers->portLib, "\tjsrspawnPC = %d, jsrretPC = %d, jsroriginalPC = %d\n", entry->jsrData->spawnPC, entry->jsrData->retPC, entry->jsrData->originalPC);
				jsrDataDump = entry->jsrData;
				while (jsrDataDump->previousJSR) {
					j9tty_printf(inlineBuffers->portLib, "\t\tlink %d\n", jsrDataDump->spawnPC);
					jsrDataDump = jsrDataDump->previousJSR;
				}
				for (j = 0; j < entry->length; j++	) {
					j9tty_printf(inlineBuffers->portLib, " %02x", inlineBuffers->destBuffer[entry->newPC + j]);
				}
				j9tty_printf(inlineBuffers->portLib, "\n");
				entry = entry->nextBlock;
			}
		}
	}
}



/*
Description:
This is to determine if the method has the opcode JSR, JSR_W, RET or WIDE RET.

Args:
classfile - the buffer where the bytecode blocks are found

Return:
void

Notes:
*/

I_32 
checkForJsrs(J9CfrClassFile * classfile)
{
	J9CfrMethod *method;
	J9CfrAttributeCode *code;
	U_8 *buffer, *bcIndex;
	UDATA bc;
	UDATA length, pc, i;
	I_32 hasRET = 0;

	for (i = 0; i < classfile->methodsCount; i++) {
		method = &(classfile->methods[i]);
		code = method->codeAttribute;

		/* skip abstracts and natives - they have no code */
		if (!code) {
			continue;
		}

		buffer = code->code;
		length = (UDATA) (code->codeLength);
		pc = 0;
		while (pc < length) {

			I_32 high, low;
			UDATA npairs, index;

			bc = buffer[pc];

			/* Short circuit most bytecodes */
			if (bc <= CFR_BC_goto) {
				pc += (sunJavaByteCodeRelocation[bc] & 7);
				continue;
			}

			/* Unknown bytecode found */
			if (bc > CFR_BC_jsr_w) {
				return hasRET;
			}

			/* jsr or ret found */
			if ((bc == CFR_BC_jsr) || (bc == CFR_BC_jsr_w) || (bc == CFR_BC_ret)) {
				if (bc == CFR_BC_ret) {
					hasRET = 1;
				} 
				method->j9Flags |= CFR_J9FLAG_HAS_JSR;
				classfile->j9Flags |= CFR_J9FLAG_HAS_JSR;
			}

			/* handle a wide */
			if(bc == CFR_BC_wide) {
				pc++;
				if (pc >= length) {
					return hasRET;
				}
				bc = buffer[pc];
				/* wide ret found */
				if(bc == CFR_BC_ret) {
					hasRET = 1;
					method->j9Flags |= CFR_J9FLAG_HAS_JSR;
					classfile->j9Flags |= CFR_J9FLAG_HAS_JSR;
				} else if (bc == CFR_BC_iinc) {
					pc += 1;
				}
				pc += 1;
				/* Will get the rest of the index count from the else of the switch if */
			}

			/* handle switches */
			if ((bc == CFR_BC_tableswitch) || (bc == CFR_BC_lookupswitch)) {
				pc += 4 - (pc & 3);
				pc += 4; /* skip default branch */
				bcIndex = &(buffer[pc]);
				pc += 4;
				if (pc > length) {
					return hasRET;
				}
				NEXT_I32(low, bcIndex);
				if (bc == CFR_BC_tableswitch) { 
					pc += 4;
					if (pc > length) {
						return hasRET;
					}
					NEXT_I32(high, bcIndex);
					npairs = (UDATA) (high - low + 1);
					index = 4;
				} else {
					npairs = (UDATA) low;
					index = 8;
				}
				pc += (npairs * index);
			} else {
				pc += (sunJavaByteCodeRelocation[bc] & 7);
			}
		}
	}
	return hasRET;
}


/*
Description:
Creates and returns a new J9JSRIAddressMap * initialized with the given size

Args:
size - the length of the code block
portLib - the J9PortLibrary used to directly ask for memory (Address Maps don't use pools)

Return:
The method tags all jump targets to start a new inlining block

Notes:
*/

static UDATA 
mapJumpTargets(J9JSRIData * inlineBuffers)
{
	IDATA target, start, i1, i2, tableSize;
	U_8 *bcStart, *bcIndex, *bcEnd;
	U_8 *reachable = inlineBuffers->map->reachable;
	UDATA bc, index, u1, u2, i;
	/* Changed the defaults to 1 after encountering a bogus class file with a jsr handler */
	/* without a jsr to call it - dead code with a ret bytecode CMVC 88509 */
	UDATA branchCount = 1;
	UDATA jsrCount = 1;

	bcStart = inlineBuffers->sourceBuffer;
	bcEnd = bcStart + inlineBuffers->sourceBufferSize;
	bcIndex = bcStart;
	while (bcIndex < bcEnd) {
		bc = *bcIndex;
		if (bc < CFR_BC_ifeq) {
			bcIndex += (sunJavaByteCodeRelocation[bc] & 7);
			continue;
		}
		switch (bc) {
		case CFR_BC_ret:
			start = bcIndex - bcStart;
			reachable[start] |= J9JSRI_MAP_MARKED_END_BLOCK;
			bcIndex += 2;
			break;

		case CFR_BC_jsr:
			jsrCount++;
			start = bcIndex - bcStart;
			reachable[start] |= J9JSRI_MAP_MARKED_END_BLOCK;
			bcIndex++;
			NEXT_U16(index, bcIndex);
			target = (I_16) index + start;
			reachable[target] |= J9JSRI_MAP_MARKED_END_BLOCK;
			goto _collapseTest;
			break;

		case CFR_BC_ifeq:
		case CFR_BC_ifne:
		case CFR_BC_iflt:
		case CFR_BC_ifge:
		case CFR_BC_ifgt:
		case CFR_BC_ifle:
		case CFR_BC_if_icmpeq:
		case CFR_BC_if_icmpne:
		case CFR_BC_if_icmplt:
		case CFR_BC_if_icmpge:
		case CFR_BC_if_icmpgt:
		case CFR_BC_if_icmple:
		case CFR_BC_if_acmpeq:
		case CFR_BC_if_acmpne:
		case CFR_BC_ifnull:
		case CFR_BC_ifnonnull:
			branchCount++; /* fall through */

		case CFR_BC_goto:
			start = bcIndex - bcStart;
			bcIndex++;
			NEXT_U16(index, bcIndex);
			target = (I_16) index + start;
			reachable[target] |= J9JSRI_MAP_MARKED_END_BLOCK;
			break;

		case CFR_BC_tableswitch:
		case CFR_BC_lookupswitch:
			start = bcIndex - bcStart;
			reachable[start] |= J9JSRI_MAP_MARKED_END_BLOCK;
			bcIndex++;
			i1 = 3 - (start & 3);
			bcIndex += i1;
			NEXT_U32(u1, bcIndex);
			target = start + (I_32) u1;
			reachable[target] |= J9JSRI_MAP_MARKED_END_BLOCK;
			if (bc == CFR_BC_tableswitch) {
				i1 = (I_32) NEXT_U32(u1, bcIndex);
				i2 = (I_32) NEXT_U32(u2, bcIndex);
				tableSize = i2 - i1 + 1;
				i1 = 0;	
			} else {
				NEXT_U32(tableSize, bcIndex);
				i1 = 4;
			}
			branchCount += tableSize;

			/* Count the entries */
			for (i = 0; i < (UDATA) tableSize; i++) {
				bcIndex += i1;
				NEXT_U32(u1, bcIndex);
				target = start + (I_32) u1;
				reachable[target] |= J9JSRI_MAP_MARKED_END_BLOCK;
			}
			break;

		case CFR_BC_goto_w:
			start = bcIndex - bcStart;
			bcIndex++;
			NEXT_U32(index, bcIndex);
			target = (I_32) index + start;
			reachable[target] |= J9JSRI_MAP_MARKED_END_BLOCK;
			break;

		case CFR_BC_jsr_w:
			start = bcIndex - bcStart;
			bcIndex++;
			NEXT_U32(index, bcIndex);
			target = (I_32) index + start;
			reachable[target] |= J9JSRI_MAP_MARKED_END_BLOCK;
			jsrCount++;
			reachable[start] |= J9JSRI_MAP_MARKED_END_BLOCK;

_collapseTest:

			/* tag the jsr target first instruction as a block if an astore */
			if ((inlineBuffers->flags & BCT_Xfuture) == 0) {
				/* Don't perform this optimization with -Xfuture as it removes writes to locals that are supposed to cause JCK/TCK failures */
				if ((inlineBuffers->sourceBuffer[target] >= CFR_BC_astore_0) && (inlineBuffers->sourceBuffer[target] <= CFR_BC_astore_3)) {
					reachable[target] |= J9JSRI_MAP_MARKED_JSR_COLLAPSIBLE;
					reachable[target + 1] |= J9JSRI_MAP_MARKED_END_BLOCK;
				} else if (inlineBuffers->sourceBuffer[target] == CFR_BC_astore) {
					reachable[target] |= J9JSRI_MAP_MARKED_JSR_COLLAPSIBLE;
					reachable[target + 2] |= J9JSRI_MAP_MARKED_END_BLOCK;
				} else if ((inlineBuffers->sourceBuffer[target] == CFR_BC_wide) && (inlineBuffers->sourceBuffer[target + 1] == CFR_BC_astore)) {
					reachable[target] |= J9JSRI_MAP_MARKED_JSR_COLLAPSIBLE;
					reachable[target + 4] |= J9JSRI_MAP_MARKED_END_BLOCK;
				}
			}
			break;

		default:
			bcIndex += (sunJavaByteCodeRelocation[bc] & 7);
			if(bc == CFR_BC_wide) {
				if(*bcIndex == CFR_BC_ret) {
					start = bcIndex - bcStart - 1;
					reachable[start] |= J9JSRI_MAP_MARKED_END_BLOCK;
				}
				if(*bcIndex == CFR_BC_iinc) {
					bcIndex += 5;
				} else {
					bcIndex += 3;
				}
			}
			break;
		}
	}
	return branchCount * jsrCount;
}



/*

Description:
	Fill in the original exceptions and add new exceptions if the original is full and the J9JSRIJSRData 
	is different.

*/

static void 
checkExceptionStart(J9JSRICodeBlock * thisBlock, J9JSRIJSRData * jsrData, J9JSRIData * inlineBuffers)
{
	J9JSRIExceptionListEntry *entry = inlineBuffers->originalExceptionTable;
	J9JSRIExceptionListEntry *remainingEntries;
	J9JSRIExceptionListEntry *newEntry;

#if J9JSRI_DEBUG
	PORT_ACCESS_FROM_PORT(inlineBuffers->portLib);
#endif

	/* Walk all the exceptions to find ones in range */
	while (entry) {

		if ((thisBlock->originalPC >= entry->startPC) && (thisBlock->originalPC < entry->endPC)) {
			/* Block is covered by exception range */

			if (entry->jsrData == NULL) {
				/* First block seen in the exception range - set the jsrData */
				entry->jsrData = duplicateJSRDataChain(jsrData, inlineBuffers);
				if (inlineBuffers->errorCode) {
					return;
				}
#if J9JSRI_DEBUG
				j9tty_printf(PORTLIB, "	fill exception %p at %d to %d : %d, spawned %d\n", entry, entry->startPC, entry->endPC, entry->handlerPC, jsrData->spawnPC);
#endif

			} else {
				/* Check to see if the block has a handler in the list already */
				remainingEntries = entry;

				while (remainingEntries) {
					if (entry->tableEntry == remainingEntries->tableEntry) {
						/* Block is the same type */
						if (areDataChainsEqual (jsrData, remainingEntries->jsrData)) {
							/* This block will be handled by this exception handler */
							return;
						}
					}
					remainingEntries = remainingEntries->nextInList;
				}

				if (remainingEntries == NULL) {
					/* Did not find a match in the exceptions so add a new one */ 
					newEntry = addNewExceptionEntryToList(&(inlineBuffers->originalExceptionTable), entry, inlineBuffers);
					if (inlineBuffers->errorCode) {
						return;
					}
					newEntry->startPC = entry->startPC;
					newEntry->endPC = entry->endPC;
					newEntry->handlerPC = entry->handlerPC;
					newEntry->catchType = entry->catchType;
					newEntry->tableEntry = entry->tableEntry;
					newEntry->jsrData = duplicateJSRDataChain(jsrData, inlineBuffers);
					if (inlineBuffers->errorCode) {
						return;
					}
#if J9JSRI_DEBUG
					j9tty_printf(PORTLIB, "	add exception %p at %d to %d : %d, spawned %d\n", entry, entry->startPC, entry->endPC, entry->handlerPC, jsrData->spawnPC);
#endif
				}
			}
		}
		entry = entry->nextInList;
	}
}


