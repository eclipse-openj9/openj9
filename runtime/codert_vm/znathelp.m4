dnl Copyright (c) 2017, 2019 IBM Corp. and others
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

include(zhelpers.m4)

    FILE_START(znathelp)

define({CALL_SLOW_PATH_ONLY_HELPER},{
    SAVE_ALL_REGS($1)
    ST_GPR r14,J9TR_VMThread_jitReturnAddress(J9VMTHREAD)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C_WITH_VMTHREAD($1, old_slow_$1)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_DONE_,SYM_COUNT))
    br CRINT
PLACE_LABEL(CONCAT(L_DONE_,SYM_COUNT))
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    ST_GPR J9SP,JIT_GPR_SAVE_SLOT(J9SP)
    L_GPR r2,J9TR_VMThread_returnValue(J9VMTHREAD)
    ST_GPR r2,JIT_GPR_SAVE_SLOT(2)
    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK($1)
})

define({CALL_SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE},{
    SAVE_ALL_REGS($1)
    ST_GPR r14,J9TR_VMThread_jitReturnAddress(J9VMTHREAD)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C_WITH_VMTHREAD($1, old_slow_$1)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_DONE_,SYM_COUNT))
    br CRINT
PLACE_LABEL(CONCAT(L_DONE_,SYM_COUNT))
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    ST_GPR J9SP,JIT_GPR_SAVE_SLOT(J9SP)
    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK($1)
})

define({CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION},{
    SAVE_ALL_REGS($1)
    ST_GPR r14,J9TR_VMThread_jitReturnAddress(J9VMTHREAD)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C_WITH_VMTHREAD($1, old_slow_$1)
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    ST_GPR J9SP,JIT_GPR_SAVE_SLOT(J9SP)
    L_GPR r2,J9TR_VMThread_returnValue(J9VMTHREAD)
    ST_GPR r2,JIT_GPR_SAVE_SLOT(2)
    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK($1)
})

define({CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE},{
    SAVE_ALL_REGS($1)
    ST_GPR r14,J9TR_VMThread_jitReturnAddress(J9VMTHREAD)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C_WITH_VMTHREAD($1, old_slow_$1)
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    ST_GPR J9SP,JIT_GPR_SAVE_SLOT(J9SP)
    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK($1)
})

define({SLOW_PATH_ONLY_HELPER},{
BEGIN_HELPER($1)
    CALL_SLOW_PATH_ONLY_HELPER($1)
    br r14
END_CURRENT
})

define({SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE},{
BEGIN_HELPER($1)
    CALL_SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE($1)
    br r14
END_CURRENT
})

define({SLOW_PATH_ONLY_HELPER_NO_EXCEPTION},{
BEGIN_HELPER($1)
    CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION($1)
    br r14
END_CURRENT
})

define({SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE},{
BEGIN_HELPER($1)
    CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE($1)
    br r14
END_CURRENT
})

define({TRAP_HANDLER},{
BEGIN_FUNC($1)
ifdef({J9ZOS390},{
    L_GPR r7,J9TR_VMThread_tempSlot(J9VMTHREAD)
})
    STM_GPR r0,LAST_REG,JIT_GPR_SAVE_SLOT(0)
    SWAP_SSP_VALUE
    SAVE_HIWORD_REGS
    LOAD_CAA
    SAVE_ALL_REGS($1)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C_WITH_VMTHREAD($1, old_slow_$1)
    br CRINT
END_CURRENT
})

define({PICBUILDER_FAST_PATH_ONLY_HELPER},{FAST_PATH_ONLY_HELPER($1)})

define({PICBUILDER_SLOW_PATH_ONLY_HELPER},{SLOW_PATH_ONLY_HELPER($1)})

define({EXCEPTION_THROW_HELPER},{SLOW_PATH_ONLY_HELPER($1)})

