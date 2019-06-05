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
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j9cp.h"
#include "rommeth.h"
#include "jilconsts.h"
#include "jitprotos.h"
#include "j9protos.h"
#include "stackwalk.h"
#include "MethodMetaData.h"
#include "pcstack.h"
#include "ut_j9codertvm.h"
#include "jitregmap.h"
#include "pcstack.h"
#include "VMHelpers.hpp"

extern "C" {

/* Generic rounding macro - result is a UDATA */
#define ROUND_TO(granularity, number) (((UDATA)(number) + (granularity) - 1) & ~((UDATA)(granularity) - 1))

typedef struct {
	J9JITExceptionTable * metaData;
	J9Method * method;
	UDATA * bp;
	UDATA * a0;
	UDATA * unwindSP;
	UDATA * sp;
	UDATA argCount;
	J9Method * literals;
	J9I2JState i2jState;
	UDATA * j2iFrame;
	UDATA preservedRegisterValues[J9SW_JIT_CALLEE_PRESERVED_SIZE];
	UDATA previousFrameBytecodes;
	UDATA notifyFramePop;
	UDATA resolveFrameFlags;
} J9JITDecompileState;

/* OSR result codes */
#define OSR_OK				0
#define OSR_OUT_OF_MEMORY	1

/* Bit values for J9OSRFrame->flags */
#define J9OSRFRAME_NOTIFY_FRAME_POP 1

typedef struct {
	J9VMThread *targetThread;
	J9JITExceptionTable *metaData;
	void *jitPC;
	UDATA resolveFrameFlags;
	UDATA *objectArgScanCursor;
	UDATA *objectTempScanCursor;
	J9JITStackAtlas *gcStackAtlas;
	J9Method *method;
	U_8 *liveMonitorMap;
	U_16 numberOfMapBits;
	void *inlineMap;
	void *inlinedCallSite;
	J9OSRFrame *osrFrame;
} J9OSRData;

extern void _fsdSwitchToInterpPatchEntry(void *);
extern void _fsdRestoreToJITPatchEntry(void *);

J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreter0);
J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreter1);
J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreterD);
J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreterF);
J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreterJ);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileOnReturn0);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileOnReturn1);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileOnReturnD);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileOnReturnF);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileOnReturnJ);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileOnReturnL);
J9_EXTERN_BUILDER_SYMBOL(jitReportExceptionCatch);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileAtExceptionCatch);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileAtCurrentPC);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileBeforeMethodMonitorEnter);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileBeforeReportMethodEnter);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileAfterAllocation);
J9_EXTERN_BUILDER_SYMBOL(jitDecompileAfterMonitorEnter);
J9_EXTERN_BUILDER_SYMBOL(executeCurrentBytecodeFromJIT);
J9_EXTERN_BUILDER_SYMBOL(enterMethodMonitorFromJIT);
J9_EXTERN_BUILDER_SYMBOL(reportMethodEnterFromJIT);
J9_EXTERN_BUILDER_SYMBOL(handlePopFramesFromJIT);

static J9OSRFrame* findOSRFrameAtInlineDepth(J9OSRBuffer *osrBuffer, UDATA inlineDepth);
static void   jitFramePopNotificationAdded(J9VMThread * currentThread, J9StackWalkState * walkState, UDATA inlineDepth);
static void decompPrintMethod(J9VMThread * currentThread, J9Method * method);
static UDATA decompileAllFrameIterator(J9VMThread * currentThread, J9StackWalkState * walkState);
static J9JITDecompilationInfo * deleteDecompilationForExistingFrame(J9VMThread * decompileThread, J9JITDecompilationInfo * info);
static J9JITDecompilationInfo * addDecompilation(J9VMThread * currentThread, J9StackWalkState * walkState, UDATA reason);
static void removeAllBreakpoints(J9VMThread * currentThread);
static void buildBytecodeFrame(J9VMThread *currentThread, J9OSRFrame *osrFrame);
static void buildInlineStackFrames(J9VMThread *currentThread, J9JITDecompileState *decompileState, J9JITDecompilationInfo *decompRecord, UDATA inlineDepth, J9OSRFrame *osrFrame);
static void performDecompile(J9VMThread * currentThread, J9JITDecompileState * decompileState, J9JITDecompilationInfo * decompRecord, J9OSRFrame *osrFrame, UDATA numberOfFrames);
static void decompileOuterFrame(J9VMThread * currentThread, J9JITDecompileState * decompileState, J9JITDecompilationInfo * decompRecord, J9OSRFrame *osrFrame);
static void markMethodBreakpointed(J9VMThread * currentThread, J9JITBreakpointedMethod * breakpointedMethod);
static UDATA codeBreakpointAddedFrameIterator(J9VMThread * currentThread, J9StackWalkState * walkState);
static void  decompileAllMethodsInAllStacks(J9VMThread * currentThread, UDATA reason);
static void markMethodUnbreakpointed(J9VMThread * currentThread, J9JITBreakpointedMethod * breakpointedMethod);
static void reinstallAllBreakpoints(J9VMThread * currentThread);
static UDATA decompileMethodFrameIterator(J9VMThread * currentThread, J9StackWalkState * walkState);
static void deleteAllDecompilations(J9VMThread * currentThread, UDATA reason, J9Method * method);
static void freeDecompilationRecord(J9VMThread * decompileThread, J9JITDecompilationInfo * info, UDATA retain);
static UDATA osrAllFramesSize(J9VMThread *currentThread, J9JITExceptionTable *metaData, void *jitPC, UDATA resolveFrameFlags);
static UDATA performOSR(J9VMThread *currentThread, J9StackWalkState *walkState, J9OSRBuffer *osrBuffer, U_8 *osrScratchBuffer, UDATA scratchBufferSize, UDATA jitStackFrameSize, UDATA *mustDecompile);
static UDATA* jitLocalSlotAddress(J9VMThread * currentThread, J9StackWalkState *walkState, UDATA slot, UDATA inlineDepth);
static void jitResetAllMethods(J9VMThread *currentThread);
static void fixStackForNewDecompilation(J9VMThread * currentThread, J9StackWalkState * walkState, J9JITDecompilationInfo *info, UDATA reason, J9JITDecompilationInfo **link);
static UDATA roundedOSRScratchBufferSize(J9VMThread * currentThread, J9JITExceptionTable *metaData, void *jitPC);
static j9object_t* getObjectSlotAddress(J9OSRData *osrData, U_16 slot);
static UDATA createMonitorEnterRecords(J9VMThread *currentThread, J9OSRData *osrData);
static UDATA initializeOSRFrame(J9VMThread *currentThread, J9OSRData *osrData);
static UDATA initializeOSRBuffer(J9VMThread *currentThread, J9OSRBuffer *osrBuffer, J9OSRData *osrData);
static UDATA getPendingStackHeight(J9VMThread *currentThread, U_8 *interpreterPC, J9Method *ramMethod, UDATA resolveFrameFlags);
static J9JITDecompilationInfo* jitAddDecompilationForFramePop(J9VMThread * currentThread, J9StackWalkState * walkState);
static J9JITDecompilationInfo* fetchAndUnstackDecompilationInfo(J9VMThread *currentThread);
static void fixSavedPC(J9VMThread *currentThread, J9JITDecompilationInfo *decompRecord);
static void dumpStack(J9VMThread *currentThread, char const *msg);
static J9ROMNameAndSignature* getNASFromInvoke(U_8 *bytecodePC, J9ROMClass *romClass);


/**
 * Get the J9ROMNameAndSignature for an invoke bytecode.
 *
 * @param[in] bytecodePC the PC of the invoke
 * @param[in] romClass the J9ROMClass containing the invoke bytecode
 *
 * @return the J9ROMNameAndSignature referenced by the invoke
 */
static J9ROMNameAndSignature*
getNASFromInvoke(U_8 *bytecodePC, J9ROMClass *romClass)
{
	U_16 cpIndex = *(U_16*)(bytecodePC + 1);
	J9ROMNameAndSignature* nameAndSig = NULL;
	U_8 bytecode = *bytecodePC;
	if (JBinvokedynamic == bytecode) {
		/* Convert from callSites table index to actual cpIndex by probing the
		 * J9ROMClass->callSiteData table. That data is layed out as:
		 * callSiteCount x SRP to: J9ROMNameAndSignature, callSiteCount x U16, bsmCount x J9BSMData
		 */
		J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
		nameAndSig = SRP_PTR_GET(callSiteData + cpIndex, J9ROMNameAndSignature*);
	} else {
		switch(bytecode) {
		case JBinvokeinterface2:
			cpIndex = *(U_16*)(bytecodePC + 3);
			break;
		case JBinvokestaticsplit:
			cpIndex = *(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + cpIndex);
			break;
		case JBinvokespecialsplit:
			cpIndex = *(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + cpIndex);
			break;
		}
		J9ROMMethodRef * romMethodRef = ((J9ROMMethodRef *) &(J9_ROM_CP_FROM_ROM_CLASS(romClass)[cpIndex]));
		nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
	}
	return nameAndSig;
}


/**
 * Pop the top of the decompilation stack.
 *
 * @param[in] currentThread the current J9VMThread
 *
 * @return the top element from the decompilation stack
 */
static J9JITDecompilationInfo*
fetchAndUnstackDecompilationInfo(J9VMThread *currentThread)
{
	J9JITDecompilationInfo *decompRecord = currentThread->decompilationStack;
	currentThread->decompilationStack = decompRecord->next;
	return decompRecord;
}


/**
 * Restore the PC from a decompilation record to its original location.
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] decompRecord the decompilation record
 */
static void
fixSavedPC(J9VMThread *currentThread, J9JITDecompilationInfo *decompRecord)
{
	*decompRecord->pcAddress = decompRecord->pc;
}

/**
 * If the verbose stack dumper is enabled, dump the stack of the current thread.
 *
 * @param[in] currentThread the current J9VMThread
 * @param[in] msg the header message for the stack dump
 */
static void
dumpStack(J9VMThread *currentThread, char const *msg)
{
	J9JavaVM *vm = currentThread->javaVM;
	if (NULL != vm->verboseStackDump) {
		vm->verboseStackDump(currentThread, msg);
	}
}


static void
decompPrintMethod(J9VMThread * currentThread, J9Method * method) 
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9UTF8 * className = J9ROMCLASS_CLASSNAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass);
	J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	J9UTF8 * name = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
	J9UTF8 * sig = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);

	Trc_Decomp_printMethod(currentThread, method, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));
}


static void
jitResetAllMethods(J9VMThread *currentThread)
{
	J9JavaVM * vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9Class * clazz = NULL;
	J9ClassWalkState state;

	/* First mark every compiled method for retranslation and invalidate the translation */

	clazz = vmFuncs->allClassesStartDo(&state, vm, NULL);
	while (clazz != NULL) {
		J9Method *method = clazz->ramMethods;
		U_32 methodCount = clazz->romClass->romMethodCount;

		while (methodCount != 0) {
			if (0 == ((UDATA)method->extra & J9_STARTPC_NOT_TRANSLATED)) {
				/* Do not reset JIT INLs (currently in FSD there are no compiled JNI natives) */
				if (0 == (J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers & J9AccNative)) {
					J9JITExceptionTable *metaData = vm->jitConfig->jitGetExceptionTableFromPC(currentThread, (UDATA) method->extra);
					if (NULL != metaData) {
						/* 0xCC is the "int3" instruction on x86.
						 * This will cause a crash if this instruction is executed.
						 * On other platforms, this will have unknown behaviour (likely a crash).
						 */
						*(U_8*)method->extra = 0xCC;
					}
					vmFuncs->initializeMethodRunAddress(currentThread, method);
				}
			}
			method += 1;
			methodCount -= 1;
		}
		clazz = vmFuncs->allClassesNextDo(&state);
	}
	vmFuncs->allClassesEndDo(&state);

	/* Now fix all the JIT vTables */

	clazz = vmFuncs->allClassesStartDo(&state, vm, NULL);
	while (clazz != NULL) {
		/* Interface classes do not have vTables, so skip them */
		if (!J9ROMCLASS_IS_INTERFACE(clazz->romClass)) {
			UDATA *vTableWriteCursor = JIT_VTABLE_START_ADDRESS(clazz);

			J9VTableHeader *vTableHeader = J9VTABLE_HEADER_FROM_RAM_CLASS(clazz);
			J9Method **vTableReadCursor = J9VTABLE_FROM_HEADER(vTableHeader);
			UDATA vTableSize = vTableHeader->size;

			/* Put invalid entries in the obsolete JIT vTables and reinitialize the current ones */
			if (J9_IS_CLASS_OBSOLETE(clazz)) {
				while (0 != vTableSize) {
					*vTableWriteCursor = (UDATA)-1;
					vTableWriteCursor -= 1;
					vTableSize -= 1;
				}
			} else {
				while (0 != vTableSize) {
					J9Method *method = *vTableReadCursor;
					vTableReadCursor += 1;
					vmFuncs->fillJITVTableSlot(currentThread, vTableWriteCursor, method);
					vTableWriteCursor -= 1;
					vTableSize -= 1;
				}
			}
		}
		clazz = vmFuncs->allClassesNextDo(&state);
	}
	vmFuncs->allClassesEndDo(&state);
}



#if (defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)) /* priv. proto (autogen) */

