/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#include <string.h>
#include "j9.h"
#include "jitprotos.h"
#include "rommeth.h"
#include "stackwalk.h"
#include "ut_j9codertvm.h"

#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)

static UDATA
dltIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	if (walkState->framesWalked == 2) {
		walkState->userData1 = walkState->unwindSP;
		walkState->userData2 = walkState->arg0EA;
		walkState->userData3 = walkState->method;
	}

	return J9_STACKWALK_KEEP_ITERATING;
}


void *
setUpForDLT(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9DLTInformationBlock * dltBlock = &(currentThread->dltBlock);
	UDATA * onStackTemps;
	UDATA * copiedTemps;
	void * dltEntry;
	void * currentFrame;
	UDATA * currentA0;
	J9Method * currentMethod;
	J9ROMMethod * romMethod;
	UDATA argTempCount;
	UDATA argCount;
	J9JITExceptionTable * metaData;
	UDATA * checkSP;
	J9MonitorEnterRecord * enterRecord;
	UDATA * relativeA0;

	Trc_DLT_setUpForDLT_Entry(currentThread);

	/* Fetch and clear DLT entrypoint address */

	dltEntry = dltBlock->dltEntry;
	dltBlock->dltEntry = NULL;

	/* Walk past the current frame to collect the required information */

retry:
	walkState->skipCount = 0;
	walkState->maxFrames = 3;	/* Generic special, frame to compile, caller */
	walkState->walkThread = currentThread;
	walkState->frameWalkFunction = dltIterator;
	walkState->flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_COUNT_SPECIFIED | J9_STACKWALK_MAINTAIN_REGISTER_MAP;
	vm->walkStackFrames(currentThread, walkState);
	if (walkState->framesWalked != 3) {
		Trc_DLT_setUpForDLT_Exit_MalformedStack(currentThread);
		return NULL;
	}
	currentFrame = walkState->userData1;
	currentA0 = walkState->userData2;
	currentMethod = walkState->userData3;

	/* Check for stack overflow */

	metaData = jitGetExceptionTableFromPC(currentThread, (UDATA) dltEntry);
	checkSP = walkState->sp - (metaData->totalFrameSize + J9SW_JIT_STACK_SLOTS_USED_BY_CALL);
	if (checkSP < currentThread->stackOverflowMark2) {
#if defined(J9VM_INTERP_GROWABLE_STACKS)
		/* If we are already handling an overflow, abandon the DLT */

		if ((currentThread->privateFlags & J9_PRIVATE_FLAGS_STACK_OVERFLOW) == 0) {
			J9JavaStack * stack = currentThread->stackObject;
			UDATA bytesRequired = (UDATA)stack->end - (UDATA)checkSP;
			UDATA maxSize = vm->stackSize;

			/* If the required stack is more than the maximum, abandon the DLT */

			if (bytesRequired <= maxSize) {
				bytesRequired += vm->stackSizeIncrement;
				if (bytesRequired > maxSize) {
					bytesRequired = maxSize;
				}
				
				/* If the grow succeeds, restart the stack walk since all the stack pointers have moved */

				if (vm->internalVMFunctions->growJavaStack(currentThread, bytesRequired) == 0) {
					goto retry;
				}
			}
		}
#endif
		Trc_DLT_setUpForDLT_Exit_StackOverflow(currentThread);
		return NULL;
	}

	/* Copy the temps into the buffer */

	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(currentMethod);
	argTempCount = romMethod->tempCount;
	if (romMethod->modifiers & J9AccSynchronized) {
		++argTempCount;
	} else if (J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod)) {
		++argTempCount;
	}
	argCount = romMethod->argCount;
	argTempCount += argCount;
	copiedTemps = dltBlock->inlineTempsBuffer;
	if (argTempCount > (sizeof(dltBlock->inlineTempsBuffer) / sizeof(UDATA))) {
		PORT_ACCESS_FROM_JAVAVM(vm);

		copiedTemps = j9mem_allocate_memory(argTempCount * sizeof(UDATA), OMRMEM_CATEGORY_JIT);
		if (copiedTemps == NULL) {
			Trc_DLT_setUpForDLT_Exit_FailedTempAllocate(currentThread);
			return NULL;
		}
	}
	onStackTemps = currentA0 + 1 - argTempCount;
	memcpy(copiedTemps, onStackTemps, argTempCount * sizeof(UDATA));
	dltBlock->temps = copiedTemps;

	/* Handle J2I and bytecode->bytecode differently */

	if (walkState->jitInfo != NULL) {
		J9I2JState * currentState;

		Trc_DLT_setUpForDLT_Jitted_Caller(currentThread);

		/* Fetch return address from J2I frame */

		walkState->userData1 = ((J9SFJ2IFrame *) currentFrame)->returnAddress;

		/* Copy walkState i2jState and j2iFrame values back to the current locations */

		currentState = walkState->i2jState;
		if (currentState != NULL) {
			J9I2JState * elsState = &(currentThread->entryLocalStorage->i2jState);

			elsState->pc = currentState->pc;
			elsState->literals = currentState->literals;
			elsState->a0 = currentState->a0;
			elsState->returnSP = currentState->returnSP;
		}
		currentThread->j2iFrame = walkState->j2iFrame;
	} else {
		J9I2JState * elsState = &(currentThread->entryLocalStorage->i2jState);
		UDATA * returnSP;
		UDATA * sp;
		U_32 infoWord = ((U_32 *) dltEntry)[-1];

		Trc_DLT_setUpForDLT_Bytecode_Caller(currentThread);

		if ((*(UDATA *) currentFrame) & J9_STACK_FLAGS_J2_IFRAME) {
			Trc_DLT_setUpForDLT_Exit_MalformedStack(currentThread);
			return NULL;
		}

		/* Fetch return address from I2J table */

		walkState->userData1 = ((void **) vm->jitConfig->i2jReturnTable)[infoWord & 0x000F];

		/* Set up I2J state and possibly move args for alignment */

		elsState->a0 = walkState->arg0EA;
		elsState->pc = walkState->pc;
		elsState->literals = walkState->method;
		sp = walkState->sp;
		returnSP = sp + argCount;
		returnSP = (UDATA *) ((UDATA) returnSP | J9_STACK_FLAGS_J2_IFRAME);

		if (J9_ARE_ANY_BITS_SET((UDATA)sp, sizeof(UDATA))) {
			Trc_DLT_setUpForDLT_Aligning_Arguments(currentThread);
			memmove((walkState->sp = sp - 1), sp, argCount * sizeof(UDATA));
			returnSP = (UDATA *) ((UDATA) returnSP | J9_STACK_FLAGS_ARGS_ALIGNED);
		}
		elsState->returnSP = returnSP;
	}

	currentThread->jitStackFrameFlags = 0;

	/* Drop any monitor enter records for the frame which is being DLTed */

	enterRecord = currentThread->monitorEnterRecords;
	relativeA0 =  CONVERT_TO_RELATIVE_STACK_OFFSET(currentThread, currentA0);
	while ((enterRecord != NULL) && (enterRecord->arg0EA == relativeA0)) {
		J9MonitorEnterRecord * next = enterRecord->next;

		Trc_DLT_setUpForDLT_Discard_Enter_Record(currentThread, enterRecord, enterRecord->object);
		pool_removeElement(currentThread->monitorEnterRecordPool, enterRecord);
		enterRecord = next;
	}
	currentThread->monitorEnterRecords = enterRecord;

	Trc_DLT_setUpForDLT_Exit_Success(currentThread, dltEntry);
	return dltEntry;
}

#endif