define({FAST_PATH_ONLY_HELPER},{
BEGIN_HELPER($1)
    SAVE_C_VOLATILE_REGS($1)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C_WITH_VMTHREAD($1, old_fast_$1)
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    RESTORE_C_VOLATILE_REGS($1)
    L_GPR r2,J9TR_VMThread_returnValue(J9VMTHREAD)
    ST_GPR r2,JIT_GPR_SAVE_SLOT(2)
END_HELPER
})

define({FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE},{
BEGIN_HELPER($1)
    SAVE_C_VOLATILE_REGS($1)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C_WITH_VMTHREAD($1, old_fast_$1)
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    RESTORE_C_VOLATILE_REGS($1)
END_HELPER
})

define({OLD_DUAL_MODE_HELPER},{
BEGIN_HELPER($1)
    SAVE_C_VOLATILE_REGS($1)
    ST_GPR r14,J9TR_VMThread_jitReturnAddress(J9VMTHREAD)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C_WITH_VMTHREAD($1, old_fast_$1)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_DONE_,SYM_COUNT))
    SAVE_C_NONVOLATILE_REGS
    LR_GPR CRA,CRINT
    LR_GPR CARG1,J9VMTHREAD
    CALL_INDIRECT(CRA)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_SLOW_,SYM_COUNT))
    br CRINT
PLACE_LABEL(CONCAT(L_SLOW_,SYM_COUNT))
    RESTORE_C_NONVOLATILE_REGS
PLACE_LABEL(CONCAT(L_DONE_,SYM_COUNT))
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    RESTORE_C_VOLATILE_REGS($1)
    L_GPR r2,J9TR_VMThread_returnValue(J9VMTHREAD)
    ST_GPR r2,JIT_GPR_SAVE_SLOT(2)
END_HELPER
})

define({OLD_DUAL_MODE_HELPER_NO_RETURN_VALUE},{
BEGIN_HELPER($1)
    SAVE_C_VOLATILE_REGS($1)
    ST_GPR r14,J9TR_VMThread_jitReturnAddress(J9VMTHREAD)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C_WITH_VMTHREAD($1, old_fast_$1)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_DONE_,SYM_COUNT))
    SAVE_C_NONVOLATILE_REGS
    LR_GPR CRA,CRINT
    LR_GPR CARG1,J9VMTHREAD
    CALL_INDIRECT(CRA)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_SLOW_,SYM_COUNT))
    br CRINT
PLACE_LABEL(CONCAT(L_SLOW_,SYM_COUNT))
    RESTORE_C_NONVOLATILE_REGS
PLACE_LABEL(CONCAT(L_DONE_,SYM_COUNT))
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    RESTORE_C_VOLATILE_REGS($1)
END_HELPER
})

define({NEW_DUAL_MODE_HELPER},{
BEGIN_FUNC($1)
    RELOAD_SSP
    LOAD_CAA
    ST_GPR r14,JIT_GPR_SAVE_SLOT(14)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C($1, fast_$1)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_DONE_,SYM_COUNT))
    L_GPR r14,JIT_GPR_SAVE_SLOT(14)
    ST_GPR r14,J9TR_VMThread_jitReturnAddress(J9VMTHREAD)
    SAVE_C_NONVOLATILE_GPRS
    SAVE_C_NONVOLATILE_REGS
    LR_GPR CRA,CRINT
    LR_GPR CARG1,J9VMTHREAD
    CALL_INDIRECT(CRA)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_SLOW_,SYM_COUNT))
    br CRINT
PLACE_LABEL(CONCAT(L_SLOW_,SYM_COUNT))
    RESTORE_C_NONVOLATILE_REGS
    RESTORE_C_NONVOLATILE_GPRS
PLACE_LABEL(CONCAT(L_DONE_,SYM_COUNT))
    STORE_SSP
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    L_GPR r14,JIT_GPR_SAVE_SLOT(14)
    L_GPR r2,J9TR_VMThread_returnValue(J9VMTHREAD)
    br r14
END_CURRENT
})

