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

#include <string.h>
#include <stdlib.h>

#include "j9cfg.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "j9port.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "objhelp.h"
#include "rommeth.h"
#include "stackwalk.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "ute.h"

/* Default message if no NLS message supplied, or cannot be looked up */

#define NATIVE_MEMORY_OOM_MSG "native memory exhausted"

static void internalSetCurrentExceptionWithCause(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage, const char *utfMessage, j9object_t cause);


/* Helper macro for ALWAYS_TRIGGER_J9HOOK_VM_METHOD_RETURN */
#ifdef J9VM_INTERP_NATIVE_SUPPORT
#define HOOK_METHOD_TYPE(walkState)	 ( ( (walkState)->jitInfo == NULL ) ? 0 : 1 )
#else
#define HOOK_METHOD_TYPE(walkState)	 ( 0 )
#endif

void  
setCurrentExceptionUTF(J9VMThread * vmThread, UDATA exceptionNumber, const char * detailUTF)
{
	j9object_t detailString = NULL;

	Trc_VM_setCurrentExceptionUTF_Entry(vmThread, exceptionNumber, detailUTF);

	vmThread->currentException = NULL;
	if ((detailUTF != NULL) && (NULL != J9VMJAVALANGSTRING_OR_NULL(vmThread->javaVM))){
		detailString = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, (U_8 *) detailUTF, (UDATA) strlen(detailUTF), 0);
	}
	if (vmThread->currentException == NULL) {
		setCurrentExceptionWithUtfCause(vmThread, exceptionNumber, (UDATA *) detailString, detailUTF, NULL);
	} else {
		Trc_VM_setCurrentExceptionUTF_StringAllocFailed(vmThread);
	}

	Trc_VM_setCurrentExceptionUTF_Exit(vmThread);
}



void  
setCurrentExceptionNLS(J9VMThread * vmThread, UDATA exceptionNumber, U_32 moduleName, U_32 messageNumber)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	const char * msg;

	msg = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(OMRPORT_FROM_J9PORT(PORTLIB), J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, moduleName, messageNumber, NULL);
	setCurrentExceptionUTF(vmThread, exceptionNumber, msg);
}


/**
 * Creates exception with nls message; substitutes string values into error message.
 *
 * @param vmThread current VM thread
 * @param nlsModule nls module name
 * @param nlsID nls number
 * @param exceptionIndex index to exception in vm constant pool
 * @param ... arguments to be substituted into error message
 */
void
setCurrentExceptionNLSWithArgs(J9VMThread * vmThread, U_32 nlsModule, U_32 nlsID, UDATA exceptionIndex, ...)
{
	va_list stringArgList;
	UDATA msgCharLength = 0;
	const char *nlsMsgFormat = NULL;
	char *msgChars = NULL;
	PORT_ACCESS_FROM_VMC(vmThread);
	OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);

	nlsMsgFormat = omrnls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
			nlsModule,
			nlsID,
			"");

	va_start(stringArgList, exceptionIndex);
	msgCharLength = j9str_vprintf(NULL, 0, nlsMsgFormat, stringArgList);
	va_end(stringArgList);

	msgChars = (char *)j9mem_allocate_memory(msgCharLength, J9MEM_CATEGORY_VM);
	if (NULL != msgChars) {
		va_start(stringArgList, exceptionIndex);
		j9str_vprintf(msgChars, msgCharLength, nlsMsgFormat, stringArgList);
		va_end(stringArgList);
	}

	setCurrentExceptionUTF(vmThread, exceptionIndex, msgChars);
	j9mem_free_memory(msgChars);
}
	
static UDATA   
decompStackHeadSearch(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	if (((UDATA *) walkState->userData1) == walkState->bp) {
		/* We're at the catch frame */
		return J9_STACKWALK_STOP_ITERATING;
	}

	return J9_STACKWALK_KEEP_ITERATING;
} 
	

/** 
 * \brief	Correct the decompilation stack of the passed in walkState
 * 
 * @param currentThread		current thread 
 * @param walkState			stack walk state to be fixed 
 * @param rewalkAllWalkedFrames  set to true in order to rewalk the same amount of frames as upto the frame of the exceptionHandlerSearch that called us
 * @return 					none
 *      
 *	The <code>exceptionHandlerSearch</code> function issues various events that
 *	might call user callbacks such as jvmti pop frame event. Prior to the
 *	callback we release vm access which in turn might cause our thread being
 *	marked for decompilation (for example if a jvmti agent single steps or sets/removes
 *	a breakpoint). The passed in walkState->decompilationStack will not always be valid
 *	when we come back. 
 *
 *  if (rewalkAllWalkedFrames == FALSE) 
 *		This function walks the top two frames to determine the right decompilation stack head
 *		and use it to update the passed in walkState->decompilationStack. We do this when we had 
 *      to dropToCurrentFrame prior to issuing an event (read: releasing access which might result
 *      in class loading, calling out to any jvmti agent which expect the stack to be in the correct shape, 
 *      hence we dropToCurrentFrame so that we match what they expect to see instead of having any 
 *      leftover/special frames ontop). We only need to walk 2 frames to figure out the correct 
 *      decompilation side-stack head.
 *  else
 *      In case that the exceptionHandlerSearch stack walk iterator was used for a stack walk ment
 *      to simply walk the stack and find the catch frame (ie no frame dropping involved). Once we reacquire
 *      access, we must re-walk the stack for at least as many frames as to reach the catch frame. 
 * 
 */

static void
syncDecompilationStackAfterReleasingVMAccess(J9VMThread *currentThread, J9StackWalkState *walkState, UDATA rewalkAllWalkedFrames)
{
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	if (J9_FSD_ENABLED(currentThread->javaVM)) {	
		J9StackWalkState ws;

		/* Stack walk 2 frames in order to initialize ws.decompilationStack */ 

		ws.skipCount = 0;
		ws.walkThread = walkState->walkThread;
		if (rewalkAllWalkedFrames) {
			/* Rewalk all frames walked during exception handler search */
			ws.frameWalkFunction = decompStackHeadSearch;
			ws.userData1 = (void *) walkState->bp;
			ws.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_VISIBLE_ONLY;
		} else {
			/* Walk only the top frame */
			ws.maxFrames = 2;
			ws.flags = J9_STACKWALK_COUNT_SPECIFIED ;
		}
		currentThread->javaVM->walkStackFrames(currentThread, &ws);

		/* Update the decomp stack of the passed in walk state with the most upto
		 * date head record */

		walkState->decompilationStack = ws.decompilationStack;
	}
#endif	
}

/*
 *  Helper function - check if method has been enabled for tracing
 *  @return 	non-zero if the method tracing is enabled, otherwise 0 is returned
 */
