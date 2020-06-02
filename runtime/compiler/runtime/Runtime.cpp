/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifdef TR_HOST_X86
#if defined(LINUX)
#include <time.h>
#endif
#endif   // TR_HOST_X86

#include "runtime/J9Runtime.hpp"

#include <stdint.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9comp.h"
#include "j9cp.h"
#include "j9protos.h"
#include "jitprotos.h"
#include "rommeth.h"
#include "emfloat.h"
#include "env/FrontEnd.hpp"
#include "codegen/PreprologueConst.hpp"
#include "codegen/PrivateLinkage.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/jittypes.h"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "runtime/asmprotos.h"
#include "runtime/codertinit.hpp"
#include "env/VMJ9.h"
#include "runtime/J9RuntimeAssumptions.hpp"
#if defined(J9VM_OPT_JITSERVER) && defined(TR_HOST_POWER)
#include "control/CompilationRuntime.hpp"
#include "control/MethodToBeCompiled.hpp"
#endif

extern J9JITConfig *jitConfig;
extern "C" int32_t _getnumRTHelpers();  /* 390 asm stub */

extern TR_PersistentMemory * trPersistentMemory;

extern "C" {
TR_UnloadedClassPicSite *createClassUnloadPicSite(void *classPointer, void *addressToBePatched, uint32_t size,
                                                  OMR::RuntimeAssumption **sentinel)
   {
   TR_FrontEnd * fe = (TR_FrontEnd*)jitConfig->compilationInfo; // ugly, but codert cannot include VMJ9.h
   return TR_UnloadedClassPicSite::make(fe, trPersistentMemory, (uintptr_t) classPointer,
      (uint8_t*)addressToBePatched, size, RuntimeAssumptionOnClassRedefinitionPIC, sentinel);
   }

void jitAddPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched)
   {
      J9JavaVM            *vm        = jitConfig->javaVM;
      J9VMThread          *vmContext = vm->internalVMFunctions->currentVMThread(vm);
      J9JITExceptionTable *metaData  = jitConfig->jitGetExceptionTableFromPC(vmContext, (UDATA)addressToBePatched);

      if (!createClassUnloadPicSite(classPointer, addressToBePatched, sizeof(uintptr_t),
                                    (OMR::RuntimeAssumption**)(&metaData->runtimeAssumptionList))
         || debug("failToRegisterPicSitesForClassUnloading"))
         {
         *(uintptr_t*)addressToBePatched = 0x0101dead;
         }

   }

void createJNICallSite(void *ramMethod, void *addressToBePatched, OMR::RuntimeAssumption **sentinel)
   {
   TR_FrontEnd * fe = (TR_FrontEnd*)jitConfig->compilationInfo; // ugly, but codert cannot include VMJ9.h

   TR_PatchJNICallSite::make(fe, trPersistentMemory, (uintptr_t) ramMethod, (uint8_t *) addressToBePatched, sentinel);

   }

void jitAdd32BitPicToPatchOnClassUnload(void *classPointer, void *addressToBePatched)
   {

      J9JavaVM            *vm        = jitConfig->javaVM;
      J9VMThread          *vmContext = vm->internalVMFunctions->currentVMThread(vm);
      J9JITExceptionTable *metaData  = jitConfig->jitGetExceptionTableFromPC(vmContext, (UDATA)addressToBePatched);

      if (!createClassUnloadPicSite(classPointer, addressToBePatched, 4,
                                    (OMR::RuntimeAssumption**)(&metaData->runtimeAssumptionList))
         || debug("failToRegisterPicSitesForClassUnloading"))
         {
         *(uint32_t*)addressToBePatched = 0x0101dead;
         }

   }

void createClassRedefinitionPicSite(void *classPointer, void *addressToBePatched, uint32_t size,
                                    bool unresolved, OMR::RuntimeAssumption **sentinel)
   {
   TR_FrontEnd * fe = (TR_FrontEnd*)jitConfig->compilationInfo; // ugly, but codert cannot include VMJ9.h
   if (unresolved)
      {
      TR_RedefinedClassUPicSite::make(fe, trPersistentMemory, (uintptr_t) classPointer,
                                      (uint8_t*)addressToBePatched, size, sentinel);
      }
   else
      {
      TR_RedefinedClassRPicSite::make(fe, trPersistentMemory, (uintptr_t) classPointer,
                                     (uint8_t*)addressToBePatched, size, sentinel);
      }
   }

void jitAddPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved)
   {
   J9JavaVM            *vm        = jitConfig->javaVM;
   J9VMThread          *vmThread  = vm->internalVMFunctions->currentVMThread(vm);
   J9JITExceptionTable *metaData  = jitConfig->jitGetExceptionTableFromPC(vmThread, (UDATA)addressToBePatched);
   createClassRedefinitionPicSite(classPointer, addressToBePatched, sizeof(uintptr_t), unresolved,
                                  (OMR::RuntimeAssumption**)(&metaData->runtimeAssumptionList));
   }

void jitAdd32BitPicToPatchOnClassRedefinition(void *classPointer, void *addressToBePatched, bool unresolved)
   {
   J9JavaVM            *vm        = jitConfig->javaVM;
   J9VMThread          *vmThread  = vm->internalVMFunctions->currentVMThread(vm);
   J9JITExceptionTable *metaData  = jitConfig->jitGetExceptionTableFromPC(vmThread, (UDATA)addressToBePatched);
   createClassRedefinitionPicSite(classPointer, addressToBePatched, 4, unresolved,
                                  (OMR::RuntimeAssumption**)(&metaData->runtimeAssumptionList));
   }
}



TR_RuntimeHelperTable runtimeHelpers;

void* TR_RuntimeHelperTable::translateAddress(void * a)
   {
   return
#if defined(J9ZOS390)
       // In ZOS, Entrypoint address is stored at an offset from the function descriptor
       // The offset is 20 and 24 for 32-bit and 64-bit z/OS, respectively. See J9FunctionDescriptor_T.
       // In zLinux, Entrypoint address is same as the function descriptor
       TOC_UNWRAP_ADDRESS(a);
#elif defined(TR_HOST_POWER) && (defined(TR_HOST_64BIT) || defined(AIXPPC)) && !defined(__LITTLE_ENDIAN__)
      a ? *(void **)a : (void *)0;
#else // defined(TR_HOST_POWER) && !defined(__LITTLE_ENDIAN__)
      a;
#endif // defined(TR_HOST_POWER) && !defined(__LITTLE_ENDIAN__)
   }

void* TR_RuntimeHelperTable::getFunctionPointer(TR_RuntimeHelper h)
   {
   if (h < TR_numRuntimeHelpers && (_linkage[h] == TR_CHelper || _linkage[h] == TR_Helper))
      return _helpers[h];
   else
      return reinterpret_cast<void*>(TR_RuntimeHelperTable::INVALID_FUNCTION_POINTER);
   }

void* TR_RuntimeHelperTable::getFunctionEntryPointOrConst(TR_RuntimeHelper h)
   {
#if defined(J9VM_OPT_JITSERVER) && defined(TR_HOST_POWER)
   // Fetch helper address directly from the client.
   // We only need to do this for Power because other platforms
   // can relocate helper addresses, while Power might need to store
   // the address in gr12 register and it cannot relocate helpers stored
   // in non-call instructions.
   TR::CompilationInfo *compInfo = getCompilationInfo(jitConfig);
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      auto compInfoPT = TR::compInfoPT;
      if (compInfoPT)
         {
         auto stream = compInfoPT->getMethodBeingCompiled()->_stream;
         auto vmInfo = compInfoPT->getClientData()->getOrCacheVMInfo(stream);
         return vmInfo->_helperAddresses[h];
         }
      return NULL;
      }
#endif
   if (h < TR_numRuntimeHelpers)
      {
      if (_linkage[h] == TR_CHelper || _linkage[h] == TR_Helper)
         return translateAddress(_helpers[h]);
      else
         return _helpers[h];
      }
   else
      return reinterpret_cast<void*>(TR_RuntimeHelperTable::INVALID_FUNCTION_POINTER);
   }

#if defined(TR_HOST_S390)
// For ZLinux MCC Support.
extern "C" void mcc_callPointPatching_unwrapper(void **argsPtr, void **resPtr);
extern "C" void mcc_reservationAdjustment_unwrapper(void **argsPtr, void **resPtr);
extern "C" void mcc_lookupHelperTrampoline_unwrapper(void **argsPtr, void **resPtr);
#endif

JIT_HELPER(j2iTransition);
JIT_HELPER(icallVMprJavaSendNativeStatic);
JIT_HELPER(icallVMprJavaSendStatic0);
JIT_HELPER(icallVMprJavaSendStatic1);
JIT_HELPER(icallVMprJavaSendStaticJ);
JIT_HELPER(icallVMprJavaSendStaticF);
JIT_HELPER(icallVMprJavaSendStaticD);
JIT_HELPER(icallVMprJavaSendStaticSync0);
JIT_HELPER(icallVMprJavaSendStaticSync1);
JIT_HELPER(icallVMprJavaSendStaticSyncJ);
JIT_HELPER(icallVMprJavaSendStaticSyncF);
JIT_HELPER(icallVMprJavaSendStaticSyncD);
JIT_HELPER(icallVMprJavaSendInvokeExact0);
JIT_HELPER(icallVMprJavaSendInvokeExact1);
JIT_HELPER(icallVMprJavaSendInvokeExactJ);
JIT_HELPER(icallVMprJavaSendInvokeExactL);
JIT_HELPER(icallVMprJavaSendInvokeExactF);
JIT_HELPER(icallVMprJavaSendInvokeExactD);
JIT_HELPER(icallVMprJavaSendInvokeWithArgumentsHelperL);
JIT_HELPER(initialInvokeExactThunk_unwrapper);

JIT_HELPER(estimateGPU);
JIT_HELPER(regionEntryGPU);
JIT_HELPER(regionExitGPU);
JIT_HELPER(copyToGPU);
JIT_HELPER(copyFromGPU);
JIT_HELPER(allocateGPUKernelParms);
JIT_HELPER(launchGPUKernel);
JIT_HELPER(invalidateGPU);
JIT_HELPER(getStateGPU);
JIT_HELPER(flushGPU);
JIT_HELPER(callGPU);

#ifdef TR_HOST_X86

// --------------------------------------------------------------------------------
//                                  X86 COMMON
// --------------------------------------------------------------------------------
JIT_HELPER(doubleToLong);
JIT_HELPER(doubleToInt);
JIT_HELPER(floatToLong);
JIT_HELPER(floatToInt);

JIT_HELPER(interpreterUnresolvedClassGlue);
JIT_HELPER(interpreterUnresolvedClassFromStaticFieldGlue);
JIT_HELPER(interpreterUnresolvedStringGlue);
JIT_HELPER(interpreterUnresolvedMethodTypeGlue);
JIT_HELPER(interpreterUnresolvedMethodHandleGlue);
JIT_HELPER(interpreterUnresolvedCallSiteTableEntryGlue);
JIT_HELPER(interpreterUnresolvedMethodTypeTableEntryGlue);
JIT_HELPER(interpreterUnresolvedStaticFieldGlue);
JIT_HELPER(interpreterUnresolvedStaticFieldSetterGlue);
JIT_HELPER(interpreterUnresolvedFieldGlue);
JIT_HELPER(interpreterUnresolvedFieldSetterGlue);
JIT_HELPER(interpreterUnresolvedConstantDynamicGlue);
JIT_HELPER(interpreterStaticAndSpecialGlue);

JIT_HELPER(SMPinterpreterUnresolvedStaticGlue);
JIT_HELPER(SMPinterpreterUnresolvedSpecialGlue);
JIT_HELPER(SMPinterpreterUnresolvedDirectVirtualGlue);
JIT_HELPER(SMPinterpreterUnresolvedClassGlue);
JIT_HELPER(SMPinterpreterUnresolvedClassGlue2);
JIT_HELPER(SMPinterpreterUnresolvedStringGlue);
JIT_HELPER(SMPinterpreterUnresolvedMethodTypeGlue);
JIT_HELPER(SMPinterpreterUnresolvedMethodHandleGlue);
JIT_HELPER(SMPinterpreterUnresolvedCallSiteTableEntryGlue);
JIT_HELPER(SMPinterpreterUnresolvedMethodTypeTableEntryGlue);
JIT_HELPER(SMPinterpreterUnresolvedStaticDataGlue);
JIT_HELPER(SMPinterpreterUnresolvedStaticStoreDataGlue);
JIT_HELPER(SMPinterpreterUnresolvedInstanceDataGlue);
JIT_HELPER(SMPinterpreterUnresolvedInstanceStoreDataGlue);

JIT_HELPER(resolveIPicClass);
JIT_HELPER(populateIPicSlotClass);
JIT_HELPER(populateIPicSlotCall);
JIT_HELPER(dispatchInterpretedFromIPicSlot);
JIT_HELPER(IPicLookupDispatch);
JIT_HELPER(resolveVPicClass);
JIT_HELPER(populateVPicSlotClass);
JIT_HELPER(populateVPicSlotCall);
JIT_HELPER(dispatchInterpretedFromVPicSlot);
JIT_HELPER(populateVPicVTableDispatch);
JIT_HELPER(interpreterUnresolvedSpecialGlue);
JIT_HELPER(interpreterUnresolvedStaticGlue);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
#if defined(OMR_GC_FULL_POINTERS)
JIT_HELPER(doAESInHardwareJitFull);
JIT_HELPER(expandAESKeyInHardwareJitFull);
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
JIT_HELPER(doAESInHardwareJitCompressed);
JIT_HELPER(expandAESKeyInHardwareJitCompressed);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
#endif

JIT_HELPER(jitMonitorEnterReserved);
JIT_HELPER(jitMonitorEnterReservedPrimitive);
JIT_HELPER(jitMonitorEnterPreservingReservation);
JIT_HELPER(jitMonitorExitReserved);
JIT_HELPER(jitMonitorExitReservedPrimitive);
JIT_HELPER(jitMonitorExitPreservingReservation);
JIT_HELPER(jitMethodMonitorEnterReserved);
JIT_HELPER(jitMethodMonitorEnterReservedPrimitive);
JIT_HELPER(jitMethodMonitorEnterPreservingReservation);
JIT_HELPER(jitMethodMonitorExitPreservingReservation);
JIT_HELPER(jitMethodMonitorExitReservedPrimitive);
JIT_HELPER(jitMethodMonitorExitReserved);
JIT_HELPER(prefetchTLH);
JIT_HELPER(newPrefetchTLH);

JIT_HELPER(arrayTranslateTRTO);
JIT_HELPER(arrayTranslateTROTNoBreak);
JIT_HELPER(arrayTranslateTROT);

// --------------------------------------------------------------------------------
//                                   AMD64
// --------------------------------------------------------------------------------

#ifdef TR_HOST_64BIT

JIT_HELPER(SSEdoubleRemainder);
JIT_HELPER(SSEfloatRemainder);


JIT_HELPER(icallVMprJavaSendVirtual0);
JIT_HELPER(icallVMprJavaSendVirtual1);
JIT_HELPER(icallVMprJavaSendVirtualJ);
JIT_HELPER(icallVMprJavaSendVirtualL);
JIT_HELPER(icallVMprJavaSendVirtualF);
JIT_HELPER(icallVMprJavaSendVirtualD);

JIT_HELPER(compressString);
JIT_HELPER(compressStringNoCheck);
JIT_HELPER(compressStringJ);
JIT_HELPER(compressStringNoCheckJ);
JIT_HELPER(andORString);
JIT_HELPER(encodeUTF16Big);
JIT_HELPER(encodeUTF16Little);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
JIT_HELPER(doAESENCEncrypt);
JIT_HELPER(doAESENCDecrypt);
#endif /* J9VM_OPT_JAVA_CRYPTO_ACCELERATION */

JIT_HELPER(methodHandleJ2IGlue);
JIT_HELPER(methodHandleJ2I_unwrapper);

// --------------------------------------------------------------------------------
//                                    IA32
// --------------------------------------------------------------------------------

#else /* TR_HOST_64BIT */
JIT_HELPER(longDivide);
JIT_HELPER(longRemainder);


JIT_HELPER(X87doubleRemainder);
JIT_HELPER(X87floatRemainder);

JIT_HELPER(SSEfloatRemainderIA32Thunk);
JIT_HELPER(SSEdoubleRemainderIA32Thunk);
JIT_HELPER(SSEdouble2LongIA32);

JIT_HELPER(compressString);
JIT_HELPER(compressStringNoCheck);
JIT_HELPER(compressStringJ);
JIT_HELPER(compressStringNoCheckJ);
JIT_HELPER(andORString);
JIT_HELPER(encodeUTF16Big);
JIT_HELPER(encodeUTF16Little);

JIT_HELPER(SMPVPicInit);
#endif /* TR_HOST_64BIT */