define({NEW_DUAL_MODE_HELPER_NO_RETURN_VALUE},{
BEGIN_FUNC($1)
    RELOAD_SSP
    LOAD_CAA
    ST_GPR r14,JIT_GPR_SAVE_SLOT(14)
    ST_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    CALL_C($1, fast_$1)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_DONE_,SYM_COUNT))
    L_GPR r14,JIT_GPR_SAVE_SLOT(14)
    ST_GPR r14,J9TR_VMThread_jitReturnAddress(J9VMTHREAD)
    SAVE_C_NONVOLATILE_GPRS
    SAVE_C_NONVOLATILE_REGS
    LR_GPR CRA,CRINT
    LR_GPR CARG1,J9VMTHREAD
    CALL_INDIRECT(CRA)
    LHI_GPR r0,0
    CLR_GPR CRINT,r0
    je LABEL_NAME(CONCAT(L_SLOW_,SYM_COUNT))
    br CRINT
PLACE_LABEL(CONCAT(L_SLOW_,SYM_COUNT))
    RESTORE_C_NONVOLATILE_REGS
    RESTORE_C_NONVOLATILE_GPRS
PLACE_LABEL(CONCAT(L_DONE_,SYM_COUNT))
    STORE_SSP
    L_GPR J9SP,J9TR_VMThread_sp(J9VMTHREAD)
    L_GPR r14,JIT_GPR_SAVE_SLOT(14)
    br r14
END_CURRENT
})

define({PICBUILDER_DUAL_MODE_HELPER},{OLD_DUAL_MODE_HELPER($1)})

ifdef({ASM_J9VM_JIT_NEW_DUAL_HELPERS},{

define({DUAL_MODE_HELPER},{NEW_DUAL_MODE_HELPER($1,$2)})
define({DUAL_MODE_HELPER_NO_RETURN_VALUE},{NEW_DUAL_MODE_HELPER_NO_RETURN_VALUE($1,$2)})

},{    dnl ASM_J9VM_JIT_NEW_DUAL_HELPERS

define({DUAL_MODE_HELPER},{OLD_DUAL_MODE_HELPER($1,$2)})
define({DUAL_MODE_HELPER_NO_RETURN_VALUE},{OLD_DUAL_MODE_HELPER_NO_RETURN_VALUE($1,$2)})

})    dnl ASM_J9VM_JIT_NEW_DUAL_HELPERS

define({BEGIN_RETURN_POINT},{
BEGIN_FUNC($1)
})

define({END_RETURN_POINT},{
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_floatTemp1)
END_CURRENT
})

