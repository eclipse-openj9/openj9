dnl Copyright (c) 2019, 2019 IBM Corp. and others
dnl
dnl This program and the accompanying materials are made available under
dnl the terms of the Eclipse Public License 2.0 which accompanies this
dnl distribution and is available at https://www.eclipse.org/legal/epl-2.0/
dnl or the Apache License, Version 2.0 which accompanies this distribution and
dnl is available at https://www.apache.org/licenses/LICENSE-2.0.
dnl
dnl This Source Code may also be made available under the following
dnl Secondary Licenses when the conditions for such availability set
dnl forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
dnl General Public License, version 2 with the GNU Classpath
dnl Exception [1] and GNU General Public License, version 2 with the
dnl OpenJDK Assembly Exception [2].
dnl
dnl [1] https://www.gnu.org/software/classpath/license.html
dnl [2] http://openjdk.java.net/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

dnl **** NOT TESTED ****

include(arm64helpers.m4)

	.file "arm64nathelp.s"

define({CALL_SLOW_PATH_ONLY_HELPER},{
	CALL_SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE($1)
	ldr x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
})

define({CALL_SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE},{
	SWITCH_TO_C_STACK
	SAVE_ALL_REGS
	str x30,[J9VMTHREAD,{#}J9TR_VMThread_jitReturnAddress]
	CALL_C_WITH_VMTHREAD(old_slow_$1)
	cbz x0,.L_done_$1
	ret x0
.L_done_$1:
	RESTORE_ALL_REGS
	RESTORE_FPLR
	SWITCH_TO_JAVA_STACK
})

define({CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE},{
	SWITCH_TO_C_STACK
	SAVE_ALL_REGS
	str x30,[J9VMTHREAD,{#}J9TR_VMThread_jitReturnAddress]
	CALL_C_WITH_VMTHREAD(old_slow_$1)
	RESTORE_ALL_REGS
	RESTORE_FPLR
	SWITCH_TO_JAVA_STACK
})

define({CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION},{
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE($1)
	ldr x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
})

define({SLOW_PATH_ONLY_HELPER},{
	DECLARE_EXTERN(old_slow_$1)
	BEGIN_HELPER($1)
	CALL_SLOW_PATH_ONLY_HELPER($1)
	END_HELPER($1)
})

define({SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE},{
	DECLARE_EXTERN(old_slow_$1)
	BEGIN_HELPER($1)
	CALL_SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE($1)
	END_HELPER($1)
})

define({TRAP_HANDLER},{SLOW_PATH_ONLY_HELPER($1)})

define({PICBUILDER_FAST_PATH_ONLY_HELPER},{FAST_PATH_ONLY_HELPER($1)})

define({PICBUILDER_SLOW_PATH_ONLY_HELPER},{SLOW_PATH_ONLY_HELPER($1)})

define({EXCEPTION_THROW_HELPER},{SLOW_PATH_ONLY_HELPER($1)})

define({SLOW_PATH_ONLY_HELPER_NO_EXCEPTION},{
	DECLARE_EXTERN(old_slow_$1)
	BEGIN_HELPER($1)
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION($1)
	END_HELPER($1)
})

define({SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE},{
	DECLARE_EXTERN(old_slow_$1)
	BEGIN_HELPER($1)
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE($1)
	END_HELPER($1)
})

define({FAST_PATH_ONLY_HELPER},{
	DECLARE_EXTERN(old_fast_$1)
	BEGIN_HELPER($1)
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	CALL_C_WITH_VMTHREAD(old_fast_$1)
	RESTORE_C_VOLATILE_REGS
	RESTORE_FPLR
	SWITCH_TO_JAVA_STACK
	ldr x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
	END_HELPER($1)
})

define({FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE},{
	DECLARE_EXTERN(old_fast_$1)
	BEGIN_HELPER($1)
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	CALL_C_WITH_VMTHREAD(old_fast_$1)
	RESTORE_C_VOLATILE_REGS
	RESTORE_FPLR
	SWITCH_TO_JAVA_STACK
	END_HELPER($1)
})

define({OLD_DUAL_MODE_HELPER},{
	DECLARE_EXTERN(old_fast_$1)
	BEGIN_HELPER($1)
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	str x30,[J9VMTHREAD,{#}J9TR_VMThread_jitReturnAddress]
	CALL_C_WITH_VMTHREAD(old_fast_$1)
	cbz x0,.L_done_$1
	SAVE_C_NONVOLATILE_REGS
	mov x1,x0
	mov x0,J9VMTHREAD
	blr x1
	cbz x0,.L_old_slow_$1
	ret x0
.L_old_slow_$1:
	RESTORE_C_NONVOLATILE_REGS
.L_done_$1:
	RESTORE_C_VOLATILE_REGS
	RESTORE_FPLR
	SWITCH_TO_JAVA_STACK
	ldr x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
	END_HELPER($1)
})

define({OLD_DUAL_MODE_HELPER_NO_RETURN_VALUE},{
	DECLARE_EXTERN(old_fast_$1)
	BEGIN_HELPER($1)
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	str x30,[J9VMTHREAD,{#}J9TR_VMThread_jitReturnAddress]
	CALL_C_WITH_VMTHREAD(old_fast_$1)
	cbz x0,.L_done_$1
	SAVE_C_NONVOLATILE_REGS
	mov x1,x0
	mov x0,J9VMTHREAD
	blr x1
	cbz x0,.L_old_slow_$1
	ret x0
.L_old_slow_$1:
	RESTORE_C_NONVOLATILE_REGS
.L_done_$1:
	RESTORE_C_VOLATILE_REGS
	RESTORE_FPLR
	SWITCH_TO_JAVA_STACK
	END_HELPER($1)
})

define({NEW_DUAL_MODE_HELPER},{
	DECLARE_EXTERN(fast_$1)
	START_PROC($1)
	SAVE_FPLR
	CALL_DIRECT(fast_$1)
	cbz x0,.L_done_$1
	SWITCH_TO_C_STACK
	SAVE_C_NONVOLATILE_REGS
	mov x1,x0
	mov x0,J9VMTHREAD
	blr x1
	cbz x0,.L_old_slow_$1
	ret x0
.L_old_slow_$1:
	RESTORE_C_NONVOLATILE_REGS
.L_done_$1:
	RESTORE_FPLR
	SWITCH_TO_JAVA_STACK
	ldr x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
	ret
	END_PROC($1)
})

define({NEW_DUAL_MODE_HELPER_NO_RETURN_VALUE},{
	DECLARE_EXTERN(fast_$1)
	START_PROC($1)
	SAVE_FPLR
	CALL_DIRECT(fast_$1)
	cbz x0,.L_done_$1
	SWITCH_TO_C_STACK
	SAVE_C_NONVOLATILE_REGS
	mov x1,x0
	mov x0,J9VMTHREAD
	blr x1
	cbz x0,.L_old_slow_$1
	ret x0
.L_old_slow_$1:
	RESTORE_C_NONVOLATILE_REGS
.L_done_$1:
	RESTORE_FPLR
	SWITCH_TO_JAVA_STACK
	ret
	END_PROC($1)
})

define({PICBUILDER_DUAL_MODE_HELPER},{OLD_DUAL_MODE_HELPER($1)})

ifdef({ASM_J9VM_JIT_NEW_DUAL_HELPERS},{

define({DUAL_MODE_HELPER},{NEW_DUAL_MODE_HELPER($1,$2)})
define({DUAL_MODE_HELPER_NO_RETURN_VALUE},{NEW_DUAL_MODE_HELPER_NO_RETURN_VALUE($1,$2)})

},{	dnl ASM_J9VM_JIT_NEW_DUAL_HELPERS

define({DUAL_MODE_HELPER},{OLD_DUAL_MODE_HELPER($1,$2)})
define({DUAL_MODE_HELPER_NO_RETURN_VALUE},{OLD_DUAL_MODE_HELPER_NO_RETURN_VALUE($1,$2)})

})	dnl ASM_J9VM_JIT_NEW_DUAL_HELPERS

define({BEGIN_RETURN_POINT},{
	START_PROC($1)
})

define({END_RETURN_POINT},{
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_floatTemp1)
	END_PROC($1)
})

define({CINTERP},{
	mov x0,{#}$1
	mov x1,{#}$2
	b FUNC_LABEL(cInterpreterFromJIT)
})

define({UNUSED},{
START_PROC($1)
	mov x0,{#}-1
	str x0,[x0]
END_PROC($1)
})

dnl Runtime helpers

DUAL_MODE_HELPER(jitNewObject,1)
DUAL_MODE_HELPER(jitNewObjectNoZeroInit,1)
DUAL_MODE_HELPER(jitANewArray,2)
DUAL_MODE_HELPER(jitANewArrayNoZeroInit,2)
DUAL_MODE_HELPER(jitNewArray,2)
DUAL_MODE_HELPER(jitNewArrayNoZeroInit,2)
SLOW_PATH_ONLY_HELPER(jitAMultiNewArray,3)

dnl jitStackOverflow is special case in that the frame size argument
dnl is implicit (in the register file) rather than being passed as
dnl an argument in the usual way.

SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitStackOverflow,0)

SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitCheckAsyncMessages,0)
DUAL_MODE_HELPER_NO_RETURN_VALUE(jitCheckCast,2)
DUAL_MODE_HELPER_NO_RETURN_VALUE(jitCheckCastForArrayStore,2)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitCheckIfFinalizeObject,1)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitCollapseJNIReferenceFrame,0)
FAST_PATH_ONLY_HELPER(jitInstanceOf,2)
DUAL_MODE_HELPER_NO_RETURN_VALUE(jitMethodMonitorEntry,1)
DUAL_MODE_HELPER_NO_RETURN_VALUE(jitMonitorEntry,1)
DUAL_MODE_HELPER_NO_RETURN_VALUE(jitMethodMonitorExit,1)
DUAL_MODE_HELPER_NO_RETURN_VALUE(jitMonitorExit,1)
SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitReportMethodEnter,2)
SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitReportStaticMethodEnter,1)
SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitReportMethodExit,2)
DUAL_MODE_HELPER_NO_RETURN_VALUE(jitTypeCheckArrayStore,2)
DUAL_MODE_HELPER_NO_RETURN_VALUE(jitTypeCheckArrayStoreWithNullCheck,2)
FAST_PATH_ONLY_HELPER(jitObjectHashCode,1)
SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitReportFinalFieldModified,1)
SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitReportInstanceFieldRead,2)
SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitReportInstanceFieldWrite,3)
SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitReportStaticFieldRead,1)
SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitReportStaticFieldWrite,2)

