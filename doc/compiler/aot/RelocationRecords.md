<!--
Copyright IBM Corp. and others 2020

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

This document describes at a high level what the various relocation records
are used for. The exact data stored into the SCC can be found in
`initializeCommonAOTRelocationHeader` and the various 
`initializePlatformSpecificAOTRelocationHeader` functions. Similarly, the 
exact type of the API class for each relocation kind can be found in
`TR_RelocationRecord::create`.

## Traditional (non-SVM) Validation Records
|Kind|Description|
|--|--|
|`TR_ValidateInstanceField`|Validates the class of an instance field.|
|`TR_ValidateStaticField`|Validates the class of a static field.|
|`TR_ValidateClass`|Validates a class.|
|`TR_ValidateArbitraryClass`|Validates a J9Class acquired in a manner other than by name or cpIndex.|

## SVM Validation Records
|Kind|Description|
|--|--|
|`TR_ValidateClassByName`|Validates a class acquired by name.|
|`TR_ValidateProfiledClass`|Validates a class acquired through profiling.|
|`TR_ValidateClassFromCP`|Validates a class acquired from a constant pool.|
|`TR_ValidateDefiningClassFromCP`|Validates a defining class acquired from a constant pool.|
|`TR_ValidateStaticClassFromCP`|Validates a static class acquired from a constant pool.|
|`TR_ValidateArrayClassFromComponentClass`|Validates either the array class of a class, or the component class of an array class.|
|`TR_ValidateSuperClassFromClass`|Validates the super class of a class.|
|`TR_ValidateClassInstanceOfClass`|Validates whether a class is an instance of another class.|
|`TR_ValidateSystemClassByName`|Validates a system class acquired by name.|
|`TR_ValidateClassFromITableIndexCP`|Validates an interface class from a constant pool.|
|`TR_ValidateDeclaringClassFromFieldOrStatic`|Validates the declaring class from an instance or static field.|
|`TR_ValidateConcreteSubClassFromClass`|Validates the concrete subclass from a class (uses the Persistent CH Table).|
|`TR_ValidateClassChain`|Validates the Class Chain of a class.|
|`TR_ValidateMethodFromClass`|Validates a method acquired from a class.|
|`TR_ValidateStaticMethodFromCP`|Validates a static method acquired from a constant pool.|
|`TR_ValidateSpecialMethodFromCP`|Validates a special method acquired from a constant pool.|
|`TR_ValidateVirtualMethodFromCP`|Validates a virtual method acquired from a constant pool.|
|`TR_ValidateVirtualMethodFromOffset`|Validates a virtual method acquired from an offset.|
|`TR_ValidateInterfaceMethodFromCP`|Validates a interface method acquired from a constant pool.|
|`TR_ValidateMethodFromClassAndSig`|Validates a method acquired using a class and signature.|
|`TR_ValidateStackWalkerMaySkipFramesRecord`|Validates whether the stack walker may skip frames.|
|`TR_ValidateClassInfoIsInitialized`|Validates whether a class is initialized based on the corresponding class info in the Persistent CH Table.|
|`TR_ValidateMethodFromSingleImplementer`|Validates whether a method is a single implementer (uses the Persistent CH Table).|
|`TR_ValidateMethodFromSingleInterfaceImplementer`|Validates whether a method is a single interface implementer (uses the Persistent CH Table).|
|`TR_ValidateMethodFromSingleAbstractImplementer`|Validates whether a method is a single abstract implementer (uses the Persistent CH Table).|
|`TR_ValidateImproperInterfaceMethodFromCP`|Validates an improper interface method acquired from a constant pool (nestmates).|