define({CINTERP},{
    LHI_GPR r8,$1
    LHI_GPR r9,$2
    J LABEL_NAME(L_CINTERP)
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
UNUSED(jitFindFieldSignatureClass)
UNUSED(icallVMprJavaSendInvokeWithArgumentsHelperL)
UNUSED(j2iInvokeWithArguments)

dnl Switch from the C stack to the java stack and jump via tempSlot

BEGIN_FUNC(jitRunOnJavaStack)
    SWITCH_TO_JAVA_STACK
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_CURRENT

dnl CARG2 is expected to contain reference field address
BEGIN_HELPER(jitSoftwareReadBarrier)
    SAVE_ALL_REGS(jitSoftwareReadBarrier)

    LR_GPR CARG1,J9VMTHREAD
    L_GPR CRA,J9TR_VMThread_javaVM(J9VMTHREAD)
    L_GPR CRA,J9TR_JavaVM_memoryManagerFunctions(CRA)
    L_GPR CRA,J9TR_J9MemoryManagerFunctions_J9ReadBarrier(CRA)
    CALL_INDIRECT(CRA)

    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK(jitSoftwareReadBarrier)
    br r14
END_CURRENT

dnl When the offload helpers are called,
dnl the java SP is already stored in the J9VMThread.

BEGIN_HELPER(jitPreJNICallOffloadCheck)
ifdef({ASM_J9VM_PORT_ZOS_CEEHDLRSUPPORT},{
    std fpr8,JIT_FPR_SAVE_OFFSET(8)(CSP)
    std fpr9,JIT_FPR_SAVE_OFFSET(9)(CSP)
    std fpr10,JIT_FPR_SAVE_OFFSET(10)(CSP)
    std fpr11,JIT_FPR_SAVE_OFFSET(11)(CSP)
    std fpr12,JIT_FPR_SAVE_OFFSET(12)(CSP)
    std fpr13,JIT_FPR_SAVE_OFFSET(13)(CSP)
    std fpr14,JIT_FPR_SAVE_OFFSET(14)(CSP)
    std fpr15,JIT_FPR_SAVE_OFFSET(15)(CSP)
    stfpc CEEHDLR_FPC_SAVE_OFFSET(CSP)
})
    SAVE_ALL_REGS(jitPreJNICallOffloadCheck)
    LR_GPR CARG1,J9VMTHREAD
    LOAD_LABEL_CONSTANT($1, fast_jitPreJNICallOffloadCheck, CARG2)
    CALL_INDIRECT(CARG2)
    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK(jitPreJNICallOffloadCheck)
    br r14
END_CURRENT

BEGIN_HELPER(jitPostJNICallOffloadCheck)
    SAVE_ALL_REGS(jitPostJNICallOffloadCheck)
    LR_GPR CARG1,J9VMTHREAD
    LOAD_LABEL_CONSTANT($1, fast_jitPostJNICallOffloadCheck, CARG2)
    CALL_INDIRECT(CARG2)
    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK(jitPostJNICallOffloadCheck)
    br r14
END_CURRENT

dnl When the VM access helpers are called,
dnl the java SP is already stored in the J9VMThread.

BEGIN_HELPER(jitAcquireVMAccess)
    SAVE_ALL_REGS(jitAcquireVMAccess)
    LR_GPR CARG1,J9VMTHREAD
    LOAD_LABEL_CONSTANT(jitAcquireVMAccess, fast_jitAcquireVMAccess, CARG2)
    CALL_INDIRECT(CARG2)
    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK(jitAcquireVMAccess)
    br r14
END_CURRENT

BEGIN_HELPER(jitReleaseVMAccess)
    SAVE_ALL_REGS(jitReleaseVMAccess)
    LR_GPR CARG1,J9VMTHREAD
    LOAD_LABEL_CONSTANT(jitReleaseVMAccess, fast_jitReleaseVMAccess, CARG2)
    CALL_INDIRECT(CARG2)
    RESTORE_ALL_REGS_AND_SWITCH_TO_JAVA_STACK(jitReleaseVMAccess)
    br r14
END_CURRENT

BEGIN_FUNC(cInterpreterFromJIT)
PLACE_LABEL(L_CINTERP)
    ST_GPR r8,J9TR_VMThread_returnValue(J9VMTHREAD)
    ST_GPR r9,J9TR_VMThread_returnValue2(J9VMTHREAD)
    L_GPR r14,J9TR_VMThread_javaVM(J9VMTHREAD)
    L_GPR r14,J9TR_JavaVM_cInterpreter(r14)
    br r14
END_CURRENT

BEGIN_RETURN_POINT(jitExitInterpreter0)
END_RETURN_POINT

BEGIN_RETURN_POINT(jitExitInterpreter1)
    L32_GPR r2,J9TR_VMThread_returnValue(J9VMTHREAD)
END_RETURN_POINT

BEGIN_RETURN_POINT(jitExitInterpreterF)
    le fpr0,J9TR_VMThread_returnValue(J9VMTHREAD)
END_RETURN_POINT

BEGIN_RETURN_POINT(jitExitInterpreterD)
    ld fpr0,J9TR_VMThread_returnValue(J9VMTHREAD)
END_RETURN_POINT

dnl Non-standard - Called via an invoke
dnl Arguments are reversed on stack and doesn't
dnl return to caller right away.

BEGIN_HELPER(jitInterpretNewInstanceMethod)
    CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(jitInterpretNewInstanceMethod)
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_CURRENT

dnl Non-standard - Called via an invoke
dnl Arguments are reversed on stack and doesn't
dnl return to caller right away.

BEGIN_HELPER(jitTranslateNewInstanceMethod)
    CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(jitTranslateNewInstanceMethod)
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_CURRENT

BEGIN_FUNC(returnFromJITConstructor0)
    SWITCH_TO_C_STACK
    CINTERP(J9TR_bcloop_return_from_jit_ctor, 0)
END_CURRENT

BEGIN_FUNC(returnFromJIT0)
    SWITCH_TO_C_STACK
    CINTERP(J9TR_bcloop_return_from_jit, 0)
END_CURRENT

BEGIN_FUNC(returnFromJIT1)
    SWITCH_TO_C_STACK
    st r2,J9TR_VMThread_floatTemp1(J9VMTHREAD)
    CINTERP(J9TR_bcloop_return_from_jit, 1)
END_CURRENT

BEGIN_FUNC(returnFromJITF)
    SWITCH_TO_C_STACK
    ste fpr0,J9TR_VMThread_floatTemp1(J9VMTHREAD)
    CINTERP(J9TR_bcloop_return_from_jit, 1)
END_CURRENT

BEGIN_FUNC(returnFromJITD)
    SWITCH_TO_C_STACK
    std fpr0,J9TR_VMThread_floatTemp1(J9VMTHREAD)
    CINTERP(J9TR_bcloop_return_from_jit, 2)
END_CURRENT

dnl j2iInvokeExact
dnl All arguments (including the MethodHandle) are on stack
dnl Return address is in r14
dnl JIT preserved registers are live
dnl r1 contains the method to run

BEGIN_FUNC(j2iInvokeExact)
PLACE_LABEL(L_J2IINVOKEEXACT)
    SWITCH_TO_C_STACK_AND_SAVE_PRESERVED_REGS
    SAVE_LR
    LR_GPR r9,r1
    LHI_GPR r8,J9TR_bcloop_j2i_invoke_exact
    J LABEL_NAME(L_CINTERP)
END_CURRENT

dnl j2iTransition
dnl All arguments are on stack
dnl Return address is in r14
dnl JIT preserved registers are live
dnl r1 contains the method to run

BEGIN_FUNC(j2iTransition)
PLACE_LABEL(L_J2ITRANSITION)
    SWITCH_TO_C_STACK_AND_SAVE_PRESERVED_REGS
    SAVE_LR
    LR_GPR r9,r1
    LHI_GPR r8,J9TR_bcloop_j2i_transition
    J LABEL_NAME(L_CINTERP)
END_CURRENT

dnl j2iVirtual
dnl All arguments are on stack
dnl Return address is in r14
dnl JIT preserved registers are live
dnl r1 contains the receiver
dnl r0 contains the vTable index

BEGIN_FUNC(j2iVirtual)
PLACE_LABEL(L_J2IVIRTUAL)
    SWITCH_TO_C_STACK_AND_SAVE_PRESERVED_REGS
    SAVE_LR
    ST_GPR r0,J9TR_VMThread_tempSlot(J9VMTHREAD)
    LR_GPR r9,r1
    LHI_GPR r8,J9TR_bcloop_j2i_virtual
    J LABEL_NAME(L_CINTERP)
END_CURRENT

dnl icallVMprJavaSendPatchupVirtual
dnl Arguments are in registers/stack per JIT linkage
dnl Return address is in r14
dnl JIT preserved registers are live
dnl r1 contains the receiver
dnl r0 contains the vTable index

BEGIN_HELPER(icallVMprJavaSendPatchupVirtual)
    ST_GPR r1,J9TR_VMThread_returnValue2(J9VMTHREAD)
    ST_GPR r0,J9TR_VMThread_tempSlot(J9VMTHREAD)
    CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(icallVMprJavaSendPatchupVirtual)
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendNativeStatic)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStatic0)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStatic1)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticJ)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticF)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticD)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticL)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticSync0)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticSync1)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticSyncJ)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticSyncF)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticSyncD)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendStaticSyncL)
    J LABEL_NAME(L_J2ITRANSITION)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendNativeVirtual)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtual0)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtual1)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualJ)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualF)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualD)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualL)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualSync0)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualSync1)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualSyncJ)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualSyncF)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualSyncD)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendVirtualSyncL)
    J LABEL_NAME(L_J2IVIRTUAL)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendInvokeExact0)
    J LABEL_NAME(L_J2IINVOKEEXACT)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendInvokeExact1)
    J LABEL_NAME(L_J2IINVOKEEXACT)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendInvokeExactD)
    J LABEL_NAME(L_J2IINVOKEEXACT)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendInvokeExactF)
    J LABEL_NAME(L_J2IINVOKEEXACT)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendInvokeExactJ)
    J LABEL_NAME(L_J2IINVOKEEXACT)
