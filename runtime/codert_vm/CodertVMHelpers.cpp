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

#include "j9protos.h"
#include "jitprotos.h"
#include "stackwalk.h"
#include "rommeth.h"
#include "VMHelpers.hpp"
#include "AtomicSupport.hpp"
#include "MethodMetaData.h"
#include "ut_j9codertvm.h"

extern "C" {

/* Until the JIT deletes references to these */
void * jitExitInterpreterX = NULL;
void * jitExitInterpreterY = NULL;
#if defined(J9VM_ENV_CALL_VIA_TABLE)
void init_codert_vm_fntbl(J9JavaVM *javaVM) {}
#endif /* J9VM_ENV_CALL_VIA_TABLE */

void
initializeDirectJNI(J9JavaVM *vm)
{
	vm->jniSendTarget = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_AND_SEND_JNI_NATIVE);
}

#if defined(J9SW_NEEDS_JIT_2_INTERP_THUNKS)
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendPatchupVirtual);
#else /* J9SW_NEEDS_JIT_2_INTERP_THUNKS */
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtual0);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtual1);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualJ);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualF);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualD);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualL);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualSync0);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualSync1);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualSyncJ);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualSyncF);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualSyncD);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendVirtualSyncL);
J9_EXTERN_BUILDER_SYMBOL(icallVMprJavaSendNativeVirtual);
#endif /* J9SW_NEEDS_JIT_2_INTERP_THUNKS */
J9_EXTERN_BUILDER_SYMBOL(jitFillOSRBufferReturn);
J9_EXTERN_BUILDER_SYMBOL(returnFromJIT0);
J9_EXTERN_BUILDER_SYMBOL(returnFromJIT1);
J9_EXTERN_BUILDER_SYMBOL(returnFromJITJ);
J9_EXTERN_BUILDER_SYMBOL(returnFromJITF);
J9_EXTERN_BUILDER_SYMBOL(returnFromJITD);
#if defined(J9VM_ENV_DATA64)
J9_EXTERN_BUILDER_SYMBOL(returnFromJITL);
#endif /* J9VM_ENV_DATA64 */
J9_EXTERN_BUILDER_SYMBOL(returnFromJITConstructor0);
J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreter0);
J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreter1);
J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreterJ);
J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreterF);
J9_EXTERN_BUILDER_SYMBOL(jitExitInterpreterD);

static void *i2jReturnTable[16] = {0};

void *jit2InterpreterSendTargetTable[13] = {0};

static J9Method* jitGetExceptionCatcher(J9VMThread *currentThread, void *handlerPC, J9JITExceptionTable *metaData, IDATA *location);

void
initializeCodertFunctionTable(J9JavaVM *javaVM)
{
	J9JITConfig *jitConfig = javaVM->jitConfig;
	jitConfig->i2jReturnTable = (void*)&i2jReturnTable;
	i2jReturnTable[0] = J9_BUILDER_SYMBOL(returnFromJIT0);
	i2jReturnTable[1] = J9_BUILDER_SYMBOL(returnFromJIT1);
	i2jReturnTable[2] = J9_BUILDER_SYMBOL(returnFromJITJ);
	i2jReturnTable[3] = J9_BUILDER_SYMBOL(returnFromJITF);
	i2jReturnTable[4] = J9_BUILDER_SYMBOL(returnFromJITD);
#if defined(J9VM_ENV_DATA64)
	i2jReturnTable[5] = J9_BUILDER_SYMBOL(returnFromJITL);
#else /* J9VM_ENV_DATA64 */
	i2jReturnTable[5] = J9_BUILDER_SYMBOL(returnFromJIT1);
#endif /* J9VM_ENV_DATA64 */
	i2jReturnTable[6] = J9_BUILDER_SYMBOL(returnFromJITF);
	i2jReturnTable[7] = J9_BUILDER_SYMBOL(returnFromJITD);
	i2jReturnTable[8] = J9_BUILDER_SYMBOL(returnFromJITConstructor0);
#if defined(J9SW_NEEDS_JIT_2_INTERP_THUNKS)
	void *patchupVirtual = J9_BUILDER_SYMBOL(icallVMprJavaSendPatchupVirtual);
	jitConfig->patchupVirtual = patchupVirtual;
	for (UDATA i = 0; i < (sizeof(jit2InterpreterSendTargetTable) / sizeof(void*)); ++i) {
		jit2InterpreterSendTargetTable[i] = patchupVirtual;
	}
#else /* J9SW_NEEDS_JIT_2_INTERP_THUNKS */
	jit2InterpreterSendTargetTable[0] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtual0);
	jit2InterpreterSendTargetTable[1] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtual1);
	jit2InterpreterSendTargetTable[2] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualJ);
	jit2InterpreterSendTargetTable[3] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualF);
	jit2InterpreterSendTargetTable[4] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualD);
	jit2InterpreterSendTargetTable[5] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualL);
	jit2InterpreterSendTargetTable[6] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualSync0);
	jit2InterpreterSendTargetTable[7] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualSync1);
	jit2InterpreterSendTargetTable[8] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualSyncJ);
	jit2InterpreterSendTargetTable[9] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualSyncF);
	jit2InterpreterSendTargetTable[10] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualSyncD);
	jit2InterpreterSendTargetTable[11] = J9_BUILDER_SYMBOL(icallVMprJavaSendVirtualSyncL);
	jit2InterpreterSendTargetTable[12] = J9_BUILDER_SYMBOL(icallVMprJavaSendNativeVirtual);