J9JITDecompilationInfo *
jitCleanUpDecompilationStack(J9VMThread * currentThread, J9StackWalkState * walkState, UDATA dropCurrentFrame)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9JITDecompilationInfo * current;
	J9JITDecompilationInfo * currentFrameDecompile = NULL;

	current = currentThread->decompilationStack;
	while (current != walkState->decompilationStack) {
		J9JITDecompilationInfo * temp = current;

		if (!dropCurrentFrame) {
			if (current->bp == walkState->bp) {
				currentFrameDecompile = current;
				break;
			}
		}
		current = current->next;
		freeDecompilationRecord(currentThread, temp, FALSE);
	}
	currentThread->decompilationStack = current;

	return currentFrameDecompile;
}

#endif /* J9VM_INTERP_HOT_CODE_REPLACEMENT (autogen) */


void  
jitCodeBreakpointAdded(J9VMThread * currentThread, J9Method * method)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9JITConfig * jitConfig = currentThread->javaVM->jitConfig;
	J9JITBreakpointedMethod * breakpointedMethod;
	J9JITBreakpointedMethod * breakpointedMethods = jitConfig->breakpointedMethods;
	J9VMThread * loopThread;
	
	/* Called under exclusive access, so no mutex required */

	Trc_Decomp_jitCodeBreakpointAdded_Entry(currentThread, method);
	decompPrintMethod(currentThread, method);

	breakpointedMethod = breakpointedMethods;
	while (breakpointedMethod) {
		if (breakpointedMethod->method == method) {
			++(breakpointedMethod->count);
			Trc_Decomp_jitCodeBreakpointAdded_incCount(currentThread, breakpointedMethod->count);
			return;
		}
		breakpointedMethod = breakpointedMethod->link;
	}

	Trc_Decomp_jitCodeBreakpointAdded_newEntry(currentThread);

	breakpointedMethod = (J9JITBreakpointedMethod *) j9mem_allocate_memory(sizeof(J9JITBreakpointedMethod), OMRMEM_CATEGORY_JIT);
	if (!breakpointedMethod) {
		j9tty_printf(PORTLIB, "\n*** alloc failure in jitPermanentBreakpointAdded ***\n");
		Assert_Decomp_breakpointFailed();
	}

	breakpointedMethod->link = breakpointedMethods;
	jitConfig->breakpointedMethods = breakpointedMethod;

	breakpointedMethod->method = method;
	breakpointedMethod->count = 1;
	markMethodBreakpointed(currentThread, breakpointedMethod);

	Trc_Decomp_jitCodeBreakpointAdded_hasBeenTranslated(currentThread, breakpointedMethod->hasBeenTranslated);

	loopThread = currentThread;
	do {
		J9StackWalkState walkState;

		walkState.userData1 = method;
		walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_MAINTAIN_REGISTER_MAP;
		walkState.skipCount = 0;
		walkState.frameWalkFunction = codeBreakpointAddedFrameIterator;
		walkState.walkThread = loopThread;
		currentThread->javaVM->walkStackFrames(currentThread, &walkState);
	} while ((loopThread = loopThread->linkNext) != currentThread);	

	Trc_Decomp_jitCodeBreakpointAdded_Exit(currentThread);
}


void  
jitCodeBreakpointRemoved(J9VMThread * currentThread, J9Method * method)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9JITConfig * jitConfig = currentThread->javaVM->jitConfig;
	J9JITBreakpointedMethod ** previous = &(jitConfig->breakpointedMethods);
	J9JITBreakpointedMethod * breakpointedMethod;

	/* Called under exclusive access, so no mutex required */

	Trc_Decomp_jitCodeBreakpointRemoved_Entry(currentThread, method);
	decompPrintMethod(currentThread, method);

	while ((breakpointedMethod = *previous) != NULL) {
		if (breakpointedMethod->method == method) {
			UDATA count;

			if ( (count = --(breakpointedMethod->count)) == 0) {
				Trc_Decomp_jitCodeBreakpointAdded_fixingMethods(currentThread);
				markMethodUnbreakpointed(currentThread, breakpointedMethod);
				*previous = breakpointedMethod->link;
				j9mem_free_memory(breakpointedMethod);
				deleteAllDecompilations(currentThread, JITDECOMP_CODE_BREAKPOINT, method);
			}

			Trc_Decomp_jitCodeBreakpointRemoved_decCount(currentThread, count);

			return;
		}
		previous = &(breakpointedMethod->link);
	}

	Trc_Decomp_jitCodeBreakpointAdded_Failed(currentThread);
}


static UDATA
codeBreakpointAddedFrameIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	/* Decompile JIT frames running the breakpointed method */

	if (walkState->jitInfo && (walkState->method == (J9Method *) walkState->userData1)) {
		addDecompilation(currentThread, walkState, JITDECOMP_CODE_BREAKPOINT);
	}

	return J9_STACKWALK_KEEP_ITERATING;
}


#if (defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)) /* priv. proto (autogen) */

void  
jitHotswapOccurred(J9VMThread * currentThread)
{
	/* We have exclusive */

	Trc_Decomp_jitHotswapOccurred_Entry(currentThread);

	/* Remove all breakpoints before resetting the methods */

	removeAllBreakpoints(currentThread);

	/* Find every method which has been translated and mark it for retranslation */

	jitResetAllMethods(currentThread);

	/* Reinstall the breakpoints */

	reinstallAllBreakpoints(currentThread);

	/* Mark every JIT method in every stack for decompilation */

	decompileAllMethodsInAllStacks(currentThread, JITDECOMP_HOTSWAP);

	Trc_Decomp_jitHotswapOccurred_Exit(currentThread);
}

#endif /* J9VM_INTERP_HOT_CODE_REPLACEMENT (autogen) */


void  
jitDecompileMethod(J9VMThread * currentThread, J9JITDecompilationInfo * decompRecord)
{
	J9JITDecompileState decompileState;
	J9StackWalkState walkState;
	J9OSRBuffer *osrBuffer = &decompRecord->osrBuffer;
	UDATA numberOfFrames = osrBuffer->numberOfFrames;
	J9OSRFrame *osrFrame = (J9OSRFrame*)(osrBuffer + 1);

	/* Collect the required information from the stack - top visible frame is the decompile frame */
	walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_MAINTAIN_REGISTER_MAP | J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_SAVE_STACKED_REGISTERS;
	walkState.skipCount = 0;
	walkState.frameWalkFunction = decompileMethodFrameIterator;
	walkState.walkThread = currentThread;
	walkState.userData1 = &decompileState;
	walkState.userData2 = NULL;
	currentThread->javaVM->walkStackFrames(currentThread, &walkState);

	performDecompile(currentThread, &decompileState, decompRecord, osrFrame, numberOfFrames);

	freeDecompilationRecord(currentThread, decompRecord, TRUE);
}

/**
 * Find the pending stack height for a frame.
 *
 * @param[in] *currentThread current thread
 * @param[in] *interpreterP the bytecoded PC
 * @param[in] *method the J9Method
 * @param[in] resolveFrameFlags the flags of the preceding resolve frame, or 0 if not preceded by a resolve frame
 *
 * @return the pending stack height
 */

static UDATA
getPendingStackHeight(J9VMThread *currentThread, U_8 *interpreterPC, J9Method *ramMethod, UDATA resolveFrameFlags)
{
	UDATA pendingStackHeight = 0;

	/* If the JIT frame has not been built, the pending stack height is 0.  If we're at an exception catch,
	 * the height is 0 because the decompile return point will push the pending exception.
	 */
	UDATA resolveFrameType = resolveFrameFlags & J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK;
	switch(resolveFrameType) {
	case J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME:
	case J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE:
		break;
	default:
		J9ROMClass *romClass = J9_CLASS_FROM_METHOD(ramMethod)->romClass;
		J9ROMMethod * originalROMMethod = getOriginalROMMethod(ramMethod);
		UDATA offsetPC = interpreterPC - J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod);
		J9JavaVM *vm = currentThread->javaVM;
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
		J9PortLibrary *portLibrary = vm->portLibrary;
		U_8 bytecode = *interpreterPC;
		/* Use the stack mapper to determine the pending stack height */
		pendingStackHeight = (UDATA)vmFuncs->j9stackmap_StackBitsForPC(portLibrary,	offsetPC, romClass, originalROMMethod, NULL, 0, NULL, NULL, NULL);
		/* All invokes consider their arguments not to be pending - remove the arguments from the pending stack */
		switch(bytecode) {
		case JBinvokevirtual:
		case JBinvokespecial:
		case JBinvokespecialsplit:
		case JBinvokeinterface:
		case JBinvokeinterface2:
		case JBinvokehandle:
		case JBinvokehandlegeneric:
			/* Remove implicit receiver from pending stack */
			pendingStackHeight -= 1;
			/* Intentional fall-through */
		case JBinvokedynamic:
		case JBinvokestatic:
		case JBinvokestaticsplit:
			/* Remove arguments from pending stack */
			pendingStackHeight -= getSendSlotsFromSignature(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(getNASFromInvoke(interpreterPC, romClass))));
			break;
		}
		/* Reduce the pending stack height for the resolve special cases */
		switch (resolveFrameType) {
		case J9_STACK_FLAGS_JIT_MONITOR_ENTER_RESOLVE:
			/* Decompile after monitorenter completes.  Remove the object from the pending stack. */
			pendingStackHeight -= 1;
			break;
		case J9_STACK_FLAGS_JIT_ALLOCATION_RESOLVE:
			/* Decompile after allocation completes.  The JIT has not mapped the bytecode parameters in the pending stack */
			switch (bytecode) {
			case JBanewarray:
			case JBnewarray: /* 1 slot (size) stacked */
				pendingStackHeight -= 1;
				break;
			case JBmultianewarray: /* Dimensions stacked (number of dimensions is 3 bytes from the multianewarray) */
				pendingStackHeight -= interpreterPC[3];
				break;
			default: /* JBnew - no stacked parameters*/
				break;
			}
		}
	}
	Assert_CodertVM_true((IDATA)pendingStackHeight >= 0);
	Trc_Decomp_decompileMethodFrameIterator_pendingCount(currentThread, pendingStackHeight);
	return pendingStackHeight;
}


static UDATA
decompileMethodFrameIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	J9JITDecompileState * decompileState = (J9JITDecompileState *) walkState->userData1;

	Trc_Decomp_decompileMethodFrameIterator_Entry(currentThread);

	/* If the decompile frame has already been passed, record the final information and stop walking */

	if (walkState->userData2) {
		if (walkState->jitInfo) {
			UDATA * valueCursor = decompileState->preservedRegisterValues;
			UDATA ** mapCursor = (UDATA **) &(walkState->registerEAs);
			UDATA i;

			decompileState->previousFrameBytecodes = FALSE;

			for (i = 0; i < J9SW_JIT_CALLEE_PRESERVED_SIZE; ++i) {
				UDATA regNumber = jitCalleeSavedRegisterList[i];

				*(valueCursor++) = *(mapCursor[regNumber]);
			}

			Trc_Decomp_decompileMethodFrameIterator_previousIsJIT(currentThread);

		} else {
			Trc_Decomp_decompileMethodFrameIterator_previousIsBC(currentThread);
			decompileState->previousFrameBytecodes = TRUE;
		}
		return J9_STACKWALK_STOP_ITERATING;
	} else {
		/* Current frame is the decompile frame - collect the preliminary info */
		J9JITExceptionTable *metaData = walkState->jitInfo;
		J9Method *method = walkState->method;

		decompileState->metaData = metaData;
		decompileState->method = method;
		decompileState->sp = walkState->sp;
		decompileState->argCount = walkState->outgoingArgCount;
		Trc_Decomp_decompileMethodFrameIterator_outgoingArgCount(currentThread, decompileState->argCount);
		decompileState->bp = walkState->bp;
		decompileState->a0 = walkState->arg0EA;
		decompileState->literals = method;
		decompileState->unwindSP = walkState->unwindSP;
		decompileState->j2iFrame = walkState->j2iFrame;
		memcpy(&(decompileState->i2jState), walkState->i2jState, sizeof(J9I2JState));
		decompileState->resolveFrameFlags = walkState->resolveFrameFlags;

		/* Walk the next frame regardless of its visibility */

		walkState->userData2 = (void *) 1;
		walkState->flags &= ~J9_STACKWALK_VISIBLE_ONLY;
	}

	Trc_Decomp_decompileMethodFrameIterator_Exit(currentThread);

	return J9_STACKWALK_KEEP_ITERATING;
}


/**
 * Push a bytecode frame based on an OSR frame.  Assumes that a bytecode frame
 * (either pure or J2I) is already on the top of stack.
 *
 * @param[in] *currentThread current thread
 * @param[in] *osrFrame the OSR frame from which to copy the information
 */