END_CURRENT

BEGIN_FUNC(icallVMprJavaSendInvokeExactL)
    J LABEL_NAME(L_J2IINVOKEEXACT)
END_CURRENT

define({JIT_CALL_DECOMPILER},{
    CALL_C_WITH_VMTHREAD($1, $2)
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
})

define({BEGIN_DECOMPILE_ON_RETURN},{
BEGIN_FUNC($1)
    SWITCH_TO_C_STACK_AND_SAVE_PRESERVED_REGS
    LHI_GPR r0,$2
    ST_GPR r0,J9TR_VMThread_tempSlot(J9VMTHREAD)
})
define({END_DECOMPILE_ON_RETURN},{
    JIT_CALL_DECOMPILER($1, c_jitDecompileOnReturn)
END_CURRENT
})

BEGIN_DECOMPILE_ON_RETURN(jitDecompileOnReturn0, 0)
END_DECOMPILE_ON_RETURN

BEGIN_DECOMPILE_ON_RETURN(jitDecompileOnReturn1, 1)
    st r2,J9TR_VMThread_returnValue(J9VMTHREAD)
END_DECOMPILE_ON_RETURN

BEGIN_DECOMPILE_ON_RETURN(jitDecompileOnReturnF, 1)
    ste fpr0,J9TR_VMThread_returnValue(J9VMTHREAD)