#endif /* J9SW_NEEDS_JIT_2_INTERP_THUNKS */
	jitConfig->j2iInvokeWithArguments = (void*)-1;
	jitConfig->jitFillOSRBufferReturn = J9_BUILDER_SYMBOL(jitFillOSRBufferReturn);
	jitConfig->jitExitInterpreter0 = J9_BUILDER_SYMBOL(jitExitInterpreter0);
	jitConfig->jitExitInterpreter1 = J9_BUILDER_SYMBOL(jitExitInterpreter1);
	jitConfig->jitExitInterpreterJ = J9_BUILDER_SYMBOL(jitExitInterpreterJ);
	jitConfig->jitExitInterpreterF = J9_BUILDER_SYMBOL(jitExitInterpreterF);
	jitConfig->jitExitInterpreterD = J9_BUILDER_SYMBOL(jitExitInterpreterD);
	jitConfig->i2jTransition = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_I2J_TRANSITION);
	jitConfig->setUpForDLT = setUpForDLT;
	jitConfig->i2jMHTransition = (void*)-3;
	jitConfig->runJITHandler = (void*)-5;
	jitConfig->performDLT = (void*)-7;
	jitConfig->jitGetExceptionCatcher = jitGetExceptionCatcher;
	initPureCFunctionTable(javaVM);
}

#if defined(J9VM_JIT_GC_ON_RESOLVE_SUPPORT)

static void
jitEmptyObjectSlotIterator(J9VMThread *vmThread, J9StackWalkState *walkState, j9object_t *objectSlot, const void *stackLocation)
{
}

void
jitCheckScavengeOnResolve(J9VMThread *currentThread)
{
	UDATA oldState = currentThread->omrVMThread->vmState;
	currentThread->omrVMThread->vmState = J9VMSTATE_SNW_STACK_VALIDATE;
	J9JavaVM *vm = currentThread->javaVM;
	J9JITConfig *jitConfig = vm->jitConfig;
	jitConfig->gcCount += 1;
	if (jitConfig->gcCount >= jitConfig->gcOnResolveThreshold) {
		if (jitConfig->gcCount == jitConfig->gcOnResolveThreshold) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			j9tty_printf(PORTLIB, "\n<JIT: scavenge on resolve enabled gc #%d>", jitConfig->gcCount);
		}
		J9StackWalkState walkState;
		walkState.objectSlotWalkFunction = jitEmptyObjectSlotIterator;
		walkState.walkThread = currentThread;
		walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS;
		vm->walkStackFrames(currentThread, &walkState);
	}
	currentThread->omrVMThread->vmState = oldState;
}

#endif /* J9VM_JIT_GC_ON_RESOLVE_SUPPORT */

