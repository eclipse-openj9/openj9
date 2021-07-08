dnl Copyright IBM Corp. and others 2017
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
dnl [2] https://openjdk.org/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0

include(xhelpers.m4)

	FILE_START

define({CALL_SLOW_PATH_ONLY_HELPER},{
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_ALL_REGS
	FASTCALL_C_WITH_VMTHREAD(old_slow_$1,1)
	test _rax,_rax
	je SHORT_JMP LABEL(L_done_$1)
	jmp _rax
LABEL(L_done_$1):
	RESTORE_ALL_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
})

define({CALL_SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE},{
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_ALL_REGS
	FASTCALL_C_WITH_VMTHREAD(old_slow_$1,1)
	test _rax,_rax
	je SHORT_JMP LABEL(L_done_$1)
	jmp _rax
LABEL(L_done_$1):
	RESTORE_ALL_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
})

define({CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE},{
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_ALL_REGS
	FASTCALL_C_WITH_VMTHREAD(old_slow_$1,1)
	RESTORE_ALL_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
})

define({CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION},{
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE($1)
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
})

define({SLOW_PATH_ONLY_HELPER},{
	FASTCALL_EXTERN(old_slow_$1,1)
	BEGIN_HELPER($1)
	CALL_SLOW_PATH_ONLY_HELPER($1,$2)
	END_HELPER($1,$2)
})

define({SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE},{
	FASTCALL_EXTERN(old_slow_$1,1)
	BEGIN_HELPER($1)
	CALL_SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE($1,$2)
	END_HELPER($1,$2)
})

define({TRAP_HANDLER},{
	FASTCALL_EXTERN(old_slow_$1,1)
	BEGIN_HELPER($1)
	SWITCH_TO_C_STACK
	SAVE_ALL_REGS
	FASTCALL_C_WITH_VMTHREAD(old_slow_$1,1)
	jmp _rax
	END_HELPER($1,$2)
})

define({PICBUILDER_FAST_PATH_ONLY_HELPER},{FAST_PATH_ONLY_HELPER($1,$2)})

define({PICBUILDER_SLOW_PATH_ONLY_HELPER},{SLOW_PATH_ONLY_HELPER($1,$2)})

define({EXCEPTION_THROW_HELPER},{SLOW_PATH_ONLY_HELPER($1,$2)})

define({SLOW_PATH_ONLY_HELPER_NO_EXCEPTION},{
	FASTCALL_EXTERN(old_slow_$1,1)
	BEGIN_HELPER($1)
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION($1,$2)
	END_HELPER($1,$2)
})

define({SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE},{
	FASTCALL_EXTERN(old_slow_$1,1)
	BEGIN_HELPER($1)
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE($1,$2)
	END_HELPER($1,$2)
})