END_DECOMPILE_ON_RETURN

BEGIN_DECOMPILE_ON_RETURN(jitDecompileOnReturnD, 2)
    std fpr0,J9TR_VMThread_returnValue(J9VMTHREAD)
END_DECOMPILE_ON_RETURN

BEGIN_FUNC(jitReportExceptionCatch)
    SWITCH_TO_C_STACK_AND_SAVE_PRESERVED_REGS
    CALL_C_WITH_VMTHREAD(jitReportExceptionCatch, c_jitReportExceptionCatch)
    RESTORE_PRESERVED_REGS_AND_SWITCH_TO_JAVA_STACK
    BRANCH_VIA_VMTHREAD(J9TR_VMThread_tempSlot)
END_CURRENT

BEGIN_FUNC(jitDecompileAtExceptionCatch)
    SWITCH_TO_C_STACK_AND_SAVE_PRESERVED_REGS
    JIT_CALL_DECOMPILER(jitDecompileAtExceptionCatch, c_jitDecompileAtExceptionCatch)
END_CURRENT

BEGIN_FUNC(jitDecompileAtCurrentPC)
    SWITCH_TO_C_STACK
    JIT_CALL_DECOMPILER(jitDecompileAtCurrentPC, c_jitDecompileAtCurrentPC)
END_CURRENT

BEGIN_FUNC(jitDecompileBeforeReportMethodEnter)
    SWITCH_TO_C_STACK
    JIT_CALL_DECOMPILER(jitDecompileBeforeReportMethodEnter, c_jitDecompileBeforeReportMethodEnter)
END_CURRENT

BEGIN_FUNC(jitDecompileBeforeMethodMonitorEnter)
    SWITCH_TO_C_STACK
    JIT_CALL_DECOMPILER(jitDecompileBeforeMethodMonitorEnter, c_jitDecompileBeforeMethodMonitorEnter)
END_CURRENT

