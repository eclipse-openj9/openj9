/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#include "codegen/CodeGenerator.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/JITServerHelpers.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "env/ClassTableCriticalSection.hpp"
#include "env/J2IThunk.hpp"
#include "env/j9methodServer.hpp"
#include "env/JITServerPersistentCHTable.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "net/ClientStream.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "runtime/J9VMAccess.hpp"
#include "runtime/JITClientSession.hpp"
#include "runtime/JITServerIProfiler.hpp"
#include "runtime/RelocationTarget.hpp"
#include "jitprotos.h"
#include "vmaccess.h"

extern TR::Monitor *assumptionTableMutex;

// TODO: This method is copied from runtime/jit_vm/ctsupport.c,
// in the future it's probably better to make that method publicly accessible
static UDATA
findField(J9VMThread *vmStruct, J9ConstantPool *constantPool, UDATA index, BOOLEAN isStatic, J9Class **declaringClass)
   {
   J9JavaVM *javaVM = vmStruct->javaVM;
   J9ROMFieldRef *romRef;
   J9ROMClassRef *classRef; /* ROM class of the field */
   U_32 classRefCPIndex;
   J9UTF8 *classNameUTF;
   J9Class *clazz;
   UDATA result = 0;

   *declaringClass = NULL;
   romRef = (J9ROMFieldRef*) &(((J9ROMConstantPoolItem*) constantPool->romConstantPool)[index]);
   classRefCPIndex = romRef->classRefCPIndex;
   classRef = (J9ROMClassRef*) &(((J9ROMConstantPoolItem*) constantPool->romConstantPool)[classRefCPIndex]);
   classNameUTF = J9ROMCLASSREF_NAME(classRef);
   clazz = javaVM->internalVMFunctions->internalFindClassUTF8(vmStruct, J9UTF8_DATA(classNameUTF),
                  J9UTF8_LENGTH(classNameUTF), constantPool->ramClass->classLoader, J9_FINDCLASS_FLAG_EXISTING_ONLY);
   if (NULL != clazz)
      {
      J9ROMNameAndSignature *nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE(romRef);
      J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
      J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);

      if (!isStatic)
         {
         UDATA instanceField;
         IDATA offset = javaVM->internalVMFunctions->instanceFieldOffset(
                         vmStruct, clazz, J9UTF8_DATA(name),
                         J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), declaringClass,
                         &instanceField, J9_LOOK_NO_JAVA);

         if (-1 != offset)
            {
            result = instanceField;
            }
         }
      else
         {
         UDATA staticField;
         void * addr = javaVM->internalVMFunctions->staticFieldAddress(
                         vmStruct, clazz, J9UTF8_DATA(name),
                         J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), declaringClass,
                         &staticField, J9_LOOK_NO_JAVA, NULL);

         if (NULL != addr)
            {
            result = staticField;
            }
         }
      }
   return result;
   }

static void
handler_IProfiler_profilingSample(JITServer::ClientStream *client, TR_J9VM *fe, TR::Compilation *comp)
   {
   auto recv = client->getRecvData<TR_OpaqueMethodBlock*, uint32_t, uintptrj_t>();
   auto method = std::get<0>(recv);
   auto bcIndex = std::get<1>(recv);
   auto data = std::get<2>(recv); // data==1 means 'send info for 1 bytecode'; data==0 means 'send info for entire method if possible'

   JITClientIProfiler *iProfiler = (JITClientIProfiler *)fe->getIProfiler();

   bool isCompiled = TR::CompilationInfo::isCompiled((J9Method*)method);
   bool isInProgress = comp->getMethodBeingCompiled()->getPersistentIdentifier() == method;
   bool abort = false;
   // Used to tell the server if a profiled entry should be stored in persistent or heap memory
   bool usePersistentCache = isCompiled || isInProgress;
   bool wholeMethodInfo = data == 0;

   if (wholeMethodInfo)
      {
      // Serialize all the information related to this method
      abort = iProfiler->serializeAndSendIProfileInfoForMethod(method, comp, client, usePersistentCache);
      }
   if (!wholeMethodInfo || abort) // Send information just for this entry
      {
      auto entry = iProfiler->profilingSample(method, bcIndex, comp, data, false);
      if (entry && !entry->isInvalid())
         {
         uint32_t canPersist = entry->canBeSerialized(comp->getPersistentInfo()); // This may lock the entry
         if (canPersist == IPBC_ENTRY_CAN_PERSIST)
            {
            uint32_t bytes = entry->getBytesFootprint();
            std::string entryBytes(bytes, '\0');
            auto storage = (TR_IPBCDataStorageHeader*)&entryBytes[0];
            uintptrj_t methodStartAddress = (uintptrj_t)TR::Compiler->mtd.bytecodeStart(method);
            entry->serialize(methodStartAddress, storage, comp->getPersistentInfo());
            client->write(JITServer::MessageType::IProfiler_profilingSample, entryBytes, false, usePersistentCache);
            }
         else
            {
            client->write(JITServer::MessageType::IProfiler_profilingSample, std::string(), false, usePersistentCache);
            }
         // Unlock the entry
         if (auto callGraphEntry = entry->asIPBCDataCallGraph())
            if (canPersist != IPBC_ENTRY_PERSIST_LOCK && callGraphEntry->isLocked())
               callGraphEntry->releaseEntry();
         }
      else // No valid info for specified bytecode index
         {
         client->write(JITServer::MessageType::IProfiler_profilingSample, std::string(), false, usePersistentCache);
         }
      }
   }

