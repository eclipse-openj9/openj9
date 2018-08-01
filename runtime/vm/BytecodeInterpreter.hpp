/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#if !defined(BYTECODEINTERPRETER_HPP_)
#define BYTECODEINTERPRETER_HPP_

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j9vmnls.h"
#include "j9jclnls.h"
#include "j9bcvnls.h"
#include "bcnames.h"
#include "lockNurseryUtil.h"
#include "rommeth.h"
#include "stackwalk.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "jni.h"
#define FFI_BUILDING /* Needed on Windows to link libffi statically */ 
#include "ffi.h"
#include "jitregmap.h"
#include "j2sever.h"
#include "vmaccess.h"

#include "ArrayCopyHelpers.hpp"
#include "AtomicSupport.hpp"
#include "BytecodeAction.hpp"
#include "MHInterpreter.hpp"
#include "ObjectAccessBarrierAPI.hpp"
#include "ObjectHash.hpp"
#include "VMHelpers.hpp"
#include "VMAccess.hpp"
#include "ObjectAllocationAPI.hpp"
#include "OutOfLineINL.hpp"
#include "UnsafeAPI.hpp"
#include "ObjectMonitor.hpp"
#include "JITInterface.hpp"

#if 0
#define DEBUG_MUST_HAVE_VM_ACCESS(vmThread) Assert_VM_mustHaveVMAccess(vmThread)
#else
#define DEBUG_MUST_HAVE_VM_ACCESS(vmThread)
#endif

#define DO_INTERPRETER_PROFILING
#if defined(DEBUG_VERSION)
#define DO_HOOKS
#define DO_SINGLE_STEP
#define INTERPRETER_CLASS VM_DebugBytecodeInterpreter
#else
#define INTERPRETER_CLASS VM_BytecodeInterpreter
#endif

typedef enum {
	VM_NO,
	VM_YES,
	VM_MAYBE
} VM_YesNoMaybe;

#define LOCAL_PC
#define LOCAL_SP

#if defined(LOCAL_CURRENT_THREAD)
#define CURRENT_THREAD J9VMThread * &_currentThread
#define CURRENT_THREAD_PARAM _currentThread
#endif

#if defined(LOCAL_ARG0EA)
#define ARG0EA UDATA * &_arg0EA
#define ARG0EA_PARAM _arg0EA
#endif

#if defined(LOCAL_SP)
#define SP UDATA * &_sp
#define SP_PARAM _sp
#endif

#if defined(LOCAL_PC)
#define PC U_8 * &_pc
#define PC_PARAM _pc
#endif

#if defined(LOCAL_LITERALS)
#define LITERALS J9Method * &_literals
#define LITERALS_PARAM _literals
#endif

#define REGISTER_ARGS_LIST SP, PC
#define REGISTER_ARGS SP_PARAM, PC_PARAM

#if defined(DEBUG_VERSION)
#define DEBUG_UPDATE_VMSTRUCT() updateVMStruct(REGISTER_ARGS)
#else
#define DEBUG_UPDATE_VMSTRUCT()
#endif

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
extern "C" {
extern void CEEJNIWrapper(J9VMThread *currentThread);
}
#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */

class INTERPRETER_CLASS
{
/*
 * Data members
 */
private:
	J9JavaVM * const _vm;
#if !defined(LOCAL_CURRENT_THREAD)
	J9VMThread * const _currentThread;
#endif
#if !defined(LOCAL_ARG0EA)
	UDATA *_arg0EA;
#endif
#if !defined(LOCAL_SP)
	UDATA *_sp;
#endif
#if !defined(LOCAL_PC)
	U_8 *_pc;
#endif
#if !defined(LOCAL_LITERALS)
	J9Method *_literals;
#endif
	UDATA _nextAction;
	J9Method *_sendMethod;
#if defined(DO_SINGLE_STEP)
	bool _skipSingleStep;
#endif
	MM_ObjectAllocationAPI _objectAllocate;
	MM_ObjectAccessBarrierAPI _objectAccessBarrier;

protected:

public:

/*
 * Function members
 */
private:
	/**
	 * Run a methodHandle using the MethodHandle interpreter/
	 * @param methodHandle[in] The MethodHandle to run
	 * @return the next action to take.
	 */
	VMINLINE VM_BytecodeAction
	interpretMethodHandle(REGISTER_ARGS_LIST, j9object_t methodHandle)
	{
		updateVMStruct(REGISTER_ARGS);
		VM_MHInterpreter mhInterpreter(_currentThread, &_objectAllocate, &_objectAccessBarrier);
		VM_BytecodeAction next = mhInterpreter.dispatchLoop(methodHandle);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		return next;
	}

	/**
	 * Modify the MH.invocationCount so that invocations from the interpreter don't
	 * actually modify the counts or affect CustomThunk compilation.
	 * Shareable thunk compilation is based on the ThunkTuple counting.
	 * Shareable thunks then modify the MH.invocationCount on each invocation.  We
	 * only want invocations from jitted code to drive CustomThunk compilation so the
	 * interpreter needs to pre-emptively negate the modification to the invocationCount.
	 *
	 * @param methodHandle[in] The MethodHandle to modify the coun on
	 */
	VMINLINE void
	modifyMethodHandleCountForI2J(REGISTER_ARGS_LIST, j9object_t methodHandle)
	{
		/* Decrement the MH.invocationCount as the shareableThunks increment it.  We only want
		 * invocations from compiled code to be counted.
		 */
		I_32 count = J9VMJAVALANGINVOKEMETHODHANDLE_INVOCATIONCOUNT(_currentThread, methodHandle);
		J9VMJAVALANGINVOKEMETHODHANDLE_SET_INVOCATIONCOUNT(_currentThread, methodHandle, count - 1);
	}

#if defined(DO_SINGLE_STEP)
	void VMINLINE
	skipNextSingleStep()
	{
		if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_SINGLE_STEP)) {
			_skipSingleStep = true;
		}
	}

	VM_BytecodeAction VMINLINE
	singleStep(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = FALL_THROUGH;
		if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_SINGLE_STEP)) {
			if (_skipSingleStep) {
				_skipSingleStep = false;
			} else {
				updateVMStruct(REGISTER_ARGS);
				ALWAYS_TRIGGER_J9HOOK_VM_SINGLE_STEP(_vm->hookInterface, _currentThread, _literals, _pc - _literals->bytecodes);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				if (JBbreakpoint == *_pc) {
					rc = GOTO_EXECUTE_BREAKPOINTED_BYTECODE;
				} else if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
				}
			}
		}
		return rc;
	}
#endif

	/* Profiling records:
	 *
	 * J9ProfilingBytecodeRecord
	 * 		U_8* _pc;
	 * 		J9ProfilingBytecodeBranchRecord
	 * 			U_8 taken;
	 * 		J9ProfilingBytecodeCastRecord
	 * 			J9Class *instanceClass;
	 * 		J9ProfilingBytecodeSwitchRecord
	 * 			U_32 index;
	 * 		J9ProfilingBytecodeMethodEnterExitRecord (_pc == 0 for enter, 1 for exit, 2 for exit due to throw)
	 * 			J9Method *method;
	 * 		J9ProfilingBytecodeInvokeRecord
	 * 			J9Class *receiverClass;
	 * 			J9Method *callingMethod;
	 * 			J9Method *targetMethod;
	 * 		J9ProfilingBytecodeCallingMethodRecord
	 * 			J9Method *callingMethod;
	 */
	VMINLINE U_8*
	startProfilingRecord(REGISTER_ARGS_LIST, UDATA dataSize)
	{
#if defined(DO_INTERPRETER_PROFILING)
retry:
		U_8 *profilingCursor = NULL;
		if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL)) {
			IDATA count = (IDATA)(UDATA)_literals->extra;
			if ((count > 0) && (count <= (IDATA)_currentThread->maxProfilingCount)) {
#if defined(DEBUG_VERSION)
				/* Do not profile breakpointed methods because the pc points to memory
				 * which will go away when the breakpoint is removed.
				 */
				if (!methodIsBreakpointed(_literals))
#endif /* DEBUG_VERSION */
				{
					U_8 *nextRecord = _currentThread->profilingBufferCursor;
					profilingCursor = nextRecord;
					nextRecord += (sizeof(U_8*) + dataSize);
					if (nextRecord >= _currentThread->profilingBufferEnd) {
						updateVMStruct(REGISTER_ARGS);
						flushBytecodeProfilingData(_currentThread);
						goto retry;
					} else {
						_currentThread->profilingBufferCursor = nextRecord;
						*(U_8**)profilingCursor = _pc;
						profilingCursor += sizeof(U_8*);
					}
				}
			}
		}
		return profilingCursor;
#else
		return NULL;
#endif
	}

	VMINLINE void
	profileCallingMethod(REGISTER_ARGS_LIST)
	{
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(J9Method*));
		if (NULL != profilingCursor) {
			*(J9Method**)profilingCursor = _literals;
		}
	}

	VMINLINE void
	profileInvokeReceiver(REGISTER_ARGS_LIST, J9Class *clazz, J9Method *callingMethod, J9Method *targetMethod)
	{
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(J9Class*) + 2*sizeof(J9Method*));
		if (NULL != profilingCursor) {
#if defined(J9VM_ARCH_ARM) && !defined(J9VM_ENV_DATA64)
			/* ARMv8 does not support store multiple to unaligned addresses, and unfortunately GCC
			 * generates a store multiple for the normal version of these three assignments.
			 * To avoid the store multiple instructions, we can replace the assignments with
			 * inline assembly code.
			 */
			asm (
				"str %[clazz],  [%[cursor], #0]\n\t"
				"str %[method], [%[cursor], #4]\n\t"
				"str %[target], [%[cursor], #8]\n\t"
				:            // No outputs
				:[cursor] "r" (profilingCursor),
				 [clazz]  "r" (clazz),
				 [method] "r" (callingMethod),
				 [target] "r" (targetMethod)
			 );
#else /* defined(J9VM_ARCH_ARM) && !defined(J9VM_ENV_DATA64) */
			*(J9Class**)profilingCursor = clazz;
			profilingCursor += sizeof(J9Class*);
			*(J9Method**)profilingCursor = callingMethod;
			profilingCursor += sizeof(J9Method*);
			*(J9Method**)profilingCursor = targetMethod;
#endif /* defined(J9VM_ARCH_ARM) && !defined(J9VM_ENV_DATA64) */
		}
	}

	VMINLINE void
	profileCast(REGISTER_ARGS_LIST, J9Class *clazz)
	{
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(J9Class*));
		if (NULL != profilingCursor) {
			*(J9Class**)profilingCursor = clazz;
		}
	}

	VMINLINE void
	profileSwitch(REGISTER_ARGS_LIST, I_32 index)
	{
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(I_32));
		if (NULL != profilingCursor) {
			*(I_32*)profilingCursor = index;
		}
	}

	VMINLINE void
	updateVMStruct(REGISTER_ARGS_LIST)
	{
		J9VMThread *const thread = _currentThread;
		thread->arg0EA = _arg0EA;
		thread->sp = _sp;
		thread->pc = _pc;
		thread->literals = _literals;
	}

	VMINLINE void
	VMStructHasBeenUpdated(REGISTER_ARGS_LIST)
	{
		J9VMThread const *const thread = _currentThread;
		_arg0EA = thread->arg0EA;
		_sp = thread->sp;
		_pc = thread->pc;
		_literals = thread->literals;
	}

	VMINLINE UDATA*
	buildSpecialStackFrame(REGISTER_ARGS_LIST, UDATA type, UDATA flags, bool visible)
	{
		*--_sp = (UDATA)_arg0EA | (visible ? 0 : J9SF_A0_INVISIBLE_TAG);
		UDATA *bp = _sp;
		*--_sp = (UDATA)_pc;
		*--_sp = (UDATA)_literals;
		*--_sp = (flags);
		_pc = (U_8*)(type);
		_literals = NULL;
		return bp;
	}

	VMINLINE void
	buildGenericSpecialStackFrame(REGISTER_ARGS_LIST, UDATA flags)
	{
		_arg0EA = buildSpecialStackFrame(REGISTER_ARGS, J9SF_FRAME_TYPE_GENERIC_SPECIAL, flags, false);
	}

	VMINLINE UDATA*
	buildMethodFrame(REGISTER_ARGS_LIST, J9Method *method, UDATA flags)
	{
		UDATA *bp = buildSpecialStackFrame(REGISTER_ARGS, J9SF_FRAME_TYPE_METHOD, flags, false);
		*--_sp = (UDATA)method;
		_arg0EA = bp + J9_ROM_METHOD_FROM_RAM_METHOD(method)->argCount;
		return bp;
	}

	VMINLINE UDATA
	jitStackFrameFlags(REGISTER_ARGS_LIST, UDATA constantFlags)
	{
		UDATA flags = _currentThread->jitStackFrameFlags;
		_currentThread->jitStackFrameFlags = 0;
		return flags | constantFlags;
	}

	VMINLINE void
	restoreGenericSpecialStackFrame(REGISTER_ARGS_LIST)
	{
		_sp = _arg0EA + 1;
		_literals = (J9Method*)(_sp[-3]);
		_pc = (U_8*)(_sp[-2]);
		_arg0EA = (UDATA*)(_sp[-1] & ~(UDATA)J9SF_A0_INVISIBLE_TAG);
	}

	VMINLINE void
	restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS_LIST, UDATA *bp)
	{
		_sp = bp + 1;
		_literals = (J9Method*)(bp[-2]);
		_pc = (U_8*)(bp[-1]);
		_arg0EA = (UDATA*)(bp[0] & ~(UDATA)J9SF_A0_INVISIBLE_TAG);
	}

	VMINLINE void
	restoreSpecialStackFrameAndDrop(REGISTER_ARGS_LIST, UDATA *bp)
	{
		_sp = _arg0EA + 1;
		_literals = (J9Method*)(bp[-2]);
		_pc = (U_8*)(bp[-1]);
		_arg0EA = (UDATA*)(bp[0] & ~(UDATA)J9SF_A0_INVISIBLE_TAG);
	}

	VMINLINE UDATA*
	buildInternalNativeStackFrame(REGISTER_ARGS_LIST)
	{
		UDATA *bp = buildSpecialStackFrame(REGISTER_ARGS, J9SF_FRAME_TYPE_NATIVE_METHOD, jitStackFrameFlags(REGISTER_ARGS, 0), true);
		*--_sp = (UDATA)_sendMethod;
		_arg0EA = bp + J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod)->argCount;
		return bp;
	}

	VMINLINE void
	restoreInternalNativeStackFrame(REGISTER_ARGS_LIST)
	{
		J9SFNativeMethodFrame *nativeMethodFrame = (J9SFNativeMethodFrame*)_sp;
		_currentThread->jitStackFrameFlags = nativeMethodFrame->specialFrameFlags & J9_SSF_JIT_NATIVE_TRANSITION_FRAME;
		restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, ((UDATA*)(nativeMethodFrame + 1)) - 1);
	}

	VMINLINE J9SFJNINativeMethodFrame*
	recordJNIReturn(REGISTER_ARGS_LIST, UDATA *bp)
	{
		J9SFJNINativeMethodFrame *frame = ((J9SFJNINativeMethodFrame*)(bp + 1)) - 1;
		UDATA flags = frame->specialFrameFlags;
		if (flags & J9_SSF_JNI_REFS_REDIRECTED) {
			freeStacks(_currentThread, bp);
		}
		if (flags & J9_SSF_CALL_OUT_FRAME_ALLOC) {
			jniPopFrame(_currentThread, JNIFRAME_TYPE_INTERNAL);
		}
		return frame;
	}

	/**
	 * Unwind a J9SFMethodTypeFrame from the stack.
	 *
	 * @param frame[in] A pointer to the J9SFMethodTypeFrame
	 * @param spPriorToFrameBuild[in] The stack pointer prior to building the frame.
	 */
	VMINLINE void
	restoreMethodTypeFrame(REGISTER_ARGS_LIST, J9SFMethodTypeFrame *frame, UDATA *spPriorToFrameBuild)
	{
		_literals = frame->savedCP;
		_pc = frame->savedPC;
		_arg0EA = UNTAGGED_A0(frame);
		_sp = spPriorToFrameBuild;
	}

	VMINLINE UDATA*
	bpForCurrentBytecodedMethod(REGISTER_ARGS_LIST)
	{
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_literals);
		UDATA localCount = romMethod->argCount + romMethod->tempCount;
		if (romMethod->modifiers & J9AccSynchronized) {
			localCount += 1;
		}
		if (J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod)) {
			localCount += 1;
		}
		return _arg0EA - localCount;
	}

	VMINLINE void
	pushObjectInSpecialFrame(REGISTER_ARGS_LIST, j9object_t object)
	{
		_sp -= 1;
		_literals = (J9Method*)((UDATA)_literals + sizeof(object));
		*(j9object_t*)_sp = object;
	}

	VMINLINE j9object_t
	popObjectInSpecialFrame(REGISTER_ARGS_LIST)
	{
		j9object_t object = *(j9object_t*)_sp;
		_sp += 1;
		_literals = (J9Method*)((UDATA)_literals - sizeof(object));
		return object;
	}

	VMINLINE VM_BytecodeAction
	i2jTransition(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = RUN_METHOD_INTERPRETED;
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod);
		void* const jitStartAddress = _sendMethod->extra;
		if (startAddressIsCompiled((UDATA)jitStartAddress)) {
			/* If we are single stepping, or about to run a breakpointed method, fall back to the interpreter.
			 * Check FSD enabled first, to minimize the number of extra instructions in the normal execution path
			 * and because the breakpoint bit is re-used by Z/OS as the offload bit for native methods.  Natives are
			 * not currently compiled when FSD is enabled - if they ever are, a check will need to be added here.
			 */
			J9JITConfig *jitConfig = _vm->jitConfig;
			if (jitConfig->fsdEnabled) {
				if (!methodCanBeRunCompiled(_sendMethod)) {
					goto done;
				}
			}
			/* If we got here straight from JIT, jump directly to the method - do not follow the interpreter path */
			if (0 != _currentThread->jitStackFrameFlags) {
				_currentThread->jitStackFrameFlags = 0;
				rc = promotedMethodOnTransitionFromJIT(REGISTER_ARGS, (void*)_literals, jitStartAddress);
				goto done;
			}
			/* Run the compiled code */
			rc = jitTransition(REGISTER_ARGS, romMethod->argCount, jitStartAddress);
		}
done:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	i2jMHTransition(REGISTER_ARGS_LIST)
	{
		/* VMThread->tempSlot will hold the MethodHandle.
		 * VMThread->floatTemp1 will hold the compiledEntryPoint
		 */
		void* const jitStartAddress = _currentThread->floatTemp1;
		j9object_t methodHandle = (j9object_t)_currentThread->tempSlot;
		j9object_t methodType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(_currentThread, methodHandle);
		/* Currently FSD should be disabling compilation of MethodHandles */
		J9JITConfig *jitConfig = _vm->jitConfig;
		Assert_VM_false(jitConfig->fsdEnabled);
		/* Add one to MethodType->argSlots to account for the MethodHandle receiver */
		return jitTransition(REGISTER_ARGS, J9VMJAVALANGINVOKEMETHODTYPE_ARGSLOTS(_currentThread, methodType) + 1, jitStartAddress);
	}

	VMINLINE VM_BytecodeAction
	j2iTransition(REGISTER_ARGS_LIST)
	{
		VM_JITInterface::disableRuntimeInstrumentation(_currentThread);
		VM_BytecodeAction rc = GOTO_RUN_METHOD;
		void* const jitReturnAddress = VM_JITInterface::fetchJITReturnAddress(_currentThread, _sp);
		J9ROMMethod* const romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod);
		void* const exitPoint = j2iReturnPoint(J9ROMMETHOD_SIGNATURE(romMethod));
		if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccNative | J9AccAbstract)) {
			_literals = (J9Method*)jitReturnAddress;
			_pc = nativeReturnBytecodePC(REGISTER_ARGS, romMethod);
#if defined(J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP)
			/* Variable frame */
			_arg0EA = NULL;
#else /* J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP */
			/* Fixed frame - remember the SP so it can be reset upon return from the native */
			_arg0EA = _sp;
#endif /* J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP */
			/* Set the flag indicating that the caller was the JIT */
			_currentThread->jitStackFrameFlags = J9_SSF_JIT_NATIVE_TRANSITION_FRAME;
			/* If a stop request has been posted, handle it instead of running the native */
			if (J9_ARE_ANY_BITS_SET(_currentThread->publicFlags, J9_PUBLIC_FLAGS_STOP)) {
				buildMethodFrame(REGISTER_ARGS, _sendMethod, jitStackFrameFlags(REGISTER_ARGS, 0));
				_currentThread->currentException = _currentThread->stopThrowable;
				_currentThread->stopThrowable = NULL;
				VM_VMAccess::clearPublicFlagsNoMutex(_currentThread, J9_PUBLIC_FLAGS_STOP);
				omrthread_clear_priority_interrupted();
				rc = GOTO_THROW_CURRENT_EXCEPTION;
			}
		} else {
			bool decompileOccurred = false;
			_pc = (U_8*)jitReturnAddress;
			UDATA preCount = 0;
			UDATA postCount = 0;
			UDATA result = 0;
			do {
				preCount = (UDATA)_sendMethod->extra;
				postCount = preCount - _currentThread->jitCountDelta;
				if (J9_ARE_NO_BITS_SET(preCount, J9_STARTPC_NOT_TRANSLATED)) {
					/* Already compiled */
					break;
				}
				if ((IDATA)preCount < 0) {
					/* This method should not be translated (already enqueued, already failed, etc) */
					break;
				}
				if ((IDATA)postCount < 0) {
					/* Attempt to compile the method */
					_arg0EA = _sp;
					_literals = (J9Method*)_pc;
					UDATA *bp = buildMethodFrame(REGISTER_ARGS, _sendMethod, J9_SSF_JIT_NATIVE_TRANSITION_FRAME);
					updateVMStruct(REGISTER_ARGS);
					/* this call cannot change bp as no java code is run */
					UDATA oldState = pushVMState(REGISTER_ARGS, J9VMSTATE_JIT_CODEGEN);
					J9JITConfig *jitConfig = _vm->jitConfig;
					jitConfig->entryPoint(jitConfig, _currentThread, _sendMethod, 0);
					popVMState(REGISTER_ARGS, oldState);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, bp);
					if (MASK_PC(_pc) != MASK_PC(_literals)) {
						decompileOccurred = true;
						_pc = (U_8*)_literals;
					}
					/* If the method is now compiled, run it compiled, otherwise run it bytecoded */
					UDATA const jitStartAddress = (UDATA)_sendMethod->extra;
					if (startAddressIsCompiled(jitStartAddress)) {
						if (methodCanBeRunCompiled(_sendMethod)) {
							if (decompileOccurred) {
								/* The return address is not currently on the stack.  It will be pushed into the
								 * next stack slot.
								 */
								_currentThread->decompilationStack->pcAddress = (U_8**)(_sp - 1);
							}
							rc = promotedMethodOnTransitionFromJIT(REGISTER_ARGS, (void*)_pc, (void*)jitStartAddress);
							goto done;
						}
					}
					break;
				}
				result = VM_AtomicSupport::lockCompareExchange((UDATA*)&_sendMethod->extra, preCount, postCount);
				/* If count updates, run method interpreted, else loop around and try again */
			} while (result != preCount);
			/* Run the method interpreted */
			_currentThread->jitStackFrameFlags = 0;
			{
				UDATA stackUse = VM_VMHelpers::calculateStackUse(romMethod, sizeof(J9SFJ2IFrame));
				UDATA *checkSP = _sp - stackUse;
				if ((checkSP > _sp) || VM_VMHelpers::shouldGrowForSP(_currentThread, checkSP)) {
					checkSP -= (sizeof(J9SFMethodFrame) / sizeof(UDATA));
					UDATA currentUsed = (UDATA)_currentThread->stackObject->end - (UDATA)checkSP;
					UDATA maxStackSize = _vm->stackSize;
					_arg0EA = _sp;
					_literals = (J9Method*)_pc;
					buildMethodFrame(REGISTER_ARGS, _sendMethod, J9_SSF_JIT_NATIVE_TRANSITION_FRAME);
					updateVMStruct(REGISTER_ARGS);
					if (currentUsed > maxStackSize) {
throwStackOverflow:
						if (J9_ARE_ANY_BITS_SET(_currentThread->privateFlags, J9_PRIVATE_FLAGS_STACK_OVERFLOW)) {
							// vmStruct already up-to-date in all paths to here
							fatalRecursiveStackOverflow(_currentThread);
						}
						setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGSTACKOVERFLOWERROR, NULL);
						VMStructHasBeenUpdated(REGISTER_ARGS);
						rc = GOTO_THROW_CURRENT_EXCEPTION;
						goto done;
					}
					currentUsed += _vm->stackSizeIncrement;
					if (currentUsed > maxStackSize) {
						currentUsed = maxStackSize;
					}
					if (0 != growJavaStack(_currentThread, currentUsed)) {
						goto throwStackOverflow;
					}
					VMStructHasBeenUpdated(REGISTER_ARGS);
					UDATA *bp = ((UDATA*)(((J9SFMethodFrame*)_sp) + 1)) - 1;
					restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, bp);
					if (MASK_PC(_pc) != MASK_PC(_literals)) {
						decompileOccurred = true;
						_pc = (U_8*)_literals;
					}
				}
			}
#if defined(J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP)
			/* Variable frame - return SP should pop the arguments */
			_arg0EA = _sp + romMethod->argCount;
#else /* J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP */
			/* Fixed frame - remember the SP so it can be reset upon return to the JIT */
			_arg0EA = _sp;
#endif /* J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP */
			_literals = (J9Method*)exitPoint;
			rc = inlineSendTarget(REGISTER_ARGS, VM_MAYBE, VM_MAYBE, VM_MAYBE, VM_MAYBE, true, decompileOccurred);
		}
done:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	promotedMethodOnTransitionFromJIT(REGISTER_ARGS_LIST, void *returnAddress, void *jumpAddress)
	{
		VM_JITInterface::restoreJITReturnAddress(_currentThread, _sp, returnAddress);
		_currentThread->tempSlot =  (UDATA)jumpAddress;
		_nextAction = J9_BCLOOP_LOAD_PRESERVED_AND_BRANCH;
		VM_JITInterface::enableRuntimeInstrumentation(_currentThread);
		return GOTO_DONE;
	}

	VMINLINE J9Method*
	j2iVirtualMethod(REGISTER_ARGS_LIST, j9object_t receiver, UDATA interfaceVTableIndex)
	{
		void* const jitReturnAddress = VM_JITInterface::peekJITReturnAddress(_currentThread, _sp);
		UDATA jitVTableOffset = VM_JITInterface::jitVTableIndex(jitReturnAddress, interfaceVTableIndex);
		UDATA vTableOffset = sizeof(J9Class) - jitVTableOffset;
		J9Class *clazz = J9OBJECT_CLAZZ(_currentThread, receiver);
		return *(J9Method**)((UDATA)clazz + vTableOffset);
	}

	VMINLINE void
	restorePreservedRegistersFromWalkState(REGISTER_ARGS_LIST, J9StackWalkState *walkState)
	{
		J9VMEntryLocalStorage* const els = _currentThread->entryLocalStorage;
		UDATA* const jitGlobalStorageBase = els->jitGlobalStorageBase;
		UDATA* const * const preservedBase = (UDATA**)&walkState->registerEAs;
		for (UDATA i = 0; i < J9SW_JIT_CALLEE_PRESERVED_SIZE; ++i) {
			U_8 const registerNumber = jitCalleeSavedRegisterList[i];
			jitGlobalStorageBase[registerNumber] = *(preservedBase[registerNumber]);
		}
	}

	VMINLINE VM_BytecodeAction
	retFromNativeHelper(REGISTER_ARGS_LIST, UDATA slots, void *jitReturn)
	{
		memmove(&_currentThread->returnValue, _sp, slots * sizeof(UDATA));
#if defined(J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP)
		/* Variable frame - drop the return value */
		_sp += slots;
#else /* J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP */
		/* Fixed frame - reset sp to it's value when the call occurred */
		_sp = _arg0EA;
#endif /* J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP */
		_currentThread->jitStackFrameFlags = 0;
		_currentThread->floatTemp1 = (void*)_literals;
		_currentThread->tempSlot = (UDATA)jitReturn;
		_nextAction = J9_BCLOOP_LOAD_PRESERVED_AND_BRANCH;
		return GOTO_DONE;
	}

	VMINLINE VM_BytecodeAction
	fillOSRBuffer(REGISTER_ARGS_LIST, void *osrBlock)
	{
		_sp = (UDATA*)_currentThread->osrJittedFrameCopy;
		_currentThread->osrReturnAddress = _vm->jitConfig->jitFillOSRBufferReturn;
		_currentThread->tempSlot = (UDATA)osrBlock;
		_nextAction = J9_BCLOOP_LOAD_PRESERVED_AND_BRANCH;
		return GOTO_DONE;
	}

	VMINLINE VM_BytecodeAction
	returnFromJIT(REGISTER_ARGS_LIST, UDATA slotCount, bool isConstructor)
	{
		VM_JITInterface::disableRuntimeInstrumentation(_currentThread);
		if (isConstructor) {
			VM_AtomicSupport::writeBarrier();
		}
		J9I2JState *i2jState = &_currentThread->entryLocalStorage->i2jState;
		_sp = UNTAG2(i2jState->returnSP, UDATA*) - slotCount;
		_arg0EA = i2jState->a0;
		_literals= i2jState->literals;
		_pc = i2jState->pc + 3;
		memmove(_sp, &_currentThread->floatTemp1, sizeof(UDATA) * slotCount);
		_currentThread->jitStackFrameFlags = 0;
#if defined(TRACE_TRANSITIONS)
		char currentMethodName[1024];
		PORT_ACCESS_FROM_JAVAVM(_vm);
		getMethodName(PORTLIB, _literals, _pc, currentMethodName);
		switch(slotCount) {
		case 0:
			j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_RETURN_FROM_JIT %s %s\n", _currentThread, currentMethodName, isConstructor ? "ctor" : "void");
			break;
		case 1:
			j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_RETURN_FROM_JIT %s value=0x%zx\n", _currentThread, currentMethodName, *_sp);
			break;
		case 2:
			j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_RETURN_FROM_JIT %s value=0x%llx\n", _currentThread, currentMethodName, *(U_64*)_sp);
			break;
		default:
			Assert_VM_unreachable();
		}
#endif /* TRACE_TRANSITIONS */
		return EXECUTE_BYTECODE;
	}

	VMINLINE VM_BytecodeAction
	jitTransition(REGISTER_ARGS_LIST, UDATA argCount, void *jitStartAddress)
	{
		UDATA returnSP = ((UDATA)(_sp + argCount)) | J9_STACK_FLAGS_J2_IFRAME;
		/* Align the java stack such that sp is double-slot aligned upon entry to the JIT code */
		if (J9_ARE_ANY_BITS_SET((UDATA)_sp, sizeof(UDATA))) {
			_sp -= 1;
			memmove(_sp, _sp + 1, sizeof(UDATA) * argCount);
			returnSP |= J9_STACK_FLAGS_ARGS_ALIGNED;
		}
		J9I2JState *i2jState = &_currentThread->entryLocalStorage->i2jState;
		i2jState->returnSP = (UDATA*)returnSP;
		i2jState->a0 = _arg0EA;
		i2jState->literals = _literals;
		i2jState->pc = _pc;
		U_32 returnTypeIndex = ((U_32*)jitStartAddress)[-1] & 0xF;
		void *returnPoint = ((void**)_vm->jitConfig->i2jReturnTable)[returnTypeIndex];
		// TODO: not loading receiver for I2J
		// TODO: not loading pseudoTOC
		return promotedMethodOnTransitionFromJIT(REGISTER_ARGS, returnPoint, jitStartAddress);
	}

	VMINLINE void*
	j2iReturnPoint(J9UTF8 *signature)
	{
		J9JITConfig* const jitConfig = _vm->jitConfig;
		void *returnPoint = jitConfig->jitExitInterpreter1;
		U_16 length = J9UTF8_LENGTH(signature);
		U_8 *data = J9UTF8_DATA(signature);
		U_8 sigChar = data[length - 1];
		if ('[' == data[length - 2]) {
			goto obj;
		}
		switch(sigChar) {
		case 'V':
			returnPoint = jitConfig->jitExitInterpreter0;
			break;
		case ';':
obj:;
		/* On 32-bit, object uses the "1" target (already loaded, so just break).
		 * On 64-bit, object uses the "J" target (fall through)
		 */
#if !defined(J9VM_ENV_DATA64)
			break;
#endif /* !J9VM_ENV_DATA64 */
		case 'J':
			returnPoint = jitConfig->jitExitInterpreterJ;
			break;
		case 'F':
			returnPoint = jitConfig->jitExitInterpreterF;
			break;
		case 'D':
			returnPoint = jitConfig->jitExitInterpreterD;
			break;
		}
		return returnPoint;
	}

	VMINLINE void*
	j2iReturnPoint(J9Class *returnType)
	{
		J9JITConfig* const jitConfig = _vm->jitConfig;
		void *returnPoint = jitConfig->jitExitInterpreter1;
		J9ROMClass* const romClass = returnType->romClass;
		if (J9ROMCLASS_IS_PRIMITIVE_TYPE(romClass)) {
			if (returnType == _vm->voidReflectClass) {
				returnPoint = jitConfig->jitExitInterpreter0;
			} else if (returnType == _vm->longReflectClass) {
				returnPoint = jitConfig->jitExitInterpreterJ;
			} else if (returnType == _vm->floatReflectClass) {
				returnPoint = jitConfig->jitExitInterpreterF;
			} else if (returnType == _vm->doubleReflectClass) {
				returnPoint = jitConfig->jitExitInterpreterD;
			}
		} else {
			/* On 32-bit, object uses the "1" target (already loaded).
			 * On 64-bit, object uses the "J" target
			 */
#if defined(J9VM_ENV_DATA64)
			returnPoint = jitConfig->jitExitInterpreterJ;
#endif /* !J9VM_ENV_DATA64 */
		}
		return returnPoint;
	}

	VMINLINE void
	fillInJ2IValues(J9SFJ2IFrame* const j2iFrame, void* const exitPoint, void* const jitReturnAddress, UDATA *returnSP)
	{
		J9VMEntryLocalStorage* const els = _currentThread->entryLocalStorage;
		UDATA* const jitGlobalStorageBase = els->jitGlobalStorageBase;
		UDATA* const j2iPreservedBase = ((UDATA*)&j2iFrame->previousJ2iFrame) + 1;
		j2iFrame->i2jState = els->i2jState;
		j2iFrame->previousJ2iFrame = _currentThread->j2iFrame;
		for (UDATA i = 0; i < J9SW_JIT_CALLEE_PRESERVED_SIZE; ++i) {
			j2iPreservedBase[i] = jitGlobalStorageBase[jitCalleeSavedRegisterList[i]];
		}
		j2iFrame->specialFrameFlags = J9_SSF_JIT_CALLIN;
		j2iFrame->exitPoint = exitPoint;
		j2iFrame->returnAddress = (U_8*)jitReturnAddress;
		j2iFrame->taggedReturnSP = returnSP;
		_currentThread->j2iFrame = (UDATA*)&j2iFrame->taggedReturnSP;
	}

	VMINLINE void
	restoreJ2IValues(J9SFJ2IFrame* const j2iFrame)
	{
		J9VMEntryLocalStorage* const els = _currentThread->entryLocalStorage;
		UDATA* const jitGlobalStorageBase = els->jitGlobalStorageBase;
		UDATA* const j2iPreservedBase = ((UDATA*)&j2iFrame->previousJ2iFrame) + 1;
		els->i2jState = j2iFrame->i2jState;
		_currentThread->j2iFrame = j2iFrame->previousJ2iFrame;
		for (UDATA i = 0; i < J9SW_JIT_CALLEE_PRESERVED_SIZE; ++i) {
			jitGlobalStorageBase[jitCalleeSavedRegisterList[i]] = j2iPreservedBase[i];
		}
	}

	VMINLINE VM_BytecodeAction
	j2iReturn(REGISTER_ARGS_LIST)
	{
		memmove(&_currentThread->returnValue, _sp, sizeof(U_64));
		J9SFJ2IFrame* const j2iFrame = ((J9SFJ2IFrame*)(_currentThread->j2iFrame + 1)) - 1;
		restoreJ2IValues(j2iFrame);
		_sp = UNTAG2(j2iFrame->taggedReturnSP, UDATA*);
		_currentThread->jitStackFrameFlags = 0;
		_currentThread->floatTemp1 = (void*)j2iFrame->returnAddress;
		_currentThread->tempSlot = (UDATA)j2iFrame->exitPoint;
		_nextAction = J9_BCLOOP_LOAD_PRESERVED_AND_BRANCH;
		return GOTO_DONE;
	}

	VMINLINE U_8*
	nativeReturnBytecodePC(REGISTER_ARGS_LIST, J9ROMMethod* const romMethod)
	{
		static const U_8 returnFromNativeBytecodes[][4] = {
			{ JBinvokestatic, 0, 0, JBretFromNative0 }, /* void */
			{ JBinvokestatic, 0, 0, JBretFromNative1 }, /* boolean */
			{ JBinvokestatic, 0, 0, JBretFromNative1 }, /* byte */
			{ JBinvokestatic, 0, 0, JBretFromNative1 }, /* char */
			{ JBinvokestatic, 0, 0, JBretFromNative1 }, /* short */
			{ JBinvokestatic, 0, 0, JBretFromNativeF }, /* float */
			{ JBinvokestatic, 0, 0, JBretFromNative1 }, /* int */
			{ JBinvokestatic, 0, 0, JBretFromNativeD }, /* double */
			{ JBinvokestatic, 0, 0, JBretFromNativeJ }, /* long */
#if defined(J9VM_ENV_DATA64)
			{ JBinvokestatic, 0, 0, JBretFromNativeJ }, /* object */
#else /* J9VM_ENV_DATA64 */
			{ JBinvokestatic, 0, 0, JBretFromNative1 }, /* object */
#endif /* J9VM_ENV_DATA64 */
		};
		U_8 *bytecodes = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
		return (U_8*)returnFromNativeBytecodes[bytecodes[1]];
	}

	VMINLINE VM_BytecodeAction
	j2iInvokeExact(REGISTER_ARGS_LIST, j9object_t methodHandle)
	{
		VM_JITInterface::disableRuntimeInstrumentation(_currentThread);
		VM_BytecodeAction rc = GOTO_RUN_METHODHANDLE;
		static U_8 const bcReturnFromJ2I[] = { JBinvokestatic, 0, 0, JBreturnFromJ2I };
		void* const jitReturnAddress = VM_JITInterface::fetchJITReturnAddress(_currentThread, _sp);
		j9object_t methodType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(_currentThread, methodHandle);
		j9object_t returnType = J9VMJAVALANGINVOKEMETHODTYPE_RETURNTYPE(_currentThread, methodType);
		void* const exitPoint = j2iReturnPoint(J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, returnType));
		/* Get the argument count from the MethodHandle */
		UDATA argCount = J9VMJAVALANGINVOKEMETHODTYPE_ARGSLOTS(_currentThread, methodType) + 1; /* argSlots does not include the receiver of the invokeExact */
		/* Decrement the MH invocationCount if we are going to run jitted as the shareable
		 * thunks will increment the count.  We only want shareableThunks called from the
		 * JIT to modify the MH invocationCount. We will increment the count later if we
		 * didn't run the MH compiled.
		 */
		J9VMJAVALANGINVOKEMETHODHANDLE_SET_INVOCATIONCOUNT(_currentThread, methodHandle, J9VMJAVALANGINVOKEMETHODHANDLE_INVOCATIONCOUNT(_currentThread, methodHandle) - 1);
#if defined(J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP)
		/* Variable frame - return SP should pop the arguments */
		UDATA *returnSP = _sp + argCount;
#else /* J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP */
		/* Fixed frame - remember the SP so it can be reset upon return to the JIT */
		UDATA *returnSP = _sp;
#endif /* J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP */
		/* Buy the space for the J2I frame and shift the arguments down */
		UDATA *spOnEntry = _sp;
		_sp = (UDATA*)(((J9SFJ2IFrame*)_sp) - 1);
		memmove(_sp, spOnEntry, argCount * sizeof(UDATA));
		/* Fill in the J2I frame and set up the return information */
		J9SFJ2IFrame* const j2iFrame = (J9SFJ2IFrame*)(_sp + argCount);
		fillInJ2IValues(j2iFrame, exitPoint, jitReturnAddress, (UDATA*)((UDATA)returnSP | J9SF_A0_INVISIBLE_TAG));
		_arg0EA = _currentThread->j2iFrame;
		_pc = (U_8*)bcReturnFromJ2I;
		_literals = NULL;
		_currentThread->jitStackFrameFlags = 0;
		/* Protect the pending args to the MH */
		UDATA *spPriorToMethodTypeFrame = _sp;
		updateVMStruct(REGISTER_ARGS);
		buildMethodTypeFrame(_currentThread, methodType);
		/* If a stop request has been posted, handle it instead of running the MH */
		if (J9_ARE_ANY_BITS_SET(_currentThread->publicFlags, J9_PUBLIC_FLAGS_STOP)) {
			VMStructHasBeenUpdated(REGISTER_ARGS);
			_currentThread->currentException = _currentThread->stopThrowable;
			_currentThread->stopThrowable = NULL;
			VM_VMAccess::clearPublicFlagsNoMutex(_currentThread, J9_PUBLIC_FLAGS_STOP);
			omrthread_clear_priority_interrupted();
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		} else {
			/* Ask the JIT to compile the MH */
			void *i2jThunkAddress = NULL;
			J9JITConfig* const jitConfig = _vm->jitConfig;
			if (NULL != jitConfig->translateMethodHandle) {
				i2jThunkAddress = jitConfig->translateMethodHandle(_currentThread, methodHandle, NULL, J9_METHOD_HANDLE_COMPILE_SHARED);
				/* Compiler can release vmaccess: refetch the MH from the stack */
				methodHandle = ((j9object_t*)j2iFrame)[-1];
			}
			VMStructHasBeenUpdated(REGISTER_ARGS);
			/* Pop the MethodType frame */
			restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, ((UDATA*)(((J9SFMethodTypeFrame*)_sp) + 1)) - 1);
			_sp = spPriorToMethodTypeFrame;
			if (NULL != i2jThunkAddress) {
				/* Run the MH compiled - unwind the J2I frame and move the args back */
				restoreJ2IValues(j2iFrame);
				memmove(spOnEntry, _sp, argCount * sizeof(UDATA));
				_sp = spOnEntry;
				rc = promotedMethodOnTransitionFromJIT(REGISTER_ARGS, jitReturnAddress, i2jThunkAddress);
			} else {
				/* Not running jitted, increment the count - this means the count will not have changed. */
				J9VMJAVALANGINVOKEMETHODHANDLE_SET_INVOCATIONCOUNT(_currentThread, methodHandle, J9VMJAVALANGINVOKEMETHODHANDLE_INVOCATIONCOUNT(_currentThread, methodHandle) + 1);
				/* Stash MethodHandle into tempSlot where MHInterpreter expects to find it */
				_currentThread->tempSlot = (UDATA)methodHandle;
			}
		}
		return rc;
	}

	/**
	 * Perform a non-instrumentable allocation of a non-indexable class.
	 * If inline allocation fails, the out of line allocator will be called.
	 * This function assumes that the stack and live values are in a valid state for GC.
	 *
	 * @param j9clazz[in] the non-indexable J9Class to instantiate
	 * @param initializeSlots[in] whether or not to initialize the slots (default true)
	 * @param memoryBarrier[in] whether or not to issue a write barrier (default true)
	 *
	 * @returns the new object, or NULL if allocation failed
	 */
	VMINLINE j9object_t
	allocateObject(REGISTER_ARGS_LIST, J9Class *clazz, bool initializeSlots = true, bool memoryBarrier = true)
	{
		j9object_t instance = _objectAllocate.inlineAllocateObject(_currentThread, clazz, initializeSlots, memoryBarrier);
		if (NULL == instance) {
			updateVMStruct(REGISTER_ARGS);
			instance = _vm->memoryManagerFunctions->J9AllocateObject(_currentThread, clazz, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			VMStructHasBeenUpdated(REGISTER_ARGS);
		}
		return instance;
	}

	/**
	 * Perform a non-instrumentable allocation of an indexable class.
	 * If inline allocation fails, the out of line allocator will be called.
	 * This function assumes that the stack and live values are in a valid state for GC.
	 *
	 * @param arrayClass[in] the indexable J9Class to instantiate
	 * @param size[in] the desired size of the array
	 * @param initializeSlots[in] whether or not to initialize the slots (default true)
	 * @param memoryBarrier[in] whether or not to issue a write barrier (default true)
	 * @param sizeCheck[in] whether or not to perform the maximum size check (default true)
	 *
	 * @returns the new object, or NULL if allocation failed
	 */
	VMINLINE j9object_t
	allocateIndexableObject(REGISTER_ARGS_LIST, J9Class *arrayClass, U_32 size, bool initializeSlots = true, bool memoryBarrier = true, bool sizeCheck = true)
	{
		j9object_t instance = _objectAllocate.inlineAllocateIndexableObject(_currentThread, arrayClass, size, initializeSlots, memoryBarrier, sizeCheck);
		if (NULL == instance) {
			updateVMStruct(REGISTER_ARGS);
			instance = _vm->memoryManagerFunctions->J9AllocateIndexableObject(_currentThread, arrayClass, size, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			VMStructHasBeenUpdated(REGISTER_ARGS);
		}
		return instance;
	}

	/**
	 * Initialize a class if it requires it.
	 * This function assumes that the stack and live values are in a valid state for GC/call-in.
	 *
	 * @param j9clazz[in] the J9Class to initialize
	 *
	 * @returns a bytecode action (EXECUTE_BYTECODE on success)
	 */
	VMINLINE VM_BytecodeAction
	initializeClassIfNeeded(REGISTER_ARGS_LIST, J9Class* &j9clazz)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		if (J9_UNEXPECTED(VM_VMHelpers::classRequiresInitialization(_currentThread, j9clazz))) {
			updateVMStruct(REGISTER_ARGS);
			initializeClass(_currentThread, j9clazz);
			j9clazz = VM_VMHelpers::currentClass(j9clazz);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			if (J9_UNEXPECTED(immediateAsyncPending())) {
				rc = GOTO_ASYNC_CHECK;
			} else if (J9_UNEXPECTED(VM_VMHelpers::exceptionPending(_currentThread))) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
			}
		}
		return rc;
	}

	VMINLINE IDATA
	enterObjectMonitor(REGISTER_ARGS_LIST, j9object_t obj)
	{
		IDATA rc = (IDATA)obj;
		if (true || !VM_ObjectMonitor::inlineFastObjectMonitorEnter(_currentThread, obj)) {
			rc = objectMonitorEnterNonBlocking(_currentThread, obj);
			if (1 == rc) {
				updateVMStruct(REGISTER_ARGS);
				rc = objectMonitorEnterBlocking(_currentThread);
				VMStructHasBeenUpdated(REGISTER_ARGS);
			}
		}
		return rc;
	}

	VMINLINE IDATA
	exitObjectMonitor(REGISTER_ARGS_LIST, j9object_t obj)
	{
		IDATA rc = 0;
		if (!VM_ObjectMonitor::inlineFastObjectMonitorExit(_currentThread, obj)) {
			rc = objectMonitorExit(_currentThread, obj);
		}
		return rc;
	}

	VMINLINE VM_BytecodeAction
	doReferenceArrayCopy(REGISTER_ARGS_LIST, j9object_t srcObject, I_32 srcStart, j9object_t destObject, I_32 destStart, I_32 elementCount)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;

		if (elementCount < 0) {
			buildInternalNativeStackFrame(REGISTER_ARGS);
			_currentThread->tempSlot = (UDATA)elementCount;
			rc = THROW_AIOB;
		} else {
			I_32 invalidIndex = 0;
			if (!VM_ArrayCopyHelpers::rangeCheck(_currentThread, srcObject, srcStart, elementCount, &invalidIndex)) {
				buildInternalNativeStackFrame(REGISTER_ARGS);
				_currentThread->tempSlot = (UDATA)invalidIndex;
				rc = THROW_AIOB;
			} else {
				if (!VM_ArrayCopyHelpers::rangeCheck(_currentThread, destObject, destStart, elementCount, &invalidIndex)) {
					buildInternalNativeStackFrame(REGISTER_ARGS);
					_currentThread->tempSlot = (UDATA)invalidIndex;
					rc = THROW_AIOB;
				} else {
					I_32 value = VM_ArrayCopyHelpers::referenceArrayCopy(_currentThread, srcObject, srcStart, destObject, destStart, elementCount);
					if (-1 != value) {
						buildInternalNativeStackFrame(REGISTER_ARGS);
						rc = THROW_ARRAY_STORE;
					}
				}
			}
		}

		return rc;
	}

	VMINLINE VM_BytecodeAction
	doPrimitiveArrayCopy(REGISTER_ARGS_LIST, j9object_t srcObject, I_32 srcStart, j9object_t destObject, I_32 destStart, U_32 arrayShape, I_32 elementCount)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		/* Primitive array */
		if (elementCount < 0) {
			buildInternalNativeStackFrame(REGISTER_ARGS);
			_currentThread->tempSlot = (UDATA)elementCount;
			rc = THROW_AIOB;
		} else {
			I_32 invalidIndex = 0;
			if (!VM_ArrayCopyHelpers::rangeCheck(_currentThread, srcObject, srcStart, elementCount, &invalidIndex)) {
				buildInternalNativeStackFrame(REGISTER_ARGS);
				_currentThread->tempSlot = (UDATA)invalidIndex;
				rc = THROW_AIOB;
			} else {
				if (!VM_ArrayCopyHelpers::rangeCheck(_currentThread, destObject, destStart, elementCount, &invalidIndex)) {
					buildInternalNativeStackFrame(REGISTER_ARGS);
					_currentThread->tempSlot = (UDATA)invalidIndex;
					rc = THROW_AIOB;
				} else {
					if (elementCount > 0) {
						VM_ArrayCopyHelpers::primitiveArrayCopy(_currentThread, srcObject, srcStart, destObject, destStart, elementCount, arrayShape);
					}
				}
			}
		}
		return rc;
	}

#if defined(DEBUG_VERSION)
	VMINLINE void
	cleanUpForFramePop(REGISTER_ARGS_LIST, UDATA *bp)
	{
		/* Exit any monitors acquired in the current frame, and discard the monitor enter records */
		J9Pool *monitorEnterRecordPool = _currentThread->monitorEnterRecordPool;
		J9MonitorEnterRecord *enterRecord = _currentThread->monitorEnterRecords;
		UDATA *relativeA0 = CONVERT_TO_RELATIVE_STACK_OFFSET(_currentThread, _arg0EA);
		while ((NULL != enterRecord) && (relativeA0 == enterRecord->arg0EA)) {
			J9MonitorEnterRecord *recordToFree = enterRecord;
			enterRecord = enterRecord->next;
			UDATA dropEnterCount = recordToFree->dropEnterCount;
			j9object_t object = recordToFree->object;
			while (0 != dropEnterCount) {
				objectMonitorExit(_currentThread, object);
				dropEnterCount -= 1;
			}
			pool_removeElement(monitorEnterRecordPool, recordToFree);
		}
		_currentThread->monitorEnterRecords = enterRecord;
		/* Handle synchronized, constructor and Object constructor */
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_literals);
		VM_AtomicSupport::writeBarrier();
		if (romMethod->modifiers & J9AccSynchronized) {
			objectMonitorExit(_currentThread, ((j9object_t*)bp)[1]);
		}
		if (J9ROMMETHOD_IS_OBJECT_CONSTRUCTOR(romMethod)) {
			j9object_t finalizeObject = ((j9object_t*)bp)[1];
			if (J9CLASS_FLAGS(J9OBJECT_CLAZZ(_currentThread, finalizeObject)) & J9AccClassFinalizeNeeded) {
				_vm->memoryManagerFunctions->finalizeObjectCreated(_currentThread, finalizeObject);
			}
		}
	}

	VMINLINE void
	pushForceEarlyReturnValue(REGISTER_ARGS_LIST)
	{
		switch (_currentThread->ferReturnType)
		{
		case JVMTI_TYPE_JINT:
		case JVMTI_TYPE_JFLOAT:
			_sp -= 1;
			memmove(_sp, &_currentThread->ferReturnValue, 4);
			break;

		case JVMTI_TYPE_JLONG:
		case JVMTI_TYPE_JDOUBLE:
			_sp -= 2;
			memmove(_sp, &_currentThread->ferReturnValue, 8);
			break;

		case JVMTI_TYPE_JOBJECT:
			_sp -= 1;
			*(j9object_t*)_sp = _currentThread->forceEarlyReturnObjectSlot;
			_currentThread->forceEarlyReturnObjectSlot = NULL;
			break;
		}

		/* Clear the FER returnType so we do not erroneously process the next PopFrame interrupt as an Early Return */

		_currentThread->ferReturnType = 0;
	}

	VMINLINE VM_BytecodeAction
	handlePopFramesInterrupt(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		J9StackWalkState walkState;

		/* The interrupt will be fully handled here, and the current thread will no longer be throwing */
		_currentThread->currentException = NULL;
		clearEventFlag(_currentThread, J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT);
		/* Find top visible stack frame */
		updateVMStruct(REGISTER_ARGS);
		walkState.skipCount = 0;
		walkState.walkThread = _currentThread;
		walkState.maxFrames = 1;
		walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_COUNT_SPECIFIED;
		_vm->walkStackFrames(_currentThread, &walkState);
		if (NULL != walkState.jitInfo) {
			/* Top stack frame is compiled - decompile it now (decompile record was added by JVMTI) */
			_vm->jitConfig->jitDecompileMethodForFramePop(_currentThread, 0);
			VMStructHasBeenUpdated(REGISTER_ARGS);
		} else {
			/* Top stack frame not compiled, make it current */
			_sp = walkState.sp;
			_pc = walkState.pc;
			_arg0EA = walkState.arg0EA;
			_literals = walkState.literals;
		}
		if (0 == _currentThread->ferReturnType) {
			/* PopFrame - find the next stack frame (current frame is a bytecode frame) */
			updateVMStruct(REGISTER_ARGS);
			walkState.skipCount = 0;
			walkState.walkThread = _currentThread;
			walkState.maxFrames = 2;
			walkState.flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_COUNT_SPECIFIED;
			_vm->walkStackFrames(_currentThread, &walkState);
			if (NULL != walkState.jitInfo) {
				/* Compiled frame - decompile, discarding the top frame */
				_vm->jitConfig->jitDecompileMethodForFramePop(_currentThread, 1);
				VMStructHasBeenUpdated(REGISTER_ARGS);
			} else {
				/* Bytecoded frame - restore the stack from the walk state */
				_sp = walkState.sp;
				_arg0EA = walkState.arg0EA;
				_pc = walkState.pc;
				_literals = walkState.literals;
			}
			switch (*_pc) {
			case JBinvokeinterface:
				/* If the re-executed bytecode is invokeinterface, back up to the invokeinterface2 */
				_pc -= 2;
				break;
			case JBinvokehandle:
			case JBinvokehandlegeneric: {
				/* The MH object gets eaten during invokehandle calls. Upon restart, unwindSP (savedMethod) is the
				 * 'would be' slot of MH. We must decrement sp and clear all the slots above unwindSP so that we throw a NPE
				 * instead of trying to run using the saved method slot. All the slots must be cleared because when the sp is
				 * decremented there is a possibility that the stack slot values do not match with the expected types. By clearing
				 * all entries we avoid a situation where the GC treats an native (or garbage) value as a live object */
				U_16 index = *(U_16*)(_pc + 1);
				J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
				J9RAMMethodRef *ramMethodRef = ((J9RAMMethodRef*)ramConstantPool) + index;
				UDATA volatile methodIndexAndArgCount = ramMethodRef->methodIndexAndArgCount;
				_sp -= 1;
				/* clear arguments */
				memset(_sp, 0, sizeof(UDATA) * ((methodIndexAndArgCount & 0xFF) + 1));
				break;
			}
			}
		} else {
			/* ForceEarlyReturn */
			bool hooked = J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_METHOD_RETURN);
			bool traced = VM_VMHelpers::methodBeingTraced(_vm, _literals);
			if (hooked || traced) {
				updateVMStruct(REGISTER_ARGS);
				void *returnValue = &_currentThread->ferReturnValue;
				if (JVMTI_TYPE_JOBJECT == _currentThread->ferReturnType) {
					returnValue = &_currentThread->forceEarlyReturnObjectSlot;
				}
				if (traced) {
					UTSI_TRACEMETHODEXIT_FROMVM(_vm, _currentThread, _literals, NULL, returnValue, 0);
				}
				if (hooked) {
					ALWAYS_TRIGGER_J9HOOK_VM_METHOD_RETURN(_vm->hookInterface, _currentThread, _literals, FALSE, returnValue, 0);
				}
				VMStructHasBeenUpdated(REGISTER_ARGS);
			}
			UDATA *bp = bpForCurrentBytecodedMethod(REGISTER_ARGS);
			if (*bp & J9SF_A0_REPORT_FRAME_POP_TAG) {
				if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_FRAME_POP)) {
					updateVMStruct(REGISTER_ARGS);
					ALWAYS_TRIGGER_J9HOOK_VM_FRAME_POP(_vm->hookInterface, _currentThread, _literals, FALSE);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					bp = bpForCurrentBytecodedMethod(REGISTER_ARGS);
				}
				*bp &= ~(UDATA)J9SF_A0_REPORT_FRAME_POP_TAG;
			}
			/* Clean up monitors, etc for the frame that is about to be popped */
			cleanUpForFramePop(REGISTER_ARGS, bp);
			/* Return from this frame */
			if (bp == _currentThread->j2iFrame) {
				pushForceEarlyReturnValue(REGISTER_ARGS);
				rc = j2iReturn(REGISTER_ARGS);
			} else {
				J9SFStackFrame *frame = ((J9SFStackFrame*)(bp + 1)) - 1;
				_sp = _arg0EA + 1;
				_literals = frame->savedCP;
				_pc = frame->savedPC + 3;
				_arg0EA = frame->savedA0;
				pushForceEarlyReturnValue(REGISTER_ARGS);
			}
		}
		return rc;
	}
#endif /* DEBUG_VERSION */

	VMINLINE VM_BytecodeAction
	checkAsync(REGISTER_ARGS_LIST)
	{
		/* The current stack frame may be one of the following:
		 *
		 * 1) Special frame
		 *
		 *	1a) INL frame (for an INL which returns void)
		 *
		 *		No frame build is required, and the restoreSpecialStackFrameAndDrop will remove the
		 *		argument from the stack before resuming execution.
		 *
		 *	1b) An immediate async is pending
		 *
		 *		The stack is already in a walkable state, and the restore and executeCurrentBytecode
		 *		will not take place.
		 *
		 * 2) Bytecode or JNI call-in frame
		 *
		 *	Build a generic special frame, tear it down when done.
		 */
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		bool inlFrame = false;
		UDATA *bp = NULL;
		if ((UDATA)_pc <= J9SF_MAX_SPECIAL_FRAME_TYPE) {
			if (J9SF_FRAME_TYPE_NATIVE_METHOD == (UDATA)_pc) {
				inlFrame = true;
				bp = ((UDATA*)(((J9SFNativeMethodFrame*)((UDATA)_sp + (UDATA)_literals)) + 1)) - 1;
			}
		} else {
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			bp = _arg0EA;
		}
		UDATA relativeBP = bp - _arg0EA;
		updateVMStruct(REGISTER_ARGS);
		UDATA action = javaCheckAsyncMessages(_currentThread, TRUE);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		switch(action) {
		case J9_CHECK_ASYNC_THROW_EXCEPTION:
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			break;
#if defined(DEBUG_VERSION)
		case J9_CHECK_ASYNC_POP_FRAMES:
			rc = HANDLE_POP_FRAMES;
			break;
#endif
		default:
			restoreSpecialStackFrameAndDrop(REGISTER_ARGS, _arg0EA + relativeBP);
			if (inlFrame) {
				_pc += 3;
			}
			break;
		}
		return rc;
	}

	VMINLINE bool
	objectArrayStoreAllowed(j9object_t array, j9object_t storeValue)
	{
		bool rc = true;
		if (NULL != storeValue) {
			J9Class *valueClass = J9OBJECT_CLAZZ(_currentThread, storeValue);
			J9Class *componentType = ((J9ArrayClass*)J9OBJECT_CLAZZ(_currentThread, array))->componentType;
			/* quick check -- is this a store of a C into a C[]? */
			if (valueClass != componentType) {
				/* quick check -- is this a store of a C into a java.lang.Object[]? */
				if (0 != J9CLASS_DEPTH(componentType)) {
					rc = VM_VMHelpers::inlineCheckCast(valueClass, componentType);
				}
			}
		}
		return rc;
	}

	VMINLINE bool immediateAsyncPending()
	{
#if defined(DEBUG_VERSION)
		return VM_VMHelpers::immediateAsyncPending(_currentThread);
#else
		return false;
#endif
	}

	VMINLINE VM_BytecodeAction
	throwUnsatisfiedLinkOrAbstractMethodError(REGISTER_ARGS_LIST)
	{
		/* We need a frame to describe the method arguments (in particular, for the case where we got here directly from the JIT) */
		buildMethodFrame(REGISTER_ARGS, _sendMethod, jitStackFrameFlags(REGISTER_ARGS, 0));
		UDATA exception = J9VMCONSTANTPOOL_JAVALANGUNSATISFIEDLINKERROR;
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod);
		if (romMethod->modifiers & J9AccAbstract) {
			exception = J9VMCONSTANTPOOL_JAVALANGABSTRACTMETHODERROR;
			/* If the method class is an interface, do the special checks to throw the correct error */
			J9Class *methodClass = J9_CLASS_FROM_METHOD(_sendMethod);
			if (J9ROMCLASS_IS_INTERFACE(methodClass->romClass)) {
				J9Method *method = (J9Method*)javaLookupMethod(
						_currentThread,
						J9OBJECT_CLAZZ(_currentThread, *(j9object_t*)_arg0EA),
						&romMethod->nameAndSignature,
						NULL,
						J9_LOOK_VIRTUAL | J9_LOOK_NO_THROW);
				if (NULL != method) {
					if (0 == (J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers & J9AccPublic)) {
						exception = J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR;
						_sendMethod = method;
					}
				}
			}
		}
		updateVMStruct(REGISTER_ARGS);
		setCurrentException(_currentThread, exception, (UDATA*)methodToString(_currentThread, _sendMethod));
		VMStructHasBeenUpdated(REGISTER_ARGS);
		return  GOTO_THROW_CURRENT_EXCEPTION;
	}

	VMINLINE VM_BytecodeAction
	throwForDefaultConflict(REGISTER_ARGS_LIST)
	{
		/* We need a frame to describe the method arguments (in particular, for the case where we got here directly from the JIT) */
		buildMethodFrame(REGISTER_ARGS, _sendMethod, jitStackFrameFlags(REGISTER_ARGS, 0));
		updateVMStruct(REGISTER_ARGS);
		setIncompatibleClassChangeErrorForDefaultConflict(_currentThread, _sendMethod);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		return  GOTO_THROW_CURRENT_EXCEPTION;
	}

	VMINLINE VM_BytecodeAction
	inlineSendTarget(
			REGISTER_ARGS_LIST,
			VM_YesNoMaybe isStatic,
			VM_YesNoMaybe isSynchonized,
			VM_YesNoMaybe isObjectConstructor,
			VM_YesNoMaybe zeroing,
			bool j2i = false,
			bool decompileOccurred = false
	) {
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod);
		U_32 modifiers = romMethod->modifiers;
		UDATA *newA0 = (_sp + romMethod->argCount) - 1;
		j9object_t syncObject = NULL;
		bool methodIsSynchronized = (VM_YES == isSynchonized) || ((VM_MAYBE == isSynchonized) && (modifiers & J9AccSynchronized));
		bool methodIsStatic = (VM_YES == isStatic) || ((VM_MAYBE == isStatic) && (modifiers & J9AccStatic));
		bool methodIsObjectConstructor = (VM_YES == isObjectConstructor) || ((VM_MAYBE == isObjectConstructor) && J9ROMMETHOD_IS_OBJECT_CONSTRUCTOR(romMethod));
		bool tempsNeedZeroing = (VM_YES == zeroing) || ((VM_MAYBE == zeroing) && (_vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_DEBUG_MODE));
		if (methodIsSynchronized) {
			if (methodIsStatic) {
				syncObject = J9VM_J9CLASS_TO_HEAPCLASS(J9_CLASS_FROM_METHOD(_sendMethod));
			} else {
				syncObject = (j9object_t)*newA0;
			}
		}
		U_16 tempCount = romMethod->tempCount;
		_sp -= tempCount;
		if (tempsNeedZeroing) {
			memset(_sp, 0, tempCount * sizeof(UDATA));
		}
		if (methodIsSynchronized) {
			*--_sp = (UDATA)syncObject;
		} else if (methodIsObjectConstructor) {
			if (0 == (modifiers & J9AccEmptyMethod)) {
				*--_sp = (UDATA)*newA0;
			}
		}
		if (j2i) {
			J9SFJ2IFrame* const j2iFrame = ((J9SFJ2IFrame*)_sp) - 1;
			fillInJ2IValues(j2iFrame, (void*)_literals, (void*)_pc, _arg0EA);
			_sp = (UDATA*)j2iFrame;
			if (decompileOccurred) {
				/* The return address will be pushed immediately on entry to the compiled method */
				_currentThread->decompilationStack->pcAddress = &j2iFrame->returnAddress;
			}
		} else {
			*--_sp = (UDATA)_arg0EA;
			*--_sp = (UDATA)_pc;
			*--_sp = (UDATA)_literals;
		}
		_arg0EA = newA0;
		_literals = _sendMethod;
		_pc = _sendMethod->bytecodes;
		UDATA volatile stackOverflowMark = (UDATA)_currentThread->stackOverflowMark;
		if ((UDATA)_sp >= stackOverflowMark) {
			if (methodIsSynchronized) {
				IDATA monitorRC = enterObjectMonitor(REGISTER_ARGS, syncObject);
				/* Monitor enter can only fail in the nonblocking case, which does not
				 * release VM access, so the immediate async and failed enter cases are
				 * mutually exclusive.
				 */
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
				if (0 == monitorRC) {
					/* Monitor was not entered - hide the frame to prevent exception throw from processing it */
					if (j2i) {
						((UDATA*)(((J9SFJ2IFrame*)_sp) + 1))[-1] |= J9SF_A0_INVISIBLE_TAG;
					} else {
						((UDATA*)(((J9SFStackFrame*)_sp) + 1))[-1] |= J9SF_A0_INVISIBLE_TAG;
					}
					rc = THROW_MONITOR_ALLOC_FAIL;
					goto done;
				}
			}
#if defined(DO_HOOKS)
			rc = REPORT_METHOD_ENTER;
#endif
		} else {
			rc = GOTO_JAVA_STACK_OVERFLOW;
		}
done:
		return rc;
	}

#if defined(DO_HOOKS)
	VMINLINE VM_BytecodeAction
	reportMethodEnter(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		bool hooked = J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_METHOD_ENTER);
		bool traced = VM_VMHelpers::methodBeingTraced(_vm, _sendMethod);
		if (hooked || traced) {
			UDATA *receiverAddress = _arg0EA;
			buildGenericSpecialStackFrame(REGISTER_ARGS, J9_SSF_METHOD_ENTRY);
			updateVMStruct(REGISTER_ARGS);
			if (traced) {
				UTSI_TRACEMETHODENTER_FROMVM(_vm, _currentThread, _sendMethod, (void*)receiverAddress, 0);
			}
			if (hooked) {
				ALWAYS_TRIGGER_J9HOOK_VM_METHOD_ENTER(_vm->hookInterface, _currentThread, _sendMethod, (void*)receiverAddress, 0);
			}
			VMStructHasBeenUpdated(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
				goto done;
			}
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
		}
done:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	enterMethodMonitor(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = REPORT_METHOD_ENTER;
		UDATA *bp = bpForCurrentBytecodedMethod(REGISTER_ARGS);
		IDATA monitorRC = enterObjectMonitor(REGISTER_ARGS, ((j9object_t*)bp)[1]);
		/* Monitor enter can only fail in the nonblocking case, which does not
		 * release VM access, so the immediate async and failed enter cases are
		 * mutually exclusive.
		 */
		if (immediateAsyncPending()) {
			rc = GOTO_ASYNC_CHECK;
			goto done;
		}
		if (0 == monitorRC) {
			/* Monitor was not entered - hide the frame to prevent exception throw from processing it.
			 * Note that BP can not have changed during a failed enter.
			 */
			*bp |= J9SF_A0_INVISIBLE_TAG;
			rc = THROW_MONITOR_ALLOC_FAIL;
			goto done;
		}
done:
		return rc;
	}
#endif /* DO_HOOKS */

	/* A method has just been entered (stack frame built, no monitor entered or method enter hook called) and the SP is below the stackOverflowMark */
	VMINLINE VM_BytecodeAction
	stackOverflow(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_literals);
		UDATA relativeBP = 0;
		if (*_sp & J9_STACK_FLAGS_J2_IFRAME) {
			relativeBP = (((UDATA*)(((J9SFJ2IFrame*)_sp) + 1)) - 1) - _arg0EA;
		} else {
			relativeBP = (((UDATA*)(((J9SFStackFrame*)_sp) + 1)) - 1) - _arg0EA;
		}
		/* See if we really overflowed the stack (SOM could be -1 to indicate an async pending) */
		if (VM_VMHelpers::shouldGrowForSP(_currentThread, _sp)) {
			/* Attempt to grow the stack to prevent the StackOverflowError. The frame and temps are on stack,
			 * but the maxStack (max pending pushes) must also be taken into account.
			 */
			UDATA *checkSP = _sp - romMethod->maxStack;
			UDATA currentUsed = (UDATA)_currentThread->stackObject->end - (UDATA)checkSP;
			UDATA maxStackSize = _vm->stackSize;
			if (currentUsed > maxStackSize) {
throwStackOverflow:
				if (*_sp & J9_STACK_FLAGS_J2_IFRAME) {
					/* Hide the J2I frame.  This means it will not show in the exception stack trace.
					 * If the method is synchronized, this also means the thrower will not attempt to
					 * exit the monitor for it.
					 */
					*(_arg0EA + relativeBP) |= J9SF_A0_INVISIBLE_TAG;
				} else {
					/* Discard the current bytecode frame and clean up the stack of the caller */
					restoreSpecialStackFrameAndDrop(REGISTER_ARGS, _arg0EA + relativeBP);
					updateVMStruct(REGISTER_ARGS);
					dropPendingSendPushes(_currentThread);
					VMStructHasBeenUpdated(REGISTER_ARGS);
				}
				buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
				updateVMStruct(REGISTER_ARGS);
				if (J9_ARE_ANY_BITS_SET(_currentThread->privateFlags, J9_PRIVATE_FLAGS_STACK_OVERFLOW)) {
					// vmStruct already up-to-date in all paths to here
					fatalRecursiveStackOverflow(_currentThread);
				}
				setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGSTACKOVERFLOWERROR, NULL);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				if (immediateAsyncPending()) {
					_currentThread->currentException = NULL;
					rc = GOTO_ASYNC_CHECK;
				}
				goto done;
			}
			currentUsed += _vm->stackSizeIncrement;
			if (currentUsed > maxStackSize) {
				currentUsed = maxStackSize;
			}
			/* Hide the current frame during grow to indicate method enter (i.e. keep object arguments alive) */
			*(_arg0EA + relativeBP) |= J9SF_A0_INVISIBLE_TAG;
			updateVMStruct(REGISTER_ARGS);
			UDATA growRC = growJavaStack(_currentThread, currentUsed);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			*(_arg0EA + relativeBP) &= ~(UDATA)J9SF_A0_INVISIBLE_TAG;
			if (0 != growRC) {
				goto throwStackOverflow;
			}
		}
		{
			/* Stack did not fatally overflow - hide the frame and perform an async check */
			*(_arg0EA + relativeBP) |= J9SF_A0_INVISIBLE_TAG;
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			UDATA action = javaCheckAsyncMessages(_currentThread, TRUE);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			if (J9_CHECK_ASYNC_THROW_EXCEPTION == action) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
#if defined(DEBUG_VERSION)
			if (J9_CHECK_ASYNC_POP_FRAMES == action) {
				UDATA executeAsyncCheck = FALSE;
				updateVMStruct(REGISTER_ARGS);
				ALWAYS_TRIGGER_J9HOOK_VM_POP_FRAMES_INTERRUPT(_vm->hookInterface, _currentThread, executeAsyncCheck);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				if (executeAsyncCheck) {
					rc = GOTO_ASYNC_CHECK;
				}
				goto done;
			}
#endif
			restoreSpecialStackFrameAndDrop(REGISTER_ARGS, _arg0EA);
			*(_arg0EA + relativeBP) &= ~(UDATA)J9SF_A0_INVISIBLE_TAG;
			/* If the method is synchronized, enter its monitor */
#if defined(DEBUG_VERSION)
			romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_literals);
#endif /* defined(DEBUG_VERSION) */
			if (romMethod->modifiers & J9AccSynchronized) {
				j9object_t syncObject = ((j9object_t*)(_arg0EA + relativeBP))[1];
				IDATA monitorRC = enterObjectMonitor(REGISTER_ARGS, syncObject);
				/* Monitor enter can only fail in the nonblocking case, which does not
				 * release VM access, so the immediate async and failed enter cases are
				 * mutually exclusive.
				 */
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
				if (0 == monitorRC) {
					/* Monitor was not entered - hide the frame to prevent exception throw from processing it */
					*(_arg0EA + relativeBP) |= J9SF_A0_INVISIBLE_TAG;
					rc = THROW_MONITOR_ALLOC_FAIL;
					goto done;
				}
			}
#if defined(DO_HOOKS)
			rc = REPORT_METHOD_ENTER;
#endif
		}
done:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	takeBranch(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		UDATA volatile stackOverflowMark = (UDATA)_currentThread->stackOverflowMark;
		if (((UDATA) _literals->constantPool) & J9_STARTPC_DLT_READY) {
			J9JITConfig *jitConfig = _vm->jitConfig;
			if (NULL != jitConfig->isDLTReady(_currentThread, _literals, _pc - _literals->bytecodes)) {
				rc = PERFORM_DLT;
				goto done;
			}
		}
		if (J9_EVENT_SOM_VALUE == stackOverflowMark) {
			rc = GOTO_ASYNC_CHECK;
		}
done:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	performDLT(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		UDATA *temps = _currentThread->dltBlock.temps;
		UDATA *inlineTempsBuffer = _currentThread->dltBlock.inlineTempsBuffer;
		if (temps != inlineTempsBuffer) {
			PORT_ACCESS_FROM_VMC(_currentThread);
			j9mem_free_memory(temps);
			_currentThread->dltBlock.temps = inlineTempsBuffer;
		}
		buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
		updateVMStruct(REGISTER_ARGS);
		J9StackWalkState *walkState = _currentThread->stackWalkState;
		void *dltEntry = _vm->jitConfig->setUpForDLT(_currentThread, walkState);
		// No need for VMStructHasBeenUpdated - setUpForDLT didn't modify any of the vmThread values
		if (NULL == dltEntry) {
			/* Toss the generic special frame and resume execution in the interpreter */
			restoreSpecialStackFrameAndDrop(REGISTER_ARGS, _arg0EA);
		} else {
			/* See if the current frame is bytecode->bytecode or J2I */
			if (NULL != walkState->jitInfo) {
				/* Restore preserved registers */
				restorePreservedRegistersFromWalkState(REGISTER_ARGS, walkState);
			}
			// TODO: macro loadPseudoTOC in the interp case
			/* Collapse stack frame */
			_sp = walkState->sp;
			/* Set up return address and branch to DLT entrypoint */
			rc = promotedMethodOnTransitionFromJIT(REGISTER_ARGS, walkState->userData1, dltEntry);
		}
		return rc;
	}

	VMINLINE VM_BytecodeAction sendTargetSmallNonSync(REGISTER_ARGS_LIST) { return inlineSendTarget(REGISTER_ARGS, VM_NO, VM_NO, VM_NO, VM_NO); }
	VMINLINE VM_BytecodeAction sendTargetSmallSync(REGISTER_ARGS_LIST) { return inlineSendTarget(REGISTER_ARGS, VM_NO, VM_YES, VM_NO, VM_NO); }
	VMINLINE VM_BytecodeAction sendTargetSmallSyncStatic(REGISTER_ARGS_LIST) { return inlineSendTarget(REGISTER_ARGS, VM_YES, VM_YES, VM_NO, VM_NO); }
	VMINLINE VM_BytecodeAction sendTargetSmallObjectConstructor(REGISTER_ARGS_LIST) { return inlineSendTarget(REGISTER_ARGS, VM_NO, VM_NO, VM_YES, VM_NO); }
	VMINLINE VM_BytecodeAction sendTargetSmallZeroing(REGISTER_ARGS_LIST) { return inlineSendTarget(REGISTER_ARGS, VM_MAYBE, VM_MAYBE, VM_MAYBE, VM_YES); }

	VMINLINE VM_BytecodeAction
	sendTargetEmptyObjectConstructor(REGISTER_ARGS_LIST)
	{
		j9object_t receiver = (j9object_t)*_sp++;
		_pc += 3;
		VM_VMHelpers::checkIfFinalizeObject(_currentThread, receiver);
		return EXECUTE_BYTECODE;
	}

	VMINLINE UDATA
	pushVMState(REGISTER_ARGS_LIST, UDATA newState)
	{
		UDATA oldState = _currentThread->omrVMThread->vmState;
		_currentThread->omrVMThread->vmState = newState;
		return oldState;
	}

	VMINLINE void popVMState(REGISTER_ARGS_LIST, UDATA oldState) { _currentThread->omrVMThread->vmState = oldState; }

	VMINLINE bool startAddressIsCompiled(UDATA extra) { return J9_ARE_NO_BITS_SET(extra, J9_STARTPC_NOT_TRANSLATED); }
	VMINLINE bool methodIsCompiled(J9Method *method) { return startAddressIsCompiled((UDATA)method->extra); }
	VMINLINE bool singleStepEnabled() { return 0 != _vm->jitConfig->singleStepCount; }
	VMINLINE bool methodIsBreakpointed(J9Method *method) { return J9_ARE_ANY_BITS_SET((UDATA)method->constantPool, J9_STARTPC_METHOD_BREAKPOINTED); }
	VMINLINE bool methodCanBeRunCompiled(J9Method *method) { return !singleStepEnabled() && !methodIsBreakpointed(method); }

	VMINLINE bool
	countAndCompile(REGISTER_ARGS_LIST)
	{
		bool runMethodCompiled = false;
		UDATA preCount = 0;
		UDATA postCount = 0;
		UDATA result = 0;
		do {
			preCount = (UDATA)_sendMethod->extra;
			postCount = preCount - _currentThread->jitCountDelta;
			if (J9_ARE_NO_BITS_SET(preCount, J9_STARTPC_NOT_TRANSLATED)) {
				/* Already compiled */
				runMethodCompiled = true;
				break;
			}
			if ((IDATA)preCount < 0) {
				/* This method should not be translated (already enqueued, already failed, etc) */
				break;
			}
			if ((IDATA)postCount < 0) {
				/* Attempt to compile the method */
				UDATA *bp = buildMethodFrame(REGISTER_ARGS, _sendMethod, 0);
				updateVMStruct(REGISTER_ARGS);
				/* this call cannot change bp as no java code is run */
				UDATA oldState = pushVMState(REGISTER_ARGS, J9VMSTATE_JIT_CODEGEN);
				J9JITConfig *jitConfig = _vm->jitConfig;
				jitConfig->entryPoint(jitConfig, _currentThread, _sendMethod, 0);
				popVMState(REGISTER_ARGS, oldState);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, bp);
				/* If the method is now compiled, run it compiled, otherwise run it bytecoded */
				if (methodIsCompiled(_sendMethod)) {
					runMethodCompiled = true;
				}
				break;
			}
			result = VM_AtomicSupport::lockCompareExchange((UDATA*)&_sendMethod->extra, preCount, postCount);
			/* If count updates, run method interpreted, else loop around and try again */
		} while (result != preCount);
		return runMethodCompiled;
	}

	VMINLINE VM_BytecodeAction
	initialStaticMethod(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_RUN_METHOD;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
		updateVMStruct(REGISTER_ARGS);
		J9Method *method = resolveStaticMethodRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_CHECK_CLINIT);
		if ((J9Method*)-1 == method) {
			method = ((J9RAMStaticMethodRef*)&_currentThread->floatTemp1)->method;
		}
		VMStructHasBeenUpdated(REGISTER_ARGS);
		restoreGenericSpecialStackFrame(REGISTER_ARGS);
		if (immediateAsyncPending()) {
			rc = GOTO_ASYNC_CHECK;
		} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		} else {
			_sendMethod = method;
		}
		return rc;
	}

	VMINLINE VM_BytecodeAction
	initialSpecialMethod(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_RUN_METHOD;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
		updateVMStruct(REGISTER_ARGS);
		J9Method *method = resolveSpecialMethodRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		restoreGenericSpecialStackFrame(REGISTER_ARGS);
		if (immediateAsyncPending()) {
			rc = GOTO_ASYNC_CHECK;
		} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		} else {
			_sendMethod = method;
		}
		return rc;
	}

	VMINLINE VM_BytecodeAction
	initialVirtualMethod(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_RUN_METHOD;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
		updateVMStruct(REGISTER_ARGS);
		resolveVirtualMethodRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE, NULL);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		restoreGenericSpecialStackFrame(REGISTER_ARGS);
		if (immediateAsyncPending()) {
			rc = GOTO_ASYNC_CHECK;
		} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		} else {
			rc = invokevirtualLogic(REGISTER_ARGS, false);
		}
		return rc;
	}

#if defined(J9VM_OPT_VALHALLA_NESTMATES)
	VMINLINE VM_BytecodeAction
	invokePrivateMethod(REGISTER_ARGS_LIST)
	{
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);

		_sendMethod = ((J9RAMVirtualMethodRef*)&ramConstantPool[index])->method;

		return GOTO_RUN_METHOD;
	}
#endif /* J9VM_OPT_VALHALLA_NESTMATES */

	VMINLINE VM_BytecodeAction
	bindNative(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_RUN_METHOD;;
		buildMethodFrame(REGISTER_ARGS, _sendMethod, jitStackFrameFlags(REGISTER_ARGS, 0));
		updateVMStruct(REGISTER_ARGS);
		UDATA bindRC = resolveNativeAddress(_currentThread, _sendMethod, TRUE);
		if (J9_NATIVE_METHOD_BIND_OUT_OF_MEMORY == bindRC) {
			_vm->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(_currentThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
			bindRC = resolveNativeAddress(_currentThread, _sendMethod, TRUE);
		}
		switch(bindRC) {
		case J9_NATIVE_METHOD_BIND_SUCCESS: {
			VMStructHasBeenUpdated(REGISTER_ARGS);
			J9SFMethodFrame *methodFrame = (J9SFMethodFrame*)_sp;
			_currentThread->jitStackFrameFlags = methodFrame->specialFrameFlags & J9_SSF_JIT_NATIVE_TRANSITION_FRAME;
			restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, ((UDATA*)(methodFrame + 1)) - 1);
			break;
		}
		case J9_NATIVE_METHOD_BIND_OUT_OF_MEMORY:
			setNativeBindOutOfMemoryError(_currentThread, _sendMethod);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			break;
		case J9_NATIVE_METHOD_BIND_RECURSIVE:
			setRecursiveBindError(_currentThread, _sendMethod);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			break;
		default:
			setNativeNotFoundError(_currentThread, _sendMethod);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			break;
		}
		return rc;
	}

	VMINLINE VM_BytecodeAction
	countAndSendJNINative(REGISTER_ARGS_LIST)
	{
		VM_AtomicSupport::lockCompareExchange(
				(UDATA*)&_sendMethod->methodRunAddress,
				(UDATA)J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_AND_SEND_JNI_NATIVE),
				(UDATA)J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COMPILE_JNI_NATIVE));
		return RUN_JNI_NATIVE;
	}

	VMINLINE VM_BytecodeAction
	compileJNINative(REGISTER_ARGS_LIST)
	{
		UDATA methodRunAddress = (UDATA)J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_RUN_JNI_NATIVE);
		VM_AtomicSupport::lockCompareExchange(
				(UDATA*)&_sendMethod->methodRunAddress,
				(UDATA)J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_COMPILE_JNI_NATIVE),
				methodRunAddress);
		/* Attempt to compile the method */
		UDATA *bp = buildMethodFrame(REGISTER_ARGS, _sendMethod, jitStackFrameFlags(REGISTER_ARGS, 0));
		updateVMStruct(REGISTER_ARGS);
		/* this call cannot change bp as no java code is run */
		UDATA oldState = pushVMState(REGISTER_ARGS, J9VMSTATE_JIT_CODEGEN);
		J9JITConfig *jitConfig = _vm->jitConfig;
		jitConfig->entryPoint(jitConfig, _currentThread, _sendMethod, 0);
		popVMState(REGISTER_ARGS, oldState);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		J9SFMethodFrame *methodFrame = (J9SFMethodFrame*)_sp;
		_currentThread->jitStackFrameFlags = methodFrame->specialFrameFlags & J9_SSF_JIT_NATIVE_TRANSITION_FRAME;
		restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, bp);
		return RUN_JNI_NATIVE;
	}

	VMINLINE VM_BytecodeAction
	runJNINative(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_DONE;
		void *jniMethodStartAddress = _sendMethod->extra;
		U_8 returnType = 0;
		UDATA *bp = NULL;
		void *receiverAddress = NULL;
		UDATA *javaArgs = NULL;
		UDATA tracing = VM_VMHelpers::methodBeingTraced(_vm, _sendMethod);
		FFI_Return ret = ffiFailed;
		const UDATA jniRequiredAlignment = 2;
		bool isSynchronized = J9_ARE_ALL_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod)->modifiers, J9AccSynchronized);
		bool isStatic = J9_ARE_ALL_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod)->modifiers, J9AccStatic);

		if (J9_ARE_NO_BITS_SET((UDATA)jniMethodStartAddress, J9_STARTPC_NOT_TRANSLATED)) {
			Trc_VM_JNI_native_translated(_currentThread, _sendMethod);
			rc = RUN_METHOD_COMPILED;
			goto done;
		}
		jniMethodStartAddress = (void *)((UDATA)jniMethodStartAddress & ~(jniRequiredAlignment - 1));

		/* It's possible the current thread has not yet seen the write to extra,
		 * even though it has seen the write to methodRunAddress
		 * Handle this case by simply re-running the method until the write is visible
		 */
		if (0 == jniMethodStartAddress) {
			rc = GOTO_RUN_METHOD;
			goto done;
		}

		/* Build a call-out stack frame that is not visible to the debugger.
		 * After the synchronization (if any) is done, the visible bit will be or'ed in.
		 */
		bp = buildSpecialStackFrame(REGISTER_ARGS, J9SF_FRAME_TYPE_JNI_NATIVE_METHOD, jitStackFrameFlags(REGISTER_ARGS, 0), true);
		*--_sp = (UDATA) _sendMethod;
		_arg0EA = &_sp[J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod)->argCount + (sizeof(J9SFNativeMethodFrame) / sizeof(UDATA)) - 1];

		/* Store the original argument pointer before any possibility of the stack growing */
		javaArgs = _arg0EA;
		/* Get the receiver (either the actual receiver for virtuals, or the method class for statics). */
		receiverAddress = _arg0EA;
		if (isStatic) {
			receiverAddress = &(J9_CLASS_FROM_METHOD(_sendMethod)->classObject);
		}
		if (isSynchronized) {
			j9object_t receiver = *(j9object_t *)receiverAddress;
			UDATA relativeBP = _arg0EA - bp;
			IDATA monitorRC = enterObjectMonitor(REGISTER_ARGS, receiver);
			// No immediate async possible due to the current frame being for a native method.
			bp = _arg0EA - relativeBP;
			if (0 == monitorRC) {
				rc = THROW_MONITOR_ALLOC_FAIL;
				goto done;
			}
		}
		{
			bool enterHooked = J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_NATIVE_METHOD_ENTER);
			if (tracing || enterHooked) {
				UDATA relativeBP = _arg0EA - bp;
				updateVMStruct(REGISTER_ARGS);		
				if (tracing) {
					UTSI_TRACEMETHODENTER_FROMVM(_vm, _currentThread, _sendMethod, _arg0EA, 0);
				}
				if (enterHooked) {
					ALWAYS_TRIGGER_J9HOOK_VM_NATIVE_METHOD_ENTER(_vm->hookInterface, _currentThread, _sendMethod, _arg0EA);
				}
				VMStructHasBeenUpdated(REGISTER_ARGS);
				bp = _arg0EA - relativeBP;
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
			}
		}
		/* If the stack grew, receiverAddress is still pointing to the correct slot
		 * in the old stack, so does not need to be updated here.
		 */

		ret = callCFunction(REGISTER_ARGS, jniMethodStartAddress, receiverAddress, javaArgs, &bp, isStatic, &returnType);

		if (isSynchronized) {
			j9object_t syncObject = NULL;
			if (isStatic) {
				syncObject = J9VM_J9CLASS_TO_HEAPCLASS(J9_CLASS_FROM_METHOD(_sendMethod));
			} else {
				syncObject = *(j9object_t *)_arg0EA;
				if (J9_ARE_ALL_BITS_SET((UDATA)syncObject, J9_REDIRECTED_REFERENCE)){
					syncObject = *(j9object_t *)( (UDATA)syncObject & ~J9_REDIRECTED_REFERENCE );
				}
			}
			if (0 != exitObjectMonitor(REGISTER_ARGS, syncObject)) {
				rc = THROW_ILLEGAL_MONITOR_STATE;
				goto done;
			}
		}
		if (ffiFailed == ret) {
			Trc_VM_JNI_callCFunction_failed(_currentThread, _sendMethod);
			updateVMStruct(REGISTER_ARGS);
			setCurrentException(_currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
		} else if (ffiOOM == ret) {
			updateVMStruct(REGISTER_ARGS);
			setNativeOutOfMemoryError(_currentThread, J9NLS_VM_NATIVE_OOM);
			VMStructHasBeenUpdated(REGISTER_ARGS);
		}
		/* If there's an exception pending, don't report the method exit event - immediately invoke the exception thrower who will do it. */
		if (VM_VMHelpers::exceptionPending(_currentThread)) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		{
			bool exitHooked = J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_NATIVE_METHOD_RETURN);
			if (tracing || exitHooked) {
				UDATA relativeBP = _arg0EA - bp;
				updateVMStruct(REGISTER_ARGS);
				UDATA returnValue[2];
				void * returnValuePtr = returnValue;
				jobject returnRef = (jobject) _currentThread->returnValue2;	/* The JNI send target put the returned ref here in the object case */
				returnValue[0] = _currentThread->returnValue;
				returnValue[1] = _currentThread->returnValue2;
				if (returnType == J9NtcObject) {
					PUSH_OBJECT_IN_SPECIAL_FRAME(_currentThread, (j9object_t) returnValue[0]);
					returnValuePtr = _currentThread->sp;
				}
				/* Currently not called when an exception is pending, so always use NULL for exceptionPtr */
				if (tracing) {
					UTSI_TRACEMETHODEXIT_FROMVM(_vm, _currentThread, _sendMethod, NULL, returnValuePtr, 0);
				}
				if (exitHooked) {
					ALWAYS_TRIGGER_J9HOOK_VM_NATIVE_METHOD_RETURN(_vm->hookInterface, _currentThread, _sendMethod, FALSE, returnValuePtr, returnRef);
				}
				if (returnType == J9NtcObject) {
					returnValue[0] = (UDATA) POP_OBJECT_IN_SPECIAL_FRAME(_currentThread);
				}
				_currentThread->returnValue = returnValue[0];
				_currentThread->returnValue2 = returnValue[1];
				VMStructHasBeenUpdated(REGISTER_ARGS);
				bp = _arg0EA - relativeBP;
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
			}
		}
		{
			J9SFJNINativeMethodFrame *nativeMethodFrame = recordJNIReturn(REGISTER_ARGS, bp);
			_currentThread->jitStackFrameFlags = nativeMethodFrame->specialFrameFlags & J9_SSF_JIT_NATIVE_TRANSITION_FRAME;
			J9SFStackFrame *frame = (((J9SFStackFrame*)(bp + 1)) - 1);
			_sp = _arg0EA;
			_literals = frame->savedCP;
			_pc = frame->savedPC + 3;
			_arg0EA = frame->savedA0;
			_arg0EA = (UDATA *)((UDATA)_arg0EA & ~(UDATA)J9SF_A0_INVISIBLE_TAG);
		}
		if (J9NtcVoid == returnType) {
			_sp += 1;
		} else if ((J9NtcLong == returnType) || (J9NtcDouble == returnType)) {
#if !defined(J9VM_ENV_DATA64)
			*_sp = (UDATA)_currentThread->returnValue2;
#endif
			*--_sp = (UDATA)_currentThread->returnValue;
		} else if (J9NtcObject == returnType) {
			*_sp = _currentThread->returnValue;
		} else {
			*(U_32 *)_sp = (U_32)_currentThread->returnValue;
		}
		rc = EXECUTE_BYTECODE;
done:;
		return rc;
	}

	VMINLINE FFI_Return
	callCFunction(REGISTER_ARGS_LIST, void * jniMethodStartAddress, void *receiverAddress, UDATA *javaArgs, UDATA **bp, bool isStatic, U_8 *returnType)
	{
		/* Release VM access (all object pointers are indirect referenced from here on) */
		UDATA relativeBP = _arg0EA - *bp;
		updateVMStruct(REGISTER_ARGS);
		VM_VMAccess::inlineExitVMToJNI(_currentThread);
		VM_VMHelpers::beforeJNICall(_currentThread);
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
		if (J9_ARE_ANY_BITS_SET(_vm->sigFlags, J9_SIG_ZOS_CEEHDLR)) {
			_currentThread->tempSlot = (UDATA)jniMethodStartAddress;
			jniMethodStartAddress = (void*)CEEJNIWrapper;
		}
#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */
		UDATA oldVMState = pushVMState(REGISTER_ARGS, J9VMSTATE_JNI);
		FFI_Return result = cJNICallout(REGISTER_ARGS, receiverAddress, javaArgs, returnType, &(_currentThread->returnValue), jniMethodStartAddress, isStatic);
		popVMState(REGISTER_ARGS, oldVMState);
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
		if (J9_ARE_ANY_BITS_SET(_vm->sigFlags, J9_SIG_ZOS_CEEHDLR)) {
			/* verify that the native didn't resume execution after an LE condition and illegally return to Java */
			if (J9_ARE_ANY_BITS_SET(_currentThread->privateFlags, J9_PRIVATE_FLAGS_STACKS_OUT_OF_SYNC)) {
				javaAndCStacksMustBeInSync(_currentThread, FALSE);
			}
		}
#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */
		VM_VMHelpers::afterJNICall(_currentThread);
		/* Reacquire VM access (all object pointers can be direct referenced from this point on) */
		VM_VMAccess::inlineEnterVMFromJNI(_currentThread);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		*bp = _arg0EA - relativeBP;
		if ((UDATA)-1 == (((J9SFJNINativeMethodFrame *)(*bp + 1)) - 1)->specialFrameFlags) {
			result = ffiFailed;
		}
		if (ffiSuccess == result) {
			VM_VMHelpers::convertJNIReturnValue(*returnType, &(_currentThread->returnValue));
		}
		return result;
	}

	static const VMINLINE ffi_type *
	getFFIType(U_8 j9ntc) {
		static const ffi_type * const J9NtcToFFI[] = {
				&ffi_type_void,		/* J9NtcVoid */
				&ffi_type_uint8,	/* J9NtcBoolean */
				&ffi_type_sint8,	/* J9NtcByte */
				&ffi_type_uint16,	/* J9NtcChar */
				&ffi_type_sint16,	/* J9NtcShort */
				&ffi_type_float,	/* J9NtcFloat */
				&ffi_type_sint32,	/* J9NtcInt */
				&ffi_type_double,	/* J9NtcDouble */
				&ffi_type_sint64,	/* J9NtcLong */
				&ffi_type_pointer,	/* J9NtcObject */
				&ffi_type_pointer	/* J9NtcClass */
		};

		return J9NtcToFFI[j9ntc];
	}

	typedef struct J9JavaNativeBytecodeArrayHeader {
		U_8 nativeArgCount;
		U_8 nativeReturnType;
		U_8 nativeArgTypes[1];
	} J9JavaNativeBytecodeArrayHeader;

	VMINLINE FFI_Return
	cJNICallout(REGISTER_ARGS_LIST, void *receiverAddress, UDATA *javaArgs, U_8 *returnType, void *returnStorage, void *function, bool isStatic)
	{
		const U_8 extraArgs = 2;
		const U_8 minimalCallout = 14;
#if !defined(J9VM_ENV_LITTLE_ENDIAN)
		const U_8 extraBytesBoolAndByte = 3;
		const U_8 extraBytesShortAndChar = 2;
#endif /* J9VM_ENV_LITTLE_ENDIAN */
		FFI_Return ret = ffiOOM;
		ffi_type *sArgs[16];
		void *sValues[16];
		UDATA spValues[16];
		ffi_type **args = NULL;
		void **values = NULL;
#if FFI_NATIVE_RAW_API
		/* Make sure we can fit a double in each sValues_raw[] slot but assuring we end up
		 * with an int that is a  multiple of sizeof(ffi_raw)
		 */
		const U_8 valRawWorstCaseMulFactor = ((sizeof(double) - 1U)/sizeof(ffi_raw)) + 1U;
		ffi_raw sValues_raw[valRawWorstCaseMulFactor * 16];
		ffi_raw * values_raw = NULL;
#endif /* FFI_NATIVE_RAW_API */
		UDATA *pointerValues = NULL;
		J9JavaNativeBytecodeArrayHeader *bcArrayHeader = (J9JavaNativeBytecodeArrayHeader *)(_sendMethod->bytecodes);
		U_8 *argTypes = bcArrayHeader->nativeArgTypes;
		U_8 argCount = bcArrayHeader->nativeArgCount;
		*returnType = bcArrayHeader->nativeReturnType;
		bool isMinimal =  argCount <= minimalCallout;
		PORT_ACCESS_FROM_JAVAVM(_vm);

		/* Get a pointer to the first argument excluding the receiver for virtual */
		if (!isStatic) {
			javaArgs -= 1;
		}
		if (isMinimal) {
			args = sArgs;
			values = sValues;
			pointerValues = spValues;
#if FFI_NATIVE_RAW_API
			values_raw = sValues_raw;
#endif /* FFI_NATIVE_RAW_API */
		} else {
			args = (ffi_type **)j9mem_allocate_memory(sizeof(ffi_type *) * (argCount + extraArgs), OMRMEM_CATEGORY_VM);
			if (NULL == args) {
				goto ffi_exit;
			}

			values = (void **)j9mem_allocate_memory(sizeof(void *) * (argCount + extraArgs), OMRMEM_CATEGORY_VM);
			if (NULL == values) {
				goto ffi_exit;
			}

			pointerValues = (UDATA *)j9mem_allocate_memory(sizeof(UDATA) * argCount, OMRMEM_CATEGORY_VM);
			if (NULL == pointerValues) {
				goto ffi_exit;
			}
#if FFI_NATIVE_RAW_API
			values_raw = (ffi_raw *)j9mem_allocate_memory((valRawWorstCaseMulFactor * sizeof(ffi_raw)) * (argCount + extraArgs), OMRMEM_CATEGORY_VM);
			if (NULL == values_raw) {
				goto ffi_exit;
			}
#endif /* FFI_NATIVE_RAW_API */
		}
		{
			args[0] = &ffi_type_pointer;
			args[1] = &ffi_type_pointer;
			values[0] = (void *)&_currentThread;
			values[1] = &receiverAddress;

			javaArgs = javaArgs + 1;
			IDATA offset = 0;
			for (U_8 i = 0; i < argCount; i++) {
				args[i + extraArgs] = (ffi_type *)getFFIType(argTypes[i]);
				offset -= 1;
				if ((J9NtcLong == argTypes[i]) || (J9NtcDouble == argTypes[i])) {
					offset -= 1;
				}

				if (0 == javaArgs[offset]) {
					values[i + extraArgs] = &(javaArgs[offset]);
				} else if ((J9NtcClass == argTypes[i]) || (J9NtcObject == argTypes[i])) {
					/* ffi expects the address of the pointer which for JNI is the address of the stackslot */
					pointerValues[i] = (UDATA)(javaArgs + offset);
					values[i + extraArgs] = &pointerValues[i];
				} else {
					values[i + extraArgs] = &(javaArgs[offset]);
#if !defined(J9VM_ENV_LITTLE_ENDIAN)
					if ((J9NtcShort == argTypes[i]) || (J9NtcChar == argTypes[i])) {
						values[i + extraArgs] = (void *)((UDATA)values[i + extraArgs] + extraBytesShortAndChar);
					}else if ((J9NtcByte == argTypes[i]) || (J9NtcBoolean == argTypes[i])) {
						values[i + extraArgs] = (void *)((UDATA)values[i + extraArgs] + extraBytesBoolAndByte);
					}
#endif /*J9VM_ENV_LITTLE_ENDIAN */
				}
			}
			ffi_type *ffiReturnType = (ffi_type *)getFFIType(*returnType);
			ffi_status status = FFI_OK;

			ffi_cif cif_t;
			ffi_cif * const cif = &cif_t;
			if (FFI_OK != ffi_prep_cif(cif, FFI_DEFAULT_ABI, argCount + extraArgs, ffiReturnType, args)) {
				ret = ffiFailed;
				goto ffi_exit;
			}

			if (FFI_OK == status) {
#if FFI_NATIVE_RAW_API
				ffi_ptrarray_to_raw(cif, values, values_raw);
				ffi_raw_call(cif, FFI_FN(function), returnStorage, values_raw);
#else /* FFI_NATIVE_RAW_API */
				ffi_call(cif, FFI_FN(function), returnStorage, values);
#endif /* FFI_NATOVE_RAW_API */
				ret = ffiSuccess;
			}
		}
ffi_exit:
		if (!isMinimal) {
			j9mem_free_memory(args);
			j9mem_free_memory(values);
			j9mem_free_memory(pointerValues);
#if FFI_NATIVE_RAW_API
			j9mem_free_memory(values_raw);
#endif /* FFI_NATIVE_RAW_API */
		}
		return ret;
	}

	VMINLINE VM_BytecodeAction
	throwException(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		J9StackWalkState *walkState = _currentThread->stackWalkState;
		updateVMStruct(REGISTER_ARGS);
		/* Toss the pushes in the current stack frame so the GC won't need a description for them and build a special stack frame so we can find classes */
		prepareForExceptionThrow(_currentThread);
		/* Fetch the exception and clear it from the vmThread */
		j9object_t exception = _currentThread->currentException;
		_currentThread->currentException = NULL;
		/* Only report the event if the tag is set - this prevents reporting multiple throws of the same exception when returning from internal call-in */
		if (_currentThread->privateFlags & J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW) {
			if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_EXCEPTION_THROW)) {
				ALWAYS_TRIGGER_J9HOOK_VM_EXCEPTION_THROW(_vm->hookInterface, _currentThread, exception);
				if (immediateAsyncPending()) {
					VMStructHasBeenUpdated(REGISTER_ARGS);
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
			}
		}
		exception = walkStackForExceptionThrow(_currentThread, exception, FALSE);
		{
			/* Drop the decompilation records for frames removed from the stack if FSD is enabled */
			J9JITConfig *jitConfig = _vm->jitConfig;
			if (NULL != jitConfig) {
				if (jitConfig->fsdEnabled) {
					jitConfig->jitExceptionCaught(_currentThread);
				}
			}
		}
		if (J9_EXCEPT_SEARCH_JAVA_HANDLER == (UDATA)walkState->userData3) {
			_sp = walkState->unwindSP;
			*--_sp = (UDATA)exception;
			_pc = (U_8*)walkState->userData2;
			_arg0EA = walkState->arg0EA;
			_literals = walkState->literals;
			_currentThread->j2iFrame = walkState->j2iFrame;
			if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_EXCEPTION_CATCH)) {
				updateVMStruct(REGISTER_ARGS);
				ALWAYS_TRIGGER_J9HOOK_VM_EXCEPTION_CATCH(_vm->hookInterface, _currentThread, exception, NULL);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				*_sp = (UDATA)exception;
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
			}
		} else if (J9_EXCEPT_SEARCH_JNI_HANDLER == (UDATA)walkState->userData3) {
			_sp = walkState->unwindSP;
			_pc = walkState->pc;
			_arg0EA = walkState->arg0EA;
			_literals = walkState->literals;
			_currentThread->j2iFrame = walkState->j2iFrame;
			_currentThread->currentException = exception;
			/* Execute the call-in return bytecode at _pc */
		} else if (J9_EXCEPT_SEARCH_JIT_HANDLER == (UDATA)walkState->userData3) {
			_sp = walkState->unwindSP;
			_currentThread->jitException = (j9object_t)walkState->restartException;
			_currentThread->j2iFrame = walkState->j2iFrame;
			restorePreservedRegistersFromWalkState(REGISTER_ARGS, walkState);
			J9I2JState* const i2jState = walkState->i2jState;
			if (NULL != i2jState) {
				_currentThread->entryLocalStorage->i2jState = *i2jState;
			}
			_currentThread->tempSlot = (UDATA)walkState->userData2;
			_nextAction = J9_BCLOOP_LOAD_PRESERVED_AND_BRANCH;
			VM_JITInterface::enableRuntimeInstrumentation(_currentThread);
			rc = GOTO_DONE;
		} else {
			Assert_VM_unreachable();
		}
done:
		return rc;
	}

	VMINLINE J9ConstantPool *ramConstantPool(J9Method * &_literals) { return J9_CP_FROM_METHOD(_literals); }
	VMINLINE J9RAMConstantPoolItem *ramConstantPoolItem(J9Method * &_literals, U_16 index) { return (J9RAMConstantPoolItem*)ramConstantPool(_literals) + index; }
	VMINLINE J9ROMConstantPoolItem *romConstantPool(J9Method * &_literals) { return J9_ROM_CP_FROM_CP(ramConstantPool(_literals)); }
	VMINLINE J9ROMConstantPoolItem *romConstantPoolItem(J9Method * &_literals, U_16 index) { return romConstantPool(_literals) + index; }

	/* INL native implementations
	 *
	 * Each INL must pop its arguments from the stack, push the return value
	 * and increment the PC by 3 to skip over the invoke bytecode in the caller.
	 */

	VMINLINE void
	returnVoidFromINL(REGISTER_ARGS_LIST, UDATA slotCount)
	{
		_pc += 3;
		_sp += slotCount;
	}

	VMINLINE void
	returnSingleFromINL(REGISTER_ARGS_LIST, I_32 value, UDATA slotCount)
	{
		_pc += 3;
		_sp += (slotCount - 1);
		*(I_32*)_sp = value;
	}

	VMINLINE void
	returnDoubleFromINL(REGISTER_ARGS_LIST, I_64 value, UDATA slotCount)
	{
		_pc += 3;
		_sp += (slotCount - 2);
		*(I_64*)_sp = value;
	}

	VMINLINE void
	returnObjectFromINL(REGISTER_ARGS_LIST, j9object_t value, UDATA slotCount)
	{
		_pc += 3;
		_sp += (slotCount - 1);
		*(j9object_t*)_sp = value;
	}

	VMINLINE void
	returnThisFromINL(REGISTER_ARGS_LIST, UDATA slotCount)
	{
		_pc += 3;
		_sp += (slotCount - 1);
	}

	/* java.lang.Thread: public static native Thread _currentThread(); */
	VMINLINE VM_BytecodeAction
	inlThreadCurrentThread(REGISTER_ARGS_LIST)
	{
		returnObjectFromINL(REGISTER_ARGS, _currentThread->threadObject, 0);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.Thread: public static native Thread onSpinWait(); */
	VMINLINE VM_BytecodeAction
	inlThreadOnSpinWait(REGISTER_ARGS_LIST)
	{
		VM_AtomicSupport::yieldCPU();
		returnVoidFromINL(REGISTER_ARGS, 0);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.Thread: public static native boolean interrupted(); */
	VMINLINE VM_BytecodeAction
	inlThreadInterrupted(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 result = 0;
#if defined(WIN32)
		/* Synchronize on the thread lock around interrupted() on windows */
		j9object_t threadLock = J9VMJAVALANGTHREAD_LOCK(_currentThread, _currentThread->threadObject);
		if (!VM_ObjectMonitor::inlineFastObjectMonitorEnter(_currentThread, threadLock)) {
			IDATA monitorRC = objectMonitorEnterNonBlocking(_currentThread, threadLock);
			if (0 == monitorRC) {
				rc = THROW_MONITOR_ALLOC_FAIL;
				goto done;
			} else if (1 == monitorRC) {
				buildInternalNativeStackFrame(REGISTER_ARGS);
				updateVMStruct(REGISTER_ARGS);
				threadLock = (j9object_t)(UDATA)objectMonitorEnterBlocking(_currentThread);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				restoreInternalNativeStackFrame(REGISTER_ARGS);
			}
		}
		/* This field is only initialized on windows */
		if (NULL != _vm->sidecarClearInterruptFunction) {
			_vm->sidecarClearInterruptFunction(_currentThread);
		}
#endif /* WIN32 */
		if (0 != omrthread_clear_interrupted()) {
			result = 1;
		}
#if defined(WIN32)
		exitObjectMonitor(REGISTER_ARGS, threadLock);
#endif /* WIN32 */
		returnSingleFromINL(REGISTER_ARGS, result, 0);
#if defined(WIN32)
done:
#endif /* WIN32 */
		return rc;
	}

	/* java.lang.Object: public final native Class<? extends Object> getClass(); */
	VMINLINE VM_BytecodeAction
	inlObjectGetClass(REGISTER_ARGS_LIST)
	{
		returnObjectFromINL(REGISTER_ARGS, J9VM_J9CLASS_TO_HEAPCLASS(J9OBJECT_CLAZZ(_currentThread, *(j9object_t*) _sp)), 1);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.Object: public final native void notify(); */
	/* java.lang.Object: public final native void notifyAll(); */
	VMINLINE VM_BytecodeAction
	inlObjectNotify(REGISTER_ARGS_LIST, IDATA (*notifyFunction)(omrthread_monitor_t))
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t receiver = ((j9object_t*)_sp)[0];
		omrthread_monitor_t monitorPtr = NULL;

		if (VM_ObjectMonitor::getMonitorForNotify(_currentThread, receiver, &monitorPtr, true)) {
			if (0 != notifyFunction(monitorPtr)) {
				buildInternalNativeStackFrame(REGISTER_ARGS);
				rc = THROW_ILLEGAL_MONITOR_STATE;
				goto done;
			}
		} else if (NULL != monitorPtr) {
			buildInternalNativeStackFrame(REGISTER_ARGS);
			rc = THROW_ILLEGAL_MONITOR_STATE;
			goto done;
		}

		returnVoidFromINL(REGISTER_ARGS, 1);
done:
		return rc;
	}

	/* java.lang.Class: public native boolean isAssignableFrom(Class<?> cls); */
	VMINLINE VM_BytecodeAction
	inlClassIsAssignableFrom(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t cls = ((j9object_t*)_sp)[0];
		if (NULL == cls) {
			buildInternalNativeStackFrame(REGISTER_ARGS);
			rc = THROW_NPE;
		} else {
			I_32 result = 0;
			J9Class *parmClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, cls);
			J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, ((j9object_t*)_sp)[1]);
			if (J9ROMCLASS_IS_PRIMITIVE_TYPE(parmClazz->romClass) || J9ROMCLASS_IS_PRIMITIVE_TYPE(receiverClazz->romClass)) {
				if (parmClazz == receiverClazz) {
					result = 1;
				}
			} else {
				result = (I_32)VM_VMHelpers::inlineCheckCast(parmClazz, receiverClazz);
			}
			returnSingleFromINL(REGISTER_ARGS, result, 2);
		}
		return rc;
	}

	/* java.lang.Class: public native boolean isArray(); */
	VMINLINE VM_BytecodeAction
	inlClassIsArray(REGISTER_ARGS_LIST)
	{
		J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, *(j9object_t*)_sp);
		returnSingleFromINL(REGISTER_ARGS, (J9ROMCLASS_IS_ARRAY(receiverClazz->romClass) ? 1 : 0), 1);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.Class: public native boolean isPrimitive(); */
	VMINLINE VM_BytecodeAction
	inlClassIsPrimitive(REGISTER_ARGS_LIST)
	{
		J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, *(j9object_t*)_sp);
		returnSingleFromINL(REGISTER_ARGS, (J9ROMCLASS_IS_PRIMITIVE_TYPE(receiverClazz->romClass) ? 1 : 0), 1);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.Class: public native boolean isInstance(Object object); */
	VMINLINE VM_BytecodeAction
	inlClassIsInstance(REGISTER_ARGS_LIST)
	{
		I_32 result = 0;
		j9object_t object = ((j9object_t*)_sp)[0];
		/* null is not an instance of anything */
		if (NULL != object) {
			J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, ((j9object_t*)_sp)[1]);
			J9Class *objectClazz = J9OBJECT_CLAZZ(_currentThread, object);
			result = (I_32)VM_VMHelpers::inlineCheckCast(objectClazz, receiverClazz);
		}
		returnSingleFromINL(REGISTER_ARGS, result, 2);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.Class: private native int getModifiersImpl(); */
	VMINLINE VM_BytecodeAction
	inlClassGetModifiersImpl(REGISTER_ARGS_LIST)
	{
		J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, *(j9object_t*)_sp);
		J9ROMClass *romClass = NULL;
		bool isArray = J9CLASS_IS_ARRAY(receiverClazz);
		if (isArray) {
			/* Use underlying type for arrays */
			romClass = ((J9ArrayClass*)receiverClazz)->leafComponentType->romClass;
		} else {
			romClass = receiverClazz->romClass;
		}

		U_32 modifiers = 0;
		if (J9_ARE_NO_BITS_SET(romClass->extraModifiers, J9AccClassInnerClass)) {
			/* Not an inner class - use the modifiers field */
			modifiers = romClass->modifiers;
		} else {
			/* Use memberAccessFlags if the receiver is an inner class */
			modifiers = romClass->memberAccessFlags;
		}

		if (isArray) {
			/* OR in the required Sun bits */
			modifiers |= (J9AccAbstract + J9AccFinal);
		}
		returnSingleFromINL(REGISTER_ARGS, modifiers, 1);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.Class: public native Class<?> getComponentType(); */
	VMINLINE VM_BytecodeAction
	inlClassGetComponentType(REGISTER_ARGS_LIST)
	{
		J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, *(j9object_t*)_sp);
		j9object_t componentType = NULL;
		if (J9CLASS_IS_ARRAY(receiverClazz)) {
			componentType = J9VM_J9CLASS_TO_HEAPCLASS(((J9ArrayClass*)receiverClazz)->componentType);
		}
		returnObjectFromINL(REGISTER_ARGS, componentType, 1);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.Class: private native String getSimpleNameImpl(); */
	VMINLINE VM_BytecodeAction
	inlClassGetSimpleNameImpl(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		J9Class *receiverClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, *(j9object_t*)_sp);
		j9object_t simpleName = NULL;
		J9ROMClass *romClass = receiverClazz->romClass;
		J9UTF8 *simpleNameUTF = getSimpleNameForROMClass(_vm, receiverClazz->classLoader, romClass);
		if (NULL != simpleNameUTF) {
			buildInternalNativeStackFrame(REGISTER_ARGS);
			updateVMStruct(REGISTER_ARGS);
			simpleName = _vm->memoryManagerFunctions->j9gc_createJavaLangString(_currentThread, J9UTF8_DATA(simpleNameUTF), J9UTF8_LENGTH(simpleNameUTF), 0);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			releaseOptInfoBuffer(_vm, romClass);
			if (NULL == simpleName) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			restoreInternalNativeStackFrame(REGISTER_ARGS);
		}
		returnObjectFromINL(REGISTER_ARGS, simpleName, 1);
done:
		return rc;
	}

	/* java.lang.System: public static native void arraycopy(Object src, int srcPos, Object dest, int destPos, int length); */
	VMINLINE VM_BytecodeAction
	inlSystemArraycopy(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t src = *(j9object_t*)(_sp + 4);
		I_32 srcPos = *(I_32*)(_sp + 3);
		j9object_t dest = *(j9object_t*)(_sp + 2);
		I_32 destPos = *(I_32*)(_sp + 1);
		I_32 length = *(I_32*)_sp;

		if ((NULL == src) || (NULL == dest)) {
			buildInternalNativeStackFrame(REGISTER_ARGS);
			rc = THROW_NPE;
		} else {
			J9Class *srcClass = J9OBJECT_CLAZZ(_currentThread, src);
			J9ROMClass *srcRom = srcClass->romClass;
			if (J9ROMCLASS_IS_ARRAY(srcRom)) {
				J9Class *destClass = J9OBJECT_CLAZZ(_currentThread, dest);
				J9ROMClass *destRom = destClass->romClass;
				if (!J9ROMCLASS_IS_ARRAY(destRom)) {
					buildInternalNativeStackFrame(REGISTER_ARGS);
					rc = THROW_ARRAY_STORE;
				} else {
					if (OBJECT_HEADER_SHAPE_POINTERS == J9CLASS_SHAPE(srcClass)) {
						/* Reference array */
						if (OBJECT_HEADER_SHAPE_POINTERS != J9CLASS_SHAPE(destClass)) {
							buildInternalNativeStackFrame(REGISTER_ARGS);
							rc = THROW_ARRAY_STORE;
						} else {
							rc = doReferenceArrayCopy(REGISTER_ARGS, src, srcPos, dest, destPos, length);
						}
					} else {
						if (srcClass != destClass) {
							buildInternalNativeStackFrame(REGISTER_ARGS);
							rc = THROW_ARRAY_STORE;
						} else {
							rc = doPrimitiveArrayCopy(REGISTER_ARGS, src, srcPos, dest, destPos, ((J9ROMArrayClass*)srcClass->romClass)->arrayShape & 0x0000FFFF, length);
						}
					}
				}
			} else {
				buildInternalNativeStackFrame(REGISTER_ARGS);
				rc = THROW_ARRAY_STORE;
			}
		}

		if (EXECUTE_BYTECODE == rc) {
			returnVoidFromINL(REGISTER_ARGS, 5);
		}

		return rc;
	}

	/* java.lang.System: public static native long currentTimeMillis(); */
	VMINLINE VM_BytecodeAction
	inlSystemCurrentTimeMillis(REGISTER_ARGS_LIST)
	{
		PORT_ACCESS_FROM_JAVAVM(_vm);
		returnDoubleFromINL(REGISTER_ARGS, j9time_current_time_millis(), 0);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.System: public static native long nanoTime(); */
	VMINLINE VM_BytecodeAction
	inlSystemNanoTime(REGISTER_ARGS_LIST)
	{
		PORT_ACCESS_FROM_JAVAVM(_vm);
		returnDoubleFromINL(REGISTER_ARGS, j9time_nano_time(), 0);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.J9VMInternals: static native Class getSuperclass(Class clazz); */
	VMINLINE VM_BytecodeAction
	inlInternalsGetSuperclass(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t clazz = *(j9object_t*)_sp;
		J9Class *j9clazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, clazz);
		J9ROMClass *romClass = j9clazz->romClass;
		j9object_t superClazz = NULL;
		if (!(J9ROMCLASS_IS_INTERFACE(romClass) || J9ROMCLASS_IS_PRIMITIVE_TYPE(romClass))) {
			superClazz = J9VM_J9CLASS_TO_HEAPCLASS(VM_VMHelpers::getSuperclass(j9clazz));
		}
		returnObjectFromINL(REGISTER_ARGS, superClazz, 1);
		return rc;
	}

	/* java.lang.J9VMInternals: static native int identityHashCode(Object anObject); */
	VMINLINE VM_BytecodeAction
	inlInternalsIdentityHashCode(REGISTER_ARGS_LIST)
	{
		j9object_t obj = *(j9object_t*)_sp;
		/* Caller has null-checked obj already */
		_pc += 3;
		*(I_32*)_sp = VM_ObjectHash::inlineObjectHashCode(_vm, obj);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.String: public native String intern(); */
	VMINLINE VM_BytecodeAction
	inlStringIntern(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t stringObject = *(j9object_t*)_sp;
		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		stringObject = _vm->memoryManagerFunctions->j9gc_internString(_currentThread, stringObject);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		if (NULL == stringObject) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		} else {
			restoreInternalNativeStackFrame(REGISTER_ARGS);
			returnObjectFromINL(REGISTER_ARGS, stringObject, 1);
		}
		return rc;
	}

	/* java.lang.Throwable: public native Throwable fillInStackTrace(); */
	VMINLINE VM_BytecodeAction
	inlThrowableFillInStackTrace(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		/* Don't fill in stack traces if -XX:-StackTraceInThrowable is in effect */
		if (0 == (_vm->runtimeFlags & J9_RUNTIME_OMIT_STACK_TRACES)) {
			j9object_t receiver = *(j9object_t*)_sp;
			/* If the enableWritableStackTrace field is unresolved (i.e. doesn't exist) or is set to true,
			 * continue filling in the stack trace.
			 */
			if (VM_VMHelpers::vmConstantPoolFieldIsResolved(_vm, J9VMCONSTANTPOOL_JAVALANGTHROWABLE_ENABLEWRITABLESTACKTRACE)
				&& J9VMJAVALANGTHROWABLE_ENABLEWRITABLESTACKTRACE(_currentThread, receiver)
			) {
				buildInternalNativeStackFrame(REGISTER_ARGS);
				j9object_t walkback = (j9object_t)J9VMJAVALANGTHROWABLE_WALKBACK(_currentThread, receiver);
				J9StackWalkState *walkState = _currentThread->stackWalkState;
				UDATA walkFlags = J9_STACKWALK_CACHE_PCS | J9_STACKWALK_WALK_TRANSLATE_PC |
					J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_SKIP_INLINES;
				/* Do not hide exception frames if fillInStackTrace is called on an exception which already
				 * has a stack trace.  In the out of memory case, there is a bit indicating that we should
				 * explicitly override this behaviour, since we've precached the stack trace array.
				 */
				if ((NULL == walkback) || (_currentThread->privateFlags & J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE)) {
					walkFlags |= J9_STACKWALK_HIDE_EXCEPTION_FRAMES;
					walkState->restartException = receiver;
				}
				walkState->flags = walkFlags;
				walkState->skipCount = 1;	// skip the INL frame
				walkState->walkThread = _currentThread;
				updateVMStruct(REGISTER_ARGS);
				UDATA walkRC = _vm->walkStackFrames(_currentThread, walkState);
				/* No need for VMStructHasBeenUpdated as the above walk cannot change the roots */
				UDATA framesWalked = walkState->framesWalked;
				UDATA *cachePointer = walkState->cache;
				if (J9_STACKWALK_RC_NONE != walkRC) {
					/* Avoid infinite recursion if already throwing OOM */
					if (_currentThread->privateFlags & J9_PRIVATE_FLAGS_OUT_OF_MEMORY) {
						goto recursiveOOM;
					}
					/* vmStruct is already up-to-date */
					setNativeOutOfMemoryError(_currentThread, J9NLS_JCL_FAILED_TO_CREATE_STACK_TRACE);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
				/* If there is no stack trace in the exception, or we are not in the out of memory case,
				 * allocate a new stack trace.  The cached receiver object is invalid after this point.
				 */
				if ((NULL == walkback) || (0 == (_currentThread->privateFlags & J9_PRIVATE_FLAGS_FILL_EXISTING_TRACE))) {
#if defined(J9VM_ENV_DATA64)
					J9Class *arrayClass = _vm->longArrayClass;
#else
					J9Class *arrayClass = _vm->intArrayClass;
#endif
					walkback = allocateIndexableObject(REGISTER_ARGS, arrayClass, (U_32)framesWalked, false);
					if (J9_UNEXPECTED(NULL == walkback)) {
						rc = THROW_HEAP_OOM;
						goto done;
					}
				} else {
					/* Using existing array - be sure not to overrun it */
					UDATA maxSize = J9INDEXABLEOBJECT_SIZE(_currentThread, walkback);
					if (framesWalked > maxSize) {
						framesWalked = maxSize;
					}
				}
				for (UDATA i = 0; i < framesWalked; ++i) {
#if defined(J9VM_ENV_DATA64)
					_objectAccessBarrier.inlineIndexableObjectStoreI64(_currentThread, walkback, i, cachePointer[i]);
#else
					_objectAccessBarrier.inlineIndexableObjectStoreI32(_currentThread, walkback, i, cachePointer[i]);
#endif
				}
				freeStackWalkCaches(_currentThread, walkState);
recursiveOOM:
				restoreInternalNativeStackFrame(REGISTER_ARGS);
				receiver = *(j9object_t*)_sp;
				J9VMJAVALANGTHROWABLE_SET_WALKBACK(_currentThread, receiver, walkback);
				J9VMJAVALANGTHROWABLE_SET_STACKTRACE(_currentThread, receiver, NULL);
			}
		}
		returnThisFromINL(REGISTER_ARGS, 1);
done:
		return rc;
	}

	/* java.lang.J9VMInternals: native static Object newInstanceImpl(Class clazz) throws IllegalAccessException, InstantiationException; */
	VMINLINE VM_BytecodeAction
	inlInternalsNewInstanceImpl(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t clazz = *(j9object_t*)_sp;
		j9object_t instance = NULL;
		buildInternalNativeStackFrame(REGISTER_ARGS);
		J9Class *j9clazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, clazz);
		/* senderClass == NULL means that no protection checking will be done, which is OK if the constructor is public */
		J9Class *senderClass = NULL;
		/* The cache is only filled in for public constructors, and only if the class can be instantiated and is initialized */
		J9Method *method = j9clazz->initializerCache;
		J9StackWalkState *walkState = NULL;
		if (NULL == method) {
			/* Ensure this class is instantiable */
			if (J9_UNEXPECTED(!VM_VMHelpers::classCanBeInstantiated(j9clazz))) {
				updateVMStruct(REGISTER_ARGS);
				setCurrentException(_currentThread, J9_EX_CTOR_CLASS | J9VMCONSTANTPOOL_JAVALANGINSTANTIATIONEXCEPTION, (UDATA*)clazz);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			/* Find the class of the method which called newInstance */
			walkState = _currentThread->stackWalkState;
			walkState->userData1 = (void*)2; /* skip newInstanceImpl and newInstance */
			walkState->userData2 = NULL;
			walkState->userData3 = (void *) FALSE;
			walkState->frameWalkFunction = cInterpGetStackClassJEP176Iterator;

			walkState->skipCount = 0;
			walkState->walkThread = _currentThread;
			walkState->flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_ITERATE_FRAMES;
			updateVMStruct(REGISTER_ARGS);
			_vm->walkStackFrames(_currentThread, walkState);

			/* throw out pending InternalError exception first if found */
			if (TRUE == (UDATA)walkState->userData3) {
				setCurrentExceptionNLS(_currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, J9NLS_VM_CALLER_NOT_ANNOTATED_AS_CALLERSENSITIVE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}

			senderClass = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, walkState->userData2);
			/* See if the class is visible to the sender */
			if (NULL != senderClass) {
				IDATA checkResult = checkVisibility(_currentThread, senderClass, j9clazz, j9clazz->romClass->modifiers, J9_LOOK_REFLECT_CALL);
				if (checkResult < J9_VISIBILITY_ALLOWED) {
					char *nlsStr = illegalAccessMessage(_currentThread, -1, senderClass, j9clazz, checkResult);
					/* VM struct is already up-to-date */
					setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSEXCEPTION, nlsStr);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					PORT_ACCESS_FROM_JAVAVM(_vm);
					j9mem_free_memory(nlsStr);
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
			}
			rc = initializeClassIfNeeded(REGISTER_ARGS, j9clazz);
			if (J9_UNEXPECTED(EXECUTE_BYTECODE != rc)) {
				goto done;
			}
		} else {
		}
		instance = allocateObject(REGISTER_ARGS, j9clazz);
		if (J9_UNEXPECTED(NULL == instance)) {
			rc = THROW_HEAP_OOM;
			goto done;
		}
		/* If the method is known and empty, just do the post-construction finalize check instead of calling in to send the method */
		if ((NULL != method) && J9ROMMETHOD_IS_EMPTY(J9_ROM_METHOD_FROM_RAM_METHOD(method))) {
			VM_VMHelpers::checkIfFinalizeObject(_currentThread, instance);
		} else {
			pushObjectInSpecialFrame(REGISTER_ARGS, instance);
			updateVMStruct(REGISTER_ARGS);
			sendInit(_currentThread, instance, senderClass, J9_LOOK_NEW_INSTANCE|J9_LOOK_REFLECT_CALL, 0);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			instance = popObjectInSpecialFrame(REGISTER_ARGS);
		}
		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnObjectFromINL(REGISTER_ARGS, instance, 1);
done:
		return rc;
	}

	/* java.lang.J9VMInternals: static native Object primitiveClone(Object anObject) throws CloneNotSupportedException; */
	VMINLINE VM_BytecodeAction
	inlInternalsPrimitiveClone(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t original = *(j9object_t*)_sp;
		j9object_t copy = NULL;
		buildInternalNativeStackFrame(REGISTER_ARGS);
		/* Check to see if the original object is cloneable */
		J9Class *objectClass = J9OBJECT_CLAZZ(_currentThread, original);
		UDATA flags = J9CLASS_FLAGS(objectClass);
		if (J9_UNEXPECTED(0 == (flags & J9AccClassCloneable))) {
			updateVMStruct(REGISTER_ARGS);
			setCurrentException(_currentThread, J9VMCONSTANTPOOL_JAVALANGCLONENOTSUPPORTEDEXCEPTION, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		if (flags & J9AccClassArray) {
			U_32 size = J9INDEXABLEOBJECT_SIZE(_currentThread, original);
			copy = _objectAllocate.inlineAllocateIndexableObject(_currentThread, objectClass, size, false, false, false);
			if (NULL == copy) {
				pushObjectInSpecialFrame(REGISTER_ARGS, original);
				updateVMStruct(REGISTER_ARGS);
				copy = _vm->memoryManagerFunctions->J9AllocateIndexableObject(_currentThread, objectClass, size, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				original = popObjectInSpecialFrame(REGISTER_ARGS);
				if (J9_UNEXPECTED(NULL == copy)) {
					rc = THROW_HEAP_OOM;
					goto done;
				}
				objectClass = VM_VMHelpers::currentClass(objectClass);
			}
			_objectAccessBarrier.cloneArray(_currentThread, original, copy, objectClass, size);
		} else {
			copy = _objectAllocate.inlineAllocateObject(_currentThread, objectClass, false, false);
			if (NULL == copy) {
				pushObjectInSpecialFrame(REGISTER_ARGS, original);
				updateVMStruct(REGISTER_ARGS);
				copy = _vm->memoryManagerFunctions->J9AllocateObject(_currentThread, objectClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				original = popObjectInSpecialFrame(REGISTER_ARGS);
				if (J9_UNEXPECTED(NULL == copy)) {
					rc = THROW_HEAP_OOM;
					goto done;
				}
				objectClass = VM_VMHelpers::currentClass(objectClass);
			}
			_objectAccessBarrier.cloneObject(_currentThread, original, copy, objectClass);
			VM_VMHelpers::checkIfFinalizeObject(_currentThread, copy);
		}
		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnObjectFromINL(REGISTER_ARGS, copy, 1);
done:
		return rc;
	}

	/* java.lang.ref.Reference: private native T getImpl() */
	VMINLINE VM_BytecodeAction
	inlReferenceGetImpl(REGISTER_ARGS_LIST)
	{
		/* Only called from java for metronome GC policy */
		j9object_t referent = _vm->memoryManagerFunctions->j9gc_objaccess_referenceGet(_currentThread, *(j9object_t*)_sp);
		returnObjectFromINL(REGISTER_ARGS, referent, 1);
		return EXECUTE_BYTECODE;
	}

	VMINLINE VM_BytecodeAction
	unsafePutEpilogue(REGISTER_ARGS_LIST, UDATA argCount)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;

		if (J9_ARE_NO_BITS_SET(_currentThread->privateFlags2, J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS)) {
			prepareForExceptionThrow(_currentThread);
			setCurrentException(_currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		} else {
			_currentThread->privateFlags2 &= ~J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
			restoreInternalNativeStackFrame(REGISTER_ARGS);
			returnVoidFromINL(REGISTER_ARGS, argCount);
		}
		return rc;
	}

	VMINLINE VM_BytecodeAction
	unsafeGetEpilogue(REGISTER_ARGS_LIST, I_64 value, UDATA argCount, bool returnDouble)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;

		if (J9_ARE_NO_BITS_SET(_currentThread->privateFlags2, J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS)) {
			prepareForExceptionThrow(_currentThread);
			setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, "SIGBUS");
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		} else {
			_currentThread->privateFlags2 &= ~J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
			restoreInternalNativeStackFrame(REGISTER_ARGS);
			if (returnDouble) {
				returnDoubleFromINL(REGISTER_ARGS, value, argCount);
			} else {
				returnSingleFromINL(REGISTER_ARGS, (I_32)value, argCount);
			}
		}
		return rc;
	}

	/* sun.misc.Unsafe: public native Object allocateInstance(Class clazz); */
	VMINLINE VM_BytecodeAction
	inlUnsafeAllocateInstance(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t classObject = *(j9object_t*)_sp;
		J9Class *clazz = NULL;
		j9object_t instance = NULL;
		buildInternalNativeStackFrame(REGISTER_ARGS);
		if (NULL == classObject) {
			rc = THROW_NPE;
			goto done;
		}
		clazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, classObject);
		if (!VM_VMHelpers::classCanBeInstantiated(clazz)) {
			updateVMStruct(REGISTER_ARGS);
			setCurrentException(_currentThread, J9_EX_CTOR_CLASS | J9VMCONSTANTPOOL_JAVALANGINSTANTIATIONEXCEPTION, (UDATA*)classObject);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		rc = initializeClassIfNeeded(REGISTER_ARGS, clazz);
		if (J9_UNEXPECTED(EXECUTE_BYTECODE != rc)) {
			goto done;
		}
		instance = allocateObject(REGISTER_ARGS, clazz);
		if (NULL == instance) {
			rc = THROW_HEAP_OOM;
			goto done;
		}
		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnObjectFromINL(REGISTER_ARGS, instance, 2);
done:
		return rc;
	}

	/* sun.misc.Unsafe: public native byte getByte(long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetByteNative(REGISTER_ARGS_LIST)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		I_8 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = UNSAFE_GET8(offset);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 3, false);
	}

	/* sun.misc.Unsafe: public native byte getByte(Object obj, long offset); */
	/* sun.misc.Unsafe: public native byte getByteVolatile(Object obj, long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetByte(REGISTER_ARGS_LIST, bool isVolatile)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		j9object_t obj = *(j9object_t*)(_sp + 2);
		I_8 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = VM_UnsafeAPI::getByte(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 4, false);
	}

	/* sun.misc.Unsafe: public native void putByte(long offset, byte value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutByteNative(REGISTER_ARGS_LIST)
	{
		I_8 value = (I_8)*(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		UNSAFE_PUT8(offset, value);
		return unsafePutEpilogue(REGISTER_ARGS, 4);
	}

	/* sun.misc.Unsafe: public native void putByte(Object obj, long offset, byte value); */
	/* sun.misc.Unsafe: public native void putByteVolatile(Object obj, long offset, byte value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutByte(REGISTER_ARGS_LIST, bool isVolatile)
	{
		I_8 value = (I_8)*(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);
		j9object_t obj = *(j9object_t*)(_sp + 3);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		VM_UnsafeAPI::putByte(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile, (I_32)value);
		return unsafePutEpilogue(REGISTER_ARGS, 5);
	}

	/* sun.misc.Unsafe: public native boolean getBoolean(Object obj, long offset); */
	/* sun.misc.Unsafe: public native boolean getBooleanVolatile(Object obj, long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetBoolean(REGISTER_ARGS_LIST, bool isVolatile)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		j9object_t obj = *(j9object_t*)(_sp + 2);
		U_8 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = VM_UnsafeAPI::getBoolean(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 4, false);
	}

	/* sun.misc.Unsafe: public native void putBoolean(Object obj, long offset, boolean value); */
	/* sun.misc.Unsafe: public native void putBooleanVolatile(Object obj, long offset, boolean value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutBoolean(REGISTER_ARGS_LIST, bool isVolatile)
	{
		U_8 value = (U_8)*(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);
		j9object_t obj = *(j9object_t*)(_sp + 3);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		VM_UnsafeAPI::putBoolean(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile, value);
		return unsafePutEpilogue(REGISTER_ARGS, 5);
	}

	/* sun.misc.Unsafe: public native short getShort(long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetShortNative(REGISTER_ARGS_LIST)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		I_16 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = UNSAFE_GET16(offset);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 3, false);
	}

	/* sun.misc.Unsafe: public native short getShort(Object obj, long offset); */
	/* sun.misc.Unsafe: public native short getShortVolatile(Object obj, long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetShort(REGISTER_ARGS_LIST, bool isVolatile)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		j9object_t obj = *(j9object_t*)(_sp + 2);
		I_16 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = VM_UnsafeAPI::getShort(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 4, false);
	}

	/* sun.misc.Unsafe: public native void putShort(long offset, short value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutShortNative(REGISTER_ARGS_LIST)
	{
		I_16 value = (I_16)*(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		UNSAFE_PUT16(offset, value);
		return unsafePutEpilogue(REGISTER_ARGS, 4);
	}

	/* sun.misc.Unsafe: public native void putShort(Object obj, long offset, short value); */
	/* sun.misc.Unsafe: public native void putShortVolatile(Object obj, long offset, short value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutShort(REGISTER_ARGS_LIST, bool isVolatile)
	{
		I_16 value = (I_16)*(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);
		j9object_t obj = *(j9object_t*)(_sp + 3);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		VM_UnsafeAPI::putShort(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile, value);
		return unsafePutEpilogue(REGISTER_ARGS, 5);
	}

	/* sun.misc.Unsafe: public native char getChar(long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetCharNative(REGISTER_ARGS_LIST)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		U_16 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = (U_16)UNSAFE_GET16(offset);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 3, false);
	}

	/* sun.misc.Unsafe: public native char getChar(Object obj, long offset); */
	/* sun.misc.Unsafe: public native char getCharVolatile(Object obj, long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetChar(REGISTER_ARGS_LIST, bool isVolatile)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		j9object_t obj = *(j9object_t*)(_sp + 2);
		U_16 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = VM_UnsafeAPI::getChar(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 4, false);
	}

	/* sun.misc.Unsafe: public native void putChar(long offset, char value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutCharNative(REGISTER_ARGS_LIST)
	{
		U_16 value = (U_16)*(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		UNSAFE_PUT16(offset, (I_16)value);
		return unsafePutEpilogue(REGISTER_ARGS, 4);
	}

	/* sun.misc.Unsafe: public native void putChar(Object obj, long offset, char value); */
	/* sun.misc.Unsafe: public native void putCharVolatile(Object obj, long offset, char value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutChar(REGISTER_ARGS_LIST, bool isVolatile)
	{
		U_16 value = (U_16)*(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);
		j9object_t obj = *(j9object_t*)(_sp + 3);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		VM_UnsafeAPI::putChar(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile, value);
		return unsafePutEpilogue(REGISTER_ARGS, 5);
	}

	/* sun.misc.Unsafe: public native int getInt(long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetIntNative(REGISTER_ARGS_LIST)
	{
		I_64 offset = (UDATA)*(I_64*)_sp;
		I_32 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = UNSAFE_GET32(offset);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 3, false);
	}

	/* sun.misc.Unsafe: public native int getInt(Object obj, long offset); */
	/* sun.misc.Unsafe: public native int getIntVolatile(Object obj, long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetInt(REGISTER_ARGS_LIST, bool isVolatile)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		j9object_t obj = *(j9object_t*)(_sp + 2);
		I_32 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = VM_UnsafeAPI::getInt(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 4, false);
	}

	/* sun.misc.Unsafe: public native int putInt(long offset, int value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutIntNative(REGISTER_ARGS_LIST)
	{
		I_32 value = *(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		UNSAFE_PUT32(offset, value);
		return unsafePutEpilogue(REGISTER_ARGS, 4);
	}

	/* sun.misc.Unsafe: public native void putInt(Object obj, long offset, int value); */
	/* sun.misc.Unsafe: public native void putIntVolatile(Object obj, long offset, int value); */
	/* sun.misc.Unsafe: public native void putOrderedInt(Object obj, long offset, int value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutInt(REGISTER_ARGS_LIST, bool isVolatile)
	{
		I_32 value = *(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);
		j9object_t obj = *(j9object_t*)(_sp + 3);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		VM_UnsafeAPI::putInt(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile, value);
		return unsafePutEpilogue(REGISTER_ARGS, 5);
	}

	/* sun.misc.Unsafe: public native float getFloat(long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetFloatNative(REGISTER_ARGS_LIST)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		U_32 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = (U_32)UNSAFE_GET32(offset);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 3, false);
	}

	/* sun.misc.Unsafe: public native float getFloat(Object obj, long offset); */
	/* sun.misc.Unsafe: public native float getFloatVolatile(Object obj, long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetFloat(REGISTER_ARGS_LIST, bool isVolatile)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		j9object_t obj = *(j9object_t*)(_sp + 2);
		U_32 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = VM_UnsafeAPI::getFloat(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 4, false);
	}

	/* sun.misc.Unsafe: public native void putFloat(long offset, float value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutFloatNative(REGISTER_ARGS_LIST)
	{
		U_32 value = (U_32)*(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		UNSAFE_PUT32(offset, (I_32)value);
		return unsafePutEpilogue(REGISTER_ARGS, 4);
	}

	/* sun.misc.Unsafe: public native void putFloat(Object obj, long offset, float value); */
	/* sun.misc.Unsafe: public native void putFloatVolatile(Object obj, long offset, float value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutFloat(REGISTER_ARGS_LIST, bool isVolatile)
	{
		U_32 value = (U_32)*(I_32*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);
		j9object_t obj = *(j9object_t*)(_sp + 3);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		VM_UnsafeAPI::putFloat(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile, value);
		return unsafePutEpilogue(REGISTER_ARGS, 5);
	}

	/* sun.misc.Unsafe: public native long getLong(long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetLongNative(REGISTER_ARGS_LIST)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		I_64 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = UNSAFE_GET64(offset);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 3, true);
	}

	/* sun.misc.Unsafe: public native long getLong(Object obj, long offset); */
	/* sun.misc.Unsafe: public native long getLongVolatile(Object obj, long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetLong(REGISTER_ARGS_LIST, bool isVolatile)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		j9object_t obj = *(j9object_t*)(_sp + 2);
		I_64 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = VM_UnsafeAPI::getLong(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 4, true);
	}

	/* sun.misc.Unsafe: public native void putLong(long offset, long value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutLongNative(REGISTER_ARGS_LIST)
	{
		I_64 value = (I_64)*(I_64*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 2);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		UNSAFE_PUT64(offset, value);
		return unsafePutEpilogue(REGISTER_ARGS, 5);
	}

	/* sun.misc.Unsafe: public native void putLong(Object obj, long offset, long value); */
	/* sun.misc.Unsafe: public native void putLongVolatile(Object obj, long offset, long value); */
	/* sun.misc.Unsafe: public native void putOrderedLong(Object obj, long offset, long value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutLong(REGISTER_ARGS_LIST, bool isVolatile)
	{
		I_64 value = (I_64)*(I_64*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 2);
		j9object_t obj = *(j9object_t*)(_sp + 4);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		VM_UnsafeAPI::putLong(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile, value);
		return unsafePutEpilogue(REGISTER_ARGS, 6);
	}

	/* sun.misc.Unsafe: public native double getDouble(long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetDoubleNative(REGISTER_ARGS_LIST)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		U_64 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = (U_64)UNSAFE_GET64(offset);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 3, true);
	}

	/* sun.misc.Unsafe: public native double getDouble(Object obj, long offset); */
	/* sun.misc.Unsafe: public native double getDoubleVolatile(Object obj, long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetDouble(REGISTER_ARGS_LIST, bool isVolatile)
	{
		UDATA offset = (UDATA)*(I_64*)_sp;
		j9object_t obj = *(j9object_t*)(_sp + 2);
		U_64 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = VM_UnsafeAPI::getDouble(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 4, true);
	}

	/* sun.misc.Unsafe: public native void putDouble(long offset, double value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutDoubleNative(REGISTER_ARGS_LIST)
	{
		I_64 value = (I_64)*(I_64*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 2);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		UNSAFE_PUT64(offset, value);
		return unsafePutEpilogue(REGISTER_ARGS, 5);
	}

	/* sun.misc.Unsafe: public native void putDouble(Object obj, long offset, double value); */
	/* sun.misc.Unsafe: public native void putDoubleVolatile(Object obj, long offset, double value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutDouble(REGISTER_ARGS_LIST, bool isVolatile)
	{
		U_64 value = (U_64)*(I_64*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 2);
		j9object_t obj = *(j9object_t*)(_sp + 4);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		VM_UnsafeAPI::putDouble(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile, value);
		return unsafePutEpilogue(REGISTER_ARGS, 6);
	}

	/* sun.misc.Unsafe: public native Object getObject(Object obj, long offset); */
	/* sun.misc.Unsafe: public native Object getObjectVolatile(Object obj, long offset); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetObject(REGISTER_ARGS_LIST, bool isVolatile)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		UDATA offset = (UDATA)*(I_64*)_sp;
		j9object_t obj = *(j9object_t*)(_sp + 2);
		j9object_t value = VM_UnsafeAPI::getObject(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile);
		returnObjectFromINL(REGISTER_ARGS, value, 4);
		return rc;
	}

	/* sun.misc.Unsafe: public native void putObject(Object obj, long offset, Object value); */
	/* sun.misc.Unsafe: public native void putObjectVolatile(Object obj, long offset, Object value); */
	/* sun.misc.Unsafe: public native void putOrderedObject(Object obj, long offset, Object value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutObject(REGISTER_ARGS_LIST, bool isVolatile)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t value = *(j9object_t*)_sp;
		UDATA offset = (UDATA)*(I_64*)(_sp + 1);
		j9object_t obj = *(j9object_t*)(_sp + 3);
		
		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		VM_UnsafeAPI::putObject(_currentThread, &_objectAccessBarrier, obj, offset, isVolatile, value);
		restoreInternalNativeStackFrame(REGISTER_ARGS);

		returnVoidFromINL(REGISTER_ARGS, 5);
		return rc;
	}

	/* sun.misc.Unsafe: public native long getAddress(long address); */
	VMINLINE VM_BytecodeAction
	inlUnsafeGetAddress(REGISTER_ARGS_LIST)
	{
		I_64 address = *(I_64*)_sp;
		I_64 value = 0;

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		value = VM_UnsafeAPI::getAddress(address);
		return unsafeGetEpilogue(REGISTER_ARGS, value, 3, true);
	}

	/* sun.misc.Unsafe: public native void putAddress(long address, long value); */
	VMINLINE VM_BytecodeAction
	inlUnsafePutAddress(REGISTER_ARGS_LIST)
	{
		I_64 value = *(I_64*)_sp;
		I_64 address = *(I_64*)(_sp + 2);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		_currentThread->privateFlags2 |= J9_PRIVATE_FLAGS2_UNSAFE_HANDLE_SIGBUS;
		VM_UnsafeAPI::putAddress(address, value);
		return unsafePutEpilogue(REGISTER_ARGS, 5);
	}

	/* sun.misc.Unsafe: public native int arrayBaseOffset(java.lang.Class); */
	VMINLINE VM_BytecodeAction
	inlUnsafeArrayBaseOffset(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t classObject = *(j9object_t*)_sp;
		if (NULL == classObject) {
			buildInternalNativeStackFrame(REGISTER_ARGS);
			rc = THROW_NPE;
		} else {
			J9Class *arrayClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, classObject);
			if (J9CLASS_IS_ARRAY(arrayClazz)) {
				returnSingleFromINL(REGISTER_ARGS, VM_UnsafeAPI::arrayBaseOffset((J9ArrayClass*)arrayClazz), 2);
			} else {
				buildInternalNativeStackFrame(REGISTER_ARGS);
				updateVMStruct(REGISTER_ARGS);
				prepareForExceptionThrow(_currentThread);
				setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				rc = GOTO_THROW_CURRENT_EXCEPTION;
			}
		}
		return rc;
	}

	/* sun.misc.Unsafe: public native int arrayIndexScale(java.lang.Class); */
	VMINLINE VM_BytecodeAction
	inlUnsafeArrayIndexScale(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t classObject = *(j9object_t*)_sp;
		if (NULL == classObject) {
			buildInternalNativeStackFrame(REGISTER_ARGS);
			rc = THROW_NPE;
		} else {
			J9Class *arrayClazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, classObject);
			if (J9CLASS_IS_ARRAY(arrayClazz)) {
				returnSingleFromINL(REGISTER_ARGS, VM_UnsafeAPI::arrayIndexScale((J9ArrayClass*)arrayClazz), 2);
			} else {
				buildInternalNativeStackFrame(REGISTER_ARGS);
				updateVMStruct(REGISTER_ARGS);
				prepareForExceptionThrow(_currentThread);
				setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				rc = GOTO_THROW_CURRENT_EXCEPTION;
			}
		}
		return rc;
	}

	/* sun.misc.Unsafe: public native int addressSize(); */
	VMINLINE VM_BytecodeAction
	inlUnsafeAddressSize(REGISTER_ARGS_LIST)
	{
		returnSingleFromINL(REGISTER_ARGS, VM_UnsafeAPI::addressSize(), 1);
		return EXECUTE_BYTECODE;
	}

	/* sun.misc.Unsafe: public native void loadFence(); */
	VMINLINE VM_BytecodeAction
	inlUnsafeLoadFence(REGISTER_ARGS_LIST)
	{
		VM_UnsafeAPI::loadFence();
		returnVoidFromINL(REGISTER_ARGS, 1);
		return EXECUTE_BYTECODE;
	}

	/* sun.misc.Unsafe: public native void storeFence(); */
	VMINLINE VM_BytecodeAction
	inlUnsafeStoreFence(REGISTER_ARGS_LIST)
	{
		VM_UnsafeAPI::storeFence();
		returnVoidFromINL(REGISTER_ARGS, 1);
		return EXECUTE_BYTECODE;
	}

	/* sun.misc.Unsafe: public final native boolean compareAndSwapObject(java.lang.Object obj, long offset, java.lang.Object compareValue, java.lang.Object swapValue); */
	VMINLINE VM_BytecodeAction
	inlUnsafeCompareAndSwapObject(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t swapValue = *(j9object_t*)_sp;
		j9object_t compareValue = *(j9object_t*)(_sp + 1);
		UDATA offset = (UDATA)*(I_64*)(_sp + 2);
		j9object_t obj = *(j9object_t*)(_sp + 4);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		bool value = VM_UnsafeAPI::compareAndSwapObject(_currentThread, &_objectAccessBarrier, obj, offset, compareValue, swapValue);
		restoreInternalNativeStackFrame(REGISTER_ARGS);

		returnSingleFromINL(REGISTER_ARGS, value ? 1 : 0, 6);
		return rc;
	}

	/* sun.misc.Unsafe: public final native boolean compareAndSwapLong(java.lang.Object obj, long offset, long compareValue, long swapValue); */
	VMINLINE VM_BytecodeAction
	inlUnsafeCompareAndSwapLong(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_64 swapValue = *(U_64*)_sp;
		U_64 compareValue = *(U_64*)(_sp + 2);
		UDATA offset = (UDATA)*(I_64*)(_sp + 4);
		j9object_t obj = *(j9object_t*)(_sp + 6);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		bool value = VM_UnsafeAPI::compareAndSwapLong(_currentThread, &_objectAccessBarrier, obj, offset, compareValue, swapValue);
		restoreInternalNativeStackFrame(REGISTER_ARGS);

		returnSingleFromINL(REGISTER_ARGS, value ? 1 : 0, 8);
		return rc;
	}

	/* sun.misc.Unsafe: public final native boolean compareAndSwapInt(java.lang.Object obj, long offset, int compareValue, int swapValue); */
	VMINLINE VM_BytecodeAction
	inlUnsafeCompareAndSwapInt(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_32 swapValue = *(U_32*)_sp;
		U_32 compareValue = *(U_32*)(_sp + 1);
		UDATA offset = (UDATA)*(I_64*)(_sp + 2);
		j9object_t obj = *(j9object_t*)(_sp + 4);

		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		bool value = VM_UnsafeAPI::compareAndSwapInt(_currentThread, &_objectAccessBarrier, obj, offset, compareValue, swapValue);
		restoreInternalNativeStackFrame(REGISTER_ARGS);

		returnSingleFromINL(REGISTER_ARGS, value ? 1 : 0, 6);
		return rc;
	}

	/* java.lang.J9VMInternals: private native static void prepareClassImpl(Class clazz); */
	VMINLINE VM_BytecodeAction
	inlInternalsPrepareClassImpl(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t clazz = *(j9object_t*)_sp;
		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		prepareClass(_currentThread, J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, clazz));
		VMStructHasBeenUpdated(REGISTER_ARGS);
		if (VM_VMHelpers::exceptionPending(_currentThread) ) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnVoidFromINL(REGISTER_ARGS, 1);
done:
		return rc;
	}

	/* java.lang.J9VMInternals: static native Class[] getInterfaces(Class clazz); */
	VMINLINE VM_BytecodeAction
	inlInternalsGetInterfaces(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t clazz = *(j9object_t*)_sp;
		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		j9object_t array = getInterfacesHelper(_currentThread, clazz);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		if (VM_VMHelpers::exceptionPending(_currentThread)) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnObjectFromINL(REGISTER_ARGS, array, 1);
done:
		return rc;
	}

	/* java.lang.reflect.Array: private static native Object newArrayImpl(Class componentType, int dimension); */
	VMINLINE VM_BytecodeAction
	inlArrayNewArrayImpl(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t array = NULL;
		I_32 dimension = *(I_32*)_sp;
		j9object_t clazz = *(j9object_t*)(_sp + 1);
		J9Class *arrayClass = NULL;

		buildInternalNativeStackFrame(REGISTER_ARGS);

		J9Class *componentType = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, clazz);

		if (J9ROMCLASS_IS_ARRAY(componentType->romClass) && ((((J9ArrayClass *)componentType)->arity + 1) > J9_ARRAY_DIMENSION_LIMIT)) {
			/* The spec says to throw this exception if the number of dimensions is greater than J9_ARRAY_DIMENSION_LIMIT */
			updateVMStruct(REGISTER_ARGS);
			prepareForExceptionThrow(_currentThread);
			setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}

		arrayClass = componentType->arrayClass;
		if (NULL == arrayClass) {
			J9ROMImageHeader *romHeader = _vm->arrayROMClasses;
			Assert_VM_false(J9ROMCLASS_IS_PRIMITIVE_TYPE(componentType->romClass));
			updateVMStruct(REGISTER_ARGS);
			arrayClass = internalCreateArrayClass(_currentThread, (J9ROMArrayClass*)J9ROMIMAGEHEADER_FIRSTCLASS(romHeader), componentType);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
		}

		array = allocateIndexableObject(REGISTER_ARGS, arrayClass, dimension, true, false, false);
		if (J9_UNEXPECTED(NULL == array)) {
			rc = THROW_HEAP_OOM;
			goto done;
		}

		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnObjectFromINL(REGISTER_ARGS, array, 2);
done:
		return rc;
	}

	/* java.lang.ClassLoader: private native Class findLoadedClassImpl (String className); */
	VMINLINE VM_BytecodeAction
	inlClassLoaderFindLoadedClassImpl(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t className = *(j9object_t*)_sp;
		j9object_t classloaderObject = *(j9object_t*)(_sp + 1);
		J9Class *j9Class = NULL;

		if (NULL != className) {
			J9ClassLoader *loader = J9VMJAVALANGCLASSLOADER_VMREF(_currentThread, classloaderObject);
			if (NULL != loader) {
				if (CLASSNAME_INVALID != verifyQualifiedName(_currentThread, className)) {
					buildInternalNativeStackFrame(REGISTER_ARGS);
					updateVMStruct(REGISTER_ARGS);
					j9Class = internalFindClassString(_currentThread, NULL, className, loader, J9_FINDCLASS_FLAG_EXISTING_ONLY);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					if (VM_VMHelpers::exceptionPending(_currentThread)) {
						rc = GOTO_THROW_CURRENT_EXCEPTION;
						goto done;
					}
					restoreInternalNativeStackFrame(REGISTER_ARGS);
				}
			}
		}

		returnObjectFromINL(REGISTER_ARGS, J9VM_J9CLASS_TO_HEAPCLASS(j9Class), 2);
done:
		return rc;
	}

	/* com.ibm.oti.vm.VM: static final native ClassLoader getStackClassLoader(int depth); */
	VMINLINE VM_BytecodeAction
	inlVMGetStackClassLoader(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 depth = *(I_32*)_sp;
		J9ClassLoader *loader = NULL;

		buildInternalNativeStackFrame(REGISTER_ARGS);

		J9StackWalkState *walkState = _currentThread->stackWalkState;
		walkState->userData1 = (void*)(UDATA)(depth + 1); /* Skip the local INL frame */
		walkState->userData2 = NULL;
		walkState->userData3 = (void *) FALSE;
		walkState->frameWalkFunction = cInterpGetStackClassJEP176Iterator;

		walkState->skipCount = 0;
		walkState->walkThread = _currentThread;
		walkState->flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_ITERATE_FRAMES;
		updateVMStruct(REGISTER_ARGS);
		_vm->walkStackFrames(_currentThread, walkState);

		/* throw out pending InternalError exception first if found */
		if (TRUE == (UDATA)walkState->userData3) {
			setCurrentExceptionNLS(_currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, J9NLS_VM_CALLER_NOT_ANNOTATED_AS_CALLERSENSITIVE);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		} else {
			if (NULL == walkState->userData2) {
				/* Use the bootstrap class loader if we walked off the end of the stack */
				loader = _vm->systemClassLoader;
			} else {
				loader = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, walkState->userData2)->classLoader;
			}

			j9object_t loaderObject = J9CLASSLOADER_CLASSLOADEROBJECT(_currentThread, loader);
			restoreInternalNativeStackFrame(REGISTER_ARGS);
			returnObjectFromINL(REGISTER_ARGS, loaderObject, 1);
		}

		return rc;
	}

	/* java.lang.VMAccess: protected static native Class findClassOrNull(String className, ClassLoader classLoader); */
	VMINLINE VM_BytecodeAction
	inlVMFindClassOrNull(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t classloaderObject = *(j9object_t*)_sp;
		j9object_t className = *(j9object_t*)(_sp + 1);
		J9Class *j9Class = NULL;
		J9ClassLoader *loader = NULL;

		buildInternalNativeStackFrame(REGISTER_ARGS);

		if (NULL == className) {
			rc = THROW_NPE;
			goto done;
		}

		/* This native is only called for the boot loader, so vmRef is guaranteed to be initialized. */
		if (NULL != classloaderObject) {
			loader = J9VMJAVALANGCLASSLOADER_VMREF(_currentThread, classloaderObject);
		} else {
			loader = _vm->systemClassLoader;
		}
		if (CLASSNAME_VALID_NON_ARRARY == verifyQualifiedName(_currentThread, className)) {
			updateVMStruct(REGISTER_ARGS);
			j9Class = internalFindClassString(_currentThread, NULL, className, loader, J9_FINDCLASS_FLAG_USE_LOADER_CP_ENTRIES);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			if (VM_VMHelpers::exceptionPending(_currentThread)) {
				/* The VMStruct is already updated */
				J9Class *exceptionClass = J9VMJAVALANGCLASSNOTFOUNDEXCEPTION(_vm);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				/* If the current exception is ClassNotFoundException, discard it. */
				if (exceptionClass == J9OBJECT_CLAZZ(_currentThread, _currentThread->currentException)) {
					VM_VMHelpers::clearException(_currentThread);
				} else {
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
			}

		}

		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnObjectFromINL(REGISTER_ARGS, J9VM_J9CLASS_TO_HEAPCLASS(j9Class), 2);
done:
		return rc;
	}

	/* java.lang.ClassLoader: static final native void initAnonClassLoader(InternalAnonymousClassLoader anonClassLoader); */
	VMINLINE VM_BytecodeAction
	inlInitializeAnonClassLoader(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t anonClassLoaderObject = *(j9object_t*)(_sp);
		buildInternalNativeStackFrame(REGISTER_ARGS);

		if (NULL != J9VMJAVALANGCLASSLOADER_VMREF(_currentThread, anonClassLoaderObject)) {
			updateVMStruct(REGISTER_ARGS);
			setCurrentException(_currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		_vm->anonClassLoader = allocateClassLoader(_vm);
		if (NULL == _vm->anonClassLoader) {
			updateVMStruct(REGISTER_ARGS);
			setNativeOutOfMemoryError(_currentThread, J9NLS_VM_NATIVE_OOM);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		_vm->anonClassLoader->flags |= J9CLASSLOADER_ANON_CLASS_LOADER;
		J9CLASSLOADER_SET_CLASSLOADEROBJECT(_currentThread, _vm->anonClassLoader, anonClassLoaderObject);
		VM_AtomicSupport::writeBarrier();
		J9VMJAVALANGCLASSLOADER_SET_VMREF(_currentThread, anonClassLoaderObject, _vm->anonClassLoader);

		/* add the ClassLoader in classloaderList, in order to iterate the classLoader later in Metronome GC */
		TRIGGER_J9HOOK_VM_CLASS_LOADER_INITIALIZED(_vm->hookInterface, _currentThread, _vm->anonClassLoader);

		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnVoidFromINL(REGISTER_ARGS, 1);
done:
		return rc;
	}

	/* com.ibm.oti.vm.VM: public static final native void initializeClassLoader(ClassLoader classLoader, int loaderType, boolean parallelCapable); */
	VMINLINE VM_BytecodeAction
	inlVMInitializeClassLoader(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 parallelCapable = *(I_32*)_sp;
		I_32 loaderType = *(I_32*)(_sp + 1);
		j9object_t classLoaderObject = *(j9object_t*)(_sp + 2);
		buildInternalNativeStackFrame(REGISTER_ARGS);
		if (NULL != J9VMJAVALANGCLASSLOADER_VMREF(_currentThread, classLoaderObject)) {
internalError:
			updateVMStruct(REGISTER_ARGS);
			setCurrentException(_currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		if (J9_CLASSLOADER_TYPE_BOOT == loaderType) {
			/* if called with bootLoader, assign the system one to this instance */
			J9ClassLoader *classLoaderStruct = _vm->systemClassLoader;
			j9object_t loaderObject = J9CLASSLOADER_CLASSLOADEROBJECT(_currentThread, classLoaderStruct);
			if (NULL != loaderObject) {
				goto internalError;
			}
			J9CLASSLOADER_SET_CLASSLOADEROBJECT(_currentThread, classLoaderStruct, classLoaderObject);
			if (parallelCapable) {
				classLoaderStruct->flags |= J9CLASSLOADER_PARALLEL_CAPABLE;
			}
			VM_AtomicSupport::writeBarrier();
			J9VMJAVALANGCLASSLOADER_SET_VMREF(_currentThread, classLoaderObject, classLoaderStruct);
			TRIGGER_J9HOOK_VM_CLASS_LOADER_INITIALIZED(_vm->hookInterface, _currentThread, classLoaderStruct);

			J9ClassWalkState classWalkState;
			J9Class* clazz = allClassesStartDo(&classWalkState, _vm, classLoaderStruct);
			while (NULL != clazz) {
				J9VMJAVALANGCLASS_SET_CLASSLOADER(_currentThread, clazz->classObject, classLoaderObject);
				clazz = allClassesNextDo(&classWalkState);
			}
			allClassesEndDo(&classWalkState);
		} else {
			updateVMStruct(REGISTER_ARGS);
			J9ClassLoader* result = internalAllocateClassLoader(_vm, classLoaderObject);
			VMStructHasBeenUpdated(REGISTER_ARGS); // likely unncessary - no code runs in internalAllocateClassLoader
			if (NULL == result) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			if (J9_CLASSLOADER_TYPE_PLATFORM == loaderType) {
				_vm->platformClassLoader = result;
			}
		}
		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnVoidFromINL(REGISTER_ARGS, 3);
done:
		return rc;
	}

	/* com.ibm.oti.vm.VM: public static final native int getClassPathEntryType(java.lang.ClassLoader classLoader, int cpIndex); */
	VMINLINE VM_BytecodeAction
	inlVMGetClassPathEntryType(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 cpIndex = *(I_32*)_sp;
		j9object_t classLoaderObject = *(j9object_t*)(_sp + 1);
		I_32 type = CPE_TYPE_UNUSABLE;
		/* This native is only called for the boot loader, so vmRef is guaranteed to be initialized */
		J9ClassLoader *classLoader = J9VMJAVALANGCLASSLOADER_VMREF(_currentThread, classLoaderObject);
		/* Bounds check the parameter */
		if ((cpIndex >= 0) && (cpIndex < (I_32)classLoader->classPathEntryCount)) {
			/* Check the flags of the translation data struct */
			J9TranslationBufferSet *translationData = _vm->dynamicLoadBuffers;
			if (NULL != translationData) {
				/* Initialize the class path entry */
				J9ClassPathEntry *cpEntry = classLoader->classPathEntries + cpIndex;
				type = (I_32)initializeClassPathEntry(_vm, cpEntry);
			}
		}
		returnSingleFromINL(REGISTER_ARGS, type, 2);
		return rc;
	}

	/* com.ibm.oti.vm.VM: static private final native boolean isBootstrapClassLoader(ClassLoader loader); */
	VMINLINE VM_BytecodeAction
	inlVMIsBootstrapClassLoader(REGISTER_ARGS_LIST)
	{
		I_32 isBoot = 0;
		j9object_t classLoaderObject = *(j9object_t*)_sp;
		if (NULL == classLoaderObject) {
			isBoot = 1;
		} else {
			J9ClassLoader *loader = J9VMJAVALANGCLASSLOADER_VMREF(_currentThread, classLoaderObject);
			if (loader == _vm->systemClassLoader) {
				isBoot = 1;
			}
		}
		returnSingleFromINL(REGISTER_ARGS, isBoot, 1);
		return EXECUTE_BYTECODE;
	}

	/* java.lang.Class: private static native Class forNameImpl(String className, boolean initializeBoolean, ClassLoader classLoader) throws ClassNotFoundException; */
	VMINLINE VM_BytecodeAction
	inlClassForNameImpl(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t classLoaderObject = *(j9object_t*)_sp;
		I_32 initializeBoolean = *(I_32*)(_sp + 1);
		j9object_t classNameObject = *(j9object_t*)(_sp + 2);
		J9ClassLoader *classLoader = NULL;
		J9Class *foundClass = NULL;

		buildInternalNativeStackFrame(REGISTER_ARGS);

		if (NULL == classNameObject) {
			rc = THROW_NPE;
			goto done;
		}

		/* Fetch the J9ClasLoader, creating it if need be */
		if (NULL == classLoaderObject) {
			classLoader = _vm->systemClassLoader;
		} else {
			classLoader = J9VMJAVALANGCLASSLOADER_VMREF(_currentThread, classLoaderObject);
			if (NULL == classLoader) {
				pushObjectInSpecialFrame(REGISTER_ARGS, classNameObject);
				updateVMStruct(REGISTER_ARGS);
				classLoader = internalAllocateClassLoader(_vm, classLoaderObject);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				classNameObject = popObjectInSpecialFrame(REGISTER_ARGS);
				if (NULL == classLoader) {
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
			}
		}

		/* Make sure the name is legal */
		if (CLASSNAME_INVALID == verifyQualifiedName(_currentThread, classNameObject)) {
			goto throwCNFE;
		}

		/* Find the class */
		pushObjectInSpecialFrame(REGISTER_ARGS, classNameObject);
		updateVMStruct(REGISTER_ARGS);
		foundClass = internalFindClassString(_currentThread, NULL, classNameObject, classLoader, 0);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		classNameObject = popObjectInSpecialFrame(REGISTER_ARGS);
		if (NULL == foundClass) {
			/* Not found - if no exception is pending, throw ClassNotFoundException */
			if (NULL == _currentThread->currentException) {
throwCNFE:
				updateVMStruct(REGISTER_ARGS);
				setCurrentException(_currentThread, J9VMCONSTANTPOOL_JAVALANGCLASSNOTFOUNDEXCEPTION, (UDATA*)classNameObject);
				VMStructHasBeenUpdated(REGISTER_ARGS);
			}
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}


		/* Initialize the class if requested */
		if (initializeBoolean) {
			rc = initializeClassIfNeeded(REGISTER_ARGS, foundClass);
			if (J9_UNEXPECTED(EXECUTE_BYTECODE != rc)) {
				goto done;
			}
		}

		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnObjectFromINL(REGISTER_ARGS, (j9object_t)foundClass->classObject, 3);
done:
		return rc;
	}

	/* com.ibm.oti.vm.VM: protected static native int getCPIndexImpl(Class targetClass); */
	VMINLINE VM_BytecodeAction
	inlVMGetCPIndexImpl(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t targetClass = *(j9object_t*)_sp;
		J9Class *j9clazz = J9VM_J9CLASS_FROM_HEAPCLASS(_currentThread, targetClass);

		omrthread_monitor_enter(_vm->classLoaderModuleAndLocationMutex);
		J9ClassLocation *classLocation = findClassLocationForClass(_currentThread, j9clazz);
		omrthread_monitor_exit(_vm->classLoaderModuleAndLocationMutex);

		/* (classLocation->locationType < 0) points to generated classes which don't have location.
		 * But, their class location has been hacked to properly retrieve package information.
		 * Such location types must return J9_CP_INDEX_NONE for classpath index.
		 */
		if ((NULL == classLocation) || (classLocation->locationType < 0)) {
			returnSingleFromINL(REGISTER_ARGS, (I_32)J9_CP_INDEX_NONE, 1);
		} else {
			returnSingleFromINL(REGISTER_ARGS, (I_32)classLocation->entryIndex, 1);
		}
		return rc;
	}

	/* com.ibm.tools.attach.target.Attachment: private native int loadAgentLibraryImpl(ClassLoader loader, String agentLibrary, String options, boolean decorate); */
	VMINLINE VM_BytecodeAction
	inlAttachmentLoadAgentLibraryImpl(REGISTER_ARGS_LIST)
	{
		UDATA decorate = *(I_32*)_sp;
		jstring agentOptions = (jstring)(_sp + 1);
		jstring agentLibrary = (jstring)(_sp + 2);
		UDATA *bp = buildSpecialStackFrame(REGISTER_ARGS, J9SF_FRAME_TYPE_JNI_NATIVE_METHOD, jitStackFrameFlags(REGISTER_ARGS, J9_STACK_FLAGS_USE_SPECIFIED_CLASS_LOADER), true);
		*--_sp = (UDATA)_sendMethod;
		_arg0EA = bp + 5;
		updateVMStruct(REGISTER_ARGS);
		exitVMToJNI(_currentThread);
		I_32 status = JNI_ERR;
		JNIEnv *env = (JNIEnv*)_currentThread;
		const char *agentLibraryUTF = env->GetStringUTFChars(agentLibrary, NULL);
		if (NULL != agentLibraryUTF) {
//			Trc_JCL_attach_loadAgentLibrary(env, agentLibraryUTF, agentOptions, decorate);
			const char *agentOptionsUTF = env->GetStringUTFChars(agentOptions, NULL);
			if (NULL != agentOptionsUTF) {
					status = (_vm->loadAgentLibraryOnAttach)(_vm, agentLibraryUTF, agentOptionsUTF, decorate);
					env->ReleaseStringUTFChars(agentOptions, agentOptionsUTF);
			}
			env->ReleaseStringUTFChars(agentLibrary, agentLibraryUTF);
		}
//		Trc_JCL_attach_loadAgentLibraryStatus(env, status);
		env->ExceptionClear();
		enterVMFromJNI(_currentThread);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		bp = _arg0EA - 5;
		J9SFJNINativeMethodFrame *nativeMethodFrame = recordJNIReturn(REGISTER_ARGS, bp);
		_currentThread->jitStackFrameFlags = nativeMethodFrame->specialFrameFlags & J9_SSF_JIT_NATIVE_TRANSITION_FRAME;
		restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, bp);
		returnSingleFromINL(REGISTER_ARGS, status, 5);
		return EXECUTE_BYTECODE;
	}

	/* com.ibm.oti.vm.VM: static final native Class getStackClass(int depth); */
	VMINLINE VM_BytecodeAction
	inlVMGetStackClass(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 depth = *(I_32*)_sp;

		buildInternalNativeStackFrame(REGISTER_ARGS);

		J9StackWalkState *walkState = _currentThread->stackWalkState;
		walkState->userData1 = (void*)(UDATA)(depth + 1); /* Skip the local INL frame */
		walkState->userData2 = NULL;
		walkState->userData3 = (void *) FALSE;
		walkState->frameWalkFunction = cInterpGetStackClassJEP176Iterator;

		walkState->skipCount = 0;
		walkState->walkThread = _currentThread;
		walkState->flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_ITERATE_FRAMES;
		updateVMStruct(REGISTER_ARGS);
		_vm->walkStackFrames(_currentThread, walkState);

		/* throw out pending InternalError exception first if found */
		if (TRUE == (UDATA)walkState->userData3) {
			setCurrentExceptionNLS(_currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, J9NLS_VM_CALLER_NOT_ANNOTATED_AS_CALLERSENSITIVE);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}

		/* PR 85136: walkState->userData2 will be NULL if we walked off the end of the stack.
		 * Return null to java in this case.
		 */

		restoreInternalNativeStackFrame(REGISTER_ARGS);
		returnObjectFromINL(REGISTER_ARGS, (j9object_t)walkState->userData2, 1);
done:
		return rc;
	}

	/* java.lang.Thread: public static native void sleep(long millis, int nanos); */
	VMINLINE VM_BytecodeAction
	inlThreadSleep(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 nanos = *(I_32*)_sp;
		I_64 millis = *(I_64*)(_sp + 1);
		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		IDATA sleepResult = threadSleepImpl(_currentThread, millis, nanos);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		if (0 == sleepResult) {
			if (VM_VMHelpers::asyncMessagePending(_currentThread)) {
				rc = GOTO_ASYNC_CHECK;
			} else {
				restoreInternalNativeStackFrame(REGISTER_ARGS);
				returnVoidFromINL(REGISTER_ARGS, 3);
			}
		} else {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		}
		return rc;
	}

	/* java.lang.Object: public final native void wait(long millis, int nanos); */
	VMINLINE VM_BytecodeAction
	inlObjectWait(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 nanos = *(I_32*)_sp;
		I_64 millis = *(I_64*)(_sp + 1);
		j9object_t object = *(j9object_t*)(_sp + 3);
		buildInternalNativeStackFrame(REGISTER_ARGS);
		updateVMStruct(REGISTER_ARGS);
		IDATA waitResult = monitorWaitImpl(_currentThread, object, millis, nanos, TRUE);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		if (0 == waitResult) {
			if (VM_VMHelpers::asyncMessagePending(_currentThread)) {
				rc = GOTO_ASYNC_CHECK;
			} else {
				restoreInternalNativeStackFrame(REGISTER_ARGS);
				returnVoidFromINL(REGISTER_ARGS, 4);
			}
		} else {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
		}
		return rc;
	}

	/* java.lang.ClassLoader: static private native byte[] loadLibraryWithPath(byte[] libName, ClassLoader loader, byte[] libraryPath); */
	VMINLINE VM_BytecodeAction
	inlClassLoaderLoadLibraryWithPath(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t libraryPath = *(j9object_t*)_sp;
		j9object_t loader = *(j9object_t*)(_sp + 1);
		j9object_t *libNamePtr = (j9object_t*)(_sp + 2);
		j9object_t libName = *libNamePtr;
		char *cLibName = NULL;
		char *cLibPath = NULL;
		char *errBuf = NULL;
		UDATA bufLen = 512;
		UDATA registerRC = 0;
		j9object_t errorBytes = NULL;
		J9ClassLoader *classLoader = NULL;
		PORT_ACCESS_FROM_JAVAVM(_vm);
		UDATA *bp = buildSpecialStackFrame(REGISTER_ARGS, J9SF_FRAME_TYPE_JNI_NATIVE_METHOD, jitStackFrameFlags(REGISTER_ARGS, J9_STACK_FLAGS_USE_SPECIFIED_CLASS_LOADER), true);
		*--_sp = (UDATA)_sendMethod;
		_arg0EA = bp + 3;
		updateVMStruct(REGISTER_ARGS);
		if (NULL == libName) {
			rc = THROW_NPE;
			goto done;
		}
		cLibName = convertByteArrayToCString(_currentThread, libName);
		if (NULL == cLibName) {
nativeOOM:
			setNativeOutOfMemoryError(_currentThread, J9NLS_JCL_FAILED_TO_CREATE_STACK_TRACE);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		if (NULL != libraryPath) {
			cLibPath = convertByteArrayToCString(_currentThread, libraryPath);
			if (NULL == cLibPath) {
				goto nativeOOM;
			}
		}
		if (NULL == loader) {
			classLoader = _vm->systemClassLoader;
		} else {
			classLoader = J9VMJAVALANGCLASSLOADER_VMREF(_currentThread, loader);
			if (NULL == classLoader) {
				classLoader = internalAllocateClassLoader(_vm, loader);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				if (NULL == classLoader) {
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
			}
		}
		errBuf = (char*)j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
		if (NULL == errBuf) {
			bufLen = 0;
		}
		/* vmStruct already up-to-date */
		internalReleaseVMAccess(_currentThread);
		registerRC = registerNativeLibrary(_currentThread, classLoader, cLibName, cLibPath, NULL, errBuf, bufLen);
		internalAcquireVMAccess(_currentThread);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		if (VM_VMHelpers::exceptionPending(_currentThread)) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		if (0 != registerRC) {
			if (NULL != errBuf) {
				/* vmStruct already up-to-date */
				errorBytes = convertCStringToByteArray(_currentThread, errBuf);
				VMStructHasBeenUpdated(REGISTER_ARGS);
			}
			if (NULL == errorBytes) {
				errorBytes = *libNamePtr;
			}
		}
		bp = _arg0EA - 3;
		{
			J9SFJNINativeMethodFrame *nativeMethodFrame = recordJNIReturn(REGISTER_ARGS, bp);
			_currentThread->jitStackFrameFlags = nativeMethodFrame->specialFrameFlags & J9_SSF_JIT_NATIVE_TRANSITION_FRAME;
			restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, bp);
			returnObjectFromINL(REGISTER_ARGS, errorBytes, 3);
		}
done:
		j9mem_free_memory(errBuf);
		j9mem_free_memory(cLibPath);
		j9mem_free_memory(cLibName);
		return rc;
	}

	/* java.lang.Thread: public native boolean isInterruptedImpl(); */
	VMINLINE VM_BytecodeAction
	inlThreadIsInterruptedImpl(REGISTER_ARGS_LIST)
	{
		j9object_t receiverObject = *(j9object_t*)_sp;
		I_32 result = VM_VMHelpers::threadIsInterruptedImpl(_currentThread, receiverObject) ? 1 : 0;
		returnSingleFromINL(REGISTER_ARGS, result, 1);
		return EXECUTE_BYTECODE;
	}

	/* Redirect to out of line INL methods */
	VMINLINE VM_BytecodeAction
	outOfLineINL(REGISTER_ARGS_LIST)
	{
		updateVMStruct(REGISTER_ARGS);
		J9OutOfLineINLMethod *target = (J9OutOfLineINLMethod *)(((UDATA)_sendMethod->extra) & ~J9_STARTPC_NOT_TRANSLATED);
		VM_BytecodeAction rc = target(_currentThread, _sendMethod);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		return rc;
	}

	/* Bytecode implementations.
	 *
	 * All of the bytecode stack descriptions are in terms of stack slots.
	 * long/double values require 2 stack slots, even on 64-bit platforms.
	 * On 64-bit, the high-memory half of int-sized stack slots and the high-memory
	 * stack slot of long/double values are undefined.
	 */

	/* ... => ... */
	VMINLINE VM_BytecodeAction
	nop(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value */
	VMINLINE VM_BytecodeAction
	aconst(REGISTER_ARGS_LIST, UDATA value)
	{
		_pc += 1;
		_sp -= 1;
		*_sp = value;
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value */
	VMINLINE VM_BytecodeAction
	iconst(REGISTER_ARGS_LIST, I_32 value)
	{
		_pc += 1;
		_sp -= 1;
		*(I_32*)_sp = value;
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value1, value2 */
	VMINLINE VM_BytecodeAction
	lconst(REGISTER_ARGS_LIST, I_64 value)
	{
		_pc += 1;
		_sp -= 2;
		*(I_64*)_sp = value;
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value */
	VMINLINE VM_BytecodeAction
	bipush(REGISTER_ARGS_LIST)
	{
		I_32 val = *(I_8*)(_pc + 1);
		_pc += 2;
		_sp -= 1;
		*(I_32*)_sp = val;
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value */
	VMINLINE VM_BytecodeAction
	sipush(REGISTER_ARGS_LIST)
	{
		I_32 val = *(I_16*)(_pc + 1);
		_pc += 3;
		_sp -= 1;
		*(I_32*)_sp = val;
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value1, value2 */
	VMINLINE VM_BytecodeAction
	ldc2lw(REGISTER_ARGS_LIST)
	{
		U_16 index = *(U_16*)(_pc + 1);
		J9ROMConstantRef *romConstant = (J9ROMConstantRef*)romConstantPoolItem(_literals, index);
		_pc += 3;
		_sp -= 2;
		((U_32*)_sp)[0] = romConstant->slot1;
		((U_32*)_sp)[1] = romConstant->slot2;
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value */
	VMINLINE VM_BytecodeAction
	aload(REGISTER_ARGS_LIST)
	{
		U_8 index = _pc[1];
		_pc += 2;
		_sp -= 1;
		*_sp = *(_arg0EA - index);
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value */
	VMINLINE VM_BytecodeAction
	aload(REGISTER_ARGS_LIST, UDATA index) {
		_pc += 1;
		_sp -= 1;
		*_sp = *(_arg0EA - index);
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ... */
	VMINLINE VM_BytecodeAction
	astore(REGISTER_ARGS_LIST)
	{
		U_8 index = _pc[1];
		_pc += 2;
		*(_arg0EA - index) = *_sp;
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ... */
	VMINLINE VM_BytecodeAction
	astore(REGISTER_ARGS_LIST, UDATA index)
	{
		_pc += 1;
		*(_arg0EA - index) = *_sp;
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value1, value2 */
	VMINLINE VM_BytecodeAction
	lload(REGISTER_ARGS_LIST) {
		U_8 index = _pc[1];
		_pc += 2;
		_sp -= 2;
		_sp[0] = *(_arg0EA - index - 1);
#if !defined(J9VM_ENV_DATA64)
		_sp[1] = *(_arg0EA - index);
#endif
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value1, value2 */
	VMINLINE VM_BytecodeAction
	lload(REGISTER_ARGS_LIST, UDATA index)
	{
		_pc += 1;
		_sp -= 2;
		_sp[0] = *(_arg0EA - index - 1);
#if !defined(J9VM_ENV_DATA64)
		_sp[1] = *(_arg0EA - index);
#endif
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ... */
	VMINLINE VM_BytecodeAction
	lstore(REGISTER_ARGS_LIST)
	{
		U_8 index = _pc[1];
		_pc += 2;
		*(_arg0EA - index - 1) = _sp[0];
#if !defined(J9VM_ENV_DATA64)
		*(_arg0EA - index) = _sp[1];
#endif
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ... */
	VMINLINE VM_BytecodeAction
	lstore(REGISTER_ARGS_LIST, UDATA index)
	{
		_pc += 1;
		*(_arg0EA - index - 1) = _sp[0];
#if !defined(J9VM_ENV_DATA64)
		*(_arg0EA - index) = _sp[1];
#endif
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1...valueN => ... */
	VMINLINE VM_BytecodeAction
	pop(REGISTER_ARGS_LIST, UDATA slots)
	{
		_pc += 1;
		_sp += slots;
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., value, value */
	VMINLINE VM_BytecodeAction
	dup(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		_sp -= 1;
		_sp[0] = _sp[1];
		return EXECUTE_BYTECODE;
	}

	/* ..., value2, value1 => ..., value1, value2, value1 */
	VMINLINE VM_BytecodeAction
	dupx1(REGISTER_ARGS_LIST)
	{
		UDATA value1 = _sp[0];
		UDATA value2 = _sp[1];
		_pc += 1;
		_sp -= 1;
		_sp[0] = value1;
		_sp[1] = value2;
		_sp[2] = value1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value3, value2, value1 => ..., value1, value3, value2, value1 */
	VMINLINE VM_BytecodeAction
	dupx2(REGISTER_ARGS_LIST)
	{
		UDATA value1 = _sp[0];
		UDATA value2 = _sp[1];
		UDATA value3 = _sp[2];
		_pc += 1;
		_sp -= 1;
		_sp[0] = value1;
		_sp[1] = value2;
		_sp[2] = value3;
		_sp[3] = value1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., value1, value2, value1, value2 */
	VMINLINE VM_BytecodeAction
	dup2(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		_sp -= 2;
		_sp[0] = _sp[2];
		_sp[1] = _sp[3];
		return EXECUTE_BYTECODE;
	}

	/* ..., value3, value2, value1 => ..., value2, value1, value3, value2, value1 */
	VMINLINE VM_BytecodeAction
	dup2x1(REGISTER_ARGS_LIST)
	{
		UDATA value1 = _sp[0];
		UDATA value2 = _sp[1];
		UDATA value3 = _sp[2];
		_pc += 1;
		_sp -= 2;
		_sp[0] = value1;
		_sp[1] = value2;
		_sp[2] = value3;
		_sp[3] = value1;
		_sp[4] = value2;
		return EXECUTE_BYTECODE;
	}

	/* ..., value4, value3, value2, value1 => ..., value2, value1, value4, value3, value2, value1 */
	VMINLINE VM_BytecodeAction
	dup2x2(REGISTER_ARGS_LIST)
	{
		UDATA value1 = _sp[0];
		UDATA value2 = _sp[1];
		UDATA value3 = _sp[2];
		UDATA value4 = _sp[3];
		_pc += 1;
		_sp -= 2;
		_sp[0] = value1;
		_sp[1] = value2;
		_sp[2] = value3;
		_sp[3] = value4;
		_sp[4] = value1;
		_sp[5] = value2;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., value1, value2 */
	VMINLINE VM_BytecodeAction
	swap(REGISTER_ARGS_LIST) {
		_pc += 1;
		UDATA temp = _sp[1];
		_sp[1] = _sp[0];
		_sp[0] = temp;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs, rhs => ..., result */
	VMINLINE VM_BytecodeAction
	iadd(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)(_sp + 1) += *(I_32*)_sp;
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs1, lhs2, rhs1, rhs2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	ladd(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*((I_64*)(_sp + 2)) += *(I_64*)_sp;
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs, rhs => ..., result */
	VMINLINE VM_BytecodeAction
	fadd(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperFloatPlusFloat((jfloat*)(_sp + 1), (jfloat*)_sp, (jfloat*)(_sp + 1));
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs1, lhs2, rhs1, rhs2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	dadd(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperDoublePlusDouble((jdouble*)(_sp + 2), (jdouble*)_sp, (jdouble*)(_sp + 2));
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs, rhs => ..., result */
	VMINLINE VM_BytecodeAction
	isub(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)(_sp + 1) -= *(I_32*)_sp;
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs1, lhs2, rhs1, rhs2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	lsub(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_64*)(_sp + 2) -= *(I_64*)_sp;
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs, rhs => ..., result */
	VMINLINE VM_BytecodeAction
	fsub(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperFloatMinusFloat((jfloat*)(_sp + 1), (jfloat*)_sp, (jfloat*)(_sp + 1));
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs1, lhs2, rhs1, rhs2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	dsub(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperDoubleMinusDouble((jdouble*)(_sp + 2), (jdouble*)_sp, (jdouble*)(_sp + 2));
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result */
	VMINLINE VM_BytecodeAction
	imul(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)(_sp + 1) *= *(I_32*)_sp;
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs1, lhs2, rhs1, rhs2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	lmul(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_64*)(_sp + 2) *= *(I_64*)_sp;
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs, rhs => ..., result */
	VMINLINE VM_BytecodeAction
	fmul(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperFloatMultiplyFloat((jfloat*)(_sp + 1), (jfloat*)_sp, (jfloat*)(_sp + 1));
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs1, lhs2, rhs1, rhs2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	dmul(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperDoubleMultiplyDouble((jdouble*)(_sp + 2), (jdouble*)_sp, (jdouble*)(_sp + 2));
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., dividend, divisor => ..., result */
	VMINLINE VM_BytecodeAction
	fdiv(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperFloatDivideFloat((jfloat*)(_sp + 1), (jfloat*)_sp, (jfloat*)(_sp + 1));
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., dividend1, dividend2, divisor1, divisor2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	ddiv(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperDoubleDivideDouble((jdouble*)(_sp + 2), (jdouble*)_sp, (jdouble*)(_sp + 2));
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., dividend, divisor => ..., result */
	VMINLINE VM_BytecodeAction
	frem(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperFloatRemainderFloat((jfloat*)(_sp + 1), (jfloat*)_sp, (jfloat*)(_sp + 1));
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., dividend1, dividend2, divisor1, divisor2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	drem(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperDoubleRemainderDouble((jdouble*)(_sp + 2), (jdouble*)_sp, (jdouble*)(_sp + 2));
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result */
	VMINLINE VM_BytecodeAction
	ineg(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)_sp = -*(I_32*)_sp;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	lneg(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_64*)_sp = -*(I_64*)_sp;
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result */
	VMINLINE VM_BytecodeAction
	fneg(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperNegateFloat((jfloat*)_sp, (jfloat*)_sp);
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	dneg(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperNegateDouble((jdouble*)_sp, (jdouble*)_sp);
		return EXECUTE_BYTECODE;
	}

	/* ..., value, shift => ..., result */
	VMINLINE VM_BytecodeAction
	ishl(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)(_sp + 1) <<= (*(U_32*)_sp & 31);
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2, shift => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	lshl(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_64*)(_sp + 1) <<= (*(U_32*)_sp & 63);
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value, shift => ..., result */
	VMINLINE VM_BytecodeAction
	ishr(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)(_sp + 1) >>= (*(U_32*)_sp & 31);
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2, shift => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	lshr(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_64*)(_sp + 1) >>= (*(U_32*)_sp & 63);
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value, shift => ..., result */
	VMINLINE VM_BytecodeAction
	iushr(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(U_32*)(_sp + 1) >>= (*(U_32*)_sp & 31);
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2, shift => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	lushr(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(U_64*)(_sp + 1) >>= (*(U_32*)_sp & 63);
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result */
	VMINLINE VM_BytecodeAction
	iand(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)(_sp + 1) &= *(I_32*)_sp;
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2, value3, value4 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	land(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_64*)(_sp + 2) &= *(I_64*)_sp;
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result */
	VMINLINE VM_BytecodeAction
	ior(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)(_sp + 1) |= *(I_32*)_sp;
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2, value3, value4 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	lor(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_64*)(_sp + 2) |= *(I_64*)_sp;
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result */
	VMINLINE VM_BytecodeAction
	ixor(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)(_sp + 1) ^= *(I_32*)_sp;
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2, value3, value4 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	lxor(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_64*)(_sp + 2) ^= *(I_64*)_sp;
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ... => ... */
	VMINLINE VM_BytecodeAction
	iinc(REGISTER_ARGS_LIST, UDATA parmSize)
	{
		U_16 index = 0;
		I_16 val = 0;
		if (parmSize == 1) {
			index = _pc[1];
			val = *(I_8*)(_pc + 2);
		} else if (parmSize == 2) {
			index = *(U_16*)(_pc + 1);
			val = *(I_16*)(_pc + 3);
		} else {
			Assert_VM_unreachable();
		}
		_pc += (1 + parmSize + parmSize);
		*(I_32*)(_arg0EA - index) += (I_32)val;
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	i2l(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		_sp -= 1;
		*(I_64*)_sp = *(I_32*)(_sp + 1);
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result */
	VMINLINE VM_BytecodeAction
	i2f(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperConvertIntegerToFloat((I_32*)_sp, (jfloat*)_sp);
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	i2d(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		_sp -= 1;
		helperConvertIntegerToDouble((I_32*)(_sp + 1), (jdouble*)_sp);
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result */
	VMINLINE VM_BytecodeAction
	l2i(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)(_sp + 1) = (I_32)(*(I_64*)_sp);
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result */
	VMINLINE VM_BytecodeAction
	l2f(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperConvertLongToFloat((I_64*)_sp, (jfloat*)(_sp + 1));
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	l2d(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperConvertLongToDouble((I_64*)_sp, (jdouble*)_sp);
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result */
	VMINLINE VM_BytecodeAction
	f2i(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperConvertFloatToInteger((jfloat*)_sp, (I_32*)_sp);
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	f2l(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		_sp -= 1;
		helperConvertFloatToLong((jfloat*)(_sp + 1), (I_64*)_sp);
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	f2d(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		_sp -= 1;
		helperConvertFloatToDouble((jfloat*)(_sp + 1), (jdouble*)_sp);
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result */
	VMINLINE VM_BytecodeAction
	d2i(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperConvertDoubleToInteger((jdouble*)_sp, (I_32*)(_sp + 1));
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	d2l(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperConvertDoubleToLong((jdouble*)_sp, (I_64*)_sp);
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ..., result */
	VMINLINE VM_BytecodeAction
	d2f(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		helperConvertDoubleToFloat((jdouble*)_sp, (jfloat*)(_sp + 1));
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result */
	VMINLINE VM_BytecodeAction
	i2b(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)_sp = (I_8)*(I_32*)_sp;
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result */
	VMINLINE VM_BytecodeAction
	i2c(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)_sp = (U_16)*(I_32*)_sp;
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ..., result */
	VMINLINE VM_BytecodeAction
	i2s(REGISTER_ARGS_LIST)
	{
		_pc += 1;
		*(I_32*)_sp = (I_16)*(I_32*)_sp;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs1, lhs2, rhs1, rhs2 => ..., result */
	VMINLINE VM_BytecodeAction
	lcmp(REGISTER_ARGS_LIST)
	{
		I_64 rhs = *(I_64*)_sp;
		I_64 lhs = *(I_64*)(_sp + 2);
		_pc += 1;
		_sp += 3;
		if (lhs == rhs) {
			*(I_32*)_sp = 0;
		} else if (lhs > rhs) {
			*(I_32*)_sp = 1;
		} else {
			*(I_32*)_sp = -1;
		}
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs, rhs => ..., result */
	VMINLINE VM_BytecodeAction
	fcmp(REGISTER_ARGS_LIST, I_32 nanValue)
	{
		I_32 result = helperFloatCompareFloat((jfloat*)(_sp + 1), (jfloat*)_sp);
		_pc += 1;
		_sp += 1;
		if (-2 == result) {
			result = nanValue;
		}
		*(I_32*)_sp = result;
		return EXECUTE_BYTECODE;
	}

	/* ..., lhs, lhs2, rhs1, rhs2 => ..., result */
	VMINLINE VM_BytecodeAction
	dcmp(REGISTER_ARGS_LIST, I_32 nanValue)
	{
		I_32 result = helperDoubleCompareDouble((jdouble*)(_sp + 2), (jdouble*)_sp);
		_pc += 1;
		_sp += 3;
		if (-2 == result) {
			result = nanValue;
		}
		*(I_32*)_sp = result;
		return EXECUTE_BYTECODE;
	}

	/* ..., arrayref, index => ..., value */
	VMINLINE VM_BytecodeAction
	iaload(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 1);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)_sp;
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				_pc += 1;
				_sp += 1;
				*(I_32*)_sp = _objectAccessBarrier.inlineIndexableObjectReadI32(_currentThread, arrayref, index);
			}
		}
		return rc;
	}

	/* ..., arrayref, index, value => ... */
	VMINLINE VM_BytecodeAction
	iastore(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 2);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)(_sp + 1);
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				I_32 value = *(I_32*)_sp;
				_pc += 1;
				_sp += 3;
				_objectAccessBarrier.inlineIndexableObjectStoreI32(_currentThread, arrayref, index, value);
			}
		}
		return rc;
	}

	/* ..., arrayref, index => ..., value */
	VMINLINE VM_BytecodeAction
	faload(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 1);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)_sp;
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				_pc += 1;
				_sp += 1;
				*(U_32*)_sp = _objectAccessBarrier.inlineIndexableObjectReadU32(_currentThread, arrayref, index);
			}
		}
		return rc;
	}

	/* ..., arrayref, index, value => ... */
	VMINLINE VM_BytecodeAction
	fastore(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 2);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)(_sp + 1);
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				U_32 value = *(U_32*)_sp;
				_pc += 1;
				_sp += 3;
				_objectAccessBarrier.inlineIndexableObjectStoreU32(_currentThread, arrayref, index, value);
			}
		}
		return rc;
	}

	/* ..., arrayref, index => ..., value */
	VMINLINE VM_BytecodeAction
	laload(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 1);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)_sp;
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				_pc += 1;
				*(I_64*)_sp = _objectAccessBarrier.inlineIndexableObjectReadI64(_currentThread, arrayref, index);
			}
		}
		return rc;
	}

	/* ..., arrayref, index, value1, value2 => ... */
	VMINLINE VM_BytecodeAction
	lastore(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 3);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)(_sp + 2);
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				I_64 value = *(I_64*)_sp;
				_pc += 1;
				_sp += 4;
				_objectAccessBarrier.inlineIndexableObjectStoreI64(_currentThread, arrayref, index, value);
			}
		}
		return rc;
	}

	/* ..., arrayref, index => ..., value */
	VMINLINE VM_BytecodeAction
	daload(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 1);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)_sp;
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				_pc += 1;
				*(U_64*)_sp = _objectAccessBarrier.inlineIndexableObjectReadU64(_currentThread, arrayref, index);
			}
		}
		return rc;
	}

	/* ..., arrayref, index, value1, value2 => ... */
	VMINLINE VM_BytecodeAction
	dastore(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 3);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)(_sp + 2);
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				U_64 value = *(U_64*)_sp;
				_pc += 1;
				_sp += 4;
				_objectAccessBarrier.inlineIndexableObjectStoreU64(_currentThread, arrayref, index, value);
			}
		}
		return rc;
	}

	/* ..., arrayref, index => ..., value */
	VMINLINE VM_BytecodeAction
	baload(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 1);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)_sp;
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				_pc += 1;
				_sp += 1;
				*(I_32*)_sp = (I_32)_objectAccessBarrier.inlineIndexableObjectReadI8(_currentThread, arrayref, index);
			}
		}
		return rc;
	}

	/* ..., arrayref, index, value => ... */
	VMINLINE VM_BytecodeAction
	bastore(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 2);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)(_sp + 1);
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				I_8 value = (I_8)*(I_32*)_sp;
				_pc += 1;
				_sp += 3;
				_objectAccessBarrier.inlineIndexableObjectStoreI8(_currentThread, arrayref, index, value);
			}
		}
		return rc;
	}

	/* ..., arrayref, index => ..., value */
	VMINLINE VM_BytecodeAction
	caload(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 1);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)_sp;
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				_pc += 1;
				_sp += 1;
				*(I_32*)_sp = (I_32)_objectAccessBarrier.inlineIndexableObjectReadU16(_currentThread, arrayref, index);
			}
		}
		return rc;
	}

	/* ..., arrayref, index, value => ... */
	VMINLINE VM_BytecodeAction
	castore(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 2);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)(_sp + 1);
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				U_16 value = (U_16)*(I_32*)_sp;
				_pc += 1;
				_sp += 3;
				_objectAccessBarrier.inlineIndexableObjectStoreU16(_currentThread, arrayref, index, value);
			}
		}
		return rc;
	}

	/* ..., arrayref, index => ..., value */
	VMINLINE VM_BytecodeAction
	saload(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 1);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)_sp;
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				_pc += 1;
				_sp += 1;
				*(I_32*)_sp = (I_32)_objectAccessBarrier.inlineIndexableObjectReadI16(_currentThread, arrayref, index);
			}
		}
		return rc;
	}

	/* ..., arrayref, index, value => ... */
	VMINLINE VM_BytecodeAction
	sastore(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 2);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)(_sp + 1);
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				I_16 value = (I_16)*(I_32*)_sp;
				_pc += 1;
				_sp += 3;
				_objectAccessBarrier.inlineIndexableObjectStoreI16(_currentThread, arrayref, index, value);
			}
		}
		return rc;
	}

	/* ..., arrayref, index => ..., value */
	VMINLINE VM_BytecodeAction
	aaload(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 1);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)_sp;
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				j9object_t value = _objectAccessBarrier.inlineIndexableObjectReadObject(_currentThread, arrayref, index);
				_pc += 1;
				_sp += 1;
				*(j9object_t*)_sp = value;
			}
		}
		return rc;
	}

	/* ..., arrayref, index, value => ... */
	VMINLINE VM_BytecodeAction
	aastore(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)(_sp + 2);
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			U_32 arrayLength = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
			U_32 index = *(U_32*)(_sp + 1);
			/* By using U_32 for index, we also catch the negative case, as all negative values are
			 * greater than the maximum array size (31 bits unsigned).
			 */
			if (index >= arrayLength) {
				_currentThread->tempSlot = (UDATA)index;
				rc = THROW_AIOB;
			} else {
				j9object_t value = *(j9object_t*)_sp;
				/* Runtime check class compatibility */
				if (false == objectArrayStoreAllowed(arrayref, value)) {
					rc = THROW_ARRAY_STORE;
				} else {
					_objectAccessBarrier.inlineIndexableObjectStoreObject(_currentThread, arrayref, index, value);
					_pc += 1;
					_sp += 3;
				}
			}
		}
		return rc;
	}

	/* ... => ..., value */
	VMINLINE VM_BytecodeAction
	aloadw(REGISTER_ARGS_LIST)
	{
		U_16 index = *(U_16*)(_pc + 1);
		_pc += 3;
		_sp -= 1;
		*_sp = *(_arg0EA - index);
		return EXECUTE_BYTECODE;
	}

	/* ... => ..., value1, value2 */
	VMINLINE VM_BytecodeAction
	lloadw(REGISTER_ARGS_LIST)
	{
		U_16 index = *(U_16*)(_pc + 1);
		_pc += 3;
		_sp -= 2;
		_sp[0] = *(_arg0EA - index - 1);
#if !defined(J9VM_ENV_DATA64)
		_sp[1] = *(_arg0EA - index);
#endif
		return EXECUTE_BYTECODE;
	}

	/* ..., value => ... */
	VMINLINE VM_BytecodeAction
	astorew(REGISTER_ARGS_LIST)
	{
		U_16 index = *(U_16*)(_pc + 1);
		_pc += 3;
		*(_arg0EA - index) = *_sp;
		_sp += 1;
		return EXECUTE_BYTECODE;
	}

	/* ..., value1, value2 => ... */
	VMINLINE VM_BytecodeAction
	lstorew(REGISTER_ARGS_LIST)
	{
		U_16 index = *(U_16*)(_pc + 1);
		_pc += 3;
		*(_arg0EA - index - 1) = _sp[0];
#if !defined(J9VM_ENV_DATA64)
		*(_arg0EA - index) = _sp[1];
#endif
		_sp += 2;
		return EXECUTE_BYTECODE;
	}

	/* ..., dividend, divisor => ..., result */
	VMINLINE VM_BytecodeAction
	idiv(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 divisor = *(I_32*)_sp;
		I_32 dividend = *(I_32*)(_sp + 1);
		if (0 == divisor) {
			rc = THROW_DIVIDE_BY_ZERO;
		} else {
			_pc += 1;
			_sp += 1;
			if (!((I_32_MIN == dividend) && (-1 == divisor))) {
				*(I_32*)_sp = dividend / divisor;
			}
		}
		return rc;
	}

	/* ..., dividend1, dividend2, divisor1, divisor2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	ldiv(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_64 divisor = *(I_64*)_sp;
		I_64 dividend = *(I_64*)(_sp + 2);
		if (0 == divisor) {
			rc = THROW_DIVIDE_BY_ZERO;
		} else {
			_pc += 1;
			_sp += 2;
			if (!((I_64_MIN == dividend) && (-1 == divisor))) {
				*(I_64*)_sp = dividend / divisor;
			}
		}
		return rc;
	}

	/* ..., dividend, divisor => ..., result */
	VMINLINE VM_BytecodeAction
	irem(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 divisor = *(I_32*)_sp;
		I_32 dividend = *(I_32*)(_sp + 1);
		if (0 == divisor) {
			rc = THROW_DIVIDE_BY_ZERO;
		} else {
			_pc += 1;
			_sp += 1;
			if ((I_32_MIN == dividend) && (-1 == divisor)) {
				*(I_32*)_sp = 0;
			} else {
				*(I_32*)_sp = dividend % divisor;
			}
		}
		return rc;
	}

	/* ..., dividend1, dividend2, divisor1, divisor2 => ..., result1, result2 */
	VMINLINE VM_BytecodeAction
	lrem(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_64 divisor = *(I_64*)_sp;
		I_64 dividend = *(I_64*)(_sp + 2);
		if (0 == divisor) {
			rc = THROW_DIVIDE_BY_ZERO;
		} else {
			_pc += 1;
			_sp += 2;
			if ((I_64_MIN == dividend) && (-1 == divisor)) {
				*(I_64*)_sp = 0;
			} else {
				*(I_64*)_sp = dividend % divisor;
			}
		}
		return rc;
	}

	/* ..., objectref => ..., length */
	VMINLINE VM_BytecodeAction
	arraylength(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t arrayref = *(j9object_t*)_sp;
		if (NULL == arrayref) {
			rc = THROW_NPE;
		} else {
			_pc += 1;
			*(U_32*)_sp = J9INDEXABLEOBJECT_SIZE(_currentThread, arrayref);
		}
		return rc;
	}

	/* <0, 1 or 2 slot value> => empty */
	VMINLINE VM_BytecodeAction
	returnSlotsImpl(REGISTER_ARGS_LIST, UDATA slots)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
#if defined(DO_HOOKS)
		UDATA *bp = NULL;
		bool hooked = J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_METHOD_RETURN);
		bool traced = VM_VMHelpers::methodBeingTraced(_vm, _literals);
		if (hooked || traced) {
			updateVMStruct(REGISTER_ARGS);
			if (traced) {
				UTSI_TRACEMETHODEXIT_FROMVM(_vm, _currentThread, _literals, NULL, _sp, 0);
			}
			if (hooked) {
				ALWAYS_TRIGGER_J9HOOK_VM_METHOD_RETURN(_vm->hookInterface, _currentThread, _literals, FALSE, _sp, 0);
			}
			VMStructHasBeenUpdated(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
				goto done;
			}
		}
		bp = bpForCurrentBytecodedMethod(REGISTER_ARGS);
		if (*bp & J9SF_A0_REPORT_FRAME_POP_TAG) {
			if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_FRAME_POP)) {
				updateVMStruct(REGISTER_ARGS);
				ALWAYS_TRIGGER_J9HOOK_VM_FRAME_POP(_vm->hookInterface, _currentThread, _literals, FALSE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
				bp = bpForCurrentBytecodedMethod(REGISTER_ARGS);
			}
			*bp &= ~(UDATA)J9SF_A0_REPORT_FRAME_POP_TAG;
		}
#endif
		if (_sp[slots] & J9_STACK_FLAGS_J2_IFRAME) {
			rc = j2iReturn(REGISTER_ARGS);
		} else {
			J9SFStackFrame *frame = (J9SFStackFrame*)(_sp + slots);
			UDATA returnValue0 = 0;
			UDATA returnValue1 = 0;
			if (0 != slots) {
				returnValue0 = _sp[0];
				if (2 == slots) {
					returnValue1 = _sp[1];
				}
			}
			_sp = _arg0EA + 1 - slots;
			_literals = frame->savedCP;
			_pc = frame->savedPC + 3;
			_arg0EA = frame->savedA0;
			if (0 != slots) {
				_sp[0] = returnValue0;
				if (2 == slots) {
					_sp[1] = returnValue1;
				}
			}
		}
#if defined(DO_HOOKS)
done:
#endif
		return rc;
	}

	/* <0, 1 or 2 slot value> => empty */
	VMINLINE VM_BytecodeAction
	syncReturn(REGISTER_ARGS_LIST, UDATA slots)
	{
		VM_BytecodeAction rc = THROW_ILLEGAL_MONITOR_STATE;
		j9object_t syncObject = ((j9object_t*)bpForCurrentBytecodedMethod(REGISTER_ARGS))[1];
		IDATA monitorRC = exitObjectMonitor(REGISTER_ARGS, syncObject);
		if (0 == monitorRC) {
			rc = returnSlotsImpl(REGISTER_ARGS, slots);
		}
		return rc;
	}

	/* <0, 1 or 2 slot value> => empty */
	VMINLINE VM_BytecodeAction
	returnSlots(REGISTER_ARGS_LIST, UDATA slots)
	{
		return returnSlotsImpl(REGISTER_ARGS, slots);
	}

	/* empty => empty */
	VMINLINE VM_BytecodeAction
	returnFromConstructor(REGISTER_ARGS_LIST)
	{
		VM_AtomicSupport::writeBarrier();
		return returnSlotsImpl(REGISTER_ARGS, 0);
	}

	VMINLINE VM_BytecodeAction
	returnC(REGISTER_ARGS_LIST)
	{
		*(I_32*)_sp = (U_16)*(I_32*)_sp;
		return returnSlots(REGISTER_ARGS, 1);
	}

	VMINLINE VM_BytecodeAction
	returnS(REGISTER_ARGS_LIST)
	{
		*(I_32*)_sp = (I_16)*(I_32*)_sp;
		return returnSlots(REGISTER_ARGS, 1);
	}

	VMINLINE VM_BytecodeAction
	returnB(REGISTER_ARGS_LIST)
	{
		*(I_32*)_sp = (I_8)*(I_32*)_sp;
		return returnSlots(REGISTER_ARGS, 1);
	}

	VMINLINE VM_BytecodeAction
	returnZ(REGISTER_ARGS_LIST)
	{
		*(U_32*)_sp = 1 & *(U_32*)_sp;
		return returnSlots(REGISTER_ARGS, 1);
	}

	/* ... => ..., value */
	VMINLINE VM_BytecodeAction
	ldc(REGISTER_ARGS_LIST, UDATA parmSize)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_16 index = 0;
		if (1 == parmSize) {
			index = _pc[1];
		} else if (2 == parmSize) {
			index = *(U_16*)(_pc + 1);
		} else {
			Assert_VM_unreachable();
		}
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMStringRef *ramCPEntry = ((J9RAMStringRef*)ramConstantPool) + index;
		J9ROMStringRef *romCPEntry = (J9ROMStringRef*)(ramConstantPool->romConstantPool + index);
		j9object_t volatile value = ramCPEntry->stringObject;
		if (J9_UNEXPECTED(NULL == value)) {
			/* Unresolved */
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			switch (romCPEntry->cpType & J9DescriptionCpTypeMask) {
			case J9DescriptionCpTypeClass:
				resolveClassRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
				break;
			case J9DescriptionCpTypeObject:
				resolveStringRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
				break;
			case J9DescriptionCpTypeMethodType:
				resolveMethodTypeRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
				break;
			case J9DescriptionCpTypeMethodHandle:
				resolveMethodHandleRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_NO_CLASS_INIT);
				break;
			default:
				Assert_VM_unreachable();
				break;
			}
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
				goto done;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			goto retry;
		}
		_pc += (1 + parmSize);
		_sp -= 1;
		if (J9DescriptionCpTypeClass == romCPEntry->cpType) {
			value = J9VM_J9CLASS_TO_HEAPCLASS((J9Class*)value);
		}
		*_sp = (UDATA)value;
done:
		return rc;
	}

	/* ... => ..., <1 or 2 slot value> */
	VMINLINE VM_BytecodeAction
	getstatic(REGISTER_ARGS_LIST)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMStaticFieldRef *ramStaticFieldRef = ((J9RAMStaticFieldRef*)ramConstantPool) + index;
		UDATA volatile valueOffset = ramStaticFieldRef->valueOffset;
		IDATA volatile flagsAndClass = ramStaticFieldRef->flagsAndClass;
		UDATA volatile classAndFlags = 0;
		void* volatile valueAddress = NULL;

		/* In an unresolved static fieldref, the valueOffset will be -1 or flagsAndClass will be <= 0.
		 * If the fieldref was resolved as an instance fieldref, the high bit of flagsAndClass will be
		 * set, so it will be < 0 and will be treated as an unresolved static fieldref.
		 *
		 * Since instruction re-ordering may result in us reading an updated valueOffset but
		 * a stale flagsAndClass, we check that both fields have been updated. It is crucial
		 * that we do not use a stale flagsAndClass with non-zero value, as doing so may cause the
		 * the StaticFieldRefDouble bit check to succeed when it shouldn't.
		 */
		bool resolveNeeded = (flagsAndClass <= 0) || (valueOffset == (UDATA)-1);
		if (J9_UNEXPECTED(resolveNeeded)) {
			/* Unresolved */
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			void *resolveResult = resolveStaticFieldRef(_currentThread, NULL, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_CHECK_CLINIT, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
				goto done;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			if ((void*)-1 != resolveResult) {
				goto retry;
			}
			ramStaticFieldRef = (J9RAMStaticFieldRef*)&_currentThread->floatTemp1;
			valueOffset = ramStaticFieldRef->valueOffset;
			flagsAndClass = ramStaticFieldRef->flagsAndClass;
		}
		/* Swap flags and class subfield order. */
		classAndFlags = J9CLASSANDFLAGS_FROM_FLAGSANDCLASS(flagsAndClass);
		valueAddress = J9STATICADDRESS(flagsAndClass, valueOffset);
#if defined(DO_HOOKS)
		if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_GET_STATIC_FIELD)) {
			J9Class *fieldClass = (J9Class*)(classAndFlags & ~(UDATA)J9StaticFieldRefFlagBits);
			if (J9_ARE_ANY_BITS_SET(fieldClass->classFlags, J9ClassHasWatchedFields)) {
				updateVMStruct(REGISTER_ARGS);
				ALWAYS_TRIGGER_J9HOOK_VM_GET_STATIC_FIELD(_vm->hookInterface, _currentThread, _literals, _pc - _literals->bytecodes, fieldClass, valueAddress);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
			}
		}
#endif
		{
			J9Class *fieldClass = (J9Class*)(classAndFlags & ~(UDATA)J9StaticFieldRefFlagBits);
			bool isVolatile = (0 != (classAndFlags & J9StaticFieldRefVolatile));
			if (classAndFlags & J9StaticFieldRefBaseType) {
				if (classAndFlags & J9StaticFieldRefDouble) {
					_sp -= 2;
					*(U_64*)_sp = _objectAccessBarrier.inlineStaticReadU64(_currentThread, fieldClass, (U_64 *)valueAddress, isVolatile);
				} else {
					_sp -= 1;
					*(U_32*)_sp = _objectAccessBarrier.inlineStaticReadU32(_currentThread, fieldClass, (U_32 *)valueAddress, isVolatile);
				}
			} else {
				_sp -= 1;
				*(j9object_t*)_sp = _objectAccessBarrier.inlineStaticReadObject(_currentThread, fieldClass, (j9object_t *)valueAddress, isVolatile);
			}
		}
		_pc += 3;
done:
		return rc;
	}

	/* ..., <1 or 2 slot value> => ... */
	VMINLINE VM_BytecodeAction
	putstatic(REGISTER_ARGS_LIST)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMStaticFieldRef *ramStaticFieldRef = ((J9RAMStaticFieldRef*)ramConstantPool) + index;
		UDATA volatile valueOffset = ramStaticFieldRef->valueOffset;
		IDATA volatile flagsAndClass = ramStaticFieldRef->flagsAndClass;
		UDATA volatile classAndFlags = 0;
		void* volatile valueAddress = NULL;
		void *resolveResult = NULL;

		/* In an unresolved static fieldref, the valueOffset will be -1 or flagsAndClass will be <= 0.
		 * If the fieldref was resolved as an instance fieldref, the high bit of flagsAndClass will be
		 * set, so it will be < 0 and will be treated as an unresolved static fieldref.
		 *
		 * Since instruction re-ordering may result in us reading an updated valueOffset but
		 * a stale flagsAndClass, we check that both fields have been updated. It is crucial
		 * that we do not use a stale flagsAndClass with non-zero value, as doing so may cause the
		 * the StaticFieldRefDouble bit check to succeed when it shouldn't.
		 *
		 * It is also necessary to check if the fieldref has been resolved for putstatic, since the
		 * fieldref could be shared with a getstatic. The math below checks the put-resolved flag
		 * in the flags byte of "flags and class", instead of the "class and flags" order expected by
		 * the J9StaticFieldRef* flag definitions.
		 */
		bool resolveNeeded = (flagsAndClass <= 0) || (valueOffset == (UDATA) -1)
				|| (0 == (flagsAndClass & (UDATA(J9StaticFieldRefPutResolved) << (8 * sizeof(UDATA) - J9_REQUIRED_CLASS_SHIFT))));
		if (J9_UNEXPECTED(resolveNeeded)) {
			/* Unresolved */
			J9Method *method = _literals;
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			resolveResult = resolveStaticFieldRef(_currentThread, method, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_FIELD_SETTER | J9_RESOLVE_FLAG_CHECK_CLINIT, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
				goto done;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			if ((void*)-1 != resolveResult) {
				goto retry;
			}
			ramStaticFieldRef = (J9RAMStaticFieldRef*)&_currentThread->floatTemp1;
			valueOffset = ramStaticFieldRef->valueOffset;
			flagsAndClass = ramStaticFieldRef->flagsAndClass;
		}
		/* Swap flags and class subfield order. */
		classAndFlags = J9CLASSANDFLAGS_FROM_FLAGSANDCLASS(flagsAndClass);
		valueAddress = J9STATICADDRESS(flagsAndClass, valueOffset);
#if defined(DO_HOOKS)
		if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_PUT_STATIC_FIELD)) {
			J9Class *fieldClass = (J9Class*)(classAndFlags & ~(UDATA)J9StaticFieldRefFlagBits);
			if (J9_ARE_ANY_BITS_SET(fieldClass->classFlags, J9ClassHasWatchedFields)) {
				updateVMStruct(REGISTER_ARGS);
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_STATIC_FIELD(_vm->hookInterface, _currentThread, _literals, _pc - _literals->bytecodes, fieldClass, valueAddress, *(U_64*)_sp);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
			}
		}
#endif
		{
			J9Class *fieldClass = (J9Class*)(classAndFlags & ~(UDATA)J9StaticFieldRefFlagBits);
			bool isVolatile = (0 != (classAndFlags & J9StaticFieldRefVolatile));
			if (classAndFlags & J9StaticFieldRefBaseType) {
				if (classAndFlags & J9StaticFieldRefDouble) {
					_objectAccessBarrier.inlineStaticStoreU64(_currentThread, fieldClass, (U_64*)valueAddress, *(U_64*)_sp, isVolatile);
					_sp += 2;
				} else {
					_objectAccessBarrier.inlineStaticStoreU32(_currentThread, fieldClass, (U_32*)valueAddress, *(U_32*)_sp, isVolatile);
					_sp += 1;
				}
			} else {
				_objectAccessBarrier.inlineStaticStoreObject(_currentThread, fieldClass, (j9object_t*)valueAddress, *(j9object_t*)_sp, isVolatile);
				_sp += 1;
			}
		}
		_pc += 3;
done:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	getfieldLogic(REGISTER_ARGS_LIST, UDATA indexOffset, j9object_t *objectLocation, UDATA slotsToPop)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_16 index = *(U_16*)(_pc + indexOffset);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMFieldRef *ramFieldRef = ((J9RAMFieldRef*)ramConstantPool) + index;
		UDATA const flags = ramFieldRef->flags;
		UDATA const valueOffset = ramFieldRef->valueOffset;

		/* In a resolved field, flags will have the J9FieldFlagResolved bit set, thus
		 * having a higher value than any valid valueOffset.
		 *
		 * This check avoids the need for a barrier, as it will only succeed if flags
		 * and valueOffset have both been updated. It is crucial that we do not treat
		 * a field ref as resolved if only one of the two values has been set (by
		 * another thread that is in the middle of a resolve).
		 */
		if (J9_UNEXPECTED(flags <= valueOffset)) {
			/* Unresolved */
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			resolveInstanceFieldRef(_currentThread, NULL, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
				goto done;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			goto retry;
		}
		{
			j9object_t objectref = *objectLocation;
			if (NULL == objectref) {
				rc = THROW_NPE;
			} else {
#if defined(DO_HOOKS)
				if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
					if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(_currentThread, objectref)->classFlags, J9ClassHasWatchedFields)) {
						updateVMStruct(REGISTER_ARGS);
						ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(_vm->hookInterface, _currentThread, _literals, _pc - _literals->bytecodes, objectref, valueOffset);
						VMStructHasBeenUpdated(REGISTER_ARGS);
						if (immediateAsyncPending()) {
							rc = GOTO_ASYNC_CHECK;
							goto done;
						}
						objectref = *objectLocation;
					}
				}
#endif

				{
					UDATA const newValueOffset = valueOffset + J9_OBJECT_HEADER_SIZE;
					bool isVolatile = (0 != (flags & J9AccVolatile));
					if (flags & J9FieldSizeDouble) {
						_sp += (slotsToPop - 2);
						*(U_64*)_sp = _objectAccessBarrier.inlineMixedObjectReadU64(_currentThread, objectref, newValueOffset, isVolatile);
					} else if (flags & J9FieldFlagObject) {
						_sp += (slotsToPop - 1);
						*(j9object_t*)_sp = _objectAccessBarrier.inlineMixedObjectReadObject(_currentThread, objectref, newValueOffset, isVolatile);
					} else {
						_sp += (slotsToPop - 1);
						*(U_32*)_sp = _objectAccessBarrier.inlineMixedObjectReadU32(_currentThread, objectref, newValueOffset, isVolatile);
					}
				}
				_pc += (indexOffset + 2);
			}
		}
done:
		return rc;
	}

	/* ... => ..., <1 or 2 slot value> */
	VMINLINE VM_BytecodeAction
	aload0getfield(REGISTER_ARGS_LIST)
	{
		return getfieldLogic(REGISTER_ARGS, 2, (j9object_t*)_arg0EA, 0);
	}

	/* ..., objectref => ..., <1 or 2 slot value> */
	VMINLINE VM_BytecodeAction
	getfield(REGISTER_ARGS_LIST)
	{
		return getfieldLogic(REGISTER_ARGS, 1, (j9object_t*)_sp, 1);
	}

	/* ..., objectref, <1 or 2 slot value> => ... */
	VMINLINE VM_BytecodeAction
	putfield(REGISTER_ARGS_LIST)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMFieldRef *ramFieldRef = ((J9RAMFieldRef*)ramConstantPool) + index;
		UDATA const flags = ramFieldRef->flags;
		UDATA const valueOffset = ramFieldRef->valueOffset;

		/* In a resolved field, flags will have the J9FieldFlagResolved bit set, thus
		 * having a higher value than any valid valueOffset.
		 *
		 * This check avoids the need for a barrier, as it will only succeed if flags
		 * and valueOffset have both been updated. It is crucial that we do not treat
		 * a field ref as resolved if only one of the two values has been set (by
		 * another thread that is in the middle of a resolve).
		 */
		if (J9_UNEXPECTED((flags <= valueOffset) || (0 == (flags & J9FieldFlagPutResolved)))) {
			/* Unresolved */
			J9Method *method = _literals;
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			resolveInstanceFieldRef(_currentThread, method, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_FIELD_SETTER, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
				goto done;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			goto retry;
		}
#if defined(DO_HOOKS)
		if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
			j9object_t objectref = ((j9object_t*)_sp)[(flags & J9FieldSizeDouble) ? 2 : 1];
			if (NULL != objectref) {
				if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(_currentThread, objectref)->classFlags, J9ClassHasWatchedFields)) {
					updateVMStruct(REGISTER_ARGS);
					ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(_vm->hookInterface, _currentThread, _literals, _pc - _literals->bytecodes, objectref, valueOffset, *(U_64*)_sp);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					if (immediateAsyncPending()) {
						rc = GOTO_ASYNC_CHECK;
						goto done;
					}
				}
			}
		}
#endif
		{
			bool isVolatile = (0 != (flags & J9AccVolatile));
			UDATA const newValueOffset = valueOffset + J9_OBJECT_HEADER_SIZE;
			if (flags & J9FieldSizeDouble) {
				j9object_t objectref = *(j9object_t*)(_sp + 2);
				if (NULL == objectref) {
					rc = THROW_NPE;
					goto done;
				}
				_objectAccessBarrier.inlineMixedObjectStoreU64(_currentThread, objectref, newValueOffset, *(U_64*)_sp, isVolatile);
				_sp += 3;
			} else if (flags & J9FieldFlagObject) {
				j9object_t objectref = *(j9object_t*)(_sp + 1);
				if (NULL == objectref) {
					rc = THROW_NPE;
					goto done;
				}
				_objectAccessBarrier.inlineMixedObjectStoreObject(_currentThread, objectref, newValueOffset, *(j9object_t*)_sp, isVolatile);
				_sp += 2;
			} else {
				j9object_t objectref = *(j9object_t*)(_sp + 1);
				if (NULL == objectref) {
					rc = THROW_NPE;
					goto done;
				}
				_objectAccessBarrier.inlineMixedObjectStoreU32(_currentThread, objectref, newValueOffset, *(U_32*)_sp, isVolatile);
				_sp += 2;
			}
		}
		_pc += 3;
done:
		return rc;
	}

	/* ..., objectref => objectref */
	VMINLINE VM_BytecodeAction
	athrow(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_THROW_CURRENT_EXCEPTION;
		j9object_t objectref = *(j9object_t*)_sp;
		_sp += 1;
		if (J9_UNEXPECTED(NULL == objectref)) {
			rc = THROW_NPE;
		} else {
			VM_VMHelpers::setExceptionPending(_currentThread, objectref);
		}
		return rc;
	}

	VMINLINE VM_BytecodeAction
	invokevirtualLogic(REGISTER_ARGS_LIST, bool fromBytecode)
	{
		VM_BytecodeAction rc = GOTO_RUN_METHOD;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMVirtualMethodRef *ramMethodRef = ((J9RAMVirtualMethodRef*)ramConstantPool) + index;
		UDATA volatile methodIndexAndArgCount = ramMethodRef->methodIndexAndArgCount;
		UDATA methodIndex = methodIndexAndArgCount >> 8;
		j9object_t receiver = ((j9object_t*)_sp)[methodIndexAndArgCount & 0xFF];
		if (J9_UNEXPECTED(NULL == receiver)) {
			/* Resolution exceptions must be thrown first, so check if methodRef 
			 * is resolved before throwing NPE on receiver.
			 */
			if (methodIndex != J9VTABLE_INITIAL_VIRTUAL_OFFSET) {
				rc = THROW_NPE;
			} else {
				_sendMethod = (J9Method *)_vm->initialMethods.initialVirtualMethod;
			}
		} else {
			J9Class *receiverClass = J9OBJECT_CLAZZ(_currentThread, receiver);
			_sendMethod = *(J9Method**)((UDATA)receiverClass + methodIndex);
			if (fromBytecode) {
				profileInvokeReceiver(REGISTER_ARGS, receiverClass, _literals, _sendMethod);
			}
		}

		return rc;
	}

	VMINLINE VM_BytecodeAction
	invokevirtual(REGISTER_ARGS_LIST)
	{
		return invokevirtualLogic(REGISTER_ARGS, true);
	}

	VMINLINE VM_BytecodeAction
	invokespecial(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_RUN_METHOD;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMSpecialMethodRef *ramMethodRef = ((J9RAMSpecialMethodRef*)ramConstantPool) + index;
		/* argCount was initialized when we initialized the class (i.e. it is non-volatile), so no memory barrier is required */
		j9object_t receiver = ((j9object_t*)_sp)[ramMethodRef->methodIndexAndArgCount & 0xFF];
		if (NULL == receiver) {
			rc = THROW_NPE;
		} else {
			profileCallingMethod(REGISTER_ARGS);
			_sendMethod = ramMethodRef->method;
			J9Class *currentClass = J9_CLASS_FROM_CP(ramConstantPool);
			/* hostClass is exclusively defined only in Unsafe.defineAnonymousClass.
			 * For all other cases, clazz->hostClass points to itself (clazz).
			 */
			currentClass = currentClass->hostClass;
			if (J9_ARE_ALL_BITS_SET(currentClass->romClass->modifiers, J9AccInterface)) {
				J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)&ramConstantPool->romConstantPool[index];
				J9ROMNameAndSignature *nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romMethodRef);
				J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
				U_8 *name = J9UTF8_DATA(nameUTF);
				UDATA nameLength = J9UTF8_LENGTH(nameUTF);
				/* Ignore <init> or <clinit> methods */
				if ((0 != nameLength) && (name[0] != '<')) {
					/* Resolve special method ref when _sendMethod == initialSpecialMethod */
					if (_sendMethod == _vm->initialMethods.initialSpecialMethod) {
						buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
						updateVMStruct(REGISTER_ARGS);
						_sendMethod = resolveSpecialMethodRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
						VMStructHasBeenUpdated(REGISTER_ARGS);
						restoreGenericSpecialStackFrame(REGISTER_ARGS);
						if (immediateAsyncPending()) {
							rc = GOTO_ASYNC_CHECK;
							goto exit;
						}
						if (VM_VMHelpers::exceptionPending(_currentThread)) {
							rc = GOTO_THROW_CURRENT_EXCEPTION;
							goto exit;
						}
					}
					/* Check the receiver class is the subtype of the current class [= interface] */
					J9Class *resolvedClass = J9_CLASS_FROM_METHOD(_sendMethod);
					J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod);
					/* Skip the check for final methods in java.lang.Object (resolvedClassDepth == 0) */
					if ((0 != J9CLASS_DEPTH(resolvedClass)) || J9_ARE_NO_BITS_SET(romMethod->modifiers, J9AccFinal)) {
						J9Class *receiverClass = J9OBJECT_CLAZZ(_currentThread, receiver);
						if (!instanceOfOrCheckCast(receiverClass, currentClass)) {
							buildMethodFrame(REGISTER_ARGS, _sendMethod, 0);
							updateVMStruct(REGISTER_ARGS);
							setIllegalAccessErrorReceiverNotSameOrSubtypeOfCurrentClass(_currentThread, receiverClass, currentClass);
							VMStructHasBeenUpdated(REGISTER_ARGS);
							rc = GOTO_THROW_CURRENT_EXCEPTION;
						}
					}
				}
			}
		}
exit:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	invokespecialsplit(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_RUN_METHOD;
		U_16 splitTableIndex = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = (J9ConstantPool *)J9_CP_FROM_METHOD(_literals);
		J9Class *clazz = ramConstantPool->ramClass;
		Assert_VM_true(splitTableIndex < clazz->romClass->specialSplitMethodRefCount);
		U_16 cpIndex = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(clazz->romClass) + splitTableIndex);
		J9RAMSpecialMethodRef *ramMethodRef = (J9RAMSpecialMethodRef*)ramConstantPool + cpIndex;
		/* argCount was initialized when we initialized the class (i.e. it is non-volatile), so no memory barrier is required */
		if (0 == _sp[ramMethodRef->methodIndexAndArgCount & 0xFF]) {
			rc = THROW_NPE;
		} else {
			_sendMethod = clazz->specialSplitMethodTable[splitTableIndex];
			if (NULL == _sendMethod) {
				buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
				updateVMStruct(REGISTER_ARGS);
				J9Method *method = resolveSpecialSplitMethodRef(_currentThread, ramConstantPool, splitTableIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				restoreGenericSpecialStackFrame(REGISTER_ARGS);
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
				} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
					rc = GOTO_THROW_CURRENT_EXCEPTION;
				} else {
					profileCallingMethod(REGISTER_ARGS);
					_sendMethod = method;
				}
			}
		}
		return rc;
	}

	VMINLINE VM_BytecodeAction
	invokestatic(REGISTER_ARGS_LIST)
	{
		U_16 index = *(U_16*)(_pc + 1);
		profileCallingMethod(REGISTER_ARGS);
		J9RAMStaticMethodRef *ramMethodRef = ((J9RAMStaticMethodRef*)J9_CP_FROM_METHOD(_literals)) + index;
		_sendMethod = ramMethodRef->method;
		return GOTO_RUN_METHOD;
	}

	VMINLINE VM_BytecodeAction
	invokestaticsplit(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_RUN_METHOD;
		U_16 splitTableIndex = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9Class *clazz = ramConstantPool->ramClass;
		Assert_VM_true(splitTableIndex < clazz->romClass->staticSplitMethodRefCount);

		_sendMethod = clazz->staticSplitMethodTable[splitTableIndex];
		if (NULL == _sendMethod) {
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			J9Method *method = resolveStaticSplitMethodRef(_currentThread, ramConstantPool, splitTableIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
			if ((J9Method*)-1 == method) {
				method = ((J9RAMStaticMethodRef*)&_currentThread->floatTemp1)->method;
			}
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
			} else {
				profileCallingMethod(REGISTER_ARGS);
				_sendMethod = method;
			}
		}
		return rc;
	}

	VMINLINE VM_BytecodeAction
	invokeinterfaceOffset(REGISTER_ARGS_LIST, UDATA offset)
	{
retry:
		VM_BytecodeAction rc = GOTO_RUN_METHOD;
		U_16 index = *(U_16*)(_pc + 1 + offset);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMInterfaceMethodRef *ramMethodRef = ((J9RAMInterfaceMethodRef*)ramConstantPool) + index;
		/* interfaceClass is used to indicate 'resolved'. Make sure to read it first */
		J9Class* const interfaceClass = (J9Class*)ramMethodRef->interfaceClass;
		VM_AtomicSupport::readBarrier();
		UDATA const methodIndexAndArgCount = ramMethodRef->methodIndexAndArgCount;
		j9object_t receiver = ((j9object_t*)_sp)[methodIndexAndArgCount & 0xFF];
		if (J9_UNEXPECTED(NULL == receiver)) {
			if (NULL == interfaceClass) {
resolve:
				/* Unresolved */
				buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
				updateVMStruct(REGISTER_ARGS);
				resolveInterfaceMethodRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				restoreGenericSpecialStackFrame(REGISTER_ARGS);
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
				goto retry;
			}
			rc = THROW_NPE;
		} else {
			J9Class *receiverClass = J9OBJECT_CLAZZ(_currentThread, receiver);
			UDATA methodIndex = methodIndexAndArgCount >> 8;
			J9ROMMethod *romMethod = NULL;

			/* Run search in receiverClass->lastITable */
			J9ITable *iTable = receiverClass->lastITable;
			if (interfaceClass == iTable->interfaceClass) {
				goto foundITableCache;
			}

			/* Start search from receiverClass->iTable */
			iTable = (J9ITable*)receiverClass->iTable;
			while (NULL != iTable) {
				if (interfaceClass == iTable->interfaceClass) {
					receiverClass->lastITable = iTable;
foundITableCache:
					_sendMethod = *(J9Method**)((UDATA)receiverClass + ((UDATA*)(iTable + 1))[methodIndex]);
					romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod);
					if (!J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccPublic)) {
						/* We need a frame to describe the method arguments (in particular, for the case where we got here directly from the JIT) */
						buildMethodFrame(REGISTER_ARGS, _sendMethod, jitStackFrameFlags(REGISTER_ARGS, 0));
						updateVMStruct(REGISTER_ARGS);
						setIllegalAccessErrorNonPublicInvokeInterface(_currentThread, _sendMethod);
						VMStructHasBeenUpdated(REGISTER_ARGS);
						rc = GOTO_THROW_CURRENT_EXCEPTION;
						goto done;
					}
foundMethod:
					profileInvokeReceiver(REGISTER_ARGS, receiverClass, _literals, _sendMethod);
					_pc += offset;
					goto done;
				}
				iTable = iTable->next;
			}
			if (J9_UNEXPECTED(NULL == interfaceClass)) {
				goto resolve;
			}
			if (J9_EXPECTED(0 == (interfaceClass->romClass->modifiers & J9AccInterface))) {
				/* Must be able to call java/lang/Object methods on interface */
				_sendMethod = ((J9Method*)javaLookupMethod(
					_currentThread,
					receiverClass,
					J9ROMMETHODREF_NAMEANDSIGNATURE(((J9ROMMethodRef*)ramConstantPool->romConstantPool + index)),
					NULL,
					J9_LOOK_VIRTUAL));
				goto foundMethod;
			} else {
				rc = THROW_INCOMPATIBLE_CLASS_CHANGE;
			}
		}
done:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	invokeinterface2(REGISTER_ARGS_LIST)
	{
		return invokeinterfaceOffset(REGISTER_ARGS, 2);
	}

	VMINLINE VM_BytecodeAction
	invokeinterface(REGISTER_ARGS_LIST)
	{
		return invokeinterfaceOffset(REGISTER_ARGS, 0);
	}

#if defined(J9VM_ENV_DATA64)
	/* ..., objectref => ... */
	VMINLINE VM_BytecodeAction
	ifnull(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		if (*_sp++ == 0) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., objectref => ... */
	VMINLINE VM_BytecodeAction
	ifnonnull(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		if (*_sp++ != 0) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}
#endif

	/* ..., value => ... */
	VMINLINE VM_BytecodeAction
	ifeq(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		if (*(I_32*)(_sp++) == 0) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., value => ... */
	VMINLINE VM_BytecodeAction
	ifne(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		if (*(I_32*)(_sp++) != 0) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., value => ... */
	VMINLINE VM_BytecodeAction
	iflt(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		if (*(I_32*)(_sp++) < 0) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., value => ... */
	VMINLINE VM_BytecodeAction
	ifge(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		if (*(I_32*)(_sp++) >= 0) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., value => ... */
	VMINLINE VM_BytecodeAction
	ifgt(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		if (*(I_32*)(_sp++) > 0) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., value => ... */
	VMINLINE VM_BytecodeAction
	ifle(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		if (*(I_32*)(_sp++) <= 0) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., lhs, rhs => ... */
	VMINLINE VM_BytecodeAction
	ificmpeq(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 rhs = *(I_32*)_sp;
		I_32 lhs = *(I_32*)(_sp + 1);
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		_sp += 2;
		if (lhs == rhs) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., lhs, rhs => ... */
	VMINLINE VM_BytecodeAction
	ificmpne(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 rhs = *(I_32*)_sp;
		I_32 lhs = *(I_32*)(_sp + 1);
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		_sp += 2;
		if (lhs != rhs) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., lhs, rhs => ... */
	VMINLINE VM_BytecodeAction
	ificmplt(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 rhs = *(I_32*)_sp;
		I_32 lhs = *(I_32*)(_sp + 1);
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		_sp += 2;
		if (lhs < rhs) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., lhs, rhs => ... */
	VMINLINE VM_BytecodeAction
	ificmpge(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 rhs = *(I_32*)_sp;
		I_32 lhs = *(I_32*)(_sp + 1);
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		_sp += 2;
		if (lhs >= rhs) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., lhs, rhs => ... */
	VMINLINE VM_BytecodeAction
	ificmpgt(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 rhs = *(I_32*)_sp;
		I_32 lhs = *(I_32*)(_sp + 1);
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		_sp += 2;
		if (lhs > rhs) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., lhs, rhs => ... */
	VMINLINE VM_BytecodeAction
	ificmple(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		I_32 rhs = *(I_32*)_sp;
		I_32 lhs = *(I_32*)(_sp + 1);
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		_sp += 2;
		if (lhs <= rhs) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., lhs, rhs => ... */
	VMINLINE VM_BytecodeAction
	ifacmpeq(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t rhs = *(j9object_t*)_sp;
		j9object_t lhs = *(j9object_t*)(_sp + 1);
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		_sp += 2;
		if (lhs == rhs) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ..., lhs, rhs => ... */
	VMINLINE VM_BytecodeAction
	ifacmpne(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t rhs = *(j9object_t*)_sp;
		j9object_t lhs = *(j9object_t*)(_sp + 1);
		U_8 *profilingCursor = startProfilingRecord(REGISTER_ARGS, sizeof(U_8));
		_sp += 2;
		if (lhs != rhs) {
			_pc += *(I_16*)(_pc + 1);
			if (NULL != profilingCursor) {
				*profilingCursor = 1;
			}
			rc = GOTO_BRANCH_WITH_ASYNC_CHECK;
		} else {
			_pc += 3;
			if (NULL != profilingCursor) {
				*profilingCursor = 0;
			}
		}
		return rc;
	}

	/* ... => ... */
	VMINLINE VM_BytecodeAction
	gotoImpl(REGISTER_ARGS_LIST, UDATA parmSize)
	{
		if (parmSize == 2) {
			_pc += *(I_16*)(_pc + 1);
		} else if (parmSize == 4) {
			_pc += *(I_32*)(_pc + 1);
		} else {
			Assert_VM_unreachable();
		}
		return GOTO_BRANCH_WITH_ASYNC_CHECK;
	}

	/* ..., index => ... */
	VMINLINE VM_BytecodeAction
	tableswitch(REGISTER_ARGS_LIST)
	{
		I_32 *data = (I_32*)((((UDATA)_pc)+1+3)&~(UDATA)3);
		I_32 defaultValue = *data++;
		I_32 low = *data++;
		I_32 high = *data++;
		I_32 index = *(I_32*)_sp;
		profileSwitch(REGISTER_ARGS, index);
		_sp += 1;
		if ((index >= low) && (index <= high)) {
			_pc += data[index - low];
		} else {
			_pc += defaultValue;
		}
		return GOTO_BRANCH_WITH_ASYNC_CHECK;
	}

	/* ..., key => ... */
	VMINLINE VM_BytecodeAction
	lookupswitch(REGISTER_ARGS_LIST)
	{
		I_32 *data = (I_32*)((((UDATA)_pc)+1+3)&~(UDATA)3);
		I_32 defaultValue = *data++;
		I_32 nPairs = *data++;
		I_32 key = *(I_32*)_sp;
		profileSwitch(REGISTER_ARGS, key);
		_sp += 1;
		while (nPairs != 0) {
			I_32 matchKey = *data++;
			I_32 matchOffset = *data++;
			if (key == matchKey) {
				_pc += matchOffset;
				goto done;
			}
			nPairs -= 1;
		}
		_pc += defaultValue;
done:
		return GOTO_BRANCH_WITH_ASYNC_CHECK;
	}

	VMINLINE VM_BytecodeAction
	newLogic(REGISTER_ARGS_LIST, j9object_t *newObject)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMClassRef *ramCPEntry = ((J9RAMClassRef*)ramConstantPool) + index;
		J9Class* volatile resolvedClass = ramCPEntry->value;
		if ((NULL != resolvedClass) && (resolvedClass->romClass->modifiers & (J9AccAbstract | J9AccInterface)) == 0) {
			if (!VM_VMHelpers::classRequiresInitialization(_currentThread, resolvedClass)) {
				j9object_t instance = _objectAllocate.inlineAllocateObject(_currentThread, resolvedClass);
				if (NULL == instance) {
					updateVMStruct(REGISTER_ARGS);
					instance = _vm->memoryManagerFunctions->J9AllocateObject(_currentThread, resolvedClass, J9_GC_ALLOCATE_OBJECT_INSTRUMENTABLE);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					if (J9_UNEXPECTED(NULL == instance)) {
						rc = THROW_HEAP_OOM;
						goto done;
					}
				}
				*newObject = instance;
				goto done;
			}
			/* Class requires initialization */
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			initializeClass(_currentThread, resolvedClass);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
				goto done;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			goto retry;
		}
		/* Unresolved */
		buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
		updateVMStruct(REGISTER_ARGS);
		resolveClassRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_INIT_CLASS | J9_RESOLVE_FLAG_INSTANTIABLE);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		restoreGenericSpecialStackFrame(REGISTER_ARGS);
		if (immediateAsyncPending()) {
			rc = GOTO_ASYNC_CHECK;
			goto done;
		} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		goto retry;
done:
		return rc;
	}

	/* ... => ..., objectref */
	VMINLINE VM_BytecodeAction
	newImpl(REGISTER_ARGS_LIST)
	{
		j9object_t newObject = NULL;
		VM_BytecodeAction rc = newLogic(REGISTER_ARGS, &newObject);
		if (EXECUTE_BYTECODE == rc) {
			_pc += 3;
			*(j9object_t*)--_sp = newObject;
		}
		return rc;
	}

	/* ... => ..., objectref, objectref */
	VMINLINE VM_BytecodeAction
	newdup(REGISTER_ARGS_LIST)
	{
		j9object_t newObject = NULL;
		VM_BytecodeAction rc = newLogic(REGISTER_ARGS, &newObject);
		if (EXECUTE_BYTECODE == rc) {
			_pc += 4;
			*(j9object_t*)--_sp = newObject;
			*(j9object_t*)--_sp = newObject;
		}
		return rc;
	}

	/* ..., count => ..., arrayref */
	VMINLINE VM_BytecodeAction
	newarray(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_8 arrayType = _pc[1];
		I_32 size = *(I_32*)_sp;
		if (J9_UNEXPECTED(size < 0)) {
			rc = THROW_NEGATIVE_ARRAY_SIZE;
		} else {
			J9Class *arrayClass = (&(_vm->booleanArrayClass))[arrayType - 4];
			j9object_t instance = _objectAllocate.inlineAllocateIndexableObject(_currentThread, arrayClass, (U_32)size);
			if (NULL == instance) {
				updateVMStruct(REGISTER_ARGS);
				instance = _vm->memoryManagerFunctions->J9AllocateIndexableObject(_currentThread, arrayClass, (U_32)size, J9_GC_ALLOCATE_OBJECT_INSTRUMENTABLE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				if (J9_UNEXPECTED(NULL == instance)) {
					rc = THROW_HEAP_OOM;
					goto done;
				}
			}
			_pc += 2;
			*(j9object_t*)_sp = instance;
		}
done:
		return rc;
	}

	/* ..., count => ..., arrayref */
	VMINLINE VM_BytecodeAction
	anewarray(REGISTER_ARGS_LIST)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMClassRef *ramCPEntry = ((J9RAMClassRef*)ramConstantPool) + index;
		J9Class* volatile resolvedClass = ramCPEntry->value;
		I_32 size = *(I_32*)_sp;
		if (J9_UNEXPECTED(size < 0)) {
			rc = THROW_NEGATIVE_ARRAY_SIZE;
			goto done;
		}
		if (J9_EXPECTED(NULL != resolvedClass)) {
			J9Class *arrayClass = resolvedClass->arrayClass;
			if (J9_EXPECTED(NULL != arrayClass)) {
				j9object_t instance = _objectAllocate.inlineAllocateIndexableObject(_currentThread, arrayClass, (U_32)size);
				if (NULL == instance) {
					updateVMStruct(REGISTER_ARGS);
					instance = _vm->memoryManagerFunctions->J9AllocateIndexableObject(_currentThread, arrayClass, (U_32)size, J9_GC_ALLOCATE_OBJECT_INSTRUMENTABLE);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					if (J9_UNEXPECTED(NULL == instance)) {
						rc = THROW_HEAP_OOM;
						goto done;
					}
				}
				_pc += 3;
				*(j9object_t*)_sp = instance;
				goto done;
			}
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			arrayClass = internalCreateArrayClass(_currentThread, (J9ROMArrayClass*)J9ROMIMAGEHEADER_FIRSTCLASS(_vm->arrayROMClasses), resolvedClass);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (VM_VMHelpers::exceptionPending(_currentThread) ) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			goto retry;
		}
		/* Unresolved */
		buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
		updateVMStruct(REGISTER_ARGS);
		resolveClassRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		restoreGenericSpecialStackFrame(REGISTER_ARGS);
		if (immediateAsyncPending()) {
			rc = GOTO_ASYNC_CHECK;
			goto done;
		} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
			rc = GOTO_THROW_CURRENT_EXCEPTION;
			goto done;
		}
		goto retry;
done:
		return rc;
	}

	/* ..., objectref => ..., objectref */
	VMINLINE VM_BytecodeAction
	checkcast(REGISTER_ARGS_LIST)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t obj = *(j9object_t*)_sp;
		if (NULL != obj) {
			U_16 index = *(U_16*)(_pc + 1);
			J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
			J9RAMClassRef *ramCPEntry = ((J9RAMClassRef*)ramConstantPool) + index;
			J9Class* volatile castClass = ramCPEntry->value;
			if (NULL != castClass) {
				J9Class *instanceClass = J9OBJECT_CLAZZ(_currentThread, obj);
				profileCast(REGISTER_ARGS, instanceClass);
				if (!VM_VMHelpers::inlineCheckCast(instanceClass, castClass)) {
					updateVMStruct(REGISTER_ARGS);
					prepareForExceptionThrow(_currentThread);
					setClassCastException(_currentThread, instanceClass, castClass);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
			} else {
				/* Unresolved */
				buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
				updateVMStruct(REGISTER_ARGS);
				resolveClassRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				restoreGenericSpecialStackFrame(REGISTER_ARGS);
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
				goto retry;
			}
		}
		_pc += 3;
done:
		return rc;
	}

	/* ..., objectref => ..., result */
	VMINLINE VM_BytecodeAction
	instanceof(REGISTER_ARGS_LIST)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t obj = *(j9object_t*)_sp;
		if (NULL != obj) {
			U_16 index = *(U_16*)(_pc + 1);
			J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
			J9RAMClassRef *ramCPEntry = ((J9RAMClassRef*)ramConstantPool) + index;
			J9Class* volatile castClass = ramCPEntry->value;
			if (NULL != castClass) {
				J9Class *instanceClass = J9OBJECT_CLAZZ(_currentThread, obj);
				profileCast(REGISTER_ARGS, instanceClass);
				*(I_32*)_sp = (I_32)VM_VMHelpers::inlineCheckCast(instanceClass, castClass);
			} else {
				/* Unresolved */
				buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
				updateVMStruct(REGISTER_ARGS);
				resolveClassRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				restoreGenericSpecialStackFrame(REGISTER_ARGS);
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
				goto retry;
			}
		} else {
			*(I_32*)_sp = 0;
		}
		_pc += 3;
done:
		return rc;
	}

	/* ..., objectref => ... */
	VMINLINE VM_BytecodeAction
	monitorenter(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t obj = *(j9object_t*)_sp;
		if (NULL == obj) {
			rc = THROW_NPE;
		} else {
			IDATA monitorRC = enterObjectMonitor(REGISTER_ARGS, obj);
			/* Monitor enter can only fail in the nonblocking case, which does not
			 * release VM access, so the immediate async and failed enter cases are
			 * mutually exclusive.
			 */
			if (0 == monitorRC) {
				rc = THROW_MONITOR_ALLOC_FAIL;
			} else {
				if (J9_UNEXPECTED(!VM_ObjectMonitor::recordBytecodeMonitorEnter(_currentThread, (j9object_t)monitorRC, _arg0EA))) {
					objectMonitorExit(_currentThread, obj);
					rc = THROW_MONITOR_ALLOC_FAIL;
					if (immediateAsyncPending()) {
						rc = GOTO_ASYNC_CHECK;
					}
					goto done;
				}
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
				_pc += 1;
				_sp += 1;
			}
		}
done:
		return rc;
	}

	/* ..., objectref => ... */
	VMINLINE VM_BytecodeAction
	monitorexit(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		j9object_t obj = *(j9object_t*)_sp;
		_sp += 1;
		if (NULL == obj) {
			rc = THROW_NPE;
		} else {
			IDATA monitorRC = exitObjectMonitor(REGISTER_ARGS, obj);
			if (0 != monitorRC) {
				rc = THROW_ILLEGAL_MONITOR_STATE;
			} else {
				VM_ObjectMonitor::recordBytecodeMonitorExit(_currentThread, obj);
				_pc += 1;
			}
		}
		return rc;
	}

	/* ..., count1, [count2, ...] => ..., arrayref */
	VMINLINE VM_BytecodeAction
	multianewarray(REGISTER_ARGS_LIST)
	{
retry:
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		U_16 index = *(U_16*)(_pc + 1);
		UDATA dimensions = _pc[3];
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMClassRef *ramCPEntry = ((J9RAMClassRef*)ramConstantPool) + index;
		J9Class* volatile resolvedClass = ramCPEntry->value;
		if (NULL != resolvedClass) {
			J9Class *arrayClass = resolvedClass->arrayClass;
			j9object_t instance = NULL;
			I_32 *dimensionsArray = NULL;
			if (NULL == arrayClass) {
				buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
				updateVMStruct(REGISTER_ARGS);
				arrayClass = internalCreateArrayClass(_currentThread, (J9ROMArrayClass*)J9ROMIMAGEHEADER_FIRSTCLASS(_vm->arrayROMClasses), resolvedClass);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				restoreGenericSpecialStackFrame(REGISTER_ARGS);
				if (VM_VMHelpers::exceptionPending(_currentThread) ) {
					rc = GOTO_THROW_CURRENT_EXCEPTION;
					goto done;
				}
			}
#if defined(J9VM_ENV_DATA64)
			for (UDATA i = 1; i <= dimensions; ++i) {
				((I_32*)_sp)[i] = ((I_32*)_sp)[i*2];
			}
#endif
			dimensionsArray = (I_32*)_sp;
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			instance = helperMultiANewArray(_currentThread, (J9ArrayClass*)arrayClass, dimensions, dimensionsArray, J9_GC_ALLOCATE_OBJECT_INSTRUMENTABLE);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			_pc += 4;
			_sp += (dimensions - 1);
			*(j9object_t*)_sp = instance;
		} else {
			/* Unresolved */
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			resolveClassRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
				goto done;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
				goto done;
			}
			goto retry;
		}
done:
		return rc;
	}

	/* ..., <0, 1 or 2 slot return value> => empty */
	VMINLINE VM_BytecodeAction
	genericReturn(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_literals);;
		UDATA isConstructor = FALSE;
		UDATA isObjectConstructor = FALSE;
		J9UTF8 *sig = NULL;
		UDATA sigLength = 0;
		U_8 *sigData = NULL;
		UDATA returnSlots = 0;
		UDATA *bp = bpForCurrentBytecodedMethod(REGISTER_ARGS);
		/* Check for synchronized */
		if (romMethod->modifiers & J9AccSynchronized) {
			IDATA monitorRC = exitObjectMonitor(REGISTER_ARGS, ((j9object_t*)bp)[1]);
			if (0 != monitorRC) {
				rc = THROW_ILLEGAL_MONITOR_STATE;
				goto done;
			}
			bp = bpForCurrentBytecodedMethod(REGISTER_ARGS);
		}
#if defined(DO_HOOKS)
		{
			bool hooked = J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_METHOD_RETURN);
			bool traced = VM_VMHelpers::methodBeingTraced(_vm, _literals);
			if (hooked || traced) {
				updateVMStruct(REGISTER_ARGS);
				if (traced) {
					UTSI_TRACEMETHODEXIT_FROMVM(_vm, _currentThread, _literals, NULL, _sp, 0);
				}
				if (hooked) {
					ALWAYS_TRIGGER_J9HOOK_VM_METHOD_RETURN(_vm->hookInterface, _currentThread, _literals, FALSE, _sp, 0);
				}
				VMStructHasBeenUpdated(REGISTER_ARGS);
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
				bp = bpForCurrentBytecodedMethod(REGISTER_ARGS);
			}
		}
		if (*bp & J9SF_A0_REPORT_FRAME_POP_TAG) {
			if (J9_EVENT_IS_HOOKED(_vm->hookInterface, J9HOOK_VM_FRAME_POP)) {
				updateVMStruct(REGISTER_ARGS);
				ALWAYS_TRIGGER_J9HOOK_VM_FRAME_POP(_vm->hookInterface, _currentThread, _literals, FALSE);
				VMStructHasBeenUpdated(REGISTER_ARGS);
				if (immediateAsyncPending()) {
					rc = GOTO_ASYNC_CHECK;
					goto done;
				}
				bp = bpForCurrentBytecodedMethod(REGISTER_ARGS);
			}
			*bp &= ~(UDATA)J9SF_A0_REPORT_FRAME_POP_TAG;
		}
#endif
		/* Examine the method signature to determine the return type (number of return slots) */
		sig = J9ROMMETHOD_SIGNATURE(romMethod);
		sigLength = sig->length;
		sigData = sig->data;
		/* are we returning an array? */
		if ('[' == sigData[sigLength - 2]) {
			returnSlots = 1;
		} else {
			switch(sigData[sigLength - 1]) {
			case 'V': {
				/* Is this a constructor? */
				J9UTF8 *name = J9ROMMETHOD_NAME(romMethod);
				if ((strlen("<init>") == name->length) && ('<' == name->data[0])) {
					isConstructor = TRUE;
					VM_AtomicSupport::writeBarrier();
				}
				break;
			}
			case 'J':
			case 'D':
				returnSlots = 2;
				break;

			/* For byte, char, short, and bool returns we need to perform appropriate truncation */
			case 'B':
				returnSlots = 1;
				*(I_32*)_sp = (I_8)*(I_32*)_sp;
				break;
			case 'C':
				returnSlots = 1;
				*(I_32*)_sp = (U_16)*(I_32*)_sp;
				break;
			case 'S':
				returnSlots = 1;
				*(I_32*)_sp = (I_16)*(I_32*)_sp;
				break;
			case 'Z':
				returnSlots = 1;
				*(U_32*)_sp = 1 & *(U_32*)_sp;
				break;
			default:
				returnSlots = 1;
				break;
			}
		}
		if (J9ROMMETHOD_IS_OBJECT_CONSTRUCTOR(romMethod)) {
			j9object_t finalizeObject = ((j9object_t*)bp)[1];
			isObjectConstructor = TRUE;
			VM_VMHelpers::checkIfFinalizeObject(_currentThread, finalizeObject);
		}
		if (bp == _currentThread->j2iFrame) {
			J9SFJ2IFrame *j2iFrame = ((J9SFJ2IFrame*)(bp + 1)) - 1;
			/* Don't overwrite the breakpoint bytecode or the return from the Object constructor */
			if ((JBbreakpoint != *_pc) && (FALSE == isObjectConstructor)) {
				/* Is this a clean return? */
				if (returnSlots == (UDATA)(((UDATA*)j2iFrame) - _sp)) {
					U_8 newBytecode = JBreturn0;
					if (romMethod->modifiers & J9AccSynchronized) {
						newBytecode = JBsyncReturn0;
					} else if (isConstructor) {
						newBytecode = JBreturnFromConstructor;
					}
					*_pc = (newBytecode + (U_8)returnSlots);
				}
			}
			rc = j2iReturn(REGISTER_ARGS);
			goto done;
		} else {
			UDATA returnValue0 = _sp[0];
			UDATA returnValue1 = _sp[1];
			J9SFStackFrame *frame = ((J9SFStackFrame*)(bp + 1)) - 1;
			/* Don't overwrite the breakpoint bytecode or the return from the Object constructor */
			if ((JBbreakpoint != *_pc) && (FALSE == isObjectConstructor)) {
				/* Is this a clean return? */
				if (returnSlots == (UDATA)(((UDATA*)frame) - _sp)) {
					U_8 newBytecode = JBreturn0;
					if (romMethod->modifiers & J9AccSynchronized) {
						newBytecode = JBsyncReturn0;
					} else if (isConstructor) {
						newBytecode = JBreturnFromConstructor;
					}
					*_pc = (newBytecode + (U_8)returnSlots);
				}
			}
			/* Collapse the frame and copy the return value */
			_sp = _arg0EA;
			_literals = frame->savedCP;
			_pc = frame->savedPC + 3;
			_arg0EA = frame->savedA0;
			switch(returnSlots) {
			case 0:
				_sp += 1;
				break;
			case 1:
				*_sp = returnValue0;
				break;
			default: /* 2 */
				*_sp = returnValue1;
				*--_sp = returnValue0;
				break;
			}
		}
done:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	unimplementedBytecode(REGISTER_ARGS_LIST)
	{
		updateVMStruct(REGISTER_ARGS);
		Assert_VM_unreachable();
		return EXECUTE_BYTECODE;
	}

	VMINLINE VM_BytecodeAction
	invokedynamic(REGISTER_ARGS_LIST)
	{
#if defined(J9VM_OPT_METHOD_HANDLE)
retry:
		VM_BytecodeAction rc = GOTO_RUN_METHODHANDLE;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		j9object_t *callSite = ramConstantPool->ramClass->callSites + index;
		j9object_t volatile methodHandle = *callSite;
		if (J9_EXPECTED(NULL != methodHandle)) {
			/* Copy the stack up to create a free slot for the receiver.  Write the MH into that slot */
			j9object_t currentType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(_currentThread, methodHandle);
			U_32 argSlots = (U_32)J9VMJAVALANGINVOKEMETHODTYPE_ARGSLOTS(_currentThread, currentType);
			_sp -= 1;
			memmove(_sp, _sp + 1, argSlots * sizeof(UDATA));
			((j9object_t *)_sp)[argSlots] = methodHandle;
			_currentThread->tempSlot = (UDATA) methodHandle;
		} else {
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			resolveInvokeDynamic(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
			} else {
				goto retry;
			}
		}
		return rc;
#else
	Assert_VM_unreachable();
	return EXECUTE_BYTECODE;
#endif /* J9VM_OPT_METHOD_HANDLE */
	}

	VMINLINE VM_BytecodeAction
	invokehandle(REGISTER_ARGS_LIST)
	{
#if defined(J9VM_OPT_METHOD_HANDLE)
retry:
		/* An invokevirtual MethodHandle.invokeExact() becomes invokehandle.
		 * The resolved MethodType and MethodHandle.type() must be equal or an exception will be thrown.
		 */
		VM_BytecodeAction rc = GOTO_RUN_METHODHANDLE;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMMethodRef *ramMethodRef = ((J9RAMMethodRef*)ramConstantPool) + index;
		UDATA volatile methodIndexAndArgCount = ramMethodRef->methodIndexAndArgCount;
		UDATA methodTypeIndex = ramMethodRef->methodIndexAndArgCount >> 8;
		j9object_t volatile type = J9_CLASS_FROM_CP(ramConstantPool)->methodTypes[methodTypeIndex];
		if (J9_EXPECTED(NULL != type)) {
			j9object_t mhReceiver = ((j9object_t*)_sp)[methodIndexAndArgCount & 0xFF];
			if (J9_UNEXPECTED(NULL == mhReceiver)) {
				rc = THROW_NPE;
				goto done;
			}
			/* MethodType check: require an exact match as MethodTypes are interned */
			if (J9_UNEXPECTED(J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(_currentThread, mhReceiver) != type)) {
				rc = THROW_WRONG_METHOD_TYPE;
				goto done;
			}
			_currentThread->tempSlot = (UDATA) mhReceiver;
		} else {
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			resolveVirtualMethodRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
			} else {
				goto retry;
			}
		}
done:
		return rc;
#else
	Assert_VM_unreachable();
	return EXECUTE_BYTECODE;
#endif /* J9VM_OPT_METHOD_HANDLE */
	}

	VMINLINE VM_BytecodeAction
	invokehandlegeneric(REGISTER_ARGS_LIST)
	{
#if defined(J9VM_OPT_METHOD_HANDLE)
retry:
		VM_BytecodeAction rc = GOTO_RUN_METHODHANDLE;
		U_16 index = *(U_16*)(_pc + 1);
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		J9RAMMethodRef *ramMethodRef = ((J9RAMMethodRef*)ramConstantPool) + index;
		UDATA volatile methodIndexAndArgCount = ramMethodRef->methodIndexAndArgCount;
		UDATA methodTypeIndex = ramMethodRef->methodIndexAndArgCount >> 8;
		j9object_t volatile type = J9_CLASS_FROM_CP(ramConstantPool)->methodTypes[methodTypeIndex];
		if (J9_EXPECTED(NULL != type)) {
			j9object_t mhReceiver = ((j9object_t*)_sp)[methodIndexAndArgCount & 0xFF];
			if (J9_UNEXPECTED(NULL == mhReceiver)) {
				rc = THROW_NPE;
				goto done;
			}
			/* MethodType check: require an exact match as MethodTypes are interned */
			if (J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(_currentThread, mhReceiver) != type) {
				/* Check if we can reuse the cached MethodHandle */
				j9object_t const cachedHandle = J9VMJAVALANGINVOKEMETHODHANDLE_PREVIOUSASTYPE(_currentThread, mhReceiver);
				j9object_t cachedHandleType = NULL;
				if (cachedHandle != NULL) {
					cachedHandleType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(_currentThread, cachedHandle);
				}
				if (type == cachedHandleType) {
					mhReceiver = cachedHandle;
				} else {
					buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
					updateVMStruct(REGISTER_ARGS);
					sendForGenericInvoke (_currentThread, mhReceiver, type, FALSE /* dropFirstArg */, 0 /* reserved */);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					mhReceiver = (j9object_t) _currentThread->returnValue;
					if (VM_VMHelpers::exceptionPending(_currentThread)) {
						rc = GOTO_THROW_CURRENT_EXCEPTION;
						goto done;
					}
					restoreGenericSpecialStackFrame(REGISTER_ARGS);
				}
				((j9object_t*)_sp)[ramMethodRef->methodIndexAndArgCount & 0xFF] = mhReceiver;
			}
			_currentThread->tempSlot = (UDATA) mhReceiver;
		} else {
			buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
			updateVMStruct(REGISTER_ARGS);
			resolveVirtualMethodRef(_currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			restoreGenericSpecialStackFrame(REGISTER_ARGS);
			if (immediateAsyncPending()) {
				rc = GOTO_ASYNC_CHECK;
			} else if (VM_VMHelpers::exceptionPending(_currentThread)) {
				rc = GOTO_THROW_CURRENT_EXCEPTION;
			} else {
				goto retry;
			}
		}
done:
		return rc;
#else
	Assert_VM_unreachable();
	return EXECUTE_BYTECODE;
#endif /* J9VM_OPT_METHOD_HANDLE */
	}

	VMINLINE j9object_t
	countAndCompileMethodHandle(REGISTER_ARGS_LIST, j9object_t methodHandle, void **compiledEntryPoint)
	{
#if defined(J9VM_OPT_METHOD_HANDLE)
		J9JITConfig *jitConfig = _vm->jitConfig;
		if (NULL != jitConfig) {
			j9object_t thunks = J9VMJAVALANGINVOKEMETHODHANDLE_THUNKS(_currentThread, methodHandle);
			bool isUpdated = false;
			I_32 count = 0;
			do {
				count = J9VMJAVALANGINVOKETHUNKTUPLE_INVOCATIONCOUNT(_currentThread, thunks);
				isUpdated = _objectAccessBarrier.inlineMixedObjectCompareAndSwapU32(
					_currentThread,
					thunks,
					J9VMJAVALANGINVOKETHUNKTUPLE_INVOCATIONCOUNT_OFFSET(_currentThread),
					count,
					count + 1);
			} while(!isUpdated);

			if (count == (IDATA)_vm->methodHandleCompileCount) {
				j9object_t currentType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(_currentThread, methodHandle);
				U_32 argSlots = (U_32)J9VMJAVALANGINVOKEMETHODTYPE_ARGSLOTS(_currentThread, currentType);
				UDATA *_spPriorToFrameBuild = _sp;
				updateVMStruct(REGISTER_ARGS);
				J9SFMethodTypeFrame *frame = buildMethodTypeFrame(_currentThread, currentType);
				*compiledEntryPoint = jitConfig->translateMethodHandle(_currentThread, methodHandle, NULL, J9_METHOD_HANDLE_COMPILE_SHARED);
				restoreMethodTypeFrame(REGISTER_ARGS, frame, _spPriorToFrameBuild);
				/* Refetch MH from the stack as a GC may have occurred while compiling */
				methodHandle = ((j9object_t *)_sp)[argSlots];
			}
		}
		return methodHandle;
#else
	Assert_VM_unreachable();
	return NULL;
#endif /* J9VM_OPT_METHOD_HANDLE */
	}

	VMINLINE VM_BytecodeAction
	retFromNative0(REGISTER_ARGS_LIST)
	{
		return retFromNativeHelper(REGISTER_ARGS, 0, _vm->jitConfig->jitExitInterpreter0);
	}

	VMINLINE VM_BytecodeAction
	invokevarhandle(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = GOTO_RUN_METHODHANDLE;

		/* Determine stack shape */
		J9ConstantPool *ramConstantPool = J9_CP_FROM_METHOD(_literals);
		U_16 cpIndex = *(U_16*)(_pc + 1);
		J9RAMMethodRef *ramMethodRef = ((J9RAMMethodRef*)ramConstantPool) + cpIndex;
		UDATA volatile methodArgCount = ramMethodRef->methodIndexAndArgCount & 0xFF;

		/* Find the VarHandle receiver on the stack */
		j9object_t varHandle = ((j9object_t*)_sp)[methodArgCount];
		UDATA operation = J9_VH_DECODE_ACCESS_MODE(_sendMethod->extra);

		if (J9_UNEXPECTED(NULL == varHandle)) {
			rc = THROW_NPE;
		} else {
			/* Get MethodHandle for this operation from the VarHandles handleTable */
			j9object_t handleTable = J9VMJAVALANGINVOKEVARHANDLE_HANDLETABLE(_currentThread, varHandle);
			j9object_t methodHandle = J9JAVAARRAYOFOBJECT_LOAD(_currentThread, handleTable, operation);
			j9object_t handleType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(_currentThread, methodHandle);

			/* Get call site MethodType */
			j9object_t volatile callSiteType = NULL;
			J9Class *ramClass = J9_CLASS_FROM_CP(ramConstantPool);
			J9ROMClass *romClass = ramClass->romClass;
			U_32 index = VM_VMHelpers::lookupVarHandleMethodTypeCacheIndex(romClass, cpIndex);
			callSiteType = ramClass->varHandleMethodTypes[index];

			if (callSiteType != handleType) {
				/* Generic invoke */
				j9object_t const cachedHandle = J9VMJAVALANGINVOKEMETHODHANDLE_PREVIOUSASTYPE(_currentThread, methodHandle);
				j9object_t cachedHandleType = NULL;
				if (cachedHandle != NULL) {
					cachedHandleType = J9VMJAVALANGINVOKEMETHODHANDLE_TYPE(_currentThread, cachedHandle);
				}
				if (cachedHandleType == callSiteType) {
					/* The cached AsTypeHandle was applicable to this call site. Use the cache. */
					methodHandle = cachedHandle;
				} else {
					/* Create generic invocation handle */
					buildGenericSpecialStackFrame(REGISTER_ARGS, 0);
					pushObjectInSpecialFrame(REGISTER_ARGS, varHandle);
					updateVMStruct(REGISTER_ARGS);
					sendForGenericInvoke(_currentThread, methodHandle, callSiteType, FALSE /* dropFirstArg */, 0 /* reserved */);
					VMStructHasBeenUpdated(REGISTER_ARGS);
					varHandle = popObjectInSpecialFrame(REGISTER_ARGS);
					restoreGenericSpecialStackFrame(REGISTER_ARGS);
					methodHandle = (j9object_t) _currentThread->returnValue;
					if (NULL == methodHandle) {
						rc = GOTO_THROW_CURRENT_EXCEPTION;
						goto done;
					}
				}
			}

			/* Replace VarHandle receiver on the stack with the MethodHandle receiver for this operation */
			((j9object_t*)_sp)[methodArgCount] = methodHandle;

			/* Place the VarHandle on top of the stack */
			_sp -= 1;
			((j9object_t*)_sp)[0] = varHandle;

			/* MethodHandle interpretation reads MethodHandle from _currentThread->tempSlot */
			_currentThread->tempSlot = (UDATA)methodHandle;
		}
done:
		return rc;
	}

	VMINLINE VM_BytecodeAction
	retFromNative1(REGISTER_ARGS_LIST)
	{
		return retFromNativeHelper(REGISTER_ARGS, 1, _vm->jitConfig->jitExitInterpreter1);
	}

	VMINLINE VM_BytecodeAction
	retFromNativeF(REGISTER_ARGS_LIST)
	{
		return retFromNativeHelper(REGISTER_ARGS, 1, _vm->jitConfig->jitExitInterpreterF);
	}

	VMINLINE VM_BytecodeAction
	retFromNativeD(REGISTER_ARGS_LIST)
	{
		return retFromNativeHelper(REGISTER_ARGS, 2, _vm->jitConfig->jitExitInterpreterD);
	}

	VMINLINE VM_BytecodeAction
	retFromNativeJ(REGISTER_ARGS_LIST)
	{
		return retFromNativeHelper(REGISTER_ARGS, 2, _vm->jitConfig->jitExitInterpreterJ);
	}

	VMINLINE VM_BytecodeAction
	impdep1(REGISTER_ARGS_LIST)
	{
		updateVMStruct(REGISTER_ARGS);
		VM_MHInterpreter mhInterpreter(_currentThread, &_objectAllocate, &_objectAccessBarrier);
		VM_BytecodeAction next = mhInterpreter.impdep1();
		VMStructHasBeenUpdated(REGISTER_ARGS);
		return next;
	}

	VMINLINE VM_BytecodeAction
	impdep2(REGISTER_ARGS_LIST)
	{
		_nextAction = J9_BCLOOP_EXIT_INTERPRETER;
		return GOTO_DONE;
	}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	VMINLINE VM_BytecodeAction
	defaultvalue(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		// TODO: Implement bytecode
		return rc;
	}

	VMINLINE VM_BytecodeAction
	withfield(REGISTER_ARGS_LIST)
	{
		VM_BytecodeAction rc = EXECUTE_BYTECODE;
		// TODO: Implement bytecode
		return rc;
	}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

protected:

public:

#if defined(J9VM_ARCH_X86) && defined(__GNUC__) && ((__GNUC__> 4) || (__GNUC__ == 4 && __GNUC_MINOR__ > 6))
	/*
	 * This method can't be 'VMINLINE' because it declares local static data
	 * triggering a warning with GCC compilers newer than 4.6.
	 */
#else
	VMINLINE
#endif
	UDATA
	run(J9VMThread *vmThread)
	{
		void *actionData = (void *)vmThread->returnValue2;
#if defined(TRACE_TRANSITIONS)
		char currentMethodName[1024];
		char nextMethodName[1024];
		PORT_ACCESS_FROM_VMC(vmThread);
#endif /* defined(TRACE_TRANSITIONS) */
#if defined(LOCAL_CURRENT_THREAD)
		J9VMThread * const _currentThread = vmThread;
#endif
#if defined(LOCAL_ARG0EA)
		UDATA *_arg0EA = vmThread->arg0EA;
#endif
#if defined(LOCAL_SP)
		UDATA *_sp = vmThread->sp;
#endif
#if defined(LOCAL_PC)
		U_8 *_pc = vmThread->pc;
#endif
#if defined(LOCAL_LITERALS)
		J9Method *_literals = vmThread->literals;
#endif

		DEBUG_MUST_HAVE_VM_ACCESS(vmThread);

#if defined(COUNT_BYTECODE_PAIRS)
		U_8 previousBytecode = JBinvokedynamic;
#define RECORD_BYTECODE_PAIR(number) \
		do { \
			UDATA *matrix = (UDATA *)interpreter._vm->debugField1; \
			U_8 currentBytecode = (number); \
			matrix[(previousBytecode * 256) + currentBytecode] += 1; \
			previousBytecode = currentBytecode; \
		} while (0)
#else /* COUNT_BYTECODE_PAIRS */
#define RECORD_BYTECODE_PAIR(number)
#endif /* COUNT_BYTECODE_PAIRS */

#if !defined(USE_COMPUTED_GOTO)
#if defined(DEBUG_VERSION)
		U_8 bytecode = 0;
#endif /* defined(DEBUG_VERSION) */
#define JUMP_TARGET(name) case name
#define EXECUTE_CURRENT_BYTECODE() goto executeCurrentBytecode
#define EXECUTE_BYTECODE_NUMBER(number) \
		do { \
			bytecode = (number); \
			goto executeBytecodeFromLocal; \
		} while (0)
#else /* !defined(USE_COMPUTED_GOTO) */
#define JUMP_TARGET(name) c##name
#define JUMP_TABLE_TYPE const void * const
#define JUMP_TABLE_ENTRY(name) &&c##name
#define EXECUTE_BYTECODE_NUMBER(number) \
		do { \
			RECORD_BYTECODE_PAIR(number); \
			goto *(bytecodeTable[number]); \
		} while (0)
#define EXECUTE_SEND_TARGET(number) goto *(sendTargetTable[number])
#define EXECUTE_CURRENT_BYTECODE() EXECUTE_BYTECODE_NUMBER(*_pc)
	static JUMP_TABLE_TYPE bytecodeTable[] = {
		JUMP_TABLE_ENTRY(JBnop),
		JUMP_TABLE_ENTRY(JBaconstnull),
		JUMP_TABLE_ENTRY(JBiconstm1),
		JUMP_TABLE_ENTRY(JBiconst0),
		JUMP_TABLE_ENTRY(JBiconst1),
		JUMP_TABLE_ENTRY(JBiconst2),
		JUMP_TABLE_ENTRY(JBiconst3),
		JUMP_TABLE_ENTRY(JBiconst4),
		JUMP_TABLE_ENTRY(JBiconst5),
		JUMP_TABLE_ENTRY(JBlconst0),
		JUMP_TABLE_ENTRY(JBlconst1),
		JUMP_TABLE_ENTRY(JBfconst0),
		JUMP_TABLE_ENTRY(JBfconst1),
		JUMP_TABLE_ENTRY(JBfconst2),
		JUMP_TABLE_ENTRY(JBdconst0),
		JUMP_TABLE_ENTRY(JBdconst1),
		JUMP_TABLE_ENTRY(JBbipush),
		JUMP_TABLE_ENTRY(JBsipush),
		JUMP_TABLE_ENTRY(JBldc),
		JUMP_TABLE_ENTRY(JBldcw),
		JUMP_TABLE_ENTRY(JBldc2lw),
		JUMP_TABLE_ENTRY(JBiload),
		JUMP_TABLE_ENTRY(JBlload),
		JUMP_TABLE_ENTRY(JBfload),
		JUMP_TABLE_ENTRY(JBdload),
		JUMP_TABLE_ENTRY(JBaload),
		JUMP_TABLE_ENTRY(JBiload0),
		JUMP_TABLE_ENTRY(JBiload1),
		JUMP_TABLE_ENTRY(JBiload2),
		JUMP_TABLE_ENTRY(JBiload3),
		JUMP_TABLE_ENTRY(JBlload0),
		JUMP_TABLE_ENTRY(JBlload1),
		JUMP_TABLE_ENTRY(JBlload2),
		JUMP_TABLE_ENTRY(JBlload3),
		JUMP_TABLE_ENTRY(JBfload0),
		JUMP_TABLE_ENTRY(JBfload1),
		JUMP_TABLE_ENTRY(JBfload2),
		JUMP_TABLE_ENTRY(JBfload3),
		JUMP_TABLE_ENTRY(JBdload0),
		JUMP_TABLE_ENTRY(JBdload1),
		JUMP_TABLE_ENTRY(JBdload2),
		JUMP_TABLE_ENTRY(JBdload3),
		JUMP_TABLE_ENTRY(JBaload0),
		JUMP_TABLE_ENTRY(JBaload1),
		JUMP_TABLE_ENTRY(JBaload2),
		JUMP_TABLE_ENTRY(JBaload3),
		JUMP_TABLE_ENTRY(JBiaload),
		JUMP_TABLE_ENTRY(JBlaload),
		JUMP_TABLE_ENTRY(JBfaload),
		JUMP_TABLE_ENTRY(JBdaload),
		JUMP_TABLE_ENTRY(JBaaload),
		JUMP_TABLE_ENTRY(JBbaload),
		JUMP_TABLE_ENTRY(JBcaload),
		JUMP_TABLE_ENTRY(JBsaload),
		JUMP_TABLE_ENTRY(JBistore),
		JUMP_TABLE_ENTRY(JBlstore),
		JUMP_TABLE_ENTRY(JBfstore),
		JUMP_TABLE_ENTRY(JBdstore),
		JUMP_TABLE_ENTRY(JBastore),
		JUMP_TABLE_ENTRY(JBistore0),
		JUMP_TABLE_ENTRY(JBistore1),
		JUMP_TABLE_ENTRY(JBistore2),
		JUMP_TABLE_ENTRY(JBistore3),
		JUMP_TABLE_ENTRY(JBlstore0),
		JUMP_TABLE_ENTRY(JBlstore1),
		JUMP_TABLE_ENTRY(JBlstore2),
		JUMP_TABLE_ENTRY(JBlstore3),
		JUMP_TABLE_ENTRY(JBfstore0),
		JUMP_TABLE_ENTRY(JBfstore1),
		JUMP_TABLE_ENTRY(JBfstore2),
		JUMP_TABLE_ENTRY(JBfstore3),
		JUMP_TABLE_ENTRY(JBdstore0),
		JUMP_TABLE_ENTRY(JBdstore1),
		JUMP_TABLE_ENTRY(JBdstore2),
		JUMP_TABLE_ENTRY(JBdstore3),
		JUMP_TABLE_ENTRY(JBastore0),
		JUMP_TABLE_ENTRY(JBastore1),
		JUMP_TABLE_ENTRY(JBastore2),
		JUMP_TABLE_ENTRY(JBastore3),
		JUMP_TABLE_ENTRY(JBiastore),
		JUMP_TABLE_ENTRY(JBlastore),
		JUMP_TABLE_ENTRY(JBfastore),
		JUMP_TABLE_ENTRY(JBdastore),
		JUMP_TABLE_ENTRY(JBaastore),
		JUMP_TABLE_ENTRY(JBbastore),
		JUMP_TABLE_ENTRY(JBcastore),
		JUMP_TABLE_ENTRY(JBsastore),
		JUMP_TABLE_ENTRY(JBpop),
		JUMP_TABLE_ENTRY(JBpop2),
		JUMP_TABLE_ENTRY(JBdup),
		JUMP_TABLE_ENTRY(JBdupx1),
		JUMP_TABLE_ENTRY(JBdupx2),
		JUMP_TABLE_ENTRY(JBdup2),
		JUMP_TABLE_ENTRY(JBdup2x1),
		JUMP_TABLE_ENTRY(JBdup2x2),
		JUMP_TABLE_ENTRY(JBswap),
		JUMP_TABLE_ENTRY(JBiadd),
		JUMP_TABLE_ENTRY(JBladd),
		JUMP_TABLE_ENTRY(JBfadd),
		JUMP_TABLE_ENTRY(JBdadd),
		JUMP_TABLE_ENTRY(JBisub),
		JUMP_TABLE_ENTRY(JBlsub),
		JUMP_TABLE_ENTRY(JBfsub),
		JUMP_TABLE_ENTRY(JBdsub),
		JUMP_TABLE_ENTRY(JBimul),
		JUMP_TABLE_ENTRY(JBlmul),
		JUMP_TABLE_ENTRY(JBfmul),
		JUMP_TABLE_ENTRY(JBdmul),
		JUMP_TABLE_ENTRY(JBidiv),
		JUMP_TABLE_ENTRY(JBldiv),
		JUMP_TABLE_ENTRY(JBfdiv),
		JUMP_TABLE_ENTRY(JBddiv),
		JUMP_TABLE_ENTRY(JBirem),
		JUMP_TABLE_ENTRY(JBlrem),
		JUMP_TABLE_ENTRY(JBfrem),
		JUMP_TABLE_ENTRY(JBdrem),
		JUMP_TABLE_ENTRY(JBineg),
		JUMP_TABLE_ENTRY(JBlneg),
		JUMP_TABLE_ENTRY(JBfneg),
		JUMP_TABLE_ENTRY(JBdneg),
		JUMP_TABLE_ENTRY(JBishl),
		JUMP_TABLE_ENTRY(JBlshl),
		JUMP_TABLE_ENTRY(JBishr),
		JUMP_TABLE_ENTRY(JBlshr),
		JUMP_TABLE_ENTRY(JBiushr),
		JUMP_TABLE_ENTRY(JBlushr),
		JUMP_TABLE_ENTRY(JBiand),
		JUMP_TABLE_ENTRY(JBland),
		JUMP_TABLE_ENTRY(JBior),
		JUMP_TABLE_ENTRY(JBlor),
		JUMP_TABLE_ENTRY(JBixor),
		JUMP_TABLE_ENTRY(JBlxor),
		JUMP_TABLE_ENTRY(JBiinc),
		JUMP_TABLE_ENTRY(JBi2l),
		JUMP_TABLE_ENTRY(JBi2f),
		JUMP_TABLE_ENTRY(JBi2d),
		JUMP_TABLE_ENTRY(JBl2i),
		JUMP_TABLE_ENTRY(JBl2f),
		JUMP_TABLE_ENTRY(JBl2d),
		JUMP_TABLE_ENTRY(JBf2i),
		JUMP_TABLE_ENTRY(JBf2l),
		JUMP_TABLE_ENTRY(JBf2d),
		JUMP_TABLE_ENTRY(JBd2i),
		JUMP_TABLE_ENTRY(JBd2l),
		JUMP_TABLE_ENTRY(JBd2f),
		JUMP_TABLE_ENTRY(JBi2b),
		JUMP_TABLE_ENTRY(JBi2c),
		JUMP_TABLE_ENTRY(JBi2s),
		JUMP_TABLE_ENTRY(JBlcmp),
		JUMP_TABLE_ENTRY(JBfcmpl),
		JUMP_TABLE_ENTRY(JBfcmpg),
		JUMP_TABLE_ENTRY(JBdcmpl),
		JUMP_TABLE_ENTRY(JBdcmpg),
		JUMP_TABLE_ENTRY(JBifeq),
		JUMP_TABLE_ENTRY(JBifne),
		JUMP_TABLE_ENTRY(JBiflt),
		JUMP_TABLE_ENTRY(JBifge),
		JUMP_TABLE_ENTRY(JBifgt),
		JUMP_TABLE_ENTRY(JBifle),
		JUMP_TABLE_ENTRY(JBificmpeq),
		JUMP_TABLE_ENTRY(JBificmpne),
		JUMP_TABLE_ENTRY(JBificmplt),
		JUMP_TABLE_ENTRY(JBificmpge),
		JUMP_TABLE_ENTRY(JBificmpgt),
		JUMP_TABLE_ENTRY(JBificmple),
		JUMP_TABLE_ENTRY(JBifacmpeq),
		JUMP_TABLE_ENTRY(JBifacmpne),
		JUMP_TABLE_ENTRY(JBgoto),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBtableswitch),
		JUMP_TABLE_ENTRY(JBlookupswitch),
		JUMP_TABLE_ENTRY(JBreturn0),
		JUMP_TABLE_ENTRY(JBreturn1),
		JUMP_TABLE_ENTRY(JBreturn2),
		JUMP_TABLE_ENTRY(JBsyncReturn0),
		JUMP_TABLE_ENTRY(JBsyncReturn1),
		JUMP_TABLE_ENTRY(JBsyncReturn2),
		JUMP_TABLE_ENTRY(JBgetstatic),
		JUMP_TABLE_ENTRY(JBputstatic),
		JUMP_TABLE_ENTRY(JBgetfield),
		JUMP_TABLE_ENTRY(JBputfield),
		JUMP_TABLE_ENTRY(JBinvokevirtual),
		JUMP_TABLE_ENTRY(JBinvokespecial),
		JUMP_TABLE_ENTRY(JBinvokestatic),
		JUMP_TABLE_ENTRY(JBinvokeinterface),
		JUMP_TABLE_ENTRY(JBinvokedynamic),
		JUMP_TABLE_ENTRY(JBnew),
		JUMP_TABLE_ENTRY(JBnewarray),
		JUMP_TABLE_ENTRY(JBanewarray),
		JUMP_TABLE_ENTRY(JBarraylength),
		JUMP_TABLE_ENTRY(JBathrow),
		JUMP_TABLE_ENTRY(JBcheckcast),
		JUMP_TABLE_ENTRY(JBinstanceof),
		JUMP_TABLE_ENTRY(JBmonitorenter),
		JUMP_TABLE_ENTRY(JBmonitorexit),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBmultianewarray),
		JUMP_TABLE_ENTRY(JBifnull),
		JUMP_TABLE_ENTRY(JBifnonnull),
		JUMP_TABLE_ENTRY(JBgotow),
		JUMP_TABLE_ENTRY(JBunimplemented),
#if defined(DEBUG_VERSION)
		JUMP_TABLE_ENTRY(JBbreakpoint),
#else /* DEBUG_VERSION */
		JUMP_TABLE_ENTRY(JBunimplemented),
#endif /* DEBUG_VERSION */
		JUMP_TABLE_ENTRY(JBiloadw),
		JUMP_TABLE_ENTRY(JBlloadw),
		JUMP_TABLE_ENTRY(JBfloadw),
		JUMP_TABLE_ENTRY(JBdloadw),
		JUMP_TABLE_ENTRY(JBaloadw),
		JUMP_TABLE_ENTRY(JBistorew),
		JUMP_TABLE_ENTRY(JBlstorew),
		JUMP_TABLE_ENTRY(JBfstorew),
		JUMP_TABLE_ENTRY(JBdstorew),
		JUMP_TABLE_ENTRY(JBastorew),
		JUMP_TABLE_ENTRY(JBiincw),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBaload0getfield),
		JUMP_TABLE_ENTRY(JBnewdup),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		JUMP_TABLE_ENTRY(JBdefaultvalue),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBwithfield),
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBreturnFromConstructor),
		JUMP_TABLE_ENTRY(JBgenericReturn),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBinvokeinterface2),
		JUMP_TABLE_ENTRY(JBinvokehandle),
		JUMP_TABLE_ENTRY(JBinvokehandlegeneric),
		JUMP_TABLE_ENTRY(JBinvokestaticsplit),
		JUMP_TABLE_ENTRY(JBinvokespecialsplit),
		JUMP_TABLE_ENTRY(JBreturnC),
		JUMP_TABLE_ENTRY(JBreturnS),
		JUMP_TABLE_ENTRY(JBreturnB),
		JUMP_TABLE_ENTRY(JBreturnZ),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBretFromNative0),
		JUMP_TABLE_ENTRY(JBretFromNative1),
		JUMP_TABLE_ENTRY(JBretFromNativeF),
		JUMP_TABLE_ENTRY(JBretFromNativeD),
		JUMP_TABLE_ENTRY(JBretFromNativeJ),
		JUMP_TABLE_ENTRY(JBldc2dw),
		JUMP_TABLE_ENTRY(JBasyncCheck),
		JUMP_TABLE_ENTRY(JBreturnFromJ2I),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBunimplemented),
		JUMP_TABLE_ENTRY(JBimpdep1),
		JUMP_TABLE_ENTRY(JBimpdep2),
	};
	static JUMP_TABLE_TYPE sendTargetTable[] = {
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INITIAL_STATIC),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INITIAL_SPECIAL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INITIAL_VIRTUAL),
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INVOKE_PRIVATE),
#endif /* J9VM_OPT_VALHALLA_NESTMATES */
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_UNSATISFIED_OR_ABSTRACT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_DEFAULT_CONFLICT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_COUNT_NON_SYNC),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_NON_SYNC),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_COUNT_SYNC),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_SYNC),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_COUNT_SYNC_STATIC),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_SYNC_STATIC),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_COUNT_OBJ_CTOR),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_OBJ_CTOR),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_COUNT_LARGE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_LARGE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_COUNT_EMPTY_OBJ_CTOR),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_EMPTY_OBJ_CTOR),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_COUNT_ZEROING),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_ZEROING),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_BIND_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_COUNT_AND_SEND_JNI_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_COMPILE_JNI_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_RUN_JNI_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_I2J_TRANSITION),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_OBJECT_GET_CLASS),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_ASSIGNABLE_FROM),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_ARRAY),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_PRIMITIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASS_GET_MODIFIERS_IMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASS_GET_COMPONENT_TYPE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_THREAD_CURRENT_THREAD),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_STRING_INTERN),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_SYSTEM_ARRAYCOPY),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_SYSTEM_CURRENT_TIME_MILLIS),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_SYSTEM_NANO_TIME),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_GET_SUPERCLASS),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_IDENTITY_HASH_CODE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_THROWABLE_FILL_IN_STACK_TRACE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_NEWINSTANCEIMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_PRIMITIVE_CLONE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_REFERENCE_GETIMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBYTE_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBYTE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBYTE_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBYTE_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBYTE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBYTE_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBOOLEAN),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBOOLEAN_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBOOLEAN),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBOOLEAN_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETSHORT_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETSHORT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETSHORT_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTSHORT_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTSHORT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTSHORT_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETCHAR_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETCHAR),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETCHAR_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTCHAR_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTCHAR),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTCHAR_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETINT_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETINT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETINT_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTINT_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTINT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTINT_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETFLOAT_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETFLOAT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETFLOAT_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTFLOAT_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTFLOAT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTFLOAT_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETLONG_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETLONG),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETLONG_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTLONG_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTLONG),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTLONG_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETDOUBLE_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETDOUBLE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETDOUBLE_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTDOUBLE_NATIVE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTDOUBLE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTDOUBLE_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETOBJECT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETOBJECT_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTOBJECT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTOBJECT_VOLATILE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETADDRESS),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTADDRESS),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ADDRESS_SIZE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ARRAY_BASE_OFFSET),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ARRAY_INDEX_SCALE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_LOAD_FENCE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_STORE_FENCE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_COMPAREANDSWAPOBJECT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_COMPAREANDSWAPLONG),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_COMPAREANDSWAPINT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_GET_INTERFACES),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_ARRAY_NEW_ARRAY_IMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASSLOADER_FIND_LOADED_CLASS_IMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_VM_FIND_CLASS_OR_NULL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_CLASS_FORNAMEIMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_THREAD_INTERRUPTED),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_VM_GET_CP_INDEX_IMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_VM_GET_STACK_CLASS_LOADER),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_OBJECT_NOTIFY_ALL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_OBJECT_NOTIFY),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_INSTANCE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASS_GET_SIMPLE_NAME_IMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_VM_INITIALIZE_CLASS_LOADER),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_VM_GET_CLASS_PATH_ENTRY_TYPE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_VM_IS_BOOTSTRAP_CLASS_LOADER),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ALLOCATE_INSTANCE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_PREPARE_CLASS_IMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_ATTACHMENT_LOADAGENTLIBRARYIMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_VM_GETSTACKCLASS),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_THREAD_SLEEP),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_OBJECT_WAIT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASSLOADER_LOADLIBRARYWITHPATH),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_THREAD_ISINTERRUPTEDIMPL),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_CLASSLOADER_INITIALIZEANONCLASSLOADER),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_VARHANDLE),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_INL_THREAD_ON_SPIN_WAIT),
		JUMP_TABLE_ENTRY(J9_BCLOOP_SEND_TARGET_OUT_OF_LINE_INL),
	};
#endif /* !defined(USE_COMPUTED_GOTO) */

/*
 * Important: To work around an XLC compiler bug, the cases in PERFORM_ACTION below
 * must be in the identical order to their declaration in BytecodeAction.hpp.
 */
#if defined(DEBUG_VERSION)
#define DEBUG_ACTIONS \
		case GOTO_EXECUTE_BREAKPOINTED_BYTECODE: \
			goto executeBreakpointedBytecode; \
		case REPORT_METHOD_ENTER: \
			goto methodEnter; \
		case HANDLE_POP_FRAMES: \
			goto popFrames; \
		case FALL_THROUGH: \
			break;
#else
#define DEBUG_ACTIONS
#endif

#define PERFORM_ACTION(functionCall) \
	do { \
		DEBUG_UPDATE_VMSTRUCT(); \
		DEBUG_MUST_HAVE_VM_ACCESS(_currentThread); \
		VM_BytecodeAction returnCode = functionCall; \
		switch (returnCode) { \
		case EXECUTE_BYTECODE: \
			EXECUTE_CURRENT_BYTECODE(); \
		case GOTO_RUN_METHOD: \
			goto runMethod; \
		case GOTO_NOUPDATE: \
			goto noUpdate; \
		case GOTO_DONE: \
			goto done; \
		case GOTO_BRANCH_WITH_ASYNC_CHECK: \
			goto branchWithAsyncCheck; \
		case GOTO_THROW_CURRENT_EXCEPTION: \
			goto throwCurrentException; \
		case GOTO_ASYNC_CHECK: \
			goto asyncCheck; \
		case GOTO_JAVA_STACK_OVERFLOW: \
			goto javaStackOverflow; \
		case GOTO_RUN_METHODHANDLE: \
			goto runMethodHandle; \
		case GOTO_RUN_METHODHANDLE_COMPILED: \
			goto runMethodHandleCompiled; \
		case GOTO_RUN_METHOD_FROM_METHOD_HANDLE: \
			goto runMethodFromMethodHandle; \
		case THROW_MONITOR_ALLOC_FAIL: \
			goto failedToAllocateMonitor; \
		case THROW_HEAP_OOM: \
			goto heapOOM; \
		case THROW_NEGATIVE_ARRAY_SIZE: \
			goto negativeArraySize; \
		case THROW_NPE: \
			goto nullPointer; \
		case THROW_AIOB: \
			goto arrayIndex; \
		case THROW_ARRAY_STORE: \
			goto arrayStoreException; \
		case THROW_DIVIDE_BY_ZERO: \
			goto divideByZero; \
		case THROW_ILLEGAL_MONITOR_STATE: \
			goto illegalMonitorState; \
		case THROW_INCOMPATIBLE_CLASS_CHANGE: \
			goto incompatibleClassChange; \
		case THROW_WRONG_METHOD_TYPE: \
			goto wrongMethodType; \
		case PERFORM_DLT: \
			goto dlt; \
		case RUN_METHOD_INTERPRETED: \
			goto runMethodInterpreted; \
		case RUN_JNI_NATIVE: \
			goto jni; \
		case RUN_METHOD_COMPILED: \
			goto i2j; \
		DEBUG_ACTIONS \
		default: \
			Assert_VM_unreachable(); \
		} \
	} while(0)

#if defined(DO_SINGLE_STEP)
#define SINGLE_STEP() PERFORM_ACTION(singleStep(REGISTER_ARGS))
#else
#define SINGLE_STEP()
#endif

	switch (vmThread->returnValue) {
	case J9_BCLOOP_RUN_METHOD:
		_sendMethod = (J9Method *)actionData;
#if defined(TRACE_TRANSITIONS)
		getMethodName(PORTLIB, _sendMethod, (U_8*)-1, currentMethodName);
		j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_RUN_METHOD %s\n", vmThread, currentMethodName);
#endif
		goto runMethod;
	case J9_BCLOOP_RUN_METHOD_INTERPRETED:
		_sendMethod = (J9Method *)actionData;
#if defined(TRACE_TRANSITIONS)
		getMethodName(PORTLIB, _sendMethod, (U_8*)-1, currentMethodName);
		j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_RUN_METHOD_INTERPRETED %s\n", vmThread, currentMethodName);
#endif
		goto runMethodInterpreted;
	case J9_BCLOOP_RUN_METHOD_HANDLE:
		/* Stash MethodHandle into tempSlot where MHInterpreter expects to find it */
		vmThread->tempSlot = (UDATA)actionData;
#if defined(TRACE_TRANSITIONS)
		getMethodName(PORTLIB, _literals, _pc, currentMethodName);
		j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_RUN_METHOD_HANDLE %s\n", vmThread, currentMethodName);
#endif
		goto runMethodHandle;
	case J9_BCLOOP_EXECUTE_BYTECODE:
#if defined(TRACE_TRANSITIONS)
		getMethodName(PORTLIB, _literals, _pc, currentMethodName);
		j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_EXECUTE_BYTECODE %s\n", vmThread, currentMethodName);
#endif
		EXECUTE_CURRENT_BYTECODE();
#if defined(DEBUG_VERSION)
	case J9_BCLOOP_HANDLE_POP_FRAMES:
#if defined(TRACE_TRANSITIONS)
		j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_HANDLE_POP_FRAMES\n", vmThread);
#endif
		goto popFrames;
#endif
	case J9_BCLOOP_THROW_CURRENT_EXCEPTION:
#if defined(TRACE_TRANSITIONS)
		j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_THROW_CURRENT_EXCEPTION exception=%p\n", vmThread, vmThread->currentException);
#endif
		goto throwCurrentException;
	case J9_BCLOOP_CHECK_ASYNC:
		goto asyncCheck;
	case J9_BCLOOP_J2I_VIRTUAL: {
		j9object_t receiver = (j9object_t)actionData;
		UDATA interfaceVTableIndex = vmThread->tempSlot;
		actionData = (void*)j2iVirtualMethod(REGISTER_ARGS, receiver, interfaceVTableIndex);
		// intentional fall-through
	}
	case J9_BCLOOP_J2I_TRANSITION:
		_sendMethod = (J9Method *)actionData;
#if defined(TRACE_TRANSITIONS)
		getMethodName(PORTLIB, _sendMethod, (U_8*)-1, currentMethodName);
		j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_J2I_TRANSITION %s\n", vmThread, currentMethodName);
#endif
		PERFORM_ACTION(j2iTransition(REGISTER_ARGS));
	case J9_BCLOOP_J2I_INVOKE_EXACT: {
		j9object_t methodHandle = (j9object_t)actionData;
#if defined(TRACE_TRANSITIONS)
		j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_J2I_INVOKE_EXACT methodHandle=%p\n", vmThread, methodHandle);
#endif
		PERFORM_ACTION(j2iInvokeExact(REGISTER_ARGS, methodHandle));
	}
	case J9_BCLOOP_I2J_TRANSITION:
		_sendMethod = (J9Method *)actionData;
#if defined(TRACE_TRANSITIONS)
		getMethodName(PORTLIB, _sendMethod, (U_8*)-1, currentMethodName);
		j9tty_printf(PORTLIB, "<%p> enter: J9_BCLOOP_I2J_TRANSITION %s\n", vmThread, currentMethodName);
#endif
		goto i2j;
	case J9_BCLOOP_RETURN_FROM_JIT:
		PERFORM_ACTION(returnFromJIT(REGISTER_ARGS, (UDATA)actionData, false));
	case J9_BCLOOP_RETURN_FROM_JIT_CTOR:
		PERFORM_ACTION(returnFromJIT(REGISTER_ARGS, 0, true));
	case J9_BCLOOP_FILL_OSR_BUFFER:
		PERFORM_ACTION(fillOSRBuffer(REGISTER_ARGS, actionData));
	case J9_BCLOOP_EXIT_INTERPRETER:
		_nextAction = J9_BCLOOP_EXIT_INTERPRETER;
		goto done;
#if defined(DO_HOOKS)
	case J9_BCLOOP_ENTER_METHOD_MONITOR:
		_sendMethod = (J9Method *)actionData;
		PERFORM_ACTION(enterMethodMonitor(REGISTER_ARGS));
	case J9_BCLOOP_REPORT_METHOD_ENTER:
		_sendMethod = (J9Method *)actionData;
		goto methodEnter;
#endif /* DO_HOOKS */
	default:
#if defined(TRACE_TRANSITIONS)
		j9tty_printf(PORTLIB, "<%p> enter: UNKNOWN %d\n", vmThread, vmThread->returnValue);
#endif
		break;
	}
	Assert_VM_unreachable();

#if defined(DO_HOOKS)
methodEnter:
	PERFORM_ACTION(reportMethodEnter(REGISTER_ARGS));
#endif /* DO_HOOKS */

runMethodInterpreted:
	if (J9_ARE_ANY_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod)->modifiers, J9AccNative)) {
		goto jni;
	}
	/* intentional fall-through to targetLargeStack */
targetLargeStack: {
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(_sendMethod);
	U_32 modifiers = romMethod->modifiers;
	UDATA stackUse = VM_VMHelpers::calculateStackUse(romMethod);
	UDATA *checkSP = _sp - stackUse;
	if ((checkSP > _sp) || VM_VMHelpers::shouldGrowForSP(_currentThread, checkSP)) {
		Trc_VM_VMprCheckStackAndSend_overflowDetected(_currentThread, checkSP, _currentThread->stackOverflowMark2);
		if (J9_ARE_ANY_BITS_SET(_currentThread->privateFlags, J9_PRIVATE_FLAGS_STACK_OVERFLOW)) {
			Trc_VM_VMprCheckStackAndSend_recursiveOverflow(_currentThread);
		}
		checkSP -= (sizeof(J9SFMethodFrame) / sizeof(UDATA));
		UDATA currentUsed = (UDATA)_currentThread->stackObject->end - (UDATA)checkSP;
		UDATA maxStackSize = _vm->stackSize;
		buildMethodFrame(REGISTER_ARGS, _sendMethod, 0);
		updateVMStruct(REGISTER_ARGS);
		if (currentUsed > maxStackSize) {
throwStackOverflow:
			if (J9_ARE_ANY_BITS_SET(_currentThread->privateFlags, J9_PRIVATE_FLAGS_STACK_OVERFLOW)) {
				// vmStruct already up-to-date in all paths to here
				fatalRecursiveStackOverflow(_currentThread);
			}
			Trc_VM_VMprCheckStackAndSend_throwingError(_currentThread);
			setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGSTACKOVERFLOWERROR, NULL);
			VMStructHasBeenUpdated(REGISTER_ARGS);
			goto throwCurrentException;
		}
		currentUsed += _vm->stackSizeIncrement;
		if (currentUsed > maxStackSize) {
			currentUsed = maxStackSize;
		}
		if (0 != growJavaStack(_currentThread, currentUsed)) {
			goto throwStackOverflow;
		}
		Trc_VM_VMprCheckStackAndSend_succesfulGrowth(_currentThread);
		VMStructHasBeenUpdated(REGISTER_ARGS);
		UDATA *bp = ((UDATA*)(((J9SFMethodFrame*)_sp) + 1)) - 1;
		restoreSpecialStackFrameLeavingArgs(REGISTER_ARGS, bp);
	}
	if (_vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_CAN_ACCESS_LOCALS) {
		goto targetZeroing;
	}
	if (modifiers & J9AccMethodObjectConstructor) {
		goto targetObjectConstructor;
	}
	if (modifiers & J9AccSynchronized) {
		if (modifiers & J9AccStatic) {
			goto targetSyncStatic;
		}
		goto targetSync;
	}
	goto targetNonSync;
}

targetSync:
	PERFORM_ACTION(sendTargetSmallSync(REGISTER_ARGS));

targetNonSync:
	PERFORM_ACTION(sendTargetSmallNonSync(REGISTER_ARGS));

targetSyncStatic:
	PERFORM_ACTION(sendTargetSmallSyncStatic(REGISTER_ARGS));

targetObjectConstructor:
	PERFORM_ACTION(sendTargetSmallObjectConstructor(REGISTER_ARGS));

targetZeroing:
	PERFORM_ACTION(sendTargetSmallZeroing(REGISTER_ARGS));


#if defined(DEBUG_VERSION)
popFrames:
	PERFORM_ACTION(handlePopFramesInterrupt(REGISTER_ARGS));
#endif

dlt:
	PERFORM_ACTION(performDLT(REGISTER_ARGS));

runMethod: {
	void *methodRunAddress = _sendMethod->methodRunAddress;
#if defined(USE_COMPUTED_GOTO)
	EXECUTE_SEND_TARGET(J9_BCLOOP_DECODE_SEND_TARGET(methodRunAddress));
#else
	switch(J9_BCLOOP_DECODE_SEND_TARGET(methodRunAddress)) {
#endif

	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INITIAL_STATIC):
		PERFORM_ACTION(initialStaticMethod(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INITIAL_SPECIAL):
		PERFORM_ACTION(initialSpecialMethod(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INITIAL_VIRTUAL):
		PERFORM_ACTION(initialVirtualMethod(REGISTER_ARGS));
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INVOKE_PRIVATE):
		PERFORM_ACTION(invokePrivateMethod(REGISTER_ARGS));
#endif /* J9VM_OPT_VALHALLA_NESTMATES */
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_UNSATISFIED_OR_ABSTRACT):
		PERFORM_ACTION(throwUnsatisfiedLinkOrAbstractMethodError(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_DEFAULT_CONFLICT):
		PERFORM_ACTION(throwForDefaultConflict(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_NON_SYNC):
		if (countAndCompile(REGISTER_ARGS)) {
			goto i2j;
		}
		/* Intentional fall-through */
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_NON_SYNC):
		goto targetNonSync;
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_SYNC):
		if (countAndCompile(REGISTER_ARGS)) {
			goto i2j;
		}
		/* Intentional fall-through */
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_SYNC):
		goto targetSync;
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_SYNC_STATIC):
		if (countAndCompile(REGISTER_ARGS)) {
			goto i2j;
		}
		/* Intentional fall-through */
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_SYNC_STATIC):
		goto targetSyncStatic;
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_OBJ_CTOR):
		if (countAndCompile(REGISTER_ARGS)) {
			goto i2j;
		}
		/* Intentional fall-through */
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_OBJ_CTOR):
		goto targetObjectConstructor;
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_ZEROING):
		if (countAndCompile(REGISTER_ARGS)) {
			goto i2j;
		}
		/* Intentional fall-through */
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_ZEROING):
		goto targetZeroing;
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_EMPTY_OBJ_CTOR):
		if (countAndCompile(REGISTER_ARGS)) {
			goto i2j;
		}
		/* Intentional fall-through */
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_EMPTY_OBJ_CTOR):
		PERFORM_ACTION(sendTargetEmptyObjectConstructor(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_LARGE):
		if (countAndCompile(REGISTER_ARGS)) {
			goto i2j;
		}
		/* Intentional fall-through */
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_LARGE):
		goto targetLargeStack;
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_BIND_NATIVE):
		PERFORM_ACTION(bindNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_COUNT_AND_SEND_JNI_NATIVE):
		PERFORM_ACTION(countAndSendJNINative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_COMPILE_JNI_NATIVE):
		PERFORM_ACTION(compileJNINative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_RUN_JNI_NATIVE):
		goto jni;
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_I2J_TRANSITION):
		goto i2j;
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_THREAD_CURRENT_THREAD):
		PERFORM_ACTION(inlThreadCurrentThread(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_OBJECT_GET_CLASS):
		PERFORM_ACTION(inlObjectGetClass(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_ASSIGNABLE_FROM):
		PERFORM_ACTION(inlClassIsAssignableFrom(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_ARRAY):
		PERFORM_ACTION(inlClassIsArray(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_PRIMITIVE):
		PERFORM_ACTION(inlClassIsPrimitive(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASS_GET_MODIFIERS_IMPL):
		PERFORM_ACTION(inlClassGetModifiersImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASS_GET_COMPONENT_TYPE):
		PERFORM_ACTION(inlClassGetComponentType(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_SYSTEM_ARRAYCOPY):
		PERFORM_ACTION(inlSystemArraycopy(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_SYSTEM_CURRENT_TIME_MILLIS):
		PERFORM_ACTION(inlSystemCurrentTimeMillis(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_SYSTEM_NANO_TIME):
		PERFORM_ACTION(inlSystemNanoTime(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_GET_SUPERCLASS):
		PERFORM_ACTION(inlInternalsGetSuperclass(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_IDENTITY_HASH_CODE):
		PERFORM_ACTION(inlInternalsIdentityHashCode(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_STRING_INTERN):
		PERFORM_ACTION(inlStringIntern(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_THROWABLE_FILL_IN_STACK_TRACE):
		PERFORM_ACTION(inlThrowableFillInStackTrace(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_NEWINSTANCEIMPL):
		PERFORM_ACTION(inlInternalsNewInstanceImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_PRIMITIVE_CLONE):
		PERFORM_ACTION(inlInternalsPrimitiveClone(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_REFERENCE_GETIMPL):
		PERFORM_ACTION(inlReferenceGetImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBYTE_NATIVE):
		PERFORM_ACTION(inlUnsafeGetByteNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBYTE):
		PERFORM_ACTION(inlUnsafeGetByte(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBYTE_VOLATILE):
		PERFORM_ACTION(inlUnsafeGetByte(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBYTE_NATIVE):
		PERFORM_ACTION(inlUnsafePutByteNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBYTE):
		PERFORM_ACTION(inlUnsafePutByte(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBYTE_VOLATILE):
		PERFORM_ACTION(inlUnsafePutByte(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBOOLEAN):
		PERFORM_ACTION(inlUnsafeGetBoolean(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBOOLEAN_VOLATILE):
		PERFORM_ACTION(inlUnsafeGetBoolean(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBOOLEAN):
		PERFORM_ACTION(inlUnsafePutBoolean(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBOOLEAN_VOLATILE):
		PERFORM_ACTION(inlUnsafePutBoolean(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETSHORT_NATIVE):
		PERFORM_ACTION(inlUnsafeGetShortNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETSHORT):
		PERFORM_ACTION(inlUnsafeGetShort(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETSHORT_VOLATILE):
		PERFORM_ACTION(inlUnsafeGetShort(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTSHORT_NATIVE):
		PERFORM_ACTION(inlUnsafePutShortNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTSHORT):
		PERFORM_ACTION(inlUnsafePutShort(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTSHORT_VOLATILE):
		PERFORM_ACTION(inlUnsafePutShort(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETCHAR_NATIVE):
		PERFORM_ACTION(inlUnsafeGetCharNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETCHAR):
		PERFORM_ACTION(inlUnsafeGetChar(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETCHAR_VOLATILE):
		PERFORM_ACTION(inlUnsafeGetChar(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTCHAR_NATIVE):
		PERFORM_ACTION(inlUnsafePutCharNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTCHAR):
		PERFORM_ACTION(inlUnsafePutChar(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTCHAR_VOLATILE):
		PERFORM_ACTION(inlUnsafePutChar(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETINT_NATIVE):
		PERFORM_ACTION(inlUnsafeGetIntNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETINT):
		PERFORM_ACTION(inlUnsafeGetInt(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETINT_VOLATILE):
		PERFORM_ACTION(inlUnsafeGetInt(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTINT_NATIVE):
		PERFORM_ACTION(inlUnsafePutIntNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTINT):
		PERFORM_ACTION(inlUnsafePutInt(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTINT_VOLATILE):
		PERFORM_ACTION(inlUnsafePutInt(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETFLOAT_NATIVE):
		PERFORM_ACTION(inlUnsafeGetFloatNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETFLOAT):
		PERFORM_ACTION(inlUnsafeGetFloat(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETFLOAT_VOLATILE):
		PERFORM_ACTION(inlUnsafeGetFloat(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTFLOAT_NATIVE):
		PERFORM_ACTION(inlUnsafePutFloatNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTFLOAT):
		PERFORM_ACTION(inlUnsafePutFloat(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTFLOAT_VOLATILE):
		PERFORM_ACTION(inlUnsafePutFloat(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETLONG_NATIVE):
		PERFORM_ACTION(inlUnsafeGetLongNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETLONG):
		PERFORM_ACTION(inlUnsafeGetLong(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETLONG_VOLATILE):
		PERFORM_ACTION(inlUnsafeGetLong(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTLONG_NATIVE):
		PERFORM_ACTION(inlUnsafePutLongNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTLONG):
		PERFORM_ACTION(inlUnsafePutLong(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTLONG_VOLATILE):
		PERFORM_ACTION(inlUnsafePutLong(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETDOUBLE_NATIVE):
		PERFORM_ACTION(inlUnsafeGetDoubleNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETDOUBLE):
		PERFORM_ACTION(inlUnsafeGetDouble(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETDOUBLE_VOLATILE):
		PERFORM_ACTION(inlUnsafeGetDouble(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTDOUBLE_NATIVE):
		PERFORM_ACTION(inlUnsafePutDoubleNative(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTDOUBLE):
		PERFORM_ACTION(inlUnsafePutDouble(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTDOUBLE_VOLATILE):
		PERFORM_ACTION(inlUnsafePutDouble(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETOBJECT):
		PERFORM_ACTION(inlUnsafeGetObject(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETOBJECT_VOLATILE):
		PERFORM_ACTION(inlUnsafeGetObject(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTOBJECT):
		PERFORM_ACTION(inlUnsafePutObject(REGISTER_ARGS, false));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTOBJECT_VOLATILE):
		PERFORM_ACTION(inlUnsafePutObject(REGISTER_ARGS, true));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETADDRESS):
		PERFORM_ACTION(inlUnsafeGetAddress(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTADDRESS):
		PERFORM_ACTION(inlUnsafePutAddress(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ADDRESS_SIZE):
		PERFORM_ACTION(inlUnsafeAddressSize(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ARRAY_BASE_OFFSET):
		PERFORM_ACTION(inlUnsafeArrayBaseOffset(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ARRAY_INDEX_SCALE):
		PERFORM_ACTION(inlUnsafeArrayIndexScale(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_LOAD_FENCE):
		PERFORM_ACTION(inlUnsafeLoadFence(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_STORE_FENCE):
		PERFORM_ACTION(inlUnsafeStoreFence(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_COMPAREANDSWAPOBJECT):
		PERFORM_ACTION(inlUnsafeCompareAndSwapObject(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_COMPAREANDSWAPLONG):
		PERFORM_ACTION(inlUnsafeCompareAndSwapLong(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_COMPAREANDSWAPINT):
		PERFORM_ACTION(inlUnsafeCompareAndSwapInt(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_GET_INTERFACES):
		PERFORM_ACTION(inlInternalsGetInterfaces(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_ARRAY_NEW_ARRAY_IMPL):
		PERFORM_ACTION(inlArrayNewArrayImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASSLOADER_FIND_LOADED_CLASS_IMPL):
		PERFORM_ACTION(inlClassLoaderFindLoadedClassImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_VM_FIND_CLASS_OR_NULL):
		PERFORM_ACTION(inlVMFindClassOrNull(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_CLASS_FORNAMEIMPL):
		PERFORM_ACTION(inlClassForNameImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_THREAD_INTERRUPTED):
		PERFORM_ACTION(inlThreadInterrupted(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_VM_GET_CP_INDEX_IMPL):
		PERFORM_ACTION(inlVMGetCPIndexImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_VM_GET_STACK_CLASS_LOADER):
		PERFORM_ACTION(inlVMGetStackClassLoader(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_OBJECT_NOTIFY_ALL):
		PERFORM_ACTION(inlObjectNotify(REGISTER_ARGS, omrthread_monitor_notify_all));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_OBJECT_NOTIFY):
		PERFORM_ACTION(inlObjectNotify(REGISTER_ARGS, omrthread_monitor_notify));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_INSTANCE):
		PERFORM_ACTION(inlClassIsInstance(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASS_GET_SIMPLE_NAME_IMPL):
		PERFORM_ACTION(inlClassGetSimpleNameImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_VM_INITIALIZE_CLASS_LOADER):
		PERFORM_ACTION(inlVMInitializeClassLoader(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_VM_GET_CLASS_PATH_ENTRY_TYPE):
		PERFORM_ACTION(inlVMGetClassPathEntryType(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_VM_IS_BOOTSTRAP_CLASS_LOADER):
		PERFORM_ACTION(inlVMIsBootstrapClassLoader(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ALLOCATE_INSTANCE):
		PERFORM_ACTION(inlUnsafeAllocateInstance(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_INTERNALS_PREPARE_CLASS_IMPL):
		PERFORM_ACTION(inlInternalsPrepareClassImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_ATTACHMENT_LOADAGENTLIBRARYIMPL):
		PERFORM_ACTION(inlAttachmentLoadAgentLibraryImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_VM_GETSTACKCLASS):
		PERFORM_ACTION(inlVMGetStackClass(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_THREAD_SLEEP):
		PERFORM_ACTION(inlThreadSleep(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_OBJECT_WAIT):
		PERFORM_ACTION(inlObjectWait(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASSLOADER_LOADLIBRARYWITHPATH):
		PERFORM_ACTION(inlClassLoaderLoadLibraryWithPath(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_THREAD_ISINTERRUPTEDIMPL):
		PERFORM_ACTION(inlThreadIsInterruptedImpl(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_CLASSLOADER_INITIALIZEANONCLASSLOADER):
		PERFORM_ACTION(inlInitializeAnonClassLoader(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_VARHANDLE):
			PERFORM_ACTION(invokevarhandle(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_INL_THREAD_ON_SPIN_WAIT):
		PERFORM_ACTION(inlThreadOnSpinWait(REGISTER_ARGS));
	JUMP_TARGET(J9_BCLOOP_SEND_TARGET_OUT_OF_LINE_INL):
		PERFORM_ACTION(outOfLineINL(REGISTER_ARGS));
#if !defined(USE_COMPUTED_GOTO)
	default:
		Assert_VM_unreachable();
	}
#endif
}

i2j:
	PERFORM_ACTION(i2jTransition(REGISTER_ARGS));

jni:
	PERFORM_ACTION(runJNINative(REGISTER_ARGS));

runMethodHandle: {
#if defined(J9VM_OPT_METHOD_HANDLE)
	j9object_t methodHandle = (j9object_t)_currentThread->tempSlot;
	void *compiledEntryPoint = VM_VMHelpers::methodHandleCompiledEntryPoint(_vm, _currentThread, methodHandle);
	if (NULL != compiledEntryPoint) {
		_currentThread->floatTemp1 = compiledEntryPoint;
		goto runMethodHandleCompiled;
	}
	methodHandle = countAndCompileMethodHandle(REGISTER_ARGS, methodHandle, &compiledEntryPoint);
	/* update tempSlot as countAndCompile may release VMAccess */
	_currentThread->tempSlot = (UDATA)methodHandle;
	if (NULL != compiledEntryPoint) {
		_currentThread->floatTemp1 = compiledEntryPoint;
		goto runMethodHandleCompiled;
	}
	PERFORM_ACTION(interpretMethodHandle(REGISTER_ARGS, methodHandle));
#else
	Assert_VM_unreachable();
#endif /* J9VM_OPT_METHOD_HANDLE */
}

runMethodHandleCompiled:
#if defined(J9VM_OPT_METHOD_HANDLE)
	/* VMThread->tempSlot will hold the MethodHandle.
	 * VMThread->floatTemp1 will hold the compiledEntryPoint
	 */
	Assert_VM_true(_currentThread->tempSlot != 0);
	Assert_VM_notNull(_currentThread->floatTemp1);
	modifyMethodHandleCountForI2J(REGISTER_ARGS, (j9object_t) _currentThread->tempSlot);
	PERFORM_ACTION(i2jMHTransition(REGISTER_ARGS));
#else
	Assert_VM_unreachable();
#endif /* J9VM_OPT_METHOD_HANDLE */

runMethodFromMethodHandle:
#if defined(J9VM_OPT_METHOD_HANDLE)
	_sendMethod = (J9Method *)_currentThread->tempSlot;
	goto runMethod;
#else
	Assert_VM_unreachable();
#endif /* J9VM_OPT_METHOD_HANDLE */

asyncCheck:
	PERFORM_ACTION(checkAsync(REGISTER_ARGS));

throwCurrentException:
	PERFORM_ACTION(throwException(REGISTER_ARGS));

branchWithAsyncCheck:
	PERFORM_ACTION(takeBranch(REGISTER_ARGS));

javaStackOverflow:
	PERFORM_ACTION(stackOverflow(REGISTER_ARGS));

nullPointer:
	updateVMStruct(REGISTER_ARGS);
	prepareForExceptionThrow(_currentThread);
	setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;

wrongMethodType:
	updateVMStruct(REGISTER_ARGS);
	prepareForExceptionThrow(_currentThread);
	setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGINVOKEWRONGMETHODTYPEEXCEPTION, NULL);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;

arrayIndex: {
	I_32 aiobIndex = (I_32)_currentThread->tempSlot;
	updateVMStruct(REGISTER_ARGS);
	prepareForExceptionThrow(_currentThread);
	setArrayIndexOutOfBoundsException(_currentThread, aiobIndex);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;
}

divideByZero:
	updateVMStruct(REGISTER_ARGS);
	prepareForExceptionThrow(_currentThread);
	setCurrentExceptionNLS(_currentThread, J9VMCONSTANTPOOL_JAVALANGARITHMETICEXCEPTION, J9NLS_VM_DIVIDE_BY_ZERO);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;

failedToAllocateMonitor:
	/* vmStruct is already up-to-date */
	prepareForExceptionThrow(_currentThread);
	setNativeOutOfMemoryError(_currentThread, J9NLS_VM_FAILED_TO_ALLOCATE_MONITOR);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;

illegalMonitorState:
	updateVMStruct(REGISTER_ARGS);
	prepareForExceptionThrow(_currentThread);
	setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;

incompatibleClassChange:
	updateVMStruct(REGISTER_ARGS);
	prepareForExceptionThrow(_currentThread);
	setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, NULL);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;

negativeArraySize:
	updateVMStruct(REGISTER_ARGS);
	prepareForExceptionThrow(_currentThread);
	setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGNEGATIVEARRAYSIZEEXCEPTION, NULL);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;

heapOOM:
	updateVMStruct(REGISTER_ARGS);
	prepareForExceptionThrow(_currentThread);
	setHeapOutOfMemoryError(_currentThread);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;

arrayStoreException:
	updateVMStruct(REGISTER_ARGS);
	prepareForExceptionThrow(_currentThread);
	setCurrentExceptionUTF(_currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYSTOREEXCEPTION, NULL);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	goto throwCurrentException;

#if defined(DEBUG_VERSION)
executeBreakpointedBytecode: {
	UDATA originalBytecode = JBbreakpoint;
	updateVMStruct(REGISTER_ARGS);
	TRIGGER_J9HOOK_VM_BREAKPOINT(
			_vm->hookInterface,
			_currentThread,
			_literals,
			_pc - _literals->bytecodes,
			originalBytecode);
	VMStructHasBeenUpdated(REGISTER_ARGS);
	if (immediateAsyncPending()) {
		goto asyncCheck;
	}
	/* Single step was reported before breakpoint, don't report it again for this bytecode */
	skipNextSingleStep();
	EXECUTE_BYTECODE_NUMBER((U_8)originalBytecode);
}
#endif /* DEBUG_VERSION */

#if defined(USE_COMPUTED_GOTO)
	EXECUTE_CURRENT_BYTECODE();
#else
executeCurrentBytecode:
#if defined(DEBUG_VERSION)
	bytecode = *_pc;
executeBytecodeFromLocal:
	RECORD_BYTECODE_PAIR(bytecode);
	switch (bytecode)
#else
	RECORD_BYTECODE_PAIR(*_pc);
	switch (*_pc)
#endif
#endif
	{
		JUMP_TARGET(JBnop):
		JUMP_TARGET(JBasyncCheck):
			SINGLE_STEP();
			PERFORM_ACTION(nop(REGISTER_ARGS));
		JUMP_TARGET(JBaconstnull):
		JUMP_TARGET(JBiconst0):
		JUMP_TARGET(JBfconst0):
			SINGLE_STEP();
			PERFORM_ACTION(aconst(REGISTER_ARGS, 0));
		JUMP_TARGET(JBiconstm1):
			SINGLE_STEP();
			PERFORM_ACTION(iconst(REGISTER_ARGS, -1));
		JUMP_TARGET(JBiconst1):
			SINGLE_STEP();
			PERFORM_ACTION(iconst(REGISTER_ARGS, 1));
		JUMP_TARGET(JBiconst2):
			SINGLE_STEP();
			PERFORM_ACTION(iconst(REGISTER_ARGS, 2));
		JUMP_TARGET(JBiconst3):
			SINGLE_STEP();
			PERFORM_ACTION(iconst(REGISTER_ARGS, 3));
		JUMP_TARGET(JBiconst4):
			SINGLE_STEP();
			PERFORM_ACTION(iconst(REGISTER_ARGS, 4));
		JUMP_TARGET(JBiconst5):
			SINGLE_STEP();
			PERFORM_ACTION(iconst(REGISTER_ARGS, 5));
		JUMP_TARGET(JBlconst0):
		JUMP_TARGET(JBdconst0):
			SINGLE_STEP();
			PERFORM_ACTION(lconst(REGISTER_ARGS, 0));
		JUMP_TARGET(JBlconst1):
			SINGLE_STEP();
			PERFORM_ACTION(lconst(REGISTER_ARGS, 1));
		JUMP_TARGET(JBfconst1):
			SINGLE_STEP();
			PERFORM_ACTION(iconst(REGISTER_ARGS, 0x3F800000));
		JUMP_TARGET(JBfconst2):
			SINGLE_STEP();
			PERFORM_ACTION(iconst(REGISTER_ARGS, 0x40000000));
		JUMP_TARGET(JBdconst1):
			SINGLE_STEP();
			PERFORM_ACTION(lconst(REGISTER_ARGS, J9CONST64(0x3FF0000000000000)));
		JUMP_TARGET(JBbipush):
			SINGLE_STEP();
			PERFORM_ACTION(bipush(REGISTER_ARGS));
		JUMP_TARGET(JBsipush):
			SINGLE_STEP();
			PERFORM_ACTION(sipush(REGISTER_ARGS));
		JUMP_TARGET(JBldc):
			SINGLE_STEP();
			PERFORM_ACTION(ldc(REGISTER_ARGS, 1));
		JUMP_TARGET(JBldcw):
			SINGLE_STEP();
			PERFORM_ACTION(ldc(REGISTER_ARGS, 2));
		JUMP_TARGET(JBldc2dw):
		JUMP_TARGET(JBldc2lw):
			SINGLE_STEP();
			PERFORM_ACTION(ldc2lw(REGISTER_ARGS));
		JUMP_TARGET(JBaload):
		JUMP_TARGET(JBiload):
		JUMP_TARGET(JBfload):
			SINGLE_STEP();
			PERFORM_ACTION(aload(REGISTER_ARGS));
		JUMP_TARGET(JBlload):
		JUMP_TARGET(JBdload):
			SINGLE_STEP();
			PERFORM_ACTION(lload(REGISTER_ARGS));
		JUMP_TARGET(JBaload0getfield):
#if !defined(DO_SINGLE_STEP)
			PERFORM_ACTION(aload0getfield(REGISTER_ARGS));
#endif /* DEBUG_VERSION */
		/* Intentional fall-through when single stepping is possible */
		JUMP_TARGET(JBaload0):
		JUMP_TARGET(JBiload0):
		JUMP_TARGET(JBfload0):
			SINGLE_STEP();
			PERFORM_ACTION(aload(REGISTER_ARGS, 0));
		JUMP_TARGET(JBaload1):
		JUMP_TARGET(JBiload1):
		JUMP_TARGET(JBfload1):
			SINGLE_STEP();
			PERFORM_ACTION(aload(REGISTER_ARGS, 1));
		JUMP_TARGET(JBaload2):
		JUMP_TARGET(JBiload2):
		JUMP_TARGET(JBfload2):
			SINGLE_STEP();
			PERFORM_ACTION(aload(REGISTER_ARGS, 2));
		JUMP_TARGET(JBaload3):
		JUMP_TARGET(JBiload3):
		JUMP_TARGET(JBfload3):
			SINGLE_STEP();
			PERFORM_ACTION(aload(REGISTER_ARGS, 3));
		JUMP_TARGET(JBlload0):
		JUMP_TARGET(JBdload0):
			SINGLE_STEP();
			PERFORM_ACTION(lload(REGISTER_ARGS, 0));
		JUMP_TARGET(JBlload1):
		JUMP_TARGET(JBdload1):
			SINGLE_STEP();
			PERFORM_ACTION(lload(REGISTER_ARGS, 1));
		JUMP_TARGET(JBlload2):
		JUMP_TARGET(JBdload2):
			SINGLE_STEP();
			PERFORM_ACTION(lload(REGISTER_ARGS, 2));
		JUMP_TARGET(JBlload3):
		JUMP_TARGET(JBdload3):
			SINGLE_STEP();
			PERFORM_ACTION(lload(REGISTER_ARGS, 3));
		JUMP_TARGET(JBastore):
		JUMP_TARGET(JBistore):
		JUMP_TARGET(JBfstore):
			SINGLE_STEP();
			PERFORM_ACTION(astore(REGISTER_ARGS));
		JUMP_TARGET(JBlstore):
		JUMP_TARGET(JBdstore):
			SINGLE_STEP();
			PERFORM_ACTION(lstore(REGISTER_ARGS));
		JUMP_TARGET(JBastore0):
		JUMP_TARGET(JBistore0):
		JUMP_TARGET(JBfstore0):
			SINGLE_STEP();
			PERFORM_ACTION(astore(REGISTER_ARGS, 0));
		JUMP_TARGET(JBastore1):
		JUMP_TARGET(JBistore1):
		JUMP_TARGET(JBfstore1):
			SINGLE_STEP();
			PERFORM_ACTION(astore(REGISTER_ARGS, 1));
		JUMP_TARGET(JBastore2):
		JUMP_TARGET(JBistore2):
		JUMP_TARGET(JBfstore2):
			SINGLE_STEP();
			PERFORM_ACTION(astore(REGISTER_ARGS, 2));
		JUMP_TARGET(JBastore3):
		JUMP_TARGET(JBistore3):
		JUMP_TARGET(JBfstore3):
			SINGLE_STEP();
			PERFORM_ACTION(astore(REGISTER_ARGS, 3));
		JUMP_TARGET(JBlstore0):
		JUMP_TARGET(JBdstore0):
			SINGLE_STEP();
			PERFORM_ACTION(lstore(REGISTER_ARGS, 0));
		JUMP_TARGET(JBlstore1):
		JUMP_TARGET(JBdstore1):
			SINGLE_STEP();
			PERFORM_ACTION(lstore(REGISTER_ARGS, 1));
		JUMP_TARGET(JBlstore2):
		JUMP_TARGET(JBdstore2):
			SINGLE_STEP();
			PERFORM_ACTION(lstore(REGISTER_ARGS, 2));
		JUMP_TARGET(JBlstore3):
		JUMP_TARGET(JBdstore3):
			SINGLE_STEP();
			PERFORM_ACTION(lstore(REGISTER_ARGS, 3));
		JUMP_TARGET(JBiaload):
			SINGLE_STEP();
			PERFORM_ACTION(iaload(REGISTER_ARGS));
		JUMP_TARGET(JBlaload):
			SINGLE_STEP();
			PERFORM_ACTION(laload(REGISTER_ARGS));
		JUMP_TARGET(JBfaload):
			SINGLE_STEP();
			PERFORM_ACTION(faload(REGISTER_ARGS));
		JUMP_TARGET(JBdaload):
			SINGLE_STEP();
			PERFORM_ACTION(daload(REGISTER_ARGS));
		JUMP_TARGET(JBaaload):
			SINGLE_STEP();
			PERFORM_ACTION(aaload(REGISTER_ARGS));
		JUMP_TARGET(JBbaload):
			SINGLE_STEP();
			PERFORM_ACTION(baload(REGISTER_ARGS));
		JUMP_TARGET(JBcaload):
			SINGLE_STEP();
			PERFORM_ACTION(caload(REGISTER_ARGS));
		JUMP_TARGET(JBsaload):
			SINGLE_STEP();
			PERFORM_ACTION(saload(REGISTER_ARGS));
		JUMP_TARGET(JBiastore):
			SINGLE_STEP();
			PERFORM_ACTION(iastore(REGISTER_ARGS));
		JUMP_TARGET(JBlastore):
			SINGLE_STEP();
			PERFORM_ACTION(lastore(REGISTER_ARGS));
		JUMP_TARGET(JBfastore):
			SINGLE_STEP();
			PERFORM_ACTION(fastore(REGISTER_ARGS));
		JUMP_TARGET(JBdastore):
			SINGLE_STEP();
			PERFORM_ACTION(dastore(REGISTER_ARGS));
		JUMP_TARGET(JBaastore):
			SINGLE_STEP();
			PERFORM_ACTION(aastore(REGISTER_ARGS));
		JUMP_TARGET(JBbastore):
			SINGLE_STEP();
			PERFORM_ACTION(bastore(REGISTER_ARGS));
		JUMP_TARGET(JBcastore):
			SINGLE_STEP();
			PERFORM_ACTION(castore(REGISTER_ARGS));
		JUMP_TARGET(JBsastore):
			SINGLE_STEP();
			PERFORM_ACTION(sastore(REGISTER_ARGS));
		JUMP_TARGET(JBpop):
			SINGLE_STEP();
			PERFORM_ACTION(pop(REGISTER_ARGS, 1));
		JUMP_TARGET(JBpop2):
			SINGLE_STEP();
			PERFORM_ACTION(pop(REGISTER_ARGS, 2));
		JUMP_TARGET(JBdup):
			SINGLE_STEP();
			PERFORM_ACTION(dup(REGISTER_ARGS));
		JUMP_TARGET(JBdupx1):
			SINGLE_STEP();
			PERFORM_ACTION(dupx1(REGISTER_ARGS));
		JUMP_TARGET(JBdupx2):
			SINGLE_STEP();
			PERFORM_ACTION(dupx2(REGISTER_ARGS));
		JUMP_TARGET(JBdup2):
			SINGLE_STEP();
			PERFORM_ACTION(dup2(REGISTER_ARGS));
		JUMP_TARGET(JBdup2x1):
			SINGLE_STEP();
			PERFORM_ACTION(dup2x1(REGISTER_ARGS));
		JUMP_TARGET(JBdup2x2):
			SINGLE_STEP();
			PERFORM_ACTION(dup2x2(REGISTER_ARGS));
		JUMP_TARGET(JBswap):
			SINGLE_STEP();
			PERFORM_ACTION(swap(REGISTER_ARGS));
		JUMP_TARGET(JBiadd):
			SINGLE_STEP();
			PERFORM_ACTION(iadd(REGISTER_ARGS));
		JUMP_TARGET(JBladd):
			SINGLE_STEP();
			PERFORM_ACTION(ladd(REGISTER_ARGS));
		JUMP_TARGET(JBfadd):
			SINGLE_STEP();
			PERFORM_ACTION(fadd(REGISTER_ARGS));
		JUMP_TARGET(JBdadd):
			SINGLE_STEP();
			PERFORM_ACTION(dadd(REGISTER_ARGS));
		JUMP_TARGET(JBisub):
			SINGLE_STEP();
			PERFORM_ACTION(isub(REGISTER_ARGS));
		JUMP_TARGET(JBlsub):
			SINGLE_STEP();
			PERFORM_ACTION(lsub(REGISTER_ARGS));
		JUMP_TARGET(JBfsub):
			SINGLE_STEP();
			PERFORM_ACTION(fsub(REGISTER_ARGS));
		JUMP_TARGET(JBdsub):
			SINGLE_STEP();
			PERFORM_ACTION(dsub(REGISTER_ARGS));
		JUMP_TARGET(JBimul):
			SINGLE_STEP();
			PERFORM_ACTION(imul(REGISTER_ARGS));
		JUMP_TARGET(JBlmul):
			SINGLE_STEP();
			PERFORM_ACTION(lmul(REGISTER_ARGS));
		JUMP_TARGET(JBfmul):
			SINGLE_STEP();
			PERFORM_ACTION(fmul(REGISTER_ARGS));
		JUMP_TARGET(JBdmul):
			SINGLE_STEP();
			PERFORM_ACTION(dmul(REGISTER_ARGS));
		JUMP_TARGET(JBidiv):
			SINGLE_STEP();
			PERFORM_ACTION(idiv(REGISTER_ARGS));
		JUMP_TARGET(JBldiv):
			SINGLE_STEP();
			PERFORM_ACTION(ldiv(REGISTER_ARGS));
		JUMP_TARGET(JBfdiv):
			SINGLE_STEP();
			PERFORM_ACTION(fdiv(REGISTER_ARGS));
		JUMP_TARGET(JBddiv):
			SINGLE_STEP();
			PERFORM_ACTION(ddiv(REGISTER_ARGS));
		JUMP_TARGET(JBirem):
			SINGLE_STEP();
			PERFORM_ACTION(irem(REGISTER_ARGS));
		JUMP_TARGET(JBlrem):
			SINGLE_STEP();
			PERFORM_ACTION(lrem(REGISTER_ARGS));
		JUMP_TARGET(JBfrem):
			SINGLE_STEP();
			PERFORM_ACTION(frem(REGISTER_ARGS));
		JUMP_TARGET(JBdrem):
			SINGLE_STEP();
			PERFORM_ACTION(drem(REGISTER_ARGS));
		JUMP_TARGET(JBineg):
			SINGLE_STEP();
			PERFORM_ACTION(ineg(REGISTER_ARGS));
		JUMP_TARGET(JBlneg):
			SINGLE_STEP();
			PERFORM_ACTION(lneg(REGISTER_ARGS));
		JUMP_TARGET(JBfneg):
			SINGLE_STEP();
			PERFORM_ACTION(fneg(REGISTER_ARGS));
		JUMP_TARGET(JBdneg):
			SINGLE_STEP();
			PERFORM_ACTION(dneg(REGISTER_ARGS));
		JUMP_TARGET(JBishl):
			SINGLE_STEP();
			PERFORM_ACTION(ishl(REGISTER_ARGS));
		JUMP_TARGET(JBlshl):
			SINGLE_STEP();
			PERFORM_ACTION(lshl(REGISTER_ARGS));
		JUMP_TARGET(JBishr):
			SINGLE_STEP();
			PERFORM_ACTION(ishr(REGISTER_ARGS));
		JUMP_TARGET(JBlshr):
			SINGLE_STEP();
			PERFORM_ACTION(lshr(REGISTER_ARGS));
		JUMP_TARGET(JBiushr):
			SINGLE_STEP();
			PERFORM_ACTION(iushr(REGISTER_ARGS));
		JUMP_TARGET(JBlushr):
			SINGLE_STEP();
			PERFORM_ACTION(lushr(REGISTER_ARGS));
		JUMP_TARGET(JBiand):
			SINGLE_STEP();
			PERFORM_ACTION(iand(REGISTER_ARGS));
		JUMP_TARGET(JBland):
			SINGLE_STEP();
			PERFORM_ACTION(land(REGISTER_ARGS));
		JUMP_TARGET(JBior):
			SINGLE_STEP();
			PERFORM_ACTION(ior(REGISTER_ARGS));
		JUMP_TARGET(JBlor):
			SINGLE_STEP();
			PERFORM_ACTION(lor(REGISTER_ARGS));
		JUMP_TARGET(JBixor):
			SINGLE_STEP();
			PERFORM_ACTION(ixor(REGISTER_ARGS));
		JUMP_TARGET(JBlxor):
			SINGLE_STEP();
			PERFORM_ACTION(lxor(REGISTER_ARGS));
		JUMP_TARGET(JBiinc):
			SINGLE_STEP();
			PERFORM_ACTION(iinc(REGISTER_ARGS, 1));
		JUMP_TARGET(JBi2l):
			SINGLE_STEP();
			PERFORM_ACTION(i2l(REGISTER_ARGS));
		JUMP_TARGET(JBi2f):
			SINGLE_STEP();
			PERFORM_ACTION(i2f(REGISTER_ARGS));
		JUMP_TARGET(JBi2d):
			SINGLE_STEP();
			PERFORM_ACTION(i2d(REGISTER_ARGS));
		JUMP_TARGET(JBl2i):
			SINGLE_STEP();
			PERFORM_ACTION(l2i(REGISTER_ARGS));
		JUMP_TARGET(JBl2f):
			SINGLE_STEP();
			PERFORM_ACTION(l2f(REGISTER_ARGS));
		JUMP_TARGET(JBl2d):
			SINGLE_STEP();
			PERFORM_ACTION(l2d(REGISTER_ARGS));
		JUMP_TARGET(JBf2i):
			SINGLE_STEP();
			PERFORM_ACTION(f2i(REGISTER_ARGS));
		JUMP_TARGET(JBf2l):
			SINGLE_STEP();
			PERFORM_ACTION(f2l(REGISTER_ARGS));
		JUMP_TARGET(JBf2d):
			SINGLE_STEP();
			PERFORM_ACTION(f2d(REGISTER_ARGS));
		JUMP_TARGET(JBd2i):
			SINGLE_STEP();
			PERFORM_ACTION(d2i(REGISTER_ARGS));
		JUMP_TARGET(JBd2l):
			SINGLE_STEP();
			PERFORM_ACTION(d2l(REGISTER_ARGS));
		JUMP_TARGET(JBd2f):
			SINGLE_STEP();
			PERFORM_ACTION(d2f(REGISTER_ARGS));
		JUMP_TARGET(JBi2b):
			SINGLE_STEP();
			PERFORM_ACTION(i2b(REGISTER_ARGS));
		JUMP_TARGET(JBi2c):
			SINGLE_STEP();
			PERFORM_ACTION(i2c(REGISTER_ARGS));
		JUMP_TARGET(JBi2s):
			SINGLE_STEP();
			PERFORM_ACTION(i2s(REGISTER_ARGS));
		JUMP_TARGET(JBlcmp):
			SINGLE_STEP();
			PERFORM_ACTION(lcmp(REGISTER_ARGS));
		JUMP_TARGET(JBfcmpl):
			SINGLE_STEP();
			PERFORM_ACTION(fcmp(REGISTER_ARGS, -1));
		JUMP_TARGET(JBfcmpg):
			SINGLE_STEP();
			PERFORM_ACTION(fcmp(REGISTER_ARGS, 1));
		JUMP_TARGET(JBdcmpl):
			SINGLE_STEP();
			PERFORM_ACTION(dcmp(REGISTER_ARGS, -1));
		JUMP_TARGET(JBdcmpg):
			SINGLE_STEP();
			PERFORM_ACTION(dcmp(REGISTER_ARGS, 1));
		JUMP_TARGET(JBifnull):
#if defined(J9VM_ENV_DATA64)
			SINGLE_STEP();
			PERFORM_ACTION(ifnull(REGISTER_ARGS));
#endif
		JUMP_TARGET(JBifeq):
			SINGLE_STEP();
			PERFORM_ACTION(ifeq(REGISTER_ARGS));
		JUMP_TARGET(JBifnonnull):
#if defined(J9VM_ENV_DATA64)
			SINGLE_STEP();
			PERFORM_ACTION(ifnonnull(REGISTER_ARGS));
#endif
		JUMP_TARGET(JBifne):
			SINGLE_STEP();
			PERFORM_ACTION(ifne(REGISTER_ARGS));
		JUMP_TARGET(JBiflt):
			SINGLE_STEP();
			PERFORM_ACTION(iflt(REGISTER_ARGS));
		JUMP_TARGET(JBifge):
			SINGLE_STEP();
			PERFORM_ACTION(ifge(REGISTER_ARGS));
		JUMP_TARGET(JBifgt):
			SINGLE_STEP();
			PERFORM_ACTION(ifgt(REGISTER_ARGS));
		JUMP_TARGET(JBifle):
			SINGLE_STEP();
			PERFORM_ACTION(ifle(REGISTER_ARGS));
		JUMP_TARGET(JBificmpeq):
			SINGLE_STEP();
			PERFORM_ACTION(ificmpeq(REGISTER_ARGS));
		JUMP_TARGET(JBificmpne):
			SINGLE_STEP();
			PERFORM_ACTION(ificmpne(REGISTER_ARGS));
		JUMP_TARGET(JBificmplt):
			SINGLE_STEP();
			PERFORM_ACTION(ificmplt(REGISTER_ARGS));
		JUMP_TARGET(JBificmpge):
			SINGLE_STEP();
			PERFORM_ACTION(ificmpge(REGISTER_ARGS));
		JUMP_TARGET(JBificmpgt):
			SINGLE_STEP();
			PERFORM_ACTION(ificmpgt(REGISTER_ARGS));
		JUMP_TARGET(JBificmple):
			SINGLE_STEP();
			PERFORM_ACTION(ificmple(REGISTER_ARGS));
		JUMP_TARGET(JBifacmpeq):
			SINGLE_STEP();
			PERFORM_ACTION(ifacmpeq(REGISTER_ARGS));
		JUMP_TARGET(JBifacmpne):
			SINGLE_STEP();
			PERFORM_ACTION(ifacmpne(REGISTER_ARGS));
		JUMP_TARGET(JBgoto):
			SINGLE_STEP();
			PERFORM_ACTION(gotoImpl(REGISTER_ARGS, 2));
		JUMP_TARGET(JBgotow):
			SINGLE_STEP();
			PERFORM_ACTION(gotoImpl(REGISTER_ARGS, 4));
		JUMP_TARGET(JBtableswitch):
			SINGLE_STEP();
			PERFORM_ACTION(tableswitch(REGISTER_ARGS));
		JUMP_TARGET(JBlookupswitch):
			SINGLE_STEP();
			PERFORM_ACTION(lookupswitch(REGISTER_ARGS));
		JUMP_TARGET(JBsyncReturn0):
			SINGLE_STEP();
			PERFORM_ACTION(syncReturn(REGISTER_ARGS, 0));
		JUMP_TARGET(JBreturn0):
			SINGLE_STEP();
			PERFORM_ACTION(returnSlots(REGISTER_ARGS, 0));
		JUMP_TARGET(JBsyncReturn1):
			SINGLE_STEP();
			PERFORM_ACTION(syncReturn(REGISTER_ARGS, 1));
		JUMP_TARGET(JBreturn1):
			SINGLE_STEP();
			PERFORM_ACTION(returnSlots(REGISTER_ARGS, 1));
		JUMP_TARGET(JBsyncReturn2):
			SINGLE_STEP();
			PERFORM_ACTION(syncReturn(REGISTER_ARGS, 2));
		JUMP_TARGET(JBreturn2):
			SINGLE_STEP();
			PERFORM_ACTION(returnSlots(REGISTER_ARGS, 2));
		JUMP_TARGET(JBgetstatic):
			SINGLE_STEP();
			PERFORM_ACTION(getstatic(REGISTER_ARGS));
		JUMP_TARGET(JBputstatic):
			SINGLE_STEP();
			PERFORM_ACTION(putstatic(REGISTER_ARGS));
		JUMP_TARGET(JBgetfield):
			SINGLE_STEP();
			PERFORM_ACTION(getfield(REGISTER_ARGS));
		JUMP_TARGET(JBputfield):
			SINGLE_STEP();
			PERFORM_ACTION(putfield(REGISTER_ARGS));
		JUMP_TARGET(JBinvokevirtual):
			SINGLE_STEP();
			PERFORM_ACTION(invokevirtual(REGISTER_ARGS));
		JUMP_TARGET(JBinvokespecial):
			SINGLE_STEP();
			PERFORM_ACTION(invokespecial(REGISTER_ARGS));
		JUMP_TARGET(JBinvokestatic):
			SINGLE_STEP();
			PERFORM_ACTION(invokestatic(REGISTER_ARGS));
		JUMP_TARGET(JBinvokeinterface2):
			SINGLE_STEP();
			PERFORM_ACTION(invokeinterface2(REGISTER_ARGS));
		JUMP_TARGET(JBinvokeinterface):
			SINGLE_STEP();
			PERFORM_ACTION(invokeinterface(REGISTER_ARGS));
		JUMP_TARGET(JBnew):
			SINGLE_STEP();
			PERFORM_ACTION(newImpl(REGISTER_ARGS));
		JUMP_TARGET(JBnewdup):
			SINGLE_STEP();
			PERFORM_ACTION(newdup(REGISTER_ARGS));
		JUMP_TARGET(JBnewarray):
			SINGLE_STEP();
			PERFORM_ACTION(newarray(REGISTER_ARGS));
		JUMP_TARGET(JBanewarray):
			SINGLE_STEP();
			PERFORM_ACTION(anewarray(REGISTER_ARGS));
		JUMP_TARGET(JBarraylength):
			SINGLE_STEP();
			PERFORM_ACTION(arraylength(REGISTER_ARGS));
		JUMP_TARGET(JBathrow):
			SINGLE_STEP();
			PERFORM_ACTION(athrow(REGISTER_ARGS));
		JUMP_TARGET(JBcheckcast):
			SINGLE_STEP();
			PERFORM_ACTION(checkcast(REGISTER_ARGS));
		JUMP_TARGET(JBinstanceof):
			SINGLE_STEP();
			PERFORM_ACTION(instanceof(REGISTER_ARGS));
		JUMP_TARGET(JBmonitorenter):
			SINGLE_STEP();
			PERFORM_ACTION(monitorenter(REGISTER_ARGS));
		JUMP_TARGET(JBmonitorexit):
			SINGLE_STEP();
			PERFORM_ACTION(monitorexit(REGISTER_ARGS));
		JUMP_TARGET(JBmultianewarray):
			SINGLE_STEP();
			PERFORM_ACTION(multianewarray(REGISTER_ARGS));
		JUMP_TARGET(JBaloadw):
		JUMP_TARGET(JBiloadw):
		JUMP_TARGET(JBfloadw):
			SINGLE_STEP();
			PERFORM_ACTION(aloadw(REGISTER_ARGS));
		JUMP_TARGET(JBlloadw):
		JUMP_TARGET(JBdloadw):
			SINGLE_STEP();
			PERFORM_ACTION(lloadw(REGISTER_ARGS));
		JUMP_TARGET(JBastorew):
		JUMP_TARGET(JBistorew):
		JUMP_TARGET(JBfstorew):
			SINGLE_STEP();
			PERFORM_ACTION(astorew(REGISTER_ARGS));
		JUMP_TARGET(JBlstorew):
		JUMP_TARGET(JBdstorew):
			SINGLE_STEP();
			PERFORM_ACTION(lstorew(REGISTER_ARGS));
		JUMP_TARGET(JBiincw):
			SINGLE_STEP();
			PERFORM_ACTION(iinc(REGISTER_ARGS, 2));
		JUMP_TARGET(JBreturnFromConstructor):
			SINGLE_STEP();
			PERFORM_ACTION(returnFromConstructor(REGISTER_ARGS));
		JUMP_TARGET(JBgenericReturn):
			SINGLE_STEP();
			PERFORM_ACTION(genericReturn(REGISTER_ARGS));
#if defined(DEBUG_VERSION)
		JUMP_TARGET(JBbreakpoint):
			PERFORM_ACTION(singleStep(REGISTER_ARGS));
			if (JBbreakpoint == *_pc) {
				/* Single step was not reported, current bytecode is still breakpointed */
				goto executeBreakpointedBytecode;
			}
			/* Single step was reported, current bytecode is no longer breakpointed.
			 * Must run the current bytecode, but not report single step again.
			 */
			skipNextSingleStep();
			EXECUTE_CURRENT_BYTECODE();
#endif /* DEBUG_VERSION */
		JUMP_TARGET(JBinvokedynamic):
			SINGLE_STEP();
			PERFORM_ACTION(invokedynamic(REGISTER_ARGS));
		JUMP_TARGET(JBinvokehandle):
			SINGLE_STEP();
			PERFORM_ACTION(invokehandle(REGISTER_ARGS));
		JUMP_TARGET(JBinvokehandlegeneric):
			SINGLE_STEP();
			PERFORM_ACTION(invokehandlegeneric(REGISTER_ARGS));
		JUMP_TARGET(JBinvokestaticsplit):
			SINGLE_STEP();
			PERFORM_ACTION(invokestaticsplit(REGISTER_ARGS));
		JUMP_TARGET(JBinvokespecialsplit):
			SINGLE_STEP();
			PERFORM_ACTION(invokespecialsplit(REGISTER_ARGS));
		JUMP_TARGET(JBreturnC):
			SINGLE_STEP();
			PERFORM_ACTION(returnC(REGISTER_ARGS));
		JUMP_TARGET(JBreturnS):
			SINGLE_STEP();
			PERFORM_ACTION(returnS(REGISTER_ARGS));
		JUMP_TARGET(JBreturnB):
			SINGLE_STEP();
			PERFORM_ACTION(returnB(REGISTER_ARGS));
		JUMP_TARGET(JBreturnZ):
			SINGLE_STEP();
			PERFORM_ACTION(returnZ(REGISTER_ARGS));
		JUMP_TARGET(JBretFromNative0):
			/* No single step for this bytecode */
			PERFORM_ACTION(retFromNative0(REGISTER_ARGS));
		JUMP_TARGET(JBretFromNative1):
			/* No single step for this bytecode */
			PERFORM_ACTION(retFromNative1(REGISTER_ARGS));
		JUMP_TARGET(JBretFromNativeF):
			/* No single step for this bytecode */
			PERFORM_ACTION(retFromNativeF(REGISTER_ARGS));
		JUMP_TARGET(JBretFromNativeD):
			/* No single step for this bytecode */
			PERFORM_ACTION(retFromNativeD(REGISTER_ARGS));
		JUMP_TARGET(JBretFromNativeJ):
			/* No single step for this bytecode */
			PERFORM_ACTION(retFromNativeJ(REGISTER_ARGS));
		JUMP_TARGET(JBreturnFromJ2I):
			/* No single step for this bytecode */
			PERFORM_ACTION(j2iReturn(REGISTER_ARGS));
		JUMP_TARGET(JBimpdep1):
			/* No single step for this bytecode */
			PERFORM_ACTION(impdep1(REGISTER_ARGS));
		JUMP_TARGET(JBimpdep2):
			/* No single step for this bytecode */
			PERFORM_ACTION(impdep2(REGISTER_ARGS));
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		JUMP_TARGET(JBdefaultvalue):
			SINGLE_STEP();
			PERFORM_ACTION(defaultvalue(REGISTER_ARGS));
		JUMP_TARGET(JBwithfield):
			SINGLE_STEP();
			PERFORM_ACTION(withfield(REGISTER_ARGS));
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
#if defined(USE_COMPUTED_GOTO)
		cJBunimplemented:
#else
		default:
#endif
			/* No single step for this bytecode */
			PERFORM_ACTION(unimplementedBytecode(REGISTER_ARGS));
		}

done:
		updateVMStruct(REGISTER_ARGS);
noUpdate:
#if defined(TRACE_TRANSITIONS)
		switch(_nextAction) {
		case J9_BCLOOP_EXECUTE_BYTECODE:
			getMethodName(PORTLIB, _literals, _pc, currentMethodName);
			j9tty_printf(PORTLIB, "<%p> exit : %s J9_BCLOOP_EXECUTE_BYTECODE %s (%d)\n", vmThread, currentMethodName, JavaBCNames[*vmThread->pc], *vmThread->pc);
			break;
		case J9_BCLOOP_RUN_METHOD:
			getMethodName(PORTLIB, (J9Method*)vmThread->tempSlot, (U_8*)-1, nextMethodName);
			j9tty_printf(PORTLIB, "<%p> exit : %s J9_BCLOOP_RUN_METHOD %s\n", vmThread, currentMethodName, nextMethodName);
			break;
		case J9_BCLOOP_JUMP_BYTECODE_PROTOTYPE:
			j9tty_printf(PORTLIB, "<%p> exit : J9_BCLOOP_JUMP_BYTECODE_PROTOTYPE %p ft=%p\n", vmThread, vmThread->tempSlot, vmThread->floatTemp1);
			break;
		case J9_BCLOOP_RUN_METHOD_COMPILED:
			getMethodName(PORTLIB, (J9Method*)vmThread->tempSlot, (U_8*)-1, nextMethodName);
			j9tty_printf(PORTLIB, "<%p> exit : J9_BCLOOP_RUN_METHOD_COMPILED %s\n", vmThread, nextMethodName);
			break;
		case J9_BCLOOP_RUN_EXCEPTION_HANDLER:
			j9tty_printf(PORTLIB, "<%p> exit : J9_BCLOOP_RUN_EXCEPTION_HANDLER %p\n", vmThread, vmThread->tempSlot);
			break;
		case J9_BCLOOP_RUN_JNI_NATIVE:
			getMethodName(PORTLIB, (J9Method*)vmThread->tempSlot, (U_8*)-1, nextMethodName);
			j9tty_printf(PORTLIB, "<%p> exit : J9_BCLOOP_RUN_JNI_NATIVE %s\n", vmThread, nextMethodName);
			break;
		case J9_BCLOOP_RUN_METHOD_HANDLE:
			j9tty_printf(PORTLIB, "<%p> exit : J9_BCLOOP_RUN_METHOD_HANDLE\n", vmThread); // TODO: print MH_KIND
			break;
		case J9_BCLOOP_RUN_METHOD_HANDLE_COMPILED:
			j9tty_printf(PORTLIB, "<%p> exit : J9_BCLOOP_RUN_METHOD_HANDLE_COMPILED\n", vmThread); // TODO: print MH_KIND
			break;
		case J9_BCLOOP_LOAD_PRESERVED_AND_BRANCH:
			j9tty_printf(PORTLIB, "<%p> exit : J9_BCLOOP_LOAD_PRESERVED_AND_BRANCH %p\n", vmThread, vmThread->tempSlot);
			break;
		case J9_BCLOOP_EXIT_INTERPRETER:
			j9tty_printf(PORTLIB, "<%p> exit : J9_BCLOOP_EXIT_INTERPRETER\n", vmThread);
			break;
		default:
			j9tty_printf(PORTLIB, "<%p> exit : UNKNOWN %d\n", vmThread, _nextAction);
			Assert_VM_unreachable();
			break;
		}
#endif
		return _nextAction;
	}

	/**
	 * Create an instance.
	 */
	VMINLINE
	INTERPRETER_CLASS(J9VMThread *currentThread)
		: _vm(currentThread->javaVM)
#if !defined(LOCAL_CURRENT_THREAD)
		, _currentThread(currentThread)
#endif
#if !defined(LOCAL_ARG0EA)
		, _arg0EA(currentThread->arg0EA)
#endif
#if !defined(LOCAL_SP)
		, _sp(currentThread->sp)
#endif
#if !defined(LOCAL_PC)
		, _pc(currentThread->pc)
#endif
#if !defined(LOCAL_LITERALS)
		, _literals(currentThread->literals)
#endif
		, _nextAction(J9_BCLOOP_EXECUTE_BYTECODE)
		, _sendMethod(NULL)
#if defined(DO_SINGLE_STEP)
		, _skipSingleStep(false)
#endif
		, _objectAllocate(currentThread)
		, _objectAccessBarrier(currentThread)
	{
	}
};

#endif /* BYTECODEINTERPRETER_HPP_ */