dnl Trap handlers

TRAP_HANDLER(jitHandleArrayIndexOutOfBoundsTrap,0)
TRAP_HANDLER(jitHandleIntegerDivideByZeroTrap,0)
TRAP_HANDLER(jitHandleNullPointerExceptionTrap,0)
TRAP_HANDLER(jitHandleInternalErrorTrap,0)

dnl Only called from PicBuilder

PICBUILDER_FAST_PATH_ONLY_HELPER(jitMethodIsNative,1)
PICBUILDER_FAST_PATH_ONLY_HELPER(jitMethodIsSync,1)
PICBUILDER_FAST_PATH_ONLY_HELPER(jitResolvedFieldIsVolatile,3)
PICBUILDER_DUAL_MODE_HELPER(jitLookupInterfaceMethod,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveString,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveClass,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveClassFromStaticField,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveField,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveFieldSetter,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveStaticField,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveStaticFieldSetter,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveInterfaceMethod,2)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveSpecialMethod,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveStaticMethod,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveVirtualMethod,2)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveMethodType,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveMethodHandle,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveInvokeDynamic,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveConstantDynamic,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveHandleMethod,3)

dnl Direct call field resolve helpers

SLOW_PATH_ONLY_HELPER(jitResolveFieldDirect,2)
SLOW_PATH_ONLY_HELPER(jitResolveFieldSetterDirect,2)
SLOW_PATH_ONLY_HELPER(jitResolveStaticFieldDirect,2)
SLOW_PATH_ONLY_HELPER(jitResolveStaticFieldSetterDirect,2)