static void
buildBytecodeFrame(J9VMThread *currentThread, J9OSRFrame *osrFrame)
{
	J9Method *method = osrFrame->method;
	U_8 *bytecodePC = osrFrame->bytecodePCOffset + J9_BYTECODE_START_FROM_RAM_METHOD(method);
	UDATA numberOfLocals = osrFrame->numberOfLocals;
	UDATA maxStack = osrFrame->maxStack;
	UDATA pendingStackHeight = osrFrame->pendingStackHeight;
	UDATA *locals = ((UDATA*)(osrFrame + 1)) + maxStack;
	UDATA *pending = locals - pendingStackHeight;
	UDATA *sp = currentThread->sp;
	UDATA *a0 = currentThread->arg0EA;
	U_8 *pc = currentThread->pc;
	J9Method *literals = currentThread->literals;
	UDATA *newA0 = sp - 1;
	J9SFStackFrame *stackFrame = NULL;

	/* Push the locals */
	sp -= numberOfLocals;
	memcpy(sp, locals, numberOfLocals * sizeof(UDATA));

	/* Push the stack frame */
	stackFrame = (J9SFStackFrame*)sp - 1;
	stackFrame->savedPC = pc;
	stackFrame->savedCP = literals;
	stackFrame->savedA0 = a0;

	/* Push the pendings */
	sp = (UDATA*)stackFrame - pendingStackHeight;
	memcpy(sp, pending, pendingStackHeight * sizeof(UDATA));

	/* Update the root values in the J9VMThread */
	currentThread->arg0EA = newA0;
	currentThread->pc = bytecodePC;
	currentThread->literals = method;
	currentThread->sp = sp;
}


/**
 * Recursive helper for decompiling inlined frames.
 *
 * @param[in] *currentThread current thread
 * @param[in] *decompileState the decompilation state copied from the stack walk
 * @param[in] *decompRecord the decompilation record
 * @param[in] inlineDepth the depth of inlining, 0 for the outer frame
 * @param[in] *osrFrame the OSR frame from which to copy the information
 */
static void
buildInlineStackFrames(J9VMThread *currentThread, J9JITDecompileState *decompileState, J9JITDecompilationInfo *decompRecord, UDATA inlineDepth, J9OSRFrame *osrFrame)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9MonitorEnterRecord *firstEnterRecord = osrFrame->monitorEnterRecords;
	J9Method *method = osrFrame->method;
	if (0 == inlineDepth) {
		decompileOuterFrame(currentThread, decompileState, decompRecord, osrFrame);
	} else {
		J9OSRFrame *nextOSRFrame = (J9OSRFrame*)((U_8*)osrFrame + osrFrameSize(method));
		buildInlineStackFrames(currentThread, decompileState, decompRecord, inlineDepth - 1, nextOSRFrame);
		buildBytecodeFrame(currentThread, osrFrame);
	}
	/* Fix all of the monitor enter records for this frame to point to the new arg0EA.
	 * Filter out the record for the syncObject in synchronized methods.  Only one such
	 * record will ever be detected in any frame.  Link the remaining records into the list.
	 */
	if (NULL != firstEnterRecord) {
		J9MonitorEnterRecord listHead;
		J9MonitorEnterRecord *lastEnterRecord = &listHead;
		J9MonitorEnterRecord *enterRecord = firstEnterRecord;
		UDATA *arg0EA = currentThread->arg0EA;
		j9object_t syncObject = NULL;
		if (J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers & J9AccSynchronized) {
			syncObject = *(j9object_t*)(arg0EA - osrFrame->numberOfLocals + 1);
		}
		listHead.next = firstEnterRecord;
		do {
			J9MonitorEnterRecord *nextRecord = enterRecord->next;
			if (enterRecord->object == syncObject) {
				syncObject = NULL; /* Make sure syncObject isn't matched more than once */
				lastEnterRecord->next = nextRecord;
				pool_removeElement(currentThread->monitorEnterRecordPool, enterRecord);
			} else {
				enterRecord->arg0EA = CONVERT_TO_RELATIVE_STACK_OFFSET(currentThread, arg0EA);
				lastEnterRecord = enterRecord;
			}
			enterRecord = nextRecord;
		} while (NULL != enterRecord);
		lastEnterRecord->next = currentThread->monitorEnterRecords;
		currentThread->monitorEnterRecords = listHead.next;
	}

	/* If this frame has a notify frame pop pending, set the bit */

	if (osrFrame->flags & J9OSRFRAME_NOTIFY_FRAME_POP) {
		UDATA *bp = currentThread->arg0EA - osrFrame->numberOfLocals;
		Trc_Decomp_performDecompile_addedPopNotification(currentThread, bp);
		*bp |= J9SF_A0_REPORT_FRAME_POP_TAG;
	}
}


/**
 * Perform decompilation after gather information from the stack walk.
 *
 * @param[in] *currentThread current thread
 * @param[in] *decompileState the decompilation state copied from the stack walk
 * @param[in] *decompRecord the decompilation record
 * @param[in] *osrFrame the first OSR frame to rebuild
 * @param[in] numberOfFrames the total number of frames to rebuild
 */
static void
performDecompile(J9VMThread *currentThread, J9JITDecompileState *decompileState, J9JITDecompilationInfo *decompRecord, J9OSRFrame *osrFrame, UDATA numberOfFrames)
{
	J9JavaVM *vm = currentThread->javaVM;
	UDATA outgoingArgs[255];
	UDATA outgoingArgCount = decompileState->argCount;
	UDATA inlineDepth = numberOfFrames - 1;

	Trc_Decomp_performDecompile_Entry(currentThread);

	dumpStack(currentThread, "before decompilation");

	if (FALSE == decompRecord->usesOSR) {
		/* Not compiled with OSR - copy stack slots from JIT frame into OSR frame */
		UDATA *jitTempBase = ((UDATA *) (((U_8 *) decompileState->bp) + ((J9JITStackAtlas *) decompileState->metaData->gcStackAtlas)->localBaseOffset)) + decompileState->metaData->tempOffset;
		UDATA *osrTempBase = ((UDATA*)(osrFrame + 1)) + osrFrame->maxStack;
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(osrFrame->method);
		UDATA argCount = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);
		UDATA tempCount = osrFrame->numberOfLocals - argCount;
		UDATA pendingStackHeight = osrFrame->pendingStackHeight;

		/* Decompiling without OSR means old-style single-frame FSD */
		Assert_CodertVM_true(vm->jitConfig->fsdEnabled);
		Assert_CodertVM_true(1 == numberOfFrames);

		/* The pending slots immediately precede the temps in both the OSR frame and the JIT frame, so copy them all at once */
		memcpy(osrTempBase - pendingStackHeight, jitTempBase - pendingStackHeight, (tempCount + pendingStackHeight) * sizeof(UDATA));
	}

	/* Temporarily copy the outgoing arguments to the C stack */
	memcpy(outgoingArgs, decompileState->sp, outgoingArgCount * sizeof(UDATA));

	/* Rebuild the interpreter stack frames */
	buildInlineStackFrames(currentThread, decompileState, decompRecord, inlineDepth, osrFrame);

	/* Push the outgoing arguments onto the java stack */
	currentThread->sp -= outgoingArgCount;
	memcpy(currentThread->sp, outgoingArgs, outgoingArgCount * sizeof(UDATA));

	Trc_Decomp_performDecompile_Exit(currentThread, currentThread->sp);
}


/**
 * Decompile the outer (and possibly only) frame.
 *
 * @param[in] *currentThread current thread
 * @param[in] *decompileState the decompilation state copied from the stack walk
 * @param[in] *decompRecord the decompilation record
 * @param[in] *osrFrame the OSR frame from which to copy the information
 */
static void
decompileOuterFrame(J9VMThread * currentThread, J9JITDecompileState * decompileState, J9JITDecompilationInfo * decompRecord, J9OSRFrame *osrFrame)
{
	UDATA * newTempBase;
	UDATA * oldTempBase;
	UDATA * newPendingBase;
	UDATA * oldPendingBase;
	UDATA * stackFrame;
	UDATA * newSP;
	J9Method *method = osrFrame->method;
	J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	UDATA argCount = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);
	UDATA numberOfLocals = osrFrame->numberOfLocals;
	UDATA tempCount = numberOfLocals - argCount;
	U_8 ** jitReturnPCAddress =  (U_8 **) &(((J9JITFrame *) (decompileState->bp))->returnPC);
	U_8 * jitReturnPC = *jitReturnPCAddress;
	UDATA * currentJ2iFrame = decompileState->j2iFrame;
	UDATA pendingStackHeight = osrFrame->pendingStackHeight;

	oldTempBase = ((UDATA*)(osrFrame + 1)) + osrFrame->maxStack;

	/* Determine the shape of the new stack frame */

	newTempBase = decompileState->a0 - numberOfLocals + 1;
	if (decompileState->previousFrameBytecodes) {
		/* Use the I2J returnSP in this case in order to account for the possible 8-alignment of the I2J args */
		UDATA * correctTempBase = UNTAG2(decompileState->i2jState.returnSP, UDATA *) - numberOfLocals;

		/* If the args were moved for alignment, copy them back to their original position */
		if (newTempBase != correctTempBase) {
			UDATA * newA0 = correctTempBase + numberOfLocals - 1;
			memmove(correctTempBase + tempCount, newTempBase + tempCount, argCount * sizeof(UDATA));
			newTempBase = correctTempBase;
			decompileState->a0 = newA0;
		}
		stackFrame = (UDATA *) (((U_8 *) newTempBase) - sizeof(J9SFStackFrame));
	} else {
		stackFrame = (UDATA *) (((U_8 *) newTempBase) - sizeof(J9SFJ2IFrame));
	}
	newPendingBase = stackFrame - pendingStackHeight;
	oldPendingBase = oldTempBase - pendingStackHeight;
	newSP = newPendingBase;

	/* If the JIT stack frame has not been constructed, don't attempt to read from it */

	UDATA resolveFrameType = decompileState->resolveFrameFlags & J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK;
	if (J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME == resolveFrameType) {
		Trc_Decomp_performDecompile_monitorNotEntered(currentThread);

		/* Temps have not been pushed yet - zero them and set up the sync object (if any) */

		memset(newTempBase, 0, tempCount * sizeof(UDATA));
		if (romMethod->modifiers & J9AccSynchronized) {
			if (romMethod->modifiers & J9AccStatic) {
				newTempBase[0] = (UDATA)J9VM_J9CLASS_TO_HEAPCLASS(J9_CLASS_FROM_METHOD(method));
			} else {
				newTempBase[0] = *(decompileState->a0);
			}
		} else if (J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod)) {
			newTempBase[0] = *(decompileState->a0);
		}
	} else {
		/* Copy the temps to their new location */

		memcpy(newTempBase, oldTempBase, tempCount * sizeof(UDATA));
	}

	/* Copy pending pushes */

	memcpy(newPendingBase, oldPendingBase, pendingStackHeight * sizeof(UDATA));

	/* Create the stack frame, either a pure bytecode frame or a J2I frame, depending on the previous frame. */

	if (decompileState->previousFrameBytecodes) {
		J9SFStackFrame * bytecodeFrame = (J9SFStackFrame *) stackFrame;

		Trc_Decomp_performDecompile_buildingBCFrame(currentThread, stackFrame);

		bytecodeFrame->savedPC = decompileState->i2jState.pc;
		bytecodeFrame->savedCP = decompileState->i2jState.literals;
		bytecodeFrame->savedA0 = decompileState->i2jState.a0;
	} else {
		J9SFJ2IFrame * j2iFrame = (J9SFJ2IFrame *) stackFrame;
		char * returnChar;
		J9JITDecompilationInfo * info;

		Trc_Decomp_performDecompile_buildingJ2IFrame(currentThread, stackFrame);

		memcpy(&(j2iFrame->i2jState), &(decompileState->i2jState), sizeof(J9I2JState));
		memcpy(&(j2iFrame->J9SW_LOWEST_MEMORY_PRESERVED_REGISTER), decompileState->preservedRegisterValues, J9SW_JIT_CALLEE_PRESERVED_SIZE * sizeof(UDATA));
		j2iFrame->specialFrameFlags = J9_STACK_FLAGS_JIT_CALL_IN_FRAME | J9_STACK_FLAGS_JIT_CALL_IN_TYPE_J2_I;
		j2iFrame->returnAddress = jitReturnPC;
		j2iFrame->previousJ2iFrame = currentJ2iFrame;
		currentJ2iFrame = (UDATA *) &(j2iFrame->taggedReturnSP);

		returnChar = (char *) J9UTF8_DATA(J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(method)->romClass, romMethod));
		while (*returnChar++ != ')') ;
		switch(*returnChar) {
			case 'V':
				j2iFrame->exitPoint = J9_BUILDER_SYMBOL(jitExitInterpreter0);
				break;
			case 'D':
				j2iFrame->exitPoint = J9_BUILDER_SYMBOL(jitExitInterpreterD);
				break;
			case 'F':
				j2iFrame->exitPoint = J9_BUILDER_SYMBOL(jitExitInterpreterF);
				break;
			case 'J':
#ifdef J9VM_ENV_DATA64
			case '[':
			case 'L':
#endif
				j2iFrame->exitPoint = J9_BUILDER_SYMBOL(jitExitInterpreterJ);
				break;
			default:
				j2iFrame->exitPoint = J9_BUILDER_SYMBOL(jitExitInterpreter1);
				break;
		}

#ifdef J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
		j2iFrame->taggedReturnSP = (UDATA *) (((U_8 *) (((UDATA *) (j2iFrame + 1)) + argCount + tempCount)));
#else
		j2iFrame->taggedReturnSP = (UDATA *) (((U_8 *) (((UDATA *) (j2iFrame + 1)) + tempCount)));