#elif defined(TR_HOST_POWER)
JIT_HELPER(__longDivide);
JIT_HELPER(__longDivideEP);
JIT_HELPER(__unsignedLongDivide);
JIT_HELPER(__unsignedLongDivideEP);
JIT_HELPER(__double2Long);
JIT_HELPER(__doubleRemainder);
JIT_HELPER(__integer2Double);
JIT_HELPER(__long2Double);
JIT_HELPER(__long2Float);
JIT_HELPER(__long2Float_mv);
JIT_HELPER(_interpreterUnresolvedStaticGlue);
JIT_HELPER(_interpreterUnresolvedSpecialGlue);
JIT_HELPER(_interpreterUnresolvedDirectVirtualGlue);
JIT_HELPER(_interpreterUnresolvedClassGlue);
JIT_HELPER(_interpreterUnresolvedClassGlue2);
JIT_HELPER(_interpreterUnresolvedStringGlue);
JIT_HELPER(_interpreterUnresolvedConstantDynamicGlue);
JIT_HELPER(_interpreterUnresolvedMethodTypeGlue);
JIT_HELPER(_interpreterUnresolvedMethodHandleGlue);
JIT_HELPER(_interpreterUnresolvedCallSiteTableEntryGlue);
JIT_HELPER(_interpreterUnresolvedMethodTypeTableEntryGlue);
JIT_HELPER(_interpreterUnresolvedStaticDataGlue);
JIT_HELPER(_interpreterUnresolvedStaticDataStoreGlue);
JIT_HELPER(_interpreterUnresolvedInstanceDataGlue);
JIT_HELPER(_interpreterUnresolvedInstanceDataStoreGlue);
JIT_HELPER(_virtualUnresolvedHelper);
JIT_HELPER(_interfaceCallHelper);
JIT_HELPER(icallVMprJavaSendVirtual0);
JIT_HELPER(icallVMprJavaSendVirtual1);
JIT_HELPER(icallVMprJavaSendVirtualJ);
JIT_HELPER(icallVMprJavaSendVirtualL);
JIT_HELPER(icallVMprJavaSendVirtualF);
JIT_HELPER(icallVMprJavaSendVirtualD);
JIT_HELPER(_interpreterVoidStaticGlue);
JIT_HELPER(_interpreterSyncVoidStaticGlue);
JIT_HELPER(_interpreterGPR3StaticGlue);
JIT_HELPER(_interpreterSyncGPR3StaticGlue);
JIT_HELPER(_interpreterGPR3GPR4StaticGlue);
JIT_HELPER(_interpreterSyncGPR3GPR4StaticGlue);
JIT_HELPER(_interpreterFPR0FStaticGlue);
JIT_HELPER(_interpreterSyncFPR0FStaticGlue);
JIT_HELPER(_interpreterFPR0DStaticGlue);
JIT_HELPER(_interpreterSyncFPR0DStaticGlue);
JIT_HELPER(_nativeStaticHelperForUnresolvedGlue);
JIT_HELPER(_nativeStaticHelper);
JIT_HELPER(jitCollapseReferenceFrame);
JIT_HELPER(_interfaceCompeteSlot2);
JIT_HELPER(_interfaceSlotsUnavailable);
JIT_HELPER(__arrayCopy);
JIT_HELPER(__wordArrayCopy);
JIT_HELPER(__halfWordArrayCopy);
JIT_HELPER(__forwardArrayCopy);
JIT_HELPER(__forwardWordArrayCopy);
JIT_HELPER(__forwardHalfWordArrayCopy);
JIT_HELPER(__arrayCopy_dp);
JIT_HELPER(__wordArrayCopy_dp);
JIT_HELPER(__halfWordArrayCopy_dp);
JIT_HELPER(__forwardArrayCopy_dp);
JIT_HELPER(__forwardWordArrayCopy_dp);
JIT_HELPER(__forwardHalfWordArrayCopy_dp);
JIT_HELPER(__referenceArrayCopy);
JIT_HELPER(__generalArrayCopy);
JIT_HELPER(__encodeUTF16Big);
JIT_HELPER(__encodeUTF16Little);

JIT_HELPER(__quadWordArrayCopy_vsx);
JIT_HELPER(__forwardQuadWordArrayCopy_vsx);

JIT_HELPER(__postP10ForwardCopy);
JIT_HELPER(__postP10GenericCopy);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
#if defined(OMR_GC_FULL_POINTERS)
JIT_HELPER(ECP256MUL_PPC_full);
JIT_HELPER(ECP256MOD_PPC_full);
JIT_HELPER(ECP256addNoMod_PPC_full);
JIT_HELPER(ECP256subNoMod_PPC_full);
#endif /* defined(OMR_GC_FULL_POINTERS) */
#if defined(OMR_GC_COMPRESSED_POINTERS)
JIT_HELPER(ECP256MUL_PPC_compressed);
JIT_HELPER(ECP256MOD_PPC_compressed);
JIT_HELPER(ECP256addNoMod_PPC_compressed);
JIT_HELPER(ECP256subNoMod_PPC_compressed);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
#endif

#ifndef LINUX
JIT_HELPER(__compressString);
JIT_HELPER(__compressStringNoCheck);
JIT_HELPER(__compressStringJ);
JIT_HELPER(__compressStringNoCheckJ);
JIT_HELPER(__andORString);
#endif
JIT_HELPER(__arrayTranslateTRTO);
JIT_HELPER(__arrayTranslateTRTO255);
JIT_HELPER(__arrayTranslateTROT255);
JIT_HELPER(__arrayTranslateTROT);
JIT_HELPER(__arrayCmpVMX);
JIT_HELPER(__arrayCmpLenVMX);
JIT_HELPER(__arrayCmpScalar);
JIT_HELPER(__arrayCmpLenScalar);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
JIT_HELPER(AESKeyExpansion_PPC);
JIT_HELPER(AESEncryptVMX_PPC);
JIT_HELPER(AESDecryptVMX_PPC);
JIT_HELPER(AESEncrypt_PPC);
JIT_HELPER(AESDecrypt_PPC);
#if defined(OMR_GC_FULL_POINTERS)
JIT_HELPER(AESCBCDecrypt_PPC_full);
JIT_HELPER(AESCBCEncrypt_PPC_full);
#endif /* OMR_GC_FULL_POINTERS */
#if defined(OMR_GC_COMPRESSED_POINTERS)
JIT_HELPER(AESCBCDecrypt_PPC_compressed);
JIT_HELPER(AESCBCEncrypt_PPC_compressed);
#endif /* OMR_GC_COMPRESSED_POINTERS */
#endif

#elif defined(TR_HOST_ARM)
JIT_HELPER(__intDivide);
JIT_HELPER(__intRemainder);
JIT_HELPER(__longDivide);
JIT_HELPER(__longRemainder);
JIT_HELPER(__longShiftRightArithmetic);
JIT_HELPER(__longShiftRightLogical);
JIT_HELPER(__longShiftLeftLogical);
JIT_HELPER(__double2Long);
JIT_HELPER(__doubleRemainder);
JIT_HELPER(__double2Integer);
JIT_HELPER(__float2Long);
JIT_HELPER(__floatRemainder);
JIT_HELPER(__integer2Double);
JIT_HELPER(__long2Double);
JIT_HELPER(__long2Float);
JIT_HELPER(__arrayCopy);
JIT_HELPER(__compressString);
JIT_HELPER(__compressStringNoCheck);
JIT_HELPER(__compressStringJ);
JIT_HELPER(__compressStringNoCheckJ);
JIT_HELPER(__andORString);
JIT_HELPER(__multi64);
JIT_HELPER(_interpreterUnresolvedStaticGlue);
JIT_HELPER(_interpreterUnresolvedSpecialGlue);
JIT_HELPER(_interpreterUnresolvedDirectVirtualGlue);
JIT_HELPER(_interpreterUnresolvedClassGlue);
JIT_HELPER(_interpreterUnresolvedClassGlue2);
JIT_HELPER(_interpreterUnresolvedStringGlue);
JIT_HELPER(_interpreterUnresolvedMethodTypeGlue);
JIT_HELPER(_interpreterUnresolvedMethodHandleGlue);
JIT_HELPER(_interpreterUnresolvedCallSiteTableEntryGlue);
JIT_HELPER(_interpreterUnresolvedMethodTypeTableEntryGlue);
JIT_HELPER(_interpreterUnresolvedStaticDataGlue);
JIT_HELPER(_interpreterUnresolvedInstanceDataGlue);
JIT_HELPER(_interpreterUnresolvedStaticDataStoreGlue);
JIT_HELPER(_interpreterUnresolvedInstanceDataStoreGlue);
JIT_HELPER(_virtualUnresolvedHelper);
JIT_HELPER(_interfaceCallHelper);
JIT_HELPER(icallVMprJavaSendVirtual0);
JIT_HELPER(icallVMprJavaSendVirtual1);
JIT_HELPER(icallVMprJavaSendVirtualJ);
JIT_HELPER(icallVMprJavaSendVirtualF);
JIT_HELPER(icallVMprJavaSendVirtualD);
JIT_HELPER(_interpreterVoidStaticGlue);
JIT_HELPER(_interpreterSyncVoidStaticGlue);
JIT_HELPER(_interpreterGPR3StaticGlue);
JIT_HELPER(_interpreterSyncGPR3StaticGlue);
JIT_HELPER(_interpreterGPR3GPR4StaticGlue);
JIT_HELPER(_interpreterSyncGPR3GPR4StaticGlue);
JIT_HELPER(_interpreterFPR0FStaticGlue);
JIT_HELPER(_interpreterSyncFPR0FStaticGlue);
JIT_HELPER(_interpreterFPR0DStaticGlue);
JIT_HELPER(_interpreterSyncFPR0DStaticGlue);
JIT_HELPER(_nativeStaticHelper);
JIT_HELPER(_interfaceCompeteSlot2);
JIT_HELPER(_interfaceSlotsUnavailable);
JIT_HELPER(icallVMprJavaSendNativeStatic);
JIT_HELPER(icallVMprJavaSendStatic0);
JIT_HELPER(icallVMprJavaSendStatic1);
JIT_HELPER(icallVMprJavaSendStaticJ);
JIT_HELPER(icallVMprJavaSendStaticF);
JIT_HELPER(icallVMprJavaSendStaticD);
JIT_HELPER(icallVMprJavaSendStaticSync0);
JIT_HELPER(icallVMprJavaSendStaticSync1);
JIT_HELPER(icallVMprJavaSendStaticSyncJ);
JIT_HELPER(icallVMprJavaSendStaticSyncF);
JIT_HELPER(icallVMprJavaSendStaticSyncD);

#elif defined(TR_HOST_ARM64)
JIT_HELPER(_interpreterUnresolvedStaticGlue);
JIT_HELPER(_interpreterUnresolvedSpecialGlue);
JIT_HELPER(_interpreterUnresolvedDirectVirtualGlue);
JIT_HELPER(_interpreterUnresolvedClassGlue);
JIT_HELPER(_interpreterUnresolvedClassGlue2);
JIT_HELPER(_interpreterUnresolvedStringGlue);
JIT_HELPER(_interpreterUnresolvedConstantDynamicGlue);
JIT_HELPER(_interpreterUnresolvedMethodTypeGlue);
JIT_HELPER(_interpreterUnresolvedMethodHandleGlue);
JIT_HELPER(_interpreterUnresolvedCallSiteTableEntryGlue);
JIT_HELPER(_interpreterUnresolvedMethodTypeTableEntryGlue);
JIT_HELPER(_interpreterUnresolvedStaticDataGlue);
JIT_HELPER(_interpreterUnresolvedStaticDataStoreGlue);
JIT_HELPER(_interpreterUnresolvedInstanceDataGlue);
JIT_HELPER(_interpreterUnresolvedInstanceDataStoreGlue);
JIT_HELPER(_virtualUnresolvedHelper);
JIT_HELPER(_interfaceCallHelper);
JIT_HELPER(icallVMprJavaSendVirtual0);
JIT_HELPER(icallVMprJavaSendVirtual1);
JIT_HELPER(icallVMprJavaSendVirtualJ);
JIT_HELPER(icallVMprJavaSendVirtualF);
JIT_HELPER(icallVMprJavaSendVirtualD);
JIT_HELPER(_interpreterVoidStaticGlue);
JIT_HELPER(_interpreterSyncVoidStaticGlue);
JIT_HELPER(_interpreterIntStaticGlue);
JIT_HELPER(_interpreterSyncIntStaticGlue);
JIT_HELPER(_interpreterLongStaticGlue);
JIT_HELPER(_interpreterSyncLongStaticGlue);
JIT_HELPER(_interpreterFloatStaticGlue);
JIT_HELPER(_interpreterSyncFloatStaticGlue);
JIT_HELPER(_interpreterDoubleStaticGlue);
JIT_HELPER(_interpreterSyncDoubleStaticGlue);
JIT_HELPER(_nativeStaticHelper);
JIT_HELPER(_interfaceDispatch);

#elif defined(TR_HOST_S390)
JIT_HELPER(__double2Long);
JIT_HELPER(__doubleRemainder);
JIT_HELPER(__floatRemainder);
JIT_HELPER(__double2Integer);
JIT_HELPER(__integer2Double);
JIT_HELPER(__long2Double);
JIT_HELPER(__referenceArrayCopyHelper);
JIT_HELPER(_longDivide);
JIT_HELPER(_longRemainder);
JIT_HELPER(_longShiftLeft);
JIT_HELPER(_longShiftRight);
JIT_HELPER(_longUShiftRight);
JIT_HELPER(_interpreterUnresolvedStaticGlue);
JIT_HELPER(_interpreterUnresolvedSpecialGlue);
JIT_HELPER(_interpreterUnresolvedDirectVirtualGlue);
JIT_HELPER(_interpreterUnresolvedClassGlue);
JIT_HELPER(_interpreterUnresolvedClassGlue2);
JIT_HELPER(_interpreterUnresolvedStringGlue);
JIT_HELPER(_interpreterUnresolvedMethodTypeGlue);
JIT_HELPER(_interpreterUnresolvedMethodHandleGlue);
JIT_HELPER(_interpreterUnresolvedCallSiteTableEntryGlue);
JIT_HELPER(_interpreterUnresolvedMethodTypeTableEntryGlue);
JIT_HELPER(_interpreterUnresolvedStaticDataGlue);
JIT_HELPER(_interpreterUnresolvedStaticDataStoreGlue);
JIT_HELPER(_interpreterUnresolvedInstanceDataGlue);
JIT_HELPER(_interpreterUnresolvedInstanceDataStoreGlue);
JIT_HELPER(_virtualUnresolvedHelper);
JIT_HELPER(_interfaceCallHelper);
JIT_HELPER(_interfaceCallHelperSingleDynamicSlot);
JIT_HELPER(_interfaceCallHelperMultiSlots);
JIT_HELPER(icallVMprJavaSendVirtual0);
JIT_HELPER(icallVMprJavaSendVirtual1);
JIT_HELPER(icallVMprJavaSendVirtualJ);
JIT_HELPER(icallVMprJavaSendVirtualF);
JIT_HELPER(icallVMprJavaSendVirtualD);
JIT_HELPER(_interpreterVoidStaticGlue);
JIT_HELPER(_interpreterSyncVoidStaticGlue);
JIT_HELPER(_interpreterIntStaticGlue);
JIT_HELPER(_interpreterSyncIntStaticGlue);
JIT_HELPER(_interpreterLongStaticGlue);
JIT_HELPER(_interpreterSyncLongStaticGlue);
JIT_HELPER(_interpreterFloatStaticGlue);
JIT_HELPER(_interpreterSyncFloatStaticGlue);
JIT_HELPER(_interpreterDoubleStaticGlue);
JIT_HELPER(_interpreterSyncDoubleStaticGlue);
JIT_HELPER(_jitResolveConstantDynamic);
JIT_HELPER(_nativeStaticHelper);
JIT_HELPER(jitLookupInterfaceMethod);
JIT_HELPER(jitMethodIsNative);
JIT_HELPER(jitMethodIsSync);
JIT_HELPER(jitPreJNICallOffloadCheck);
JIT_HELPER(jitPostJNICallOffloadCheck);
JIT_HELPER(jitResolveClass);
JIT_HELPER(jitResolveClassFromStaticField);
JIT_HELPER(jitResolveField);
JIT_HELPER(jitResolvedFieldIsVolatile);
JIT_HELPER(jitResolveFieldSetter);
JIT_HELPER(jitResolveInterfaceMethod);
JIT_HELPER(jitResolveStaticField);
JIT_HELPER(jitResolveStaticFieldSetter);
JIT_HELPER(jitResolveString);
JIT_HELPER(jitResolveMethodType);
JIT_HELPER(jitResolveMethodHandle);
JIT_HELPER(jitResolveInvokeDynamic);
JIT_HELPER(jitResolveHandleMethod);
JIT_HELPER(jitResolveVirtualMethod);
JIT_HELPER(jitResolveSpecialMethod);
JIT_HELPER(jitResolveStaticMethod);
JIT_HELPER(jitRetranslateMethod);
JIT_HELPER(methodHandleJ2IGlue);
JIT_HELPER(methodHandleJ2I_unwrapper);
#endif

#define SET runtimeHelpers.setAddress
#define SET_CONST runtimeHelpers.setConstant

#if defined(TR_HOST_S390)

//
// Unfortunately, this initialization code can not be done on the JIT side since
// these services are required to be able to run AOT'ed code
//
static void initS390ArrayCopyTable(uint8_t* code)
   {
   // Right now, this is just a series of MVCs of 0 to 255 bytes
   // The optimized version should special case short moves (up to 8 bytes)
   // Linkage:  R14 is the return address
   //           R0 may be clobbered as part of the call
   //           R1,R2 are the dest/src arrays
   static const uint8_t templat3[]     =
                                      "\xD2\x00\x10\x00\x20\x00"  // MVC 0(x,R1),0(R2)
                                      "\x07\xFE\x00\x00"          // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00";
   static const uint8_t zeroTemplate[] =
                                      "\x07\xFE\x00\x00"
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00";


   memcpy(code, zeroTemplate, 16);
   for (int i=1; i<256; ++i)
      {
      memcpy(&code[i<<4], templat3, 16);
      code[(i<<4) + 1] = (i-1);
      }
   }