define({FAST_PATH_ONLY_HELPER},{
	FASTCALL_EXTERN(old_fast_$1,1)
	BEGIN_HELPER($1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	FASTCALL_C_WITH_VMTHREAD(old_fast_$1,1)
	RESTORE_C_VOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
	END_HELPER($1,$2)
})

define({FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE},{
	FASTCALL_EXTERN(old_fast_$1,1)
	BEGIN_HELPER($1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	FASTCALL_C_WITH_VMTHREAD(old_fast_$1,1)
	RESTORE_C_VOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	END_HELPER($1,$2)
})

define({OLD_DUAL_MODE_HELPER},{
	FASTCALL_EXTERN(old_fast_$1,1)
	BEGIN_HELPER($1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	FASTCALL_C_WITH_VMTHREAD(old_fast_$1,1)
	test _rax,_rax
	je LABEL(L_done_$1)
	SAVE_C_NONVOLATILE_REGS
	FASTCALL_INDIRECT_WITH_VMTHREAD(_rax)
	test _rax,_rax
	je SHORT_JMP LABEL(L_slow_$1)
	jmp _rax
LABEL(L_slow_$1):
	RESTORE_C_NONVOLATILE_REGS
LABEL(L_done_$1):
	RESTORE_C_VOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
	END_HELPER($1,$2)
})

define({OLD_DUAL_MODE_HELPER_NO_RETURN_VALUE},{
	FASTCALL_EXTERN(old_fast_$1,1)
	BEGIN_HELPER($1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	FASTCALL_C_WITH_VMTHREAD(old_fast_$1,1)
	test _rax,_rax
	je LABEL(L_done_$1)
	SAVE_C_NONVOLATILE_REGS
	FASTCALL_INDIRECT_WITH_VMTHREAD(_rax)
	test _rax,_rax
	je SHORT_JMP LABEL(L_slow_$1)
	jmp _rax
LABEL(L_slow_$1):
	RESTORE_C_NONVOLATILE_REGS
LABEL(L_done_$1):
	RESTORE_C_VOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	END_HELPER($1,$2)
})

define({NEW_DUAL_MODE_HELPER},{
	FASTCALL_EXTERN(fast_$1,$2+1)
	BEGIN_HELPER($1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	call FASTCALL_SYMBOL(fast_$1,$2+1)
	test _rax,_rax
	jne SHORT_JMP LABEL(L_slow_$1)
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
	ret
LABEL(L_slow_$1):
	SWITCH_TO_C_STACK
	SAVE_C_NONVOLATILE_REGS
	FASTCALL_INDIRECT_WITH_VMTHREAD(_rax)
	test _rax,_rax
	jne SHORT_JMP LABEL(L_exception_$1)
	RESTORE_C_NONVOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
	ret
LABEL(L_exception_$1):
	jmp _rax
	END_PROC($1)
})

define({NEW_DUAL_MODE_HELPER_NO_RETURN_VALUE},{
	FASTCALL_EXTERN(fast_$1,$2+1)
	BEGIN_HELPER($1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	call FASTCALL_SYMBOL(fast_$1,$2+1)
	test _rax,_rax
	jne SHORT_JMP LABEL(L_slow_$1)
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	ret
LABEL(L_slow_$1):
	SWITCH_TO_C_STACK
	SAVE_C_NONVOLATILE_REGS
	FASTCALL_INDIRECT_WITH_VMTHREAD(_rax)
	test _rax,_rax
	jne SHORT_JMP LABEL(L_exception_$1)
	RESTORE_C_NONVOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	ret
LABEL(L_exception_$1):
	jmp _rax
	END_PROC($1)
})

ifdef({ASM_J9VM_ENV_DATA64},{

dnl All arguments are register-based.

define({NEW_DUAL_MODE_ALLOCATION_HELPER},{
	FASTCALL_EXTERN(fast_$1,$2+1)
	BEGIN_HELPER($1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	call FASTCALL_SYMBOL(fast_$1,$2+1)
	test _rax,_rax
	jne SHORT_JMP LABEL(L_slow_$1)
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
	ret
LABEL(L_slow_$1):
	SAVE_C_NONVOLATILE_REGS
	FASTCALL_INDIRECT_WITH_VMTHREAD(_rax)
	test _rax,_rax
	jne SHORT_JMP LABEL(L_exception_$1)
	RESTORE_C_NONVOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
	ret
LABEL(L_exception_$1):
	jmp _rax
	END_PROC($1)
})

},{	dnl ASM_J9VM_ENV_DATA64

dnl Arguments are mixed register/stack (fastcall).
dnl First two args can stay in their registers.
dnl If there is a third arg, pop it from the java
dnl stack and push it to the C stack before the call
dnl (the call will remove it from the C stack).
dnl
dnl No allocation helper has more than 2 args
dnl after the current thread arg.

define({NEW_DUAL_MODE_ALLOCATION_HELPER},{
	FASTCALL_EXTERN(fast_$1,$2+1)
	BEGIN_HELPER($1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	ifelse($2,0,,$2,1,,$2,2,{pop eax},{ERROR more than 2 args})
	SWITCH_TO_C_STACK
	ifelse($2,0,,$2,1,,$2,2,{push eax})
	call FASTCALL_SYMBOL(fast_$1,$2+1)
	test _rax,_rax
	jne SHORT_JMP LABEL(L_slow_$1)
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
	ret
LABEL(L_slow_$1):
	SAVE_C_NONVOLATILE_REGS
	FASTCALL_INDIRECT_WITH_VMTHREAD(_rax)
	test _rax,_rax
	jne SHORT_JMP LABEL(L_exception_$1)
	RESTORE_C_NONVOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	mov _rax,uword ptr J9TR_VMThread_returnValue[_rbp]
	ret
LABEL(L_exception_$1):
	jmp _rax
	END_PROC($1)
})

})	dnl ASM_J9VM_ENV_DATA64


define({PICBUILDER_DUAL_MODE_HELPER},{OLD_DUAL_MODE_HELPER($1,$2)})

ifdef({ASM_J9VM_JIT_NEW_DUAL_HELPERS},{

define({DUAL_MODE_ALLOCATION_HELPER},{NEW_DUAL_MODE_ALLOCATION_HELPER($1,$2)})
define({DUAL_MODE_HELPER},{NEW_DUAL_MODE_HELPER($1,$2)})
define({DUAL_MODE_HELPER_NO_RETURN_VALUE},{NEW_DUAL_MODE_HELPER_NO_RETURN_VALUE($1,$2)})

},{	dnl ASM_J9VM_JIT_NEW_DUAL_HELPERS

define({DUAL_MODE_ALLOCATION_HELPER},{OLD_DUAL_MODE_HELPER($1,$2)})
define({DUAL_MODE_HELPER},{OLD_DUAL_MODE_HELPER($1,$2)})
define({DUAL_MODE_HELPER_NO_RETURN_VALUE},{OLD_DUAL_MODE_HELPER_NO_RETURN_VALUE($1,$2)})

})	dnl ASM_J9VM_JIT_NEW_DUAL_HELPERS

define({BEGIN_RETURN_POINT},{
	START_PROC($1)
})

define({END_RETURN_POINT},{
	push uword ptr J9TR_VMThread_floatTemp1[_rbp]
	ret
	END_PROC($1)
})

define({UNUSED},{
START_PROC($1)
	mov _rax,-1
	mov uword ptr [_rax],_rax
END_PROC($1)
})

define({CINTERP},{
	mov _rax,uword ptr J9TR_VMThread_javaVM[_rbp]
	jmp uword ptr J9TR_JavaVM_cInterpreter[_rax]
})

dnl Helpers that are at a method invocation point.
dnl
dnl See definition of SAVE_C_VOLATILE_REGS in xhelpers.m4
dnl for details.

define({METHOD_INVOCATION})

PICBUILDER_DUAL_MODE_HELPER(jitLookupInterfaceMethod,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveInterfaceMethod,2)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveSpecialMethod,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveStaticMethod,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveVirtualMethod,2)
SLOW_PATH_ONLY_HELPER(jitInduceOSRAtCurrentPC,0)
SLOW_PATH_ONLY_HELPER(jitInduceOSRAtCurrentPCAndRecompile,0)
SLOW_PATH_ONLY_HELPER(jitRetranslateMethod,3)

dnl jitStackOverflow is special case in that the frame size argument
dnl is implicit (in the register file) rather than being passed as
dnl an argument in the usual way.

SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitStackOverflow,0)

dnl Helpers that are not at a method invocation point.
dnl
dnl See definition of SAVE_C_VOLATILE_REGS in xhelpers.m4
dnl for details.

undefine({METHOD_INVOCATION})

dnl Runtime helpers

DUAL_MODE_ALLOCATION_HELPER(jitNewValue,1)
DUAL_MODE_ALLOCATION_HELPER(jitNewValueNoZeroInit,1)
DUAL_MODE_ALLOCATION_HELPER(jitNewObject,1)
DUAL_MODE_ALLOCATION_HELPER(jitNewObjectNoZeroInit,1)
DUAL_MODE_ALLOCATION_HELPER(jitANewArray,2)
DUAL_MODE_HELPER(jitANewArrayNoZeroInit,2)
DUAL_MODE_ALLOCATION_HELPER(jitNewArray,2)
DUAL_MODE_ALLOCATION_HELPER(jitNewArrayNoZeroInit,2)
SLOW_PATH_ONLY_HELPER(jitAMultiNewArray,3)

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
FAST_PATH_ONLY_HELPER(jitAcmpeqHelper,2)
FAST_PATH_ONLY_HELPER(jitAcmpneHelper,2)
OLD_DUAL_MODE_HELPER(jitGetFlattenableField,2)
OLD_DUAL_MODE_HELPER(jitCloneValueType, 1)
OLD_DUAL_MODE_HELPER(jitWithFlattenableField,3)
OLD_DUAL_MODE_HELPER_NO_RETURN_VALUE(jitPutFlattenableField,3)
OLD_DUAL_MODE_HELPER(jitGetFlattenableStaticField,2)
OLD_DUAL_MODE_HELPER_NO_RETURN_VALUE(jitPutFlattenableStaticField,3)
OLD_DUAL_MODE_HELPER(jitLoadFlattenableArrayElement,2)
OLD_DUAL_MODE_HELPER_NO_RETURN_VALUE(jitStoreFlattenableArrayElement,3)
SLOW_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitResolveFlattenableField,3)

ifdef({ASM_J9VM_OPT_OPENJDK_METHODHANDLE},{
OLD_DUAL_MODE_HELPER(jitLookupDynamicPublicInterfaceMethod,2)
}) dnl ASM_J9VM_OPT_OPENJDK_METHODHANDLE

dnl Trap handlers

TRAP_HANDLER(jitHandleArrayIndexOutOfBoundsTrap,0)
TRAP_HANDLER(jitHandleIntegerDivideByZeroTrap,0)
TRAP_HANDLER(jitHandleNullPointerExceptionTrap,0)
TRAP_HANDLER(jitHandleInternalErrorTrap,0)

dnl Only called from PicBuilder

PICBUILDER_FAST_PATH_ONLY_HELPER(jitMethodIsNative,1)
PICBUILDER_FAST_PATH_ONLY_HELPER(jitMethodIsSync,1)
PICBUILDER_FAST_PATH_ONLY_HELPER(jitResolvedFieldIsVolatile,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveString,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveClass,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveClassFromStaticField,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveField,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveFieldSetter,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveStaticField,3)
PICBUILDER_SLOW_PATH_ONLY_HELPER(jitResolveStaticFieldSetter,3)
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

dnl Exception throw helpers

EXCEPTION_THROW_HELPER(jitThrowCurrentException,0)
EXCEPTION_THROW_HELPER(jitThrowException,1)
EXCEPTION_THROW_HELPER(jitThrowUnreportedException,1)
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
EXCEPTION_THROW_HELPER(jitThrowIdentityException,0)

dnl Write barrier helpers

define({JIT_WRITE_BARRIER_WRAPPER},{
	FASTCALL_EXTERN(fast_$1,$2+1)
	BEGIN_HELPER($1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	mov PARM_REG(1),_rbp
	mov PARM_REG(2),uword ptr J9TR_VMThread_floatTemp1[_rbp]
	ifelse($2, 2, {
		ifdef({ASM_J9VM_ENV_DATA64},{
			mov PARM_REG(3),uword ptr J9TR_VMThread_floatTemp2[_rbp]
		}, {
			push uword ptr J9TR_VMThread_floatTemp2[_rbp]
		})
	})
	call FASTCALL_SYMBOL(fast_$1,$2+1)
	RESTORE_C_VOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	dnl parms are passed via fields of VM thread instead of register or stack
	dnl therefore, the helper's parm count is 0
	END_HELPER($1,0)
})

JIT_WRITE_BARRIER_WRAPPER(jitWriteBarrierBatchStore,1)
UNUSED(jitWriteBarrierBatchStoreWithRange,3)
UNUSED(jitWriteBarrierJ9ClassBatchStore,1)
UNUSED(jitWriteBarrierJ9ClassStore,2)
JIT_WRITE_BARRIER_WRAPPER(jitWriteBarrierStore,2)
JIT_WRITE_BARRIER_WRAPPER(jitWriteBarrierStoreGenerational,2)
JIT_WRITE_BARRIER_WRAPPER(jitWriteBarrierStoreGenerationalAndConcurrentMark,2)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierClassStoreMetronome,3)
FAST_PATH_ONLY_HELPER_NO_RETURN_VALUE(jitWriteBarrierStoreMetronome,3)

dnl Misc

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
UNUSED(j2iInvokeWithArguments)

START_PROC(jitThrowClassCastException)
	FASTCALL_EXTERN(impl_jitClassCastException,1)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	pop uword ptr J9TR_VMThread_floatTemp1[_rbp]
	pop uword ptr J9TR_VMThread_floatTemp2[_rbp]
	SWITCH_TO_C_STACK
	SAVE_ALL_REGS
	FASTCALL_C_WITH_VMTHREAD(impl_jitClassCastException)
	jmp _rax
END_PROC(jitThrowClassCastException)

dnl Switch from the C stack to the java stack and jump via tempSlot

START_PROC(jitRunOnJavaStack)
	SWITCH_TO_JAVA_STACK
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitRunOnJavaStack)

	FASTCALL_EXTERN(fast_jitAcquireVMAccess,1)

dnl Non-standard - _RSP points to C stack when this is called
BEGIN_HELPER(jitAcquireVMAccess)
dnl Ensure _rsp is pointing to the C interpreter stack frame
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SAVE_ALL_REGS
	FASTCALL_C_WITH_VMTHREAD(fast_jitAcquireVMAccess)
	RESTORE_ALL_REGS
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
END_HELPER(jitAcquireVMAccess,0)

ifdef({OMIT},{
dnl currently declared by JIT, but could move here

	FASTCALL_EXTERN(fast_jitReleaseVMAccess)

dnl Non-standard - _RSP points to C stack when this is called
dnl Extra JNI arguments may be pushed on the stack, so the code
dnl below is incorrect.
BEGIN_HELPER(jitReleaseVMAccess)
dnl Ensure _rsp is pointing to the C interpreter stack frame
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SAVE_ALL_REGS
	FASTCALL_C_WITH_VMTHREAD(fast_jitReleaseVMAccess)
	RESTORE_ALL_REGS
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
END_HELPER(jitReleaseVMAccess,0)
})

	FASTCALL_EXTERN(old_slow_jitInterpretNewInstanceMethod,1)
	FASTCALL_EXTERN(old_slow_jitTranslateNewInstanceMethod,1)

dnl Non-standard - Called via an invoke - arguments are reversed on stack and doesn't return to caller right away
BEGIN_HELPER(jitInterpretNewInstanceMethod)
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(jitInterpretNewInstanceMethod)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_HELPER(jitInterpretNewInstanceMethod,0)

dnl Non-standard - Called via an invoke - arguments are reversed on stack and doesn't return to caller right away
BEGIN_HELPER(jitTranslateNewInstanceMethod)
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(jitTranslateNewInstanceMethod)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_HELPER(jitTranslateNewInstanceMethod,0)

ifelse(eval(ASM_JAVA_SPEC_VERSION >= 24), 1, {
BEGIN_RETURN_POINT(jitExitInterpreter0RestoreAll)
dnl java stack is active on entry
	SWITCH_TO_C_STACK
	RESTORE_ALL_REGS
	SWITCH_TO_JAVA_STACK
END_RETURN_POINT(jitExitInterpreter0RestoreAll)
}) dnl jitExitInterpreter0RestoreAll is only supported on JAVA 24+

BEGIN_RETURN_POINT(jitExitInterpreter0)
END_RETURN_POINT(jitExitInterpreter0)

BEGIN_RETURN_POINT(jitExitInterpreter1)
	mov eax,dword ptr J9TR_VMThread_returnValue[_rbp]
END_RETURN_POINT(jitExitInterpreter1)

BEGIN_RETURN_POINT(jitExitInterpreterF)
	movss xmm0,dword ptr J9TR_VMThread_returnValue[_rbp]
END_RETURN_POINT(jitExitInterpreterF)

BEGIN_RETURN_POINT(jitExitInterpreterD)
	movq xmm0,qword ptr J9TR_VMThread_returnValue[_rbp]
END_RETURN_POINT(jitExitInterpreterD)

START_PROC(returnFromJITConstructor0)
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_return_from_jit_ctor
	SWITCH_TO_C_STACK
	CINTERP
END_PROC(returnFromJITConstructor0)

START_PROC(returnFromJIT0)
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_return_from_jit
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],0
	SWITCH_TO_C_STACK
	CINTERP
END_PROC(returnFromJIT0)

START_PROC(returnFromJIT1)
	mov dword ptr J9TR_VMThread_floatTemp1[_rbp],eax
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_return_from_jit
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],1
	SWITCH_TO_C_STACK
	CINTERP
END_PROC(returnFromJIT1)

START_PROC(returnFromJITF)
	movd dword ptr J9TR_VMThread_floatTemp1[_rbp],xmm0
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_return_from_jit
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],1
	SWITCH_TO_C_STACK
	CINTERP
END_PROC(returnFromJITF)

START_PROC(returnFromJITD)
	movq qword ptr J9TR_VMThread_floatTemp1[_rbp],xmm0
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_return_from_jit
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],2
	SWITCH_TO_C_STACK
	CINTERP