#endif

		/* If the next thing on the decompilation stack used to point to the return address save slot in the decompiled JIT frame, point it to the slot in the J2I frame. */

		if ((info = currentThread->decompilationStack) != NULL) {
			if (info->pcAddress == jitReturnPCAddress) {
				Trc_Decomp_performDecompile_fixingDecompStack(currentThread, info, info->pcAddress, &(j2iFrame->returnAddress), info->pc);
				info->pcAddress = &(j2iFrame->returnAddress);
			}
		}

	}

	/* Fill in vmThread values for bytecode execution */

	currentThread->pc = osrFrame->bytecodePCOffset + J9_BYTECODE_START_FROM_RAM_METHOD(osrFrame->method);
	currentThread->literals = decompileState->literals;
	currentThread->arg0EA = decompileState->a0;
	currentThread->sp = newSP;
	currentThread->j2iFrame = currentJ2iFrame;

	/* If the outer framed failed a method monitor enter, hide the interpreted version to prevent exception throw
	 * from exiting the monitor that was never entered.
	 */
	if (J9_STACK_FLAGS_JIT_FAILED_METHOD_MONITOR_ENTER_RESOLVE == resolveFrameType) {
		newTempBase[-1] |= J9SF_A0_INVISIBLE_TAG;
	}
}


/**
 * Fix the java stack to account for a newly-added decompilation.  This involves modifying
 * the return address in the stack which would be used to return to the frame for which the
 * decompilation was added.
 *
 * @param[in] *currentThread current thread
 * @param[in] *walkState stack walk state (already at the OSR point)
 * @param[in] *info the new decompilation record
 * @param[in] reason the reason for the decompilation
 * @param[in] **link pointer to the next pointer of the previous decompilation record
 * 					 (this may be the root of the decompilation stack)
 */
static void
fixStackForNewDecompilation(J9VMThread * currentThread, J9StackWalkState * walkState, J9JITDecompilationInfo *info, UDATA reason, J9JITDecompilationInfo **link)
{
	J9JavaVM* vm = currentThread->javaVM;
	U_8 ** pcStoreAddress = walkState->pcAddress;

	/* Fill in the decompilation record and link it into the stack */

	info->pcAddress = walkState->pcAddress;
	info->bp = walkState->bp;
	info->method = walkState->method;
	info->reason = reason;
	info->pc = walkState->pc;
	info->next = *link;
	*link = info;

	/* Slam the return address to point to the appropriate decompile */

	if (0 != walkState->resolveFrameFlags) {
		UDATA resolveFrameType = walkState->resolveFrameFlags & J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK;
		switch(resolveFrameType) {
		case J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME:
			if (J9_ARE_ANY_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method)->modifiers, J9AccSynchronized)) {
				Trc_Decomp_addDecompilation_beforeSyncMonitorEnter(currentThread);
				*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileBeforeMethodMonitorEnter);
				break;
			}
			/* Intentional fall-through */
		case J9_STACK_FLAGS_JIT_METHOD_MONITOR_ENTER_RESOLVE:
			Trc_Decomp_addDecompilation_beforeReportMethodEnter(currentThread);
			*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileBeforeReportMethodEnter);
			break;
		case J9_STACK_FLAGS_JIT_MONITOR_ENTER_RESOLVE:
			Trc_Decomp_addDecompilation_atMonitorEnter(currentThread);
			*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileAfterMonitorEnter);
			break;
		case J9_STACK_FLAGS_JIT_ALLOCATION_RESOLVE:
			Trc_Decomp_addDecompilation_atAllocate(currentThread);
			*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileAfterAllocation);
			break;
		case J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE:
			/* Decompile during jitReportExceptionCatch */
			Trc_Decomp_addDecompilation_atExceptionCatch(currentThread);
			*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileAtExceptionCatch);
			break;
		default:
			Trc_Decomp_addDecompilation_atCurrentPC(currentThread);
			*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileAtCurrentPC);
			break;
		}
	} else {
		J9UTF8 * sig, *name;
		char * returnChar;
		J9OSRBuffer *osrBuffer = &info->osrBuffer;
		J9OSRFrame *osrFrame = (J9OSRFrame*)(osrBuffer + 1);
		U_8 * bytecodePC = osrFrame->bytecodePCOffset + J9_BYTECODE_START_FROM_RAM_METHOD(osrFrame->method);

		Trc_Decomp_addDecompilation_atInvoke(currentThread);

		/* PC points at an invoke - fetch the name and signature it references */
		J9ROMNameAndSignature *nameAndSig = getNASFromInvoke(bytecodePC, J9_CLASS_FROM_METHOD(osrFrame->method)->romClass);
		sig = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
		name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
		Trc_Decomp_addDecompilation_explainInvoke(currentThread, J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));

		returnChar = (char *) J9UTF8_DATA(sig);
		while (*returnChar++ != ')') ;
		switch(*returnChar) {
			case 'V':
				*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileOnReturn0);
				break;
			case 'D':
				*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileOnReturnD);
				break;
			case 'F':
				*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileOnReturnF);
				break;
			case 'J':
				*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileOnReturnJ);
				break;
#ifdef J9VM_ENV_DATA64
			case '[':
			case 'L':
				*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileOnReturnL);
				break;
#endif
			default:
				*pcStoreAddress = (U_8 *) J9_BUILDER_SYMBOL(jitDecompileOnReturn1);
				break;
		}
	}

	/* Enable stack dump after linking the decompilation into the stack if -verbose:stackwalk=0 is specified */
	dumpStack(walkState->walkThread, "after fixStackForNewDecompilation");
}


static J9JITDecompilationInfo *
addDecompilation(J9VMThread * currentThread, J9StackWalkState * walkState, UDATA reason)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9JavaVM* vm = currentThread->javaVM;
	J9VMThread * decompileThread = walkState->walkThread;
	J9JITDecompilationInfo * info;
	J9JITDecompilationInfo ** previous;
	J9JITDecompilationInfo * current;
	J9JITExceptionTable *metaData = walkState->jitInfo;
	UDATA allocSize = sizeof(J9JITDecompilationInfo);
	UDATA osrFrame = FALSE;
	J9OSRData osrData;

	Trc_Decomp_addDecompilation_Entry(currentThread, walkState->method);
	decompPrintMethod(currentThread, walkState->method);
	Trc_Decomp_addDecompilation_walkStateInfo(currentThread, walkState->bp, walkState->arg0EA, walkState->constantPool, walkState->pc);
	Trc_Decomp_addDecompilation_reason(currentThread, reason,
		(reason & JITDECOMP_CODE_BREAKPOINT) ? " CODE_BREAKPOINT" : "",
		(reason & JITDECOMP_DATA_BREAKPOINT) ? " DATA_BREAKPOINT" : "",
		(reason & JITDECOMP_HOTSWAP) ? " HOTSWAP" : "",
		(reason & JITDECOMP_POP_FRAMES) ? " POP_FRAMES" : "",
		(reason & JITDECOMP_SINGLE_STEP) ? " SINGLE_STEP" : "",
		(reason & JITDECOMP_STACK_LOCALS_MODIFIED) ? " STACK_LOCALS_MODIFIED" : "",
		(reason & JITDECOMP_FRAME_POP_NOTIFICATION) ? " FRAME_POP_NOTIFICATION" : "");

	/* addDecompilation must be called only when the walkState represents a compiled frame */
	Assert_CodertVM_true(NULL != metaData);

	/* The decompilation is ordered by frame, with lesser frame values appearing at the beginning */

	previous = &(decompileThread->decompilationStack);
	while ((current = *previous) != NULL) {
		/* If a decompilation for this frame already exists, tag it with the new reason and return */

		if (current->bp == walkState->bp) {
			Trc_Decomp_addDecompilation_existingRecord(currentThread, current);
			current->reason |= reason;
			return current;
		}
		if (current->bp > walkState->bp) break;
		previous = &(current->next);
	}

	/* OSR is not appropriate for frames whose stack frame has not been created */
	UDATA resolveFrameFlags = walkState->resolveFrameFlags;
	UDATA resolveFrameType = resolveFrameFlags & J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK;
	if (J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME != resolveFrameType) {
		if (usesOSR(currentThread, metaData)) {
			Trc_Decomp_addDecompilation_osrFrame(currentThread);
			osrFrame = TRUE;
		}
	}

	/* Allocate and zero the decompilation record, taking the OSR buffer into account */
	allocSize += osrAllFramesSize(currentThread, metaData, walkState->pc, resolveFrameFlags);
	info = (J9JITDecompilationInfo *) j9mem_allocate_memory(allocSize, OMRMEM_CATEGORY_JIT);
	if (!info) {
		Trc_Decomp_addDecompilation_allocFailed(currentThread);
		return NULL;
	}
	memset(info, 0, allocSize);
	Trc_Decomp_addDecompilation_allocRecord(currentThread, info);
	info->usesOSR = osrFrame;

	/* Fill in the interpreter values in the OSR buffer */
	osrData.targetThread = walkState->walkThread;
	osrData.metaData = metaData;
	osrData.jitPC = walkState->pc;
	osrData.resolveFrameFlags = walkState->resolveFrameFlags;
	osrData.objectArgScanCursor = getObjectArgScanCursor(walkState);
	osrData.objectTempScanCursor = getObjectTempScanCursor(walkState);
	if (OSR_OK != initializeOSRBuffer(currentThread, &info->osrBuffer, &osrData)) {
		Trc_Decomp_addDecompilation_allocFailed(currentThread);
		j9mem_free_memory(info);
		return NULL;
	}

	if (osrFrame) {
		UDATA scratchBufferSize = roundedOSRScratchBufferSize(currentThread, metaData, walkState->pc);
		UDATA jitStackFrameSize = (UDATA)(walkState->arg0EA + 1) - (UDATA)walkState->unwindSP;
		void *osrScratchBuffer = j9mem_allocate_memory(scratchBufferSize + jitStackFrameSize, OMRMEM_CATEGORY_JIT);
		UDATA mustDecompile = FALSE;
		UDATA osrResultCode = OSR_OK;

		/* TODO: use global buffer here? */
		if (NULL == osrScratchBuffer) {
			Trc_Decomp_addDecompilation_allocFailed(currentThread);
			j9mem_free_memory(info);
			return NULL;
		}

		osrResultCode = performOSR(currentThread, walkState, &info->osrBuffer, (U_8*)osrScratchBuffer, scratchBufferSize, jitStackFrameSize, &mustDecompile);
		if (OSR_OK != osrResultCode) {
			Trc_Decomp_addDecompilation_osrFailed(currentThread);
			j9mem_free_memory(osrScratchBuffer);
			j9mem_free_memory(info);
			return NULL;
		}
		if (mustDecompile) {
			Trc_Decomp_addDecompilation_osrMustDecompile(currentThread);
			reason |= JITDECOMP_ON_STACK_REPLACEMENT;
		}
		j9mem_free_memory(osrScratchBuffer);
	}

	/* Add the decompilation to the stack */
	fixStackForNewDecompilation(currentThread, walkState, info, reason, previous);

	Trc_Decomp_addDecompilation_Exit(currentThread, info);

	return info;
}

void  
c_jitDecompileAtExceptionCatch(J9VMThread * currentThread)
{
	/* Fetch the exception from the JIT */
	j9object_t exception = (j9object_t)currentThread->jitException;
	/* Fetch and unstack the decompilation information for this frame */
	J9JITDecompilationInfo *decompRecord = fetchAndUnstackDecompilationInfo(currentThread);
	U_8 *jitPC = decompRecord->pc;
	/* Simulate a call to a resolve helper to make the stack walkable */
	buildBranchJITResolveFrame(currentThread, jitPC, J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE);

	J9JavaVM *vm = currentThread->javaVM;
	J9JITDecompileState decompileState;
	J9StackWalkState walkState;
	J9OSRBuffer *osrBuffer = &decompRecord->osrBuffer;
	UDATA numberOfFrames = osrBuffer->numberOfFrames;
	J9OSRFrame *osrFrame = (J9OSRFrame*)(osrBuffer + 1);
	J9JITExceptionTable *metaData = NULL;

	/* Collect the required information from the stack - top visible frame is the decompile frame */
	walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_MAINTAIN_REGISTER_MAP | J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_SAVE_STACKED_REGISTERS;
	walkState.skipCount = 0;
	walkState.frameWalkFunction = decompileMethodFrameIterator;
	walkState.walkThread = currentThread;
	walkState.userData1 = &decompileState;
	walkState.userData2 = NULL;
	vm->walkStackFrames(currentThread, &walkState);
	metaData = decompileState.metaData;

	/* Determine in which inlined frame the exception is being caught */
	UDATA newNumberOfFrames = 1;
	void *stackMap = NULL;
	void *inlineMap = NULL;
	void *inlinedCallSite = NULL;
	/* Note we need to add 1 to the JIT PC here in order to get the correct map at the exception handler
	 * because jitGetMapsFromPC is expecting a return address, so it subtracts 1.  The value stored in the
	 * decomp record is the start address of the compiled exception handler.
	 */
	jitGetMapsFromPC(vm, metaData, (UDATA)jitPC + 1, &stackMap, &inlineMap);
	Assert_CodertVM_false(NULL == inlineMap);
	if (NULL != getJitInlinedCallInfo(metaData)) {
		inlinedCallSite = getFirstInlinedCallSite(metaData, inlineMap);
		if (inlinedCallSite != NULL) {
			newNumberOfFrames = getJitInlineDepthFromCallSite(metaData, inlinedCallSite) + 1;
		}
	}
	Assert_CodertVM_true(numberOfFrames >= newNumberOfFrames);
	J9Pool *monitorEnterRecordPool = currentThread->monitorEnterRecordPool;
	while (numberOfFrames != newNumberOfFrames) {
		/* Discard the monitor records for the popped frames */
		J9MonitorEnterRecord *enterRecord = osrFrame->monitorEnterRecords;
		while (NULL != enterRecord) {
			J9MonitorEnterRecord *recordToFree = enterRecord;
			enterRecord = enterRecord->next;
			pool_removeElement(monitorEnterRecordPool, recordToFree);
		}
		osrFrame->monitorEnterRecords = NULL;
		osrFrame = (J9OSRFrame*)((U_8*)osrFrame + osrFrameSize(osrFrame->method));
		numberOfFrames -= 1;
	}

	/* Fix the OSR frame to resume at the catch point with an empty pending stack (the caught exception
	 * is pushed after decompilation completes).
	 */
	osrFrame->bytecodePCOffset = getCurrentByteCodeIndexAndIsSameReceiver(metaData, inlineMap, inlinedCallSite, NULL);
	Trc_Decomp_jitInterpreterPCFromWalkState_Entry(jitPC);
	Trc_Decomp_jitInterpreterPCFromWalkState_exHandler(osrFrame->bytecodePCOffset);
	osrFrame->pendingStackHeight = 0;

	performDecompile(currentThread, &decompileState, decompRecord, osrFrame, numberOfFrames);

	freeDecompilationRecord(currentThread, decompRecord, TRUE);

	/* Push the exception */
	UDATA *sp = currentThread->sp - 1;
	*(j9object_t*)sp = exception;
	currentThread->sp = sp;
	dumpStack(currentThread, "after jitDecompileAtExceptionCatch");
	currentThread->tempSlot = (UDATA)J9_BUILDER_SYMBOL(executeCurrentBytecodeFromJIT);
}