static void initS390ArraySetZeroTable(uint8_t* code)
   {
   // Right now, this is just a series of XCs of 0 to 255 bytes
   // The optimized version should special case short clears (up to 8 bytes)
   // Linkage:  R14 is the return address
   //           R0 may be clobbered as part of the call
   //           R1 is the storage to clear
   static const uint8_t templat3[]     =
                                      "\xD7\x00\x10\x00\x10\x00"  // XC 0(x,R1),0(R1)
                                      "\x07\xFE"                  // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00";
   static const uint8_t zeroTemplate[] =
                                      "\x07\xFE\x00\x00"          // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00";


   memcpy(code, zeroTemplate, 16);
   for (int i=1; i<256; ++i)
      {
      memcpy(&code[i<<4], templat3, 16);
      code[(i<<4) + 1] = (i-1);
      }
   }

static void initS390ArraySetGeneralTable(uint8_t* code)
   {
   // Right now, this is just a series of MVCs of 0 to 255 bytes
   // The optimized version should special case short moves (up to 8 bytes)
   // Linkage:  R14 is the return address
   //           R0 may be clobbered as part of the call
   //           R1 is the storage to clear
   //           R2 contains the byte value
   static const uint8_t templat3[]     =
                                      "\x42\x20\x10\x00"          // STC R2,0(,R1)
                                      "\xD2\x00\x10\x01\x10\x00"  // MVC 0(x,R1),1(R1)
                                      "\x07\xFE"                  // BR R14
                                      "\x00\x00\x00\x00";
   static const uint8_t zeroTemplate[] =
                                      "\x07\xFE\x00\x00"          // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00";
   static const uint8_t oneTemplate[]  =
                                      "\x42\x20\x10\x00"          // STC R2,0(,R1)
                                      "\x07\xFE\x00\x00"          // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00";


   memcpy(code, zeroTemplate, 16);
   memcpy(&code[16], oneTemplate, 16);
   for (int i=2; i<256; ++i)
      {
      memcpy(&code[i<<4], templat3, 16);
      code[(i<<4) + 5] = (i-2);
      }
   }

static void initS390ArrayCmpTable(uint8_t* code)
   {
   // Right now, this is just a series of CLCs of 0 to 255 bytes
   // The optimized version should special case short compares (up to 8 bytes)
   // Linkage:  R14 is the return address
   //           R1,R2 are the dest/src arrays on input
   //           R0 may be clobbered as part of the call
   //           CC is set based on CLC operation for perusal from mainline code
   static const uint8_t templat3[]     =
                                      "\xD5\x00\x10\x00\x20\x00"  // CLC 0(x,R1),0(R2)
                                      "\x07\xFE\x00\x00"          // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00";
   static const uint8_t zeroTemplate[] =
                                      "\x17\x00"                  // XR R0,R0 (set CC to 0)
                                      "\x07\xFE"                  // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00";


   memcpy(code, zeroTemplate, 16);
   for (int i=1; i<256; ++i)
      {
      memcpy(&code[i<<4], templat3, 16);
      code[(i<<4) + 1] = (i-1);
      }
   }

static void initS390ArrayTranslateAndTestTable(uint8_t* code)
   {
   // Right now, this is just a series of CLCs of 0 to 255 bytes
   // The optimized version should special case short compares (up to 8 bytes)
   // Linkage:  R14 is the return address
   //           R1 is the start address of TRT's input (also output)
   //           R2 is clobbered by TRT
   //           R3 is the table address of TRT's input
   //           R0 may be clobbered as part of the call
   //           CC is set based on CLC operation for perusal from mainline code
   static const uint8_t templat3[]     =
                                      "\xDD\x00\x10\x00\x30\x00"  // TRT 0(x,R1),0(R3)
                                      "\x07\xFE\x00\x00"          // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00";
   static const uint8_t zeroTemplate[] =
                                      "\x19\x00"                  // CR R0,R0 (set zero flag)
                                      "\x07\xFE"                  // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00";

   memcpy(code, zeroTemplate, 16);
   for (int i=1; i<256; ++i)
      {
      memcpy(&code[i<<4], templat3, 16);
      code[(i<<4) + 1] = (i-1);
      }
   }

static void initS390Long2StringTable(uint8_t* code)
   {
   // Right now, this is just a series of CLCs of 0 to 255 bytes
   // The optimized version should special case short compares (up to 8 bytes)
   // Linkage:  R14 is the return address
   //           R1 is the start address of UNPKU's input
   //           R2 is the start address of UNPKU's output
   //           CC is modified by UNPKU
   static const uint8_t templat3[]     =
                                      "\xE2\x00\x20\x00\x10\x00"  // UNPKU 0(x,R2),0(R1)
                                      "\x07\xFE\x00\x00"          // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00";
   static const uint8_t zeroTemplate[] =
                                      "\x07\xFE"                  // BR R14
                                      "\x00\x00"
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00";


   memcpy(code, zeroTemplate, 16);
   for (int i=1; i<256; ++i)
      {
      memcpy(&code[i<<4], templat3, 16);
      code[(i<<4) + 1] = (i << 1) - 1;
      }
   }

static void initS390ArrayBitOpMemTable(uint8_t* code, uint8_t opcode)
   {
   // Generate a series of SS instructions of 0 to 255 bytes
   // The optimized version should special case short moves (up to 8 bytes)
   // Linkage:  R14 is the return address
   //           R0 may be clobbered as part of the call
   //           R1,R2 are the dest/src arrays
   static uint8_t templat3[]     =
                                      "\x00\x00\x10\x00\x20\x00"  // SSInstruction 0(x,R1),0(R2)
                                      "\x07\xFE\x00\x00"          // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00";
   static const uint8_t zeroTemplate[] =
                                      "\x07\xFE\x00\x00"     // BR R14
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00"
                                      "\x00\x00\x00\x00";

   templat3[0] = opcode;
   memcpy(code, zeroTemplate, 16);
   for (int i=1; i<256; ++i)
      {
      memcpy(&code[i<<4], templat3, 16);
      code[(i<<4) + 1] = (i-1);
      }
   }


static void initS390WriteOnceHelpers(J9JITConfig *jitConfig,
                                     enum TR_RuntimeHelper arrayCopyHelper,       enum TR_RuntimeHelper arraySetZeroHelper,
                                     enum TR_RuntimeHelper arraySetGeneralHelper, enum TR_RuntimeHelper arrayCmpHelper)
   {
      J9JavaVM *javaVM = jitConfig->javaVM;
      PORT_ACCESS_FROM_JAVAVM(javaVM);
      UDATA codeSize = (9) * 4096; // 9 tables of 4K each

      /* Memory Leak: The base address of this allocated memory is never stored, so it can't possibly ever be de-allocated.
       * If ever this memory is to be de-allocated then a reference to the J9PortVmenIdentifier must be maintained and passed
       * to j9vmem_free_memory.
       */
      J9PortVmemParams vmemParams;
      j9vmem_vmem_params_init(&vmemParams);

      UDATA allocMode =J9PORT_VMEM_MEMORY_MODE_READ | J9PORT_VMEM_MEMORY_MODE_WRITE | J9PORT_VMEM_MEMORY_MODE_EXECUTE | J9PORT_VMEM_MEMORY_MODE_COMMIT;
      vmemParams.mode = allocMode;
      vmemParams.category = J9MEM_CATEGORY_JIT;

      UDATA *pageSizes = j9vmem_supported_page_sizes();
      UDATA *pageFlags = j9vmem_supported_page_flags();
      vmemParams.pageSize  = pageSizes[0]; // Default page size is always the first element *
      vmemParams.pageFlags = pageFlags[0];

      // Requested size should be a multiple of page size.
      vmemParams.byteAmount = (codeSize + vmemParams.pageSize - 1) & (~(vmemParams.pageSize - 1));

      // Alignment should be to 4k.
      vmemParams.alignmentInBytes = 4096;

      J9PortVmemIdentifier identifier;
      uint8_t *tables = (uint8_t *) j9vmem_reserve_memory_ex(&identifier, &vmemParams);

      uint8_t *arrayCopyTable       = &tables[4096*0];
      uint8_t *arraySetZeroTable    = &tables[4096*1];
      uint8_t *arraySetGeneralTable = &tables[4096*2];
      uint8_t *arrayCmpTable        = &tables[4096*3];
      uint8_t *arrayTranslateAndTestTable = &tables[4096*4];
      uint8_t *long2StringTable      = &tables[4096*5];
      uint8_t *arrayXORTable         = &tables[4096*6];
      uint8_t *arrayORTable          = &tables[4096*7];
      uint8_t *arrayANDTable         = &tables[4096*8];

      initS390ArrayCopyTable(arrayCopyTable);
      initS390ArraySetZeroTable(arraySetZeroTable);
      initS390ArraySetGeneralTable(arraySetGeneralTable);
      initS390ArrayCmpTable(arrayCmpTable);
      initS390ArrayTranslateAndTestTable(arrayTranslateAndTestTable);
      initS390Long2StringTable(long2StringTable);
      initS390ArrayBitOpMemTable(arrayXORTable, 0xD7);
      initS390ArrayBitOpMemTable(arrayORTable,  0xD6);
      initS390ArrayBitOpMemTable(arrayANDTable, 0xD4);

      SET_CONST(TR_S390arrayCopyHelper,        (void *) arrayCopyTable);
      SET_CONST(TR_S390arraySetZeroHelper,     (void *) arraySetZeroTable);
      SET_CONST(TR_S390arraySetGeneralHelper,  (void *) arraySetGeneralTable);
      SET_CONST(TR_S390arrayCmpHelper,         (void *) arrayCmpTable);
      SET_CONST(TR_S390arrayTranslateAndTestHelper,         (void *) arrayTranslateAndTestTable);
      SET_CONST(TR_S390long2StringHelper,      (void *) long2StringTable);
      SET_CONST(TR_S390arrayXORHelper,         (void *) arrayXORTable);
      SET_CONST(TR_S390arrayORHelper,          (void *) arrayORTable);
      SET_CONST(TR_S390arrayANDHelper,         (void *) arrayANDTable);
   }
#endif