UDATA
methodBeingTraced(J9JavaVM *vm, J9Method *method)
{
	UDATA rc = FALSE;
	if (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_METHOD_TRACE_ENABLED) {
		U_8 *pmethodflags = fetchMethodExtendedFlagsPointer(method);
		if (J9_RAS_IS_METHOD_TRACED(*pmethodflags)) {
			rc = TRUE;
		}
	}
	return rc;
}

/*
	Helper function, called for each stack frame.  Assumptions:
		1) current thread and walk state are passed as parameters.
		2) walkState->userData1 is a flags field that controls the walk (input)
		3) walkState->userData2 is the handler PC (output, not filled in for JNI catches)
		4) walkState->userData3 is a cookie describing the frame type (output)
		5) walkState->userData4 is the thrown class (input)
*/

UDATA   
exceptionHandlerSearch(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	J9JavaVM * vm = walkState->walkThread->javaVM;

	Trc_VM_exceptionHandlerSearch_Entry(currentThread);

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (walkState->jitInfo != NULL) {
		if (vm->jitExceptionHandlerSearch(currentThread, walkState) == J9_STACKWALK_STOP_ITERATING) {
			Trc_VM_exceptionHandlerSearch_Exit(currentThread, J9_STACKWALK_STOP_ITERATING);
			return J9_STACKWALK_STOP_ITERATING;
		}
	} else
#endif

	/* Special frames (natives in particular) do not handle exceptions */

	if (!IS_SPECIAL_FRAME_PC(walkState->pc)) {
		J9ROMMethod * romMethod;

		if (walkState->pc[0] == 0xFF) { /* impdep2 = 0xFF - indicates a JNI call-in frame */
			walkState->userData3 = (void *) J9_EXCEPT_SEARCH_JNI_HANDLER;
			Trc_VM_exceptionHandlerSearch_Exit(currentThread, J9_STACKWALK_STOP_ITERATING);
			return J9_STACKWALK_STOP_ITERATING;
		}
		romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);
		if (J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)) {
			{
				J9ExceptionInfo * eData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
				UDATA ranges = eData->catchCount;

				if (ranges != 0) {
					J9ExceptionHandler * handlerCursor = (J9ExceptionHandler *) (eData + 1);
					U_32 deltaPC = (U_32) walkState->bytecodePCOffset;

					do {
						if ((deltaPC >= handlerCursor->startPC) && (deltaPC < handlerCursor->endPC)) {
							if (isExceptionTypeCaughtByHandler(currentThread, walkState->userData4, walkState->constantPool, handlerCursor->exceptionClassIndex, walkState) != 0) {
								U_32 handlerPC = handlerCursor->handlerPC;

								walkState->userData1 = (void *)(UDATA) handlerPC;
								walkState->userData2 = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod) + handlerPC;
								walkState->userData3 = (void *)(UDATA) J9_EXCEPT_SEARCH_JAVA_HANDLER;
								Trc_VM_exceptionHandlerSearch_Exit(currentThread, J9_STACKWALK_STOP_ITERATING);
								return J9_STACKWALK_STOP_ITERATING;
							}
						}
						++handlerCursor;
					} while (--ranges != 0);
				}
			}
		}
	}

	/* Exception not handled in this frame */

	if ((((UDATA) walkState->userData1) & J9_EXCEPT_SEARCH_NO_UNWIND) == 0) {
		J9StackWalkState tempWalkState;

		/* Report the method return and frame pop hooks if necessary */

		if (IS_SPECIAL_FRAME_PC(walkState->pc)) {
			if (walkState->pc == (U_8 *) J9SF_FRAME_TYPE_JNI_NATIVE_METHOD) {
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
				if ((walkState->frameFlags & J9_STACK_FLAGS_JIT_JNI_CALL_OUT_FRAME) == 0)
#endif
				{
					UDATA traced = methodBeingTraced(vm, walkState->method);
					UDATA hooked = J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_NATIVE_METHOD_RETURN);
					if (traced || hooked ) {
						/* Do not drop to current frame here:
						 *
						 * 1) The native method is guaranteed to be the top frame
						 * 2) The stack must be left alone to ensure consistency of the JNI refs and stack frame flags
						 */
	
						PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, walkState->restartException);
						tempWalkState.previous = currentThread->stackWalkState;
						currentThread->stackWalkState = &tempWalkState;
						if (traced) {
							UTSI_TRACEMETHODEXIT_FROMVM(vm, currentThread, walkState->method, walkState->restartException, NULL, 0);
						}
						if (hooked) {
							ALWAYS_TRIGGER_J9HOOK_VM_NATIVE_METHOD_RETURN(vm->hookInterface, currentThread, walkState->method, TRUE, NULL, NULL);
						}
						currentThread->stackWalkState = tempWalkState.previous;
						walkState->restartException = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
#ifdef J9VM_JIT_FULL_SPEED_DEBUG						
						walkState->decompilationStack = walkState->walkThread->decompilationStack;
#endif									
					}
				}

				/* The stack will be dropped before any further events are reported or classes are loaded,
				 * so the JNI cleanup code must be run before continuing the unwind. Note that this can happen
				 * only once per walk, since the exception will be caught by a JNI call-in frame before another JNI
				 * native method frame could be seen.
				 */

				returnFromJNI(currentThread, walkState->bp);
			}
		} else {
			UDATA traced = methodBeingTraced(vm, walkState->method);
			UDATA hooked = J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_NATIVE_METHOD_RETURN);
			if (traced || hooked) {
				walkState->dropToCurrentFrame(walkState);
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, walkState->restartException);
				tempWalkState.previous = currentThread->stackWalkState;
				currentThread->stackWalkState = &tempWalkState;
				if (traced) {
					UTSI_TRACEMETHODEXIT_FROMVM(vm, currentThread, walkState->method, walkState->restartException, NULL, HOOK_METHOD_TYPE(walkState) );
				}
				if (hooked) {
					ALWAYS_TRIGGER_J9HOOK_VM_METHOD_RETURN(vm->hookInterface, currentThread, walkState->method, TRUE, NULL, HOOK_METHOD_TYPE(walkState) );
				}
				currentThread->stackWalkState = tempWalkState.previous;
				walkState->restartException = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				syncDecompilationStackAfterReleasingVMAccess(currentThread, walkState, FALSE);
			}
#ifdef J9VM_INTERP_NATIVE_SUPPORT
			if (walkState->jitInfo != NULL) {
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
				J9JITDecompilationInfo *decompilationRecord = walkState->decompilationRecord;
				if ((NULL != decompilationRecord) && (decompilationRecord->reason & JITDECOMP_FRAME_POP_NOTIFICATION)) {
					if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_FRAME_POP)) {
						walkState->dropToCurrentFrame(walkState);
						PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, walkState->restartException);
						tempWalkState.previous = currentThread->stackWalkState;
						currentThread->stackWalkState = &tempWalkState;
						ALWAYS_TRIGGER_J9HOOK_VM_FRAME_POP(vm->hookInterface, currentThread, walkState->method, TRUE);
						currentThread->stackWalkState = tempWalkState.previous;
						walkState->restartException = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
						syncDecompilationStackAfterReleasingVMAccess(currentThread, walkState, FALSE);
					}				
				}