#if (defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)) /* priv. proto (autogen) */

void  
jitDecompileMethodForFramePop(J9VMThread * currentThread, UDATA skipCount)
{
	J9JITDecompileState decompileState;
	J9StackWalkState walkState;
	J9JITDecompilationInfo * decompRecord = currentThread->decompilationStack;
	J9JavaVM *vm = currentThread->javaVM;
	J9OSRBuffer *osrBuffer = &decompRecord->osrBuffer;
	UDATA numberOfFrames = osrBuffer->numberOfFrames;
	J9OSRFrame *osrFrame = (J9OSRFrame*)(osrBuffer + 1);

	/* Unstack the decomp record and fix the stacked PC */

	*(decompRecord->pcAddress) = decompRecord->pc;
	currentThread->decompilationStack = decompRecord->next;

	/* Collect the required information from the stack */

	walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_MAINTAIN_REGISTER_MAP;
	walkState.skipCount = skipCount;
	walkState.frameWalkFunction = decompileMethodFrameIterator;
	walkState.walkThread = currentThread;
	walkState.userData1 = &decompileState;
	walkState.userData2 = NULL;
	currentThread->javaVM->walkStackFrames(currentThread, &walkState);

 	/* Now decompile the frame */

	performDecompile(currentThread, &decompileState, decompRecord, osrFrame, numberOfFrames);
	freeDecompilationRecord(currentThread, decompRecord, TRUE);

	dumpStack(currentThread, "after jitDecompileMethodForFramePop");
}

#endif /* J9VM_INTERP_HOT_CODE_REPLACEMENT (autogen) */


static J9JITDecompilationInfo *
deleteDecompilationForExistingFrame(J9VMThread * decompileThread, J9JITDecompilationInfo * info)
{
	PORT_ACCESS_FROM_VMC(decompileThread);
	J9JITDecompilationInfo * next = info->next;

	Trc_Decomp_deleteDecompilationForExistingFrame_Entry();

	*(info->pcAddress) = info->pc;

	Trc_Decomp_deleteDecompilationForExistingFrame_freeDecomp(info, info->bp);

	freeDecompilationRecord(decompileThread, info, FALSE);

	Trc_Decomp_deleteDecompilationForExistingFrame_Exit();

	return next;
}


static void
freeDecompilationRecord(J9VMThread * decompileThread, J9JITDecompilationInfo * info, UDATA retain)
{
	PORT_ACCESS_FROM_VMC(decompileThread);

	j9mem_free_memory(decompileThread->lastDecompilation);
	decompileThread->lastDecompilation = NULL;
	if (info->reason & JITDECOMP_OSR_GLOBAL_BUFFER_USED) {
		omrthread_monitor_exit(decompileThread->javaVM->osrGlobalBufferLock);
	} else {
		if (retain) {
			decompileThread->lastDecompilation = info;
		} else {
			j9mem_free_memory(info);
		}
	}
}


void  
jitDataBreakpointAdded(J9VMThread * currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9JITConfig *jitConfig = vm->jitConfig;

	/* We have exclusive */

	Trc_Decomp_jitDataBreakpointAdded_Entry(currentThread);

	++(jitConfig->dataBreakpointCount);

	/* In normal mode (where the JIT does not generate code to trigger field watch events), every addition
	 * of a watch must discard all compiled code.  In the new mode where the JIT does generate watch triggers,
	 * Only the addition of the first watch requires discarding compiled code.  From then on, the JIT will
	 * generate the appropriate code, so no discarding is required.  See jitDataBreakpointAdded regarding
	 * transitioning back out of this mode.
	 */
	bool inlineWatches = J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_JIT_INLINE_WATCHES);
	if (!inlineWatches || !jitConfig->inlineFieldWatches) {
		/* Remove all breakpoints before resetting the methods */

		removeAllBreakpoints(currentThread);

		/* Toss the compilation queue */

		if (inlineWatches) {
			jitConfig->jitFlushCompilationQueue(currentThread, J9FlushCompQueueDataBreakpoint);
			/* Make all future compilations inline the field watch code */
			jitConfig->inlineFieldWatches = TRUE;
		} else {
			jitConfig->jitClassesRedefined(currentThread, 0, NULL);			
		}

		/* Find every method which has been translated and mark it for retranslation */

		jitResetAllMethods(currentThread);

		/* Reinstall the breakpoints */

		reinstallAllBreakpoints(currentThread);

		/* Mark every JIT method in every stack for decompilation */

		decompileAllMethodsInAllStacks(currentThread, JITDECOMP_DATA_BREAKPOINT);
	}

	Trc_Decomp_jitDataBreakpointAdded_Exit(currentThread);
}


void  
jitDataBreakpointRemoved(J9VMThread * currentThread)
{
	J9JavaVM *vm = currentThread->javaVM;

	/* We have exclusive */

	Trc_Decomp_jitDataBreakpointRemoved_Entry(currentThread);

	--(vm->jitConfig->dataBreakpointCount);

	/* For now, once the JIT is in field watch mode, it stays that way forever, even if all current
	 * watches are removed.  This may change in the future.
	 */
	if (J9_ARE_NO_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_JIT_INLINE_WATCHES)) {
		/* Remove all breakpoints before resetting the methods */

		removeAllBreakpoints(currentThread);

		/* Find every method which has been prevented from being translated and mark it for retranslation */

		jitResetAllUntranslateableMethods(currentThread);

		/* Reinstall the breakpoints */

		reinstallAllBreakpoints(currentThread);
	}

	Trc_Decomp_jitDataBreakpointRemoved_Exit(currentThread);
}


static void
decompileAllMethodsInAllStacks(J9VMThread * currentThread, UDATA reason)
{
	J9VMThread * loopThread;

	/* Mark every JIT method in every stack for decompilation */

	loopThread = currentThread;
	do {
		J9StackWalkState walkState;

		walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_MAINTAIN_REGISTER_MAP;
		walkState.skipCount = 0;
		walkState.frameWalkFunction = decompileAllFrameIterator;
		walkState.walkThread = loopThread;
		walkState.userData1 = (void *) reason;
		currentThread->javaVM->walkStackFrames(currentThread, &walkState);
	} while ((loopThread = loopThread->linkNext) != currentThread);
}


static UDATA
decompileAllFrameIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	/* Decompile all JIT frames */

	if (walkState->jitInfo) {
		addDecompilation(currentThread, walkState, (UDATA) walkState->userData1);
	}

	return J9_STACKWALK_KEEP_ITERATING;
}


static void
deleteAllDecompilations(J9VMThread * currentThread, UDATA reason, J9Method * method)
{
	J9VMThread * loopThread;

	Trc_Decomp_deleteAllDecompilations_Entry(currentThread);

	loopThread = currentThread;
	do {
		J9JITDecompilationInfo ** previous;
		J9JITDecompilationInfo * current;

		previous = &(loopThread->decompilationStack);
		while ((current = *previous) != NULL) {
			if ((current->reason & reason) && (!method || (current->method == method))) {
				current->reason &= ~reason;
				if (current->reason) {
					Trc_Decomp_deleteAllDecompilations_notFreeingRecord(currentThread, current, current->reason);
					previous = &(current->next);
				} else {
					*previous = deleteDecompilationForExistingFrame(loopThread, current);
				}
			} else {
				previous = &(current->next);
			}
		}
	} while ((loopThread = loopThread->linkNext) != currentThread);	

	Trc_Decomp_deleteAllDecompilations_Exit(currentThread);
}


void  
jitExceptionCaught(J9VMThread * currentThread)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	J9StackWalkState * walkState = currentThread->stackWalkState;
	J9JITDecompilationInfo * catchDecompile = NULL;
	UDATA reportEvent = FALSE;

	Trc_Decomp_jitExceptionCaught_Entry(currentThread, walkState->arg0EA);

	/* Called when an exception is caught, the thread's walkState knows how far to pop */

	catchDecompile = jitCleanUpDecompilationStack(currentThread, walkState, FALSE);

	/* If the catch frame is not JIT, nothing more needs to be done */

	if (!walkState->jitInfo) {
		Trc_Decomp_jitExceptionCaught_notJITFrame(currentThread);
		return;
	}

	Trc_Decomp_jitExceptionCaught_JITFrame(currentThread);

	/* If a decompilation record exists for the catch frame, update the PC to be the exception handler PC */

	if (catchDecompile) {
		Trc_Decomp_jitExceptionCaught_frameMarked(currentThread, walkState->bp);
		catchDecompile->pc = (U_8 *) walkState->userData2;
	}

	reportEvent = J9_EVENT_IS_HOOKED(currentThread->javaVM->hookInterface, J9HOOK_VM_EXCEPTION_CATCH);
	if (reportEvent) {
		Trc_Decomp_jitExceptionCaught_mustReportNormal(currentThread, walkState->arg0EA);

		if (catchDecompile) {
			Trc_Decomp_jitExceptionCaught_decompileRequested(currentThread);
			currentThread->floatTemp4 = J9_BUILDER_SYMBOL(jitDecompileAtExceptionCatch);
		} else {
			Trc_Decomp_jitExceptionCaught_notDecompiling(currentThread);
			currentThread->floatTemp4 = walkState->userData2;
		}
		walkState->userData2 = J9_BUILDER_SYMBOL(jitReportExceptionCatch);
	} else {
		Trc_Decomp_jitExceptionCaught_notReportingCatch(currentThread);
		if (catchDecompile) {
			Trc_Decomp_jitExceptionCaught_decompileRequested(currentThread);
			walkState->userData2 = J9_BUILDER_SYMBOL(jitDecompileAtExceptionCatch);
		} else {
			Trc_Decomp_jitExceptionCaught_notDecompiling(currentThread);
		}
	}

	Trc_Decomp_jitExceptionCaught_Exit(currentThread);
}


static void
markMethodBreakpointed(J9VMThread * currentThread, J9JITBreakpointedMethod * breakpointedMethod)
{
	J9Method * method = breakpointedMethod->method;

	breakpointedMethod->hasBeenTranslated = FALSE;
	if ((((UDATA) method->extra) & J9_STARTPC_NOT_TRANSLATED) == 0) {
		breakpointedMethod->hasBeenTranslated = TRUE;
		_fsdSwitchToInterpPatchEntry(method->extra);
	}

	method->constantPool = (J9ConstantPool *) ((UDATA) method->constantPool | (UDATA)J9_STARTPC_METHOD_BREAKPOINTED);
	if (NULL != currentThread->javaVM->jitConfig->jitMethodBreakpointed) { // TODO: remove once JIT is updated
		currentThread->javaVM->jitConfig->jitMethodBreakpointed(currentThread, method);
	}
}


static void
markMethodUnbreakpointed(J9VMThread * currentThread, J9JITBreakpointedMethod * breakpointedMethod)
{
	J9Method * method = breakpointedMethod->method;

	method->constantPool = (J9ConstantPool *) ((UDATA) method->constantPool & ~(UDATA)J9_STARTPC_METHOD_BREAKPOINTED);
	if (breakpointedMethod->hasBeenTranslated) {
		_fsdRestoreToJITPatchEntry(method->extra);
	}
	if (NULL != currentThread->javaVM->jitConfig->jitMethodUnbreakpointed) { // TODO: remove once JIT is updated
		currentThread->javaVM->jitConfig->jitMethodUnbreakpointed(currentThread, method);
	}
}