void initializeCodeRuntimeHelperTable(J9JITConfig *jitConfig, char isSMP)
   {
   SET(TR_j2iTransition,                                    (void *)j2iTransition,                               TR_Helper);
   SET(TR_icallVMprJavaSendStatic0,                         (void *)icallVMprJavaSendStatic0,                    TR_Helper);
   SET(TR_icallVMprJavaSendStatic1,                         (void *)icallVMprJavaSendStatic1,                    TR_Helper);
   SET(TR_icallVMprJavaSendStaticJ,                         (void *)icallVMprJavaSendStaticJ,                    TR_Helper);
   SET(TR_icallVMprJavaSendStaticF,                         (void *)icallVMprJavaSendStaticF,                    TR_Helper);
   SET(TR_icallVMprJavaSendStaticD,                         (void *)icallVMprJavaSendStaticD,                    TR_Helper);
   SET(TR_icallVMprJavaSendStaticSync0,                     (void *)icallVMprJavaSendStaticSync0,                TR_Helper);
   SET(TR_icallVMprJavaSendStaticSync1,                     (void *)icallVMprJavaSendStaticSync1,                TR_Helper);
   SET(TR_icallVMprJavaSendStaticSyncJ,                     (void *)icallVMprJavaSendStaticSyncJ,                TR_Helper);
   SET(TR_icallVMprJavaSendStaticSyncF,                     (void *)icallVMprJavaSendStaticSyncF,                TR_Helper);
   SET(TR_icallVMprJavaSendStaticSyncD,                     (void *)icallVMprJavaSendStaticSyncD,                TR_Helper);
   SET(TR_icallVMprJavaSendInvokeExact0,                    (void *)icallVMprJavaSendInvokeExact0,               TR_Helper);
   SET(TR_icallVMprJavaSendInvokeExact1,                    (void *)icallVMprJavaSendInvokeExact1,               TR_Helper);
   SET(TR_icallVMprJavaSendInvokeExactJ,                    (void *)icallVMprJavaSendInvokeExactJ,               TR_Helper);
   SET(TR_icallVMprJavaSendInvokeExactL,                    (void *)icallVMprJavaSendInvokeExactL,               TR_Helper);
   SET(TR_icallVMprJavaSendInvokeExactF,                    (void *)icallVMprJavaSendInvokeExactF,               TR_Helper);
   SET(TR_icallVMprJavaSendInvokeExactD,                    (void *)icallVMprJavaSendInvokeExactD,               TR_Helper);
   SET(TR_icallVMprJavaSendInvokeWithArguments,             (void *)icallVMprJavaSendInvokeWithArgumentsHelperL, TR_Helper);
   SET(TR_icallVMprJavaSendNativeStatic,                    (void *)icallVMprJavaSendNativeStatic,               TR_Helper);
   SET_CONST(TR_initialInvokeExactThunk_unwrapper,          (void *)initialInvokeExactThunk_unwrapper);

   // Platforms that implement these can override below
   SET(TR_methodHandleJ2IGlue,                     (void *)0, TR_Helper);
   SET(TR_methodHandleJ2I_unwrapper,               (void *)0, TR_Helper);

#if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390)) && defined(J9VM_JIT_NEW_DUAL_HELPERS)
   SET(TR_checkCast,                  (void *)jitCheckCast,              TR_CHelper);
   SET(TR_checkCastForArrayStore,     (void *)jitCheckCastForArrayStore, TR_CHelper);
#else
   SET(TR_checkCast,                  (void *)jitCheckCast,              TR_Helper);
   SET(TR_checkCastForArrayStore,     (void *)jitCheckCastForArrayStore, TR_Helper);
#endif
#if defined(TR_HOST_X86) || defined(TR_HOST_POWER) || (defined(TR_HOST_S390) && defined(J9VM_JIT_NEW_DUAL_HELPERS))
   SET(TR_instanceOf,                 (void *)fast_jitInstanceOf,        TR_CHelper);
   SET(TR_checkAssignable,            (void *)fast_jitCheckAssignable,   TR_CHelper);
#else
   SET(TR_instanceOf,                 (void *)jitInstanceOf,             TR_Helper);
#endif
   SET(TR_induceOSRAtCurrentPC,       (void *)jitInduceOSRAtCurrentPC,   TR_Helper);
   SET(TR_induceOSRAtCurrentPCAndRecompile,       (void *)jitInduceOSRAtCurrentPCAndRecompile,   TR_Helper);
#if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390)) && defined(J9VM_JIT_NEW_DUAL_HELPERS)
   SET(TR_monitorEntry,               (void *)jitMonitorEntry,           TR_CHelper);
   SET(TR_methodMonitorEntry,         (void *)jitMethodMonitorEntry,     TR_CHelper);
   SET(TR_monitorExit,                (void *)jitMonitorExit,            TR_CHelper);
   SET(TR_methodMonitorExit,          (void *)jitMethodMonitorExit,      TR_CHelper);
#else
   SET(TR_monitorEntry,               (void *)jitMonitorEntry,           TR_Helper);
   SET(TR_methodMonitorEntry,         (void *)jitMethodMonitorEntry,     TR_Helper);
   SET(TR_monitorExit,                (void *)jitMonitorExit,            TR_Helper);
   SET(TR_methodMonitorExit,          (void *)jitMethodMonitorExit,      TR_Helper);
#endif
   SET(TR_transactionEntry,           (void *)0,                         TR_Helper);
   SET(TR_transactionAbort,           (void *)0,                         TR_Helper);
   SET(TR_transactionExit,            (void *)0,                         TR_Helper);
   SET(TR_asyncCheck,                 (void *)jitCheckAsyncMessages,     TR_Helper);
   SET(TR_estimateGPU,                (void *)estimateGPU,               TR_Helper);
   SET(TR_regionEntryGPU,             (void *)regionEntryGPU,            TR_Helper);
   SET(TR_regionExitGPU,              (void *)regionExitGPU,             TR_Helper);
   SET(TR_copyToGPU,                  (void *)copyToGPU,                 TR_Helper);
   SET(TR_copyFromGPU,                (void *)copyFromGPU,               TR_Helper);
   SET(TR_allocateGPUKernelParms,     (void *)allocateGPUKernelParms,    TR_Helper);
   SET(TR_launchGPUKernel,            (void *)launchGPUKernel,           TR_Helper);
   SET(TR_invalidateGPU,              (void *)invalidateGPU,             TR_Helper);
   SET(TR_getStateGPU,                (void *)getStateGPU,               TR_Helper);
   SET(TR_flushGPU,                   (void *)flushGPU,                  TR_Helper);
   SET(TR_callGPU,                    (void *)callGPU,                   TR_Helper);

#if defined(TR_HOST_X86) || defined(TR_HOST_POWER)
   SET(TR_referenceArrayCopy,         (void *)jitReferenceArrayCopy,     TR_Helper);
#endif

#if !defined(TR_HOST_64BIT)
#if !defined (TR_HOST_X86)
   SET(TR_volatileReadLong,           (void *)jitVolatileReadLong,       TR_Helper);
   SET(TR_volatileWriteLong,          (void *)jitVolatileWriteLong,      TR_Helper);
#endif
#if defined(TR_HOST_POWER)
   SET(TR_volatileReadDouble,         (void *)jitVolatileReadDouble,     TR_Helper);
   SET(TR_volatileWriteDouble,        (void *)jitVolatileWriteDouble,    TR_Helper);
#endif
#endif

#if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390)) && defined(J9VM_JIT_NEW_DUAL_HELPERS)
   SET(TR_newObject,                  (void *)jitNewObject,              TR_CHelper);
   SET(TR_newValue,                   (void *)jitNewValue,              TR_CHelper);
   SET(TR_newArray,                   (void *)jitNewArray,               TR_CHelper);
   SET(TR_aNewArray,                  (void *)jitANewArray,              TR_CHelper);

   SET(TR_newObjectNoZeroInit,        (void *)jitNewObjectNoZeroInit, TR_CHelper);
   SET(TR_newValueNoZeroInit,        (void *)jitNewValueNoZeroInit, TR_CHelper);
   SET(TR_newArrayNoZeroInit,         (void *)jitNewArrayNoZeroInit,  TR_CHelper);
   SET(TR_aNewArrayNoZeroInit,        (void *)jitANewArrayNoZeroInit, TR_CHelper);
#else
   SET(TR_newObject,                  (void *)jitNewObject,              TR_Helper);
   SET(TR_newValue,                  (void *)jitNewValue,              TR_Helper);
   SET(TR_newArray,                   (void *)jitNewArray,               TR_Helper);
   SET(TR_aNewArray,                  (void *)jitANewArray,              TR_Helper);

   SET(TR_newObjectNoZeroInit,        (void *)jitNewObjectNoZeroInit, TR_Helper);
   SET(TR_newValueNoZeroInit,        (void *)jitNewValueNoZeroInit, TR_Helper);
   SET(TR_newArrayNoZeroInit,         (void *)jitNewArrayNoZeroInit,  TR_Helper);
   SET(TR_aNewArrayNoZeroInit,        (void *)jitANewArrayNoZeroInit, TR_Helper);
#endif

   SET(TR_getFlattenableField,        (void *)jitGetFlattenableField, TR_Helper);
   SET(TR_withFlattenableField,        (void *)jitWithFlattenableField, TR_Helper);
   SET(TR_putFlattenableField,        (void *)jitPutFlattenableField, TR_Helper);
   SET(TR_getFlattenableStaticField,        (void *)jitGetFlattenableStaticField, TR_Helper);
   SET(TR_putFlattenableStaticField,        (void *)jitPutFlattenableStaticField, TR_Helper);
   SET(TR_ldFlattenableArrayElement,        (void *)jitLoadFlattenableArrayElement, TR_Helper);
   SET(TR_strFlattenableArrayElement,        (void *)jitStoreFlattenableArrayElement, TR_Helper);

   SET(TR_acmpHelper,                  (void *)jitAcmpHelper, TR_Helper);
   SET(TR_multiANewArray,             (void *)jitAMultiNewArray, TR_Helper);
   SET(TR_aThrow,                     (void *)jitThrowException, TR_Helper);

   SET(TR_nullCheck,                  (void *)jitThrowNullPointerException,          TR_Helper);
   SET(TR_methodTypeCheck,            (void *)jitThrowWrongMethodTypeException,      TR_Helper);
   SET(TR_incompatibleReceiver,       (void *)jitThrowIncompatibleReceiver,          TR_Helper);
   SET(TR_IncompatibleClassChangeError, (void *)jitThrowIncompatibleClassChangeError, TR_Helper);
   SET(TR_arrayBoundsCheck,           (void *)jitThrowArrayIndexOutOfBounds,         TR_Helper);
   SET(TR_divCheck,                   (void *)jitThrowArithmeticException,           TR_Helper);
   SET(TR_arrayStoreException,        (void *)jitThrowArrayStoreException,           TR_Helper);
#if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390)) && defined(J9VM_JIT_NEW_DUAL_HELPERS)
   SET(TR_typeCheckArrayStore,        (void *)jitTypeCheckArrayStoreWithNullCheck,   TR_CHelper);
#else
   SET(TR_typeCheckArrayStore,        (void *)jitTypeCheckArrayStoreWithNullCheck,   TR_Helper);
#endif

#if defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM64)
   SET(TR_softwareReadBarrier,                              (void *)jitSoftwareReadBarrier,                         TR_Helper);
#endif
   SET(TR_writeBarrierStore,                                (void *)jitWriteBarrierStore,                              TR_Helper);
   SET(TR_writeBarrierStoreGenerational,                    (void *)jitWriteBarrierStoreGenerational,                  TR_Helper);
   SET(TR_writeBarrierStoreGenerationalAndConcurrentMark,   (void *)jitWriteBarrierStoreGenerationalAndConcurrentMark, TR_Helper);
   SET(TR_writeBarrierStoreRealTimeGC,                      (void *)jitWriteBarrierStoreMetronome,                     TR_Helper);
   SET(TR_writeBarrierClassStoreRealTimeGC,                 (void *)jitWriteBarrierClassStoreMetronome,                TR_Helper);
   SET(TR_writeBarrierBatchStore,                           (void *)jitWriteBarrierBatchStore,                         TR_Helper);

   SET(TR_stackOverflow,              (void *)jitStackOverflow,              TR_Helper);
   SET(TR_reportMethodEnter,          (void *)jitReportMethodEnter,          TR_Helper);
   SET(TR_reportStaticMethodEnter,    (void *)jitReportStaticMethodEnter,    TR_Helper);
   SET(TR_reportMethodExit,           (void *)jitReportMethodExit,           TR_Helper);
   SET(TR_reportFinalFieldModified,   (void *)jitReportFinalFieldModified,   TR_Helper);
   SET(TR_acquireVMAccess,            (void *)jitAcquireVMAccess,            TR_Helper);
   SET(TR_jitReportInstanceFieldRead, (void *)jitReportInstanceFieldRead,    TR_Helper);
   SET(TR_jitReportInstanceFieldWrite,(void *)jitReportInstanceFieldWrite,   TR_Helper);
   SET(TR_jitReportStaticFieldRead,   (void *)jitReportStaticFieldRead,      TR_Helper);
   SET(TR_jitReportStaticFieldWrite,  (void *)jitReportStaticFieldWrite,     TR_Helper);

   SET(TR_jitResolveFieldDirect,              (void *)jitResolveFieldDirect,              TR_Helper);
   SET(TR_jitResolveFieldSetterDirect,        (void *)jitResolveFieldSetterDirect,        TR_Helper);
   SET(TR_jitResolveStaticFieldDirect,        (void *)jitResolveStaticFieldDirect,        TR_Helper);
   SET(TR_jitResolveStaticFieldSetterDirect,  (void *)jitResolveStaticFieldSetterDirect,  TR_Helper);
#if defined(TR_HOST_X86) || defined(TR_HOST_POWER)
   SET(TR_jitCheckIfFinalizeObject,   (void *)fast_jitCheckIfFinalizeObject, TR_CHelper);
#else
   SET(TR_jitCheckIfFinalizeObject,   (void *)jitCheckIfFinalizeObject,      TR_Helper);
#endif
#if !defined(TR_HOST_ARM)
   SET(TR_releaseVMAccess,            (void *)jitReleaseVMAccess,            TR_Helper);
#endif
   SET(TR_throwCurrentException,      (void *)jitThrowCurrentException,      TR_Helper);
   SET(TR_throwUnreportedException,   (void *)jitThrowUnreportedException,   TR_Helper); // used for throwing exceptions from synthetic handlers

#if defined (TR_HOST_X86)
   SET(TR_throwClassCastException,    (void *)jitThrowClassCastException,    TR_Helper);
#endif

   SET(TR_newInstanceImplAccessCheck, (void *)jitNewInstanceImplAccessCheck,         TR_Helper);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   // AES crypto helpers
   //
#if defined (TR_HOST_X86)
   SET(TR_isAESSupportedByHardware,    (void *) 0,                         TR_Helper);
#if defined(OMR_GC_COMPRESSED_POINTERS)
   if (TR::Compiler->om.compressObjectReferences())
      {
      SET(TR_doAESInHardwareInner,        (void *) doAESInHardwareJitCompressed,        TR_Helper);
      SET(TR_expandAESKeyInHardwareInner, (void *) expandAESKeyInHardwareJitCompressed, TR_Helper);
      }
   else
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
      {
#if defined(OMR_GC_FULL_POINTERS)
      SET(TR_doAESInHardwareInner,        (void *) doAESInHardwareJitFull,        TR_Helper);
      SET(TR_expandAESKeyInHardwareInner, (void *) expandAESKeyInHardwareJitFull, TR_Helper);
#endif /* defined(OMR_GC_FULL_POINTERS) */
      }
#elif defined (TR_HOST_POWER)
   SET(TR_PPCAESKeyExpansion, (void *) AESKeyExpansion_PPC, TR_Helper);
   SET(TR_PPCAESEncryptVMX,   (void *) AESEncryptVMX_PPC,   TR_Helper);
   SET(TR_PPCAESDecryptVMX,   (void *) AESDecryptVMX_PPC,   TR_Helper);
   SET(TR_PPCAESEncrypt,      (void *) AESEncrypt_PPC,      TR_Helper);
   SET(TR_PPCAESDecrypt,      (void *) AESDecrypt_PPC,      TR_Helper);
#if defined(OMR_GC_COMPRESSED_POINTERS)
   if (TR::Compiler->om.compressObjectReferences())
      {
      SET(TR_PPCAESCBCDecrypt,   (void *) AESCBCDecrypt_PPC_compressed,   TR_Helper);
      SET(TR_PPCAESCBCEncrypt,   (void *) AESCBCEncrypt_PPC_compressed,   TR_Helper);
      }
   else
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
      {
#if defined(OMR_GC_FULL_POINTERS)
      SET(TR_PPCAESCBCDecrypt,   (void *) AESCBCDecrypt_PPC_full,   TR_Helper);
      SET(TR_PPCAESCBCEncrypt,   (void *) AESCBCEncrypt_PPC_full,   TR_Helper);
#endif /* defined(OMR_GC_FULL_POINTERS) */
      }
#else
   SET(TR_isAESSupportedByHardware,    (void *) 0, TR_Helper);
   SET(TR_doAESInHardwareInner,        (void *) 0, TR_Helper);
   SET(TR_expandAESKeyInHardwareInner, (void *) 0, TR_Helper);
#endif

#endif

   SET(TR_jitRetranslateCaller,         (void *)jitRetranslateCaller,                TR_Helper);
   SET(TR_jitRetranslateCallerWithPrep, (void *)jitRetranslateCallerWithPreparation, TR_Helper);

#ifdef TR_HOST_X86

   // --------------------------------- X86 ------------------------------------

   SET(TR_X86resolveIPicClass,                        (void *)resolveIPicClass,                TR_Helper);
   SET(TR_X86populateIPicSlotClass,                   (void *)populateIPicSlotClass,           TR_Helper);
   SET(TR_X86populateIPicSlotCall,                    (void *)populateIPicSlotCall,            TR_Helper);
   SET(TR_X86dispatchInterpretedFromIPicSlot,         (void *)dispatchInterpretedFromIPicSlot, TR_Helper);
   SET(TR_X86IPicLookupDispatch,                      (void *)IPicLookupDispatch,              TR_Helper);
   SET(TR_X86resolveVPicClass,                        (void *)resolveVPicClass,                TR_Helper);
   SET(TR_X86populateVPicSlotClass,                   (void *)populateVPicSlotClass,           TR_Helper);
   SET(TR_X86populateVPicSlotCall,                    (void *)populateVPicSlotCall,            TR_Helper);
   SET(TR_X86dispatchInterpretedFromVPicSlot,         (void *)dispatchInterpretedFromVPicSlot, TR_Helper);
   SET(TR_X86populateVPicVTableDispatch,              (void *)populateVPicVTableDispatch,      TR_Helper);

   SET(TR_X86interpreterUnresolvedStaticGlue,         (void *)interpreterUnresolvedStaticGlue,  TR_Helper);
   SET(TR_X86interpreterUnresolvedSpecialGlue,        (void *)interpreterUnresolvedSpecialGlue,  TR_Helper);

   SET(TR_X86interpreterUnresolvedClassGlue,                (void *)interpreterUnresolvedClassGlue,                TR_Helper);
   SET(TR_X86interpreterUnresolvedClassFromStaticFieldGlue, (void *)interpreterUnresolvedClassFromStaticFieldGlue, TR_Helper);
   SET(TR_X86interpreterUnresolvedStringGlue,               (void *)interpreterUnresolvedStringGlue,               TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeGlue,              (void *)interpreterUnresolvedMethodTypeGlue,           TR_Helper);
   SET(TR_interpreterUnresolvedMethodHandleGlue,            (void *)interpreterUnresolvedMethodHandleGlue,         TR_Helper);
   SET(TR_interpreterUnresolvedCallSiteTableEntryGlue,      (void *)interpreterUnresolvedCallSiteTableEntryGlue,   TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeTableEntryGlue,    (void *)interpreterUnresolvedMethodTypeTableEntryGlue, TR_Helper);
   SET(TR_X86interpreterUnresolvedStaticFieldGlue,          (void *)interpreterUnresolvedStaticFieldGlue,          TR_Helper);
   SET(TR_X86interpreterUnresolvedStaticFieldSetterGlue,    (void *)interpreterUnresolvedStaticFieldSetterGlue,    TR_Helper);
   SET(TR_X86interpreterUnresolvedFieldGlue,                (void *)interpreterUnresolvedFieldGlue,                TR_Helper);
   SET(TR_X86interpreterUnresolvedFieldSetterGlue,          (void *)interpreterUnresolvedFieldSetterGlue,          TR_Helper);
   SET(TR_X86interpreterUnresolvedConstantDynamicGlue,      (void *)interpreterUnresolvedConstantDynamicGlue,      TR_Helper);
   SET(TR_X86interpreterStaticAndSpecialGlue,               (void *)interpreterStaticAndSpecialGlue,               TR_Helper);

   SET(TR_X86prefetchTLH,                (void *)prefetchTLH,                 TR_Helper);
   SET(TR_X86newPrefetchTLH,             (void *)newPrefetchTLH,              TR_Helper);
   SET(TR_X86CodeCachePrefetchHelper,    (void *)prefetchTLH,                 TR_Helper); // needs to be set while compiling

#ifdef TR_HOST_64BIT

   // -------------------------------- AMD64 ------------------------------------

   SET(TR_AMD64floatRemainder,                        (void *)SSEfloatRemainder,  TR_Helper);
   SET(TR_AMD64doubleRemainder,                       (void *)SSEdoubleRemainder, TR_Helper);

   SET(TR_AMD64icallVMprJavaSendVirtual0,             (void *)icallVMprJavaSendVirtual0, TR_Helper);
   SET(TR_AMD64icallVMprJavaSendVirtual1,             (void *)icallVMprJavaSendVirtual1, TR_Helper);
   SET(TR_AMD64icallVMprJavaSendVirtualJ,             (void *)icallVMprJavaSendVirtualJ, TR_Helper);
   SET(TR_AMD64icallVMprJavaSendVirtualL,             (void *)icallVMprJavaSendVirtualL, TR_Helper);
   SET(TR_AMD64icallVMprJavaSendVirtualF,             (void *)icallVMprJavaSendVirtualF, TR_Helper);
   SET(TR_AMD64icallVMprJavaSendVirtualD,             (void *)icallVMprJavaSendVirtualD, TR_Helper);

   SET(TR_AMD64jitCollapseJNIReferenceFrame,          (void *)jitCollapseJNIReferenceFrame,   TR_Helper);
   SET(TR_AMD64compressString,                        (void *)compressString,            TR_Helper);
   SET(TR_AMD64compressStringNoCheck,                 (void *)compressStringNoCheck,     TR_Helper);
   SET(TR_AMD64compressStringJ,                       (void *)compressStringJ,           TR_Helper);
   SET(TR_AMD64compressStringNoCheckJ,                (void *)compressStringNoCheckJ,    TR_Helper);
   SET(TR_AMD64andORString,                           (void *)andORString,               TR_Helper);
   SET(TR_AMD64arrayTranslateTRTO,                    (void *)arrayTranslateTRTO,        TR_Helper);
   SET(TR_AMD64arrayTranslateTROTNoBreak,             (void *)arrayTranslateTROTNoBreak, TR_Helper);
   SET(TR_AMD64arrayTranslateTROT,                    (void *)arrayTranslateTROT,        TR_Helper);
   SET(TR_AMD64encodeUTF16Big,                        (void *)encodeUTF16Big,            TR_Helper);
   SET(TR_AMD64encodeUTF16Little,                     (void *)encodeUTF16Little,         TR_Helper);
#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
   SET(TR_AMD64doAESENCEncrypt,                       (void *)doAESENCEncrypt,           TR_Helper);
   SET(TR_AMD64doAESENCDecrypt,                       (void *)doAESENCDecrypt,           TR_Helper);
#endif /* J9VM_OPT_JAVA_CRYPTO_ACCELERATION */
#if defined(LINUX)
   SET(TR_AMD64clockGetTime,                          (void *)clock_gettime, TR_System);
#endif
   SET(TR_AMD64JitMonitorEnterReserved,                    (void *)jitMonitorEnterReserved,                    TR_CHelper);
   SET(TR_AMD64JitMonitorEnterReservedPrimitive,           (void *)jitMonitorEnterReservedPrimitive,           TR_CHelper);
   SET(TR_AMD64JitMonitorEnterPreservingReservation,       (void *)jitMonitorEnterPreservingReservation,       TR_CHelper);
   SET(TR_AMD64JitMonitorExitReserved,                     (void *)jitMonitorExitReserved,                     TR_CHelper);
   SET(TR_AMD64JitMonitorExitReservedPrimitive,            (void *)jitMonitorExitReservedPrimitive,            TR_CHelper);
   SET(TR_AMD64JitMonitorExitPreservingReservation,        (void *)jitMonitorExitPreservingReservation,        TR_CHelper);
   SET(TR_AMD64JitMethodMonitorEnterReserved,              (void *)jitMethodMonitorEnterReserved,              TR_CHelper);
   SET(TR_AMD64JitMethodMonitorEnterReservedPrimitive,     (void *)jitMethodMonitorEnterReservedPrimitive,     TR_CHelper);
   SET(TR_AMD64JitMethodMonitorEnterPreservingReservation, (void *)jitMethodMonitorEnterPreservingReservation, TR_CHelper);
   SET(TR_AMD64JitMethodMonitorExitPreservingReservation,  (void *)jitMethodMonitorExitPreservingReservation,  TR_CHelper);
   SET(TR_AMD64JitMethodMonitorExitReservedPrimitive,      (void *)jitMethodMonitorExitReservedPrimitive,      TR_CHelper);
   SET(TR_AMD64JitMethodMonitorExitReserved,               (void *)jitMethodMonitorExitReserved,               TR_CHelper);

   SET(TR_methodHandleJ2IGlue,                        (void *)methodHandleJ2IGlue,       TR_Helper);
   SET(TR_methodHandleJ2I_unwrapper,                  (void *)methodHandleJ2I_unwrapper, TR_Helper);

#else // AMD64

   // -------------------------------- IA32 ------------------------------------
   SET(TR_IA32longDivide,                             (void *)longDivide,               TR_Helper);
   SET(TR_IA32longRemainder,                          (void *)longRemainder,            TR_Helper);

   SET(TR_IA32jitCollapseJNIReferenceFrame,           (void *)jitCollapseJNIReferenceFrame,    TR_Helper);

   SET(TR_IA32floatRemainder,                         (void *)X87floatRemainder,  TR_Helper);
   SET(TR_IA32doubleRemainder,                        (void *)X87doubleRemainder, TR_Helper);

   SET(TR_IA32floatRemainderSSE,                      (void *)SSEfloatRemainderIA32Thunk,  TR_Helper);
   SET(TR_IA32doubleRemainderSSE,                     (void *)SSEdoubleRemainderIA32Thunk, TR_Helper);
   SET(TR_IA32double2LongSSE,                         (void *)SSEdouble2LongIA32, TR_Helper);

   SET(TR_IA32doubleToLong,                           (void *)doubleToLong, TR_Helper);
   SET(TR_IA32doubleToInt,                            (void *)doubleToInt,  TR_Helper);
   SET(TR_IA32floatToLong,                            (void *)floatToLong,  TR_Helper);
   SET(TR_IA32floatToInt,                             (void *)floatToInt,   TR_Helper);

   SET(TR_IA32compressString,                         (void *)compressString,            TR_Helper);
   SET(TR_IA32compressStringNoCheck,                  (void *)compressStringNoCheck,     TR_Helper);
   SET(TR_IA32compressStringJ,                        (void *)compressStringJ,           TR_Helper);
   SET(TR_IA32compressStringNoCheckJ,                 (void *)compressStringNoCheckJ,    TR_Helper);
   SET(TR_IA32andORString,                            (void *)andORString,               TR_Helper);
   SET(TR_IA32arrayTranslateTRTO,                     (void *)arrayTranslateTRTO,        TR_Helper);
   SET(TR_IA32arrayTranslateTROTNoBreak,              (void *)arrayTranslateTROTNoBreak, TR_Helper);
   SET(TR_IA32arrayTranslateTROT,                     (void *)arrayTranslateTROT,        TR_Helper);
   SET(TR_IA32encodeUTF16Big,                         (void *)encodeUTF16Big,            TR_Helper);
   SET(TR_IA32encodeUTF16Little,                      (void *)encodeUTF16Little,         TR_Helper);

   SET(TR_jitAddPicToPatchOnClassUnload,              (void *)jitAddPicToPatchOnClassUnload, TR_Helper);

   SET(TR_IA32JitMonitorEnterReserved,                    (void *)jitMonitorEnterReserved,                    TR_CHelper);
   SET(TR_IA32JitMonitorEnterReservedPrimitive,           (void *)jitMonitorEnterReservedPrimitive,           TR_CHelper);
   SET(TR_IA32JitMonitorEnterPreservingReservation,       (void *)jitMonitorEnterPreservingReservation,       TR_CHelper);
   SET(TR_IA32JitMonitorExitReserved,                     (void *)jitMonitorExitReserved,                     TR_CHelper);
   SET(TR_IA32JitMonitorExitReservedPrimitive,            (void *)jitMonitorExitReservedPrimitive,            TR_CHelper);
   SET(TR_IA32JitMonitorExitPreservingReservation,        (void *)jitMonitorExitPreservingReservation,        TR_CHelper);
   SET(TR_IA32JitMethodMonitorEnterReserved,              (void *)jitMethodMonitorEnterReserved,              TR_CHelper);
   SET(TR_IA32JitMethodMonitorEnterReservedPrimitive,     (void *)jitMethodMonitorEnterReservedPrimitive,     TR_CHelper);
   SET(TR_IA32JitMethodMonitorEnterPreservingReservation, (void *)jitMethodMonitorEnterPreservingReservation, TR_CHelper);
   SET(TR_IA32JitMethodMonitorExitPreservingReservation,  (void *)jitMethodMonitorExitPreservingReservation,  TR_CHelper);
   SET(TR_IA32JitMethodMonitorExitReservedPrimitive,      (void *)jitMethodMonitorExitReservedPrimitive,      TR_CHelper);
   SET(TR_IA32JitMethodMonitorExitReserved,               (void *)jitMethodMonitorExitReserved,               TR_CHelper);

#endif
#elif defined(TR_HOST_POWER)
   SET(TR_PPCdouble2Long,           (void *) __double2Long,     TR_Helper);
   SET(TR_PPCdoubleRemainder,       (void *) __doubleRemainder, TR_Helper);
   SET(TR_PPCinteger2Double,        (void *) __integer2Double,  TR_Helper);
   SET(TR_PPClong2Double,           (void *) __long2Double,     TR_Helper);
   SET(TR_PPClong2Float,            (void *) __long2Float,      TR_Helper);
   SET(TR_PPClong2Float_mv,         (void *) __long2Float_mv,   TR_Helper);
   SET(TR_PPClongMultiply,          (void *) 0,                 TR_Helper);
   SET(TR_PPClongDivide,            (void *) __longDivide,      TR_Helper);
   SET(TR_PPClongDivideEP,          (void *) __longDivideEP,    TR_Helper);
   SET(TR_PPCinterpreterUnresolvedStaticGlue,             (void *) _interpreterUnresolvedStaticGlue,              TR_Helper);
   SET(TR_PPCinterpreterUnresolvedSpecialGlue,            (void *) _interpreterUnresolvedSpecialGlue,             TR_Helper);
   SET(TR_PPCinterpreterUnresolvedDirectVirtualGlue,      (void *) _interpreterUnresolvedDirectVirtualGlue,       TR_Helper);
   SET(TR_PPCinterpreterUnresolvedClassGlue,              (void *) _interpreterUnresolvedClassGlue,               TR_Helper);
   SET(TR_PPCinterpreterUnresolvedClassGlue2,             (void *) _interpreterUnresolvedClassGlue2,              TR_Helper);
   SET(TR_PPCinterpreterUnresolvedStringGlue,             (void *) _interpreterUnresolvedStringGlue,              TR_Helper);
   SET(TR_PPCinterpreterUnresolvedConstantDynamicGlue,    (void *) _interpreterUnresolvedConstantDynamicGlue,     TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeGlue,            (void *)_interpreterUnresolvedMethodTypeGlue,           TR_Helper);
   SET(TR_interpreterUnresolvedMethodHandleGlue,          (void *)_interpreterUnresolvedMethodHandleGlue,         TR_Helper);
   SET(TR_interpreterUnresolvedCallSiteTableEntryGlue,    (void *)_interpreterUnresolvedCallSiteTableEntryGlue,   TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeTableEntryGlue,  (void *)_interpreterUnresolvedMethodTypeTableEntryGlue, TR_Helper);
   SET(TR_PPCinterpreterUnresolvedStaticDataGlue,         (void *) _interpreterUnresolvedStaticDataGlue,          TR_Helper);
   SET(TR_PPCinterpreterUnresolvedStaticDataStoreGlue,    (void *) _interpreterUnresolvedStaticDataStoreGlue,     TR_Helper);
   SET(TR_PPCinterpreterUnresolvedInstanceDataGlue,       (void *) _interpreterUnresolvedInstanceDataGlue,        TR_Helper);
   SET(TR_PPCinterpreterUnresolvedInstanceDataStoreGlue,  (void *) _interpreterUnresolvedInstanceDataStoreGlue,   TR_Helper);
   SET(TR_PPCvirtualUnresolvedHelper,                     (void *) _virtualUnresolvedHelper,                      TR_Helper);
   SET(TR_PPCinterfaceCallHelper,                         (void *) _interfaceCallHelper,                          TR_Helper);

   SET(TR_PPCicallVMprJavaSendVirtual0,           (void *) icallVMprJavaSendVirtual0,            TR_Helper);
   SET(TR_PPCicallVMprJavaSendVirtual1,           (void *) icallVMprJavaSendVirtual1,            TR_Helper);
   SET(TR_PPCicallVMprJavaSendVirtualJ,           (void *) icallVMprJavaSendVirtualJ,            TR_Helper);
   SET(TR_PPCicallVMprJavaSendVirtualF,           (void *) icallVMprJavaSendVirtualF,            TR_Helper);
   SET(TR_PPCicallVMprJavaSendVirtualD,           (void *) icallVMprJavaSendVirtualD,            TR_Helper);
   SET(TR_PPCinterpreterVoidStaticGlue,           (void *) _interpreterVoidStaticGlue,           TR_Helper);
   SET(TR_PPCinterpreterSyncVoidStaticGlue,       (void *) _interpreterSyncVoidStaticGlue,       TR_Helper);
   SET(TR_PPCinterpreterGPR3StaticGlue,           (void *) _interpreterGPR3StaticGlue,           TR_Helper);
   SET(TR_PPCinterpreterSyncGPR3StaticGlue,       (void *) _interpreterSyncGPR3StaticGlue,       TR_Helper);
   SET(TR_PPCinterpreterGPR3GPR4StaticGlue,       (void *) _interpreterGPR3GPR4StaticGlue,       TR_Helper);
   SET(TR_PPCinterpreterSyncGPR3GPR4StaticGlue,   (void *) _interpreterSyncGPR3GPR4StaticGlue,   TR_Helper);
   SET(TR_PPCinterpreterFPR0FStaticGlue,          (void *) _interpreterFPR0FStaticGlue,          TR_Helper);
   SET(TR_PPCinterpreterSyncFPR0FStaticGlue,      (void *) _interpreterSyncFPR0FStaticGlue,      TR_Helper);
   SET(TR_PPCinterpreterFPR0DStaticGlue,          (void *) _interpreterFPR0DStaticGlue,          TR_Helper);
   SET(TR_PPCinterpreterSyncFPR0DStaticGlue,      (void *) _interpreterSyncFPR0DStaticGlue,      TR_Helper);
   SET(TR_PPCnativeStaticHelperForUnresolvedGlue, (void *) _nativeStaticHelperForUnresolvedGlue, TR_Helper);
   SET(TR_PPCnativeStaticHelper,                  (void *) _nativeStaticHelper,                  TR_Helper);

   SET(TR_PPCinterfaceCompeteSlot2,        (void *) _interfaceCompeteSlot2,         TR_Helper);
   SET(TR_PPCinterfaceSlotsUnavailable,    (void *) _interfaceSlotsUnavailable,     TR_Helper);
   SET(TR_PPCcollapseJNIReferenceFrame,    (void *) jitCollapseJNIReferenceFrame,   TR_Helper);
   SET(TR_PPCarrayCopy,                    (void *) __arrayCopy,                    TR_Helper);
   SET(TR_PPCwordArrayCopy,                (void *) __wordArrayCopy,                TR_Helper);
   SET(TR_PPChalfWordArrayCopy,            (void *) __halfWordArrayCopy,            TR_Helper);
   SET(TR_PPCforwardArrayCopy,             (void *) __forwardArrayCopy,             TR_Helper);
   SET(TR_PPCforwardWordArrayCopy,         (void *) __forwardWordArrayCopy,         TR_Helper);
   SET(TR_PPCforwardHalfWordArrayCopy,     (void *) __forwardHalfWordArrayCopy,     TR_Helper);
   SET(TR_PPCarrayCopy_dp,                 (void *) __arrayCopy_dp,                 TR_Helper);
   SET(TR_PPCwordArrayCopy_dp,             (void *) __wordArrayCopy_dp,             TR_Helper);
   SET(TR_PPChalfWordArrayCopy_dp,         (void *) __halfWordArrayCopy_dp,         TR_Helper);
   SET(TR_PPCforwardArrayCopy_dp,          (void *) __forwardArrayCopy_dp,          TR_Helper);
   SET(TR_PPCforwardWordArrayCopy_dp,      (void *) __forwardWordArrayCopy_dp,      TR_Helper);
   SET(TR_PPCforwardHalfWordArrayCopy_dp,  (void *) __forwardHalfWordArrayCopy_dp,  TR_Helper);

   SET(TR_PPCquadWordArrayCopy_vsx,        (void *) __quadWordArrayCopy_vsx,        TR_Helper);
   SET(TR_PPCforwardQuadWordArrayCopy_vsx, (void *) __forwardQuadWordArrayCopy_vsx, TR_Helper);

   SET(TR_PPCpostP10ForwardCopy,           (void *) __postP10ForwardCopy,           TR_Helper);
   SET(TR_PPCpostP10GenericCopy,           (void *) __postP10GenericCopy,           TR_Helper);

#ifdef J9VM_OPT_JAVA_CRYPTO_ACCELERATION
#if defined(OMR_GC_COMPRESSED_POINTERS)
   if (TR::Compiler->om.compressObjectReferences())
      {
      SET(TR_PPCP256Multiply,                 (void *) ECP256MUL_PPC_compressed);
      SET(TR_PPCP256Mod,                      (void *) ECP256MOD_PPC_compressed);
      SET(TR_PPCP256addNoMod,                 (void *) ECP256addNoMod_PPC_compressed);
      SET(TR_PPCP256subNoMod,                 (void *) ECP256subNoMod_PPC_compressed);
      }
   else
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
      {
#if defined(OMR_GC_FULL_POINTERS)
      SET(TR_PPCP256Multiply,                 (void *) ECP256MUL_PPC_full);
      SET(TR_PPCP256Mod,                      (void *) ECP256MOD_PPC_full);
      SET(TR_PPCP256addNoMod,                 (void *) ECP256addNoMod_PPC_full);
      SET(TR_PPCP256subNoMod,                 (void *) ECP256subNoMod_PPC_full);
#endif /* defined(OMR_GC_FULL_POINTERS) */
      }
#endif

#ifndef LINUX
   SET(TR_PPCcompressString,               (void *) __compressString,               TR_Helper);
   SET(TR_PPCcompressStringNoCheck,        (void *) __compressStringNoCheck,        TR_Helper);
   SET(TR_PPCcompressStringJ,              (void *) __compressStringJ,              TR_Helper);
   SET(TR_PPCcompressStringNoCheckJ,       (void *) __compressStringNoCheckJ,       TR_Helper);
   SET(TR_PPCandORString,                  (void *) __andORString,                  TR_Helper);
#endif
   SET(TR_PPCreferenceArrayCopy,           (void *) __referenceArrayCopy,           TR_Helper);
   SET(TR_PPCgeneralArrayCopy,             (void *) __generalArrayCopy,             TR_Helper);
#if 1
   SET(TR_PPCarrayCmpVMX,                 (void *) 0, TR_Helper);
   SET(TR_PPCarrayCmpLenVMX,              (void *) 0, TR_Helper);
   SET(TR_PPCarrayCmpScalar,              (void *) 0, TR_Helper);
   SET(TR_PPCarrayCmpLenScalar,           (void *) 0, TR_Helper);
#else
   SET(TR_PPCarrayCmpVMX,                 (void *) __arrayCmpVMX,                 TR_Helper);
   SET(TR_PPCarrayCmpLenVMX,              (void *) __arrayCmpLenVMX,              TR_Helper);
   SET(TR_PPCarrayCmpScalar,              (void *) __arrayCmpScalar,              TR_Helper);
   SET(TR_PPCarrayCmpLenScalar,           (void *) __arrayCmpLenScalar,           TR_Helper);
#endif
   SET(TR_PPCarrayTranslateTRTO,       (void *) __arrayTranslateTRTO,    TR_Helper);
   SET(TR_PPCarrayTranslateTRTO255,    (void *) __arrayTranslateTRTO255, TR_Helper);
   SET(TR_PPCarrayTranslateTROT255,    (void *) __arrayTranslateTROT255, TR_Helper);
   SET(TR_PPCarrayTranslateTROT,       (void *) __arrayTranslateTROT,    TR_Helper);
   SET(TR_PPCencodeUTF16Big,           (void *) __encodeUTF16Big,        TR_Helper);
   SET(TR_PPCencodeUTF16Little,        (void *) __encodeUTF16Little,     TR_Helper);

#elif defined(TR_HOST_ARM)
   SET(TR_ARMdouble2Long,                                (void *) __double2Long,                                  TR_Helper);
   SET(TR_ARMdoubleRemainder,                            (void *) __doubleRemainder,                              TR_Helper);
   SET(TR_ARMdouble2Integer,                             (void *) __double2Integer,                               TR_Helper);
   SET(TR_ARMfloat2Long,                                 (void *) __float2Long,                                   TR_Helper);
   SET(TR_ARMfloatRemainder,                             (void *) __floatRemainder,                               TR_Helper);
   SET(TR_ARMinteger2Double,                             (void *) __integer2Double,                               TR_Helper);
   SET(TR_ARMlong2Double,                                (void *) __long2Double,                                  TR_Helper);
   SET(TR_ARMlong2Float,                                 (void *) __long2Float,                                   TR_Helper);
   SET(TR_ARMlongMultiply,                               (void *) __multi64,                                      TR_Helper);
   SET(TR_ARMintDivide,                                  (void *) __intDivide,                                    TR_Helper);
   SET(TR_ARMintRemainder,                               (void *) __intRemainder,                                 TR_Helper);
   SET(TR_ARMlongDivide,                                 (void *) __longDivide,                                   TR_Helper);
   SET(TR_ARMlongRemainder,                              (void *) __longRemainder,                                TR_Helper);
   SET(TR_ARMlongShiftRightArithmetic,                   (void *) __longShiftRightArithmetic,                     TR_Helper);
   SET(TR_ARMlongShiftRightLogical,                      (void *) __longShiftRightLogical,                        TR_Helper);
   SET(TR_ARMlongShiftLeftLogical,                       (void *) __longShiftLeftLogical,                         TR_Helper);
   SET(TR_ARMinterpreterUnresolvedStaticGlue,            (void *) _interpreterUnresolvedStaticGlue,               TR_Helper);
   SET(TR_ARMinterpreterUnresolvedSpecialGlue,           (void *) _interpreterUnresolvedSpecialGlue,              TR_Helper);
   SET(TR_ARMinterpreterUnresolvedDirectVirtualGlue,     (void *) _interpreterUnresolvedDirectVirtualGlue,        TR_Helper);
   SET(TR_ARMinterpreterUnresolvedClassGlue,             (void *) _interpreterUnresolvedClassGlue,                TR_Helper);
   SET(TR_ARMinterpreterUnresolvedClassGlue2,            (void *) _interpreterUnresolvedClassGlue2,               TR_Helper);
   SET(TR_ARMinterpreterUnresolvedStringGlue,            (void *) _interpreterUnresolvedStringGlue,               TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeGlue,           (void *) _interpreterUnresolvedMethodTypeGlue,           TR_Helper);
   SET(TR_interpreterUnresolvedMethodHandleGlue,         (void *) _interpreterUnresolvedMethodHandleGlue,         TR_Helper);
   SET(TR_interpreterUnresolvedCallSiteTableEntryGlue,   (void *) _interpreterUnresolvedCallSiteTableEntryGlue,   TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeTableEntryGlue, (void *) _interpreterUnresolvedMethodTypeTableEntryGlue, TR_Helper);
   SET(TR_ARMinterpreterUnresolvedStaticDataGlue,        (void *) _interpreterUnresolvedStaticDataGlue,           TR_Helper);
   SET(TR_ARMinterpreterUnresolvedInstanceDataGlue,      (void *) _interpreterUnresolvedInstanceDataGlue,         TR_Helper);
   SET(TR_ARMinterpreterUnresolvedStaticDataStoreGlue,   (void *) _interpreterUnresolvedStaticDataStoreGlue,      TR_Helper);
   SET(TR_ARMinterpreterUnresolvedInstanceDataStoreGlue, (void *) _interpreterUnresolvedInstanceDataStoreGlue,    TR_Helper);
   SET(TR_ARMvirtualUnresolvedHelper,                    (void *) _virtualUnresolvedHelper,                       TR_Helper);
   SET(TR_ARMinterfaceCallHelper,                        (void *) _interfaceCallHelper,                           TR_Helper);
   SET(TR_ARMarrayCopy,                                  (void *) __arrayCopy,                                    TR_Helper);

   SET(TR_ARMicallVMprJavaSendVirtual0,         (void *) icallVMprJavaSendVirtual0, TR_Helper);
   SET(TR_ARMicallVMprJavaSendVirtual1,         (void *) icallVMprJavaSendVirtual1, TR_Helper);
   SET(TR_ARMicallVMprJavaSendVirtualJ,         (void *) icallVMprJavaSendVirtualJ, TR_Helper);
   SET(TR_ARMicallVMprJavaSendVirtualF,         (void *) icallVMprJavaSendVirtualF, TR_Helper);
   SET(TR_ARMicallVMprJavaSendVirtualD,         (void *) icallVMprJavaSendVirtualD, TR_Helper);

   SET(TR_ARMinterpreterVoidStaticGlue,         (void *) _interpreterVoidStaticGlue,         TR_Helper);
   SET(TR_ARMinterpreterSyncVoidStaticGlue,     (void *) _interpreterSyncVoidStaticGlue,     TR_Helper);
   SET(TR_ARMinterpreterGPR3StaticGlue,         (void *) _interpreterGPR3StaticGlue,         TR_Helper);
   SET(TR_ARMinterpreterSyncGPR3StaticGlue,     (void *) _interpreterSyncGPR3StaticGlue,     TR_Helper);
   SET(TR_ARMinterpreterGPR3GPR4StaticGlue,     (void *) _interpreterGPR3GPR4StaticGlue,     TR_Helper);
   SET(TR_ARMinterpreterSyncGPR3GPR4StaticGlue, (void *) _interpreterSyncGPR3GPR4StaticGlue, TR_Helper);
   SET(TR_ARMinterpreterFPR0FStaticGlue,        (void *) _interpreterFPR0FStaticGlue,        TR_Helper);
   SET(TR_ARMinterpreterSyncFPR0FStaticGlue,    (void *) _interpreterSyncFPR0FStaticGlue,    TR_Helper);
   SET(TR_ARMinterpreterFPR0DStaticGlue,        (void *) _interpreterFPR0DStaticGlue,        TR_Helper);
   SET(TR_ARMinterpreterSyncFPR0DStaticGlue,    (void *) _interpreterSyncFPR0DStaticGlue,    TR_Helper);
   SET(TR_ARMnativeStaticHelper,                (void *) _nativeStaticHelper,                TR_Helper);

   SET(TR_ARMicallVMprJavaSendNativeStatic,     (void *)icallVMprJavaSendNativeStatic, TR_Helper);
   SET(TR_ARMicallVMprJavaSendStatic0,          (void *)icallVMprJavaSendStatic0,      TR_Helper);
   SET(TR_ARMicallVMprJavaSendStatic1,          (void *)icallVMprJavaSendStatic1,      TR_Helper);
   SET(TR_ARMicallVMprJavaSendStaticJ,          (void *)icallVMprJavaSendStaticJ,      TR_Helper);
   SET(TR_ARMicallVMprJavaSendStaticF,          (void *)icallVMprJavaSendStaticF,      TR_Helper);
   SET(TR_ARMicallVMprJavaSendStaticD,          (void *)icallVMprJavaSendStaticD,      TR_Helper);
   SET(TR_ARMicallVMprJavaSendStaticSync0,      (void *)icallVMprJavaSendStaticSync0,  TR_Helper);
   SET(TR_ARMicallVMprJavaSendStaticSync1,      (void *)icallVMprJavaSendStaticSync1,  TR_Helper);
   SET(TR_ARMicallVMprJavaSendStaticSyncJ,      (void *)icallVMprJavaSendStaticSyncJ,  TR_Helper);
   SET(TR_ARMicallVMprJavaSendStaticSyncF,      (void *)icallVMprJavaSendStaticSyncF,  TR_Helper);
   SET(TR_ARMicallVMprJavaSendStaticSyncD,      (void *)icallVMprJavaSendStaticSyncD,  TR_Helper);

   SET(TR_ARMinterfaceCompeteSlot2,             (void *) _interfaceCompeteSlot2,       TR_Helper);
   SET(TR_ARMinterfaceSlotsUnavailable,         (void *) _interfaceSlotsUnavailable,   TR_Helper);

#ifdef ZAURUS
   SET(TR_ARMjitMathHelperFloatAddFloat,           (void *) jitMathHelperFloatAddFloat,         TR_Helper);
   SET(TR_ARMjitMathHelperFloatSubtractFloat,      (void *) jitMathHelperFloatSubtractFloat,    TR_Helper);
   SET(TR_ARMjitMathHelperFloatMultiplyFloat,      (void *) jitMathHelperFloatMultiplyFloat,    TR_Helper);
   SET(TR_ARMjitMathHelperFloatDivideFloat,        (void *) jitMathHelperFloatDivideFloat,      TR_Helper);
   SET(TR_ARMjitMathHelperFloatRemainderFloat,     (void *) jitMathHelperFloatRemainderFloat,   TR_Helper);
   SET(TR_ARMjitMathHelperDoubleAddDouble,         (void *) jitMathHelperDoubleAddDouble,       TR_Helper);
   SET(TR_ARMjitMathHelperDoubleSubtractDouble,    (void *) jitMathHelperDoubleSubtractDouble,  TR_Helper);
   SET(TR_ARMjitMathHelperDoubleMultiplyDouble,    (void *) jitMathHelperDoubleMultiplyDouble,  TR_Helper);
   SET(TR_ARMjitMathHelperDoubleDivideDouble,      (void *) jitMathHelperDoubleDivideDouble,    TR_Helper);
   SET(TR_ARMjitMathHelperDoubleRemainderDouble,   (void *) jitMathHelperDoubleRemainderDouble, TR_Helper);
   SET(TR_ARMjitMathHelperFloatNegate,             (void *) jitMathHelperFloatNegate,           TR_Helper);
   SET(TR_ARMjitMathHelperDoubleNegate,            (void *) jitMathHelperDoubleNegate,          TR_Helper);
   SET(TR_ARMjitMathHelperConvertFloatToInt,       (void *) helperCConvertFloatToInteger,       TR_Helper);
   SET(TR_ARMjitMathHelperConvertFloatToLong,      (void *) helperCConvertFloatToLong,          TR_Helper);
   SET(TR_ARMjitMathHelperConvertFloatToDouble,    (void *) jitMathHelperConvertFloatToDouble,  TR_Helper);
   SET(TR_ARMjitMathHelperConvertDoubleToInt,      (void *) helperCConvertDoubleToInteger,      TR_Helper);
   SET(TR_ARMjitMathHelperConvertDoubleToLong,     (void *) helperCConvertDoubleToLong,         TR_Helper);
   SET(TR_ARMjitMathHelperConvertDoubleToFloat,    (void *) jitMathHelperConvertDoubleToFloat,  TR_Helper);
   SET(TR_ARMjitMathHelperConvertIntToFloat,       (void *) jitMathHelperConvertIntToFloat,     TR_Helper);
   SET(TR_ARMjitMathHelperConvertIntToDouble,      (void *) jitMathHelperConvertIntToDouble,    TR_Helper);
   SET(TR_ARMjitMathHelperConvertLongToFloat,      (void *) jitMathHelperConvertLongToFloat,    TR_Helper);
   SET(TR_ARMjitMathHelperConvertLongToDouble,     (void *) jitMathHelperConvertLongToDouble,   TR_Helper);
#else
   SET(TR_ARMjitMathHelperFloatAddFloat,           (void *) helperCFloatPlusFloat,         TR_Helper);
   SET(TR_ARMjitMathHelperFloatSubtractFloat,      (void *) helperCFloatMinusFloat,        TR_Helper);
   SET(TR_ARMjitMathHelperFloatMultiplyFloat,      (void *) helperCFloatMultiplyFloat,     TR_Helper);
   SET(TR_ARMjitMathHelperFloatDivideFloat,        (void *) helperCFloatDivideFloat,       TR_Helper);
   SET(TR_ARMjitMathHelperFloatRemainderFloat,     (void *) helperCFloatRemainderFloat,    TR_Helper);
   SET(TR_ARMjitMathHelperDoubleAddDouble,         (void *) helperCDoublePlusDouble,       TR_Helper);
   SET(TR_ARMjitMathHelperDoubleSubtractDouble,    (void *) helperCDoubleMinusDouble,      TR_Helper);
   SET(TR_ARMjitMathHelperDoubleMultiplyDouble,    (void *) helperCDoubleMultiplyDouble,   TR_Helper);
   SET(TR_ARMjitMathHelperDoubleDivideDouble,      (void *) helperCDoubleDivideDouble,     TR_Helper);
   SET(TR_ARMjitMathHelperDoubleRemainderDouble,   (void *) helperCDoubleRemainderDouble,  TR_Helper);
   SET(TR_ARMjitMathHelperFloatNegate,             (void *) jitMathHelperFloatNegate,      TR_Helper);
   SET(TR_ARMjitMathHelperDoubleNegate,            (void *) jitMathHelperDoubleNegate,     TR_Helper);
   SET(TR_ARMjitMathHelperConvertFloatToInt,       (void *) helperCConvertFloatToInteger,  TR_Helper);
   SET(TR_ARMjitMathHelperConvertFloatToLong,      (void *) helperCConvertFloatToLong,     TR_Helper);
   SET(TR_ARMjitMathHelperConvertFloatToDouble,    (void *) helperCConvertFloatToDouble,   TR_Helper);
   SET(TR_ARMjitMathHelperConvertDoubleToInt,      (void *) helperCConvertDoubleToInteger, TR_Helper);
   SET(TR_ARMjitMathHelperConvertDoubleToLong,     (void *) helperCConvertDoubleToLong,    TR_Helper);
   SET(TR_ARMjitMathHelperConvertDoubleToFloat,    (void *) helperCConvertDoubleToFloat,   TR_Helper);
   SET(TR_ARMjitMathHelperConvertIntToFloat,       (void *) helperCConvertIntegerToFloat,  TR_Helper);
   SET(TR_ARMjitMathHelperConvertIntToDouble,      (void *) helperCConvertIntegerToDouble, TR_Helper);
   SET(TR_ARMjitMathHelperConvertLongToFloat,      (void *) helperCConvertLongToFloat,     TR_Helper);
   SET(TR_ARMjitMathHelperConvertLongToDouble,     (void *) helperCConvertLongToDouble,    TR_Helper);
#endif
   SET(TR_ARMjitMathHelperFloatCompareEQ,       (void *) jitMathHelperFloatCompareEQ,   TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareEQU,      (void *) jitMathHelperFloatCompareEQU,  TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareNE,       (void *) jitMathHelperFloatCompareNE,   TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareNEU,      (void *) jitMathHelperFloatCompareNEU,  TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareLT,       (void *) jitMathHelperFloatCompareLT,   TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareLTU,      (void *) jitMathHelperFloatCompareLTU,  TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareLE,       (void *) jitMathHelperFloatCompareLE,   TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareLEU,      (void *) jitMathHelperFloatCompareLEU,  TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareGT,       (void *) jitMathHelperFloatCompareGT,   TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareGTU,      (void *) jitMathHelperFloatCompareGTU,  TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareGE,       (void *) jitMathHelperFloatCompareGE,   TR_Helper);
   SET(TR_ARMjitMathHelperFloatCompareGEU,      (void *) jitMathHelperFloatCompareGEU,  TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareEQ,      (void *) jitMathHelperDoubleCompareEQ,  TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareEQU,     (void *) jitMathHelperDoubleCompareEQU, TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareNE,      (void *) jitMathHelperDoubleCompareNE,  TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareNEU,     (void *) jitMathHelperDoubleCompareNEU, TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareLT,      (void *) jitMathHelperDoubleCompareLT,  TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareLTU,     (void *) jitMathHelperDoubleCompareLTU, TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareLE,      (void *) jitMathHelperDoubleCompareLE,  TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareLEU,     (void *) jitMathHelperDoubleCompareLEU, TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareGT,      (void *) jitMathHelperDoubleCompareGT,  TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareGTU,     (void *) jitMathHelperDoubleCompareGTU, TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareGE,      (void *) jitMathHelperDoubleCompareGE,  TR_Helper);
   SET(TR_ARMjitMathHelperDoubleCompareGEU,     (void *) jitMathHelperDoubleCompareGEU, TR_Helper);
   SET(TR_ARMjitCollapseJNIReferenceFrame,      (void *) jitCollapseJNIReferenceFrame,  TR_Helper);

#elif defined(TR_HOST_ARM64)
   SET(TR_ARM64interpreterUnresolvedStaticGlue,            (void *) _interpreterUnresolvedStaticGlue,               TR_Helper);
   SET(TR_ARM64interpreterUnresolvedSpecialGlue,           (void *) _interpreterUnresolvedSpecialGlue,              TR_Helper);
   SET(TR_ARM64interpreterUnresolvedDirectVirtualGlue,     (void *) _interpreterUnresolvedDirectVirtualGlue,        TR_Helper);
   SET(TR_ARM64interpreterUnresolvedClassGlue,             (void *) _interpreterUnresolvedClassGlue,                TR_Helper);
   SET(TR_ARM64interpreterUnresolvedClassGlue2,            (void *) _interpreterUnresolvedClassGlue2,               TR_Helper);
   SET(TR_ARM64interpreterUnresolvedStringGlue,            (void *) _interpreterUnresolvedStringGlue,               TR_Helper);
   SET(TR_ARM64interpreterUnresolvedConstantDynamicGlue,   (void *) _interpreterUnresolvedConstantDynamicGlue,      TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeGlue,             (void *) _interpreterUnresolvedMethodTypeGlue,           TR_Helper);
   SET(TR_interpreterUnresolvedMethodHandleGlue,           (void *) _interpreterUnresolvedMethodHandleGlue,         TR_Helper);
   SET(TR_interpreterUnresolvedCallSiteTableEntryGlue,     (void *) _interpreterUnresolvedCallSiteTableEntryGlue,   TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeTableEntryGlue,   (void *) _interpreterUnresolvedMethodTypeTableEntryGlue, TR_Helper);
   SET(TR_ARM64interpreterUnresolvedStaticDataGlue,        (void *) _interpreterUnresolvedStaticDataGlue,           TR_Helper);
   SET(TR_ARM64interpreterUnresolvedStaticDataStoreGlue,   (void *) _interpreterUnresolvedStaticDataStoreGlue,      TR_Helper);
   SET(TR_ARM64interpreterUnresolvedInstanceDataGlue,      (void *) _interpreterUnresolvedInstanceDataGlue,         TR_Helper);
   SET(TR_ARM64interpreterUnresolvedInstanceDataStoreGlue, (void *) _interpreterUnresolvedInstanceDataStoreGlue,    TR_Helper);
   SET(TR_ARM64virtualUnresolvedHelper,                    (void *) _virtualUnresolvedHelper,                       TR_Helper);
   SET(TR_ARM64interfaceCallHelper,                        (void *) _interfaceCallHelper,                           TR_Helper);
   SET(TR_ARM64icallVMprJavaSendVirtual0,         (void *) icallVMprJavaSendVirtual0,        TR_Helper);
   SET(TR_ARM64icallVMprJavaSendVirtual1,         (void *) icallVMprJavaSendVirtual1,        TR_Helper);
   SET(TR_ARM64icallVMprJavaSendVirtualJ,         (void *) icallVMprJavaSendVirtualJ,        TR_Helper);
   SET(TR_ARM64icallVMprJavaSendVirtualF,         (void *) icallVMprJavaSendVirtualF,        TR_Helper);
   SET(TR_ARM64icallVMprJavaSendVirtualD,         (void *) icallVMprJavaSendVirtualD,        TR_Helper);
   SET(TR_ARM64interpreterVoidStaticGlue,         (void *) _interpreterVoidStaticGlue,       TR_Helper);
   SET(TR_ARM64interpreterSyncVoidStaticGlue,     (void *) _interpreterSyncVoidStaticGlue,   TR_Helper);
   SET(TR_ARM64interpreterIntStaticGlue,          (void *) _interpreterIntStaticGlue,        TR_Helper);
   SET(TR_ARM64interpreterSyncIntStaticGlue,      (void *) _interpreterSyncIntStaticGlue,    TR_Helper);
   SET(TR_ARM64interpreterLongStaticGlue,         (void *) _interpreterLongStaticGlue,       TR_Helper);
   SET(TR_ARM64interpreterSyncLongStaticGlue,     (void *) _interpreterSyncLongStaticGlue,   TR_Helper);
   SET(TR_ARM64interpreterFloatStaticGlue,        (void *) _interpreterFloatStaticGlue,      TR_Helper);
   SET(TR_ARM64interpreterSyncFloatStaticGlue,    (void *) _interpreterSyncFloatStaticGlue,  TR_Helper);
   SET(TR_ARM64interpreterDoubleStaticGlue,       (void *) _interpreterDoubleStaticGlue,     TR_Helper);
   SET(TR_ARM64interpreterSyncDoubleStaticGlue,   (void *) _interpreterSyncDoubleStaticGlue, TR_Helper);
   SET(TR_ARM64nativeStaticHelper,                (void *) _nativeStaticHelper,              TR_Helper);
   SET(TR_ARM64interfaceDispatch,                 (void *) _interfaceDispatch,               TR_Helper);
   SET(TR_ARM64floatRemainder,                    (void *) helperCFloatRemainderFloat,       TR_Helper);
   SET(TR_ARM64doubleRemainder,                   (void *) helperCDoubleRemainderDouble,     TR_Helper);
   SET(TR_ARM64jitCollapseJNIReferenceFrame,      (void *) jitCollapseJNIReferenceFrame,     TR_Helper);

#elif defined(TR_HOST_S390)
   SET(TR_S390double2Long,                                (void *) 0,                                              TR_Helper);
   SET(TR_S390double2Integer,                             (void *) 0,                                              TR_Helper);
   SET(TR_S390integer2Double,                             (void *) 0,                                              TR_Helper);
   SET(TR_S390intDivide,                                  (void *) 0,                                              TR_Helper);
   SET(TR_S390long2Double,                                (void *) 0,                                              TR_Helper);
   SET(TR_S390longMultiply,                               (void *) 0,                                              TR_Helper);
   SET(TR_S390longDivide,                                 (void *) _longDivide,                                    TR_Helper);
   SET(TR_S390longRemainder,                              (void *) _longRemainder,                                 TR_Helper);
   SET(TR_S390doubleRemainder,                            (void *) __doubleRemainder,                              TR_Helper);
   SET(TR_S390floatRemainder,                             (void *) __floatRemainder,                               TR_Helper);
   SET_CONST(TR_S390jitMathHelperDREM,                    (void *) helperCDoubleRemainderDouble);
   SET_CONST(TR_S390jitMathHelperFREM,                    (void *) helperCFloatRemainderFloat);
   SET_CONST(TR_S390jitMathHelperConvertLongToFloat,      (void *) helperCConvertLongToFloat);
   SET(TR_S390longShiftLeft,                              (void *) 0,                                              TR_Helper);
   SET(TR_S390longShiftRight,                             (void *) 0,                                              TR_Helper);
   SET(TR_S390longUShiftRight,                            (void *) 0,                                              TR_Helper);
   SET(TR_S390interpreterUnresolvedStaticGlue,            (void *) _interpreterUnresolvedStaticGlue,               TR_Helper);
   SET(TR_S390interpreterUnresolvedSpecialGlue,           (void *) _interpreterUnresolvedSpecialGlue,              TR_Helper);
   SET(TR_S390interpreterUnresolvedDirectVirtualGlue,     (void *) _interpreterUnresolvedDirectVirtualGlue,        TR_Helper);
   SET(TR_S390interpreterUnresolvedClassGlue,             (void *) _interpreterUnresolvedClassGlue,                TR_Helper);
   SET(TR_S390interpreterUnresolvedClassGlue2,            (void *) _interpreterUnresolvedClassGlue2,               TR_Helper);
   SET(TR_S390interpreterUnresolvedStringGlue,            (void *) _interpreterUnresolvedStringGlue,               TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeGlue,            (void *) _interpreterUnresolvedMethodTypeGlue,           TR_Helper);
   SET(TR_interpreterUnresolvedMethodHandleGlue,          (void *) _interpreterUnresolvedMethodHandleGlue,         TR_Helper);
   SET(TR_interpreterUnresolvedCallSiteTableEntryGlue,    (void *) _interpreterUnresolvedCallSiteTableEntryGlue,   TR_Helper);
   SET(TR_interpreterUnresolvedMethodTypeTableEntryGlue,  (void *) _interpreterUnresolvedMethodTypeTableEntryGlue, TR_Helper);
   SET(TR_S390interpreterUnresolvedStaticDataGlue,        (void *) _interpreterUnresolvedStaticDataGlue,           TR_Helper);
   SET(TR_S390interpreterUnresolvedStaticDataStoreGlue,   (void *) _interpreterUnresolvedStaticDataStoreGlue,      TR_Helper);
   SET(TR_S390interpreterUnresolvedInstanceDataGlue,      (void *) _interpreterUnresolvedInstanceDataGlue,         TR_Helper);
   SET(TR_S390interpreterUnresolvedInstanceDataStoreGlue, (void *) _interpreterUnresolvedInstanceDataStoreGlue,    TR_Helper);
   SET(TR_S390virtualUnresolvedHelper,                    (void *) _virtualUnresolvedHelper,                       TR_Helper);
   SET(TR_S390interfaceCallHelper,                        (void *) _interfaceCallHelper,                           TR_Helper);
   SET(TR_S390interfaceCallHelperSingleDynamicSlot,       (void *) _interfaceCallHelperSingleDynamicSlot,          TR_Helper);
   SET(TR_S390interfaceCallHelperMultiSlots,              (void *) _interfaceCallHelperMultiSlots,                 TR_Helper);
   SET(TR_S390jitResolveConstantDynamicGlue,              (void *) _jitResolveConstantDynamic,                     TR_Helper);
   SET(TR_S390icallVMprJavaSendVirtual0,                  (void *) icallVMprJavaSendVirtual0,                      TR_Helper);
   SET(TR_S390icallVMprJavaSendVirtual1,                  (void *) icallVMprJavaSendVirtual1,                      TR_Helper);
   SET(TR_S390icallVMprJavaSendVirtualJ,                  (void *) icallVMprJavaSendVirtualJ,                      TR_Helper);
   SET(TR_S390icallVMprJavaSendVirtualF,                  (void *) icallVMprJavaSendVirtualF,                      TR_Helper);
   SET(TR_S390icallVMprJavaSendVirtualD,                  (void *) icallVMprJavaSendVirtualD,                      TR_Helper);
   SET(TR_S390jitLookupInterfaceMethod,                   (void *) jitLookupInterfaceMethod,                       TR_Helper);
   SET(TR_S390jitMethodIsNative,                          (void *) jitMethodIsNative,                              TR_Helper);
   SET(TR_S390jitMethodIsSync,                            (void *) jitMethodIsSync,                                TR_Helper);
   SET(TR_S390jitResolveClass,                            (void *) jitResolveClass,                                TR_Helper);
   SET(TR_S390jitResolveClassFromStaticField,             (void *) jitResolveClassFromStaticField,                 TR_Helper);
   SET(TR_S390jitResolveConstantDynamic,                  (void *) jitResolveConstantDynamic,                      TR_Helper);
   SET(TR_S390jitResolveField,                            (void *) jitResolveField,                                TR_Helper);
   SET(TR_S390jitResolveFieldSetter,                      (void *) jitResolveFieldSetter,                          TR_Helper);
   SET(TR_S390jitResolveInterfaceMethod,                  (void *) jitResolveInterfaceMethod,                      TR_Helper);
   SET(TR_S390jitResolveStaticField,                      (void *) jitResolveStaticField,                          TR_Helper);
   SET(TR_S390jitResolveStaticFieldSetter,                (void *) jitResolveStaticFieldSetter,                    TR_Helper);
   SET(TR_S390jitResolveString,                           (void *) jitResolveString,                               TR_Helper);
   SET(TR_S390jitResolveMethodType,                       (void *) jitResolveMethodType,                           TR_Helper);
   SET(TR_S390jitResolveMethodHandle,                     (void *) jitResolveMethodHandle,                         TR_Helper);
   SET(TR_S390jitResolveInvokeDynamic,                    (void *) jitResolveInvokeDynamic,                        TR_Helper);
   SET(TR_S390jitResolveHandleMethod,                     (void *) jitResolveHandleMethod,                         TR_Helper);
   SET(TR_S390jitResolveVirtualMethod,                    (void *) jitResolveVirtualMethod,                        TR_Helper);
   SET(TR_S390jitResolveSpecialMethod,                    (void *) jitResolveSpecialMethod,                        TR_Helper);
   SET(TR_S390jitResolveStaticMethod,                     (void *) jitResolveStaticMethod,                         TR_Helper);

   SET(TR_S390interpreterVoidStaticGlue,              (void *) _interpreterVoidStaticGlue,       TR_Helper);
   SET(TR_S390interpreterSyncVoidStaticGlue,          (void *) _interpreterSyncVoidStaticGlue,   TR_Helper);
   SET(TR_S390interpreterIntStaticGlue,               (void *) _interpreterIntStaticGlue,        TR_Helper);
   SET(TR_S390interpreterSyncIntStaticGlue,           (void *) _interpreterSyncIntStaticGlue,    TR_Helper);
   SET(TR_S390interpreterLongStaticGlue,              (void *) _interpreterLongStaticGlue,       TR_Helper);
   SET(TR_S390interpreterSyncLongStaticGlue,          (void *) _interpreterSyncLongStaticGlue,   TR_Helper);
   SET(TR_S390interpreterFloatStaticGlue,             (void *) _interpreterFloatStaticGlue,      TR_Helper);
   SET(TR_S390interpreterSyncFloatStaticGlue,         (void *) _interpreterSyncFloatStaticGlue,  TR_Helper);
   SET(TR_S390interpreterDoubleStaticGlue,            (void *) _interpreterDoubleStaticGlue,     TR_Helper);
   SET(TR_S390interpreterSyncDoubleStaticGlue,        (void *) _interpreterSyncDoubleStaticGlue, TR_Helper);
   SET(TR_S390nativeStaticHelper,                     (void *) _nativeStaticHelper,              TR_Helper);
   SET(TR_S390arrayCopyHelper,                        (void *) 0,                                TR_Helper);
   SET(TR_S390referenceArrayCopyHelper,               (void *) __referenceArrayCopyHelper,       TR_Helper);
   SET(TR_S390arraySetZeroHelper,                     (void *) 0,                                TR_Helper);
   SET(TR_S390arraySetGeneralHelper,                  (void *) 0,                                TR_Helper);
   SET(TR_S390arrayCmpHelper,                         (void *) 0,                                TR_Helper);
   SET(TR_S390arrayTranslateAndTestHelper,            (void *) 0,                                TR_Helper);
   SET(TR_S390long2StringHelper,                      (void *) 0,                                TR_Helper);
   SET(TR_S390arrayXORHelper,                         (void *) 0,                                TR_Helper);
   SET(TR_S390arrayORHelper,                          (void *) 0,                                TR_Helper);
   SET(TR_S390arrayANDHelper,                         (void *) 0,                                TR_Helper);
   SET(TR_S390collapseJNIReferenceFrame,              (void *) jitCollapseJNIReferenceFrame,     TR_Helper);
   SET(TR_S390jitPreJNICallOffloadCheck,              (void *) jitPreJNICallOffloadCheck,        TR_Helper);
   SET(TR_S390jitPostJNICallOffloadCheck,             (void *) jitPostJNICallOffloadCheck,       TR_Helper);
   SET(TR_S390jitCallCFunction,                       (void *) jitCallCFunction,                 TR_Helper);
   /* required so can be called from Picbuilder/Recompilation asm routines for MCC */
   SET(TR_S390mcc_reservationAdjustment_unwrapper,    (void *) mcc_reservationAdjustment_unwrapper,  TR_Helper);
   SET(TR_S390mcc_callPointPatching_unwrapper,        (void *) mcc_callPointPatching_unwrapper,      TR_Helper);
   SET(TR_S390mcc_lookupHelperTrampoline_unwrapper,   (void *) mcc_lookupHelperTrampoline_unwrapper, TR_Helper);
   /* required so can be called from recompilation asm routines */
   SET(TR_S390jitRetranslateMethod,                   (void *) jitRetranslateMethod,      TR_Helper);
   SET(TR_S390revertToInterpreter,                    (void *) revertMethodToInterpreted, TR_Helper);
   SET_CONST(TR_jitAddPicToPatchOnClassUnload,        (void *) jitAddPicToPatchOnClassUnload);

   initS390WriteOnceHelpers(jitConfig, TR_S390arrayCopyHelper, TR_S390arraySetZeroHelper,TR_S390arraySetGeneralHelper,TR_S390arrayCmpHelper);

   SET(TR_methodHandleJ2IGlue,                        (void *)methodHandleJ2IGlue,       TR_Helper);
   SET(TR_methodHandleJ2I_unwrapper,                  (void *)methodHandleJ2I_unwrapper, TR_Helper);

   // You can use this code to print out the helper address and ep
   if (0)
      {
      TR_RuntimeHelper h = TR_jitAddPicToPatchOnClassUnload;
      void * a  = runtimeHelpers.getFunctionEntryPointOrConst(h);
      fprintf(stderr,"jitAddPicToPatchOnClassUnload= _helpers[%d]=0x%p ep=0x%p\n",h,a, TOC_UNWRAP_ADDRESS(a));

      h = TR_S390interfaceCallHelperMultiSlots;
      a  = runtimeHelpers.getFunctionEntryPointOrConst(h);
      fprintf(stderr,"TR_S390interfaceCallHelperMultiSlots= _helpers[%d]=0x%p \n",h,a);

      h = TR_S390interfaceCallHelperSingleDynamicSlot;
      a  = runtimeHelpers.getFunctionEntryPointOrConst(h);
      fprintf(stderr,"TR_S390interfaceCallHelperSingleDynamicSlot= _helpers[%d]=0x%p \n",h,a);
      }
#endif

   #undef SET
   }