END_PROC(returnFromJITD)

dnl j2iInvokeExact
dnl All arguments (including the MethodHandle) are on stack, with the return address pushed on top.
dnl JIT preserved registers are live
dnl _RAX contains the MethodHandle

START_PROC(j2iInvokeExact)
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],_rax
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_j2i_invoke_exact
	CINTERP
END_PROC(j2iInvokeExact)

dnl j2iTransition
dnl All arguments are on stack, with the return address pushed on top.
dnl JIT preserved registers are live
dnl _RDI contains the method to run

START_PROC(j2iTransition)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_j2i_transition
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],_rdi
	CINTERP
END_PROC(j2iTransition)

dnl j2iVirtual
dnl All arguments are on stack, with the return address pushed on top.
dnl JIT preserved registers are live
dnl _RAX contains the receiver
dnl On 32-bit, EDX contains the vTableIndex for interface sends
dnl On 64-bit, R8  contains the vTableIndex for interface sends

START_PROC(j2iVirtual)
	SWITCH_TO_C_STACK
	STORE_VIRTUAL_REGISTERS
	SAVE_PRESERVED_REGS
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_j2i_virtual
	CINTERP
END_PROC(j2iVirtual)

START_PROC(icallVMprJavaSendNativeStatic)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendNativeStatic)

