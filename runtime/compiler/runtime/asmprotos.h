/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef ASMPROTOS_H
#define ASMPROTOS_H

#ifdef __cplusplus
extern "C" {
#endif


#if (defined(__IBMCPP__) || defined(__IBMC__) && !defined(MVS)) && !defined(J9ZOS390) && !defined(LINUXPPC64)
#if defined(AIXPPC)
#define JIT_HELPER(x) extern "C" void * x
#else
#define JIT_HELPER(x) extern "C" x()
#endif
#else
#if defined(LINUXPPC64)
#if defined(J9VM_ENV_LITTLE_ENDIAN)
#define JIT_HELPER(x) extern "C" void x()
#else /* J9VM_ENV_LITTLE_ENDIAN */
#define JIT_HELPER(x) extern "C" void * x
#endif /* J9VM_ENV_LITTLE_ENDIAN */
#elif defined(MVS) || defined(LINUX) || defined(J9ZOS390) || defined (J9HAMMER) || defined (WINDOWS) || defined (OSX)
#define JIT_HELPER(x) extern "C" void x()
#elif defined(NEUTRINO)
#define JIT_HELPER(x) extern "C" char* x[]
#else
#define JIT_HELPER(x) extern "C" x()
#endif
#endif

#if defined(TR_HOST_POWER) && !defined(__LITTLE_ENDIAN__)
#undef JIT_HELPER
#define JIT_HELPER(x) extern "C" void x()
#endif

/* #include "VM.hpp" */

JIT_HELPER(jitAcquireVMAccess);  // asm calling-convention helper
JIT_HELPER(jitAMultiNewArray);  // asm calling-convention helper
JIT_HELPER(jitANewArray);  // asm calling-convention helper
JIT_HELPER(jitCallCFunction);  // asm calling-convention helper
JIT_HELPER(jitCallJitAddPicToPatchOnClassUnload);  // asm calling-convention helper
JIT_HELPER(jitCheckAsyncMessages);  // asm calling-convention helper
JIT_HELPER(jitCheckCast);  // asm calling-convention helper
JIT_HELPER(jitCheckCastForArrayStore);  // asm calling-convention helper
JIT_HELPER(jitCheckIfFinalizeObject);  // asm calling-convention helper
JIT_HELPER(jitCollapseJNIReferenceFrame);  // asm calling-convention helper
JIT_HELPER(jitFindFieldSignatureClass);  // asm calling-convention helper
JIT_HELPER(jitHandleArrayIndexOutOfBoundsTrap);  // asm calling-convention helper
JIT_HELPER(jitHandleIntegerDivideByZeroTrap);  // asm calling-convention helper
JIT_HELPER(jitHandleNullPointerExceptionTrap);  // asm calling-convention helper
JIT_HELPER(jitHandleInternalErrorTrap);  // asm calling-convention helper
JIT_HELPER(jitInduceOSRAtCurrentPC);  // asm calling-convention helper
JIT_HELPER(jitInduceOSRAtCurrentPCAndRecompile);  // asm calling-convention helper
JIT_HELPER(jitInstanceOf);  // asm calling-convention helper
JIT_HELPER(jitInterpretNewInstanceMethod);  // asm calling-convention helper
JIT_HELPER(jitLookupInterfaceMethod);  // asm calling-convention helper
JIT_HELPER(jitMethodIsNative);  // asm calling-convention helper
JIT_HELPER(jitMethodIsSync);  // asm calling-convention helper
JIT_HELPER(jitMethodMonitorEntry);  // asm calling-convention helper
JIT_HELPER(jitMethodMonitorExit);  // asm calling-convention helper
JIT_HELPER(jitMonitorEntry);  // asm calling-convention helper
JIT_HELPER(jitMonitorExit);  // asm calling-convention helper
JIT_HELPER(jitNewArray);  // asm calling-convention helper
JIT_HELPER(jitNewInstanceImplAccessCheck);  // asm calling-convention helper
JIT_HELPER(jitNewObject);  // asm calling-convention helper
JIT_HELPER(jitObjectHashCode);  // asm calling-convention helper
JIT_HELPER(jitPostJNICallOffloadCheck);  // asm calling-convention helper
JIT_HELPER(jitPreJNICallOffloadCheck);  // asm calling-convention helper
JIT_HELPER(jitReferenceArrayCopy);  // asm calling-convention helper
JIT_HELPER(jitReleaseVMAccess);  // asm calling-convention helper
JIT_HELPER(jitReportMethodEnter);  // asm calling-convention helper
JIT_HELPER(jitReportMethodExit);  // asm calling-convention helper
JIT_HELPER(jitReportStaticMethodEnter);  // asm calling-convention helper
JIT_HELPER(jitResolveClass);  // asm calling-convention helper
JIT_HELPER(jitResolveClassFromStaticField);  // asm calling-convention helper
JIT_HELPER(jitResolvedFieldIsVolatile);  // asm calling-convention helper
JIT_HELPER(jitResolveField);  // asm calling-convention helper
JIT_HELPER(jitResolveFieldSetter);  // asm calling-convention helper
JIT_HELPER(jitResolveFieldDirect);  // asm calling-convention helper
JIT_HELPER(jitResolveFieldSetterDirect);  // asm calling-convention helper
JIT_HELPER(jitResolveHandleMethod);  // asm calling-convention helper
JIT_HELPER(jitResolveInterfaceMethod);  // asm calling-convention helper
JIT_HELPER(jitResolveInvokeDynamic);  // asm calling-convention helper
JIT_HELPER(jitResolveMethodHandle);  // asm calling-convention helper
JIT_HELPER(jitResolveMethodType);  // asm calling-convention helper
JIT_HELPER(jitResolveSpecialMethod);  // asm calling-convention helper
JIT_HELPER(jitResolveStaticField);  // asm calling-convention helper
JIT_HELPER(jitResolveStaticFieldSetter);  // asm calling-convention helper
JIT_HELPER(jitResolveStaticFieldDirect);  // asm calling-convention helper
JIT_HELPER(jitResolveStaticFieldSetterDirect);  // asm calling-convention helper
JIT_HELPER(jitResolveStaticMethod);  // asm calling-convention helper
JIT_HELPER(jitResolveString);  // asm calling-convention helper
JIT_HELPER(jitResolveConstantDynamic);  // asm calling-convention helper
JIT_HELPER(jitResolveVirtualMethod);  // asm calling-convention helper
JIT_HELPER(jitRetranslateCaller);  // asm calling-convention helper
JIT_HELPER(jitRetranslateCallerWithPreparation);  // asm calling-convention helper
JIT_HELPER(jitRetranslateMethod);  // asm calling-convention helper
JIT_HELPER(jitStackOverflow);  // asm calling-convention helper
JIT_HELPER(jitThrowArithmeticException);  // asm calling-convention helper
JIT_HELPER(jitThrowArrayIndexOutOfBounds);  // asm calling-convention helper
JIT_HELPER(jitThrowArrayStoreException);  // asm calling-convention helper
JIT_HELPER(jitThrowArrayStoreExceptionWithIP);  // asm calling-convention helper
JIT_HELPER(jitThrowClassCastException);  // asm calling-convention helper
JIT_HELPER(jitThrowCurrentException);  // asm calling-convention helper
JIT_HELPER(jitThrowException);  // asm calling-convention helper
JIT_HELPER(jitThrowExceptionInInitializerError);  // asm calling-convention helper
JIT_HELPER(jitThrowInstantiationException);  // asm calling-convention helper
JIT_HELPER(jitThrowNullPointerException);  // asm calling-convention helper
JIT_HELPER(jitThrowWrongMethodTypeException);  // asm calling-convention helper
JIT_HELPER(jitThrowIncompatibleReceiver);  // asm calling-convention helper
JIT_HELPER(jitThrowIncompatibleClassChangeError);  // asm calling-convention helper
JIT_HELPER(jitTranslateNewInstanceMethod);  // asm calling-convention helper
JIT_HELPER(jitTypeCheckArrayStore);  // asm calling-convention helper
JIT_HELPER(jitTypeCheckArrayStoreWithNullCheck);  // asm calling-convention helper
JIT_HELPER(jitVolatileReadDouble);  // asm calling-convention helper
JIT_HELPER(jitVolatileReadLong);  // asm calling-convention helper
JIT_HELPER(jitVolatileWriteDouble);  // asm calling-convention helper
JIT_HELPER(jitVolatileWriteLong);  // asm calling-convention helper
JIT_HELPER(jitWriteBarrierBatchStore);  // asm calling-convention helper
JIT_HELPER(jitWriteBarrierBatchStoreWithRange);  // asm calling-convention helper
JIT_HELPER(jitWriteBarrierClassStoreMetronome);  // asm calling-convention helper
JIT_HELPER(jitWriteBarrierJ9ClassBatchStore);  // asm calling-convention helper
JIT_HELPER(jitWriteBarrierJ9ClassStore);  // asm calling-convention helper
JIT_HELPER(jitWriteBarrierStore);  // asm calling-convention helper
JIT_HELPER(jitWriteBarrierStoreGenerational);  // asm calling-convention helper
JIT_HELPER(jitWriteBarrierStoreGenerationalAndConcurrentMark);  // asm calling-convention helper
JIT_HELPER(jitWriteBarrierStoreMetronome);  // asm calling-convention helper
JIT_HELPER(jitDecompileAfterAllocation);  // asm calling-convention helper
JIT_HELPER(jitDecompileAfterMonitorEnter);  // asm calling-convention helper
JIT_HELPER(jitDecompileAtCurrentPC);  // asm calling-convention helper
JIT_HELPER(jitDecompileAtExceptionCatch);  // asm calling-convention helper
JIT_HELPER(jitDecompileBeforeMethodMonitorEnter);  // asm calling-convention helper
JIT_HELPER(jitDecompileBeforeReportMethodEnter);  // asm calling-convention helper
JIT_HELPER(jitReportExceptionCatch);  // asm calling-convention helper
JIT_HELPER(jitANewArrayNoZeroInit);  // asm calling-convention helper
JIT_HELPER(jitNewArrayNoZeroInit);  // asm calling-convention helper
JIT_HELPER(jitNewObjectNoZeroInit);  // asm calling-convention helper
JIT_HELPER(jitReportFinalFieldModified); // asm calling-convention helper
JIT_HELPER(jitReportInstanceFieldRead); // asm calling-convention helper
JIT_HELPER(jitReportInstanceFieldWrite); // asm calling-convention helper
JIT_HELPER(jitReportStaticFieldRead); // asm calling-convention helper
JIT_HELPER(jitReportStaticFieldWrite); // asm calling-convention helper
JIT_HELPER(jitSoftwareReadBarrier);  // asm calling-convention helper

#ifdef __cplusplus
}
#endif

#endif /* ASMPROTOS_H */
