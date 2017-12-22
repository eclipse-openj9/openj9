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

/* #define PAINT_OLD_STACK */

/* 
 * to enable tracing, use: -Xtrace:iprint=tpid(064030-06404A) 
 */

#define GROW_STACK_PAINT_VALUE 0xD7

#include <string.h>
#include "j9protos.h"
#include "j9consts.h"
#include "stackwalk.h"
#include "ut_j9vm.h"
#include "rommeth.h"
#include "AtomicSupport.hpp"

extern "C" {

#if (defined(J9VM_INTERP_GROWABLE_STACKS))  /* File Level Build Flags */

#ifdef PAINT_OLD_STACK

#ifdef J9VM_ENV_DATA64
#define FULL_SLOT_PAINT_VALUE ((UDATA) GROW_STACK_PAINT_VALUE * (UDATA) 0x0101010101010101)
#else
#define FULL_SLOT_PAINT_VALUE ((UDATA) GROW_STACK_PAINT_VALUE * (UDATA) 0x01010101)
#endif

#define PAINT_STACK_MEMORY(x) memset((void *) vmThread->tempSlot, GROW_STACK_PAINT_VALUE, ((UDATA) (x)) - vmThread->tempSlot)

#endif

#define IS_IN_STACK(value) \
	((((UDATA) (value)) >= oldStackStart) && (((UDATA) (value)) < oldStackEnd))

#define FIX_IF_IN_RANGE(slot) \
	if (IS_IN_STACK(slot)) { *(UDATA**)&(slot) = (((UDATA *) (slot)) + delta); }

#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static void fixI2J (void * element, void * userData);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */
#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static void growSlotIterator (J9VMThread * vmThread, J9StackWalkState * walkState, j9object_t * objectSlotInNewStack, const void * stackLocation);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */
#if (defined(J9VM_JIT_FULL_SPEED_DEBUG)) 
static void fixDecompilationRecords (J9VMThread * vmThread, UDATA delta, UDATA oldStackStart, UDATA oldStackEnd);
#endif /* J9VM_JIT_FULL_SPEED_DEBUG */
static void fixStackWalkState (J9StackWalkState * walkState, UDATA oldStackStart, UDATA oldStackEnd, UDATA delta);
#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static UDATA addI2J (J9StackWalkState * walkState, J9I2JState * i2jState);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */
static UDATA growFrameIterator (J9VMThread * vmThread, J9StackWalkState * walkState);
static UDATA internalGrowJavaStack(J9VMThread * vmThread, UDATA newStackSize);


#if (defined(J9VM_JIT_FULL_SPEED_DEBUG)) 
static void fixDecompilationRecords(J9VMThread * vmThread, UDATA delta, UDATA oldStackStart, UDATA oldStackEnd)
{
	J9JITDecompilationInfo * current = vmThread->decompilationStack;

	while (current) {
		current->bp += delta;
		FIX_IF_IN_RANGE(current->pcAddress);
		current = current->next;
	}
}

#endif /* J9VM_JIT_FULL_SPEED_DEBUG */


UDATA   growJavaStack(J9VMThread * vmThread, UDATA newStackSize)
{
	UDATA rc;

	rc = internalGrowJavaStack(vmThread, newStackSize);
	if (0 != rc) {
		vmThread->javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
		rc = internalGrowJavaStack(vmThread, newStackSize);
	}

	return rc;
}


static UDATA internalGrowJavaStack(J9VMThread * vmThread, UDATA newStackSize)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	J9JavaStack * oldStack = vmThread->stackObject;
	J9JavaStack * newStack;
	UDATA delta;
	J9StackWalkState walkState;
	UDATA usedBytes = ((U_8 *) oldStack->end) - ((U_8 *) vmThread->sp);
	J9StackWalkState * currentWalkState;
	UDATA oldStackStart = (UDATA) (oldStack +1);
	UDATA oldStackEnd = (UDATA) (oldStack->end);
	UDATA oldState;
	UDATA rc = 0;

	oldState = vmThread->omrVMThread->vmState;
	vmThread->omrVMThread->vmState = J9VMSTATE_GROW_STACK;

	Trc_VM_growJavaStack_Entry(vmThread, oldStack->size, newStackSize, vmThread->sp, vmThread->stackOverflowMark, vmThread->stackOverflowMark2);

	if (usedBytes > newStackSize) {
		Trc_VM_growJavaStack_TooSmall(vmThread, usedBytes, newStackSize);
		rc = 3;
		goto done;
	}
	newStack = allocateJavaStack(vmThread->javaVM, newStackSize, oldStack);
	if (!newStack) {
		Trc_VM_growJavaStack_AllocFailed(vmThread);
		rc = 1;
		goto done;
	}
	delta = newStack->end - oldStack->end;
	/* Assert that double-slot alignment has been maintained */
	Assert_VM_false(J9_ARE_ANY_BITS_SET(delta, 1));
	Trc_VM_growJavaStack_Delta(vmThread, oldStack, newStack, delta);
	Trc_VM_growJavaStack_Copying(vmThread, vmThread->sp, vmThread->sp  + delta, usedBytes);

	memcpy(vmThread->sp + delta, vmThread->sp, usedBytes);

#ifdef PAINT_OLD_STACK
	vmThread->tempSlot = (UDATA) vmThread->sp;
#endif

	walkState.walkThread = vmThread;
	walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES;
	walkState.frameWalkFunction = growFrameIterator;
	walkState.userData1 = (void *) delta;
	walkState.userData2 = (void *) NULL;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (vmThread->javaVM->jitConfig) {
		walkState.restartPoint = pool_new(sizeof(J9I2JState *), 0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(PORTLIB));
		if (!walkState.restartPoint) {
			Trc_VM_growJavaStack_PoolAllocFailed(vmThread);
			freeJavaStack(vmThread->javaVM, newStack);
			rc = 4;
			goto done;
		}
		if (vmThread->entryLocalStorage) {
			if (addI2J(&walkState, &(vmThread->entryLocalStorage->i2jState))) {
				goto poolElementAllocFailed;
			}
		}
		walkState.restartException = NULL;
	}
#endif

	vmThread->javaVM->walkStackFrames(vmThread, &walkState);

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (vmThread->javaVM->jitConfig) {
		if (walkState.restartException) {
poolElementAllocFailed:
			pool_kill((J9Pool *) walkState.restartPoint);
			freeJavaStack(vmThread->javaVM, newStack);
			rc = 5;
			goto done;
		}
		pool_do((J9Pool *) walkState.restartPoint, fixI2J, (void *) delta);
		pool_kill((J9Pool *) walkState.restartPoint);
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
		fixDecompilationRecords(vmThread, delta, oldStackStart, oldStackEnd);
#endif
	}
#endif

#ifdef PAINT_OLD_STACK
	PAINT_STACK_MEMORY(oldStack->end);
#endif

	vmThread->sp += delta;
	vmThread->arg0EA += delta;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (vmThread->j2iFrame) {
		vmThread->j2iFrame += delta;
	}
#endif

	Trc_VM_growJavaStack_UpdateSPAndArg0EA(vmThread, vmThread->sp, vmThread->arg0EA);

	{
		vmThread->stackObject = newStack;
		UDATA *newStackOverflowMark = J9JAVASTACK_STACKOVERFLOWMARK(newStack);
		UDATA currentStackOverflowMark = (UDATA)vmThread->stackOverflowMark;
		if (J9_EVENT_SOM_VALUE != currentStackOverflowMark) {
			/* Use an atomic here to ensure we don't lose an aynsc message post.
			 * Only the current thread can ever write a value other than
			 * J9_EVENT_SOM_VALUE into stackOverflowMark.
			 */
			VM_AtomicSupport::lockCompareExchange((UDATA*)&vmThread->stackOverflowMark, currentStackOverflowMark, (UDATA)newStackOverflowMark);
		}
		vmThread->stackOverflowMark2 = newStackOverflowMark;
	}

	currentWalkState = vmThread->stackWalkState;
	do {
		fixStackWalkState(currentWalkState, oldStackStart, oldStackEnd, delta);
		currentWalkState = currentWalkState->previous;
	} while (currentWalkState);

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (vmThread->javaVM->jitConfig) {
		J9MonitorEnterRecord * record;

		Trc_VM_growJavaStack_WalkNewStack(vmThread);

		walkState.objectSlotWalkFunction = growSlotIterator;
		walkState.userData3 = (void *) oldStackStart;
		walkState.userData4 = (void *) oldStackEnd;
		walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_INCLUDE_ARRAYLET_LEAVES;
		vmThread->javaVM->walkStackFrames(vmThread, &walkState);

		/* Stack-allocated objects may escape into the interpreter enter records */

		record = vmThread->monitorEnterRecords;
		while (record != NULL) {
			FIX_IF_IN_RANGE(record->object);
			record = record->next;
		}
	}
#endif

	if (walkState.userData2) {
		Trc_VM_growJavaStack_KeepingOldStack(vmThread, walkState.userData2);
		oldStack->firstReferenceFrame = oldStack->end - ((UDATA *) walkState.userData2);
	} else {
		Trc_VM_growJavaStack_FreeingOldStack(vmThread, oldStack);
		newStack->previous = oldStack->previous;
		freeJavaStack(vmThread->javaVM, oldStack);
	}

	Trc_VM_growJavaStack_Success(vmThread);

done:
	Trc_VM_growJavaStack_Exit(vmThread);
	vmThread->omrVMThread->vmState = oldState;
	return rc;
}