// Export these helper indices so that they are visible to the AMD64 PicBuilder.
//
#ifdef TR_HOST_X86
#ifdef TR_HOST_64BIT
extern "C" const TR_RuntimeHelper resolveIPicClassHelperIndex = TR_X86resolveIPicClass;
extern "C" const TR_RuntimeHelper populateIPicSlotClassHelperIndex = TR_X86populateIPicSlotClass;
extern "C" const TR_RuntimeHelper dispatchInterpretedFromIPicSlotHelperIndex = TR_X86dispatchInterpretedFromIPicSlot;
extern "C" const TR_RuntimeHelper resolveVPicClassHelperIndex = TR_X86resolveVPicClass;
extern "C" const TR_RuntimeHelper populateVPicSlotClassHelperIndex = TR_X86populateVPicSlotClass;
extern "C" const TR_RuntimeHelper dispatchInterpretedFromVPicSlotHelperIndex = TR_X86dispatchInterpretedFromVPicSlot;
extern "C" const TR_RuntimeHelper populateVPicVTableDispatchHelperIndex = TR_X86populateVPicVTableDispatch;
#endif
#endif


#if defined(TR_HOST_X86)

void setSaveArea(void *startPC, int32_t startPCToSaveArea, uint16_t newValue)
   {
   *((uint16_t*)((uint8_t*)startPC + startPCToSaveArea)) = newValue;
   }