#endif
			} else
#endif
			{
				if (*walkState->bp & J9SF_A0_REPORT_FRAME_POP_TAG) {
					if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_FRAME_POP)) {
						walkState->dropToCurrentFrame(walkState);
						PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, walkState->restartException);
						tempWalkState.previous = currentThread->stackWalkState;
						currentThread->stackWalkState = &tempWalkState;
						ALWAYS_TRIGGER_J9HOOK_VM_FRAME_POP(vm->hookInterface, currentThread, walkState->method, TRUE);
						currentThread->stackWalkState = tempWalkState.previous;
						walkState->restartException = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
						syncDecompilationStackAfterReleasingVMAccess(currentThread, walkState, FALSE);
					}
				}
			}

			{
				J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);
				/* Exit the monitor for synchronized methods */
				if (romMethod->modifiers & J9AccSynchronized) {
					j9object_t syncObject;

#ifdef J9VM_INTERP_NATIVE_SUPPORT
					if (walkState->jitInfo != NULL) {
						if (romMethod->modifiers & J9AccStatic) {
							J9Class *syncClass = walkState->constantPool->ramClass;

							syncObject = J9VM_J9CLASS_TO_HEAPCLASS(syncClass);
						} else {
							syncObject = *((j9object_t*) walkState->arg0EA);
						}
					} else
#endif
					{
						syncObject = (j9object_t) (walkState->bp[1]);
					}

					if (objectMonitorExit(currentThread, syncObject) != 0) {
						j9object_t ime;
						J9StackWalkState tempWalkState;

						walkState->dropToCurrentFrame(walkState);
						PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, walkState->restartException);
						tempWalkState.previous = currentThread->stackWalkState;
						currentThread->stackWalkState = &tempWalkState;
						setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
						currentThread->stackWalkState = tempWalkState.previous;
						ime = currentThread->currentException;
						walkState->restartException = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
						if (ime != NULL) {
							currentThread->currentException = NULL;
							walkState->restartException = ime;
							walkState->userData4 = J9OBJECT_CLAZZ(currentThread, ime);
						}
						syncDecompilationStackAfterReleasingVMAccess(currentThread, walkState, FALSE);
					}
				}
			}
		}
	}

	Trc_VM_exceptionHandlerSearch_Exit(currentThread, J9_STACKWALK_KEEP_ITERATING);
	return J9_STACKWALK_KEEP_ITERATING;
}



UDATA  
isExceptionTypeCaughtByHandler(J9VMThread *currentThread, J9Class *thrownExceptionClass, J9ConstantPool *constantPool, UDATA handlerIndex, J9StackWalkState *walkState)
{
	J9Class * caughtExceptionClass;
	UDATA caughtDepth;
	UDATA thrownDepth;

	/* Handler index 0 means catch all */

	if (handlerIndex == 0) {
		return TRUE;
	}

	/* Get the caught class - might not be loaded yet */

	caughtExceptionClass = ((J9RAMClassRef *) (constantPool + handlerIndex))->value;
	if (caughtExceptionClass == NULL) {
		J9StackWalkState tempWalkState;

		if ((((UDATA) walkState->userData1) & J9_EXCEPT_SEARCH_NO_UNWIND) == 0) {
			walkState->dropToCurrentFrame(walkState);
		}
		PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, walkState->restartException);
		tempWalkState.previous = currentThread->stackWalkState;
		currentThread->stackWalkState = &tempWalkState;
		caughtExceptionClass = resolveClassRef(currentThread, constantPool, handlerIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
		currentThread->stackWalkState = tempWalkState.previous;
		walkState->restartException = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);

		if ((((UDATA) walkState->userData1) & J9_EXCEPT_SEARCH_NO_UNWIND) == 0) {
			syncDecompilationStackAfterReleasingVMAccess(currentThread, walkState, FALSE);
		} else {
			syncDecompilationStackAfterReleasingVMAccess(currentThread, walkState, TRUE);
		}
		
		/* If we were unable to load the class, then we can only say that the exception is not caught */

		if (caughtExceptionClass == NULL) {
			currentThread->currentException = NULL;
			return FALSE;
		}
	}

	/* See if thrown class is compatible with caught class */

	if (caughtExceptionClass == thrownExceptionClass) {
		return TRUE;
	}
	caughtDepth = J9CLASS_DEPTH(caughtExceptionClass);
	thrownDepth = J9CLASS_DEPTH(thrownExceptionClass);
	if (thrownDepth <= caughtDepth) {
		return FALSE;
	}
	return thrownExceptionClass->superclasses[caughtDepth] == caughtExceptionClass;
}


void  
setCurrentException(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage)
{
	setCurrentExceptionWithUtfCause(currentThread, exceptionNumber, detailMessage, NULL, NULL);
}

/**
 * Creates exception with its cause and detailed message
 *
 * @param currentThread
 * @param exceptionNumber    
 * @param detailMessage         
 * @param cause 
 * 
 */
static void
internalSetCurrentExceptionWithCause(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage, const char *utfMessage, j9object_t cause)
{
	UDATA index;
	UDATA * preservedMessage;
	UDATA resetOutOfMemory;
	j9object_t exception;
	J9Class * exceptionClass;
	UDATA constructorIndex;
	UDATA exceptionFlags = 0;

	index = exceptionNumber & J9_EXCEPTION_INDEX_MASK;
	constructorIndex = exceptionNumber & J9_EX_CTOR_TYPE_MASK;

	Trc_VM_setCurrentException_Entry(currentThread, index, constructorIndex, detailMessage);

	/* Clear the current exception so that it doesn't get re-triggered while loading more exception classes */

	currentThread->currentException = NULL;
	currentThread->privateFlags &= ~J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;

	/* Preserve cause object */

	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, cause);

	/* Preserve the detail message object (if it is an object) */

	switch (constructorIndex) {
		case J9_EX_CTOR_INT:
		case J9_EX_CTOR_CLASS_CLASS:
			preservedMessage = NULL;
			break;
		default:
			preservedMessage = detailMessage;
			break;
	}
	PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, (j9object_t) preservedMessage);

#ifdef J9VM_INTERP_GROWABLE_STACKS
	/*  If we are throwing StackOverflowError for the first time in this thread, grow the stack once more */

	if (index == J9VMCONSTANTPOOL_JAVALANGSTACKOVERFLOWERROR) {
		J9JavaVM * vm = currentThread->javaVM;
		UDATA stackSize = vm->stackSize + OMR_MAX(J9_STACK_OVERFLOW_AND_HEADER_SIZE, vm->stackSizeIncrement);

		if (currentThread->stackObject->size < stackSize) {
			growJavaStack(currentThread, stackSize);
		}
		if (J9_ARE_NO_BITS_SET(currentThread->privateFlags, J9_PRIVATE_FLAGS_STACK_OVERFLOW)) {
			exceptionFlags |= J9_PRIVATE_FLAGS_STACK_OVERFLOW;
		}
	}