static UDATA growFrameIterator(J9VMThread * vmThread, J9StackWalkState * walkState)
{
	/* JIT execution frames (i.e. not transition) require no relocation */

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (!walkState->jitInfo) {
#endif
		UDATA delta = (UDATA) walkState->userData1;
		UDATA ** newBP;

		switch((UDATA) walkState->pc) {
#ifdef J9VM_INTERP_NATIVE_SUPPORT
			case J9SF_FRAME_TYPE_JIT_JNI_CALLOUT:
#endif
			case J9SF_FRAME_TYPE_JNI_NATIVE_METHOD: {
				J9SFMethodFrame * methodFrame = (J9SFMethodFrame *) ((U_8 *) walkState->bp - sizeof(J9SFMethodFrame) + sizeof(UDATA));
				J9SFMethodFrame * newMethodFrame = (J9SFMethodFrame *) (((UDATA *) methodFrame) + delta);
				UDATA refCount = walkState->frameFlags & J9_SSF_JNI_PUSHED_REF_COUNT_MASK;
				UDATA tagFrame = FALSE;

				Trc_VM_growFrameIterator_JNIEntry(vmThread, walkState->bp, refCount, walkState->frameFlags, walkState->method);

				/* Relocate pushed JNI refs.  It is possible to have forwarded and unforwarded refs in the same frame. */

				if (refCount) {
					UDATA pushCount = ((UDATA *) methodFrame) - walkState->walkSP;
					UDATA * currentRef = walkState->walkSP + (pushCount - refCount);

#ifdef PAINT_OLD_STACK
					PAINT_STACK_MEMORY(currentRef);
#endif

					Trc_VM_growFrameIterator_PushCount(vmThread, pushCount);

					while (refCount) {
						/* Redirected refs are tagged pointers - if not tagged, it needs relocation */

						if (*currentRef && !(*currentRef & J9_REDIRECTED_REFERENCE)) {
							Trc_VM_growFrameIterator_FixRef(vmThread, currentRef + delta, ((UDATA) currentRef) + 1);
							*(currentRef + delta) = ((UDATA) currentRef) + 1;
							tagFrame = TRUE;
						}
						else {
							Trc_VM_growFrameIterator_RefFixed(vmThread, currentRef + delta, currentRef[delta]);
#ifdef PAINT_OLD_STACK
							*currentRef = FULL_SLOT_PAINT_VALUE;
#endif
						}
						++currentRef;
						--refCount;
					}

#ifdef PAINT_OLD_STACK
					vmThread->tempSlot = (UDATA) methodFrame;
#endif
				}

				/* If this frame is already an indirection frame, then any args have already all been relocated */

				if (!(walkState->frameFlags & J9_SSF_JNI_REFS_REDIRECTED)) {
					J9Method *nativeMethod = methodFrame->method;

#ifdef J9VM_INTERP_NATIVE_SUPPORT
					/* If this frame is a JIT JNI callout frame, there are no pushed args, but we need to remember the
					   original frame pointer so the enclosing JIT frame can be walked in the old stack (the refs passed to the
					   native method are going to be addresses of args/temps of the enclosing JIT method).
					*/

					if (walkState->frameFlags & J9_SSF_JIT_JNI_CALLOUT) {
						newMethodFrame->savedPC = (U_8 *) walkState->bp;
						Trc_VM_growFrameIterator_JITCallOut(vmThread, &(newMethodFrame->savedPC), newMethodFrame->savedPC);
#ifdef PAINT_OLD_STACK
						vmThread->returnValue = vmThread->tempSlot;
						vmThread->tempSlot = ((UDATA) (walkState->bp + 1)) + 1;
#endif
						/* Note: Do not fall out to the common code here, as we do not want to run the savedAg0EA fixup outside the case */
						newMethodFrame->specialFrameFlags |= J9_SSF_JNI_REFS_REDIRECTED;
						walkState->userData2 = walkState->bp;
						return J9_STACKWALK_KEEP_ITERATING;
					}
#endif
					/* Relocate arguments */

					if (NULL != nativeMethod) {
						/* Max size as argCount always <= 255 */
						U_32 argBits[8];
						U_32 *descriptionSlots = argBits;
						UDATA descriptionBitsRemaining = 0;
						U_32 description = 0;
						J9ROMClass *romClass = J9_CLASS_FROM_METHOD(nativeMethod)->romClass;
						J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(nativeMethod);
						UDATA * argPtr = walkState->arg0EA;

						j9localmap_ArgBitsForPC0(romClass, romMethod, descriptionSlots);
						while (argPtr != walkState->bp) {
							if (0 == descriptionBitsRemaining) {
								description = *descriptionSlots++;
								descriptionBitsRemaining = 32;
							}
							if (description & J9_REDIRECTED_REFERENCE) {
								Trc_VM_growFrameIterator_FixArg(vmThread, argPtr + delta, ((UDATA) argPtr) | J9_REDIRECTED_REFERENCE);
								*(argPtr + delta) = ((UDATA) argPtr) | J9_REDIRECTED_REFERENCE;
								tagFrame = TRUE;
							}
							description >>= 1;
							--descriptionBitsRemaining;
							--argPtr;
						}
					} else {
						/* certain JNI native stack (eg JVMTI events frames) specifies NULL nativeMethod, but still
						 * requires arguments to be relocated.
						 */
						UDATA * argPtr = walkState->arg0EA;

						while (argPtr != walkState->bp) {
							Trc_VM_growFrameIterator_FixArg(vmThread, argPtr + delta, ((UDATA) argPtr) | 1);
							*(argPtr + delta) = ((UDATA) argPtr) | 1;
							--argPtr;
							tagFrame = TRUE;
						}
					}

#ifdef PAINT_OLD_STACK
					/* Do it here to avoid painting the transition frame, which the jit stack walker will read after we return */
					PAINT_STACK_MEMORY(walkState->bp + 1);
					vmThread->tempSlot = (UDATA) (walkState->arg0EA + 1);
#endif
				}

				if (tagFrame) {
					newMethodFrame->specialFrameFlags |= J9_SSF_JNI_REFS_REDIRECTED;
					walkState->userData2 = walkState->bp;
				}

				Trc_VM_growFrameIterator_JNIExit(vmThread);
				break;
			}

#ifdef J9VM_INTERP_NATIVE_SUPPORT
			default:
				if (vmThread->javaVM->jitConfig) {
					if ((UDATA) walkState->pc > J9SF_MAX_SPECIAL_FRAME_TYPE) {
						if (*walkState->pc == 0xFF) { /* impdep2 = 0xFF - indicates a JNI call-in frame */
							if (walkState->walkedEntryLocalStorage) {
								if (addI2J(walkState, &(walkState->walkedEntryLocalStorage->i2jState))) {
addFailed:
									walkState->restartException = (void *) 1;
									return J9_STACKWALK_STOP_ITERATING;
								}
							}
						} else {
							/* Bytecode frame */
							if (walkState->frameFlags & J9_SSF_JIT_CALLIN) {
								J9SFJ2IFrame * j2iFrame = (J9SFJ2IFrame *) (walkState->unwindSP + delta);

								if (j2iFrame->previousJ2iFrame) {
									j2iFrame->previousJ2iFrame += delta;
								}
								if (addI2J(walkState, &(j2iFrame->i2jState))) {
									goto addFailed;
								}
							}
						}
					}
				}
				break;
#endif
		}

		/* Fix the saved arg0EA in the frame */

		newBP = ((UDATA **) walkState->bp) + delta;
		if (NULL != UNTAG2(*newBP, UDATA*)) {
			Trc_VM_growFrameIterator_UpdateArg0EA(vmThread, newBP, *newBP, *newBP + delta);
			*newBP += delta;
		} else {
			Trc_VM_growFrameIterator_LeaveArg0EA(vmThread, newBP, *newBP);
		}

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	}
#ifdef PAINT_OLD_STACK
	else {
		if (vmThread->tempSlot & J9_REDIRECTED_REFERENCE) {
			UDATA temp = vmThread->tempSlot & ~J9_REDIRECTED_REFERENCE;

			vmThread->tempSlot = vmThread->returnValue;
			PAINT_STACK_MEMORY(temp);					
			vmThread->tempSlot = (UDATA) (walkState->arg0EA + 1);
		}
	}
#endif
#endif

	return J9_STACKWALK_KEEP_ITERATING;
}