dnl Recompilation helpers

SLOW_PATH_ONLY_HELPER(jitRetranslateCaller,2)
SLOW_PATH_ONLY_HELPER(jitRetranslateCallerWithPreparation,3)
SLOW_PATH_ONLY_HELPER(jitRetranslateMethod,3)

dnl Exception throw helpers

EXCEPTION_THROW_HELPER(jitThrowCurrentException,0)
EXCEPTION_THROW_HELPER(jitThrowException,1)
EXCEPTION_THROW_HELPER(jitThrowAbstractMethodError,0)
EXCEPTION_THROW_HELPER(jitThrowArithmeticException,0)
EXCEPTION_THROW_HELPER(jitThrowArrayIndexOutOfBounds,0)
EXCEPTION_THROW_HELPER(jitThrowArrayStoreException,0)
EXCEPTION_THROW_HELPER(jitThrowArrayStoreExceptionWithIP,0)
EXCEPTION_THROW_HELPER(jitThrowExceptionInInitializerError,0)
EXCEPTION_THROW_HELPER(jitThrowIllegalAccessError,0)
EXCEPTION_THROW_HELPER(jitThrowIncompatibleClassChangeError,0)
EXCEPTION_THROW_HELPER(jitThrowInstantiationException,0)
EXCEPTION_THROW_HELPER(jitThrowNullPointerException,0)
EXCEPTION_THROW_HELPER(jitThrowWrongMethodTypeException,0)
EXCEPTION_THROW_HELPER(jitThrowIncompatibleReceiver,2)