#endif

	/* Keep track of the fact that an exception is being constructed */

	if (J9_ARE_NO_BITS_SET(currentThread->privateFlags, J9_PRIVATE_FLAGS_CONSTRUCTING_EXCEPTION)) {
		exceptionFlags |= J9_PRIVATE_FLAGS_CONSTRUCTING_EXCEPTION;
	}
	currentThread->privateFlags |= exceptionFlags;

	/* Handle OutOfMemory specially
	 *
	 * To prevent recursion of OOM being called, following three states are in effect.
	 *
	 * OOM_STATE_1 = First OOM. Construct OOM. Set currentThread->privateFlags |= J9_PRIVATE_FLAGS_OUT_OF_MEMORY
	 * OOM_STATE_2 = Already throwing OOM. Use cached OOM and call exception constructor (internalSendExceptionConstructor)
	 * 				 and set currentThread->privateFlags |= J9_PRIVATE_FLAGS_FINAL_CALL_OUT_OF_MEMORY
	 * OOM_STATE_3 = Already throwing cached OOM. Skip calling the constructor and just throw the exception.
	 *
	 */

	resetOutOfMemory = FALSE;
	currentThread->privateFlags &= ~J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE;
	if (index == J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR) {
		J9JavaVM * vm = currentThread->javaVM;

		/* If java.lang.Class has not yet been loaded, don't try to allocate the exception - just bail */

		if (J9VMJAVALANGCLASS_OR_NULL(vm) == NULL) {
			Trc_VM_setCurrentException_ClassNotLoaded(currentThread);
			Assert_VM_fatalOOM();
			/* Execution never reaches this point */
		}

		if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_RESOURCE_EXHAUSTED)) {
			UDATA resourceTypes;
				
			/* The caller might have specified OOM exception type bits */
			resourceTypes = exceptionNumber & J9_EX_OOM_TYPE_MASK;

			/* No bits specified, we are dealing with with a regular Java Heap OOM */
			if (resourceTypes == 0) {
				resourceTypes = J9_EX_OOM_JAVA_HEAP;  	
			}

			ALWAYS_TRIGGER_J9HOOK_VM_RESOURCE_EXHAUSTED(vm->hookInterface, currentThread, resourceTypes, utfMessage);
		}			

		/* OOM_STATE_2 : If we're already throwing OutOfMemory, use the cached Throwable */
		if (currentThread->privateFlags & J9_PRIVATE_FLAGS_OUT_OF_MEMORY) {
			j9object_t walkback;

throwOutOfMemory:
			exception = currentThread->outOfMemoryError;
			if (exception == NULL) {
				Trc_VM_setCurrentException_NoCachedOOM(currentThread);
				Assert_VM_fatalOOM();
				/* Execution never reaches this point */
			}

			/* Check whether we are in OOM_STATE_2 or OOM_STATE_3 */
			if (currentThread->privateFlags & J9_PRIVATE_FLAGS_FINAL_CALL_OUT_OF_MEMORY) {
				/* OOM_STATE_3 */
				currentThread->currentException = exception;
				DROP_OBJECT_IN_SPECIAL_FRAME(currentThread); /* preservedMessage */
				DROP_OBJECT_IN_SPECIAL_FRAME(currentThread); /* cause */
				goto done;
			}
			
			/* OOM_STATE_2 */
			currentThread->privateFlags |= J9_PRIVATE_FLAGS_FINAL_CALL_OUT_OF_MEMORY;
			
			/* If there's a walkback object in the Throwable, zero it */

			exceptionClass = J9OBJECT_CLAZZ(currentThread, exception);
			walkback = J9VMJAVALANGTHROWABLE_WALKBACK(currentThread, exception);

			if (walkback != NULL) {
				U_32 i;

				for (i = 0; i < J9INDEXABLEOBJECT_SIZE(currentThread, walkback); i++) {
					J9JAVAARRAYOFUDATA_STORE(currentThread, walkback, i, 0);
				} 
				currentThread->privateFlags |= J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE;
			}
						
			goto sendConstructor;
		}
		/* OOM_STATE_1 */
		currentThread->privateFlags |= J9_PRIVATE_FLAGS_OUT_OF_MEMORY;
		resetOutOfMemory = TRUE;
	}

	/* Get the class of the exception, loading the class if necessary.  
	 * Fetch known class fatal exits if the class cannot be found. 
     */
	exceptionClass = internalFindKnownClass(currentThread, index, J9_FINDKNOWNCLASS_FLAG_INITIALIZE | J9_FINDKNOWNCLASS_FLAG_NON_FATAL);
	if (exceptionClass == NULL) {
		Trc_VM_setCurrentException_UnableToLoadExceptionClass(currentThread);
		DROP_OBJECT_IN_SPECIAL_FRAME(currentThread); /* preservedMessage */
		DROP_OBJECT_IN_SPECIAL_FRAME(currentThread); /* cause */
		/* Exception class could not be loaded/initialized - there should be an exception pending now, if not, throw OOM */
		if (currentThread->currentException == NULL) {
			Trc_VM_setCurrentException_NoExceptionPending(currentThread);
			setHeapOutOfMemoryError(currentThread);
		}
		goto done;
	}

	/* Make an instance of the exception class */

	exception = currentThread->javaVM->memoryManagerFunctions->J9AllocateObject(
		currentThread, exceptionClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (exception == NULL) {
		Trc_VM_setCurrentException_UnableToAllocateException(currentThread);
		goto throwOutOfMemory;
	}

	/* Construct the exception */

sendConstructor:
	switch (constructorIndex) {
		case J9_EX_CTOR_INT:
		case J9_EX_CTOR_CLASS_CLASS:
			break;
		default:
			detailMessage = *((UDATA **) (currentThread->sp));
			break;
	}

	/* Look up and call the exception constructor */

	*((j9object_t*) (currentThread->sp)) = exception;
	internalSendExceptionConstructor(currentThread, exceptionClass, exception, (j9object_t) detailMessage, constructorIndex);
	currentThread->privateFlags &= ~J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE;
	exception = POP_OBJECT_IN_SPECIAL_FRAME(currentThread); /* preservedMessage */

	/* Retrieve the cause, and if it's not NULL, set it in the exception */

	cause = POP_OBJECT_IN_SPECIAL_FRAME(currentThread); /* cause */
	if (currentThread->currentException == NULL) {
		if (cause != NULL) {
			sendInitCause(currentThread, (j9object_t) exception, cause);
			exception = (j9object_t) currentThread->returnValue; /* initCause returns the receiver */
		}
	} else {
		Trc_VM_setCurrentException_ExceptionDuringConstructor(currentThread);
	}

	/* If no exception occurred during construction, report systhrow for the new exception */

	if (currentThread->currentException == NULL) {
		j9object_t localException = (j9object_t) exception;

		TRIGGER_J9HOOK_VM_EXCEPTION_SYSTHROW(currentThread->javaVM->hookInterface, currentThread, localException);
		currentThread->currentException = localException;
		currentThread->privateFlags |= J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
	} else {
		Trc_VM_setCurrentException_ExceptionDuringConstructor(currentThread);
	}

done:
	/* Reset the recursion-prevention flags */

	if (resetOutOfMemory) {
		currentThread->privateFlags &= ~J9_PRIVATE_FLAGS_OUT_OF_MEMORY;
		currentThread->privateFlags &= ~J9_PRIVATE_FLAGS_FINAL_CALL_OUT_OF_MEMORY;
	}
	currentThread->privateFlags &= ~exceptionFlags;

	Trc_VM_setCurrentException_Exit(currentThread);
}