BEGIN_FUNC(jitDecompileAfterAllocation)
    SWITCH_TO_C_STACK
    JIT_CALL_DECOMPILER(jitDecompileAfterAllocation, c_jitDecompileAfterAllocation)
END_CURRENT

BEGIN_FUNC(jitDecompileAfterMonitorEnter)
    SWITCH_TO_C_STACK
    JIT_CALL_DECOMPILER(jitDecompileAfterMonitorEnter, c_jitDecompileAfterMonitorEnter)
END_CURRENT

BEGIN_FUNC(handlePopFramesFromJIT)
    CINTERP(J9TR_bcloop_handle_pop_frames, 0)
END_CURRENT

BEGIN_FUNC(throwCurrentExceptionFromJIT)
    CINTERP(J9TR_bcloop_throw_current_exception, 0)
END_CURRENT

BEGIN_FUNC(executeCurrentBytecodeFromJIT)
    CINTERP(J9TR_bcloop_execute_bytecode, 0)
END_CURRENT

BEGIN_FUNC(enterMethodMonitorFromJIT)
    L_GPR r9,J9TR_VMThread_floatTemp1(J9VMTHREAD)
    LHI_GPR r8,J9TR_bcloop_enter_method_monitor
    J LABEL_NAME(L_CINTERP)
END_CURRENT

BEGIN_FUNC(reportMethodEnterFromJIT)
    L_GPR r9,J9TR_VMThread_floatTemp1(J9VMTHREAD)
    LHI_GPR r8,J9TR_bcloop_report_method_enter
    J LABEL_NAME(L_CINTERP)
END_CURRENT

BEGIN_FUNC(jitFillOSRBufferReturn)
    SWITCH_TO_C_STACK
    CINTERP(J9TR_bcloop_exit_interpreter, 0)
END_CURRENT

ifdef({ASM_J9VM_ENV_DATA64},{

BEGIN_RETURN_POINT(jitExitInterpreterJ)
    lg r2,J9TR_VMThread_returnValue(J9VMTHREAD)
END_RETURN_POINT

BEGIN_FUNC(returnFromJITJ)
    SWITCH_TO_C_STACK
    stg r2,J9TR_VMThread_floatTemp1(J9VMTHREAD)
    CINTERP(J9TR_bcloop_return_from_jit, 2)
END_CURRENT

BEGIN_DECOMPILE_ON_RETURN(jitDecompileOnReturnJ, 2)
    stg r2,J9TR_VMThread_returnValue(J9VMTHREAD)
END_DECOMPILE_ON_RETURN

BEGIN_FUNC(returnFromJITL)
    SWITCH_TO_C_STACK
    stg r2,J9TR_VMThread_floatTemp1(J9VMTHREAD)
    CINTERP(J9TR_bcloop_return_from_jit, 1)
END_CURRENT

BEGIN_DECOMPILE_ON_RETURN(jitDecompileOnReturnL, 1)
    stg r2,J9TR_VMThread_returnValue(J9VMTHREAD)
END_DECOMPILE_ON_RETURN

},{

BEGIN_RETURN_POINT(jitExitInterpreterJ)
    l r2,J9TR_VMThread_returnValue(J9VMTHREAD)
    l r3,J9TR_VMThread_returnValue2(J9VMTHREAD)
END_RETURN_POINT

BEGIN_FUNC(returnFromJITJ)
    SWITCH_TO_C_STACK
    st r2,J9TR_VMThread_floatTemp1(J9VMTHREAD)
    st r3,J9TR_VMThread_floatTemp2(J9VMTHREAD)
    CINTERP(J9TR_bcloop_return_from_jit, 2)
END_CURRENT

BEGIN_DECOMPILE_ON_RETURN(jitDecompileOnReturnJ, 2)
    st r2,J9TR_VMThread_returnValue(J9VMTHREAD)
    st r3,J9TR_VMThread_returnValue2(J9VMTHREAD)
END_DECOMPILE_ON_RETURN

FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitVolatileReadLong)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitVolatileWriteLong)

})

    FILE_END