static void
removeAllBreakpoints(J9VMThread * currentThread)
{
	J9JITBreakpointedMethod * breakpointedMethod = currentThread->javaVM->jitConfig->breakpointedMethods;

	while (breakpointedMethod) {
		markMethodUnbreakpointed(currentThread, breakpointedMethod);
		breakpointedMethod = breakpointedMethod->link;
	}
}


static void
reinstallAllBreakpoints(J9VMThread * currentThread)
{
	J9JITBreakpointedMethod * breakpointedMethod = currentThread->javaVM->jitConfig->breakpointedMethods;

	while (breakpointedMethod) {
		markMethodBreakpointed(currentThread, breakpointedMethod);
		breakpointedMethod = breakpointedMethod->link;
	}
}


void  
jitSingleStepAdded(J9VMThread * currentThread)
{
	/* We have exclusive */

	Trc_Decomp_jitSingleStepAdded_Entry(currentThread);

	if (++(currentThread->javaVM->jitConfig->singleStepCount) == 1) {

		/* Mark every JIT method in every stack for decompilation */

		decompileAllMethodsInAllStacks(currentThread, JITDECOMP_SINGLE_STEP);
	}

	Trc_Decomp_jitSingleStepAdded_Exit(currentThread);
}


void  
jitSingleStepRemoved(J9VMThread * currentThread)
{
	/* We have exclusive */

	Trc_Decomp_jitSingleStepRemoved_Entry(currentThread);

	if (--(currentThread->javaVM->jitConfig->singleStepCount) == 0) {

		/* Remove the single step decompilations */

		deleteAllDecompilations(currentThread, JITDECOMP_SINGLE_STEP, NULL);
	}

	Trc_Decomp_jitSingleStepRemoved_Exit(currentThread);
}


UDATA
initializeFSD(J9JavaVM * vm)
{
	J9JITConfig * jitConfig = vm->jitConfig;

	jitConfig->jitCodeBreakpointAdded = jitCodeBreakpointAdded;
	jitConfig->jitCodeBreakpointRemoved = jitCodeBreakpointRemoved;
	jitConfig->jitDataBreakpointAdded = jitDataBreakpointAdded;
	jitConfig->jitDataBreakpointRemoved = jitDataBreakpointRemoved;
	jitConfig->jitSingleStepAdded = jitSingleStepAdded;
	jitConfig->jitSingleStepRemoved = jitSingleStepRemoved;
	jitConfig->jitExceptionCaught = jitExceptionCaught;
	jitConfig->jitCleanUpDecompilationStack = jitCleanUpDecompilationStack;
#ifdef J9VM_INTERP_HOT_CODE_REPLACEMENT
	jitConfig->jitHotswapOccurred = jitHotswapOccurred;
	jitConfig->jitDecompileMethodForFramePop = jitDecompileMethodForFramePop;
#endif
#ifdef J9VM_OPT_JVMTI
	jitConfig->jitFramePopNotificationAdded = jitFramePopNotificationAdded;
#endif
	jitConfig->jitStackLocalsModified = jitStackLocalsModified;
	jitConfig->jitLocalSlotAddress = jitLocalSlotAddress;
	jitConfig->jitAddDecompilationForFramePop = jitAddDecompilationForFramePop;

	jitConfig->fsdEnabled = TRUE;

	return 0;
}

static J9OSRFrame *
findOSRFrameAtInlineDepth(J9OSRBuffer *osrBuffer, UDATA inlineDepth)
{
	J9OSRFrame *osrFrame = (J9OSRFrame*)(osrBuffer + 1);
	UDATA osrFrameInlineDepth = osrBuffer->numberOfFrames - 1;
	Assert_CodertVM_true(osrFrameInlineDepth >= inlineDepth);
	while (osrFrameInlineDepth != inlineDepth) {
		osrFrame = (J9OSRFrame*)((U_8*)osrFrame + osrFrameSize(osrFrame->method));
		osrFrameInlineDepth -= 1;
	}
	return osrFrame;
}

#if (defined(J9VM_OPT_JVMTI)) /* priv. proto (autogen) */

static void  
jitFramePopNotificationAdded(J9VMThread * currentThread, J9StackWalkState * walkState, UDATA inlineDepth)
{
	J9JITDecompilationInfo *decompilationInfo = NULL;

	Trc_Decomp_jitFramePopNotificationAdded_Entry(currentThread, walkState->walkThread, walkState->arg0EA, walkState->method);
	decompPrintMethod(currentThread, walkState->method);

	decompilationInfo = addDecompilation(currentThread, walkState, JITDECOMP_FRAME_POP_NOTIFICATION);
	if (NULL != decompilationInfo) {
		J9OSRFrame *osrFrame = findOSRFrameAtInlineDepth(&decompilationInfo->osrBuffer, inlineDepth);
		osrFrame->flags |= J9OSRFRAME_NOTIFY_FRAME_POP;
	}

	Trc_Decomp_jitFramePopNotificationAdded_Exit(currentThread);
}
#endif /* J9VM_OPT_JVMTI (autogen) */

static J9JITDecompilationInfo*
jitAddDecompilationForFramePop(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	return addDecompilation(currentThread, walkState, JITDECOMP_POP_FRAMES);
}


void
jitStackLocalsModified(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	Trc_Decomp_jitStackLocalsModified_Entry(currentThread);

	if (walkState->jitInfo == NULL) {
		Trc_Decomp_jitStackLocalsModified_notJIT(currentThread);
	} else {
		addDecompilation(currentThread, walkState, JITDECOMP_STACK_LOCALS_MODIFIED);
	}

	Trc_Decomp_jitStackLocalsModified_Exit(currentThread);
}


void  
jitBreakpointedMethodCompiled(J9VMThread * currentThread, J9Method * method, void * startAddress)
{
	J9JITConfig * jitConfig = currentThread->javaVM->jitConfig;
	J9JITBreakpointedMethod * breakpointedMethod = jitConfig->breakpointedMethods;

	Trc_Decomp_jitBreakpointedMethodCompiled_Entry(currentThread, method, startAddress);
	decompPrintMethod(currentThread, method);

	while (breakpointedMethod) {
		if (breakpointedMethod->method == method) {
			/* Assert breakpointedMethod->hasBeenTranslated == FALSE */
			breakpointedMethod->hasBeenTranslated = TRUE;
			_fsdSwitchToInterpPatchEntry(startAddress);
			Trc_Decomp_jitBreakpointedMethodCompiled_Exit_success(currentThread, breakpointedMethod);
			return;
		}
		breakpointedMethod = breakpointedMethod->link;
	}
	Trc_Decomp_jitBreakpointedMethodCompiled_Exit_fail(currentThread);
}


/**
 * Compute the number of bytes required for a single OSR frame.
 *
 * @param[in] *method the J9Method for which to compute the OSR frame size
 *
 * @return byte size of the OSR frame
 */
UDATA
osrFrameSize(J9Method *method)
{
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	return osrFrameSizeRomMethod(romMethod);
}


/**
 * Compute the number of bytes required for a single OSR frame.
 *
 * @param[in] *romMethod the J9ROMMethod for which to compute the OSR frame size
 *
 * @return byte size of the OSR frame
 */
UDATA
osrFrameSizeRomMethod(J9ROMMethod *romMethod)
{
	U_32 numberOfLocals = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
	U_32 maxStack = J9_MAX_STACK_FROM_ROM_METHOD(romMethod);

	/* Adjust the number of locals to account for any hidden slots */
	if ((romMethod->modifiers & J9AccSynchronized) || (J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod))) {
		numberOfLocals += 1;
	}

	/* Account for the fixed size structure and each UDATA slot required */

	return sizeof(J9OSRFrame) + (sizeof(UDATA) * (maxStack + numberOfLocals));
}


/**
 * Compute the number of bytes required for all OSR frames in the buffer
 * representing the stack information at the current method in the walk state.
 *
 * @param[in] *currentThread the current J9VMThread
 * @param[in] *metaData the JIT metadata for the compilation
 * @param[in] *jitPC the compiled PC at which to decompile
 * @param[in] resolveFrameFlags the flags from the previous resolve frame (0 if no resolve frame)
 *
 * @return byte size of the OSR frames
 */
static UDATA
osrAllFramesSize(J9VMThread *currentThread, J9JITExceptionTable *metaData, void *jitPC, UDATA resolveFrameFlags)
{
	J9JavaVM *vm = currentThread->javaVM;
	UDATA totalSize = 0;
	void * stackMap = NULL;
	void * inlineMap = NULL;

	/* Count the inlined methods */
	jitGetMapsFromPC(vm, metaData, (UDATA)jitPC, &stackMap, &inlineMap);
	Assert_CodertVM_false(NULL == inlineMap);
	if (NULL != getJitInlinedCallInfo(metaData)) {
		void *inlinedCallSite = getFirstInlinedCallSite(metaData, inlineMap);
		if (inlinedCallSite != NULL) {
			UDATA inlineDepth = getJitInlineDepthFromCallSite(metaData, inlinedCallSite);
			do {
				J9Method * inlinedMethod = (J9Method *)getInlinedMethod(inlinedCallSite);
				totalSize += osrFrameSize(inlinedMethod);
				inlinedCallSite = getNextInlinedCallSite(metaData, inlinedCallSite);
				inlineDepth -= 1;
			} while (0 != inlineDepth);
		}
	}

	/* Count the outer method */
	totalSize += osrFrameSize(metaData->ramMethod);

	return totalSize;
}


/**
 * Perform an OSR (fill in the OSR buffer) for the frame represented in the stack walk state.
 *
 * Note that the JITted stack frame will be copied to (osrScratchBuffer + osrScratchBufferSize).
 * This function assumes that the buffer has been allocated large enough.
 *
 * @param[in] *currentThread current thread
 * @param[in] *walkState stack walk state (already at the OSR point)
 * @param[in] *osrBuffer pointer to correctly-allocated OSR buffer
 * @param[in] *osrScratchBuffer pointer to scratch buffer
 * @param[in] osrScratchBufferSize size of the scratch buffer
 * @param[in] jitStackFrameSize the number of bytes in the JITted stack frame
 * @param[out] mustDecompile if non-NULL, set to TRUE or FALSE to indicate if the OSR has performed a destructive action
 *
 * @return an OSR result code
 */
static UDATA
performOSR(J9VMThread *currentThread, J9StackWalkState *walkState, J9OSRBuffer *osrBuffer, U_8 *osrScratchBuffer, UDATA scratchBufferSize, UDATA jitStackFrameSize, UDATA *mustDecompile)
{
	J9JavaVM *vm = currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9VMThread *targetThread = walkState->walkThread;
	J9JITExceptionTable *metaData = walkState->jitInfo;
	void *pc = walkState->pc;
	void *osrBlock = NULL;
	UDATA result = OSR_OK;
	UDATA forceDecompile = FALSE;
	UDATA *osrJittedFrameCopy = (UDATA*)(osrScratchBuffer + scratchBufferSize);

	/* Assert that this frame has been compiled using OSR */
	Assert_CodertVM_true(usesOSR(currentThread, metaData));

	/* Assert that the live registers have been tracked */
	Assert_CodertVM_true(walkState->flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP);

	/* Copy the stack frame after the scratch buffer (assume it's been allocated correctly).
	 * Assert the number of bytes provided matches the expected frame size from the metadata
	 * (arguments + return address + totalFrameSize slots).
	 */
	Assert_CodertVM_true(jitStackFrameSize == ((J9_ARG_COUNT_FROM_ROM_METHOD(J9_ROM_METHOD_FROM_RAM_METHOD(metaData->ramMethod)) + 1 + metaData->totalFrameSize) * sizeof(UDATA)));
	memcpy(osrJittedFrameCopy, walkState->unwindSP, jitStackFrameSize);

	/* Perform the OSR */
	osrBlock = preOSR(currentThread, metaData, pc);
	/* Ensure the OSR block is within either the warm or cold region of the compilation */
	Assert_CodertVM_true(
			(((UDATA)osrBlock > metaData->startPC) && ((UDATA)osrBlock < metaData->endWarmPC)) ||
			((0 != metaData->startColdPC) && (((UDATA)osrBlock >= metaData->startColdPC) && ((UDATA)osrBlock < metaData->endPC)))
	);
	currentThread->osrBuffer = osrBuffer;
	currentThread->osrScratchBuffer = osrScratchBuffer;
	currentThread->osrJittedFrameCopy = osrJittedFrameCopy;
	currentThread->osrFrameIndex = sizeof(J9OSRBuffer);
	currentThread->privateFlags |= J9_PRIVATE_FLAGS_OSR_IN_PROGRESS;
	currentThread->javaVM->internalVMFunctions->jitFillOSRBuffer(currentThread, osrBlock);
	currentThread->privateFlags &= ~J9_PRIVATE_FLAGS_OSR_IN_PROGRESS;
	currentThread->osrBuffer = NULL;
	currentThread->osrJittedFrameCopy = NULL;
	if (0 != postOSR(currentThread, metaData, pc)) {
		forceDecompile = TRUE;
	}

	if (NULL != mustDecompile) {
		*mustDecompile = forceDecompile;
	}
	return result;
}


/**
 * Find the address of a local variable slot for a compiled method.  Requires that the target frame
 * be compiled with either FSD or OSR (or both).
 *
 * @param[in] *currentThread current thread
 * @param[in] *walkState stack walk state (already at the OSR point)
 * @param[in] slot the slot number to query
 * @param[in] inlineDepth the inline depth of the frame being queried
 *
 * @return the address of the local slot, NULL if a native out of memory occurs
 */