dnl Write barrier helpers

FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierBatchStore,1)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierBatchStoreWithRange,3)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierJ9ClassBatchStore,1)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierJ9ClassStore,2)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierStore,2)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierStoreGenerational,2)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierStoreGenerationalAndConcurrentMark,2)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierClassStoreMetronome,3)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierStoreMetronome,3)

dnl Misc

SLOW_PATH_ONLY_HELPER(jitInduceOSRAtCurrentPC,0)
SLOW_PATH_ONLY_HELPER(jitInduceOSRAtCurrentPCAndRecompile,0)
SLOW_PATH_ONLY_HELPER(jitNewInstanceImplAccessCheck,3)
SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(jitCallCFunction,3)
SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(jitCallJitAddPicToPatchOnClassUnload,2)

UNUSED(jitNewDerivedPackedObject)
UNUSED(jitNewDerivedPackedArray)
UNUSED(jitPackedResolveField)
UNUSED(jitPackedResolveFieldSetter)
UNUSED(jitNewPackedArray)
UNUSED(jitResolvePackedArrayFieldLength)
UNUSED(jitResolveIsPackedFieldNested)
UNUSED(jitNewObjectNoTenantInit)
UNUSED(jitPostJNICallOffloadCheck)
UNUSED(jitPreJNICallOffloadCheck)
UNUSED(jitFindFieldSignatureClass)
UNUSED(icallVMprJavaSendInvokeWithArgumentsHelperL)
UNUSED(j2iInvokeWithArguments)

dnl Switch from the C stack to the java stack and jump via tempSlot

START_PROC(jitRunOnJavaStack)
	SWITCH_TO_JAVA_STACK
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitRunOnJavaStack)

dnl When the VM access helpers are called, the java SP is already stored in the J9VMThread

	DECLARE_EXTERN(fast_jitAcquireVMAccess)
	DECLARE_EXTERN(fast_jitReleaseVMAccess)

BEGIN_HELPER(jitAcquireVMAccess)
	SAVE_ALL_REGS
	mov x0,J9VMTHREAD
	CALL_DIRECT(fast_jitAcquireVMAccess)
	RESTORE_ALL_REGS
	RESTORE_FPLR
END_HELPER(jitAcquireVMAccess)

BEGIN_HELPER(jitReleaseVMAccess)
	SAVE_ALL_REGS
	mov x0,J9VMTHREAD
	CALL_DIRECT(fast_jitReleaseVMAccess)
	RESTORE_ALL_REGS
	RESTORE_FPLR