START_PROC(icallVMprJavaSendStatic0)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStatic0)

START_PROC(icallVMprJavaSendStatic1)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStatic1)

START_PROC(icallVMprJavaSendStaticJ)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticJ)

START_PROC(icallVMprJavaSendStaticF)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticF)

START_PROC(icallVMprJavaSendStaticD)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticD)

START_PROC(icallVMprJavaSendStaticSync0)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSync0)

START_PROC(icallVMprJavaSendStaticSync1)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSync1)

START_PROC(icallVMprJavaSendStaticSyncJ)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSyncJ)

START_PROC(icallVMprJavaSendStaticSyncF)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSyncF)

START_PROC(icallVMprJavaSendStaticSyncD)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSyncD)

START_PROC(icallVMprJavaSendNativeVirtual)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendNativeVirtual)

START_PROC(icallVMprJavaSendVirtual0)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtual0)

START_PROC(icallVMprJavaSendVirtual1)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtual1)

START_PROC(icallVMprJavaSendVirtualJ)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualJ)

START_PROC(icallVMprJavaSendVirtualF)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualF)

START_PROC(icallVMprJavaSendVirtualD)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualD)

START_PROC(icallVMprJavaSendVirtualSync0)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSync0)

START_PROC(icallVMprJavaSendVirtualSync1)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSync1)