void saveFirstTwoBytes(void *startPC, int32_t startPCToSaveArea)
   {
   char *startByte = (char*)startPC + jitEntryOffset(startPC);
   setSaveArea(startPC, startPCToSaveArea, *((uint16_t*)startByte));
   }

void replaceFirstTwoBytesWithShortJump(void *startPC, int32_t startPCToTarget)
   {
   char *startByte         = (char*)startPC + jitEntryOffset(startPC);
   *((uint16_t*)startByte) = jitEntryJmpInstruction(startPC, startPCToTarget);
   }

void replaceFirstTwoBytesWithData(void *startPC, int32_t startPCToData)
   {
   char *startByte         = (char*)startPC + jitEntryOffset(startPC);
   *((uint16_t*)startByte) = *((uint16_t*)((uint8_t*)startPC + startPCToData));
   }

//constant parts of patching offsets
#if defined(TR_HOST_64BIT)
#define START_PC_TO_ITR_GLUE_FSD             (-8) //-(SAVE_AREA_SIZE+LINKAGE_INFO_SIZE+2(size of the mini trampoline))
#define START_PC_TO_SAVE_AREA_BYTES_FSD      (-6) //-(SAVE_AREA_SIZE+LINKAGE_INFO_SIZE)
#else
#define START_PC_TO_ITR_GLUE_FSD             (-14)//-(size of switch to interpreter sequence + LINKAGE_INFO_SIZE)
#define START_PC_TO_SAVE_AREA_BYTES_FSD      (-2) //on 32bits, the last 2 bytes of linkageInfo is used as save area
#endif