END_HELPER(jitReleaseVMAccess)

START_PROC(cInterpreterFromJIT)
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
	str x1,[J9VMTHREAD,{#}J9TR_VMThread_returnValue2]
	ldr x0,[J9VMTHREAD,{#}J9TR_VMThread_javaVM]
	ldr x0,[x0,{#}J9TR_JavaVM_cInterpreter]
	br x0
END_PROC(cInterpreterFromJIT)

BEGIN_RETURN_POINT(jitExitInterpreter0)
END_RETURN_POINT(jitExitInterpreter0)

BEGIN_RETURN_POINT(jitExitInterpreter1)
	ldr w0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
END_RETURN_POINT(jitExitInterpreter1)

BEGIN_RETURN_POINT(jitExitInterpreterF)
	ldr s0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
END_RETURN_POINT(jitExitInterpreterF)

BEGIN_RETURN_POINT(jitExitInterpreterD)
	ldr d0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
END_RETURN_POINT(jitExitInterpreterD)

	DECLARE_EXTERN(old_slow_jitInterpretNewInstanceMethod)
	DECLARE_EXTERN(old_slow_jitTranslateNewInstanceMethod)

dnl Non-standard - Called via an invoke - arguments are reversed on stack and doesn't return to caller right away
BEGIN_HELPER(jitInterpretNewInstanceMethod)
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(jitInterpretNewInstanceMethod)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitInterpretNewInstanceMethod)

dnl Non-standard - Called via an invoke - arguments are reversed on stack and doesn't return to caller right away
BEGIN_HELPER(jitTranslateNewInstanceMethod)
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(jitInterpretNewInstanceMethod)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitTranslateNewInstanceMethod)

START_PROC(returnFromJITConstructor0)
	SWITCH_TO_C_STACK
	CINTERP(J9TR_bcloop_return_from_jit_ctor, 0)
END_PROC(returnFromJITConstructor0)

START_PROC(returnFromJIT0)
	SWITCH_TO_C_STACK
	CINTERP(J9TR_bcloop_return_from_jit, 0)
END_PROC(returnFromJIT0)

START_PROC(returnFromJIT1)
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_floatTemp1]
	SWITCH_TO_C_STACK
	CINTERP(J9TR_bcloop_return_from_jit, 1)
END_PROC(returnFromJIT1)

START_PROC(returnFromJITF)
	str s0,[J9VMTHREAD,{#}J9TR_VMThread_floatTemp1]
	SWITCH_TO_C_STACK
	CINTERP(J9TR_bcloop_return_from_jit, 1)
END_PROC(returnFromJITF)

START_PROC(returnFromJITD)
	str d0,[J9VMTHREAD,{#}J9TR_VMThread_floatTemp1]
	SWITCH_TO_C_STACK
	CINTERP(J9TR_bcloop_return_from_jit, 2)
END_PROC(returnFromJITD)

dnl j2iInvokeExact
dnl All arguments (including the MethodHandle) are on stack
dnl Return address is in LR
dnl JIT preserved registers are live
dnl x0 contains the method to run

START_PROC(j2iInvokeExact)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	SAVE_FPLR
	mov x1,x0
	mov x0,{#}J9TR_bcloop_j2i_invoke_exact
	b FUNC_LABEL(cInterpreterFromJIT)
END_PROC(j2iInvokeExact)

dnl j2iTransition
dnl All arguments are on stack
dnl Return address is in LR
dnl JIT preserved registers are live
dnl x0 contains the method to run

START_PROC(j2iTransition)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	SAVE_FPLR
	mov x1,x0
	mov x0,{#}J9TR_bcloop_j2i_transition
	b FUNC_LABEL(cInterpreterFromJIT)
END_PROC(j2iTransition)

dnl j2iVirtual
dnl All arguments are on stack
dnl Return address is in LR
dnl JIT preserved registers are live
dnl x0 contains the receiver
dnl x9 contains the vTable index

START_PROC(j2iVirtual)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	SAVE_FPLR
	str x9,[J9VMTHREAD,{#}J9TR_VMThread_tempSlot]
	mov x1,x0
	mov x0,{#}J9TR_bcloop_j2i_virtual
	b FUNC_LABEL(cInterpreterFromJIT)
END_PROC(j2iVirtual)

	DECLARE_EXTERN(old_slow_icallVMprJavaSendPatchupVirtual)

dnl icallVMprJavaSendPatchupVirtual
dnl Arguments are in registers/stack per JIT linkage
dnl Return address is in LR
dnl JIT preserved registers are live
dnl x0 contains the receiver
dnl x9 contains the vTable index

BEGIN_HELPER(icallVMprJavaSendPatchupVirtual)
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue2]
	str x9,[J9VMTHREAD,{#}J9TR_VMThread_tempSlot]
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(icallVMprJavaSendPatchupVirtual)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(icallVMprJavaSendPatchupVirtual)

START_PROC(icallVMprJavaSendNativeStatic)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendNativeStatic)

START_PROC(icallVMprJavaSendStatic0)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStatic0)

START_PROC(icallVMprJavaSendStatic1)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStatic1)

START_PROC(icallVMprJavaSendStaticJ)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticJ)

START_PROC(icallVMprJavaSendStaticF)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticF)

START_PROC(icallVMprJavaSendStaticD)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticD)

START_PROC(icallVMprJavaSendStaticL)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticL)

START_PROC(icallVMprJavaSendStaticSync0)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSync0)

START_PROC(icallVMprJavaSendStaticSync1)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSync1)

START_PROC(icallVMprJavaSendStaticSyncJ)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSyncJ)

START_PROC(icallVMprJavaSendStaticSyncF)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSyncF)

START_PROC(icallVMprJavaSendStaticSyncD)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSyncD)

START_PROC(icallVMprJavaSendStaticSyncL)
	b FUNC_LABEL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSyncL)