void freeStacks(J9VMThread * vmThread, UDATA * bp)
{
	J9JavaStack * keptStack = vmThread->stackObject;
	J9JavaStack * scanStack;
	UDATA relativeBP = keptStack->end - bp;

	Trc_VM_freeStacks_Entry(vmThread);

	while ((scanStack = keptStack->previous) != NULL) {
		if (scanStack->firstReferenceFrame >= relativeBP) {
			keptStack->previous = scanStack->previous;
			Trc_VM_freeStacks_Free(vmThread, scanStack, scanStack->size);
			freeJavaStack(vmThread->javaVM, scanStack);
		} else {
			keptStack = scanStack;
		}
	}

	Trc_VM_freeStacks_Exit(vmThread);
}



static void fixStackWalkState(J9StackWalkState * walkState, UDATA oldStackStart, UDATA oldStackEnd, UDATA delta)
{
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	UDATA ** registerCursor = (UDATA **) &(walkState->registerEAs);
	UDATA ** registerEnd = (UDATA **) (((U_8 *) registerCursor) + sizeof(walkState->registerEAs));

	while (registerCursor < registerEnd) {
		FIX_IF_IN_RANGE(*registerCursor);
		++registerCursor;
	}

	FIX_IF_IN_RANGE(walkState->i2jState);
	if (walkState->j2iFrame) {
		walkState->j2iFrame += delta;
	}
#endif

	FIX_IF_IN_RANGE(walkState->pcAddress);

	walkState->bp += delta;
	walkState->sp += delta;
	walkState->unwindSP += delta;
	walkState->arg0EA += delta;
	walkState->walkSP += delta;
}