static bool
handleServerMessage(JITServer::ClientStream *client, TR_J9VM *fe, JITServer::MessageType &response)
   {
   using JITServer::MessageType;
   TR::CompilationInfoPerThread *compInfoPT = fe->_compInfoPT;
   J9VMThread *vmThread = compInfoPT->getCompilationThread();
   TR_Memory  *trMemory = compInfoPT->getCompilation()->trMemory();
   TR::Compilation *comp = compInfoPT->getCompilation();

   TR_ASSERT(TR::MonitorTable::get()->getClassUnloadMonitorHoldCount(compInfoPT->getCompThreadId()) == 0, "Must not hold classUnloadMonitor");
   TR::MonitorTable *table = TR::MonitorTable::get();
   TR_ASSERT(table && table->isThreadInSafeMonitorState(vmThread), "Must not hold any monitors when waiting for server");

   response = client->read();

   // Acquire VM access and check for possible class unloading
   acquireVMAccessNoSuspend(vmThread);

   // Update statistics for server message type
   JITServerHelpers::serverMsgTypeCount[response] += 1;

   // If JVM has unloaded classes inform the server to abort this compilation
   uint8_t interruptReason = compInfoPT->compilationShouldBeInterrupted();
   if (interruptReason)
      {
      // Inform the server if compilation is not yet complete
      if ((response != MessageType::compilationCode) &&
          (response != MessageType::compilationFailure))
         client->writeError(JITServer::MessageType::compilationInterrupted, 0 /* placeholder */);

      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompilationDispatch))
         TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "Interrupting remote compilation (interruptReason %u) in handleServerMessage of %s @ %s",
                                                          interruptReason, comp->signature(), comp->getHotnessName());
      comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted in handleServerMessage");
      }

   bool done = false;
   switch (response)
      {
      case MessageType::compilationCode:
      case MessageType::compilationFailure:
         done = true;
         break;

      case MessageType::getUnloadedClassRanges:
         {
         auto unloadedClasses = comp->getPersistentInfo()->getUnloadedClassAddresses();
         std::vector<TR_AddressRange> ranges;
         ranges.reserve(unloadedClasses->getNumberOfRanges());
            {
            OMR::CriticalSection getAddressSetRanges(assumptionTableMutex);
            unloadedClasses->getRanges(ranges);
            }
         client->write(response, ranges, unloadedClasses->getMaxRanges());
         break;
         }

      case MessageType::VM_isClassLibraryClass:
         {
         bool rv = fe->isClassLibraryClass(std::get<0>(client->getRecvData<TR_OpaqueClassBlock*>()));
         client->write(response, rv);
         }
         break;
      case MessageType::VM_isClassLibraryMethod:
         {
         auto tup = client->getRecvData<TR_OpaqueMethodBlock*, bool>();
         bool rv = fe->isClassLibraryMethod(std::get<0>(tup), std::get<1>(tup));
         client->write(response, rv);
         }
         break;
      case MessageType::VM_getSuperClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getSuperClass(clazz));
         }
         break;
      case MessageType::VM_isInstanceOf:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, TR_OpaqueClassBlock *, bool, bool, bool>();
         TR_OpaqueClassBlock *a = std::get<0>(recv);
         TR_OpaqueClassBlock *b = std::get<1>(recv);
         bool objectTypeIsFixed = std::get<2>(recv);
         bool castTypeIsFixed = std::get<3>(recv);
         bool optimizeForAOT = std::get<4>(recv);
         client->write(response, fe->isInstanceOf(a, b, objectTypeIsFixed, castTypeIsFixed, optimizeForAOT));
         }
         break;
      case MessageType::VM_getSystemClassFromClassName:
         {
         auto recv = client->getRecvData<std::string, bool>();
         const std::string name = std::get<0>(recv);
         bool isVettedForAOT = std::get<1>(recv);
         client->write(response, fe->getSystemClassFromClassName(name.c_str(), name.length(), isVettedForAOT));
         }
         break;
      case MessageType::VM_isMethodTracingEnabled:
         {
         auto method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(response, fe->isMethodTracingEnabled(method));
         }
         break;
      case MessageType::VM_getClassClassPointer:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock*>());
         client->write(response, fe->getClassClassPointer(clazz));
         }
         break;
      case MessageType::VM_getClassOfMethod:
         {
         auto method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock*>());
         client->write(response, fe->getClassOfMethod(method));
         }
         break;
      case MessageType::VM_getBaseComponentClass:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock*, int32_t>();
         auto clazz = std::get<0>(recv);
         int32_t numDims = std::get<1>(recv);
         auto outClass = fe->getBaseComponentClass(clazz, numDims);
         client->write(response, outClass, numDims);
         };
         break;
      case MessageType::VM_getLeafComponentClassFromArrayClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock*>());
         client->write(response, fe->getLeafComponentClassFromArrayClass(clazz));
         }
         break;
      case MessageType::VM_getClassFromSignature:
         {
         auto recv = client->getRecvData<std::string, TR_OpaqueMethodBlock *, bool>();
         std::string sig = std::get<0>(recv);
         auto method = std::get<1>(recv);
         bool isVettedForAOT = std::get<2>(recv);
         auto clazz = fe->getClassFromSignature(sig.c_str(), sig.length(), method, isVettedForAOT);
         client->write(response, clazz);
         }
         break;
      case MessageType::VM_jitFieldsAreSame:
         {
         auto recv = client->getRecvData<TR_ResolvedMethod*, int32_t, TR_ResolvedMethod *, int32_t, int32_t>();
         TR_ResolvedMethod *method1 = std::get<0>(recv);
         int32_t cpIndex1 = std::get<1>(recv);
         TR_ResolvedMethod *method2 = std::get<2>(recv);
         int32_t cpIndex2 = std::get<3>(recv);
         int32_t isStatic = std::get<4>(recv);
         bool identical = false;
         bool compareFields = false;
         UDATA f1 = 0, f2 = 0;
         J9Class *declaringClass1 = NULL, *declaringClass2 = NULL;
         J9ConstantPool *cp1 = (J9ConstantPool *) method1->ramConstantPool();
         J9ConstantPool *cp2 = (J9ConstantPool *) method2->ramConstantPool();

         // The following code is mostly a copy of jitFieldsAreIdentical from runtime/jit_vm/ctsupport.c
         // If that code changes, this will also need to change
         J9RAMFieldRef *ramRef1 = (J9RAMFieldRef*) &(((J9RAMConstantPoolItem *)cp1)[cpIndex1]);

         if (!isStatic)
            {
            if (!J9RAMFIELDREF_IS_RESOLVED(ramRef1))
               {
               compareFields = true;
               }
            else
               {
               J9RAMFieldRef *ramRef2 = (J9RAMFieldRef*) &(((J9RAMConstantPoolItem *)cp2)[cpIndex2]);
               if (!J9RAMFIELDREF_IS_RESOLVED(ramRef2))
                  {
                  compareFields = true;
                  }
               else if (ramRef1->valueOffset == ramRef2->valueOffset)
                  {
                  compareFields = true;
                  }
               }
            }
         else
            {
            J9RAMStaticFieldRef *ramRef1 = ((J9RAMStaticFieldRef*) cp1) + cpIndex1;

            if (!J9RAMSTATICFIELDREF_IS_RESOLVED(ramRef1))
               {
               compareFields = true;
               }
            else
               {
               J9RAMStaticFieldRef *ramRef2 = ((J9RAMStaticFieldRef*) cp2) + cpIndex2;
               if (!J9RAMSTATICFIELDREF_IS_RESOLVED(ramRef2))
                  {
                  compareFields = true;
                  }
               else if (ramRef1->valueOffset == ramRef2->valueOffset)
                  {
                  compareFields = true;
                  }
               }
            }
         if (compareFields)
            {
             f1 = findField(fe->vmThread(), cp1, cpIndex1, isStatic, &declaringClass1);
             if (f1)
                f2 = findField(fe->vmThread(), cp2, cpIndex2, isStatic, &declaringClass2);
            }
         client->write(response, declaringClass1, declaringClass2, f1, f2);
         };
         break;
      case MessageType::VM_jitStaticsAreSame:
         {
         auto recv = client->getRecvData<TR_ResolvedMethod*, int32_t, TR_ResolvedMethod *, int32_t>();
         TR_ResolvedMethod *method1 = std::get<0>(recv);
         int32_t cpIndex1 = std::get<1>(recv);
         TR_ResolvedMethod *method2 = std::get<2>(recv);
         int32_t cpIndex2 = std::get<3>(recv);
         client->write(response, fe->jitStaticsAreSame(method1, cpIndex1, method2, cpIndex2));
         };
         break;
      case MessageType::VM_compiledAsDLTBefore:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_ResolvedMethod *>());
         client->write(response, fe->compiledAsDLTBefore(clazz));
         }
         break;
      case MessageType::VM_getComponentClassFromArrayClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getComponentClassFromArrayClass(clazz));
         }
         break;
      case MessageType::VM_classHasBeenReplaced:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->classHasBeenReplaced(clazz));
         }
         break;
      case MessageType::VM_classHasBeenExtended:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->classHasBeenExtended(clazz));
         }
         break;
      case MessageType::VM_isThunkArchetype:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(response, fe->isThunkArchetype(method));
         }
         break;
      case MessageType::VM_printTruncatedSignature:
         {
         J9Method *method = (J9Method *) std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         J9UTF8 * className;
         J9UTF8 * name;
         J9UTF8 * signature;
         getClassNameSignatureFromMethod(method, className, name, signature);
         std::string classNameStr(utf8Data(className), J9UTF8_LENGTH(className));
         std::string nameStr(utf8Data(name), J9UTF8_LENGTH(name));
         std::string signatureStr(utf8Data(signature), J9UTF8_LENGTH(signature));
         client->write(response, classNameStr, nameStr, signatureStr);
         }
         break;
      case MessageType::VM_getStaticHookAddress:
         {
         int32_t event = std::get<0>(client->getRecvData<int32_t>());
         client->write(response, fe->getStaticHookAddress(event));
         }
         break;
      case MessageType::VM_isClassInitialized:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->isClassInitialized(clazz));
         }
         break;
      case MessageType::VM_getOSRFrameSizeInBytes:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(response, fe->getOSRFrameSizeInBytes(method));
         }
         break;
      case MessageType::VM_getByteOffsetToLockword:
         {
         TR_OpaqueClassBlock * clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getByteOffsetToLockword(clazz));
         }
         break;
      case MessageType::VM_isString1:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->isString(clazz));
         }
         break;
      case MessageType::VM_getMethods:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getMethods(clazz));
         }
         break;
      case MessageType::VM_getResolvedMethodsAndMirror:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         uint32_t numMethods = fe->getNumMethods(clazz);
         J9Method *methods = (J9Method *) fe->getMethods(clazz);

         // Create mirrors and put methodInfo for each method in a vector to be sent to the server
         std::vector<TR_ResolvedJ9JITServerMethodInfo> methodsInfo;
         methodsInfo.reserve(numMethods);
         for (int i = 0; i < numMethods; ++i)
            {
            TR_ResolvedJ9JITServerMethodInfo methodInfo;
            TR_ResolvedJ9JITServerMethod::createResolvedMethodMirror(methodInfo, (TR_OpaqueMethodBlock *) &(methods[i]), 0, 0, fe, trMemory);
            methodsInfo.push_back(methodInfo);
            }
         client->write(response, methods, methodsInfo);
         }
         break;
      case MessageType::VM_getVMInfo:
         {
         ClientSessionData::VMInfo vmInfo = {};
         vmInfo._systemClassLoader = fe->getSystemClassLoader();
         vmInfo._processID = fe->getProcessID();
         vmInfo._canMethodEnterEventBeHooked = fe->canMethodEnterEventBeHooked();
         vmInfo._canMethodExitEventBeHooked = fe->canMethodExitEventBeHooked();
         vmInfo._usesDiscontiguousArraylets = TR::Compiler->om.usesDiscontiguousArraylets();
         vmInfo._isIProfilerEnabled = fe->getIProfiler();
         vmInfo._arrayletLeafLogSize = TR::Compiler->om.arrayletLeafLogSize();
         vmInfo._arrayletLeafSize = TR::Compiler->om.arrayletLeafSize();
         vmInfo._overflowSafeAllocSize = static_cast<uint64_t>(fe->getOverflowSafeAllocSize());
         vmInfo._compressedReferenceShift = TR::Compiler->om.compressedReferenceShift();
         vmInfo._j9SharedClassCacheDescriptorList = NULL;
         vmInfo._stringCompressionEnabled = fe->isStringCompressionEnabledVM();
         vmInfo._hasSharedClassCache = TR::Options::sharedClassCache();
         vmInfo._elgibleForPersistIprofileInfo = vmInfo._isIProfilerEnabled ? fe->getIProfiler()->elgibleForPersistIprofileInfo(comp) : false;
         vmInfo._reportByteCodeInfoAtCatchBlock = comp->getOptions()->getReportByteCodeInfoAtCatchBlock();
         for (int32_t i = 0; i <= 7; i++)
            {
            vmInfo._arrayTypeClasses[i] = fe->getClassFromNewArrayTypeNonNull(i + 4);
            }
         vmInfo._readBarrierType = TR::Compiler->om.readBarrierType();
         vmInfo._writeBarrierType = TR::Compiler->om.writeBarrierType();
         vmInfo._compressObjectReferences = TR::Compiler->om.compressObjectReferences();
         vmInfo._processorFeatureFlags = TR::Compiler->target.cpu.getProcessorFeatureFlags();
         vmInfo._invokeWithArgumentsHelperMethod = J9VMJAVALANGINVOKEMETHODHANDLE_INVOKEWITHARGUMENTSHELPER_METHOD(fe->getJ9JITConfig()->javaVM);
         vmInfo._noTypeInvokeExactThunkHelper = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact0, false, false, false)->getMethodAddress();
         vmInfo._int64InvokeExactThunkHelper = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactJ, false, false, false)->getMethodAddress();
         vmInfo._int32InvokeExactThunkHelper = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact1, false, false, false)->getMethodAddress();
         vmInfo._addressInvokeExactThunkHelper = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactL, false, false, false)->getMethodAddress();
         vmInfo._floatInvokeExactThunkHelper = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactF, false, false, false)->getMethodAddress();
         vmInfo._doubleInvokeExactThunkHelper = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactD, false, false, false)->getMethodAddress();
         vmInfo._interpreterVTableOffset = TR::Compiler->vm.getInterpreterVTableOffset();

         // For multi-layered SCC support
         std::vector<uintptr_t> listOfCacheStartAddress;
         std::vector<uintptr_t> listOfCacheSizeBytes;
         if (fe->sharedCache() && fe->sharedCache()->getCacheDescriptorList())
            {
            // The cache descriptor list is linked last to first and is circular, so last->previous == first.
            J9SharedClassCacheDescriptor *head = fe->sharedCache()->getCacheDescriptorList();
            J9SharedClassCacheDescriptor *curCache = head;
            do
               {
               listOfCacheStartAddress.push_back((uintptr_t)curCache->cacheStartAddress);
               listOfCacheSizeBytes.push_back(curCache->cacheSizeBytes);
               curCache = curCache->next;
               }
            while (curCache != head);
            }

         client->write(response, vmInfo, listOfCacheStartAddress, listOfCacheSizeBytes);
         }
         break;
      case MessageType::VM_isPrimitiveArray:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->isPrimitiveArray(clazz));
         }
         break;
      case MessageType::VM_getAllocationSize:
         {
         auto recv = client->getRecvData<TR::StaticSymbol *, TR_OpaqueClassBlock *>();
         TR::StaticSymbol *classSym = std::get<0>(recv);
         TR_OpaqueClassBlock *clazz = std::get<1>(recv);
         client->write(response, fe->getAllocationSize(classSym, clazz));
         }
         break;
      case MessageType::VM_getObjectClass:
         {
         TR::VMAccessCriticalSection getObjectClass(fe);
         uintptrj_t objectPointer = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(response, fe->getObjectClass(objectPointer));
         }
         break;
      case MessageType::VM_getStaticReferenceFieldAtAddress:
         {
         TR::VMAccessCriticalSection getStaticReferenceFieldAtAddress(fe);
         uintptrj_t fieldAddress = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(response, fe->getStaticReferenceFieldAtAddress(fieldAddress));
         }
         break;
      case MessageType::VM_stackWalkerMaySkipFrames:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *>();
         TR_OpaqueMethodBlock *method = std::get<0>(recv);
         TR_OpaqueClassBlock *clazz = std::get<1>(recv);
         client->write(response, fe->stackWalkerMaySkipFrames(method, clazz));
         }
         break;
      case MessageType::VM_hasFinalFieldsInClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->hasFinalFieldsInClass(clazz));
         }
         break;
      case MessageType::VM_getClassNameSignatureFromMethod:
         {
         auto recv = client->getRecvData<J9Method*>();
         J9Method *method = std::get<0>(recv);
         J9UTF8 *className, *name, *signature;
         getClassNameSignatureFromMethod(method, className, name, signature);
         std::string classNameStr(utf8Data(className), J9UTF8_LENGTH(className));
         std::string nameStr(utf8Data(name), J9UTF8_LENGTH(name));
         std::string signatureStr(utf8Data(signature), J9UTF8_LENGTH(signature));
         client->write(response, classNameStr, nameStr, signatureStr);
         }
         break;
      case MessageType::VM_getHostClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getHostClass(clazz));
         }
         break;
      case MessageType::VM_getStringUTF8Length:
         {
         uintptrj_t string = std::get<0>(client->getRecvData<uintptrj_t>());
            {
            TR::VMAccessCriticalSection getStringUTF8Length(fe);
            client->write(response, fe->getStringUTF8Length(string));
            }
         }
         break;
      case MessageType::VM_classInitIsFinished:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->classInitIsFinished(clazz));
         }
         break;
      case MessageType::VM_getNewArrayTypeFromClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getNewArrayTypeFromClass(clazz));
         }
         break;
      case MessageType::VM_getClassFromNewArrayType:
         {
         int32_t index = std::get<0>(client->getRecvData<int32_t>());
         client->write(response, fe->getClassFromNewArrayType(index));
         }
         break;
      case MessageType::VM_isCloneable:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->isCloneable(clazz));
         }
         break;
      case MessageType::VM_canAllocateInlineClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->canAllocateInlineClass(clazz));
         }
         break;
      case MessageType::VM_getArrayClassFromComponentClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getArrayClassFromComponentClass(clazz));
         }
         break;
      case MessageType::VM_matchRAMclassFromROMclass:
         {
         J9ROMClass *clazz = std::get<0>(client->getRecvData<J9ROMClass *>());
         client->write(response, fe->matchRAMclassFromROMclass(clazz, comp));
         }
         break;
      case MessageType::VM_getReferenceFieldAtAddress:
         {
         TR::VMAccessCriticalSection getReferenceFieldAtAddress(fe);
         uintptrj_t fieldAddress = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(response, fe->getReferenceFieldAtAddress(fieldAddress));
         }
         break;
      case MessageType::VM_getReferenceFieldAt:
         {
         auto recv = client->getRecvData<uintptrj_t, uintptrj_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         client->write(response, fe->getReferenceFieldAt(objectPointer, fieldOffset));
         }
         break;
      case MessageType::VM_getVolatileReferenceFieldAt:
         {
         auto recv = client->getRecvData<uintptrj_t, uintptrj_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         client->write(response, fe->getVolatileReferenceFieldAt(objectPointer, fieldOffset));
         }
         break;
      case MessageType::VM_getInt32FieldAt:
         {
         if (compInfoPT->getLastLocalGCCounter() != compInfoPT->getCompilationInfo()->getLocalGCCounter())
            {
            // GC happened, fail compilation
            auto comp = compInfoPT->getCompilation();
            comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted due to GC");
            }

         auto recv = client->getRecvData<uintptrj_t, uintptrj_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         client->write(response, fe->getInt32FieldAt(objectPointer, fieldOffset));
         }
         break;
      case MessageType::VM_getInt64FieldAt:
         {
         auto recv = client->getRecvData<uintptrj_t, uintptrj_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         client->write(response, fe->getInt64FieldAt(objectPointer, fieldOffset));
         }
         break;
      case MessageType::VM_setInt64FieldAt:
         {
         auto recv = client->getRecvData<uintptrj_t, uintptrj_t, int64_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         int64_t newValue = std::get<2>(recv);
         fe->setInt64FieldAt(objectPointer, fieldOffset, newValue);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::VM_compareAndSwapInt64FieldAt:
         {
         auto recv = client->getRecvData<uintptrj_t, uintptrj_t, int64_t, int64_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         int64_t oldValue = std::get<2>(recv);
         int64_t newValue = std::get<3>(recv);
         client->write(response, fe->compareAndSwapInt64FieldAt(objectPointer, fieldOffset, oldValue, newValue));
         }
         break;
      case MessageType::VM_getArrayLengthInElements:
         {
         if (compInfoPT->getLastLocalGCCounter() != compInfoPT->getCompilationInfo()->getLocalGCCounter())
            {
            // GC happened, fail compilation
            auto comp = compInfoPT->getCompilation();
            comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted due to GC");
            }

         uintptrj_t objectPointer = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(response, fe->getArrayLengthInElements(objectPointer));
         }
         break;
      case MessageType::VM_getClassFromJavaLangClass:
         {
         uintptrj_t objectPointer = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(response, fe->getClassFromJavaLangClass(objectPointer));
         }
         break;
      case MessageType::VM_getOffsetOfClassFromJavaLangClassField:
         {
         client->getRecvData<JITServer::Void>();
         client->write(response, fe->getOffsetOfClassFromJavaLangClassField());
         }
         break;
      case MessageType::VM_getIdentityHashSaltPolicy:
         {
         client->getRecvData<JITServer::Void>();
         client->write(response, fe->getIdentityHashSaltPolicy());
         }
         break;
      case MessageType::VM_getOffsetOfJLThreadJ9Thread:
         {
         client->getRecvData<JITServer::Void>();
         client->write(response, fe->getOffsetOfJLThreadJ9Thread());
         }
         break;
      case MessageType::VM_scanReferenceSlotsInClassForOffset:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, int32_t>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         int32_t offset = std::get<1>(recv);
         client->write(response, fe->scanReferenceSlotsInClassForOffset(comp, clazz, offset));
         }
         break;
      case MessageType::VM_findFirstHotFieldTenuredClassOffset:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->findFirstHotFieldTenuredClassOffset(comp, clazz));
         }
         break;
      case MessageType::VM_getResolvedVirtualMethod:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, I_32, bool>();
         auto clazz = std::get<0>(recv);
         auto offset = std::get<1>(recv);
         auto ignoreRTResolve = std::get<2>(recv);
         client->write(response, fe->getResolvedVirtualMethod(clazz, offset, ignoreRTResolve));
         }
         break;
      case MessageType::VM_getJ2IThunk:
         {
         auto recv = client->getRecvData<std::string>();
         std::string signature = std::get<0>(recv);
         client->write(response, fe->getJ2IThunk(&signature[0], signature.size(), comp));
         }
         break;
      case MessageType::VM_setJ2IThunk:
         {
         auto recv = client->getRecvData<std::string, std::string>();
         std::string signature = std::get<0>(recv);
         std::string serializedThunk = std::get<1>(recv);

         void *thunkAddress;
         if (!comp->compileRelocatableCode())
            {
            // For non-AOT, copy thunk to code cache and relocate the vm helper address right away
            uint8_t *thunkStart = TR_JITServerRelocationRuntime::copyDataToCodeCache(serializedThunk.data(), serializedThunk.size(), fe);
            if (!thunkStart)
               compInfoPT->getCompilation()->failCompilation<TR::CodeCacheError>("Failed to allocate space in the code cache");

            thunkAddress = thunkStart + 8;
            void *vmHelper = j9ThunkVMHelperFromSignature(fe->_jitConfig, signature.size(), &signature[0]);
            compInfoPT->reloRuntime()->reloTarget()->performThunkRelocation(reinterpret_cast<uint8_t *>(thunkAddress), (UDATA)vmHelper);
            }
         else
            {
            // For AOT, set address to received string, because it will be stored to SCC, so
            // no need for code cache allocation
            thunkAddress = reinterpret_cast<void *>(&serializedThunk[0] + 8);
            }

         // Ideally, should use signature.data() here, but setJ2IThunk has non-const pointer
         // as argument, and it uses it to invoke a VM function that also takes non-const pointer.
         thunkAddress = fe->setJ2IThunk(&signature[0], signature.size(), thunkAddress, comp);

         client->write(response, thunkAddress);
         }
         break;
      case MessageType::VM_needsInvokeExactJ2IThunk:
         {
         auto recv = client->getRecvData<std::string>();
         std::string signature = std::get<0>(recv);

         TR_J2IThunkTable *thunkTable = comp->getPersistentInfo()->getInvokeExactJ2IThunkTable();
         // Ideally, should use signature.data() here, but findThunk takes non-const pointer
         TR_J2IThunk *thunk = thunkTable->findThunk(&signature[0], fe);
         client->write(response, thunk == NULL);
         }
         break;
      case MessageType::VM_setInvokeExactJ2IThunk:
         {
         auto recv = client->getRecvData<std::string>();
         std::string &serializedThunk = std::get<0>(recv);

         // Do not need relocation here, because helper address should have been originally
         // fetched from the client.
         uint8_t *thunkStart = TR_JITServerRelocationRuntime::copyDataToCodeCache(serializedThunk.data(), serializedThunk.size(), fe);
         if (!thunkStart)
            compInfoPT->getCompilation()->failCompilation<TR::CodeCacheError>("Failed to allocate space in the code cache");

         void *thunkAddress = reinterpret_cast<void *>(thunkStart);
         fe->setInvokeExactJ2IThunk(thunkAddress, comp);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::VM_sameClassLoaders:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, TR_OpaqueClassBlock *>();
         TR_OpaqueClassBlock *class1 = std::get<0>(recv);
         TR_OpaqueClassBlock *class2 = std::get<1>(recv);
         client->write(response, fe->sameClassLoaders(class1, class2));
         }
         break;
      case MessageType::VM_isUnloadAssumptionRequired:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, TR_ResolvedMethod *>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         TR_ResolvedMethod *method = std::get<1>(recv);
         client->write(response, fe->isUnloadAssumptionRequired(clazz, method));
         }
         break;
      case MessageType::VM_getInstanceFieldOffset:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, std::string, std::string, UDATA>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         std::string field = std::get<1>(recv);
         std::string sig = std::get<2>(recv);
         UDATA options = std::get<3>(recv);
         client->write(response, fe->getInstanceFieldOffset(clazz, const_cast<char*>(field.c_str()), field.length(),
                  const_cast<char*>(sig.c_str()), sig.length(), options));
         }
         break;
      case MessageType::VM_getJavaLangClassHashCode:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         bool hashCodeComputed = false;
         int32_t hashCode = fe->getJavaLangClassHashCode(comp, clazz, hashCodeComputed);
         client->write(response, hashCode, hashCodeComputed);
         }
         break;
      case MessageType::VM_hasFinalizer:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->hasFinalizer(clazz));
         }
         break;
      case MessageType::VM_getClassDepthAndFlagsValue:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getClassDepthAndFlagsValue(clazz));
         }
         break;
      case MessageType::VM_getMethodFromName:
         {
         auto recv = client->getRecvData<std::string, std::string, std::string>();
         std::string className = std::get<0>(recv);
         std::string methodName = std::get<1>(recv);
         std::string signature = std::get<2>(recv);
         client->write(response, fe->getMethodFromName(const_cast<char*>(className.c_str()),
                  const_cast<char*>(methodName.c_str()), const_cast<char*>(signature.c_str())));
         }
         break;
      case MessageType::VM_getMethodFromClass:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, std::string, std::string, TR_OpaqueClassBlock *>();
         TR_OpaqueClassBlock *methodClass = std::get<0>(recv);
         std::string methodName = std::get<1>(recv);
         std::string signature = std::get<2>(recv);
         TR_OpaqueClassBlock *callingClass = std::get<3>(recv);
         client->write(response, fe->getMethodFromClass(methodClass, const_cast<char*>(methodName.c_str()),
                  const_cast<char*>(signature.c_str()), callingClass));
         }
         break;
      case MessageType::VM_createMethodHandleArchetypeSpecimen:
         {
         auto recv = client->getRecvData<uintptrj_t*>();
         uintptrj_t *methodHandleLocation = std::get<0>(recv);
         intptrj_t length;
         char *thunkableSignature;
            {
            TR::VMAccessCriticalSection createMethodHandleArchetypeSpecimen(fe);
            TR_OpaqueMethodBlock *archetype = fe->lookupMethodHandleThunkArchetype(*methodHandleLocation);
            uintptrj_t signatureString = fe->getReferenceField(fe->getReferenceField(
               *methodHandleLocation,
               "thunks",             "Ljava/lang/invoke/ThunkTuple;"),
               "thunkableSignature", "Ljava/lang/String;");
            length = fe->getStringUTF8Length(signatureString);
            thunkableSignature = (char*)trMemory->allocateStackMemory(length+1);
            fe->getStringUTF8(signatureString, thunkableSignature, length+1);
            client->write(response, archetype, std::string(thunkableSignature, length));
            }
         }
         break;
      case MessageType::VM_isClassVisible:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, TR_OpaqueClassBlock *>();
         TR_OpaqueClassBlock *sourceClass = std::get<0>(recv);
         TR_OpaqueClassBlock *destClass = std::get<1>(recv);
         client->write(response, fe->isClassVisible(sourceClass, destClass));
         }
         break;
      case MessageType::VM_markClassForTenuredAlignment:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, uint32_t>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         uint32_t alignFromStart = std::get<1>(recv);
         fe->markClassForTenuredAlignment(comp, clazz, alignFromStart);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::VM_getReferenceSlotsInClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         int32_t *start = fe->getReferenceSlotsInClass(comp, clazz);
         if (!start)
            client->write(response, std::string(""));
         else
            {
            int32_t numSlots = 0;
            for (; start[numSlots]; ++numSlots);
            // Copy the null terminated array into a string
            std::string slotsStr((char *) start, (1 + numSlots) * sizeof(int32_t));
            client->write(response, slotsStr);
            }
         }
         break;
      case MessageType::VM_getMethodSize:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(response, fe->getMethodSize(method));
         }
         break;
      case MessageType::VM_addressOfFirstClassStatic:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->addressOfFirstClassStatic(clazz));
         }
         break;
      case MessageType::VM_getStaticFieldAddress:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, std::string, std::string>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         std::string fieldName = std::get<1>(recv);
         std::string sigName = std::get<2>(recv);
         client->write(response, fe->getStaticFieldAddress(clazz,
                  reinterpret_cast<unsigned char*>(const_cast<char*>(fieldName.c_str())),
                  fieldName.length(),
                  reinterpret_cast<unsigned char*>(const_cast<char*>(sigName.c_str())),
                  sigName.length()));
         }
         break;
      case MessageType::VM_getInterpreterVTableSlot:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *>();
         TR_OpaqueMethodBlock *method = std::get<0>(recv);
         TR_OpaqueClassBlock *clazz = std::get<1>(recv);
         client->write(response, fe->getInterpreterVTableSlot(method, clazz));
         }
         break;
      case MessageType::VM_revertToInterpreted:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         fe->revertToInterpreted(method);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::VM_getLocationOfClassLoaderObjectPointer:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getLocationOfClassLoaderObjectPointer(clazz));
         }
         break;
      case MessageType::VM_isOwnableSyncClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->isOwnableSyncClass(clazz));
         }
         break;
      case MessageType::VM_getClassFromMethodBlock:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(response, fe->getClassFromMethodBlock(method));
         }
         break;
      case MessageType::VM_fetchMethodExtendedFlagsPointer:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(response, fe->fetchMethodExtendedFlagsPointer(method));
         }
         break;
      case MessageType::VM_stringEquals:
         {
         auto recv = client->getRecvData<uintptrj_t *, uintptrj_t *>();
         uintptrj_t *stringLocation1 = std::get<0>(recv);
         uintptrj_t *stringLocation2 = std::get<1>(recv);
         int32_t result;
         bool isEqual = fe->stringEquals(comp, stringLocation1, stringLocation2, result);
         client->write(response, result, isEqual);
         }
         break;
      case MessageType::VM_getStringHashCode:
         {
         uintptrj_t *stringLocation = std::get<0>(client->getRecvData<uintptrj_t *>());
         int32_t result;
         bool isGet = fe->getStringHashCode(comp, stringLocation, result);
         client->write(response, result, isGet);
         }
         break;
      case MessageType::VM_getLineNumberForMethodAndByteCodeIndex:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock *, int32_t>();
         TR_OpaqueMethodBlock *method = std::get<0>(recv);
         int32_t bcIndex = std::get<1>(recv);
         client->write(response, fe->getLineNumberForMethodAndByteCodeIndex(method, bcIndex));
         }
         break;
      case MessageType::VM_getObjectNewInstanceImplMethod:
         {
         client->getRecvData<JITServer::Void>();
         client->write(response, fe->getObjectNewInstanceImplMethod());
         }
         break;
      case MessageType::VM_getBytecodePC:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(response, TR::Compiler->mtd.bytecodeStart(method));
         }
         break;
      case MessageType::VM_isClassLoadedBySystemClassLoader:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->isClassLoadedBySystemClassLoader(clazz));
         }
         break;
      case MessageType::VM_getVFTEntry:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, int32_t>();
         client->write(response, fe->getVFTEntry(std::get<0>(recv), std::get<1>(recv)));
         }
         break;
      case MessageType::VM_isClassArray:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock*>();
         auto clazz = std::get<0>(recv);
         client->write(response, fe->isClassArray(clazz));
         }
         break;
      case MessageType::VM_instanceOfOrCheckCast:
         {
         auto recv = client->getRecvData<J9Class*, J9Class*>();
         auto clazz1 = std::get<0>(recv);
         auto clazz2 = std::get<1>(recv);
         client->write(response, fe->instanceOfOrCheckCast(clazz1, clazz2));
         }
         break;
      case MessageType::VM_transformJlrMethodInvoke:
         {
         auto recv = client->getRecvData<J9Method*, J9Class*>();
         auto method = std::get<0>(recv);
         auto clazz = std::get<1>(recv);
         client->write(response, fe->transformJlrMethodInvoke(method, clazz));
         }
         break;
      case MessageType::VM_isAnonymousClass:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         client->write(response, fe->isAnonymousClass(clazz));
         }
         break;
      case MessageType::VM_dereferenceStaticAddress:
         {
         auto recv = client->getRecvData<void *, TR::DataType>();
         void *address = std::get<0>(recv);
         auto addressType = std::get<1>(recv);
         client->write(response, fe->dereferenceStaticFinalAddress(address, addressType));
         }
         break;
      case MessageType::VM_getClassFromCP:
         {
         auto recv = client->getRecvData<J9ConstantPool *>();
         client->write(response, fe->getClassFromCP(std::get<0>(recv)));
         }
         break;
      case MessageType::VM_getROMMethodFromRAMMethod:
         {
         auto recv = client->getRecvData<J9Method *>();
         client->write(response, fe->getROMMethodFromRAMMethod(std::get<0>(recv)));
         }
         break;
      case MessageType::mirrorResolvedJ9Method:
         {
         // allocate a new TR_ResolvedJ9Method on the heap, to be used as a mirror for performing actions which are only
         // easily done on the client side.
         auto recv = client->getRecvData<TR_OpaqueMethodBlock *, TR_ResolvedJ9Method *, uint32_t, bool>();
         TR_OpaqueMethodBlock *method = std::get<0>(recv);
         auto *owningMethod = std::get<1>(recv);
         uint32_t vTableSlot = std::get<2>(recv);
         bool isAOT = std::get<3>(recv);
         TR_ResolvedJ9JITServerMethodInfo methodInfo;
         // if in AOT mode, create a relocatable method mirror
         TR_ResolvedJ9JITServerMethod::createResolvedMethodMirror(methodInfo, method, vTableSlot, owningMethod, fe, trMemory);

         client->write(response, methodInfo);
         }
         break;
      case MessageType::ResolvedMethod_getRemoteROMClassAndMethods:
         {
         J9Class *clazz = std::get<0>(client->getRecvData<J9Class *>());
         client->write(response, JITServerHelpers::packRemoteROMClassInfo(clazz, fe->vmThread(), trMemory));
         }
         break;
      case MessageType::ResolvedMethod_isJNINative:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, method->isJNINative());
         }
         break;
      case MessageType::ResolvedMethod_isInterpreted:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, method->isInterpreted());
         }
         break;
      case MessageType::ResolvedMethod_staticAttributes:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         int32_t isStore = std::get<2>(recv);
         int32_t needAOTValidation = std::get<3>(recv);
         void *address;
         TR::DataType type;
         bool volatileP, isFinal, isPrivate, unresolvedInCP;
         bool result = method->staticAttributes(comp, cpIndex, &address, &type, &volatileP, &isFinal, &isPrivate, isStore, &unresolvedInCP, needAOTValidation);
         TR_J9MethodFieldAttributes attrs(reinterpret_cast<uintptr_t>(address), type.getDataType(), volatileP, isFinal, isPrivate, unresolvedInCP, result);
         client->write(response, attrs);
         }
         break;
      case MessageType::ResolvedMethod_getClassFromConstantPool:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, uint32_t, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         uint32_t cpIndex = std::get<1>(recv);
         bool returnClassForAOT = std::get<2>(recv);
         client->write(response, method->getClassFromConstantPool(comp, cpIndex, returnClassForAOT));
         }
         break;
      case MessageType::ResolvedMethod_getDeclaringClassFromFieldOrStatic:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(response, method->getDeclaringClassFromFieldOrStatic(comp, cpIndex));
         }
         break;
      case MessageType::ResolvedMethod_classOfStatic:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         int32_t returnClassForAOT = std::get<2>(recv);
         client->write(response, method->classOfStatic(cpIndex, returnClassForAOT));
         }
         break;
      case MessageType::ResolvedMethod_fieldAttributes:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         I_32 cpIndex = std::get<1>(recv);
         bool isStore = std::get<2>(recv);
         bool needAOTValidation = std::get<3>(recv);
         U_32 fieldOffset;
         TR::DataType type;
         bool volatileP, isFinal, isPrivate, unresolvedInCP;
         bool result = method->fieldAttributes(comp, cpIndex, &fieldOffset, &type, &volatileP, &isFinal, &isPrivate, isStore, &unresolvedInCP, needAOTValidation);
         TR_J9MethodFieldAttributes attrs(static_cast<uintptr_t>(fieldOffset), type.getDataType(), volatileP, isFinal, isPrivate, unresolvedInCP, result);
         client->write(response, attrs);
         }
         break;
      case MessageType::ResolvedMethod_getResolvedStaticMethodAndMirror:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, I_32>();
         auto *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         bool unresolvedInCP;
         J9Method *ramMethod = jitGetJ9MethodUsingIndex(fe->vmThread(), method->cp(), cpIndex);
         unresolvedInCP = !ramMethod || !J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod);

            {
            TR::VMAccessCriticalSection resolveStaticMethodRef(fe);
            ramMethod = jitResolveStaticMethodRef(fe->vmThread(), method->cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
            }

            // Create a mirror right away
         TR_ResolvedJ9JITServerMethodInfo methodInfo;
         if (ramMethod)
            TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, method, fe, trMemory);

         client->write(response, ramMethod, methodInfo, unresolvedInCP);
         }
         break;
      case MessageType::ResolvedMethod_getResolvedSpecialMethodAndMirror:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, I_32>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         J9Method *ramMethod = NULL;
         TR_ResolvedJ9JITServerMethodInfo methodInfo;
         if (!((fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
               !comp->ilGenRequest().details().isMethodHandleThunk() &&
               performTransformation(comp, "Setting as unresolved special call cpIndex=%d\n",cpIndex)))
            {
            TR::VMAccessCriticalSection resolveSpecialMethodRef(fe);
            ramMethod = jitResolveSpecialMethodRef(fe->vmThread(), method->cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);

            if (ramMethod)
               TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, method, fe, trMemory);
            }

         client->write(response, ramMethod, methodInfo);
         }
         break;
      case MessageType::ResolvedMethod_classCPIndexOfMethod:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, uint32_t>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(response, method->classCPIndexOfMethod(cpIndex));
         }
         break;
      case MessageType::ResolvedMethod_startAddressForJittedMethod:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, method->startAddressForJittedMethod());
         }
         break;
      case MessageType::ResolvedMethod_getResolvedPossiblyPrivateVirtualMethodAndMirror:
         {
         // 1. Resolve method
         auto recv = client->getRecvData<TR_ResolvedMethod *, J9RAMConstantPoolItem *, I_32>();
         auto *owningMethod = std::get<0>(recv);
         auto literals = std::get<1>(recv);
         J9ConstantPool *cp = (J9ConstantPool*)literals;
         I_32 cpIndex = std::get<2>(recv);
         bool unresolvedInCP = true;

         // Only call the resolve if unresolved
         J9Method * ramMethod = 0;
         UDATA vTableIndex = (((J9RAMVirtualMethodRef*) literals)[cpIndex]).methodIndexAndArgCount;
         vTableIndex >>= 8;
         if ((TR::Compiler->vm.getInterpreterVTableOffset() + sizeof(uintptrj_t)) == vTableIndex)
            {
            TR::VMAccessCriticalSection resolveVirtualMethodRef(fe);
            vTableIndex = fe->_vmFunctionTable->resolveVirtualMethodRefInto(fe->vmThread(), cp, cpIndex,
                  J9_RESOLVE_FLAG_JIT_COMPILE_TIME, &ramMethod, NULL);
            }
         else if (!TR_ResolvedJ9Method::isInvokePrivateVTableOffset(vTableIndex))
            {
            // Go fishing for the J9Method...
            uint32_t classIndex = ((J9ROMMethodRef *) cp->romConstantPool)[cpIndex].classRefCPIndex;
            J9Class * classObject = (((J9RAMClassRef*) literals)[classIndex]).value;
            ramMethod = *(J9Method **)((char *)classObject + vTableIndex);
            unresolvedInCP = false;
            }

         if(TR_ResolvedJ9Method::isInvokePrivateVTableOffset(vTableIndex))
            ramMethod = (((J9RAMVirtualMethodRef*) literals)[cpIndex]).method;

         // 2. Mirror the resolved method on the client
         if (vTableIndex)
            {
            TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *) ramMethod;

            TR_ResolvedJ9JITServerMethodInfo methodInfo;
            TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, (uint32_t) vTableIndex, owningMethod, fe, trMemory);

            client->write(response, ramMethod, vTableIndex, unresolvedInCP, methodInfo);
            }
         else
            {
            client->write(response, ramMethod, vTableIndex, unresolvedInCP, TR_ResolvedJ9JITServerMethodInfo());
            }
         }
         break;
      case MessageType::ResolvedMethod_getResolvedVirtualMethod:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, I_32, bool, TR_ResolvedJ9Method *>();
         auto clazz = std::get<0>(recv);
         auto offset = std::get<1>(recv);
         auto ignoreRTResolve = std::get<2>(recv);
         auto owningMethod = std::get<3>(recv);
         TR_OpaqueMethodBlock *ramMethod = fe->getResolvedVirtualMethod(clazz, offset, ignoreRTResolve);
         TR_ResolvedJ9JITServerMethodInfo methodInfo;
         if (ramMethod)
            TR_ResolvedJ9JITServerMethod::createResolvedMethodMirror(methodInfo, ramMethod, 0, owningMethod, fe, trMemory);
         client->write(response, ramMethod, methodInfo);
         }
         break;
      case MessageType::ResolvedMethod_virtualMethodIsOverridden:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, method->virtualMethodIsOverridden());
         }
         break;
      case MessageType::get_params_to_construct_TR_j9method:
         {
         auto recv = client->getRecvData<J9Class *, uintptr_t>();
         J9Class * aClazz = std::get<0>(recv);
         uintptr_t cpIndex = std::get<1>(recv);
         J9ROMClass * romClass = aClazz->romClass;
         uintptr_t realCPIndex = jitGetRealCPIndex(fe->vmThread(), romClass, cpIndex);
         J9ROMMethodRef * romRef = &J9ROM_CP_BASE(romClass, J9ROMMethodRef)[realCPIndex];
         J9ROMClassRef * classRef = &J9ROM_CP_BASE(romClass, J9ROMClassRef)[romRef->classRefCPIndex];
         J9ROMNameAndSignature * nameAndSignature = J9ROMMETHODREF_NAMEANDSIGNATURE(romRef);
         J9UTF8 * className = J9ROMCLASSREF_NAME(classRef);
         J9UTF8 * name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature);
         J9UTF8 * signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature);
         std::string classNameStr(utf8Data(className), J9UTF8_LENGTH(className));
         std::string nameStr(utf8Data(name), J9UTF8_LENGTH(name));
         std::string signatureStr(utf8Data(signature), J9UTF8_LENGTH(signature));
         client->write(response, classNameStr, nameStr, signatureStr);
         }
         break;
      case MessageType::ResolvedMethod_setRecognizedMethodInfo:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, TR::RecognizedMethod>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         TR::RecognizedMethod rm = std::get<1>(recv);
         method->setRecognizedMethodInfo(rm);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::ResolvedMethod_localName:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, U_32, U_32>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         U_32 slotNumber = std::get<1>(recv);
         U_32 bcIndex = std::get<2>(recv);
         I_32 len;
         char *nameChars = method->localName(slotNumber, bcIndex, len, trMemory);
         if (nameChars)
            client->write(response, std::string(nameChars, len));
         else
            client->write(response, std::string());
         }
         break;
      case MessageType::ResolvedMethod_jniNativeMethodProperties:
         {
         auto ramMethod = std::get<0>(client->getRecvData<J9Method*>());
         uintptrj_t jniProperties;
         void *jniTargetAddress = fe->getJ9JITConfig()->javaVM->internalVMFunctions->jniNativeMethodProperties(fe->vmThread(), ramMethod, &jniProperties);
         client->write(response, jniProperties, jniTargetAddress);
         }
         break;
      case MessageType::ResolvedMethod_getResolvedInterfaceMethod_2:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, I_32>();
         auto mirror = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         UDATA pITableIndex;
         TR_OpaqueClassBlock *clazz = mirror->getResolvedInterfaceMethod(cpIndex, &pITableIndex);
         client->write(response, clazz, pITableIndex);
         }
         break;
      case MessageType::ResolvedMethod_getResolvedInterfaceMethodAndMirror_3:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *, I_32, TR_ResolvedJ9Method *>();
         auto method = std::get<0>(recv);
         auto clazz = std::get<1>(recv);
         auto cpIndex = std::get<2>(recv);
         auto owningMethod = std::get<3>(recv);
         J9Method * ramMethod = (J9Method *)fe->getResolvedInterfaceMethod(method, clazz, cpIndex);
         bool resolved = ramMethod && J9_BYTECODE_START_FROM_RAM_METHOD(ramMethod);

         // Create a mirror right away
         TR_ResolvedJ9JITServerMethodInfo methodInfo;
         if (resolved)
            TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, owningMethod, fe, trMemory);

         client->write(response, resolved, ramMethod, methodInfo);
         }
         break;
      case MessageType::ResolvedMethod_getResolvedInterfaceMethodOffset:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, TR_OpaqueClassBlock*, I_32>();
         auto mirror = std::get<0>(recv);
         auto clazz = std::get<1>(recv);
         auto cpIndex = std::get<2>(recv);
         U_32 offset = mirror->getResolvedInterfaceMethodOffset(clazz, cpIndex);
         client->write(response, offset);
         }
         break;
      case MessageType::ResolvedMethod_getResolvedImproperInterfaceMethodAndMirror:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, I_32>();
         auto mirror = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         UDATA vtableOffset = 0;
         J9Method *j9method = NULL;
            {
            TR::VMAccessCriticalSection getResolvedHandleMethod(fe);
            j9method = jitGetImproperInterfaceMethodFromCP(fe->vmThread(), mirror->cp(), cpIndex, &vtableOffset);
            }
         // Create a mirror right away
         TR_ResolvedJ9JITServerMethodInfo methodInfo;
         if (j9method)
            TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) j9method, (uint32_t)vtableOffset, mirror, fe, trMemory);

         client->write(response, j9method, methodInfo, vtableOffset);
         }
         break;
      case MessageType::ResolvedMethod_startAddressForJNIMethod:
         {
         TR_ResolvedJ9Method *ramMethod = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, ramMethod->startAddressForJNIMethod(comp));
         }
         break;
      case MessageType::ResolvedMethod_startAddressForInterpreterOfJittedMethod:
         {
         TR_ResolvedJ9Method *ramMethod = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, ramMethod->startAddressForInterpreterOfJittedMethod());
         }
         break;
      case MessageType::ResolvedMethod_getUnresolvedStaticMethodInCP:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method= std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(response, method->getUnresolvedStaticMethodInCP(cpIndex));
         }
         break;
      case MessageType::ResolvedMethod_getUnresolvedSpecialMethodInCP:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method= std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(response, method->getUnresolvedSpecialMethodInCP(cpIndex));
         }
         break;
      case MessageType::ResolvedMethod_getUnresolvedFieldInCP:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method= std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(response, method->getUnresolvedFieldInCP(cpIndex));
         }
         break;
      case MessageType::ResolvedMethod_getRemoteROMString:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, size_t, std::string>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         size_t offsetFromROMClass = std::get<1>(recv);
         std::string offsetsStr = std::get<2>(recv);
         size_t numOffsets = offsetsStr.size() / sizeof(size_t);
         size_t *offsets = (size_t*) &offsetsStr[0];
         uint8_t *ptr = (uint8_t*) method->romClassPtr() + offsetFromROMClass;
         for (size_t i = 0; i < numOffsets; i++)
            {
            size_t offset = offsets[i];
            ptr = ptr + offset + *(J9SRP*)(ptr + offset);
            }
         int32_t len;
         char * data = utf8Data((J9UTF8*) ptr, len);
         client->write(response, std::string(data, len));
         }
         break;
      case MessageType::ClassInfo_getRemoteROMString:
         {
         auto recv = client->getRecvData<J9ROMClass *, size_t, std::string>();
         J9ROMClass *romClass = std::get<0>(recv);
         size_t offsetFromROMClass = std::get<1>(recv);
         std::string offsetsStr = std::get<2>(recv);
         size_t numOffsets = offsetsStr.size() / sizeof(size_t);
         size_t *offsets = (size_t*) &offsetsStr[0];
         uint8_t *ptr = (uint8_t*) romClass + offsetFromROMClass;
         for (size_t i = 0; i < numOffsets; i++)
            {
            size_t offset = offsets[i];
            ptr = ptr + offset + *(J9SRP*)(ptr + offset);
            }
         int32_t len;
         char * data = utf8Data((J9UTF8*) ptr, len);
         client->write(response, std::string(data, len));
         }
         break;
      case MessageType::ResolvedMethod_fieldOrStaticName:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         size_t cpIndex = std::get<1>(recv);
         int32_t len;

         // Call staticName here in lieu of fieldOrStaticName as the server has already checked for "<internal field>"
         char *s = method->staticName(cpIndex, len, trMemory, heapAlloc);
         client->write(response, std::string(s, len));
         }
         break;
      case MessageType::ResolvedMethod_isSubjectToPhaseChange:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, method->isSubjectToPhaseChange(comp));
         }
         break;
      case MessageType::ResolvedMethod_getResolvedHandleMethod:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, I_32>();
         auto mirror = std::get<0>(recv);
         I_32 cpIndex = std::get<1>(recv);
         TR::VMAccessCriticalSection getResolvedHandleMethod(fe);