typedef struct J9RedirectedSetCurrentExceptionWithCauseArgs {
	J9VMThread *currentThread;
	UDATA exceptionNumber;
	UDATA *detailMessage;
	const char *utfMessage;
	j9object_t cause;
} J9RedirectedSetCurrentExceptionWithCauseArgs;

/**
 * This is an helper function to call internalSetCurrentExceptionWithCause indirectly from gpProtectAndRun function.
 * 
 * @param entryArg	Argument structure (J9RedirectedSetCurrentExceptionWithCauseArgs).
 */
static UDATA
gpProtectedSetCurrentExceptionWithCause(void *entryArg)
{
	J9RedirectedSetCurrentExceptionWithCauseArgs *args = (J9RedirectedSetCurrentExceptionWithCauseArgs *) entryArg;

	internalSetCurrentExceptionWithCause(args->currentThread, args->exceptionNumber, args->detailMessage, args->utfMessage, args->cause);

	return 0;					/* return value required to conform to port library definition */
}

/**
 * This function makes sure that call to "internalSetCurrentExceptionWithCause" is gpProtected
 *
 * @param currentThread
 * @param exceptionNumber    
 * @param detailMessage         
 * @param cause  
 *
 */
void  
setCurrentExceptionWithCause(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage, j9object_t cause)
{
	setCurrentExceptionWithUtfCause(currentThread, exceptionNumber, detailMessage, NULL, cause);
}

void
setCurrentExceptionWithUtfCause(J9VMThread *currentThread, UDATA exceptionNumber, UDATA *detailMessage, const char *utfMessage, j9object_t cause)
{
	if (currentThread->gpProtected) {
		internalSetCurrentExceptionWithCause(currentThread, exceptionNumber, detailMessage, utfMessage, cause);
	} else {
		J9RedirectedSetCurrentExceptionWithCauseArgs handlerArgs;

		handlerArgs.currentThread = currentThread;
		handlerArgs.exceptionNumber = exceptionNumber;
		handlerArgs.detailMessage = detailMessage;
		handlerArgs.utfMessage = utfMessage;
		handlerArgs.cause = cause;

		gpProtectAndRun(gpProtectedSetCurrentExceptionWithCause, (JNIEnv *)currentThread, &handlerArgs);

	}
}

j9object_t   
walkStackForExceptionThrow(J9VMThread * currentThread, j9object_t exception, UDATA walkOnly)
{
	J9StackWalkState * walkState = currentThread->stackWalkState;

	/* NOTE: This helper must use the current thread's current walk state, not a C stack-allocated one */

	walkState->skipCount = 0;
	walkState->frameWalkFunction = exceptionHandlerSearch;
	walkState->userData1 = (void *)(UDATA) (walkOnly ? J9_EXCEPT_SEARCH_NO_UNWIND : 0);
	walkState->userData2 = NULL;
	walkState->userData3 = (void *)(UDATA) J9_EXCEPT_SEARCH_NO_HANDLER;
	walkState->userData4 = J9OBJECT_CLAZZ(currentThread, exception);
	walkState->restartException = exception;
	walkState->walkThread = currentThread;
	walkState->flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_VISIBLE_ONLY;
	if (!walkOnly) {
		walkState->flags |= (J9_STACKWALK_INCLUDE_CALL_IN_FRAMES | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_MAINTAIN_REGISTER_MAP);
	}
	/* PR 81484: Clear jitStackFrameFlags before the walk.
	 * 1) It is not used by the stack walker.
	 * 2) It could affect code that runs to load exception classes during the walk.
	 */
	currentThread->jitStackFrameFlags = 0;

	currentThread->javaVM->walkStackFrames(currentThread, walkState);

	return walkState->restartException;
}






void  
setClassCastException(J9VMThread *currentThread, J9Class * instanceClass, J9Class * castClass)
{
	J9ClassCastParms detailMessage;

	detailMessage.instanceClass = instanceClass;
	detailMessage.castClass = castClass;
	setCurrentException(currentThread, J9_EX_CTOR_CLASS_CLASS + J9VMCONSTANTPOOL_JAVALANGCLASSCASTEXCEPTION, (UDATA *) &detailMessage);
}




void  
setClassLoadingConstraintError(J9VMThread * currentThread, J9ClassLoader * initiatingLoader, J9Class * existingClass)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	char * msg = NULL;
	const char * nlsMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_LOADING_CONSTRAINT_VIOLATION, NULL);
	if (nlsMessage != NULL) {
		j9object_t initiatingLoaderObject = initiatingLoader->classLoaderObject;
		J9Class * initiatingLoaderClass = J9OBJECT_CLAZZ(currentThread, initiatingLoaderObject);
		J9UTF8 * initiatingLoaderClassNameUTF = J9ROMCLASS_CLASSNAME(initiatingLoaderClass->romClass);
		U_16 initiatingLoaderClassNameLength = J9UTF8_LENGTH(initiatingLoaderClassNameUTF);
		U_8 * initiatingLoaderClassName = J9UTF8_DATA(initiatingLoaderClassNameUTF);
		I_32 initiatingLoaderHash = objectHashCode(currentThread->javaVM, initiatingLoaderObject);
		J9ClassLoader * definingLoader = existingClass->classLoader;
		j9object_t definingLoaderObject = definingLoader->classLoaderObject;
		J9Class * definingLoaderClass = J9OBJECT_CLAZZ(currentThread, definingLoaderObject);
		J9UTF8 * definingLoaderClassNameUTF = J9ROMCLASS_CLASSNAME(definingLoaderClass->romClass);
		U_16 definingLoaderClassNameLength = J9UTF8_LENGTH(definingLoaderClassNameUTF);
		U_8 * definingLoaderClassName = J9UTF8_DATA(definingLoaderClassNameUTF);
		I_32 definingLoaderHash = objectHashCode(currentThread->javaVM, definingLoaderObject);
		J9UTF8 * existingClassNameUTF = J9ROMCLASS_CLASSNAME(existingClass->romClass);
		U_16 existingClassNameLength = J9UTF8_LENGTH(existingClassNameUTF);
		U_8 * existingClassName = J9UTF8_DATA(existingClassNameUTF);
		UDATA msgLen = j9str_printf(PORTLIB, NULL, 0, nlsMessage,
			initiatingLoaderClassNameLength, initiatingLoaderClassName, initiatingLoaderHash,
			existingClassNameLength, existingClassName,
			definingLoaderClassNameLength, definingLoaderClassName, definingLoaderHash);
		msg = j9mem_allocate_memory(msgLen, OMRMEM_CATEGORY_VM);
		/* msg NULL check omitted since str_printf accepts NULL (as above) */
		j9str_printf(PORTLIB, msg, msgLen, nlsMessage,
			initiatingLoaderClassNameLength, initiatingLoaderClassName, initiatingLoaderHash,
			existingClassNameLength, existingClassName,
			definingLoaderClassNameLength, definingLoaderClassName, definingLoaderHash);
	}

	setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, msg);
	j9mem_free_memory(msg);
}