## Inlined Method Validation / Metadata Relocation Records
|Kind|Description|
|--|--|
|`TR_InlinedStaticMethodWithNopGuard`|Validates the J9Method of an inlined static method, relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`, and registers a runtime assumption for the guard; or patches the guard to jump to the slow path on failure.|
|`TR_InlinedSpecialMethodWithNopGuard`|Validates the J9Method of an inlined special method, relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`, and registers a runtime assumption for the guard; or patches the guard to jump to the slow path on failure.|
|`TR_InlinedVirtualMethodWithNopGuard`|Validates the J9Method of an inlined virtual method, relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`, and registers a runtime assumption for the guard; or patches the guard to jump to the slow path on failure.|
|`TR_InlinedInterfaceMethodWithNopGuard`|Validates the J9Method of an inlined interface method, relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`, and registers a runtime assumption for the guard; or patches the guard to jump to the slow path on failure.|
|`TR_InlinedAbstractMethodWithNopGuard`|Validates the J9Method of an inlined abstract method, relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`, and registers a runtime assumption for the guard; or patches the guard to jump to the slow path on failure.|
|`TR_InlinedStaticMethod`|Validates the J9Method of an inlined static method and relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`.|
|`TR_InlinedSpecialMethod`|Validates the J9Method of an inlined special method and relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`.|
|`TR_InlinedInterfaceMethod`|Validates the J9Method of an inlined interface method and relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`.|
|`TR_InlinedVirtualMethod`|Validates the J9Method of an inlined virtual method and relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`.|
|`TR_InlinedAbstractMethod`|Validates the J9Method of an inlined abstract method and relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`.|
|`TR_ProfiledMethodGuardRelocation`|Validates the J9Method used in a profiled method guard and relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`.|
|`TR_ProfiledClassGuardRelocation`|Validates the J9Class used in a profiled class guard and relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`.|
|`TR_ProfiledInlinedMethodRelocation`|Validates the J9Method of a profiled inlined method and relocates the appropriate entry in the inlining table in the `J9JITExceptionTable`.|