#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
         bool unresolvedInCP = mirror->isUnresolvedMethodTypeTableEntry(cpIndex);
         TR_OpaqueMethodBlock *dummyInvokeExact = fe->getMethodFromName("java/lang/invoke/MethodHandle",
               "invokeExact", "([Ljava/lang/Object;)Ljava/lang/Object;");
         J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)(mirror->cp()->romConstantPool + cpIndex);
         J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
         int32_t signatureLength;
         char   *signature = utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), signatureLength);
#else
         bool unresolvedInCP = mirror->isUnresolvedMethodType(cpIndex);
         TR_OpaqueMethodBlock *dummyInvokeExact = fe->getMethodFromName("java/lang/invoke/MethodHandle",
               "invokeExact", "([Ljava/lang/Object;)Ljava/lang/Object;");
         J9ROMMethodTypeRef *romMethodTypeRef = (J9ROMMethodTypeRef *)(mirror->cp()->romConstantPool + cpIndex);
         int32_t signatureLength;
         char   *signature = utf8Data(J9ROMMETHODTYPEREF_SIGNATURE(romMethodTypeRef), signatureLength);
#endif
         client->write(response, dummyInvokeExact, std::string(signature, signatureLength), unresolvedInCP);
         }
         break;
#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
      case MessageType::ResolvedMethod_methodTypeTableEntryAddress:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, I_32>();
         auto mirror = std::get<0>(recv);
         I_32 cpIndex = std::get<1>(recv);
         client->write(response, mirror->methodTypeTableEntryAddress(cpIndex));
         }
         break;
      case MessageType::ResolvedMethod_isUnresolvedMethodTypeTableEntry:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, I_32>();
         auto mirror = std::get<0>(recv);
         I_32 cpIndex = std::get<1>(recv);
         client->write(response, mirror->isUnresolvedMethodTypeTableEntry(cpIndex));
         }
         break;