void
jitMarkMethodReadyForDLT(J9VMThread *currentThread, J9Method *method)
{
	VM_AtomicSupport::bitOr((UDATA*)&method->constantPool, J9_STARTPC_DLT_READY);
}

void
jitMethodFailedTranslation(J9VMThread *currentThread, J9Method *method)
{
	J9JITConfig *jitConfig = currentThread->javaVM->jitConfig;
	if (J9_ARE_NO_BITS_SET(jitConfig->runtimeFlags, J9JIT_TOSS_CODE)) {
		/* Natives are already set correctly before the translation attempt */
		if (J9_ARE_NO_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers, J9AccNative)) {
			method->extra = (void*)(UDATA)(IDATA)J9_JIT_NEVER_TRANSLATE;
		}
	}
}

void
jitMethodTranslated(J9VMThread *currentThread, J9Method *method, void *jitStartAddress)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9JITConfig *jitConfig = vm->jitConfig;
	if (J9_ARE_NO_BITS_SET(jitConfig->runtimeFlags, J9JIT_TOSS_CODE)) {
		if (jitMethodIsBreakpointed(currentThread, method)) {
			jitBreakpointedMethodCompiled(currentThread, method, jitStartAddress);
		}
		/* Slam in the start PC, which also marks the method as having been translated */
		method->extra = jitStartAddress;
		method->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_I2J_TRANSITION);
		/* update vTables for virtual methods */
		if (J9ROMMETHOD_HAS_VTABLE(J9_ROM_METHOD_FROM_RAM_METHOD(method))) {
			J9Class *currentClass = J9_CLASS_FROM_METHOD(method);
			if (J9ROMCLASS_IS_INTERFACE(currentClass->romClass)) {
				currentClass = J9VMJAVALANGOBJECT_OR_NULL(vm);
			}
			UDATA initialClassDepth = VM_VMHelpers::getClassDepth(currentClass);
			void *j2jAddress = VM_VMHelpers::jitToJitStartAddress(jitStartAddress);
			do {
				J9VTableHeader* vTableHeader = J9VTABLE_HEADER_FROM_RAM_CLASS(currentClass);

				/* get number of real methods in Interpreter vTable */
				UDATA vTableWriteIndex = vTableHeader->size;
				if (0 != vTableWriteIndex) {
					/* initialize pointer to first real vTable method */
					void **vTableWriteCursor = (void**)JIT_VTABLE_START_ADDRESS(currentClass);
					J9Method **vTableReadCursor = J9VTABLE_FROM_HEADER(vTableHeader);

					while (0 != vTableWriteIndex) {
						if (method == *vTableReadCursor) {
							*vTableWriteCursor = j2jAddress;
						}
						vTableReadCursor += 1;
						vTableWriteCursor -= 1;
						vTableWriteIndex -= 1;
					}
				}
				currentClass = currentClass->subclassTraversalLink;
			} while (VM_VMHelpers::getClassDepth(currentClass) > initialClassDepth);
		}
	}
}

#if defined(J9VM_JIT_NEW_INSTANCE_PROTOTYPE)
J9_EXTERN_BUILDER_SYMBOL(jitTranslateNewInstanceMethod);
J9_EXTERN_BUILDER_SYMBOL(jitInterpretNewInstanceMethod);
#endif /* J9VM_JIT_NEW_INSTANCE_PROTOTYPE */

void*
jitNewInstanceMethodStartAddress(J9VMThread *currentThread, J9Class *clazz)
{
	void *addr = NULL;
#if defined(J9VM_JIT_NEW_INSTANCE_PROTOTYPE)
	addr = (void*)clazz->romableAotITable;
	if (addr == J9_BUILDER_SYMBOL(jitTranslateNewInstanceMethod)) {
		addr = NULL;
	}
#endif /* J9VM_JIT_NEW_INSTANCE_PROTOTYPE */
	return addr;
}

void
jitNewInstanceMethodTranslated(J9VMThread *currentThread, J9Class *clazz, void *jitStartAddress)
{
#if defined(J9VM_JIT_NEW_INSTANCE_PROTOTYPE)
	clazz->romableAotITable = (UDATA)VM_VMHelpers::jitToJitStartAddress(jitStartAddress);
#endif /* J9VM_JIT_NEW_INSTANCE_PROTOTYPE */
}