START_PROC(icallVMprJavaSendNativeVirtual)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendNativeVirtual)

START_PROC(icallVMprJavaSendVirtual0)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtual0)

START_PROC(icallVMprJavaSendVirtual1)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtual1)

START_PROC(icallVMprJavaSendVirtualJ)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualJ)

START_PROC(icallVMprJavaSendVirtualF)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualF)

START_PROC(icallVMprJavaSendVirtualD)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualD)

START_PROC(icallVMprJavaSendVirtualL)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualL)

START_PROC(icallVMprJavaSendVirtualSync0)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSync0)

START_PROC(icallVMprJavaSendVirtualSync1)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSync1)

START_PROC(icallVMprJavaSendVirtualSyncJ)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSyncJ)

START_PROC(icallVMprJavaSendVirtualSyncF)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSyncF)

START_PROC(icallVMprJavaSendVirtualSyncD)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSyncD)

START_PROC(icallVMprJavaSendVirtualSyncL)
	b FUNC_LABEL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSyncL)

START_PROC(icallVMprJavaSendInvokeExact0)
	b FUNC_LABEL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExact0)

START_PROC(icallVMprJavaSendInvokeExact1)
	b FUNC_LABEL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExact1)

START_PROC(icallVMprJavaSendInvokeExactD)
	b FUNC_LABEL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExactD)

START_PROC(icallVMprJavaSendInvokeExactF)
	b FUNC_LABEL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExactF)

START_PROC(icallVMprJavaSendInvokeExactJ)
	b FUNC_LABEL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExactJ)

START_PROC(icallVMprJavaSendInvokeExactL)
	b FUNC_LABEL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExactL)

	DECLARE_EXTERN(c_jitDecompileOnReturn)
	DECLARE_EXTERN(c_jitReportExceptionCatch)
	DECLARE_EXTERN(c_jitDecompileAtExceptionCatch)
	DECLARE_EXTERN(c_jitDecompileAtCurrentPC)
	DECLARE_EXTERN(c_jitDecompileBeforeReportMethodEnter)
	DECLARE_EXTERN(c_jitDecompileBeforeMethodMonitorEnter)
	DECLARE_EXTERN(c_jitDecompileAfterAllocation)
	DECLARE_EXTERN(c_jitDecompileAfterMonitorEnter)