#endif
      case MessageType::ResolvedMethod_isUnresolvedCallSiteTableEntry:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, int32_t>();
         auto mirror = std::get<0>(recv);
         int32_t callSiteIndex = std::get<1>(recv);
         client->write(response, mirror->isUnresolvedCallSiteTableEntry(callSiteIndex));
         }
         break;
      case MessageType::ResolvedMethod_callSiteTableEntryAddress:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, int32_t>();
         auto mirror = std::get<0>(recv);
         int32_t callSiteIndex = std::get<1>(recv);
         client->write(response, mirror->callSiteTableEntryAddress(callSiteIndex));
         }
         break;
      case MessageType::ResolvedMethod_varHandleMethodTypeTableEntryAddress:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, int32_t>();
         auto mirror = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(response, mirror->varHandleMethodTypeTableEntryAddress(cpIndex));
         }
      case MessageType::ResolvedMethod_isUnresolvedVarHandleMethodTypeTableEntry:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, int32_t>();
         auto mirror = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(response, mirror->isUnresolvedVarHandleMethodTypeTableEntry(cpIndex));
         }
      case MessageType::ResolvedMethod_getResolvedDynamicMethod:
         {
         auto recv = client->getRecvData<int32_t, J9Class *>();
         int32_t callSiteIndex = std::get<0>(recv);
         J9Class *clazz = std::get<1>(recv);

         TR::VMAccessCriticalSection getResolvedDynamicMethod(fe);
         J9ROMClass *romClass = clazz->romClass;
         bool unresolvedInCP = (clazz->callSites[callSiteIndex] == NULL);
         J9SRP                 *namesAndSigs = (J9SRP*)J9ROMCLASS_CALLSITEDATA(romClass);
         J9ROMNameAndSignature *nameAndSig   = NNSRP_GET(namesAndSigs[callSiteIndex], J9ROMNameAndSignature*);
         J9UTF8                *signature    = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
         TR_OpaqueMethodBlock *dummyInvokeExact = fe->getMethodFromName("java/lang/invoke/MethodHandle",
               "invokeExact", "([Ljava/lang/Object;)Ljava/lang/Object;");
         client->write(response, dummyInvokeExact, std::string(utf8Data(signature), J9UTF8_LENGTH(signature)), unresolvedInCP);
         }
         break;
      case MessageType::ResolvedMethod_shouldFailSetRecognizedMethodInfoBecauseOfHCR:
         {
         auto mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method*>());
         client->write(response, mirror->shouldFailSetRecognizedMethodInfoBecauseOfHCR());
         }
         break;
      case MessageType::ResolvedMethod_isSameMethod:
         {
         auto recv = client->getRecvData<uintptrj_t*, uintptrj_t*>();
         client->write(response, *std::get<0>(recv) == *std::get<1>(recv));
         }
         break;
      case MessageType::ResolvedMethod_isBigDecimalMethod:
         {
         J9Method *j9method = std::get<0>(client->getRecvData<J9Method*>());
         client->write(response, TR::Method::isBigDecimalMethod(j9method));
         }
         break;
      case MessageType::ResolvedMethod_isBigDecimalConvertersMethod:
         {
         J9Method *j9method = std::get<0>(client->getRecvData<J9Method*>());
         client->write(response, TR::Method::isBigDecimalConvertersMethod(j9method));
         }
         break;
      case MessageType::ResolvedMethod_isInlineable:
         {
         TR_ResolvedJ9Method *mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, mirror->isInlineable(comp));
         }
         break;
      case MessageType::ResolvedMethod_setWarmCallGraphTooBig:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, uint32_t>();
         TR_ResolvedJ9Method *mirror = std::get<0>(recv);
         uint32_t bcIndex = std::get<1>(recv);
         mirror->setWarmCallGraphTooBig(bcIndex, comp);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::ResolvedMethod_setVirtualMethodIsOverridden:
         {
         TR_ResolvedJ9Method *mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         mirror->setVirtualMethodIsOverridden();
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::ResolvedMethod_addressContainingIsOverriddenBit:
         {
         TR_ResolvedJ9Method *mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, mirror->addressContainingIsOverriddenBit());
         }
         break;
      case MessageType::ResolvedMethod_methodIsNotzAAPEligible:
         {
         TR_ResolvedJ9Method *mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(response, mirror->methodIsNotzAAPEligible());
         }
         break;
      case MessageType::ResolvedMethod_setClassForNewInstance:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, J9Class*>();
         TR_ResolvedJ9Method *mirror = std::get<0>(recv);
         J9Class *clazz = std::get<1>(recv);
         mirror->setClassForNewInstance(clazz);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::ResolvedMethod_getJittedBodyInfo:
         {
         auto mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         void *startPC = mirror->startAddressForInterpreterOfJittedMethod();
         auto bodyInfo = J9::Recompilation::getJittedBodyInfoFromPC(startPC);
         if (!bodyInfo)
            client->write(response, std::string(), std::string());
         else
            client->write(response, std::string((char*) bodyInfo, sizeof(TR_PersistentJittedBodyInfo)),
                          std::string((char*) bodyInfo->getMethodInfo(), sizeof(TR_PersistentMethodInfo)));
         }
         break;
      case MessageType::ResolvedMethod_isUnresolvedString:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool>();
         auto mirror = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         auto optimizeForAOT = std::get<2>(recv);
         client->write(response, mirror->isUnresolvedString(cpIndex, optimizeForAOT));
         }
         break;
      case MessageType::ResolvedMethod_stringConstant:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         auto mirror = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         client->write(response, mirror->stringConstant(cpIndex),
                       mirror->isUnresolvedString(cpIndex, true),
                       mirror->isUnresolvedString(cpIndex, false));
         }
         break;
      case MessageType::ResolvedMethod_getMultipleResolvedMethods:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, std::vector<TR_ResolvedMethodType>, std::vector<int32_t>>();
         auto owningMethod = std::get<0>(recv);
         auto methodTypes = std::get<1>(recv);
         auto cpIndices = std::get<2>(recv);
         int32_t numMethods = methodTypes.size();
         std::vector<J9Method *> ramMethods(numMethods);
         std::vector<uint32_t> vTableOffsets(numMethods);
         std::vector<TR_ResolvedJ9JITServerMethodInfo> methodInfos(numMethods);
         for (int32_t i = 0; i < numMethods; ++i)
            {
            int32_t cpIndex = cpIndices[i];
            TR_ResolvedMethodType type = methodTypes[i];
            J9Method *ramMethod = NULL;
            uint32_t vTableOffset = 0;
            TR_ResolvedJ9JITServerMethodInfo methodInfo;
            bool createMethod = false;
            switch (type)
               {
               case TR_ResolvedMethodType::VirtualFromCP:
                  {
                  ramMethod = (J9Method *) TR_ResolvedJ9Method::getVirtualMethod(fe, owningMethod->cp(), cpIndex, (UDATA *) &vTableOffset, NULL);
                  if (ramMethod && vTableOffset) createMethod = true;
                  break;
                  }
               case TR_ResolvedMethodType::Static:
                  {
                  TR::VMAccessCriticalSection resolveStaticMethodRef(fe);
                  ramMethod = jitResolveStaticMethodRef(fe->vmThread(), owningMethod->cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
                  if (ramMethod) createMethod = true;
                  break;
                  }
               case TR_ResolvedMethodType::Special:
                  {
                  if (!((fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
                                    comp->ilGenRequest().details().isMethodHandleThunk() &&
                                    performTransformation(comp, "Setting as unresolved special call cpIndex=%d\n",cpIndex)))
                     {
                     TR::VMAccessCriticalSection resolveSpecialMethodRef(fe);
                     ramMethod = jitResolveSpecialMethodRef(fe->vmThread(), owningMethod->cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
                     }
                  if (ramMethod) createMethod = true;
                  break;
                  }
               default:
                  {
                  break;
                  }
               }
            if (createMethod)
               TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9MethodMirror(
                  methodInfo,
                  (TR_OpaqueMethodBlock *) ramMethod,
                  vTableOffset,
                  owningMethod,
                  fe,
                  trMemory);
            ramMethods[i] = ramMethod;
            vTableOffsets[i] = vTableOffset;
            methodInfos[i] = methodInfo;
            }
         client->write(response, ramMethods, vTableOffsets, methodInfos);
         }
         break;
      case MessageType::ResolvedRelocatableMethod_createResolvedRelocatableJ9Method:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, J9Method *, int32_t, uint32_t>();
         auto mirror = std::get<0>(recv);
         auto j9method = std::get<1>(recv);
         auto cpIndex = std::get<2>(recv);
         auto vTableSlot = std::get<3>(recv);

         bool sameLoaders = false;
         bool sameClass = false;
         bool isRomClassForMethodInSC = false;

         // Create mirror, if possible
         TR_ResolvedJ9JITServerMethodInfo methodInfo;
         TR_ResolvedJ9JITServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) j9method, vTableSlot, mirror, fe, trMemory);

         // Collect AOT stats
         TR_ResolvedJ9Method *resolvedMethod = std::get<0>(methodInfo).remoteMirror;

         isRomClassForMethodInSC = fe->sharedCache()->isPointerInSharedCache(J9_CLASS_FROM_METHOD(j9method)->romClass);

         J9Class *j9clazz = (J9Class *) J9_CLASS_FROM_CP(((J9RAMConstantPoolItem *) J9_CP_FROM_METHOD(((J9Method *)j9method))));
         TR_OpaqueClassBlock *clazzOfInlinedMethod = fe->convertClassPtrToClassOffset(j9clazz);
         TR_OpaqueClassBlock *clazzOfCompiledMethod = fe->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(mirror->ramMethod()));
         sameLoaders = fe->sameClassLoaders(clazzOfInlinedMethod, clazzOfCompiledMethod);
         if (resolvedMethod)
            sameClass = fe->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(mirror->ramMethod())) == fe->convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(j9method));

         client->write(response, methodInfo, isRomClassForMethodInSC, sameLoaders, sameClass);
         }
         break;
      case MessageType::ResolvedRelocatableMethod_storeValidationRecordIfNecessary:
         {
         auto recv = client->getRecvData<J9Method *, J9ConstantPool *, int32_t, bool, J9Class *>();
         auto ramMethod = std::get<0>(recv);
         auto constantPool = std::get<1>(recv);
         auto cpIndex = std::get<2>(recv);
         bool isStatic = std::get<3>(recv);
         J9Class *definingClass = std::get<4>(recv);
         J9Class *clazz = (J9Class *) J9_CLASS_FROM_METHOD(ramMethod);
         if (!definingClass)
            {
            definingClass = (J9Class *) compInfoPT->reloRuntime()->getClassFromCP(fe->vmThread(), constantPool, cpIndex, isStatic);
            }
         UDATA *classChain = NULL;
         if (definingClass)
            classChain = fe->sharedCache()->rememberClass(definingClass);

         client->write(response, clazz, definingClass, classChain);
         }
         break;
      case MessageType::ResolvedRelocatableMethod_getFieldType:
         {
         auto recv = client->getRecvData<int32_t, TR_ResolvedJ9Method *>();
         auto cpIndex = std::get<0>(recv);
         TR_ResolvedJ9Method *method= std::get<1>(recv);
         UDATA ltype = getFieldType((J9ROMConstantPoolItem *)(method->romLiterals()), cpIndex);
         client->write(response, ltype);
         }
         break;
      case MessageType::ResolvedRelocatableMethod_fieldAttributes:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         I_32 cpIndex = std::get<1>(recv);
         bool isStore = std::get<2>(recv);
         bool needAOTValidation = std::get<3>(recv);
         U_32 fieldOffset;
         TR::DataType type;
         bool volatileP, isFinal, isPrivate, unresolvedInCP;
         bool result = method->fieldAttributes(comp, cpIndex, &fieldOffset, &type, &volatileP, &isFinal, &isPrivate, isStore, &unresolvedInCP, needAOTValidation);

         J9ConstantPool *constantPool = (J9ConstantPool *) J9_CP_FROM_METHOD(method->ramMethod());
         TR_OpaqueClassBlock *definingClass = compInfoPT->reloRuntime()->getClassFromCP(fe->vmThread(), constantPool, cpIndex, false);

         TR_J9MethodFieldAttributes attrs(static_cast<uintptr_t>(fieldOffset), type.getDataType(), volatileP, isFinal, isPrivate, unresolvedInCP, result, definingClass);

         client->write(response, attrs);
         }
         break;
      case MessageType::ResolvedRelocatableMethod_staticAttributes:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         int32_t isStore = std::get<2>(recv);
         int32_t needAOTValidation = std::get<3>(recv);
         void *address;
         TR::DataType type;
         bool volatileP, isFinal, isPrivate, unresolvedInCP;
         bool result = method->staticAttributes(comp, cpIndex, &address, &type, &volatileP, &isFinal, &isPrivate, isStore, &unresolvedInCP, needAOTValidation);

         J9ConstantPool *constantPool = (J9ConstantPool *) J9_CP_FROM_METHOD(method->ramMethod());
         TR_OpaqueClassBlock *definingClass = compInfoPT->reloRuntime()->getClassFromCP(fe->vmThread(), constantPool, cpIndex, true);

         TR_J9MethodFieldAttributes attrs(reinterpret_cast<uintptr_t>(address), type.getDataType(), volatileP, isFinal, isPrivate, unresolvedInCP, result, definingClass);

         client->write(response, attrs);
         }
         break;

      case MessageType::CompInfo_isCompiled:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(response, TR::CompilationInfo::isCompiled(method));
         }
         break;
      case MessageType::CompInfo_getJ9MethodExtra:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(response, (uint64_t) TR::CompilationInfo::getJ9MethodExtra(method));
         }
         break;
      case MessageType::CompInfo_getInvocationCount:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(response, TR::CompilationInfo::getInvocationCount(method));
         }
         break;
      case MessageType::CompInfo_setInvocationCount:
         {
         auto recv = client->getRecvData<J9Method *, int32_t>();
         J9Method *method = std::get<0>(recv);
         int32_t count = std::get<1>(recv);
         client->write(response, TR::CompilationInfo::setInvocationCount(method, count));
         }
         break;
      case MessageType::CompInfo_setInvocationCountAtomic:
         {
         auto recv = client->getRecvData<J9Method *, int32_t, int32_t>();
         J9Method *method = std::get<0>(recv);
         int32_t oldCount = std::get<1>(recv);
         int32_t newCount = std::get<2>(recv);
         client->write(response, TR::CompilationInfo::setInvocationCount(method, oldCount, newCount));
         }
         break;
      case MessageType::CompInfo_isJNINative:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(response, TR::CompilationInfo::isJNINative(method));
         }
         break;
      case MessageType::CompInfo_isJSR292:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(response, TR::CompilationInfo::isJSR292(method));
         }
         break;
      case MessageType::CompInfo_getMethodBytecodeSize:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(response, TR::CompilationInfo::getMethodBytecodeSize(method));
         }
         break;
      case MessageType::CompInfo_setJ9MethodExtra:
         {
         auto recv = client->getRecvData<J9Method *, uint64_t>();
         J9Method *method = std::get<0>(recv);
         uint64_t count = std::get<1>(recv);
         TR::CompilationInfo::setJ9MethodExtra(method, count);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::CompInfo_isClassSpecial:
         {
         J9Class *clazz = std::get<0>(client->getRecvData<J9Class *>());
         bool val = clazz->classDepthAndFlags & (J9AccClassReferenceWeak      |
                                                 J9AccClassReferenceSoft      |
                                                 J9AccClassFinalizeNeeded     |
                                                 J9AccClassOwnableSynchronizer);
         client->write(response, val);
         }
         break;
      case MessageType::CompInfo_getJ9MethodStartPC:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(response, TR::CompilationInfo::getJ9MethodStartPC(method));
         }
         break;

      case MessageType::ClassEnv_classFlagsValue:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, TR::Compiler->cls.classFlagsValue(clazz));
         }
         break;
      case MessageType::ClassEnv_classDepthOf:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, TR::Compiler->cls.classDepthOf(clazz));
         }
         break;
      case MessageType::ClassEnv_classInstanceSize:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, TR::Compiler->cls.classInstanceSize(clazz));
         }
         break;
      case MessageType::ClassEnv_superClassesOf:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, TR::Compiler->cls.superClassesOf(clazz));
         }
         break;
      case MessageType::ClassEnv_iTableOf:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, TR::Compiler->cls.iTableOf(clazz));
         }
         break;
      case MessageType::ClassEnv_iTableNext:
         {
         auto clazz = std::get<0>(client->getRecvData<J9ITable *>());
         client->write(response, TR::Compiler->cls.iTableNext(clazz));
         }
         break;
      case MessageType::ClassEnv_iTableRomClass:
         {
         auto clazz = std::get<0>(client->getRecvData<J9ITable *>());
         client->write(response, TR::Compiler->cls.iTableRomClass(clazz));
         }
         break;
      case MessageType::ClassEnv_getITable:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, TR::Compiler->cls.getITable(clazz));
         }
         break;
      case MessageType::ClassEnv_indexedSuperClassOf:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, size_t>();
         auto clazz = std::get<0>(recv);
         size_t index = std::get<1>(recv);
         client->write(response, TR::Compiler->cls.superClassesOf(clazz)[index]);
         }
         break;
      case MessageType::ClassEnv_classHasIllegalStaticFinalFieldModification:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, TR::Compiler->cls.classHasIllegalStaticFinalFieldModification(clazz));
         }
         break;
      case MessageType::ClassEnv_getROMClassRefName:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, uint32_t>();
         auto clazz = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         int classRefLen;
         uint8_t *classRefName = TR::Compiler->cls.getROMClassRefName(comp, clazz, cpIndex, classRefLen);
         client->write(response, std::string((char *) classRefName, classRefLen));
         }
         break;
      case MessageType::SharedCache_getClassChainOffsetInSharedCache:
         {
         auto j9class = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         uintptrj_t classChainOffsetInSharedCache = fe->sharedCache()->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCache(j9class);
         client->write(response, classChainOffsetInSharedCache);
         }
         break;
      case MessageType::SharedCache_rememberClass:
         {
         auto recv = client->getRecvData<J9Class *, bool>();
         auto clazz = std::get<0>(recv);
         bool create = std::get<1>(recv);
         client->write(response, fe->sharedCache()->rememberClass(clazz, create));
         }
         break;
      case MessageType::SharedCache_addHint:
         {
         auto recv = client->getRecvData<J9Method *, TR_SharedCacheHint>();
         auto method = std::get<0>(recv);
         auto hint = std::get<1>(recv);
         if (fe->sharedCache())
            fe->sharedCache()->addHint(method, hint);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::SharedCache_storeSharedData:
         {
         auto recv = client->getRecvData<std::string, J9SharedDataDescriptor, std::string>();
         auto key = std::get<0>(recv);
         auto descriptor = std::get<1>(recv);
         auto dataStr = std::get<2>(recv);
         descriptor.address = (U_8 *) &dataStr[0];
         auto ptr = fe->sharedCache()->storeSharedData(vmThread, (char *) key.data(), &descriptor);
         client->write(response, ptr);
         }
         break;
      case MessageType::runFEMacro_derefUintptrjPtr:
         {
         TR::VMAccessCriticalSection deref(fe);
         compInfoPT->updateLastLocalGCCounter();
         client->write(response, *std::get<0>(client->getRecvData<uintptrj_t*>()));
         }
         break;
      case MessageType::runFEMacro_invokeILGenMacrosInvokeExactAndFixup:
         {
         auto recv = client->getRecvData<uintptrj_t>();
         TR::VMAccessCriticalSection invokeILGenMacrosInvokeExactAndFixup(fe);

         if (compInfoPT->getLastLocalGCCounter() != compInfoPT->getCompilationInfo()->getLocalGCCounter())
            {
            // GC happened, fail compilation
            auto comp = compInfoPT->getCompilation();
            comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted due to GC");
            }

         uintptrj_t methodHandle = std::get<0>(recv);
         uintptrj_t methodDescriptorRef = fe->getReferenceField(fe->getReferenceField(
            methodHandle,
            "type",             "Ljava/lang/invoke/MethodType;"),
            "methodDescriptor", "Ljava/lang/String;");
         intptrj_t methodDescriptorLength = fe->getStringUTF8Length(methodDescriptorRef);
         char *methodDescriptor = (char*)alloca(methodDescriptorLength+1);
         fe->getStringUTF8(methodDescriptorRef, methodDescriptor, methodDescriptorLength+1);
         client->write(response, std::string(methodDescriptor, methodDescriptorLength));
         }
         break;
      case MessageType::runFEMacro_invokeCollectHandleNumArgsToCollect:
         {
         auto recv = client->getRecvData<uintptrj_t*, bool>();
         uintptrj_t methodHandle = *std::get<0>(recv);
         bool getPos = std::get<1>(recv);
         TR::VMAccessCriticalSection invokeCollectHandleNumArgsToCollect(fe);
         int32_t collectArraySize = fe->getInt32Field(methodHandle, "collectArraySize");
         uintptrj_t arguments = fe->getReferenceField(
                  fe->getReferenceField(methodHandle, "type", "Ljava/lang/invoke/MethodType;"),
                  "arguments", "[Ljava/lang/Class;");
         int32_t numArguments = (int32_t)fe->getArrayLengthInElements(arguments);
         int32_t collectionStart = 0;
         if (getPos)
            collectionStart = fe->getInt32Field(methodHandle, "collectPosition");
         client->write(response, collectArraySize, numArguments, collectionStart);
         }
         break;
      case MessageType::runFEMacro_invokeGuardWithTestHandleNumGuardArgs:
         {
         auto recv = client->getRecvData<uintptrj_t*>();
         TR::VMAccessCriticalSection invokeGuardWithTestHandleNumGuardArgs(fe);
         uintptrj_t methodHandle = *std::get<0>(recv);
         uintptrj_t guardArgs = fe->getReferenceField(fe->methodHandle_type(fe->getReferenceField(methodHandle,
            "guard", "Ljava/lang/invoke/MethodHandle;")),
            "arguments", "[Ljava/lang/Class;");
         int32_t numGuardArgs = (int32_t)fe->getArrayLengthInElements(guardArgs);
         client->write(response, numGuardArgs);
         }
         break;
      case MessageType::runFEMacro_invokeExplicitCastHandleConvertArgs:
         {
         auto recv = client->getRecvData<uintptrj_t*>();
         TR::VMAccessCriticalSection invokeExplicitCastHandleConvertArgs(fe);
         uintptrj_t methodHandle = *std::get<0>(recv);
         uintptrj_t methodDescriptorRef = fe->getReferenceField(fe->getReferenceField(fe->getReferenceField(
            methodHandle,
            "next",             "Ljava/lang/invoke/MethodHandle;"),
            "type",             "Ljava/lang/invoke/MethodType;"),
            "methodDescriptor", "Ljava/lang/String;");
         size_t methodDescriptorLength = fe->getStringUTF8Length(methodDescriptorRef);
         char *methodDescriptor = (char*)alloca(methodDescriptorLength+1);
         fe->getStringUTF8(methodDescriptorRef, methodDescriptor, methodDescriptorLength+1);
         client->write(response, std::string(methodDescriptor, methodDescriptorLength));
         }
         break;
      case MessageType::runFEMacro_targetTypeL:
         {
         auto recv = client->getRecvData<uintptrj_t*, int32_t>();
         int32_t argIndex = std::get<1>(recv);
         TR::VMAccessCriticalSection targetTypeL(fe);
         uintptrj_t methodHandle = *std::get<0>(recv);
         uintptrj_t targetArguments = fe->getReferenceField(fe->getReferenceField(fe->getReferenceField(
            methodHandle,
            "next",             "Ljava/lang/invoke/MethodHandle;"),
            "type",             "Ljava/lang/invoke/MethodType;"),
            "arguments",        "[Ljava/lang/Class;");
         TR_OpaqueClassBlock *targetParmClass = (TR_OpaqueClassBlock*)(intptrj_t)fe->getInt64Field(fe->getReferenceElement(targetArguments, argIndex),
                                                                          "vmRef" /* should use fej9->getOffsetOfClassFromJavaLangClassField() */);
         // Load callsite type and check if two types are compatible
         uintptrj_t sourceArguments = fe->getReferenceField(fe->getReferenceField(
            methodHandle,
            "type",             "Ljava/lang/invoke/MethodType;"),
            "arguments",        "[Ljava/lang/Class;");
         TR_OpaqueClassBlock *sourceParmClass = (TR_OpaqueClassBlock*)(intptrj_t)fe->getInt64Field(fe->getReferenceElement(sourceArguments, argIndex),
                                                                          "vmRef" /* should use fej9->getOffsetOfClassFromJavaLangClassField() */);
         client->write(response, sourceParmClass, targetParmClass);
         }
         break;
      case MessageType::runFEMacro_invokeDirectHandleDirectCall:
         {
         auto recv = client->getRecvData<uintptrj_t*, bool, bool>();
         TR::VMAccessCriticalSection invokeDirectHandleDirectCall(fe);
         uintptrj_t methodHandle   = *std::get<0>(recv);
         int64_t vmSlot         = fe->getInt64Field(methodHandle, "vmSlot");
         bool isInterface = std::get<1>(recv);
         bool isVirtual = std::get<2>(recv);
         TR_OpaqueMethodBlock * j9method;

         uintptrj_t jlClass = fe->getReferenceField(methodHandle, "defc", "Ljava/lang/Class;");
         if (isInterface)
             {
             TR_OpaqueClassBlock *clazz = fe->getClassFromJavaLangClass(jlClass);
             j9method = (TR_OpaqueMethodBlock*)&(((J9Class *)clazz)->ramMethods[vmSlot]);
             }
         else if (isVirtual)
            {
            TR_OpaqueMethodBlock **vtable = (TR_OpaqueMethodBlock**)(((uintptrj_t)fe->getClassFromJavaLangClass(jlClass)) + TR::Compiler->vm.getInterpreterVTableOffset());
            int32_t index = (int32_t)((vmSlot - TR::Compiler->vm.getInterpreterVTableOffset()) / sizeof(vtable[0]));
            j9method = vtable[index];
            }
         else
            {
            j9method = (TR_OpaqueMethodBlock*)(intptrj_t)vmSlot;
            }
         client->write(response, j9method, vmSlot);
         }
         break;
      case MessageType::runFEMacro_invokeSpreadHandleArrayArg:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeSpreadHandleArrayArg(fe);
         uintptrj_t arrayClass   = fe->getReferenceField(methodHandle, "arrayClass", "Ljava/lang/Class;");
         J9ArrayClass *arrayJ9Class = (J9ArrayClass*)(intptrj_t)fe->getInt64Field(arrayClass,
                                                                      "vmRef" /* should use fej9->getOffsetOfClassFromJavaLangClassField() */);
         J9Class *leafClass = arrayJ9Class->leafComponentType;
         UDATA arity = arrayJ9Class->arity;
         uint32_t spreadPositionOffset = fe->getInstanceFieldOffset(fe->getObjectClass(methodHandle), "spreadPosition", "I");
         int32_t spreadPosition = -1;
         if (spreadPositionOffset != ~0)
            spreadPosition = fe->getInt32FieldAt(methodHandle, spreadPositionOffset);

         int32_t leafClassNameLength;
         char *leafClassNameChars = fe->getClassNameChars((TR_OpaqueClassBlock*)leafClass, leafClassNameLength);
         bool isPrimitive = TR::Compiler->cls.isPrimitiveClass(comp, (TR_OpaqueClassBlock*)leafClass);
         client->write(response, arrayJ9Class, spreadPosition, arity, leafClass, std::string(leafClassNameChars, leafClassNameLength), isPrimitive);
         }
         break;
      case MessageType::runFEMacro_invokeSpreadHandle:
         {
         auto recv = client->getRecvData<uintptrj_t*, bool>();
         uintptrj_t methodHandle = *std::get<0>(recv);
         bool getSpreadPosition = std::get<1>(recv);

         TR::VMAccessCriticalSection invokeSpreadHandle(fe);
         uintptrj_t arguments = fe->getReferenceField(fe->methodHandle_type(methodHandle), "arguments", "[Ljava/lang/Class;");
         int32_t numArguments = (int32_t)fe->getArrayLengthInElements(arguments);
         uintptrj_t next = fe->getReferenceField(methodHandle, "next", "Ljava/lang/invoke/MethodHandle;");
         uintptrj_t nextArguments = fe->getReferenceField(fe->methodHandle_type(next), "arguments", "[Ljava/lang/Class;");
         int32_t numNextArguments = (int32_t)fe->getArrayLengthInElements(nextArguments);
         int32_t spreadStart = 0;
         // Guard to protect old code
         if (getSpreadPosition)
            spreadStart = fe->getInt32Field(methodHandle, "spreadPosition");
         client->write(response, numArguments, numNextArguments, spreadStart);
         }
         break;
      case MessageType::runFEMacro_invokeInsertHandle:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeInsertHandle(fe);
         int32_t insertionIndex = fe->getInt32Field(methodHandle, "insertionIndex");
         uintptrj_t arguments = fe->getReferenceField(fe->getReferenceField(methodHandle, "type",
                  "Ljava/lang/invoke/MethodType;"), "arguments", "[Ljava/lang/Class;");
         int32_t numArguments = (int32_t)fe->getArrayLengthInElements(arguments);
         uintptrj_t values = fe->getReferenceField(methodHandle, "values", "[Ljava/lang/Object;");
         int32_t numValues = (int32_t)fe->getArrayLengthInElements(values);
         client->write(response, insertionIndex, numArguments, numValues);
         }
         break;
      case MessageType::runFEMacro_invokeFoldHandle:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeFoldHandle(fe);
         uintptrj_t argIndices = fe->getReferenceField(methodHandle, "argumentIndices", "[I");
         int32_t arrayLength = (int32_t)fe->getArrayLengthInElements(argIndices);
         int32_t foldPosition = fe->getInt32Field(methodHandle, "foldPosition");
         std::vector<int32_t> indices(arrayLength);
         int32_t numArgs = 0;
         if (arrayLength != 0)
            {
            // Put indices in the original order, reversal is done on the server
            for (int i = 0; i < arrayLength; i++)
               {
               indices[i] = fe->getInt32Element(argIndices, i);
               }
            }
         else
            {
            uintptrj_t combiner         = fe->getReferenceField(methodHandle, "combiner", "Ljava/lang/invoke/MethodHandle;");
            uintptrj_t combinerArguments = fe->getReferenceField(fe->methodHandle_type(combiner), "arguments", "[Ljava/lang/Class;");
            numArgs = (int32_t)fe->getArrayLengthInElements(combinerArguments);
            }
         client->write(response, indices, foldPosition, numArgs);
         }
         break;
      case MessageType::runFEMacro_invokeFoldHandle2:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeFoldHandle(fe);
         int32_t foldPosition = fe->getInt32Field(methodHandle, "foldPosition");
         client->write(response, foldPosition);
         }
         break;
      case MessageType::runFEMacro_invokeFinallyHandle:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeFinallyHandle(fe);
         uintptrj_t finallyTarget = fe->getReferenceField(methodHandle, "finallyTarget", "Ljava/lang/invoke/MethodHandle;");
         uintptrj_t finallyType = fe->getReferenceField(finallyTarget, "type", "Ljava/lang/invoke/MethodType;");
         uintptrj_t arguments        = fe->getReferenceField(finallyType, "arguments", "[Ljava/lang/Class;");
         int32_t numArgsPassToFinallyTarget = (int32_t)fe->getArrayLengthInElements(arguments);

         uintptrj_t methodDescriptorRef = fe->getReferenceField(finallyType, "methodDescriptor", "Ljava/lang/String;");
         int methodDescriptorLength = fe->getStringUTF8Length(methodDescriptorRef);
         char *methodDescriptor = (char*)alloca(methodDescriptorLength+1);
         fe->getStringUTF8(methodDescriptorRef, methodDescriptor, methodDescriptorLength+1);
         client->write(response, numArgsPassToFinallyTarget, std::string(methodDescriptor, methodDescriptorLength));
         }
         break;
      case MessageType::runFEMacro_invokeFilterArgumentsHandle:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeFilterArgumentsHandle(fe);

         int32_t startPos = (int32_t)fe->getInt32Field(methodHandle, "startPos");

         uintptrj_t filters = fe->getReferenceField(methodHandle, "filters", "[Ljava/lang/invoke/MethodHandle;");
         int32_t numFilters = fe->getArrayLengthInElements(filters);
         std::vector<uintptrj_t> filtersList(numFilters);
         for (int i = 0; i < numFilters; i++)
            {
            filtersList[i] = fe->getReferenceElement(filters, i);
            }

         uintptrj_t methodDescriptorRef = fe->getReferenceField(fe->getReferenceField(fe->getReferenceField(
            methodHandle,
            "next",             "Ljava/lang/invoke/MethodHandle;"),
            "type",             "Ljava/lang/invoke/MethodType;"),
            "methodDescriptor", "Ljava/lang/String;");
         intptrj_t methodDescriptorLength = fe->getStringUTF8Length(methodDescriptorRef);
         char *nextSignature = (char*)alloca(methodDescriptorLength+1);
         fe->getStringUTF8(methodDescriptorRef, nextSignature, methodDescriptorLength+1);
         std::string nextSignatureString(nextSignature, methodDescriptorLength);
         client->write(response, startPos, nextSignatureString, filtersList);
         }
         break;
      case MessageType::runFEMacro_invokeFilterArgumentsHandle2:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeFilderArgumentsHandle(fe);
         uintptrj_t arguments = fe->getReferenceField(fe->methodHandle_type(methodHandle), "arguments", "[Ljava/lang/Class;");
         int32_t numArguments = (int32_t)fe->getArrayLengthInElements(arguments);
         int32_t startPos     = (int32_t)fe->getInt32Field(methodHandle, "startPos");
         uintptrj_t filters = fe->getReferenceField(methodHandle, "filters", "[Ljava/lang/invoke/MethodHandle;");
         int32_t numFilters = fe->getArrayLengthInElements(filters);
         client->write(response, numArguments, startPos, numFilters);
         }
         break;
      case MessageType::runFEMacro_invokeCatchHandle:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeCatchHandle(fe);
         uintptrj_t catchTarget    = fe->getReferenceField(methodHandle, "catchTarget", "Ljava/lang/invoke/MethodHandle;");
         uintptrj_t catchArguments = fe->getReferenceField(fe->getReferenceField(
            catchTarget,
            "type", "Ljava/lang/invoke/MethodType;"),
            "arguments", "[Ljava/lang/Class;");
         int32_t numCatchArguments = (int32_t)fe->getArrayLengthInElements(catchArguments);
         client->write(response, numCatchArguments);
         }
         break;
      case MessageType::runFEMacro_invokeArgumentMoverHandlePermuteArgs:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeArgumentMoverHandlePermuteArgs(fe);
         uintptrj_t methodDescriptorRef = fe->getReferenceField(fe->getReferenceField(fe->getReferenceField(
            methodHandle,
            "next",             "Ljava/lang/invoke/MethodHandle;"),
            "type",             "Ljava/lang/invoke/MethodType;"),
            "methodDescriptor", "Ljava/lang/String;");
         intptrj_t methodDescriptorLength = fe->getStringUTF8Length(methodDescriptorRef);
         char *nextHandleSignature = (char*)alloca(methodDescriptorLength+1);
         fe->getStringUTF8(methodDescriptorRef, nextHandleSignature, methodDescriptorLength+1);
         client->write(response, std::string(nextHandleSignature, methodDescriptorLength));
         }
         break;
      case MessageType::runFEMacro_invokePermuteHandlePermuteArgs:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokePermuteHandlePermuteArgs(fe);
         uintptrj_t permuteArray = fe->getReferenceField(methodHandle, "permute", "[I");
         int32_t permuteLength = fe->getArrayLengthInElements(permuteArray);
         std::vector<int32_t> argIndices(permuteLength);
         for (int32_t i=0; i < permuteLength; i++)
            {
            argIndices[i] = fe->getInt32Element(permuteArray, i);
            }
         client->write(response, permuteLength, argIndices);
         }
         break;
      case MessageType::runFEMacro_invokeILGenMacros:
         {
         if (compInfoPT->getLastLocalGCCounter() != compInfoPT->getCompilationInfo()->getLocalGCCounter())
            {
            // GC happened, fail compilation
            auto comp = compInfoPT->getCompilation();
            comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted due to GC");
            }

         uintptrj_t methodHandle = std::get<0>(client->getRecvData<uintptrj_t>());
         TR::VMAccessCriticalSection invokeILGenMacros(fe);
         uintptrj_t arguments = fe->getReferenceField(fe->getReferenceField(
            methodHandle,
            "type", "Ljava/lang/invoke/MethodType;"),
            "arguments", "[Ljava/lang/Class;");
         int32_t parameterCount = (int32_t)fe->getArrayLengthInElements(arguments);
         client->write(response, parameterCount);
         }
         break;

      case MessageType::CHTable_getAllClassInfo:
         {
         client->getRecvData<JITServer::Void>();
         auto table = (JITClientPersistentCHTable*)comp->getPersistentInfo()->getPersistentCHTable();
         TR_OpaqueClassBlock *rootClass = fe->TR_J9VM::getSystemClassFromClassName("java/lang/Object", 16);
         TR_PersistentClassInfo* result = table->findClassInfoAfterLocking(
                                                   rootClass,
                                                   comp,
                                                   true);
         std::string encoded = FlatPersistentClassInfo::serializeHierarchy(result);
         client->write(response, encoded);
         }
         break;
      case MessageType::CHTable_getClassInfoUpdates:
         {
         client->getRecvData<JITServer::Void>();
         auto table = (JITClientPersistentCHTable*)comp->getPersistentInfo()->getPersistentCHTable();
         auto encoded = table->serializeUpdates();
         client->write(response, encoded.first, encoded.second);
         }
         break;
      case MessageType::CHTable_clearReservable:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock*>());
         auto table = (JITClientPersistentCHTable*)comp->getPersistentInfo()->getPersistentCHTable();
         auto info = table->findClassInfoAfterLocking(clazz, comp);
         info->setReservable(false);
         }
         break;
      case MessageType::IProfiler_searchForMethodSample:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock*>();
         auto method = std::get<0>(recv);
         JITClientIProfiler *iProfiler = (JITClientIProfiler *) fe->getIProfiler();
         client->write(response, iProfiler->serializeIProfilerMethodEntry(method));
         }
         break;
      case MessageType::IProfiler_profilingSample:
         {
         handler_IProfiler_profilingSample(client, fe, comp);
         }
         break;
      case MessageType::IProfiler_getMaxCallCount:
         {
         TR_IProfiler *iProfiler = fe->getIProfiler();
         client->write(response, iProfiler->getMaxCallCount());
         }
         break;
      case MessageType::IProfiler_setCallCount:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock*, int32_t, int32_t>();
         auto method = std::get<0>(recv);
         auto bcIndex = std::get<1>(recv);
         auto count = std::get<2>(recv);
         TR_IProfiler * iProfiler = fe->getIProfiler();
         iProfiler->setCallCount(method, bcIndex, count, comp);
         client->write(response, JITServer::Void());
         }
         break;
      case MessageType::Recompilation_getExistingMethodInfo:
         {
         auto recomp = comp->getRecompilationInfo();
         auto methodInfo = recomp->getMethodInfo();
         client->write(response, *methodInfo);
         }
         break;
      case MessageType::Recompilation_getJittedBodyInfoFromPC:
         {
         void *startPC = std::get<0>(client->getRecvData<void *>());
         auto bodyInfo = J9::Recompilation::getJittedBodyInfoFromPC(startPC);
         if (!bodyInfo)
            client->write(response, std::string(), std::string());
         else
            client->write(response, std::string((char*) bodyInfo, sizeof(TR_PersistentJittedBodyInfo)),
                          std::string((char*) bodyInfo->getMethodInfo(), sizeof(TR_PersistentMethodInfo)));
         }
         break;
      default:
         // It is vital that this remains a hard error during dev!
         TR_ASSERT(false, "JITServer: handleServerMessage received an unknown message type: %d\n", response);
      }

   releaseVMAccess(vmThread);
   return done;
   }