START_PROC(icallVMprJavaSendVirtualSyncJ)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSyncJ)

START_PROC(icallVMprJavaSendVirtualSyncF)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSyncF)

START_PROC(icallVMprJavaSendVirtualSyncD)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSyncD)

START_PROC(icallVMprJavaSendInvokeExact0)
	jmp GLOBAL_SYMBOL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExact0)

START_PROC(icallVMprJavaSendInvokeExact1)
	jmp GLOBAL_SYMBOL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExact1)

START_PROC(icallVMprJavaSendInvokeExactD)
	jmp GLOBAL_SYMBOL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExactD)

START_PROC(icallVMprJavaSendInvokeExactF)
	jmp GLOBAL_SYMBOL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExactF)

START_PROC(icallVMprJavaSendInvokeExactJ)
	jmp GLOBAL_SYMBOL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExactJ)

START_PROC(icallVMprJavaSendInvokeExactL)
	jmp GLOBAL_SYMBOL(j2iInvokeExact)
END_PROC(icallVMprJavaSendInvokeExactL)

START_PROC(icallVMprJavaSendStaticL)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticL)

START_PROC(icallVMprJavaSendStaticSyncL)
	jmp GLOBAL_SYMBOL(j2iTransition)
END_PROC(icallVMprJavaSendStaticSyncL)