void  
setClassLoadingConstraintSignatureError(J9VMThread *currentThread, J9ClassLoader *loader1, J9Class *class1, J9ClassLoader *loader2, J9Class *class2, J9Class *exceptionClass, U_8 *methodName, UDATA methodNameLength, U_8 *signature, UDATA signatureLength)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	char * msg = NULL;
	const char * nlsMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_LOADING_CONSTRAINT_SIG_VIOLATION, NULL);
	if (nlsMessage != NULL) {
		/* Loader1 name and length */
		j9object_t loader1Object = loader1->classLoaderObject;
		J9Class * loader1Class = J9OBJECT_CLAZZ(currentThread, loader1Object);
		J9UTF8 * loader1ClassNameUTF = J9ROMCLASS_CLASSNAME(loader1Class->romClass);
		U_16 loader1ClassNameLength = J9UTF8_LENGTH(loader1ClassNameUTF);
		U_8 * loader1ClassName = J9UTF8_DATA(loader1ClassNameUTF);
		I_32 loader1Hash = objectHashCode(currentThread->javaVM, loader1Object);

		/* Loader2 name and length */
		j9object_t loader2Object = loader2->classLoaderObject;
		J9Class * loader2Class = J9OBJECT_CLAZZ(currentThread, loader2Object);
		J9UTF8 * loader2ClassNameUTF = J9ROMCLASS_CLASSNAME(loader2Class->romClass);
		U_16 loader2ClassNameLength = J9UTF8_LENGTH(loader2ClassNameUTF);
		U_8 * loader2ClassName = J9UTF8_DATA(loader2ClassNameUTF);
		I_32 loader2Hash = objectHashCode(currentThread->javaVM, loader2Object);

		/* Class1 name and length */
		J9UTF8 * class1NameUTF = J9ROMCLASS_CLASSNAME(class1->romClass);
		U_16 class1ClassNameLength = J9UTF8_LENGTH(class1NameUTF);
		U_8 * class1ClassName = J9UTF8_DATA(class1NameUTF);

		/* Class2 name and length */
		J9UTF8 * class2NameUTF = J9ROMCLASS_CLASSNAME(class2->romClass);
		U_16 class2ClassNameLength = J9UTF8_LENGTH(class2NameUTF);
		U_8 * class2ClassName = J9UTF8_DATA(class2NameUTF);

		/* ExceptionClass name and length */
		J9UTF8 * exceptionClassNameUTF = J9ROMCLASS_CLASSNAME(exceptionClass->romClass);
		U_16 exceptionClassNameLength = J9UTF8_LENGTH(exceptionClassNameUTF);
		U_8 * exceptionClassName = J9UTF8_DATA(exceptionClassNameUTF);

		UDATA msgLen = j9str_printf(PORTLIB, NULL, 0, nlsMessage,
				exceptionClassNameLength, exceptionClassName,
				methodNameLength, methodName,
				signatureLength, signature,
				loader1ClassNameLength, loader1ClassName, loader1Hash,
				class1ClassNameLength, class1ClassName,
				loader2ClassNameLength, loader2ClassName, loader2Hash,
				class2ClassNameLength, class2ClassName

		);
		msg = j9mem_allocate_memory(msgLen, OMRMEM_CATEGORY_VM);
		/* msg NULL check omitted since str_printf accepts NULL (as above) */
		j9str_printf(PORTLIB, msg, msgLen, nlsMessage,
				exceptionClassNameLength, exceptionClassName,
				methodNameLength, methodName,
				signatureLength, signature,
				 loader1ClassNameLength, loader1ClassName, loader1Hash,
				 class1ClassNameLength, class1ClassName,
				 loader2ClassNameLength, loader2ClassName, loader2Hash,
				 class2ClassNameLength, class2ClassName
			);
	}

	setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, msg);
	j9mem_free_memory(msg);
}