static TR_MethodMetaData *
remoteCompilationEnd(
   J9VMThread * vmThread,
   TR::Compilation *comp,
   TR_ResolvedMethod * compilee,
   J9Method * method,
   TR::CompilationInfoPerThreadBase *compInfoPT,
   const std::string& codeCacheStr,
   const std::string& dataCacheStr)
   {
   TR_MethodMetaData *relocatedMetaData = NULL;
   TR_J9VM *fe = comp->fej9vm();
   TR_MethodToBeCompiled *entry = compInfoPT->getMethodBeingCompiled();
   J9JITConfig *jitConfig = compInfoPT->getJitConfig();
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   const J9JITDataCacheHeader *storedCompiledMethod = NULL;
   PORT_ACCESS_FROM_JAVAVM(jitConfig->javaVM);

   if (!fe->isAOT_DEPRECATED_DO_NOT_USE()) // For relocating received JIT compilations
      {
      compInfoPT->reloRuntime()->setReloStartTime(compInfoPT->getTimeWhenCompStarted());

      relocatedMetaData = compInfoPT->reloRuntime()->prepareRelocateAOTCodeAndData(
         vmThread,
         fe,
         comp->cg()->getCodeCache(),
         (J9JITDataCacheHeader *)&dataCacheStr[0],
         method,
         false,
         comp->getOptions(),
         comp,
         compilee,
         (uint8_t *)&codeCacheStr[0]);

      if (!relocatedMetaData)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                           "JITServer Relocation failure: %d",
                                           compInfoPT->reloRuntime()->returnCode());
            }
         // Relocation failed, fail compilation
         entry->_compErrCode = compInfoPT->reloRuntime()->returnCode();
         comp->failCompilation<J9::AOTRelocationFailed>("Failed to relocate");
         }
      }
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   else // For relocating received AOT compilations
      {
      TR_ASSERT(entry->_useAotCompilation, "entry must be an AOT compilation");
      TR_ASSERT(entry->isRemoteCompReq(), "entry must be a remote compilation");
      J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
      TR::CompilationInfo::storeAOTInSharedCache(
         vmThread,
         romMethod,
         (U_8 *)(&dataCacheStr[0]),
         dataCacheStr.size(),
         (U_8 *)(&codeCacheStr[0]),
         codeCacheStr.size(),
         comp,
         jitConfig,
         entry
         );

#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)

      TR_Debug *debug = TR::Options::getDebug();
      bool canRelocateMethod = TR::CompilationInfo::canRelocateMethod(comp);

      if (canRelocateMethod)
         {
         TR_ASSERT_FATAL(comp->cg(), "CodeGenerator must be allocated");
         int32_t returnCode = 0;

         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_JITServer,
               "Applying JITServer remote AOT relocations to newly AOT compiled body for %s @ %s",
               comp->signature(),
               comp->getHotnessName()
               );
            }
         try
            {
            // Need to get a non-shared cache VM to relocate
            TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, vmThread);
            relocatedMetaData = entry->_compInfoPT->reloRuntime()->prepareRelocateAOTCodeAndData(
               vmThread,
               fe,
               comp->cg()->getCodeCache(),
               (J9JITDataCacheHeader *)&dataCacheStr[0],
               method,
               false,
               comp->getOptions(),
               comp,
               compilee,
               (uint8_t *)&codeCacheStr[0]
               );
            returnCode = entry->_compInfoPT->reloRuntime()->returnCode();
            }
         catch (std::exception &e)
            {
            // Relocation Failure
            returnCode = compilationAotRelocationInterrupted;
            }

         if (relocatedMetaData)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "JITClient successfully relocated metadata for %s", comp->signature());
               }

            if (J9_EVENT_IS_HOOKED(jitConfig->javaVM->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
               {
               TR::CompilationInfo::addJ9HookVMDynamicCodeLoadForAOT(vmThread, method, jitConfig, relocatedMetaData);
               }
            }
         else
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                                              "JITServer Relocation failure: %d",
                                              compInfoPT->reloRuntime()->returnCode());
               }
            // Relocation failed, fail compilation
            // attempt to recompile in non-AOT mode
            entry->_doNotUseAotCodeFromSharedCache = true;
            entry->_compErrCode = returnCode;

            if (entry->_compilationAttemptsLeft > 0)
               {
               entry->_tryCompilingAgain = true;
               }
            comp->failCompilation<J9::AOTRelocationFailed>("Failed to relocate");
            }
         }
      else
         {
         // AOT compilations can fail on purpose because we want to load
         // the AOT body later on. This case is signalled by having a successful compilation
         // but canRelocateMethod == false
         // We still need metadata, because metaData->startPC != 0 indicates that compilation
         // didn't actually fail.
         J9JITDataCacheHeader *dataCacheHeader = (J9JITDataCacheHeader *) &dataCacheStr[0];
         J9JITExceptionTable *metaData = compInfoPT->reloRuntime()->copyMethodMetaData(dataCacheHeader);
         // Temporarily store meta data pointer.
         // This is not exactly how it's used in baseline, but in remote AOT we do not use
         // AOT method data start for anything else, so should be fine.
         comp->setAotMethodDataStart(metaData);
         TR::CompilationInfo::replenishInvocationCount(method, comp);
         return metaData;
         }