START_PROC(icallVMprJavaSendVirtualL)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualL)

START_PROC(icallVMprJavaSendVirtualSyncL)
	jmp GLOBAL_SYMBOL(j2iVirtual)
END_PROC(icallVMprJavaSendVirtualSyncL)

	DECLARE_EXTERN(c_jitDecompileOnReturn)
	DECLARE_EXTERN(c_jitReportExceptionCatch)
	DECLARE_EXTERN(c_jitDecompileAtExceptionCatch)
	DECLARE_EXTERN(c_jitDecompileAtCurrentPC)
	DECLARE_EXTERN(c_jitDecompileBeforeReportMethodEnter)
	DECLARE_EXTERN(c_jitDecompileBeforeMethodMonitorEnter)
	DECLARE_EXTERN(c_jitDecompileAfterAllocation)
	DECLARE_EXTERN(c_jitDecompileAfterMonitorEnter)

START_PROC(jitDecompileOnReturn0)
	mov uword ptr J9TR_VMThread_tempSlot[_rbp],0
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileOnReturn0)

START_PROC(jitDecompileOnReturn1)
	mov dword ptr J9TR_VMThread_returnValue[_rbp],eax
	mov uword ptr J9TR_VMThread_tempSlot[_rbp],1
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileOnReturn1)