static UDATA *
jitLocalSlotAddress(J9VMThread * currentThread, J9StackWalkState *walkState, UDATA slot, UDATA inlineDepth)
{
	J9JITExceptionTable *metaData = walkState->jitInfo;
	UDATA *slotAddress = NULL;

	/* If this frame has been OSR-compiled, add a decompilation record (to perform the OSR) and return a pointer
	 * into the appropriate frame in the buffer.
	 */
	if (usesOSR(currentThread, metaData)) {
		J9JITDecompilationInfo *decompilationInfo = addDecompilation(currentThread, walkState, 0);

		if (NULL != decompilationInfo) {
			J9OSRBuffer *osrBuffer = &decompilationInfo->osrBuffer;
			J9OSRFrame *osrFrame = (J9OSRFrame*)(osrBuffer + 1);
			UDATA osrFrameInlineDepth = osrBuffer->numberOfFrames - 1;
			while (osrFrameInlineDepth != inlineDepth) {
				osrFrame = (J9OSRFrame*)((U_8*)osrFrame + osrFrameSize(osrFrame->method));
				osrFrameInlineDepth -= 1;
			}
			slotAddress = ((UDATA*)(osrFrame + 1)) + osrFrame->maxStack + osrFrame->numberOfLocals - 1 - slot;
		}
	} else {
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);

		/* OSR not in use in this frame - look up the stack slot using the FSD metadata */

		Assert_CodertVM_true(0 == inlineDepth);
		if (slot >= romMethod->argCount) {
			J9JITStackAtlas * stackAtlas = (J9JITStackAtlas *) (metaData->gcStackAtlas);

			slotAddress = (UDATA *) (((UDATA) walkState->bp) + stackAtlas->localBaseOffset);
			if (romMethod->modifiers & J9AccSynchronized) {
				/* Synchronized methods have one hidden temp to hold the sync object */
				++slotAddress;
			} else if (J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod)) {
				/* Non-empty java.lang.Object.<init> has one hidden temp to hold a copy of the receiver */
				++slotAddress;
			}
			slotAddress += metaData->tempOffset;	/* skip pending push area */
			slotAddress += ((romMethod->tempCount - 1) - (slot - romMethod->argCount));
		} else {
			slotAddress = walkState->arg0EA - slot;
		}
	}

	return slotAddress;
}


/**
 * Compute the required size of the OSR scratch buffer.
 *
 * @param[in] *currentThread current thread
 * @param[in] *metadata JIT metadata for this compilation
 * @param[in] *jitPC compiled PC at which to OSR
 *
 * @return the scratch buffer size
 */
static UDATA
roundedOSRScratchBufferSize(J9VMThread * currentThread, J9JITExceptionTable *metaData, void *jitPC)
{
	J9JavaVM *vm = currentThread->javaVM;
	UDATA scratchBufferSize = osrScratchBufferSize(currentThread, metaData, jitPC);

	/* Ensure a minimum size for the scratch buffer to allow for possible calls while the stack is active
	 * (in particular, Linux on x86 performs a call in order to compute the GOT).  Currently, the minimum
	 * size is 8 UDATAs.  Round the size of the scratch buffer up to UDATA.
	 */
	scratchBufferSize = OMR_MAX(scratchBufferSize, (8 * sizeof(UDATA)));
	scratchBufferSize = ROUND_TO(sizeof(UDATA), scratchBufferSize);
	return scratchBufferSize;
}


/**
 * Determine the address of an numbered object slot in a compiled stack frame.
 *
 * @param[in] *osrData the OSR data block
 * @param[in] slot the slot index, numbered from 0
 *
 * @return the address of the object slot in the compiled stack frame
 */
static j9object_t*
getObjectSlotAddress(J9OSRData *osrData, U_16 slot)
{
	UDATA *slotAddress = NULL;
	U_16 numParms = getJitNumberOfParmSlots(osrData->gcStackAtlas);

	/* The base address depends on the range of the slot index */
	if (slot < numParms) {
		slotAddress = osrData->objectArgScanCursor;
	} else {
		slotAddress = osrData->objectTempScanCursor;
		slot -= numParms;
	}
	slotAddress += slot;
	return (j9object_t*)slotAddress;
}


/**
 * Create a monitor enter record for each object locked in the OSR frame.  The records are linked
 * together and stored in the J9OSRFrame.
 *
 * @param[in] *currentThread current thread
 * @param[in] *osrData the OSR data block
 *
 * @return an OSR result code
 */
static UDATA
createMonitorEnterRecords(J9VMThread *currentThread, J9OSRData *osrData)
{
	UDATA result = OSR_OK;
	J9Pool *monitorEnterRecordPool = osrData->targetThread->monitorEnterRecordPool;
	if (NULL != monitorEnterRecordPool) {
		J9JITStackAtlas *gcStackAtlas = osrData->gcStackAtlas;
		void *inlinedCallSite = osrData->inlinedCallSite;
		U_8 *monitorMask = getMonitorMask(gcStackAtlas, inlinedCallSite);
		if (NULL != monitorMask) {
			J9MonitorEnterRecord listHead;
			J9MonitorEnterRecord *lastEnterRecord = &listHead;
			U_8 *liveMonitorMap = osrData->liveMonitorMap;
			U_16 numberOfMapBits = osrData->numberOfMapBits;
			U_16 i = 0;
			listHead.next = NULL;
			for (i = 0; i < numberOfMapBits; ++i) {
				U_8 bit = liveMonitorMap[i >> 3] & monitorMask[i >> 3] & (1 << (i & 7));
				if (0 != bit) {
					J9MonitorEnterRecord *newEnterRecord = NULL;
					j9object_t object = *getObjectSlotAddress(osrData, i);
					/* Assert that the mapped object is not stack-allocated */
					Assert_CodertVM_false(NULL == object);
					newEnterRecord = (J9MonitorEnterRecord*)pool_newElement(monitorEnterRecordPool);
					if (NULL == newEnterRecord) {
						/* Discard any records created up to this point */
						J9MonitorEnterRecord *freeRecord = listHead.next;
						while (NULL != freeRecord) {
							J9MonitorEnterRecord *nextRecord = freeRecord->next;
							pool_removeElement(monitorEnterRecordPool, freeRecord);
							freeRecord = nextRecord;
						}
						result = OSR_OUT_OF_MEMORY;
						break;
					}
					lastEnterRecord->next = newEnterRecord;
					lastEnterRecord = newEnterRecord;
					newEnterRecord->object = object;
					newEnterRecord->dropEnterCount = 1;
					newEnterRecord->arg0EA = NULL;
					newEnterRecord->next = NULL;
				}
			}
			osrData->osrFrame->monitorEnterRecords = listHead.next;
		}
	}
	return result;
}


/**
 * Initialize an OSR frame.
 *
 * @param[in] *currentThread the current J9VMThread
 * @param[in] *osrData the OSR data block
 *
 * @return an OSR result code
 */
static UDATA
initializeOSRFrame(J9VMThread *currentThread, J9OSRData *osrData)
{
	UDATA result = OSR_OK;
	J9OSRFrame *osrFrame = osrData->osrFrame;
	J9JITExceptionTable *metaData = osrData->metaData;
	void *inlineMap = osrData->inlineMap;
	void *inlinedCallSite = osrData->inlinedCallSite;
	J9Method *method = osrData->method;
	UDATA resolveFrameFlags = osrData->resolveFrameFlags;
	UDATA bytecodePCOffset = getCurrentByteCodeIndexAndIsSameReceiver(metaData, inlineMap, inlinedCallSite, NULL);
	U_8 *bytecodePC = J9_BYTECODE_START_FROM_RAM_METHOD(method) + bytecodePCOffset;
	UDATA pendingStackHeight = getPendingStackHeight(currentThread, bytecodePC, method, resolveFrameFlags);
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	U_8 argCount = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);
	U_32 numberOfLocals = argCount + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
	U_32 maxStack = J9_MAX_STACK_FROM_ROM_METHOD(romMethod);

	Trc_Decomp_jitInterpreterPCFromWalkState_Entry(osrData->jitPC);
	Trc_Decomp_jitInterpreterPCFromWalkState_stackMap(bytecodePC);

	/* Create monitor enter records */
	if (NULL != osrData->liveMonitorMap) {
		result = createMonitorEnterRecords(currentThread, osrData);
		if (OSR_OK != result) {
			goto done;
		}
	}
	/* Adjust the number of locals to account for any hidden slots */
	if ((romMethod->modifiers & J9AccSynchronized) || (J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod))) {
		numberOfLocals += 1;
	}
	osrFrame->method = method;
	osrFrame->bytecodePCOffset = bytecodePC  - J9_BYTECODE_START_FROM_RAM_METHOD(method);
	osrFrame->numberOfLocals = numberOfLocals;
	osrFrame->maxStack = maxStack;
	osrFrame->pendingStackHeight = pendingStackHeight;
	/* Move to the next frame */
	osrData->osrFrame = (J9OSRFrame*)(((UDATA*)(osrFrame + 1)) + maxStack + numberOfLocals);
done:
	return result;
}


/**
 * Initialize the OSR buffer and frames.
 *
 * @param[in] *currentThread current thread
 * @param[in] *osrBuffer OSR buffer
 * @param[in] *osrData the OSR data block
 */
static UDATA
initializeOSRBuffer(J9VMThread *currentThread, J9OSRBuffer *osrBuffer, J9OSRData *osrData)
{
	UDATA result = OSR_OK;
	J9JavaVM *vm = currentThread->javaVM;
	J9JITExceptionTable *metaData = osrData->metaData;
	void *jitPC = osrData->jitPC;
	UDATA resolveFrameFlags = osrData->resolveFrameFlags;
	UDATA numberOfFrames = 1; /* count the outer method now */
	J9Method *outerMethod = metaData->ramMethod;
	void *stackMap = NULL;
	void *inlineMap = NULL;
	J9JITStackAtlas *gcStackAtlas = NULL;
	U_8 *liveMonitorMap = NULL;
	U_16 numberOfMapBits = 0;

	/* Get the stack map, inline map and live monitor metadata */
	jitGetMapsFromPC(vm, metaData, (UDATA)jitPC, &stackMap, &inlineMap);
	liveMonitorMap = getJitLiveMonitors(metaData, stackMap);
	gcStackAtlas = (J9JITStackAtlas *)getJitGCStackAtlas(metaData);
	numberOfMapBits = getJitNumberOfMapBytes(gcStackAtlas) << 3;
	osrData->gcStackAtlas = gcStackAtlas;
	osrData->liveMonitorMap = liveMonitorMap;
	osrData->numberOfMapBits = numberOfMapBits;
	osrData->inlineMap = inlineMap;
	osrData->osrFrame = (J9OSRFrame*)(osrBuffer + 1);

	/* Fill in the data for the inlined methods */
	Assert_CodertVM_false(NULL == inlineMap);
	if (NULL != getJitInlinedCallInfo(metaData)) {
		void *inlinedCallSite = getFirstInlinedCallSite(metaData, inlineMap);
		if (inlinedCallSite != NULL) {
			UDATA inlineDepth = getJitInlineDepthFromCallSite(metaData, inlinedCallSite);
			numberOfFrames += inlineDepth;
			do {
				J9Method *inlinedMethod = (J9Method *)getInlinedMethod(inlinedCallSite);
				osrData->inlinedCallSite = inlinedCallSite;
				osrData->method = inlinedMethod;
				result = initializeOSRFrame(currentThread, osrData);
				if (OSR_OK != result) {
					goto done;
				}
				osrData->resolveFrameFlags = 0;
				inlinedCallSite = getNextInlinedCallSite(metaData, inlinedCallSite);
				inlineDepth -= 1;
			} while (0 != inlineDepth);
			Assert_CodertVM_true(NULL == inlinedCallSite);
		}
	}

	/* Fill in the frame for the outer method */
	osrData->inlinedCallSite = NULL;
	osrData->method = outerMethod;
	result = initializeOSRFrame(currentThread, osrData);
	if (OSR_OK != result) {
		goto done;
	}

	/* Fill in the OSR buffer header */
	osrBuffer->numberOfFrames = numberOfFrames;
	osrBuffer->jitPC = jitPC;
done:
	return result;
}


/**
 * Induce OSR on the current thread.
 *
 * @param[in] *currentThread current thread
 * @param[in] *jitPC compiled PC at which to OSR
 */