## Relocation Records
|Kind|Description|
|--|--|
|`TR_NoRelocation`|Indicates no relocation record is necessary.|
|`TR_HelperAddress`|Relocates a helper offset relative to the address of the instruction being relocated.|
|`TR_AbsoluteHelperAddress`|Relocates the absolute address of a helper.|
|`TR_ArrayCopyHelper`|Relocates the address of the Array Copy Helper.|
|`TR_ArrayCopyToc`|Only used on POWER. Relocates the address of the TOC for the Array Copy Helper.|
|`TR_RelativeMethodAddress`|Relocates an address that is within the method being compiled.|
|`TR_AbsoluteMethodAddress`|Relocates an address that is within the method being compiled.|
|`TR_AbsoluteMethodAddressOrderedPair`|Relocates an address that is within the method being compiled. The exact method of relocation depends on the sequence enum specified.|
|`TR_FixedSequenceAddress`|Only used on POWER. Relocates the load of a 64 bit address of a label in the method being compiled. The exact method of relocation depends on the sequence enum specified.|
|`TR_FixedSequenceAddress2`|Relocates the load of an address in the method being compiled. The exact method of relocation depends on the sequence enum specified.|
|`TR_BodyInfoAddress`|Relocates the address to the `TR_PersistentJittedBodyInfo` for the method being compiled. This is used to relocate that address in the method's preprologue, and is also used on X and Z to relocate the address to the recompilation counter, which is the first field of the `TR_PersistentJittedBodyInfo`. Also relocates some fields, as well as the `TR_PersistentMethodInfo`.|
|`TR_BodyInfoAddressLoad`|Relocates the address to the `TR_PersistentJittedBodyInfo` for the method being compiled. This is only used on Power and ARM, and only to relocate loads from the address of the recompilation counter, which is the first field of the `TR_PersistentJittedBodyInfo`.|
|`TR_CheckMethodEnter`|Patches the code if method enter tracing is enabled. Note, the AOT code should have been compiled with the assumption that tracing could be enabled.|
|`TR_CheckMethodExit`|Patches the code if method enter tracing is enabled. Note, the AOT code should have been compiled with the assumption that tracing could be enabled.|
|`TR_JNIVirtualTargetAddress`|Relocates the start address of a virtual JNI method.|
|`TR_JNIStaticTargetAddress`|Relocates the start address of a static JNI method.|
|`TR_JNISpecialTargetAddress`|Relocates the start address of a special JNI method.|
|`TR_SpecialRamMethodConst`|Relocates the J9Method of a special method.|
|`TR_VirtualRamMethodConst`|Relocates the J9Method of a virtual method.|
|`TR_StaticRamMethodConst`|Relocates the J9Method of a static method.|
|`TR_Thunks`|Registers a J2I thunk; if the thunk exists in the SCC, it loads it out and relocates it first. Also relocates a pointer to the J9ConstantPool.|
|`TR_J2IVirtualThunkPointer`|Relocates like `TR_Thunks` but additionally relocates pointers to the J2I thunk.|
|`TR_MethodPointer`|Relocates a J9Method pointer acquired via profiling. Only used without the SVM.|
|`TR_ClassPointer`|Relocates a J9Class pointer acquired via profiling. Only used without the SVM.|
|`TR_InlinedMethodPointer`|Relocates a J9Method pointer acquired via profiling that also happens to be inlined into the method being compiled. Only used without the SVM.|
|`TR_ClassAddress`|Relocates the address of a class. Only used without the SVM.|
|`TR_ArbitraryClassAddress`|Validates classes that would normally validate like a `TR_ClassAddress` but contains a cpIndex of -1, so must be validated differently. This record is limited to classes loaded by the bootstrap class loader. Only used without the SVM.|
|`TR_RamMethod`|Relocates the J9Method pointer of the method being compiled.|
|`TR_RamMethodSequence`|Relocates the J9Method pointer of the method being compiled based on the sequence enum specified.|
|`TR_DataAddress`|Relocates the address of a Java static.|
|`TR_MethodObject`|Relocates the address of a field in a J9ConstantPool struct.|
|`TR_ConstantPool`|Relocates a `J9ConstantPool *` pointer; the pointer has to be the constant pool the method being compiled or one of the inlined methods.|
|`TR_VerifyClassObjectForAlloc`|Validates the class of an inlined object allocation. Patches the guard to the slow path if invalid.|
|`TR_VerifyRefArrayForAlloc`|Validates the class of an inlined array object allocation. Patches the guard to the slow path if invalid.|
|`TR_Trampolines`|Reserves an unresolved trampoline. Also relocates a pointer to the J9ConstantPool.|
|`TR_ResolvedTrampolines`|Reserves a resolved trampoline. Only used with the SVM.|
|`TR_PicTrampolines`|Only used on x86. Reserves the necessary number of trampolines used for PICs.|
|`TR_GlobalValue`|Relocates a value materialized from the global value table indexed via the `TR_GlobalValueItem` enum.|
|`TR_HCR`|Creates HCR Runtime Assumptions against the J9Method of the method being compiled.|
|`TR_EmitClass`|Used when Hardware Profiling is enabled. Used to associate an instruction with the bytecode PC for value profiling.|
|`TR_DebugCounter`|Materializes a Debug Counter and relocates pointers to the counter.|
|`TR_ClassUnloadAssumption`|Registers a runtime assumption at the specified location in the code to trigger if any class gets unloaded.|
|`TR_BlockFrequency`|Relocates pointer to the JProfiling block frequency counter.|
|`TR_RecompQueuedFlag`|Relocates the address of the queued for recompilation flag in the `TR_BlockFrequencyInfo` object in the `TR_PersistentJittedBodyInfo` of the method being compiled.|
|`TR_Breakpoint`|Relocates Break Point guards used in FSD.|
|`TR_VMINLMethod`|Relocates the J9Method of a `VMINL` method.|
|`TR_SymbolFromManager`|Relocates a pointer materialized by using its SVM ID.|
|`TR_DiscontiguousSymbolFromManager`|Relocates a discontiguous pointer materialized by using its SVM ID.|
|`TR_MethodCallAddress`|Relocates the address of a call target. Only used in JitServer (in AOT, all other methods are assumed to be interpreted).|
|`TR_CatchBlockCounter`|Relocates the address of the catch block counter in the `TR_PersistentMethodInfo` of the method being compiled.|
|`TR_StartPC`|Relocates the startPC of the method being compiled. Only implemented and used on Power.|