// The FSD methods below are called in exclusive vm access mode - so
// they dont have to worry about race conditions

extern "C"
   {
   // The patching offset is not fixed due to recompilation
   int32_t adjustPatchingOffset(void *startPC)
      {
      J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
      return (linkageInfo->isSamplingMethodBody()? -SAMPLING_CALL_SIZE: 0) + (linkageInfo->isRecompMethodBody()? -JITTED_BODY_INFO_SIZE: 0);
      }

   int32_t startPCToSaveAreaOffset(void *startPC)
      {
      #if defined(TR_HOST_64BIT)
         return START_PC_TO_SAVE_AREA_BYTES_FSD + adjustPatchingOffset(startPC);
      #else
         return START_PC_TO_SAVE_AREA_BYTES_FSD;
      #endif
      }

   void _fsdSwitchToInterpPatchEntry(void *startPC)
      {
      saveFirstTwoBytes(startPC, startPCToSaveAreaOffset(startPC));
      replaceFirstTwoBytesWithShortJump(startPC, START_PC_TO_ITR_GLUE_FSD + adjustPatchingOffset(startPC));
      }

   void _fsdRestoreToJITPatchEntry(void *startPC)
      {
      // Restore the first two bytes
      //
      replaceFirstTwoBytesWithData(startPC, startPCToSaveAreaOffset(startPC));

      // Clear the save area
      //
      // On IA32, the save area doubles as the distance from the interpreter
      // entry point to the jitted entry point, which should always be zero
      // anyway.  By making sure it's usually zero, this lets the VM treat it
      // the same way on all platforms, which simplifies their code.
      // On AMD64, this is unnecessary but harmless.
      //
      setSaveArea(startPC, startPCToSaveAreaOffset(startPC), 0);
      }
   }