void  
setClassLoadingConstraintOverrideError(J9VMThread *currentThread, J9UTF8 *newClassNameUTF, J9ClassLoader *loader1, J9UTF8 *class1NameUTF, J9ClassLoader *loader2, J9UTF8 *class2NameUTF, J9UTF8 *exceptionClassNameUTF, U_8 *methodName, UDATA methodNameLength, U_8 *signature, UDATA signatureLength)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	char * msg = NULL;
	const char * nlsMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_LOADING_CONSTRAINT_OVERRIDE_VIOLATION, NULL);
	if (nlsMessage != NULL) {
		/* Loader1 name and length */
		j9object_t loader1Object = loader1->classLoaderObject;
		J9Class * loader1Class = J9OBJECT_CLAZZ(currentThread, loader1Object);
		J9UTF8 * loader1ClassNameUTF = J9ROMCLASS_CLASSNAME(loader1Class->romClass);
		U_16 loader1ClassNameLength = J9UTF8_LENGTH(loader1ClassNameUTF);
		U_8 * loader1ClassName = J9UTF8_DATA(loader1ClassNameUTF);
		I_32 loader1Hash = objectHashCode(currentThread->javaVM, loader1Object);

		/* Loader2 name and length */
		j9object_t loader2Object = loader2->classLoaderObject;
		J9Class * loader2Class = J9OBJECT_CLAZZ(currentThread, loader2Object);
		J9UTF8 * loader2ClassNameUTF = J9ROMCLASS_CLASSNAME(loader2Class->romClass);
		U_16 loader2ClassNameLength = J9UTF8_LENGTH(loader2ClassNameUTF);
		U_8 * loader2ClassName = J9UTF8_DATA(loader2ClassNameUTF);
		I_32 loader2Hash = objectHashCode(currentThread->javaVM, loader2Object);

		/* Class1 name and length */
		U_16 class1ClassNameLength = J9UTF8_LENGTH(class1NameUTF);
		U_8 * class1ClassName = J9UTF8_DATA(class1NameUTF);

		/* Class2 name and length */
		U_16 class2ClassNameLength = J9UTF8_LENGTH(class2NameUTF);
		U_8 * class2ClassName = J9UTF8_DATA(class2NameUTF);

		/* ExceptionClass name and length */
		U_16 exceptionClassNameLength = J9UTF8_LENGTH(exceptionClassNameUTF);
		U_8 * exceptionClassName = J9UTF8_DATA(exceptionClassNameUTF);

		/* New class name and length */
		U_16 newClassNameLength = J9UTF8_LENGTH(newClassNameUTF);
		U_8 * newClassName = J9UTF8_DATA(newClassNameUTF);

		UDATA msgLen = j9str_printf(PORTLIB, NULL, 0, nlsMessage,
				exceptionClassNameLength, exceptionClassName,
				methodNameLength, methodName,
				signatureLength, signature,
				loader1ClassNameLength, loader1ClassName, loader1Hash,
				class1ClassNameLength, class1ClassName,
				loader2ClassNameLength, loader2ClassName, loader2Hash,
				class2ClassNameLength, class2ClassName,
				newClassNameLength, newClassName

		);
		msg = j9mem_allocate_memory(msgLen, OMRMEM_CATEGORY_VM);
		/* msg NULL check omitted since str_printf accepts NULL (as above) */
		j9str_printf(PORTLIB, msg, msgLen, nlsMessage,
				exceptionClassNameLength, exceptionClassName,
				methodNameLength, methodName,
				signatureLength, signature,
				 loader1ClassNameLength, loader1ClassName, loader1Hash,
				 class1ClassNameLength, class1ClassName,
				 loader2ClassNameLength, loader2ClassName, loader2Hash,
				 class2ClassNameLength, class2ClassName
			);
	}

	setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, msg);
	j9mem_free_memory(msg);
}



static char *
nlsMessageForMethod(J9VMThread * currentThread, J9Method * method, U_32 module_name, U_32 message_num)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	char * msg = NULL;
	const char * nlsMessage = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(OMRPORT_FROM_J9PORT(PORTLIB), J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, module_name, message_num, NULL);
	if (nlsMessage != NULL) {
		J9Class * methodClass = J9_CLASS_FROM_METHOD(method);
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 * classNameUTF = J9ROMCLASS_CLASSNAME(methodClass->romClass);
		J9UTF8 * methodNameUTF = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 * methodSignatureUTF = J9ROMMETHOD_SIGNATURE(romMethod);
		U_16 classNameLength = J9UTF8_LENGTH(classNameUTF);
		U_8 * className = J9UTF8_DATA(classNameUTF);
		U_16 methodNameLength = J9UTF8_LENGTH(methodNameUTF);
		U_8 * methodName = J9UTF8_DATA(methodNameUTF);
		U_16 methodSignatureLength = J9UTF8_LENGTH(methodSignatureUTF);
		U_8 * methodSignature = J9UTF8_DATA(methodSignatureUTF);
		UDATA msgLen = j9str_printf(PORTLIB, NULL, 0, nlsMessage,
					classNameLength, className,
					methodNameLength, methodName,
					methodSignatureLength, methodSignature);
		msg = j9mem_allocate_memory(msgLen, OMRMEM_CATEGORY_VM);
		/* msg NULL check omitted since str_printf accepts NULL (as above) */
		j9str_printf(PORTLIB, msg, msgLen, nlsMessage,
					classNameLength, className,
					methodNameLength, methodName,
					methodSignatureLength, methodSignature);
	}

	return msg;
}


void  
setNativeBindOutOfMemoryError(J9VMThread * currentThread, J9Method * method)
{
	char * msg = nlsMessageForMethod(currentThread, method, J9NLS_VM_BIND_OUT_OF_MEMORY);
	PORT_ACCESS_FROM_VMC(currentThread);

	setCurrentExceptionUTF(currentThread, J9_EX_OOM_OS_HEAP | J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR, msg == NULL ? NATIVE_MEMORY_OOM_MSG : msg);
	j9mem_free_memory(msg);
}


void  
setRecursiveBindError(J9VMThread * currentThread, J9Method * method)
{
	char * msg = nlsMessageForMethod(currentThread, method, J9NLS_VM_RECURSIVE_BIND);
	PORT_ACCESS_FROM_VMC(currentThread);

	setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGUNSATISFIEDLINKERROR, msg);
	j9mem_free_memory(msg);
}


void  
setNativeNotFoundError(J9VMThread * currentThread, J9Method * method)
{
	setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGUNSATISFIEDLINKERROR, (UDATA*)methodToString(currentThread, method));
}


void  
setHeapOutOfMemoryError(J9VMThread * currentThread)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	j9object_t * heapOOMStringRef = currentThread->javaVM->heapOOMStringRef;
	const char * nlsMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_HEAP_OUT_OF_MEMORY, "Java heap space");

	setCurrentExceptionWithUtfCause(currentThread, J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR, (UDATA*)((NULL == heapOOMStringRef) ? NULL : *heapOOMStringRef), nlsMessage, NULL);
}


void  
setArrayIndexOutOfBoundsException(J9VMThread * currentThread, IDATA index)
{
	setCurrentException(currentThread, J9_EX_CTOR_INT + J9VMCONSTANTPOOL_JAVALANGARRAYINDEXOUTOFBOUNDSEXCEPTION, (UDATA*)(UDATA)index);
}


void  
setNativeOutOfMemoryError(J9VMThread * vmThread, U_32 moduleName, U_32 messageNumber)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	const char * msg;

	/* If no custom NLS message was specified, use the generic one */

	if ((moduleName == 0) && (messageNumber == 0)) {
		moduleName = J9NLS_VM_NATIVE_OOM__MODULE;
		messageNumber = J9NLS_VM_NATIVE_OOM__ID;
	}

	msg = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(OMRPORT_FROM_J9PORT(PORTLIB), J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, moduleName, messageNumber, NATIVE_MEMORY_OOM_MSG);

	setCurrentExceptionUTF(vmThread, J9_EX_OOM_OS_HEAP | J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR, msg);
}