START_PROC(jitDecompileOnReturnF)
	movd dword ptr J9TR_VMThread_returnValue[_rbp],xmm0
	mov uword ptr J9TR_VMThread_tempSlot[_rbp],1
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileOnReturnF)

START_PROC(jitDecompileOnReturnD)
	movq qword ptr J9TR_VMThread_returnValue[_rbp],xmm0
	mov uword ptr J9TR_VMThread_tempSlot[_rbp],2
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileOnReturnD)

START_PROC(jitReportExceptionCatch)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitReportExceptionCatch)
	RESTORE_PRESERVED_REGS
	SWITCH_TO_JAVA_STACK
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitReportExceptionCatch)

START_PROC(jitDecompileAtExceptionCatch)
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileAtExceptionCatch)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileAtExceptionCatch)

START_PROC(jitDecompileAtCurrentPC)
	SWITCH_TO_C_STACK
	CALL_C_WITH_VMTHREAD(c_jitDecompileAtCurrentPC)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileAtCurrentPC)

START_PROC(jitDecompileBeforeReportMethodEnter)
	SWITCH_TO_C_STACK
	CALL_C_WITH_VMTHREAD(c_jitDecompileBeforeReportMethodEnter)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileBeforeReportMethodEnter)

START_PROC(jitDecompileBeforeMethodMonitorEnter)
	SWITCH_TO_C_STACK
	CALL_C_WITH_VMTHREAD(c_jitDecompileBeforeMethodMonitorEnter)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileBeforeMethodMonitorEnter)

START_PROC(jitDecompileAfterAllocation)
	SWITCH_TO_C_STACK
	CALL_C_WITH_VMTHREAD(c_jitDecompileAfterAllocation)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileAfterAllocation)

START_PROC(jitDecompileAfterMonitorEnter)
	SWITCH_TO_C_STACK
	CALL_C_WITH_VMTHREAD(c_jitDecompileAfterMonitorEnter)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileAfterMonitorEnter)

ifelse(eval(ASM_JAVA_SPEC_VERSION >= 24), 1, {
START_PROC(yieldAtMonitorEnter)
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_yield_monent
	CINTERP
END_PROC(yieldAtMonitorEnter)
}) dnl yieldAtMonitorEnter is only supported on JAVA 24+

START_PROC(executeCurrentBytecodeFromJIT)
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_execute_bytecode
	CINTERP
END_PROC(executeCurrentBytecodeFromJIT)

START_PROC(handlePopFramesFromJIT)
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_handle_pop_frames
	CINTERP
END_PROC(handlePopFramesFromJIT)

START_PROC(throwCurrentExceptionFromJIT)
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_throw_current_exception
	CINTERP
END_PROC(throwCurrentExceptionFromJIT)

START_PROC(enterMethodMonitorFromJIT)
	mov _rax,uword ptr J9TR_VMThread_floatTemp1[_rbp]
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],_rax
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_enter_method_monitor
	CINTERP
END_PROC(enterMethodMonitorFromJIT)

START_PROC(reportMethodEnterFromJIT)
	mov _rax,uword ptr J9TR_VMThread_floatTemp1[_rbp]
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],_rax
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_report_method_enter
	CINTERP
END_PROC(reportMethodEnterFromJIT)

START_PROC(jitFillOSRBufferReturn)
	SWITCH_TO_C_STACK
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_exit_interpreter
	CINTERP
END_PROC(jitFillOSRBufferReturn)

ifdef({ASM_J9VM_ENV_DATA64},{

BEGIN_RETURN_POINT(jitExitInterpreterJ)
	mov rax,qword ptr J9TR_VMThread_returnValue[_rbp]
END_RETURN_POINT(jitExitInterpreterJ)

START_PROC(returnFromJITJ)
	mov J9TR_VMThread_floatTemp1[_rbp],rax
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_return_from_jit
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],2
	SWITCH_TO_C_STACK
	CINTERP
END_PROC(returnFromJITJ)

START_PROC(returnFromJITL)
	mov uword ptr J9TR_VMThread_floatTemp1[_rbp],rax
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_return_from_jit
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],1
	SWITCH_TO_C_STACK
	CINTERP
END_PROC(returnFromJITL)

dnl icallVMprJavaSendPatchupVirtual
dnl see j2iVirtual for calling convention details

	DECLARE_EXTERN(old_slow_icallVMprJavaSendPatchupVirtual)