#endif /* J9VM_INTERP_AOT_RUNTIME_SUPPORT */
      }
#endif // defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   return relocatedMetaData;
   }

TR_MethodMetaData *
remoteCompile(
   J9VMThread * vmThread,
   TR::Compilation * compiler,
   TR_ResolvedMethod * compilee,
   J9Method * method,
   TR::IlGeneratorMethodDetails &details,
   TR::CompilationInfoPerThreadBase *compInfoPT
   )
   {
   TR_ASSERT(vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS, "Client must work with VM access");
   // JITServer: if TR_EnableJITServerPerCompConn is set, then each remote compilation establishes a new connection
   // instead of re-using the connection shared within a compilation thread
   static bool enableJITServerPerCompConn = feGetEnv("TR_EnableJITServerPerCompConn") ? true : false;

   // Prepare the parameters for the compilation request
   J9Class *clazz = J9_CLASS_FROM_METHOD(method);
   J9ROMClass *romClass = clazz->romClass;
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
   uint32_t romMethodOffset = uint32_t((uint8_t*) romMethod - (uint8_t*) romClass);
   std::string detailsStr = std::string((char*) &details, sizeof(TR::IlGeneratorMethodDetails));
   TR::CompilationInfo *compInfo = compInfoPT->getCompilationInfo();
   bool useAotCompilation = compInfoPT->getMethodBeingCompiled()->_useAotCompilation;

   JITServer::ClientStream *client = enableJITServerPerCompConn ? NULL : compInfoPT->getClientStream();
   if (!client)
      {
      try
         {
         if (JITServerHelpers::isServerAvailable())
            {
            client = new (PERSISTENT_NEW) JITServer::ClientStream(compInfo->getPersistentInfo());
            if (!enableJITServerPerCompConn)
               compInfoPT->setClientStream(client);
            }
         else if (JITServerHelpers::shouldRetryConnection(OMRPORT_FROM_J9PORT(compInfoPT->getJitConfig()->javaVM->portLibrary)))
            {
            client = new (PERSISTENT_NEW) JITServer::ClientStream(compInfo->getPersistentInfo());
            if (!enableJITServerPerCompConn)
               compInfoPT->setClientStream(client);
            JITServerHelpers::postStreamConnectionSuccess();
            }
         else
            {
            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompilationDispatch))
               TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,
                  "Server is not available. Retry with local compilation for %s @ %s", compiler->signature(), compiler->getHotnessName());
            compiler->failCompilation<JITServer::StreamFailure>("Server is not available, should retry with local compilation.");
            }
         }
      catch (const JITServer::StreamFailure &e)
         {
         JITServerHelpers::postStreamFailure(OMRPORT_FROM_J9PORT(compInfoPT->getJitConfig()->javaVM->portLibrary));
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompilationDispatch))
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,
               "JITServer::StreamFailure: %s for %s @ %s", e.what(), compiler->signature(), compiler->getHotnessName());
         compiler->failCompilation<JITServer::StreamFailure>(e.what());
         }
      catch (const std::bad_alloc &e)
         {
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompilationDispatch))
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,
               "std::bad_alloc: %s for %s @ %s", e.what(), compiler->signature(), compiler->getHotnessName());
         compiler->failCompilation<std::bad_alloc>(e.what());
         }
      }

   if (compiler->getOption(TR_UseSymbolValidationManager))
       {
       // We do not want client to validate anything during compilation, because
       // validations are done on the server. Creating heuristic region makes SVM assume that everything is valdiated.
       compiler->enterHeuristicRegion();
       }

   if (compiler->isOptServer())
      compiler->setOption(TR_Server);
   auto classInfoTuple = JITServerHelpers::packRemoteROMClassInfo(clazz, compiler->fej9vm()->vmThread(), compiler->trMemory());
   std::string optionsStr = TR::Options::packOptions(compiler->getOptions());
   std::string recompMethodInfoStr = compiler->isRecompilationEnabled() ? std::string((char *) compiler->getRecompilationInfo()->getMethodInfo(), sizeof(TR_PersistentMethodInfo)) : std::string();

   compInfo->getSequencingMonitor()->enter();
   // Collect the list of unloaded classes
   std::vector<TR_OpaqueClassBlock*> unloadedClasses(compInfo->getUnloadedClassesTempList()->begin(), compInfo->getUnloadedClassesTempList()->end());
   compInfo->getUnloadedClassesTempList()->clear();
   // Collect and encode the CHTable updates; this will acquire CHTable mutex
   //auto table = (JITClientPersistentCHTable*)compInfo->getPersistentInfo()->getPersistentCHTable();
   //std::pair<std::string, std::string> chtableUpdates = table->serializeUpdates();
   // Update the sequence number for these updates
   uint32_t seqNo = compInfo->getCompReqSeqNo();
   compInfo->incCompReqSeqNo();
   compInfo->getSequencingMonitor()->exit();

   uint32_t statusCode = compilationFailure;
   std::string codeCacheStr;
   std::string dataCacheStr;
   CHTableCommitData chTableData;
   std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended;
   std::string logFileStr;
   std::string svmSymbolToIdStr;
   std::vector<TR_ResolvedJ9Method*> resolvedMirrorMethodsPersistIPInfo;
   TR_OptimizationPlan modifiedOptPlan;
   try
      {
      // Release VM access just before sending the compilation request
      // message just in case we block in the write operation
      releaseVMAccess(vmThread);

      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
            "Client sending compReq seqNo=%u to server for method %s @ %s.",
            seqNo, compiler->signature(), compiler->getHotnessName());
         }
      client->buildCompileRequest(TR::comp()->getPersistentInfo()->getClientUID(), romMethodOffset,
                                 method, clazz, *compInfoPT->getMethodBeingCompiled()->_optimizationPlan, detailsStr, details.getType(), unloadedClasses,
                                 classInfoTuple, optionsStr, recompMethodInfoStr, seqNo, useAotCompilation);

      JITServer::MessageType response;
      while(!handleServerMessage(client, compiler->fej9vm(), response));

      // Re-acquire VM access
      // handleServerMessage will always acquire VM access after read() and release VM access at the end
      // Therefore we need to re-acquire VM access after we get out of handleServerMessage
      acquireVMAccessNoSuspend(vmThread);

      if (JITServer::MessageType::compilationCode == response)
         {
         auto recv = client->getRecvData<std::string, std::string, CHTableCommitData, std::vector<TR_OpaqueClassBlock*>,
                                      std::string, std::string, std::vector<TR_ResolvedJ9Method*>, TR_OptimizationPlan>();
         statusCode = compilationOK;
         codeCacheStr = std::get<0>(recv);
         dataCacheStr = std::get<1>(recv);
         chTableData = std::get<2>(recv);
         classesThatShouldNotBeNewlyExtended = std::get<3>(recv);
         logFileStr = std::get<4>(recv);
         svmSymbolToIdStr = std::get<5>(recv);
         resolvedMirrorMethodsPersistIPInfo = std::get<6>(recv);
         modifiedOptPlan = std::get<7>(recv);
         }
      else
         {
         TR_ASSERT(JITServer::MessageType::compilationFailure == response, "Received %u but expect JITServer::MessageType::compilationFailure message type", response);
         auto recv = client->getRecvData<uint32_t>();
         statusCode = std::get<0>(recv);
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "remoteCompile: JITServer::MessageType::compilationFailure statusCode %u\n", statusCode);
         }

      if (statusCode >= compilationMaxError)
         throw JITServer::StreamTypeMismatch("Did not receive a valid TR_CompilationErrorCode as the final message on the stream.");
      else if (statusCode == compilationStreamVersionIncompatible)
         throw JITServer::StreamVersionIncompatible();
      else if (statusCode == compilationStreamMessageTypeMismatch)
         throw JITServer::StreamMessageTypeMismatch();
      client->setVersionCheckStatus();
      }
   catch (const JITServer::StreamFailure &e)
      {
      JITServerHelpers::postStreamFailure(OMRPORT_FROM_J9PORT(compInfoPT->getJitConfig()->javaVM->portLibrary));

      client->~ClientStream();
      TR_Memory::jitPersistentFree(client);
      compInfoPT->setClientStream(NULL);

      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompilationDispatch))
          TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,
            "JITServer::StreamFailure: %s for %s @ %s", e.what(), compiler->signature(), compiler->getHotnessName());
      compiler->failCompilation<JITServer::StreamFailure>(e.what());
      }
   catch (const JITServer::StreamVersionIncompatible &e)
      {
      client->~ClientStream();
      TR_Memory::jitPersistentFree(client);
      compInfoPT->setClientStream(NULL);
      JITServer::ClientStream::incrementIncompatibilityCount(OMRPORT_FROM_J9PORT(compInfoPT->getJitConfig()->javaVM->portLibrary));

      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompilationDispatch))
          TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,
            "JITServer::StreamVersionIncompatible: %s for %s @ %s", e.what(), compiler->signature(), compiler->getHotnessName());
      compiler->failCompilation<JITServer::StreamVersionIncompatible>(e.what());
      }
   catch (const JITServer::StreamMessageTypeMismatch &e)
      {
      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompilationDispatch))
         TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,
            "JITServer::StreamMessageTypeMismatch: %s for %s @ %s", e.what(), compiler->signature(), compiler->getHotnessName());
      compiler->failCompilation<JITServer::StreamMessageTypeMismatch>(e.what());
      }

   TR_MethodMetaData *metaData = NULL;
   if (statusCode == compilationOK || statusCode == compilationNotNeeded)
      {
      try
         {
         TR_IProfiler *iProfiler = compiler->fej9vm()->getIProfiler();
         for (TR_ResolvedJ9Method* mirror : resolvedMirrorMethodsPersistIPInfo)
            iProfiler->persistIprofileInfo(NULL, mirror, compiler);

         if (compiler->getOption(TR_UseSymbolValidationManager))
            {
            // Compilation is done, now we need client to validate all of the records accumulated by the server,
            // so need to exit heuristic region.
            compiler->exitHeuristicRegion();
            // Populate symbol to id map
            compiler->getSymbolValidationManager()->deserializeSymbolToIDMap(svmSymbolToIdStr);
            }

         TR_ASSERT(codeCacheStr.size(), "must have code cache");
         TR_ASSERT(dataCacheStr.size(), "must have data cache");

         compInfoPT->getMethodBeingCompiled()->_optimizationPlan->clone(&modifiedOptPlan);

         // Relocate the received compiled code
         metaData = remoteCompilationEnd(vmThread, compiler, compilee, method, compInfoPT, codeCacheStr, dataCacheStr);

         if (!compiler->getOption(TR_DisableCHOpts) && !useAotCompilation)
            {
            TR::ClassTableCriticalSection commit(compiler->fe());

            // Intersect classesThatShouldNotBeNewlyExtended with newlyExtendedClasses
            // and abort on overlap
            auto newlyExtendedClasses = compInfo->getNewlyExtendedClasses();
            for (TR_OpaqueClassBlock* clazz : classesThatShouldNotBeNewlyExtended)
               {
               auto it = newlyExtendedClasses->find(clazz);
               if (it != newlyExtendedClasses->end() && (it->second & (1 << TR::compInfoPT->getCompThreadId())))
                  {
                  if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompileEnd, TR_VerbosePerformance, TR_VerboseCompFailure))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "Class that should not be newly extended was extended when compiling %s", compiler->signature());
                  compiler->failCompilation<J9::CHTableCommitFailure>("Class that should not be newly extended was extended");
                  }
               }

            if (!JITClientCHTableCommit(compiler, metaData, chTableData))
               {
#ifdef COLLECT_CHTABLE_STATS
               JITClientPersistentCHTable *table = (JITClientPersistentCHTable*) TR::comp()->getPersistentInfo()->getPersistentCHTable();
               table->_numCommitFailures += 1;
#endif
               if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompileEnd, TR_VerbosePerformance, TR_VerboseCompFailure))
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "Failure while committing chtable for %s", compiler->signature());
                  }
               compiler->failCompilation<J9::CHTableCommitFailure>("CHTable commit failure");
               }
            }


         TR_ASSERT(!metaData || !metaData->startColdPC, "coldPC should be null");
         // As a debugging feature, a local compilation can be performed immediately after a remote compilation.
         // Each of them has logs with the same compilationSequenceNumber
         int compilationSequenceNumber = compiler->getOptions()->writeLogFileFromServer(logFileStr);
         if (TR::Options::getCmdLineOptions()->getOption(TR_JITServerFollowRemoteCompileWithLocalCompile) && compilationSequenceNumber)
            {
            intptr_t rtn = 0;
            compiler->getOptions()->setLogFileForClientOptions(compilationSequenceNumber);
            if ((rtn = compiler->compile()) != COMPILATION_SUCCEEDED)
               {
               TR_ASSERT(false, "Compiler returned non zero return code %d\n", rtn);
               compiler->failCompilation<TR::CompilationException>("Compilation Failure");
               }
            compiler->getOptions()->closeLogFileForClientOptions();
            }

         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_JITServer,
               "Client successfully loaded method %s @ %s following compilation request. [metaData=%p, startPC=%p]",
               compiler->signature(),
               compiler->getHotnessName(),
               metaData, (metaData) ? (void *)metaData->startPC : NULL
               );
            }
         }
      catch (const std::exception &e)
         {
         // Log for JITClient mode and re-throw
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_JITServer,
               "Client failed to load method %s @ %s following compilation request.",
               compiler->signature(),
               compiler->getHotnessName()
               );
            }
         throw;
         }
      }
   else
      {
      compInfoPT->getMethodBeingCompiled()->_compErrCode = statusCode;

      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompilationDispatch))
          TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,
            "JITServer::ServerCompilationFailure: errCode %u for %s @ %s", statusCode, compiler->signature(), compiler->getHotnessName());
      compiler->failCompilation<JITServer::ServerCompilationFailure>("JITServer compilation failed.");
      }

   if (enableJITServerPerCompConn && client)
      {
      client->~ClientStream();
      TR_Memory::jitPersistentFree(client);
      }
   return metaData;
   }