void  
setIncompatibleClassChangeErrorForDefaultConflict(J9VMThread * vmThread, J9Method *method)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	char * msg = NULL;
	/* J9NLS_VM_DEFAULT_METHOD_CONFLICT=class %2$.*1$s has conflicting defaults for method %4$.*3$s%6$.*5$s */
	const char * nlsMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_DEFAULT_METHOD_CONFLICT, NULL);
	if (nlsMessage != NULL) {
		/* Fetch defining class using constantPool*/
		J9Class * currentClass = J9_CLASS_FROM_METHOD(method);
		/* Fetch ROMMethod as bytecodes at one of default methods */
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 * methodNameUTF = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 * methodSignatureUTF = J9ROMMETHOD_SIGNATURE( romMethod);
		J9UTF8 * classNameUTF = J9ROMCLASS_CLASSNAME(currentClass->romClass);
		U_16 classNameLength = J9UTF8_LENGTH(classNameUTF);
		U_8 * className = J9UTF8_DATA(classNameUTF);
		U_16 methodNameLength = J9UTF8_LENGTH(methodNameUTF);
		U_8 * methodName = J9UTF8_DATA(methodNameUTF);
		U_16 methodSignatureLength = J9UTF8_LENGTH(methodSignatureUTF);
		U_8 * methodSignature = J9UTF8_DATA(methodSignatureUTF);
		UDATA msgLen = j9str_printf(PORTLIB, NULL, 0, nlsMessage,
					classNameLength, className,
					methodNameLength, methodName,
					methodSignatureLength, methodSignature);
		msg = j9mem_allocate_memory(msgLen, OMRMEM_CATEGORY_VM);
		/* msg NULL check omitted since str_printf accepts NULL (as above) */
		j9str_printf(PORTLIB, msg, msgLen, nlsMessage,
					classNameLength, className,
					methodNameLength, methodName,
					methodSignatureLength, methodSignature);
	}

	setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, msg);
	j9mem_free_memory(msg);
}

void  
setIllegalAccessErrorNonPublicInvokeInterface(J9VMThread *vmThread, J9Method *method)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	char * msg = NULL;
	/* J9NLS_VM_INVOKEINTERFACE_OF_NONPUBLIC_METHOD=invokeinterface of non-public method '%4$.*3$s%6$.*5$s' in %2$.*1$s */
	const char * nlsMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_INVOKEINTERFACE_OF_NONPUBLIC_METHOD, NULL);
	if (nlsMessage != NULL) {
		J9Class *clazz = J9_CLASS_FROM_METHOD(method);
		J9UTF8 *classNameUTF = J9ROMCLASS_CLASSNAME(clazz->romClass);
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9ROMNameAndSignature *nameAndSig = &(romMethod->nameAndSignature);
		J9UTF8 *methodNameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
		J9UTF8 *methodSigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);

		U_16 classNameLength = J9UTF8_LENGTH(classNameUTF);
		U_8 * className = J9UTF8_DATA(classNameUTF);
		U_16 methodNameLength = J9UTF8_LENGTH(methodNameUTF);
		U_8 * methodName = J9UTF8_DATA(methodNameUTF);
		U_16 methodSignatureLength = J9UTF8_LENGTH(methodSigUTF);
		U_8 * methodSignature = J9UTF8_DATA(methodSigUTF);
		UDATA msgLen = j9str_printf(PORTLIB, NULL, 0, nlsMessage,
				classNameLength, className,
				methodNameLength, methodName,
				methodSignatureLength, methodSignature);
		msg = j9mem_allocate_memory(msgLen, OMRMEM_CATEGORY_VM);
		/* msg NULL check omitted since str_printf accepts NULL (as above) */
		j9str_printf(PORTLIB, msg, msgLen,nlsMessage,
				classNameLength, className,
				methodNameLength, methodName,
				methodSignatureLength, methodSignature);
	}

	setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, msg);
	j9mem_free_memory(msg);
}

void  
setThreadForkOutOfMemoryError(J9VMThread * vmThread, U_32 moduleName, U_32 messageNumber)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	const char * msg;

	msg = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(OMRPORT_FROM_J9PORT(PORTLIB), J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, moduleName, messageNumber, NATIVE_MEMORY_OOM_MSG);

	setCurrentExceptionUTF(vmThread, J9_EX_OOM_THREAD | J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR, msg);
}


jint  
initializeHeapOOMMessage(J9VMThread *currentThread)
{
	J9JavaVM * vm = currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9MemoryManagerFunctions * gcfns = vm->memoryManagerFunctions;
	jint result = JNI_ENOMEM;
	const char * nlsMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_VM_HEAP_OUT_OF_MEMORY, "Java heap space");
	j9object_t message = gcfns->j9gc_createJavaLangString(currentThread, (U_8*)nlsMessage, (U_32) strlen(nlsMessage), J9_STR_TENURE);

	if (NULL != message) {
		jobject globalRef = j9jni_createGlobalRef((JNIEnv*)currentThread, message, JNI_FALSE);

		if (NULL != globalRef) {
			vm->heapOOMStringRef = (j9object_t*)globalRef;
			result = JNI_OK;			
		}
	}
	return result;
}


void  
setIllegalAccessErrorFinalFieldSet(J9VMThread *currentThread, UDATA isStatic, J9ROMClass *romClass, J9ROMFieldShape *field, J9ROMMethod *romMethod)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	char * msg = NULL;
	/* J9NLS_VM_ILLEGAL_ACCESS_STATIC_FINAL=Attempt to set static final field %2$.*1$s.%4$.*3$s from method "%6$.*5$s" that is not <clinit> */
	const char * nlsMessage = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(
			OMRPORT_FROM_J9PORT(PORTLIB),
			J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
			J9NLS_VM_ILLEGAL_ACCESS_STATIC_FINAL__MODULE,
			isStatic ? J9NLS_VM_ILLEGAL_ACCESS_STATIC_FINAL__ID : J9NLS_VM_ILLEGAL_ACCESS_NONSTATIC_FINAL__ID,
			NULL);

	if (nlsMessage != NULL) {
		J9UTF8 *classNameUTF = J9ROMCLASS_CLASSNAME(romClass);
		J9UTF8 *fieldNameUTF = J9ROMFIELDSHAPE_NAME(field);
		J9UTF8 *methodNameUTF = J9ROMMETHOD_NAME(romMethod);
		U_16 classNameLength = J9UTF8_LENGTH(classNameUTF);
		U_8 * className = J9UTF8_DATA(classNameUTF);
		U_16 fieldNameLength = J9UTF8_LENGTH(fieldNameUTF);
		U_8 * fieldName = J9UTF8_DATA(fieldNameUTF);
		U_16 methodNameLength = J9UTF8_LENGTH(methodNameUTF);
		U_8 * methodName = J9UTF8_DATA(methodNameUTF);
		UDATA msgLen = j9str_printf(PORTLIB, NULL, 0, nlsMessage,
				classNameLength, className,
				fieldNameLength, fieldName,
				methodNameLength, methodName);
		msg = j9mem_allocate_memory(msgLen, OMRMEM_CATEGORY_VM);
		/* msg NULL check omitted since str_printf accepts NULL (as above) */
		j9str_printf(PORTLIB, msg, msgLen,nlsMessage,
				classNameLength, className,
				fieldNameLength, fieldName,
				methodNameLength, methodName);
	}

	setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, msg);
	j9mem_free_memory(msg);
}