START_PROC(icallVMprJavaSendPatchupVirtual)
	STORE_VIRTUAL_REGISTERS
	CALL_SLOW_PATH_ONLY_HELPER_NO_EXCEPTION_NO_RETURN_VALUE(icallVMprJavaSendPatchupVirtual)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(icallVMprJavaSendPatchupVirtual)

START_PROC(jitDecompileOnReturnJ)
	mov qword ptr J9TR_VMThread_returnValue[_rbp],rax
	mov uword ptr J9TR_VMThread_tempSlot[_rbp],2
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileOnReturnJ)

START_PROC(jitDecompileOnReturnL)
	mov uword ptr J9TR_VMThread_returnValue[_rbp],rax
	mov uword ptr J9TR_VMThread_tempSlot[_rbp],1
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileOnReturnL)

},{	dnl ASM_J9VM_ENV_DATA64

BEGIN_RETURN_POINT(jitExitInterpreterJ)
	mov eax,dword ptr J9TR_VMThread_returnValue[_rbp]
	mov edx,dword ptr J9TR_VMThread_returnValue2[_rbp]
END_RETURN_POINT(jitExitInterpreterJ)

START_PROC(returnFromJITJ)
	mov uword ptr J9TR_VMThread_floatTemp1[_rbp],eax
	mov uword ptr J9TR_VMThread_floatTemp2[_rbp],edx
	mov uword ptr J9TR_VMThread_returnValue[_rbp],J9TR_bcloop_return_from_jit
	mov uword ptr J9TR_VMThread_returnValue2[_rbp],2
	SWITCH_TO_C_STACK
	CINTERP
END_PROC(returnFromJITJ)

START_PROC(jitDecompileOnReturnJ)
	mov dword ptr J9TR_VMThread_returnValue[_rbp],eax
	mov dword ptr J9TR_VMThread_returnValue2[_rbp],edx
	mov uword ptr J9TR_VMThread_tempSlot[_rbp],2
	SWITCH_TO_C_STACK
	SAVE_PRESERVED_REGS
	CALL_C_WITH_VMTHREAD(c_jitDecompileOnReturn)
	jmp uword ptr J9TR_VMThread_tempSlot[_rbp]
END_PROC(jitDecompileOnReturnJ)

})	dnl ASM_J9VM_ENV_DATA64

START_PROC(jitSoftwareReadBarrier)
ifdef({OMR_GC_CONCURRENT_SCAVENGER},{
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	dnl currentThread->javaVM->memoryManagerFunctions->J9ReadBarrier(currentThread,(fj9object_t*)currentThread->floatTemp1);
	mov _rax,uword ptr J9TR_VMThread_javaVM[_rbp]
	mov PARM_REG(2),uword ptr J9TR_VMThread_floatTemp1[_rbp]
	mov _rax,uword ptr J9TR_JavaVM_memoryManagerFunctions[_rax]
	mov PARM_REG(1),_rbp
	call uword ptr J9TR_J9MemoryManagerFunctions_J9ReadBarrier[_rax]
	RESTORE_C_VOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	ret
},{ dnl OMR_GC_CONCURRENT_SCAVENGER
	dnl not supported
	int 3
})	dnl OMR_GC_CONCURRENT_SCAVENGER
END_PROC(jitSoftwareReadBarrier)

	FASTCALL_EXTERN(impl_jitReferenceArrayCopy,2)
START_PROC(jitReferenceArrayCopy)
	pop uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	SWITCH_TO_C_STACK
	SAVE_C_VOLATILE_REGS
	mov uword ptr J9TR_VMThread_floatTemp3[_rbp],_rsi
	mov uword ptr J9TR_VMThread_floatTemp4[_rbp],_rdi
	mov PARM_REG(2),_rcx
	mov PARM_REG(1),_rbp
	call FASTCALL_SYMBOL(impl_jitReferenceArrayCopy,2)
	dnl Save return value to check later.
	dnl We don't check it now because restoring the register clobbers flags.
	mov dword ptr J9TR_VMThread_floatTemp3[_rbp],eax
	RESTORE_C_VOLATILE_REGS
	SWITCH_TO_JAVA_STACK
	dnl Set ZF on success.
	test dword ptr J9TR_VMThread_floatTemp3[_rbp], -1
	push uword ptr J9TR_VMThread_jitReturnAddress[_rbp]
	ret
END_PROC(jitReferenceArrayCopy)

	FILE_END