void
jitNewInstanceMethodTranslateFailed(J9VMThread *currentThread, J9Class *clazz)
{
#if defined(J9VM_JIT_NEW_INSTANCE_PROTOTYPE)
	clazz->romableAotITable = (UDATA)J9_BUILDER_SYMBOL(jitInterpretNewInstanceMethod);
#endif /* J9VM_JIT_NEW_INSTANCE_PROTOTYPE */
}

UDATA
jitTranslateMethod(J9VMThread *currentThread, J9Method *method)
{
	UDATA oldState = currentThread->omrVMThread->vmState;
	currentThread->omrVMThread->vmState = J9VMSTATE_JIT_CODEGEN;
	J9JavaVM *vm = currentThread->javaVM;
	J9JITConfig *jitConfig = vm->jitConfig;
	UDATA jitStartPC = jitConfig->entryPoint(jitConfig, currentThread, method, 0);
	currentThread->omrVMThread->vmState = oldState;
	return jitStartPC;
}

UDATA
jitUpdateCount(J9VMThread *currentThread, J9Method *method, UDATA oldCount, UDATA newCount)
{
	return oldCount == VM_AtomicSupport::lockCompareExchange((UDATA*)&method->extra, oldCount, newCount);
}

typedef void (*overrideCallback)(J9VMThread*, UDATA, J9Method*, J9Method*);

void
jitUpdateInlineAttribute(J9VMThread *currentThread, J9Class * classPtr, void *jitCallBack)
{
	/* Do not walk interface classes */
	if (J9_ARE_NO_BITS_SET(classPtr->romClass->modifiers, J9AccInterface)) {
		J9Class *superclass = VM_VMHelpers::getSuperclass(classPtr);
		/* Methods in Object never override anything */
		if (NULL != superclass) {
			/* Skip the count field and the first method in the table (not a real method) */
			J9VTableHeader *superVTableHeader = J9VTABLE_HEADER_FROM_RAM_CLASS(superclass);
			UDATA methodCount = superVTableHeader->size;
			J9Method **superMethods = J9VTABLE_FROM_HEADER(superVTableHeader);
			J9Method **subMethods = J9VTABLE_FROM_RAM_CLASS(classPtr);
			/* Walk all methods which could possibly be overridden */
			while (0 != methodCount) {
				J9Method *superMethod = *superMethods;
				J9Method *subMethod = *subMethods;
				/* If the subclass method is different from the superclass method, this is an override */
				if (superMethod != subMethod) {
					if (NULL != jitCallBack) {
						((overrideCallback)jitCallBack)(currentThread, 0, superMethod, subMethod);
					}
					/* Mark the super method as overridden */
					VM_AtomicSupport::bitOr((UDATA*)superMethod->constantPool, J9_STARTPC_METHOD_IS_OVERRIDDEN);
				}
				superMethods += 1;
				subMethods += 1;
				methodCount -= 1;
			}

		}
	}
}

static J9Method*
jitGetExceptionCatcher(J9VMThread *currentThread, void *handlerPC, J9JITExceptionTable *metaData, IDATA *location)
{
	J9Method *method = metaData->ramMethod;
	void *stackMap = NULL;
	void *inlineMap = NULL;
	void *inlinedCallSite = NULL;
	/* Note we need to add 1 to the JIT PC here in order to get the correct map at the exception handler
	 * because jitGetMapsFromPC is expecting a return address, so it subtracts 1.  The value passed in is
	 * the start address of the compiled exception handler.
	 */
	jitGetMapsFromPC(currentThread->javaVM, metaData, (UDATA)handlerPC + 1, &stackMap, &inlineMap);
	Assert_CodertVM_false(NULL == inlineMap);
	if (NULL != getJitInlinedCallInfo(metaData)) {
		inlinedCallSite = getFirstInlinedCallSite(metaData, inlineMap);
		if (NULL != inlinedCallSite) {
			method = (J9Method*)getInlinedMethod(inlinedCallSite);
		}
	}
	*location = (IDATA)getCurrentByteCodeIndexAndIsSameReceiver(metaData, inlineMap, inlinedCallSite, NULL);
	return method;
}

} /* extern "C" */