START_PROC(jitDecompileOnReturn0)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	mov x0,{#}0
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_tempSlot]
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileOnReturn0)

START_PROC(jitDecompileOnReturn1)
	str w0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	mov x0,{#}1
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_tempSlot]
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileOnReturn1)

START_PROC(jitDecompileOnReturnF)
	str s0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	mov x0,{#}1
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_tempSlot]
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileOnReturnF)

START_PROC(jitDecompileOnReturnD)
	str d0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	mov x0,{#}2
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_tempSlot]
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileOnReturnD)

START_PROC(jitReportExceptionCatch)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitReportExceptionCatch)
	RESTORE_PRESERVED_REGS
	SWITCH_TO_JAVA_STACK
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitReportExceptionCatch)

START_PROC(jitDecompileAtExceptionCatch)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileAtExceptionCatch)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileAtExceptionCatch)

START_PROC(jitDecompileAtCurrentPC)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileAtCurrentPC)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileAtCurrentPC)

START_PROC(jitDecompileBeforeReportMethodEnter)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileBeforeReportMethodEnter)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileBeforeReportMethodEnter)

START_PROC(jitDecompileBeforeMethodMonitorEnter)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileBeforeMethodMonitorEnter)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileBeforeMethodMonitorEnter)

START_PROC(jitDecompileAfterAllocation)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileAfterAllocation)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileAfterAllocation)

START_PROC(jitDecompileAfterMonitorEnter)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileAfterMonitorEnter)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileAfterMonitorEnter)

START_PROC(executeCurrentBytecodeFromJIT)
	CINTERP(J9TR_bcloop_execute_bytecode, 0)
END_PROC(executeCurrentBytecodeFromJIT)

START_PROC(handlePopFramesFromJIT)
	CINTERP(J9TR_bcloop_handle_pop_frames, 0)
END_PROC(handlePopFramesFromJIT)

START_PROC(throwCurrentExceptionFromJIT)
	CINTERP(J9TR_bcloop_throw_current_exception, 0)
END_PROC(throwCurrentExceptionFromJIT)

START_PROC(enterMethodMonitorFromJIT)
	ldr x1,[J9VMTHREAD,{#}J9TR_VMThread_floatTemp1]
	mov x0,{#}J9TR_bcloop_enter_method_monitor
	b FUNC_LABEL(cInterpreterFromJIT)
END_PROC(enterMethodMonitorFromJIT)

START_PROC(reportMethodEnterFromJIT)
	ldr x1,[J9VMTHREAD,{#}J9TR_VMThread_floatTemp1]
	mov x0,{#}J9TR_bcloop_report_method_enter
	b FUNC_LABEL(cInterpreterFromJIT)
END_PROC(reportMethodEnterFromJIT)

START_PROC(jitFillOSRBufferReturn)
	SWITCH_TO_C_STACK
	CINTERP(J9TR_bcloop_exit_interpreter, 0)
END_PROC(jitFillOSRBufferReturn)

BEGIN_RETURN_POINT(jitExitInterpreterJ)
	ldr x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
END_RETURN_POINT(jitExitInterpreterJ)

START_PROC(returnFromJITJ)
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_floatTemp1]
	SWITCH_TO_C_STACK
	CINTERP(J9TR_bcloop_return_from_jit, 2)
END_PROC(returnFromJITJ)

START_PROC(jitDecompileOnReturnJ)
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	mov x0,{#}2
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_tempSlot]
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileOnReturnJ)

START_PROC(returnFromJITL)
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_floatTemp1]
	SWITCH_TO_C_STACK
	CINTERP(J9TR_bcloop_return_from_jit, 1)
END_PROC(returnFromJITL)

START_PROC(jitDecompileOnReturnL)
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_returnValue]
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	mov x0,{#}1
	str x0,[J9VMTHREAD,{#}J9TR_VMThread_tempSlot]
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_PROC(jitDecompileOnReturnL)