#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static void growSlotIterator(J9VMThread * vmThread, J9StackWalkState * walkState, j9object_t * objectSlotInNewStack, const void * stackLocation)
{
	UDATA oldStackStart = (UDATA) walkState->userData3;
	UDATA oldStackEnd = (UDATA) walkState->userData4;
	UDATA * object = *((UDATA **) objectSlotInNewStack);

	/* Only worry about objects which are allocated on the stack */

	if (IS_IN_STACK(object)) {
		UDATA delta = (UDATA) walkState->userData1;

		*objectSlotInNewStack = (j9object_t) (object + delta);

		Trc_VM_growSlotIterator_Object(vmThread, stackLocation, object, *objectSlotInNewStack);
	}
}

#endif /* J9VM_INTERP_NATIVE_SUPPORT */


#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static UDATA addI2J(J9StackWalkState * walkState, J9I2JState * i2jState)
{
	J9I2JState ** element;

	if (!(element = (J9I2JState **) pool_newElement((J9Pool *) walkState->restartPoint))) {
		return 1;
	}
	*element = i2jState;
	return 0;
}

#endif /* J9VM_INTERP_NATIVE_SUPPORT */


#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static void fixI2J(void * element, void * userData)
{
	J9I2JState * i2jState = *((J9I2JState **) element);
	UDATA delta = (UDATA) userData;

	i2jState->returnSP += delta;
	i2jState->a0 += delta;
}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


#endif /* J9VM_INTERP_GROWABLE_STACKS */ /* End File Level Build Flags */

} /* extern "C" */