#elif defined(TR_HOST_S390)

// Save 4 bytes at jitEntryPoint at the saveLocn.  saveLocn should already
// have the value saved there, or pattern 0xdeafbeef
void saveJitEntryPoint(uint8_t* intEP, uint8_t* jitEP)
   {
   uint32_t* saveAddress = reinterpret_cast<uint32_t*>(intEP + OFFSET_INTEP_JITEP_SAVE_RESTORE_LOCATION);

   *saveAddress = *(uint32_t*)jitEP;
   }

// Restore original 4 bytes at jitEntryPoint that had been saved away at saveLocn
void restoreJitEntryPoint(uint8_t* intEP, uint8_t* jitEP)
   {
   uint32_t *saveAddress = reinterpret_cast<uint32_t*>(intEP + OFFSET_INTEP_JITEP_SAVE_RESTORE_LOCATION);

   *(uint32_t*)jitEP = *saveAddress;
   }

// The FSD methods below are called in exclusive vm access mode - so
// they dont have to worry about race conditions
extern "C"
   {
   void _fsdSwitchToInterpPatchEntry(void* intEP)
      {
      J9::PrivateLinkage::LinkageInfo* linkageInfo = J9::PrivateLinkage::LinkageInfo::get(intEP);

      uint8_t* jitEP = (uint8_t*)intEP + linkageInfo->getJitEntryOffset();

      saveJitEntryPoint((uint8_t*)intEP, jitEP);

      // Patch the JIT entry point with a BRC to the interpreter call trampoline
      uint32_t brcEncoding = 0xA7F40000 | (0x0000FFFF & ((OFFSET_INTEP_VM_CALL_HELPER_TRAMPOLINE - (int16_t)linkageInfo->getJitEntryOffset()) / 2));

      *(uint32_t*)jitEP = brcEncoding;
      }

   void _fsdRestoreToJITPatchEntry(void* intEP)
      {
      J9::PrivateLinkage::LinkageInfo* linkageInfo = J9::PrivateLinkage::LinkageInfo::get(intEP);

      uint8_t* jitEP = (uint8_t*)intEP + linkageInfo->getReservedWord();

      restoreJitEntryPoint((uint8_t*)intEP, jitEP);
      }
   }

#elif defined(TR_HOST_POWER)

extern void ppcCodeSync(uint8_t *, uint32_t);

void saveFirstInstruction(void *startPC, int32_t startPCToSaveArea)
   {
   // Save the first instruction of the method
   // Calculate entry point
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
   char *startByte = (char *) startPC + getJitEntryOffset(linkageInfo);

   uint32_t* address = (uint32_t*)((uint8_t*)startPC + startPCToSaveArea +
                       (linkageInfo->isSamplingMethodBody()?OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC:-4));
   *address = *((uint32_t*)startByte);
   }

void replaceFirstInstructionWithJump(void *startPC, int32_t startPCToTarget)
   {
   int32_t newInstr;
   int32_t distance;

   // common with Recomp.cpp
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
   char *startByte = (char *) startPC + getJitEntryOffset(linkageInfo);

   distance = startPCToTarget - 2*getJitEntryOffset(linkageInfo);
   if (linkageInfo->isSamplingMethodBody())
      distance += OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC;
   else
      distance -= 4;

   newInstr = 0x48000000|(distance & 0x03FFFFFC);
   *(uint32_t*)startByte = newInstr;
   ppcCodeSync((uint8_t *)startByte, 4);
   }

void restoreFirstInstruction(void *startPC, int32_t startPCToData)
   {
   // Restore the original instructions
   // Calculate entry point
   J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
   char *startByte = (char *) startPC + getJitEntryOffset(linkageInfo);

   uint32_t* address = (uint32_t*)((uint8_t*)startPC + startPCToData +
                       (linkageInfo->isSamplingMethodBody()?OFFSET_SAMPLING_PREPROLOGUE_FROM_STARTPC:-4));
   *((uint32_t*)startByte) = *address;

   ppcCodeSync((uint8_t *)startByte, 4);
   }

// The FSD methods below are called in exclusive vm access mode - so
// they dont have to worry about race conditions
extern "C"
   {
   void _fsdSwitchToInterpPatchEntry(void *startPC)
      {
      J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
      saveFirstInstruction(startPC, OFFSET_REVERT_INTP_PRESERVED_FSD);
      replaceFirstInstructionWithJump(startPC, OFFSET_REVERT_INTP_FIXED_PORTION);
      }

   void _fsdRestoreToJITPatchEntry(void *startPC)
      {
      J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get(startPC);
      restoreFirstInstruction(startPC, OFFSET_REVERT_INTP_PRESERVED_FSD);
      }
   }

#elif defined (J9VM_JIT_FULL_SPEED_DEBUG)  /* platforms has no fsd support*/
extern "C"
   {
   void _fsdSwitchToInterpPatchEntry(void *startPC)
      {
      }

   void _fsdRestoreToJITPatchEntry(void *startPC)
      {
      }
   }
#endif

/* This function allows you to getEnv before a jitconfig has been created. */
char * feGetEnv2(const char * s, const void * vm)
   {
   if (TR::Options::_doNotProcessEnvVars)
      return 0;

   char * envSpace = NULL;
   PORT_ACCESS_FROM_PORT(((J9JavaVM *)vm)->portLibrary);
   int32_t envSize = j9sysinfo_get_env((char *)s, NULL, 0);
   if (envSize != -1)
      {
      envSpace = (char *)j9mem_allocate_memory(envSize, J9MEM_CATEGORY_JIT);

      if (NULL != envSpace)
         {
         envSize = j9sysinfo_get_env((char *)s, envSpace, envSize);
         if (envSize != 0)
            {
            // failed to read the env: either mis-sized buffer or no env set
            j9mem_free_memory(envSpace);
            envSpace = NULL;
            }
          else
            {
            int32_t res = j9sysinfo_get_env("TR_silentEnv", NULL, 0);
            // If TR_silentEnv is not found the result is -1. Setting TR_silentEnv prevents printing envVars
            bool verboseQuery = (res == -1 ? true : false);

            if (verboseQuery)
               {
               j9tty_printf(PORTLIB, "JIT: env var %s is set to %s\n", s, envSpace);
               }
            }
         }
      }
   return envSpace;
   }

char * feGetEnv(const char * s)
   {
   char * envSpace = 0;
   if (jitConfig)
      {
      envSpace = feGetEnv2(s, (void*)(jitConfig->javaVM));
      }
   return envSpace;
   }

OMR::CodeCacheMethodHeader *getCodeCacheMethodHeader(char *p, int searchLimit, J9JITExceptionTable* metaData);
extern "C" OMR::CodeCacheMethodHeader *getCodeCacheMethodHeader_C_callable(char *p, int searchLimit, J9JITExceptionTable* metaData);

extern "C" void jitReportDynamicCodeLoadEvents(J9VMThread *currentThread)
   {
   J9JavaVM *vm = currentThread->javaVM;
   if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
      {
      J9MemorySegment *dataCache = vm->jitConfig->dataCacheList->nextSegment;
      J9JITConfig *jitConfig = vm->jitConfig;
      while (dataCache)
         {
         UDATA current = (UDATA)dataCache->heapBase;
         UDATA end = (UDATA)dataCache->heapAlloc;

         while (current < end)
            {
            J9JITDataCacheHeader * hdr = (J9JITDataCacheHeader *) current;

            if (hdr->type == J9DataTypeExceptionInfo)
               {
               J9JITExceptionTable * metaData = (J9JITExceptionTable *) (current + sizeof(J9JITDataCacheHeader));

               /* Don't report events for unloaded metaData */
               if (metaData->constantPool != NULL)
                  {
                  bool foundEyeCatcher = false;
                  OMR::CodeCacheMethodHeader *ccMethodHeader;
                  ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(vm->hookInterface, currentThread, metaData->ramMethod, (void *) metaData->startPC, metaData->endWarmPC - metaData->startPC, "JIT warm body", metaData );
                  if (metaData->startColdPC != 0)
                     {
                     ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(vm->hookInterface, currentThread, metaData->ramMethod, (void *) metaData->startColdPC, metaData->endPC - metaData->startColdPC, "JIT cold body", metaData);
                     }

                  // Find preprologue region by searching for eye-catcher
                  ccMethodHeader = getCodeCacheMethodHeader((char *)metaData->startPC, 32, metaData);

                  if (ccMethodHeader && (metaData->bodyInfo != NULL))
                     {
                     J9::PrivateLinkage::LinkageInfo *linkageInfo = J9::PrivateLinkage::LinkageInfo::get((void *)metaData->startPC);
                     if (linkageInfo->isRecompMethodBody())
                        {
                        ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(vm->hookInterface, currentThread, metaData->ramMethod, (void *)((char*)ccMethodHeader->_eyeCatcher+4), metaData->startPC - (UDATA)((char*)ccMethodHeader->_eyeCatcher+4), "JIT method header", metaData);
                        }
                     }
                  }
               }

            if (hdr->type == J9_JIT_DCE_THUNK_MAPPING)
               {
               J9ThunkMapping *thunk = (J9ThunkMapping *)(current + sizeof(J9JITDataCacheHeader));

               //report thunk address and size
               ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(vm->hookInterface, currentThread, NULL, (void *) thunk->thunkAddress, *((uint32_t *)thunk->thunkAddress - 2), "JIT virtual thunk", NULL);
               }

            current += hdr->size;
            }
         dataCache = dataCache->nextSegment;
         }

      TR::CodeCacheManager *codeCacheManager = TR::CodeCacheManager::instance();
      codeCacheManager->reportCodeLoadEvents();
      }
   }


/* Find the code cache method header given a PC value.
 * TODO: Common up with DebugExt.cpp code. */
OMR::CodeCacheMethodHeader *getCodeCacheMethodHeader(char *p, int searchLimit, J9JITExceptionTable* metaData)
   {
   char * const warmEyeCatcher = TR::CodeCacheManager::instance()->codeCacheConfig().warmEyeCatcher();
   searchLimit *= 1024; //convert to bytes
   p = p - (((UDATA) p) % 4);   //check if param address is multiple of 4 bytes, if not, shift

   OMR::CodeCacheMethodHeader *localCodeCacheMethodHeader = NULL;
   char *potentialEyeCatcher = NULL;
   int32_t size = sizeof(localCodeCacheMethodHeader->_eyeCatcher);  //size of eyecatcher and therefore the search interval as well

   // MetaData contains a pointer to the code cache method header.  Check that first.
   if (metaData)
      {
      // The codecache allocation doesn't include the method header.
      localCodeCacheMethodHeader = (OMR::CodeCacheMethodHeader *)(metaData->codeCacheAlloc - sizeof(OMR::CodeCacheMethodHeader));
      potentialEyeCatcher = localCodeCacheMethodHeader->_eyeCatcher;

      if (0 == strncmp(potentialEyeCatcher, warmEyeCatcher, size))
         return localCodeCacheMethodHeader;



      TR_ASSERT(false, "Expected to find eyecatcher at metaData->codeCacheAlloc. MetaData: %p ", metaData);
      return NULL;  // Conservatively return NULL.
      }

   int32_t sizeSearched = 0;

   int codeCacheMethodHeaderSize = sizeof(OMR::CodeCacheMethodHeader);
   while (potentialEyeCatcher == NULL || strncmp(potentialEyeCatcher, warmEyeCatcher, size) != 0)
      {
      if (sizeSearched >= searchLimit)
         {
         return NULL;
         }

      localCodeCacheMethodHeader = (OMR::CodeCacheMethodHeader *) p;
      potentialEyeCatcher = localCodeCacheMethodHeader->_eyeCatcher;
      sizeSearched += size;
      p = p - size;
      }
   return localCodeCacheMethodHeader;

   }

bool isOrderedPair(U_8 recordType)
   {
   bool isOrderedPair = false;
   switch ((recordType & TR_ExternalRelocationTargetKindMask))
      {
      case TR_AbsoluteMethodAddressOrderedPair:
      case TR_ConstantPoolOrderedPair:
#if defined(TR_HOST_POWER) || defined(TR_HOST_ARM)
      case TR_ClassAddress:
      case TR_MethodObject:
      //case TR_DataAddress:
#endif
#if defined(TR_HOST_32BIT) && defined(TR_HOST_POWER)
      case TR_ArrayCopyHelper:
      case TR_ArrayCopyToc:
      case TR_GlobalValue:
      case TR_RamMethodSequence:
      case TR_BodyInfoAddressLoad:
      case TR_DataAddress:
      case TR_DebugCounter:
#endif
         isOrderedPair = true;
         break;
      }
   return isOrderedPair;
   }