void 
induceOSROnCurrentThread(J9VMThread * currentThread)
{
	J9JavaVM * vm = currentThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9JITExceptionTable *metaData = NULL;
	void *jitPC = NULL;
	UDATA decompRecordSize = 0;
	UDATA scratchBufferSize = 0;
	UDATA jitStackFrameSize = 0;
	UDATA totalSize = 0;
	J9JITDecompilationInfo *decompilationRecord = NULL;
	UDATA reason = JITDECOMP_ON_STACK_REPLACEMENT;
	void *osrBlock = NULL;
	U_8 *osrScratchBuffer = NULL;
	UDATA osrReturnCode = OSR_OK;
	J9StackWalkState walkState;
	J9OSRData osrData;

	dumpStack(currentThread, "induceOSROnCurrentThread");

	/* The stack currently has a resolve frame on top, followed by the JIT frame we
	 * wish to OSR.  Walk the stack down to the JIT frame to gather the data required
	 * for decompilation.
	 */
	walkState.walkThread = currentThread;
	walkState.maxFrames = 2;
	walkState.flags = J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_MAINTAIN_REGISTER_MAP | J9_STACKWALK_COUNT_SPECIFIED;
	currentThread->javaVM->walkStackFrames(currentThread, &walkState);
	jitPC = walkState.pc;
	metaData = walkState.jitInfo;
	Assert_CodertVM_true(NULL != metaData);
	Assert_CodertVM_true(usesOSR(currentThread, metaData));

	/* Determine the required sizes of:
	 * 	a) a decompilation record (fixed size - a J9JITDecompilationInfo)
	 * 	b) the OSR frames
	 * 	c) the OSR scratch buffer
	 * 	d) the copy of the stack frame being OSRed
	 */
	decompRecordSize = osrAllFramesSize(currentThread, metaData, jitPC, walkState.resolveFrameFlags);
	decompRecordSize += sizeof(J9JITDecompilationInfo);
	scratchBufferSize = roundedOSRScratchBufferSize(currentThread, metaData, jitPC);
	jitStackFrameSize = (UDATA)(walkState.arg0EA + 1) - (UDATA)walkState.unwindSP;
	totalSize = decompRecordSize + scratchBufferSize + jitStackFrameSize;
	Assert_CodertVM_true(totalSize <= vm->osrGlobalBufferSize);

	/* Allocate the buffer, falling back to the global one if the allocation fails */
	decompilationRecord = (J9JITDecompilationInfo*)j9mem_allocate_memory(totalSize, OMRMEM_CATEGORY_JIT);
	if (NULL == decompilationRecord) {
		/* Trc_Decomp_induceOSROnCurrentThread_usingGlobalBuffer(currentThread); */
		omrthread_monitor_enter(vm->osrGlobalBufferLock);
		decompilationRecord = (J9JITDecompilationInfo*)vm->osrGlobalBuffer;
		reason |= JITDECOMP_OSR_GLOBAL_BUFFER_USED;
	}
	memset(decompilationRecord, 0, totalSize);
	decompilationRecord->usesOSR = TRUE;
	/* Trc_Decomp_addDecompilation_allocRecord(currentThread, info); */

	/* Fill in the interpreter values in the OSR buffer */
	osrData.targetThread = currentThread;
	osrData.metaData = metaData;
	osrData.jitPC = jitPC;
	osrData.resolveFrameFlags = walkState.resolveFrameFlags;
	osrData.objectArgScanCursor = getObjectArgScanCursor(&walkState);
	osrData.objectTempScanCursor = getObjectTempScanCursor(&walkState);
	if (OSR_OK != initializeOSRBuffer(currentThread, &decompilationRecord->osrBuffer, &osrData)) {
		Trc_Decomp_addDecompilation_allocFailed(currentThread);
		goto outOfMemory;
	}

	/* Perform the OSR to fill in the buffer.  On success, add the new decompilation to the stack.
	 * If the OSR fails, discard the decompilation - this will result in OutOfMemoryError being thrown.
	 */
	osrScratchBuffer = ((U_8*)decompilationRecord) + decompRecordSize;
	osrReturnCode = performOSR(currentThread, &walkState, &decompilationRecord->osrBuffer, osrScratchBuffer, scratchBufferSize, jitStackFrameSize, NULL);
	if (OSR_OK != osrReturnCode) {
outOfMemory:
		decompilationRecord->reason = reason;
		freeDecompilationRecord(currentThread, decompilationRecord, FALSE);
	} else {
		fixStackForNewDecompilation(currentThread, &walkState, decompilationRecord, reason, &currentThread->decompilationStack);
	}
}

/**
 * Ensure that the global OSR buffer is large enough for the given
 * input sizes.  If the buffer is too small, an attempt will be made
 * to grow it to the new size.
 *
 * @param[in] *vm the J9JavaVM
 * @param[in] osrFramesByteSize the size in bytes of the OSR frame data
 * @param[in] osrScratchBufferByteSize the size in bytes of the scratch buffer
 * @param[in] osrStackFrameByteSize the size in bytes of the JIT stack frame
 *
 * @return TRUE if the buffer is large enough, FALSE if it is not (and could not be grown)
 */
UDATA
ensureOSRBufferSize(J9JavaVM *vm, UDATA osrFramesByteSize, UDATA osrScratchBufferByteSize, UDATA osrStackFrameByteSize)
{
	UDATA result = TRUE;
	UDATA osrGlobalBufferSize = sizeof(J9JITDecompilationInfo);
	osrGlobalBufferSize += ROUND_TO(sizeof(UDATA), osrFramesByteSize);
	osrGlobalBufferSize += ROUND_TO(sizeof(UDATA), osrScratchBufferByteSize);
	osrGlobalBufferSize += ROUND_TO(sizeof(UDATA), osrStackFrameByteSize);
	if (osrGlobalBufferSize > vm->osrGlobalBufferSize) {
		omrthread_monitor_enter(vm->osrGlobalBufferLock);
		if (osrGlobalBufferSize > vm->osrGlobalBufferSize) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			void *newBuffer = j9mem_reallocate_memory(vm->osrGlobalBuffer, osrGlobalBufferSize, OMRMEM_CATEGORY_JIT);
			if (NULL == newBuffer) {
				result = FALSE;
			} else {
				vm->osrGlobalBufferSize = osrGlobalBufferSize;
				vm->osrGlobalBuffer = newBuffer;
			}
		}
		omrthread_monitor_exit(vm->osrGlobalBufferLock);
	}
	return result;
}

/* Resolve frame is already built */
void
c_jitDecompileAfterAllocation(J9VMThread *currentThread)
{
	/* Allocated object stored in floatTemp1 */
	j9object_t const obj = (j9object_t)currentThread->floatTemp1;
	/* Fetch and unstack the decompilation information for this frame */
	J9JITDecompilationInfo *decompRecord = fetchAndUnstackDecompilationInfo(currentThread);
	/* Fix the saved PC since the frame is still on the stack */
	fixSavedPC(currentThread, decompRecord);
	/* Decompile the method */
	jitDecompileMethod(currentThread, decompRecord);
	/* Push the newly-allocated object and advance the PC beyond the allocation bytecode */
	UDATA *sp = currentThread->sp - 1;
	*(j9object_t*)sp = obj;
	currentThread->sp = sp;
	currentThread->pc += (J9JavaInstructionSizeAndBranchActionTable[*currentThread->pc] & 0x7);
	dumpStack(currentThread, "after jitDecompileAfterAllocation");
	currentThread->tempSlot = (UDATA)J9_BUILDER_SYMBOL(executeCurrentBytecodeFromJIT);
}

/* Resolve frame is already built */
void
c_jitDecompileAtCurrentPC(J9VMThread *currentThread)
{
	/* Fetch and unstack the decompilation information for this frame */
	J9JITDecompilationInfo *decompRecord = fetchAndUnstackDecompilationInfo(currentThread);
	/* Fix the saved PC since the frame is still on the stack */
	fixSavedPC(currentThread, decompRecord);
	/* Decompile the method */
	jitDecompileMethod(currentThread, decompRecord);
	dumpStack(currentThread, "after jitDecompileAtCurrentPC");
	currentThread->tempSlot = (UDATA)J9_BUILDER_SYMBOL(executeCurrentBytecodeFromJIT);
}

/* Resolve frame is already built */
void
c_jitDecompileBeforeMethodMonitorEnter(J9VMThread *currentThread)
{
	/* Fetch and unstack the decompilation information for this frame */
	J9JITDecompilationInfo *decompRecord = fetchAndUnstackDecompilationInfo(currentThread);
	J9Method * const method = decompRecord->method;
	/* Fix the saved PC since the frame is still on the stack */
	fixSavedPC(currentThread, decompRecord);
	/* Decompile the method */
	jitDecompileMethod(currentThread, decompRecord);
	dumpStack(currentThread, "after jitDecompileBeforeMethodMonitorEnter");
	currentThread->floatTemp1 = (void*)method;
	currentThread->tempSlot = (UDATA)J9_BUILDER_SYMBOL(enterMethodMonitorFromJIT);
}

/* Resolve frame is already built */
void
c_jitDecompileBeforeReportMethodEnter(J9VMThread *currentThread)
{
	/* Fetch and unstack the decompilation information for this frame */
	J9JITDecompilationInfo *decompRecord = fetchAndUnstackDecompilationInfo(currentThread);
	J9Method * const method = decompRecord->method;
	/* Fix the saved PC since the frame is still on the stack */
	fixSavedPC(currentThread, decompRecord);
	/* Decompile the method */
	jitDecompileMethod(currentThread, decompRecord);
	dumpStack(currentThread, "after jitDecompileBeforeReportMethodEnter");
	currentThread->floatTemp1 = (void*)method;
	currentThread->tempSlot = (UDATA)J9_BUILDER_SYMBOL(reportMethodEnterFromJIT);
}

/* Resolve frame is already built */
void
c_jitDecompileAfterMonitorEnter(J9VMThread *currentThread)
{
	/* Fetch and unstack the decompilation information for this frame */
	J9JITDecompilationInfo *decompRecord = fetchAndUnstackDecompilationInfo(currentThread);
	/* Fix the saved PC since the frame is still on the stack */
	fixSavedPC(currentThread, decompRecord);
	/* Decompile the method */
	jitDecompileMethod(currentThread, decompRecord);
	/* See if this was actually the monitor enter for an inlined synchronized method.  If the current
	 * bytecode is not JBmonitorenter, we are at the start of a method (JBmonitorenter can not appear
	 * at bytecode 0 in any verified method).
	 */
	if (JBmonitorenter != *currentThread->pc) {
		dumpStack(currentThread, "after jitDecompileAfterMonitorEnter - inlined sync method");
		currentThread->floatTemp1 = (void*)currentThread->literals;
		currentThread->tempSlot = (UDATA)J9_BUILDER_SYMBOL(reportMethodEnterFromJIT);
	} else {
		/* Advance the PC past the monitorenter and resume execution */
		currentThread->pc += 1;
		dumpStack(currentThread, "after jitDecompileAfterMonitorEnter - JBmonitorenter");
		currentThread->tempSlot = (UDATA)J9_BUILDER_SYMBOL(executeCurrentBytecodeFromJIT);
	}
}

void
c_jitDecompileOnReturn(J9VMThread *currentThread)
{
	UDATA const slots = currentThread->tempSlot;
	/* Fetch and unstack the decompilation information for this frame */
	J9JITDecompilationInfo *decompRecord = fetchAndUnstackDecompilationInfo(currentThread);
	/* Simulate a call to a resolve helper to make the stack walkable */
	buildBranchJITResolveFrame(currentThread, decompRecord->pc, 0);
	/* Decompile the method */
	jitDecompileMethod(currentThread, decompRecord);
	/* Copy return value to stack */
	currentThread->sp -= slots;
	memmove(currentThread->sp, &currentThread->returnValue, slots * sizeof(UDATA));
	/* Advance the PC past the invoke and resume execution */
	currentThread->pc += 3;
	dumpStack(currentThread, "after jitDecompileOnReturn");
	currentThread->tempSlot = (UDATA)J9_BUILDER_SYMBOL(executeCurrentBytecodeFromJIT);
}

void
c_jitReportExceptionCatch(J9VMThread *currentThread)
{
	void *jitPC = currentThread->floatTemp4;
	J9JavaVM * const vm = currentThread->javaVM;
	/* Simulate a call to a resolve helper to make the stack walkable */
	buildBranchJITResolveFrame(currentThread, jitPC, J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE);
	/* If there was a decompilation record for the frame which is catching the exception, the PC will be
	 * pointing to jitDecompileAtExceptionCatch.  In this case, update the decompilation record so that the
	 * pcAddress points to the PC save location in the newly-pushed resolve frame.
	 */
	if (J9_BUILDER_SYMBOL(jitDecompileAtExceptionCatch) == jitPC) {
		currentThread->decompilationStack->pcAddress = (U_8**)&(((J9SFJITResolveFrame*)currentThread->sp)->returnAddress);
	}
	/* Report the event */
	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_EXCEPTION_CATCH)) {
		j9object_t exception = ((J9SFJITResolveFrame*)currentThread->sp)->savedJITException;
		ALWAYS_TRIGGER_J9HOOK_VM_EXCEPTION_CATCH(vm->hookInterface, currentThread, exception, NULL);
		if (VM_VMHelpers::immediateAsyncPending(currentThread)) {
			if (J9_CHECK_ASYNC_POP_FRAMES == vm->internalVMFunctions->javaCheckAsyncMessages(currentThread, FALSE)) {
				jitPC = J9_BUILDER_SYMBOL(handlePopFramesFromJIT);
				goto done;
			}
		}
		/* Do not cache the resolve frame pointer as the hook call may modify the SP value */
		jitPC = ((J9SFJITResolveFrame*)currentThread->sp)->returnAddress;
	}
	/* Continue running */
	restoreBranchJITResolveFrame(currentThread);
done:
	currentThread->tempSlot = (UDATA)jitPC;
}

UDATA
jitIsMethodBreakpointed(J9VMThread *currentThread, J9Method *method)
{
	return J9_ARE_ANY_BITS_SET((UDATA)method->constantPool, J9_STARTPC_METHOD_BREAKPOINTED);
}

} /* extern "C" */

