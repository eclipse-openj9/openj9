/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#include "control/JITaaSCompilationThread.hpp"
#include "vmaccess.h"
#include "runtime/J9VMAccess.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "control/CompilationRuntime.hpp"
#include "compile/Compilation.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "codegen/CodeGenerator.hpp"
#include "runtime/CodeCache.hpp"
#include "env/JITaaSPersistentCHTable.hpp"
#include "env/JITaaSCHTable.hpp"
#include "env/ClassTableCriticalSection.hpp"   // for ClassTableCriticalSection
#include "exceptions/RuntimeFailure.hpp"       // for CHTableCommitFailure
#include "runtime/IProfiler.hpp"               // for TR_IProfiler
#include "runtime/JITaaSIProfiler.hpp"           // for TR_ContiguousIPMethodHashTableEntry
#include "j9port.h" // for j9time_current_time_millis
#include "env/j9methodServer.hpp"
#include "exceptions/AOTFailure.hpp" // for AOTFailure
#include "env/TRMemory.hpp" // for jitPersistentAlloc and jitPersistentFree
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "runtime/RelocationTarget.hpp"
#include "jitprotos.h"

#if defined(J9VM_OPT_SHARED_CLASSES)
#include "j9jitnls.h"
#endif

extern TR::Monitor *assumptionTableMutex;

uint32_t serverMsgTypeCount[JITServer::MessageType_ARRAYSIZE] = {};

uint64_t JITaaSHelpers::_waitTime = 1000;
bool JITaaSHelpers::_serverAvailable = true;
uint64_t JITaaSHelpers::_nextConnectionRetryTime = 0;
TR::Monitor * JITaaSHelpers::_clientStreamMonitor = NULL;

// TODO: this method is copied from runtime/jit_vm/ctsupport.c,
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

size_t methodStringsLength(J9ROMMethod *method)
   {
   J9UTF8 *name = J9ROMMETHOD_NAME(method);
   J9UTF8 *sig = J9ROMMETHOD_SIGNATURE(method);
   return name->length + sig->length + 2 * sizeof(U_16);
   }

// Packs a ROMClass into a std::string to be transfered to the server.
// The name and signature of all methods are appended to the end of the cloned class body and the
// self referential pointers to them are updated to deal with possible interning. The method names
// and signature are needed on the server but may be interned globally on the client.
std::string packROMClass(J9ROMClass *origRomClass, TR_Memory *trMemory)
   {
   //JITaaS TODO: Add comments
   J9UTF8 *className = J9ROMCLASS_CLASSNAME(origRomClass);
   size_t classNameSize = className->length + sizeof(U_16);

   J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(origRomClass);
   size_t totalSize = origRomClass->romSize + classNameSize;
   for (size_t i = 0; i < origRomClass->romMethodCount; ++i)
      {
      totalSize += methodStringsLength(romMethod);
      romMethod = nextROMMethod(romMethod);
      }

   J9ROMClass *romClass = (J9ROMClass *)trMemory->allocateHeapMemory(totalSize);
   if (!romClass)
      throw std::bad_alloc();
   memcpy(romClass, origRomClass, origRomClass->romSize);

   uint8_t *curPos = ((uint8_t *)romClass) + romClass->romSize;

   memcpy(curPos, className, classNameSize);
   NNSRP_SET(romClass->className, curPos);
   curPos += classNameSize;

   romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   J9ROMMethod *origRomMethod = J9ROMCLASS_ROMMETHODS(origRomClass);
   for (size_t i = 0; i < romClass->romMethodCount; ++i)
      {
      J9UTF8 *field = J9ROMMETHOD_NAME(origRomMethod);
      size_t fieldLen = field->length + sizeof(U_16);
      memcpy(curPos, field, fieldLen);
      NNSRP_SET(romMethod->nameAndSignature.name, curPos);
      curPos += fieldLen;

      field = J9ROMMETHOD_SIGNATURE(origRomMethod);
      fieldLen = field->length + sizeof(U_16);
      memcpy(curPos, field, fieldLen);
      NNSRP_SET(romMethod->nameAndSignature.signature, curPos);
      curPos += fieldLen;

      romMethod = nextROMMethod(romMethod);
      origRomMethod = nextROMMethod(origRomMethod);
      }

   std::string romClassStr((char *) romClass, totalSize);
   trMemory->freeMemory(romClass, heapAlloc);
   return romClassStr;
   }

void handler_IProfiler_profilingSample(JITServer::ClientStream *client, TR_J9VM *fe, TR::Compilation *comp)
   {
   auto recv = client->getRecvData<TR_OpaqueMethodBlock*, uint32_t, uintptrj_t>();
   auto method = std::get<0>(recv);
   auto bcIndex = std::get<1>(recv);
   auto data = std::get<2>(recv); // data==1 means 'send info for 1 bytecode'; data==0 means 'send info for entire method if possible'

   TR_JITaaSClientIProfiler *iProfiler = (TR_JITaaSClientIProfiler *)fe->getIProfiler();

   bool isCompiled = TR::CompilationInfo::isCompiled((J9Method*)method);
   bool isInProgress = comp->getMethodBeingCompiled()->getPersistentIdentifier() == method;
   bool abort = false;
   // used to tell the server if a profiled entry should be stored in persistent or heap memory
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
         uint32_t canPersist = entry->canBeSerialized(comp->getPersistentInfo()); // this may lock the entry
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
         // unlock the entry
         if (auto callGraphEntry = entry->asIPBCDataCallGraph())
            if (canPersist != IPBC_ENTRY_PERSIST_LOCK && callGraphEntry->isLocked())
               callGraphEntry->releaseEntry();
         }
      else // no valid info for specified bytecode index
         {
         client->write(JITServer::MessageType::IProfiler_profilingSample, std::string(), false, usePersistentCache);
         }
      }
   }

static bool handleServerMessage(JITServer::ClientStream *client, TR_J9VM *fe, JITServer::MessageType &response)
   {
   using JITServer::MessageType;
   TR::CompilationInfoPerThread *compInfoPT = fe->_compInfoPT;
   J9VMThread *vmThread = compInfoPT->getCompilationThread();
   TR_Memory  *trMemory = compInfoPT->getCompilation()->trMemory();
   TR::Compilation *comp = compInfoPT->getCompilation();

   // release VM access before doing a potentially long wait
   TR_ASSERT(vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS, "Must have VM access");
   TR_ASSERT(TR::MonitorTable::get()->getClassUnloadMonitorHoldCount(compInfoPT->getCompThreadId()) == 0, "Must not hold classUnloadMonitor");
   TR::MonitorTable *table = TR::MonitorTable::get();
   TR_ASSERT(table && table->isThreadInSafeMonitorState(vmThread), "Must not hold any monitors when waiting for server");

   releaseVMAccess(vmThread);

   response = client->read();

   // re-acquire VM access and check for possible class unloading
   acquireVMAccessNoSuspend(vmThread);

   // Update statistics for server message type
   serverMsgTypeCount[response] += 1;

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

         // create mirrors and put methodInfo for each method in a vector to be sent to the server
         std::vector<TR_ResolvedJ9JITaaSServerMethodInfo> methodsInfo;
         methodsInfo.reserve(numMethods);
         for (int i = 0; i < numMethods; ++i)
            {
            TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodMirror(methodInfo, (TR_OpaqueMethodBlock *) &(methods[i]), 0, 0, fe, trMemory);
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
         vmInfo._cacheStartAddress = fe->sharedCache() ? fe->sharedCache()->getCacheStartAddress() : 0;
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
         client->write(response, vmInfo);
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
      case MessageType::VM_getConstantPoolFromMethod:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(response, fe->getConstantPoolFromMethod(method));
         }
         break;
      case MessageType::VM_getConstantPoolFromClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(response, fe->getConstantPoolFromClass(clazz));
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
            uint8_t *thunkStart = TR_JITaaSRelocationRuntime::copyDataToCodeCache(serializedThunk.data(), serializedThunk.size(), fe);
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
         uint8_t *thunkStart = TR_JITaaSRelocationRuntime::copyDataToCodeCache(serializedThunk.data(), serializedThunk.size(), fe);
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
      case MessageType::VM_getArrayLengthOfStaticAddress:
         {
         auto recv = client->getRecvData<void*>();
         void *ptr = std::get<0>(recv);
         int32_t length;
         bool ok = fe->getArrayLengthOfStaticAddress(ptr, length);
         client->write(response, ok, length);
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
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
         // if in AOT mode, create a relocatable method mirror
         TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodMirror(methodInfo, method, vTableSlot, owningMethod, fe, trMemory);
         
         client->write(response, methodInfo);
         }
         break;
      case MessageType::ResolvedMethod_getRemoteROMClassAndMethods:
         {
         J9Class *clazz = std::get<0>(client->getRecvData<J9Class *>());
         client->write(response, JITaaSHelpers::packRemoteROMClassInfo(clazz, fe->vmThread(), trMemory));
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
        
         // create a mirror right away
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
         if (ramMethod)
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, method, fe, trMemory);

         client->write(response, ramMethod, methodInfo, unresolvedInCP);
         }
         break;
      case MessageType::ResolvedMethod_getResolvedSpecialMethodAndMirror:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, I_32>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         J9Method *ramMethod = NULL;
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo;
         if (!((fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
               !comp->ilGenRequest().details().isMethodHandleThunk() &&
               performTransformation(comp, "Setting as unresolved special call cpIndex=%d\n",cpIndex)))
            {
            TR::VMAccessCriticalSection resolveSpecialMethodRef(fe);
            ramMethod = jitResolveSpecialMethodRef(fe->vmThread(), method->cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);

            if (ramMethod)
               TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, method, fe, trMemory);
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
         // 1. resolve method
         auto recv = client->getRecvData<TR_ResolvedMethod *, J9RAMConstantPoolItem *, I_32>();
         auto *owningMethod = std::get<0>(recv);
         auto literals = std::get<1>(recv);
         J9ConstantPool *cp = (J9ConstantPool*)literals;
         I_32 cpIndex = std::get<2>(recv);
         bool resolvedInCP = false;
         
         // only call the resolve if unresolved
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
            // go fishing for the J9Method...
            uint32_t classIndex = ((J9ROMMethodRef *) cp->romConstantPool)[cpIndex].classRefCPIndex;
            J9Class * classObject = (((J9RAMClassRef*) literals)[classIndex]).value;
            ramMethod = *(J9Method **)((char *)classObject + vTableIndex);
            resolvedInCP = true;
            }

         if(TR_ResolvedJ9Method::isInvokePrivateVTableOffset(vTableIndex))
            ramMethod = (((J9RAMVirtualMethodRef*) literals)[cpIndex]).method;

         // 2. mirror the resolved method on the client
         if (vTableIndex)
            {
            TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *) ramMethod;
           
            TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, (uint32_t) vTableIndex, owningMethod, fe, trMemory);
                        
            client->write(response, ramMethod, vTableIndex, resolvedInCP, methodInfo);
            }
         else
            {
            client->write(response, ramMethod, vTableIndex, resolvedInCP, TR_ResolvedJ9JITaaSServerMethodInfo());
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
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
         if (ramMethod)
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodMirror(methodInfo, ramMethod, 0, owningMethod, fe, trMemory);
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

         // create a mirror right away
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
         if (resolved)
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, owningMethod, fe, trMemory);

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
         J9Method *j9method = NULL;
            {
            TR::VMAccessCriticalSection getResolvedHandleMethod(fe);
            j9method = jitGetImproperInterfaceMethodFromCP(fe->vmThread(), mirror->cp(), cpIndex);
            }
         // create a mirror right away
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo;
         if (j9method)
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) j9method, 0, mirror, fe, trMemory);

         client->write(response, j9method, methodInfo);
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
         client->write(response, TR_J9MethodBase::isBigDecimalMethod(j9method));
         }
         break;
      case MessageType::ResolvedMethod_isBigDecimalConvertersMethod:
         {
         J9Method *j9method = std::get<0>(client->getRecvData<J9Method*>());
         client->write(response, TR_J9MethodBase::isBigDecimalConvertersMethod(j9method));
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
         std::vector<TR_ResolvedJ9JITaaSServerMethodInfo> methodInfos(numMethods);
         for (int32_t i = 0; i < numMethods; ++i)
            {
            int32_t cpIndex = cpIndices[i];
            TR_ResolvedMethodType type = methodTypes[i];
            J9Method *ramMethod = NULL;
            uint32_t vTableOffset = 0;
            TR_ResolvedJ9JITaaSServerMethodInfo methodInfo;
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
               TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(
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
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
         TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) j9method, vTableSlot, mirror, fe, trMemory);

         // Collect AOT stats
         TR_ResolvedJ9Method *resolvedMethod = std::get<0>(methodInfo).remoteMirror;

         isRomClassForMethodInSC = TR::CompilationInfo::get(fe->_jitConfig)->isRomClassForMethodInSharedCache(j9method, fe->_jitConfig->javaVM);

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
            definingClass = (J9Class *) compInfoPT->reloRuntime()->getClassFromCP(fe->vmThread(), fe->_jitConfig->javaVM, constantPool, cpIndex, isStatic);
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
         TR_OpaqueClassBlock *definingClass = compInfoPT->reloRuntime()->getClassFromCP(fe->vmThread(), fe->_jitConfig->javaVM, constantPool, cpIndex, false);

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
         TR_OpaqueClassBlock *definingClass = compInfoPT->reloRuntime()->getClassFromCP(fe->vmThread(), fe->_jitConfig->javaVM, constantPool, cpIndex, true);

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
            // put indices in the original order, reversal is done on the server
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
         auto table = (TR_JITaaSClientPersistentCHTable*)comp->getPersistentInfo()->getPersistentCHTable();
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
         auto table = (TR_JITaaSClientPersistentCHTable*)comp->getPersistentInfo()->getPersistentCHTable();
         auto encoded = table->serializeUpdates();
         client->write(response, encoded.first, encoded.second);
         }
         break;
      case MessageType::CHTable_clearReservable:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock*>());
         auto table = (TR_JITaaSClientPersistentCHTable*)comp->getPersistentInfo()->getPersistentCHTable();
         auto info = table->findClassInfoAfterLocking(clazz, comp);
         info->setReservable(false);
         }
         break;
      case MessageType::IProfiler_searchForMethodSample:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock*>();
         auto method = std::get<0>(recv);
         TR_JITaaSClientIProfiler *iProfiler = (TR_JITaaSClientIProfiler *) fe->getIProfiler();
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
   return done;
   }

// Method executed by the JITaaS client to schedule a remote compilation
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
   // JITaas: if TR_EnableJITaaSPerCompCon is set, then each remote compilation establishes a new connection 
   // instead of re-using the connection shared within a compilation thread
   static bool enableJITaaSPerCompConn = feGetEnv("TR_EnableJITaaSPerCompConn") ? true : false;

   // Prepare the parameters for the compilation request
   J9Class *clazz = J9_CLASS_FROM_METHOD(method);
   J9ROMClass *romClass = clazz->romClass;
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
   uint32_t romMethodOffset = uint32_t((uint8_t*) romMethod - (uint8_t*) romClass);
   std::string detailsStr = std::string((char*) &details, sizeof(TR::IlGeneratorMethodDetails));
   TR::CompilationInfo *compInfo = compInfoPT->getCompilationInfo();
   bool useAotCompilation = compInfoPT->getMethodBeingCompiled()->_useAotCompilation;

   JITServer::ClientStream *client = enableJITaaSPerCompConn ? NULL : compInfoPT->getClientStream();
   if (!client)
      {
      try 
         {
         if (JITaaSHelpers::isServerAvailable())
            {
            client = new (PERSISTENT_NEW) JITServer::ClientStream(compInfo->getPersistentInfo());
            if (!enableJITaaSPerCompConn)
               compInfoPT->setClientStream(client);
            }
         else if (JITaaSHelpers::shouldRetryConnection(OMRPORT_FROM_J9PORT(compInfoPT->getJitConfig()->javaVM->portLibrary)))
            {
            client = new (PERSISTENT_NEW) JITServer::ClientStream(compInfo->getPersistentInfo());
            if (!enableJITaaSPerCompConn)
               compInfoPT->setClientStream(client);
            JITaaSHelpers::postStreamConnectionSuccess();
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
         JITaaSHelpers::postStreamFailure(OMRPORT_FROM_J9PORT(compInfoPT->getJitConfig()->javaVM->portLibrary));
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
   auto classInfoTuple = JITaaSHelpers::packRemoteROMClassInfo(clazz, compiler->fej9vm()->vmThread(), compiler->trMemory());
   std::string optionsStr = TR::Options::packOptions(compiler->getOptions());
   std::string recompMethodInfoStr = compiler->isRecompilationEnabled() ? std::string((char *) compiler->getRecompilationInfo()->getMethodInfo(), sizeof(TR_PersistentMethodInfo)) : std::string();

   
   compInfo->getSequencingMonitor()->enter();
   // Collect the list of unloaded classes
   std::vector<TR_OpaqueClassBlock*> unloadedClasses(compInfo->getUnloadedClassesTempList()->begin(), compInfo->getUnloadedClassesTempList()->end());
   compInfo->getUnloadedClassesTempList()->clear();
   // Collect and encode the CHTable updates; this will acquire CHTable mutex
   //auto table = (TR_JITaaSClientPersistentCHTable*)compInfo->getPersistentInfo()->getPersistentCHTable();
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
      // release VM access just before sending the compilation request
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
      // re-acquire VM access and check for possible class unloading
      acquireVMAccessNoSuspend(vmThread);
      uint8_t interruptReason = compInfoPT->compilationShouldBeInterrupted();
      if (interruptReason)
         {
         auto comp = compInfoPT->getCompilation();
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITServer, TR_VerboseCompilationDispatch))
             TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "Interrupting remote compilation (interruptReason %u) in remoteCompile of %s @ %s",
                 interruptReason, comp->signature(), comp->getHotnessName());

         // Inform server that the compilation has been interrupted
         client->writeError(JITServer::MessageType::compilationInterrupted, 0 /* placeholder */);
         comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted in remoteCompile");
         }

      JITServer::MessageType response;
      while(!handleServerMessage(client, compiler->fej9vm(), response));

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
      JITaaSHelpers::postStreamFailure(OMRPORT_FROM_J9PORT(compInfoPT->getJitConfig()->javaVM->portLibrary));

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
            // compilation is done, now we need client to validate all of the records accumulated by the server,
            // so need to exit heuristic region.
            compiler->exitHeuristicRegion();
            // populate symbol to id map
            compiler->getSymbolValidationManager()->deserializeSymbolToIDMap(svmSymbolToIdStr);
            }

         TR_ASSERT(codeCacheStr.size(), "must have code cache");
         TR_ASSERT(dataCacheStr.size(), "must have data cache");

         compInfoPT->getMethodBeingCompiled()->_optimizationPlan->clone(&modifiedOptPlan);

         // relocate the received compiled code
         metaData = remoteCompilationEnd(vmThread, compiler, compilee, method, compInfoPT, codeCacheStr, dataCacheStr);

         if (!compiler->getOption(TR_DisableCHOpts) && !useAotCompilation)
            {
            TR::ClassTableCriticalSection commit(compiler->fe());

            // intersect classesThatShouldNotBeNewlyExtended with newlyExtendedClasses
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

            if (!JITaaSCHTableCommit(compiler, metaData, chTableData))
               {
#ifdef COLLECT_CHTABLE_STATS
               TR_JITaaSClientPersistentCHTable *table = (TR_JITaaSClientPersistentCHTable*) TR::comp()->getPersistentInfo()->getPersistentCHTable();
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

   if (enableJITaaSPerCompConn && client)
      {
      client->~ClientStream();
      TR_Memory::jitPersistentFree(client);
      }
   return metaData;
   }

TR_MethodMetaData * 
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

   if (!fe->isAOT_DEPRECATED_DO_NOT_USE()) // for relocating received JIT compilations
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
         // relocation failed, fail compilation
         entry->_compErrCode = compInfoPT->reloRuntime()->returnCode();
         comp->failCompilation<J9::AOTRelocationFailed>("Failed to relocate");
         }
      }
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   else // for relocating received AOT compilations
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
            // need to get a non-shared cache VM to relocate
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
            // relocation failed, fail compilation
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

void
outOfProcessCompilationEnd(
   TR_MethodToBeCompiled *entry,
   TR::Compilation *comp)
   {
   entry->_tryCompilingAgain = false; // TODO: Need to handle recompilations gracefully when relocation fails
   auto compInfoPT = ((TR::CompilationInfoPerThreadRemote*)(entry->_compInfoPT));

   TR::CodeCache *codeCache = comp->cg()->getCodeCache();

   J9JITDataCacheHeader *aotMethodHeader      = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
   TR_ASSERT(aotMethodHeader, "The header must have been set");
   TR_AOTMethodHeader   *aotMethodHeaderEntry = (TR_AOTMethodHeader *)(aotMethodHeader + 1);

   U_8 *codeStart = (U_8 *)aotMethodHeaderEntry->compileMethodCodeStartPC;
   OMR::CodeCacheMethodHeader *codeCacheHeader = (OMR::CodeCacheMethodHeader*)codeStart;

   TR_DataCache *dataCache = (TR_DataCache*)comp->getReservedDataCache();
   TR_ASSERT(dataCache, "A dataCache must be reserved for JITServer compilations\n");
   J9JITDataCacheHeader *dataCacheHeader = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
   J9JITExceptionTable *metaData = compInfoPT->getMetadata();

   size_t codeSize = codeCache->getWarmCodeAlloc() - (uint8_t*)codeCacheHeader;
   size_t dataSize = dataCache->getSegment()->heapAlloc - (uint8_t*)dataCacheHeader;

   //TR_ASSERT(((OMR::RuntimeAssumption*)metaData->runtimeAssumptionList)->getNextAssumptionForSameJittedBody() == metaData->runtimeAssumptionList, "assuming no assumptions");

   std::string codeCacheStr((char*) codeCacheHeader, codeSize);
   std::string dataCacheStr((char*) dataCacheHeader, dataSize);

   CHTableCommitData chTableData;
   if (!comp->getOption(TR_DisableCHOpts) && !entry->_useAotCompilation)
      {
      TR_CHTable *chTable = comp->getCHTable();
      chTableData = chTable->computeDataForCHTableCommit(comp);
      }

   auto classesThatShouldNotBeNewlyExtended = compInfoPT->getClassesThatShouldNotBeNewlyExtended();

   // pack log file to send to client
   std::string logFileStr = TR::Options::packLogFile(comp->getOutFile());

   std::string svmSymbolToIdStr;
   if (comp->getOption(TR_UseSymbolValidationManager))
      {
      svmSymbolToIdStr = comp->getSymbolValidationManager()->serializeSymbolToIDMap();
      }

   auto resolvedMirrorMethodsPersistIPInfo = compInfoPT->getCachedResolvedMirrorMethodsPersistIPInfo();
   entry->_stream->finishCompilation(codeCacheStr, dataCacheStr, chTableData,
                                     std::vector<TR_OpaqueClassBlock*>(classesThatShouldNotBeNewlyExtended->begin(), classesThatShouldNotBeNewlyExtended->end()),
                                     logFileStr, svmSymbolToIdStr,
                                     (resolvedMirrorMethodsPersistIPInfo) ?
                                                         std::vector<TR_ResolvedJ9Method*>(resolvedMirrorMethodsPersistIPInfo->begin(), resolvedMirrorMethodsPersistIPInfo->end()) :
                                                         std::vector<TR_ResolvedJ9Method*>(),
                                     *entry->_optimizationPlan
                                     );
   compInfoPT->clearPerCompilationCaches();

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d has successfully compiled %s",
         compInfoPT->getCompThreadId(), compInfoPT->getCompilation()->signature());
      }
   }

void printJITaaSMsgStats(J9JITConfig *jitConfig)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   j9tty_printf(PORTLIB, "JITServer Message Type Statistics:\n");
   j9tty_printf(PORTLIB, "Type# #called TypeName\n");
   const ::google::protobuf::EnumDescriptor *descriptor = JITServer::MessageType_descriptor();
   for (int i = 0; i < JITServer::MessageType_ARRAYSIZE; ++i)
      {
      if (serverMsgTypeCount[i] > 0)
         j9tty_printf(PORTLIB, "#%04d %7u %s\n", i, serverMsgTypeCount[i], descriptor->FindValueByNumber(i)->name().c_str());
      }
   }

void printJITaaSCHTableStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
#ifdef COLLECT_CHTABLE_STATS
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   j9tty_printf(PORTLIB, "JITServer CHTable Statistics:\n");
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      TR_JITaaSClientPersistentCHTable *table = (TR_JITaaSClientPersistentCHTable*)compInfo->getPersistentInfo()->getPersistentCHTable();
      j9tty_printf(PORTLIB, "Num updates sent: %d (1 per compilation)\n", table->_numUpdates);
      if (table->_numUpdates)
         {
         j9tty_printf(PORTLIB, "Num commit failures: %d. Average per compilation: %f\n", table->_numCommitFailures, table->_numCommitFailures / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Num classes updated: %d. Average per compilation: %f\n", table->_numClassesUpdated, table->_numClassesUpdated / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Num classes removed: %d. Average per compilation: %f\n", table->_numClassesRemoved, table->_numClassesRemoved / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Total update bytes: %d. Compilation max: %d. Average per compilation: %f\n", table->_updateBytes, table->_maxUpdateBytes, table->_updateBytes / float(table->_numUpdates));
         }
      }
   else if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      TR_JITaaSServerPersistentCHTable *table = (TR_JITaaSServerPersistentCHTable*)compInfo->getPersistentInfo()->getPersistentCHTable();
      j9tty_printf(PORTLIB, "Num updates received: %d (1 per compilation)\n", table->_numUpdates);
      if (table->_numUpdates)
         {
         j9tty_printf(PORTLIB, "Num classes updated: %d. Average per compilation: %f\n", table->_numClassesUpdated, table->_numClassesUpdated / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Num classes removed: %d. Average per compilation: %f\n", table->_numClassesRemoved, table->_numClassesRemoved / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Num class info queries: %d. Average per compilation: %f\n", table->_numQueries, table->_numQueries / float(table->_numUpdates));
         j9tty_printf(PORTLIB, "Total update bytes: %d. Compilation max: %d. Average per compilation: %f\n", table->_updateBytes, table->_maxUpdateBytes, table->_updateBytes / float(table->_numUpdates));
         }
      }
#endif
   }

void printJITaaSCacheStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      auto clientSessionHT = compInfo->getClientSessionHT();
      clientSessionHT->printStats();
      }
   }

void
ClientSessionData::updateTimeOfLastAccess()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   _timeOfLastAccess = j9time_current_time_millis();
   }

ClientSessionData::ClientSessionData(uint64_t clientUID, uint32_t seqNo) : 
   _clientUID(clientUID), _expectedSeqNo(seqNo), _maxReceivedSeqNo(seqNo), _OOSequenceEntryList(NULL),
   _chTableClassMap(decltype(_chTableClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _romClassMap(decltype(_romClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _J9MethodMap(decltype(_J9MethodMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classByNameMap(decltype(_classByNameMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _classChainDataMap(decltype(_classChainDataMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _constantPoolToClassMap(decltype(_constantPoolToClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _unloadedClassAddresses(NULL),
   _requestUnloadedClasses(true),
   _staticFinalDataMap(decltype(_staticFinalDataMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _rtResolve(false),
   _registeredJ2IThunksMap(decltype(_registeredJ2IThunksMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _registeredInvokeExactJ2IThunksSet(decltype(_registeredInvokeExactJ2IThunksSet)::allocator_type(TR::Compiler->persistentAllocator()))
   {
   updateTimeOfLastAccess();
   _javaLangClassPtr = NULL;
   _inUse = 1;
   _numActiveThreads = 0;
   _romMapMonitor = TR::Monitor::create("JIT-JITaaSROMMapMonitor");
   _classMapMonitor = TR::Monitor::create("JIT-JITaaSClassMapMonitor");
   _classChainDataMapMonitor = TR::Monitor::create("JIT-JITaaSClassChainDataMapMonitor");
   _sequencingMonitor = TR::Monitor::create("JIT-JITaaSSequencingMonitor");
   _constantPoolMapMonitor = TR::Monitor::create("JIT-JITaaSConstantPoolMonitor");
   _vmInfo = NULL;
   _staticMapMonitor = TR::Monitor::create("JIT-JITaaSStaticMapMonitor");
   _markedForDeletion = false;
   _thunkSetMonitor = TR::Monitor::create("JIT-JITaaSThunkSetMonitor");
   }

ClientSessionData::~ClientSessionData()
   {
   clearCaches();
   _romMapMonitor->destroy();
   _classMapMonitor->destroy();
   _classChainDataMapMonitor->destroy();
   _sequencingMonitor->destroy();
   _constantPoolMapMonitor->destroy();
   if (_unloadedClassAddresses)
      {
      _unloadedClassAddresses->destroy();
      jitPersistentFree(_unloadedClassAddresses);
      }
   _staticMapMonitor->destroy();
   if (_vmInfo)
      jitPersistentFree(_vmInfo);
   _thunkSetMonitor->destroy();
   }

void
ClientSessionData::processUnloadedClasses(JITServer::ServerStream *stream, const std::vector<TR_OpaqueClassBlock*> &classes)
   {
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server will process a list of %u unloaded classes for clientUID %llu", (unsigned)classes.size(), (unsigned long long)_clientUID);
   //Vector to hold the list of the unloaded classes and the corresponding data needed for purging the Caches
   std::vector<ClassUnloadedData> unloadedClasses;
   unloadedClasses.reserve(classes.size());
   bool updateUnloadedClasses = true;
   if (_requestUnloadedClasses)
      {
      stream->write(JITServer::MessageType::getUnloadedClassRanges, JITServer::Void());

      auto response = stream->read<std::vector<TR_AddressRange>, int32_t>();
      auto unloadedClassRanges = std::get<0>(response);
      auto maxRanges = std::get<1>(response);

         {
         OMR::CriticalSection getUnloadedClasses(getROMMapMonitor());

         if (!_unloadedClassAddresses)
            _unloadedClassAddresses = new (PERSISTENT_NEW) TR_AddressSet(trPersistentMemory, maxRanges);
         _unloadedClassAddresses->setRanges(unloadedClassRanges);
         _requestUnloadedClasses = false;
         }

      updateUnloadedClasses = false;
      }
      {
      OMR::CriticalSection processUnloadedClasses(getROMMapMonitor());

      for (auto clazz : classes)
         {
         if (updateUnloadedClasses)
            _unloadedClassAddresses->add((uintptrj_t)clazz);

         auto it = _romClassMap.find((J9Class*) clazz);
         if (it == _romClassMap.end())
            {
            //Class is not cached so this entry will be used to delete the entry from caches by value.
            ClassLoaderStringPair key;
            unloadedClasses.push_back({clazz, key, NULL, false});
            continue; // unloaded class was never cached
            }

         J9ConstantPool *cp = it->second._constantPool;

         J9ROMClass *romClass = it->second.romClass;

         J9UTF8 *clazzName = NNSRP_GET(romClass->className, J9UTF8 *);
         std::string className((const char *)clazzName->data, (int32_t)clazzName->length);
         J9ClassLoader * cl = (J9ClassLoader *)(it->second.classLoader);
         ClassLoaderStringPair key = {cl, className};
         //Class is cached, so retain the data to be used for purging the caches.
         unloadedClasses.push_back({clazz, key, cp, true});

         J9Method *methods = it->second.methodsOfClass;
         // delete all the cached J9Methods belonging to this unloaded class
         for (size_t i = 0; i < romClass->romMethodCount; i++)
            {
            J9Method *j9method = methods + i;
            auto iter = _J9MethodMap.find(j9method);
            if (iter != _J9MethodMap.end())
               {
               IPTable_t *ipDataHT = iter->second._IPData;
               if (ipDataHT)
                  {
                  for (auto entryIt : *ipDataHT)
                     {
                     auto entryPtr = entryIt.second;
                     if (entryPtr)
                        jitPersistentFree(entryPtr);
                     }
                  ipDataHT->~IPTable_t();
                  jitPersistentFree(ipDataHT);
                  iter->second._IPData = NULL;
                  }
               _J9MethodMap.erase(j9method);
               }
            }
         it->second.freeClassInfo();
         _romClassMap.erase(it);
         }
      }

      {
      OMR::CriticalSection processUnloadedClasses(getClassChainDataMapMonitor());

      //remove the class chain data from the cache for the unloaded class.
      for (auto clazz : classes)
         _classChainDataMap.erase((J9Class*)clazz);
      }

      // purge Class by name cache
      {
      OMR::CriticalSection classMapCS(getClassMapMonitor());
      JITaaSHelpers::purgeCache(&unloadedClasses, getClassByNameMap(), &ClassUnloadedData::_pair);
      }

      // purge Constant pool to class cache
      {
      OMR::CriticalSection constantPoolToClassMap(getConstantPoolMonitor());
      JITaaSHelpers::purgeCache(&unloadedClasses, getConstantPoolToClassMap(), &ClassUnloadedData::_cp);
      }
   }

TR_IPBytecodeHashTableEntry*
ClientSessionData::getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent)
   {
   *methodInfoPresent = false;
   TR_IPBytecodeHashTableEntry *ipEntry = NULL;
   OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
   // check whether info about j9method is cached
   auto & j9methodMap = getJ9MethodMap();
   auto it = j9methodMap.find((J9Method*)method);
   if (it != j9methodMap.end())
      {
      // check whether j9method data has any IP information
      auto iProfilerMap = it->second._IPData;
      if (iProfilerMap)
         {
         *methodInfoPresent = true;
         // check whether desired bcindex is cached
         auto ipData = iProfilerMap->find(byteCodeIndex);
         if (ipData != iProfilerMap->end())
            {
            ipEntry = ipData->second;
            }
         }
      }
   else
      {
      // Very unlikely scenario because the optimizer will have created  ResolvedJ9Method
      // whose constructor would have fetched and cached the j9method info
      TR_ASSERT(false, "profilingSample: asking about j9method=%p but this is not present in the J9MethodMap", method);
      }
   return ipEntry;
   }

bool 
ClientSessionData::cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry)
   {
   OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
   // check whether info about j9method exists
   auto & j9methodMap = getJ9MethodMap();
   auto it = j9methodMap.find((J9Method*)method);
   if (it != j9methodMap.end())
      {
      IPTable_t *iProfilerMap = it->second._IPData;
      if (!iProfilerMap)
         {
         // Check and update if method is compiled when collecting profiling data
         bool isCompiled = TR::CompilationInfo::isCompiled((J9Method*)method);
         if(isCompiled)
            it->second._isCompiledWhenProfiling = true;

         // allocate a new iProfiler map
         iProfilerMap = new (PERSISTENT_NEW) IPTable_t(IPTable_t::allocator_type(TR::Compiler->persistentAllocator()));
         if (iProfilerMap)
            {
            it->second._IPData = iProfilerMap;
            // entry could be null; this means that the method has no IProfiler info
            if (entry)
               iProfilerMap->insert({ byteCodeIndex, entry });
            return true;
            }
         }
      else
         {
         if (entry)
            iProfilerMap->insert({ byteCodeIndex, entry });
         return true;
         }
      }
   else
      {
      // JASS TODO: count how many times we cannot cache
      // There should be very few instances if at all
      }
   return false; // false means that caching attempt failed
   }

void
ClientSessionData::printStats()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   j9tty_printf(PORTLIB, "\tNum cached ROM classes: %d\n", _romClassMap.size());
   j9tty_printf(PORTLIB, "\tNum cached ROM methods: %d\n", _J9MethodMap.size());
   size_t total = 0;
   for (auto it : _romClassMap)
      {
      total += it.second.romClass->romSize;
      }
   j9tty_printf(PORTLIB, "\tTotal size of cached ROM classes + methods: %d bytes\n", total);
   }

void
ClientSessionData::ClassInfo::freeClassInfo()
   {
   TR_Memory::jitPersistentFree(romClass);
   // If string cache exists, free it
   if (_remoteROMStringsCache)
      {
      _remoteROMStringsCache->~PersistentUnorderedMap<TR_RemoteROMStringKey, std::string>();
      jitPersistentFree(_remoteROMStringsCache);
      }

   // if fieldOrStaticNameCache exists, free it
   if (_fieldOrStaticNameCache)
      {
      _fieldOrStaticNameCache->~PersistentUnorderedMap<int32_t, std::string>();
      jitPersistentFree(_fieldOrStaticNameCache);
      }
   // free cached interfaces
   interfaces->~PersistentVector<TR_OpaqueClassBlock *>();
   jitPersistentFree(interfaces);

   // if class of static cache exists, free it
   if (_classOfStaticCache)
      {
      _classOfStaticCache->~PersistentUnorderedMap<int32_t, TR_OpaqueClassBlock *>();
      jitPersistentFree(_classOfStaticCache);
      }

   if (_constantClassPoolCache)
      {
      _constantClassPoolCache->~PersistentUnorderedMap<int32_t, TR_OpaqueClassBlock *>();
      jitPersistentFree(_constantClassPoolCache);
      }

   if (_fieldAttributesCache)
      {
      _fieldAttributesCache->~TR_FieldAttributesCache();
      jitPersistentFree(_fieldAttributesCache);
      }
   if (_staticAttributesCache)
      {
      _staticAttributesCache->~TR_FieldAttributesCache();
      jitPersistentFree(_staticAttributesCache);
      }

   if (_fieldAttributesCacheAOT)
      {
      _fieldAttributesCacheAOT->~TR_FieldAttributesCache();
      jitPersistentFree(_fieldAttributesCacheAOT);
      }
   if (_staticAttributesCacheAOT)
      {
      _staticAttributesCacheAOT->~TR_FieldAttributesCache();
      jitPersistentFree(_staticAttributesCacheAOT);
      }
   if (_jitFieldsCache)
      {
      _jitFieldsCache->~TR_JitFieldsCache();
      jitPersistentFree(_jitFieldsCache);
      }
   if (_fieldOrStaticDeclaringClassCache)
      {
      _fieldOrStaticDeclaringClassCache->~PersistentUnorderedMap<int32_t, TR_OpaqueClassBlock *>();
      jitPersistentFree(_fieldOrStaticDeclaringClassCache);
      }
   }

ClientSessionData::VMInfo *
ClientSessionData::getOrCacheVMInfo(JITServer::ServerStream *stream)
   {
   if (!_vmInfo)
      {
      stream->write(JITServer::MessageType::VM_getVMInfo, JITServer::Void());
      _vmInfo = new (PERSISTENT_NEW) VMInfo(std::get<0>(stream->read<VMInfo>()));
      }
   return _vmInfo;
   }


void
ClientSessionData::clearCaches()
   {
      {
      OMR::CriticalSection clearCache(getClassMapMonitor());
      _classByNameMap.clear();
      }
   OMR::CriticalSection getRemoteROMClass(getROMMapMonitor());
   // Free memory for all hashtables with IProfiler info
   for (auto it : _J9MethodMap)
      {
      IPTable_t *ipDataHT = it.second._IPData;
      // It it exists, walk the collection of <pc, TR_IPBytecodeHashTableEntry*> mappings
      if (ipDataHT)
         {
         for (auto entryIt : *ipDataHT)
            {
            auto entryPtr = entryIt.second;
            if (entryPtr)
               jitPersistentFree(entryPtr);
            }
         ipDataHT->~IPTable_t();
         jitPersistentFree(ipDataHT);
         it.second._IPData = NULL;
         }
      }

   _J9MethodMap.clear();
   // Free memory for j9class info
   for (auto it : _romClassMap)
      {
      it.second.freeClassInfo();
      }

   _romClassMap.clear();

   _classChainDataMap.clear();
   // TODO: do we need another monitor here?

   // Free CHTable 
   for (auto it : _chTableClassMap)
      {
      TR_PersistentClassInfo *classInfo = it.second;
      classInfo->removeSubClasses();
      jitPersistentFree(classInfo);
      }
   _chTableClassMap.clear();
   _requestUnloadedClasses = true;
   _registeredJ2IThunksMap.clear();
   _registeredInvokeExactJ2IThunksSet.clear();
   }

void 
ClientSessionData::destroy(ClientSessionData *clientSession)
   {
   clientSession->~ClientSessionData();
   TR_PersistentMemory::jitPersistentFree(clientSession); 
   }

ClientSessionHT*
ClientSessionHT::allocate()
   {
   return new (PERSISTENT_NEW) ClientSessionHT();
   }

// Search the clientSessionHashtable for the given clientUID and return the
// data corresponding to the client.
// If the clientUID does not already exist in the HT, insert a new blank entry.
// Must have compilation monitor in hand when calling this function.
// Side effects: _inUse is incremented on the ClientSessionData
//               _expectedSeqNo is populated if a new ClientSessionData is created
//                timeOflastAccess is updated with curent time.
ClientSessionData * 
ClientSessionHT::findOrCreateClientSession(uint64_t clientUID, uint32_t seqNo, bool *newSessionWasCreated)
   {
   *newSessionWasCreated = false;
   ClientSessionData *clientData = findClientSession(clientUID);
   if (!clientData)
      {
      // alocate a new ClientSessionData object and create a clientUID mapping
      clientData = new (PERSISTENT_NEW) ClientSessionData(clientUID, seqNo);
      if (clientData)
         {
         _clientSessionMap[clientUID] = clientData;
         *newSessionWasCreated = true;
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server allocated data for a new clientUID %llu", (unsigned long long)clientUID);
         }
      else
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "ERROR: Server could not allocate client session data");
         // should we throw bad_alloc here?
         }
      }
   return clientData;
   }
// Search the clientSessionHashtable for the given clientUID and delete  the
// data corresponding to the client. Return true if client data found and deleted.
// Must have compilation monitor in hand when calling this function.

bool
ClientSessionHT::deleteClientSession(uint64_t clientUID, bool forDeletion)
   {
   ClientSessionData *clientData = NULL;
   auto clientDataIt = _clientSessionMap.find(clientUID);
   if (clientDataIt != _clientSessionMap.end())
      {
      clientData = clientDataIt->second;
      if (forDeletion)
         clientData->markForDeletion();

      if ((clientData->getInUse() == 0) && clientData->isMarkedForDeletion())
         {
         ClientSessionData::destroy(clientData); // delete the client data
         _clientSessionMap.erase(clientDataIt); // delete the mapping from the hashtable
         return true;
         }
      }
   return false;
   }

// Search the clientSessionHashtable for the given clientUID and return the
// data corresponding to the client.
// Must have compilation monitor in hand when calling this function.
// Side effects: _inUse is incremented on the ClientSessionData and the
// timeOflastAccess is updated with curent time.
ClientSessionData *
ClientSessionHT::findClientSession(uint64_t clientUID)
   {
   ClientSessionData *clientData = NULL;
   auto clientDataIt = _clientSessionMap.find(clientUID);
   if (clientDataIt != _clientSessionMap.end())
      {
      // if clientData found in hashtable, update the access time before returning it
      clientData = clientDataIt->second;
      clientData->incInUse();
      clientData->updateTimeOfLastAccess();
      }
   return clientData;
   }


ClientSessionHT::ClientSessionHT() : _clientSessionMap(decltype(_clientSessionMap)::allocator_type(TR::Compiler->persistentAllocator())),
                                     TIME_BETWEEN_PURGES(1000*60*30), // JITaaS TODO: this must come from options
                                     OLD_AGE(1000*60*1000) // 1000 minutes
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   _timeOfLastPurge = j9time_current_time_millis();
   _clientSessionMap.reserve(250); // allow room for at least 250 clients 
   }

// The destructor is currently never called because the server does not exit cleanly
ClientSessionHT::~ClientSessionHT()
   {
   for (auto iter = _clientSessionMap.begin(); iter != _clientSessionMap.end(); ++iter)
      {
      ClientSessionData::destroy(iter->second); // delete the client data
      _clientSessionMap.erase(iter); // delete the mapping from the hashtable
      }
   }

// Purge the old client session data from the hashtable and
// update the timeOfLastPurge.
// Entries with _inUse > 0 must be left alone, though having
// very old entries in use it's a sign of a programming error.
// This routine must be executed with compilation monitor in hand.
void 
ClientSessionHT::purgeOldDataIfNeeded()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   int64_t crtTime = j9time_current_time_millis();
   if (crtTime - _timeOfLastPurge > TIME_BETWEEN_PURGES)
      {
      // Time for a purge operation.
      // Scan the entire table and delete old elements that are not in use
      for (auto iter = _clientSessionMap.begin(); iter != _clientSessionMap.end(); ++iter)
         {
         TR_ASSERT(iter->second->getInUse() >= 0, "_inUse=%d must be positive\n", iter->second->getInUse());
         if (iter->second->getInUse() == 0 &&
             crtTime - iter->second->getTimeOflastAccess() > OLD_AGE)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server will purge session data for clientUID %llu", (unsigned long long)iter->first);
            ClientSessionData::destroy(iter->second); // delete the client data
            _clientSessionMap.erase(iter); // delete the mapping from the hashtable
            }
         }
      _timeOfLastPurge = crtTime;

      // JITaaS TODO: keep stats on how many elements were purged
      }
   }

// to print these stats,
// set the env var `TR_PrintJITaaSCacheStats=1`
// run the server with `-Xdump:jit:events=user`
// then `kill -3` it when you want to print them 
void
ClientSessionHT::printStats()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   j9tty_printf(PORTLIB, "Client sessions:\n");
   for (auto session : _clientSessionMap)
      {
      j9tty_printf(PORTLIB, "Session for id %d:\n", session.first);
      session.second->printStats();
      }
   }

void 
JITaaSHelpers::cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple)
   {
   ClientSessionData::ClassInfo classInfo;
   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find((J9Class*)clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      JITaaSHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, classInfoTuple, classInfo);
      }
   }

void
JITaaSHelpers::cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple, ClientSessionData::ClassInfo &classInfoStruct)
   {
   ClassInfoTuple &classInfo = *classInfoTuple;

   classInfoStruct.romClass = romClass;
   classInfoStruct.methodsOfClass = std::get<1>(classInfo);
   J9Method *methods = classInfoStruct.methodsOfClass;
   classInfoStruct.baseComponentClass = std::get<2>(classInfo);
   classInfoStruct.numDimensions = std::get<3>(classInfo);
   classInfoStruct._remoteROMStringsCache = NULL;
   classInfoStruct._fieldOrStaticNameCache = NULL;
   classInfoStruct.parentClass = std::get<4>(classInfo);
   auto &tmpInterfaces = std::get<5>(classInfo);
   classInfoStruct.interfaces = new (PERSISTENT_NEW) PersistentVector<TR_OpaqueClassBlock *>
      (tmpInterfaces.begin(), tmpInterfaces.end(),
       PersistentVector<TR_OpaqueClassBlock *>::allocator_type(TR::Compiler->persistentAllocator()));
   auto &methodTracingInfo = std::get<6>(classInfo);
   classInfoStruct.classHasFinalFields = std::get<7>(classInfo);
   classInfoStruct.classDepthAndFlags = std::get<8>(classInfo);
   classInfoStruct.classInitialized = std::get<9>(classInfo);
   classInfoStruct.byteOffsetToLockword = std::get<10>(classInfo);
   classInfoStruct.leafComponentClass = std::get<11>(classInfo);
   classInfoStruct.classLoader = std::get<12>(classInfo);
   classInfoStruct.hostClass = std::get<13>(classInfo);
   classInfoStruct.componentClass = std::get<14>(classInfo);
   classInfoStruct.arrayClass = std::get<15>(classInfo);
   classInfoStruct.totalInstanceSize = std::get<16>(classInfo);
   classInfoStruct._classOfStaticCache = NULL;
   classInfoStruct._constantClassPoolCache = NULL;
   classInfoStruct.remoteRomClass = std::get<17>(classInfo);
   classInfoStruct._fieldAttributesCache = NULL;
   classInfoStruct._staticAttributesCache = NULL;
   classInfoStruct._fieldAttributesCacheAOT = NULL;
   classInfoStruct._staticAttributesCacheAOT = NULL;
   classInfoStruct._constantPool = (J9ConstantPool *)std::get<18>(classInfo);
   classInfoStruct._jitFieldsCache = NULL;
   classInfoStruct.classFlags = std::get<19>(classInfo);
   classInfoStruct._fieldOrStaticDeclaringClassCache = NULL;
   clientSessionData->getROMClassMap().insert({ clazz, classInfoStruct});

   uint32_t numMethods = romClass->romMethodCount;
   J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   for (uint32_t i = 0; i < numMethods; i++)
      {
      clientSessionData->getJ9MethodMap().insert({&methods[i], 
            {romMethod, NULL, static_cast<bool>(methodTracingInfo[i]), (TR_OpaqueClassBlock *)clazz, false}});
      romMethod = nextROMMethod(romMethod);
      }
   }

J9ROMClass *
JITaaSHelpers::getRemoteROMClassIfCached(ClientSessionData *clientSessionData, J9Class *clazz)
   {
   OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   return (it == clientSessionData->getROMClassMap().end()) ? NULL : it->second.romClass;
   }

template <typename map, typename key>
void JITaaSHelpers::purgeCache (std::vector<ClassUnloadedData> *unloadedClasses, map m, key ClassUnloadedData::*k)
   {
   ClassUnloadedData *data = unloadedClasses->data();
   std::vector<ClassUnloadedData>::iterator it = unloadedClasses->begin();
   while (it != unloadedClasses->end())
      {
      if (it->_cached)
         {
         m.erase((data->*k));
         }
      else
         {
         //If the class is not cached this is the place to iterate the cache(Map) for deleting the entry by value rather then key.
         auto itClass = m.begin();
         while (itClass != m.end())
            {
            if (itClass->second == data->_class)
               {
               m.erase(itClass);
               break;
               }
            ++itClass;
            }
         }
      //DO NOT remove the entry from the unloadedClasses as it will be needed to purge other caches.
      ++it;
      ++data;
      }
   }

JITaaSHelpers::ClassInfoTuple
JITaaSHelpers::packRemoteROMClassInfo(J9Class *clazz, J9VMThread *vmThread, TR_Memory *trMemory)
   {
   // Always use the base VM here.
   // If this method is called inside AOT compilation, TR_J9SharedCacheVM will
   // attempt validation and return NULL for many methods invoked here.
   // We do not want that, because these values will be cached and later used in non-AOT
   // compilations, where we always need a non-NULL result.
   TR_J9VM *fe = (TR_J9VM *) TR_J9VMBase::get(vmThread->javaVM->jitConfig, vmThread);
   J9Method *methodsOfClass = (J9Method*) fe->getMethods((TR_OpaqueClassBlock*) clazz);
   int32_t numDims = 0;
   TR_OpaqueClassBlock *baseClass = fe->getBaseComponentClass((TR_OpaqueClassBlock *) clazz, numDims);
   TR_OpaqueClassBlock *parentClass = fe->getSuperClass((TR_OpaqueClassBlock *) clazz);

   uint32_t numMethods = clazz->romClass->romMethodCount;
   std::vector<uint8_t> methodTracingInfo;
   methodTracingInfo.reserve(numMethods);
   for(uint32_t i = 0; i < numMethods; ++i)
      {
      methodTracingInfo.push_back(static_cast<uint8_t>(fe->isMethodTracingEnabled((TR_OpaqueMethodBlock *) &methodsOfClass[i])));
      }
   bool classHasFinalFields = fe->hasFinalFieldsInClass((TR_OpaqueClassBlock *)clazz);
   uintptrj_t classDepthAndFlags = fe->getClassDepthAndFlagsValue((TR_OpaqueClassBlock *)clazz);
   bool classInitialized =  fe->isClassInitialized((TR_OpaqueClassBlock *)clazz);
   uint32_t byteOffsetToLockword = fe->getByteOffsetToLockword((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock * leafComponentClass = fe->getLeafComponentClassFromArrayClass((TR_OpaqueClassBlock *)clazz);
   void * classLoader = fe->getClassLoader((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock * hostClass = fe->convertClassPtrToClassOffset(clazz->hostClass);
   TR_OpaqueClassBlock * componentClass = fe->getComponentClassFromArrayClass((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock * arrayClass = fe->getArrayClassFromComponentClass((TR_OpaqueClassBlock *)clazz);
   uintptrj_t totalInstanceSize = clazz->totalInstanceSize;
   uintptrj_t cp = fe->getConstantPoolFromClass((TR_OpaqueClassBlock *)clazz);
   uintptrj_t classFlags = fe->getClassFlagsValue((TR_OpaqueClassBlock *)clazz);
   return std::make_tuple(packROMClass(clazz->romClass, trMemory), methodsOfClass, baseClass, numDims, parentClass, TR::Compiler->cls.getITable((TR_OpaqueClassBlock *) clazz), methodTracingInfo, classHasFinalFields, classDepthAndFlags, classInitialized, byteOffsetToLockword, leafComponentClass, classLoader, hostClass, componentClass, arrayClass, totalInstanceSize, clazz->romClass, cp, classFlags);
   }

J9ROMClass *
JITaaSHelpers::romClassFromString(const std::string &romClassStr, TR_PersistentMemory *trMemory)
   {
   auto romClass = (J9ROMClass *)(trMemory->allocatePersistentMemory(romClassStr.size(), TR_Memory::ROMClass));
   if (!romClass)
      throw std::bad_alloc();
   memcpy(romClass, &romClassStr[0], romClassStr.size());
   return romClass;
   }

J9ROMClass *
JITaaSHelpers::getRemoteROMClass(J9Class *clazz, JITServer::ServerStream *stream, TR_Memory *trMemory, ClassInfoTuple *classInfoTuple)
   {
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   *classInfoTuple = std::get<0>(recv);
   return romClassFromString(std::get<0>(*classInfoTuple), trMemory->trPersistentMemory());
   }

// Return true if able to get data from cache, return false otherwise
bool
JITaaSHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType, void *data)
   {
   JITaaSHelpers::ClassInfoTuple classInfoTuple;
   ClientSessionData::ClassInfo classInfo;
   if (!clazz)
      {
      return false;
      }
      {
      OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
      auto it = clientSessionData->getROMClassMap().find((J9Class*)clazz);
      if (it != clientSessionData->getROMClassMap().end())
         {
         JITaaSHelpers::getROMClassData(it->second, dataType, data);
         return true;
         }
      }
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   classInfoTuple = std::get<0>(recv);

   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      auto romClass = romClassFromString(std::get<0>(classInfoTuple), TR::comp()->trMemory()->trPersistentMemory());
      JITaaSHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, &classInfoTuple, classInfo);
      JITaaSHelpers::getROMClassData(classInfo, dataType, data);
      }
   else
      {
      JITaaSHelpers::getROMClassData(it->second, dataType, data);
      }
   return false;
   }

// Return true if able to get data from cache, return false otherwise
bool
JITaaSHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType1, void *data1, ClassInfoDataType dataType2, void *data2)
   {
   JITaaSHelpers::ClassInfoTuple classInfoTuple;
   ClientSessionData::ClassInfo classInfo;
   if (!clazz)
      {
      return false;
      }
      {
      OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
      auto it = clientSessionData->getROMClassMap().find((J9Class*)clazz);
      if (it != clientSessionData->getROMClassMap().end())
         {
         JITaaSHelpers::getROMClassData(it->second, dataType1, data1);
         JITaaSHelpers::getROMClassData(it->second, dataType2, data2);
         return true;
         }
      }
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   classInfoTuple = std::get<0>(recv);

   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      auto romClass = romClassFromString(std::get<0>(classInfoTuple), TR::comp()->trMemory()->trPersistentMemory());
      JITaaSHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, &classInfoTuple, classInfo);
      JITaaSHelpers::getROMClassData(classInfo, dataType1, data1);
      JITaaSHelpers::getROMClassData(classInfo, dataType2, data2);
      }
   else
      {
      JITaaSHelpers::getROMClassData(it->second, dataType1, data1);
      JITaaSHelpers::getROMClassData(it->second, dataType2, data2);
      }
   return false;
   }

void
JITaaSHelpers::getROMClassData(const ClientSessionData::ClassInfo &classInfo, ClassInfoDataType dataType, void *data)
   {
   switch (dataType)
      {
      case CLASSINFO_ROMCLASS_MODIFIERS :
         {
         *(uint32_t *)data = classInfo.romClass->modifiers;
         }
         break;
      case CLASSINFO_ROMCLASS_EXTRAMODIFIERS :
         {
         *(uint32_t *)data = classInfo.romClass->extraModifiers;
         }
         break;
      case CLASSINFO_BASE_COMPONENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo.baseComponentClass;
         }
         break;
      case CLASSINFO_NUMBER_DIMENSIONS :
         {
         *(int32_t *)data = classInfo.numDimensions;
         }
         break;
      case CLASSINFO_PARENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo.parentClass;
         }
         break;
      case CLASSINFO_CLASS_HAS_FINAL_FIELDS :
         {
         *(bool *)data = classInfo.classHasFinalFields;
         }
         break;
      case CLASSINFO_CLASS_DEPTH_AND_FLAGS :
         {
         *(uintptrj_t *)data = classInfo.classDepthAndFlags;
         }
         break;
      case CLASSINFO_CLASS_INITIALIZED :
         {
         *(bool *)data = classInfo.classInitialized;
         }
         break;
      case CLASSINFO_BYTE_OFFSET_TO_LOCKWORD :
         {
         *(uint32_t *)data = classInfo.byteOffsetToLockword;
         }
         break;
      case CLASSINFO_LEAF_COMPONENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo.leafComponentClass;
         }
         break;
      case CLASSINFO_CLASS_LOADER :
         {
         *(void **)data = classInfo.classLoader;
         }
         break;
      case CLASSINFO_HOST_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo.hostClass;
         }
         break;
      case CLASSINFO_COMPONENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo.componentClass;
         }
         break;
      case CLASSINFO_ARRAY_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo.arrayClass;
         }
         break;
      case CLASSINFO_TOTAL_INSTANCE_SIZE :
         {
         *(uintptrj_t *)data = classInfo.totalInstanceSize;
         }
         break;
      case CLASSINFO_REMOTE_ROM_CLASS :
         {
         *(J9ROMClass **)data = classInfo.remoteRomClass;
         }
         break;
      case CLASSINFO_CLASS_FLAGS :
         {
         *(uintptrj_t *)data = classInfo.classFlags;
         }
         break;
      case CLASSINFO_METHODS_OF_CLASS :
         {
         *(J9Method **)data = classInfo.methodsOfClass;
         }
         break;
      default :
         {
         TR_ASSERT(0, "Class Info not supported %u \n", dataType);
         }
         break;
      }
   }

void
JITaaSHelpers::postStreamFailure(OMRPortLibrary *portLibrary)
   {
   OMR::CriticalSection handleStreamFailure(getClientStreamMonitor());

   OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
   uint64_t current_time = omrtime_current_time_millis();
   if (current_time >= _nextConnectionRetryTime)
      {
      _waitTime *= 2; // exponential backoff
      }
   _nextConnectionRetryTime = current_time + _waitTime;
   _serverAvailable = false;
   }

void
JITaaSHelpers::postStreamConnectionSuccess()
   {
   _serverAvailable = true;
   _waitTime = 1000; // ms
   }

bool
JITaaSHelpers::shouldRetryConnection(OMRPortLibrary *portLibrary)
   {
   OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
   return omrtime_current_time_millis() > _nextConnectionRetryTime;
   }

// insertIntoOOSequenceEntryList needs to be executed with sequencingMonitor in hand
void 
ClientSessionData::insertIntoOOSequenceEntryList(TR_MethodToBeCompiled *entry)
   {
   uint32_t seqNo = ((TR::CompilationInfoPerThreadRemote*)(entry->_compInfoPT))->getSeqNo();
   TR_MethodToBeCompiled *crtEntry = getOOSequenceEntryList();
   TR_MethodToBeCompiled *prevEntry = NULL;
   while (crtEntry && seqNo > ((TR::CompilationInfoPerThreadRemote*)(crtEntry->_compInfoPT))->getSeqNo())
      {
      prevEntry = crtEntry;
      crtEntry = crtEntry->_next;
      }
   entry->_next = crtEntry;
   if (prevEntry)
      prevEntry->_next = entry;
   else
      _OOSequenceEntryList = entry;
   }

// notifyAndDetachFirstWaitingThread needs to be executed with sequencingMonitor in hand
TR_MethodToBeCompiled *
ClientSessionData::notifyAndDetachFirstWaitingThread()
   {
   TR_MethodToBeCompiled *entry = _OOSequenceEntryList;
   if (entry)
      {
      entry->getMonitor()->enter();
      entry->getMonitor()->notifyAll();
      entry->getMonitor()->exit();
      _OOSequenceEntryList = entry->_next;
      }
   return entry;
   }

// updateMaxReceivedSeqNo needs to be executed with sequencingMonitor in hand
void 
ClientSessionData::updateMaxReceivedSeqNo(uint32_t seqNo)
   {
   if (seqNo > _maxReceivedSeqNo)
      _maxReceivedSeqNo = seqNo;
   }


TR::CompilationInfoPerThreadRemote::CompilationInfoPerThreadRemote(TR::CompilationInfo &compInfo, J9JITConfig *jitConfig, int32_t id, bool isDiagnosticThread)
   : CompilationInfoPerThread(compInfo, jitConfig, id, isDiagnosticThread),
   _recompilationMethodInfo(NULL),
   _seqNo(0),
   _waitToBeNotified(false),
   _methodIPDataPerComp(NULL),
   _resolvedMethodInfoMap(NULL),
   _resolvedMirrorMethodsPersistIPInfo(NULL)
   {}

// waitForMyTurn needs to be executed with sequencingMonitor in hand
void
TR::CompilationInfoPerThreadRemote::waitForMyTurn(ClientSessionData *clientSession, TR_MethodToBeCompiled &entry)
   {
   TR_ASSERT(getMethodBeingCompiled() == &entry, "Must have stored the entry to be compiled into compInfoPT");
   TR_ASSERT(entry._compInfoPT == this, "Must have stored compInfoPT into the entry to be compiled");
   uint32_t seqNo = getSeqNo();

   // Insert this thread into the list of out of sequence entries
   clientSession->insertIntoOOSequenceEntryList(&entry);

   do // Do a timed wait until the missing seqNo arrives
      {
      // Always reset _waitToBeNotified before waiting on the monitor
      // If a notification does not arrive, this request will timeout and possibly clear the caches
      setWaitToBeNotified(false);

      entry.getMonitor()->enter();
      clientSession->getSequencingMonitor()->exit(); // release monitor before waiting
      const int64_t waitTimeMillis = 1000; // TODO: create an option for this
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d (entry=%p) doing a timed wait for %d ms",
         getCompThreadId(), &entry, (int32_t)waitTimeMillis);

      intptr_t monitorStatus = entry.getMonitor()->wait_timed(waitTimeMillis, 0); // 0 or J9THREAD_TIMED_OUT
      if (monitorStatus == 0) // thread was notified
         {
         entry.getMonitor()->exit();
         clientSession->getSequencingMonitor()->enter();

         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Parked compThreadID=%d (entry=%p) seqNo=%u was notified.",
               getCompThreadId(), &entry, seqNo);
         // will verify condition again to see if expectedSeqNo has advanced enough
         }
      else
         {
         entry.getMonitor()->exit();
         TR_ASSERT(monitorStatus == J9THREAD_TIMED_OUT, "Unexpected monitor state");
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompFailure, TR_VerboseJITServer, TR_VerbosePerformance))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d timed-out while waiting for seqNo=%d (entry=%p)",
               getCompThreadId(), clientSession->getExpectedSeqNo(), &entry);
        
         // The simplest thing is to delete the cached data for the session and start fresh
         // However, we must wait for any active threads to drain out
         clientSession->getSequencingMonitor()->enter(); 

         // This thread could have been waiting, then timed-out and then waited
         // to acquire the sequencing monitor. Check whether it can proceed.
         if (seqNo == clientSession->getExpectedSeqNo())
            {
            // The entry cannot be in the list
            TR_MethodToBeCompiled *headEntry = clientSession->getOOSequenceEntryList();
            if (headEntry)
               {
               uint32_t headSeqNo = ((TR::CompilationInfoPerThreadRemote*)(headEntry->_compInfoPT))->getSeqNo();
               TR_ASSERT_FATAL(seqNo < headSeqNo, "Next in line method cannot be in the waiting list: seqNo=%u >= headSeqNo=%u entry=%p headEntry=%p",
                  seqNo, headSeqNo, &entry, headEntry);
               }
            break; // it's my turn, so proceed
            }

         if (clientSession->getNumActiveThreads() <= 0 && // wait for active threads to quiesce
            &entry == clientSession->getOOSequenceEntryList() && // Allow only the smallest seqNo which is the head
            !getWaitToBeNotified()) // Avoid a cohort of threads clearing the caches
            {
            clientSession->clearCaches();
            TR_MethodToBeCompiled *detachedEntry = clientSession->notifyAndDetachFirstWaitingThread();
            clientSession->setExpectedSeqNo(seqNo);// allow myself to go through
            // Mark the next request that it should not try to clear the caches
            // but rather to sleep again waiting for my notification.
            TR_MethodToBeCompiled *nextWaitingEntry = clientSession->getOOSequenceEntryList();
            if (nextWaitingEntry)
               {
               ((TR::CompilationInfoPerThreadRemote*)(nextWaitingEntry->_compInfoPT))->setWaitToBeNotified(true);
               }
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "compThreadID=%d has cleared the session caches for clientUID=%llu expectedSeqNo=%u detachedEntry=%p",
                  getCompThreadId(), clientSession->getClientUID(), clientSession->getExpectedSeqNo(), detachedEntry);
            }
         else
            {
            // Wait until all active threads have been drained
            // and the head of the list has cleared the caches
            if (TR::Options::getVerboseOption(TR_VerboseJITServer))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "compThreadID=%d which previously timed-out will go to sleep again. Possible reasons numActiveThreads=%d waitToBeNotified=%d",
                  getCompThreadId(), clientSession->getNumActiveThreads(), getWaitToBeNotified());
            }
         }
      } while (seqNo > clientSession->getExpectedSeqNo());
   }

// Needs to be executed with clientSession->getSequencingMonitor() in hand
void
TR::CompilationInfoPerThreadRemote::updateSeqNo(ClientSessionData *clientSession)
   {
   uint32_t newSeqNo = clientSession->getExpectedSeqNo() + 1;
   clientSession->setExpectedSeqNo(newSeqNo);

   // Notify a possible waiting thread that arrived out of sequence
   // and take that entry out of the OOSequenceEntryList
   TR_MethodToBeCompiled *nextEntry = clientSession->getOOSequenceEntryList();
   if (nextEntry)
      {
      uint32_t nextWaitingSeqNo = ((CompilationInfoPerThreadRemote*)(nextEntry->_compInfoPT))->getSeqNo();
      if (nextWaitingSeqNo == newSeqNo)
         {
         clientSession->notifyAndDetachFirstWaitingThread();

         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d notifying out-of-sequence thread %d for clientUID=%llu seqNo=%u (entry=%p)",
               getCompThreadId(), nextEntry->_compInfoPT->getCompThreadId(), (unsigned long long)clientSession->getClientUID(), nextWaitingSeqNo, nextEntry);
         }
      }
   }

void
TR::CompilationInfoPerThreadRemote::processEntry(TR_MethodToBeCompiled &entry, J9::J9SegmentProvider &scratchSegmentProvider)
   {
   static bool enableJITaaSPerCompConn = feGetEnv("TR_EnableJITaaSPerCompConn") ? true : false;

   bool abortCompilation = false;
   uint64_t clientId = 0;
   TR::CompilationInfo *compInfo = getCompilationInfo();
   J9VMThread *compThread = getCompilationThread();
   JITServer::ServerStream *stream = entry._stream;
   setMethodBeingCompiled(&entry); // must have compilation monitor
   entry._compInfoPT = this; // create the reverse link
   // update the last time the compilation thread had to do something.
   compInfo->setLastReqStartTime(compInfo->getPersistentInfo()->getElapsedTime());
   clearPerCompilationCaches();

   _recompilationMethodInfo = NULL;
   // Release compMonitor before doing the blocking read
   compInfo->releaseCompMonitor(compThread);

   char * clientOptions = NULL;
   TR_OptimizationPlan *optPlan = NULL;
   _vm = NULL;
   bool useAotCompilation = false;
   uint32_t seqNo = 0;
   ClientSessionData *clientSession = NULL;
   try
      {
      auto req = stream->readCompileRequest<uint64_t, uint32_t, J9Method *, J9Class*, TR_OptimizationPlan, std::string,
         J9::IlGeneratorMethodDetailsType, std::vector<TR_OpaqueClassBlock*>,
         JITaaSHelpers::ClassInfoTuple, std::string, std::string, uint32_t, bool>();

      clientId                           = std::get<0>(req);
      uint32_t romMethodOffset           = std::get<1>(req);
      J9Method *ramMethod                = std::get<2>(req);
      J9Class *clazz                     = std::get<3>(req);
      TR_OptimizationPlan clientOptPlan  = std::get<4>(req);
      std::string detailsStr             = std::get<5>(req);
      auto detailsType                   = std::get<6>(req);
      auto &unloadedClasses              = std::get<7>(req);
      auto &classInfoTuple               = std::get<8>(req);
      std::string clientOptStr           = std::get<9>(req);
      std::string recompInfoStr          = std::get<10>(req);
      seqNo                              = std::get<11>(req); // sequence number at the client
      useAotCompilation                  = std::get<12>(req);

      if (useAotCompilation)
         {
         _vm = TR_J9VMBase::get(_jitConfig, compThread, TR_J9VMBase::J9_SHARED_CACHE_SERVER_VM);
         }
      else
         {
         _vm = TR_J9VMBase::get(_jitConfig, compThread, TR_J9VMBase::J9_SERVER_VM);
         }

      if (_vm->sharedCache())
         // Set/update stream pointer in shared cache.
         // Note that if remote-AOT is enabled, even regular J9_SERVER_VM will have a shared cache
         // This behaviour is consistent with non-JITaaS
         ((TR_J9JITaaSServerSharedCache *) _vm->sharedCache())->setStream(stream);

      //if (seqNo == 100)
      //   throw JITServer::StreamFailure(); // stress testing

      stream->setClientId(clientId);
      setSeqNo(seqNo); // memorize the sequence number of this request

      bool sessionDataWasEmpty = false;
      {
      // Get a pointer to this client's session data
      // Obtain monitor RAII style because creating a new hastable entry may throw bad_alloc
      OMR::CriticalSection compilationMonitorLock(compInfo->getCompilationMonitor());
      compInfo->getClientSessionHT()->purgeOldDataIfNeeded(); // Try to purge old data
      if (!(clientSession = compInfo->getClientSessionHT()->findOrCreateClientSession(clientId, seqNo, &sessionDataWasEmpty)))
         throw std::bad_alloc();

      setClientData(clientSession); // Cache the session data into CompilationInfoPerThreadRemote object
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d %s clientSessionData=%p for clientUID=%llu seqNo=%u",
            getCompThreadId(), sessionDataWasEmpty ? "created" : "found", clientSession, (unsigned long long)clientId, seqNo);
      } // end critical section

  
      // We must process unloaded classes lists in the same order they were generated at the client
      // Use a sequencing scheme to re-order compilation requests
      //
      clientSession->getSequencingMonitor()->enter();
      clientSession->updateMaxReceivedSeqNo(seqNo);
      if (seqNo > clientSession->getExpectedSeqNo()) // out of order messages
         {
         // park this request until the missing ones arrive
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Out-of-sequence msg detected for clientUID=%llu seqNo=%u > expectedSeqNo=%u. Parking compThreadID=%d (entry=%p)",
            (unsigned long long)clientId, seqNo, clientSession->getExpectedSeqNo(), getCompThreadId(), &entry);

         waitForMyTurn(clientSession, entry);
         }
      else if (seqNo < clientSession->getExpectedSeqNo())
         {
         // Note that it is possible for seqNo to be smaller than expectedSeqNo.
         // This could happen for instance for the very first message that arrives late.
         // In that case, the second message would have created the client session and 
         // written its own seqNo into expectedSeqNo. We should avoid processing stale updates
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompFailure, TR_VerboseJITServer, TR_VerbosePerformance))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Discarding older msg for clientUID=%llu seqNo=%u < expectedSeqNo=%u compThreadID=%d",
            (unsigned long long)clientId, seqNo, clientSession->getExpectedSeqNo(), getCompThreadId());
         clientSession->getSequencingMonitor()->exit();
         throw JITServer::StreamOOO();
         }
      // We can release the sequencing monitor now because nobody with a 
      // larger sequence number can pass until I increment expectedSeqNo
      clientSession->getSequencingMonitor()->exit();

      // At this point I know that all preceeding requests have been processed
      // Free data for all classes that were unloaded for this sequence number
      // Redefined classes are marked as unloaded, since they need to be cleared
      // from the ROM class cache.
      clientSession->processUnloadedClasses(stream, unloadedClasses); // this locks getROMMapMonitor()

      auto chTable = (TR_JITaaSServerPersistentCHTable*)compInfo->getPersistentInfo()->getPersistentCHTable();
      // TODO: is chTable always non-null?
      // TODO: we could send JVM info that is global and does not change together with CHTable
      // The following will acquire CHTable monitor and VMAccess, send a message and then release them
      bool initialized = chTable->initializeIfNeeded(_vm);

      // A thread that initialized the CHTable does not have to apply the incremental update
      if (!initialized)
         {
         // Process the CHTable updates in order
         stream->write(JITServer::MessageType::CHTable_getClassInfoUpdates, JITServer::Void());
         auto recv = stream->read<std::string, std::string>();
         const std::string &chtableUnloads = std::get<0>(recv);
         const std::string &chtableMods = std::get<1>(recv);
         // Note that applying the updates will acquire the CHTable monitor and VMAccess
         if (!chtableUnloads.empty() || !chtableMods.empty())
            chTable->doUpdate(_vm, chtableUnloads, chtableMods);
         }

      clientSession->getSequencingMonitor()->enter();
      // Update the expecting sequence number. This will allow subsequent
      // threads to pass through once we've released the sequencing monitor.
      TR_ASSERT(seqNo == clientSession->getExpectedSeqNo(), "Unexpected seqNo");
      updateSeqNo(clientSession);
      // Increment the number of active threads before issuing the read for ramClass
      clientSession->incNumActiveThreads();
      // Finally, release the sequencing monitor so that other threads
      // can process their lists of unloaded classes
      clientSession->getSequencingMonitor()->exit();

      // Copy the option strings
      size_t clientOptSize = clientOptStr.size();
      clientOptions = new (PERSISTENT_NEW) char[clientOptSize];
      memcpy(clientOptions, clientOptStr.data(), clientOptSize);

      if (recompInfoStr.size() > 0)
         {
         _recompilationMethodInfo = new (PERSISTENT_NEW) TR_PersistentMethodInfo();
         memcpy(_recompilationMethodInfo, recompInfoStr.data(), sizeof(TR_PersistentMethodInfo));
         }
      // Get the ROMClass for the method to be compiled if it is already cached
      // Or read it from the compilation request and cache it otherwise
      J9ROMClass *romClass = NULL;
      if (!(romClass = JITaaSHelpers::getRemoteROMClassIfCached(clientSession, clazz)))
         {
         romClass = JITaaSHelpers::romClassFromString(std::get<0>(classInfoTuple), compInfo->persistentMemory());
         JITaaSHelpers::cacheRemoteROMClass(getClientData(), clazz, romClass, &classInfoTuple);
         }

      J9ROMMethod *romMethod = (J9ROMMethod*)((uint8_t*)romClass + romMethodOffset);
      J9Method *methodsOfClass = std::get<1>(classInfoTuple);

      // Build my entry
      if (!(optPlan = TR_OptimizationPlan::alloc(clientOptPlan.getOptLevel())))
         throw std::bad_alloc();
      optPlan->clone(&clientOptPlan);

      TR::IlGeneratorMethodDetails *clientDetails = (TR::IlGeneratorMethodDetails*) &detailsStr[0];
      *(uintptr_t*)clientDetails = 0; // smash remote vtable pointer to catch bugs early
      TR::IlGeneratorMethodDetails details;
      TR::IlGeneratorMethodDetails &remoteDetails = details.createRemoteMethodDetails(*clientDetails, detailsType, ramMethod, romClass, romMethod, clazz, methodsOfClass);

      // All entries have the same priority for now. In the future we may want to give higher priority to sync requests
      // Also, oldStartPC is always NULL for JITaaS server
      entry._freeTag = ENTRY_IN_POOL_FREE; // pretend we just got it from the pool because we need to initialize it again
      entry.initialize(remoteDetails, NULL, CP_SYNC_NORMAL, optPlan);
      entry._jitStateWhenQueued = compInfo->getPersistentInfo()->getJitState();
      entry._stream = stream; // Add the stream to the entry
      entry._clientOptions = clientOptions;
      entry._clientOptionsSize = clientOptSize;
      entry._entryTime = compInfo->getPersistentInfo()->getElapsedTime(); // cheaper version
      entry._methodIsInSharedCache = false; // no SCC for now in JITaaS
      entry._compInfoPT = this; // need to know which comp thread is handling this request
      entry._async = true; // all of requests at the server are async
      // weight is irrelevant for JITaaS. 
      // If we want something then we need to increaseQueueWeightBy(weight) while holding compilation monitor
      entry._weight = 0;
      entry._useAotCompilation = useAotCompilation;
      }
   catch (const JITServer::StreamFailure &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Stream failed while compThreadID=%d was reading the compilation request: %s",
            getCompThreadId(), e.what());

      abortCompilation = true;
      if (!enableJITaaSPerCompConn)
         {
         // Delete server stream
         stream->~ServerStream();
         TR_Memory::jitPersistentFree(stream);
         entry._stream = NULL;
         }
      }
   catch (const JITServer::StreamVersionIncompatible &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Stream version incompatible: %s", e.what());

      stream->writeError(compilationStreamVersionIncompatible);
      abortCompilation = true;
      if (!enableJITaaSPerCompConn)
         {
         // Delete server stream
         stream->~ServerStream();
         TR_Memory::jitPersistentFree(stream);
         entry._stream = NULL;
         }
      }
   catch (const JITServer::StreamMessageTypeMismatch &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Stream message type mismatch: %s", e.what());

      stream->writeError(compilationStreamMessageTypeMismatch);
      abortCompilation = true;
      }
   catch (const JITServer::StreamConnectionTerminate &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Stream connection terminated by JITClient on stream %p while compThreadID=%d was reading the compilation request: %s",
            stream, getCompThreadId(), e.what());

      abortCompilation = true;
      if (!enableJITaaSPerCompConn) // JITServer TODO: remove the perCompConn mode
         {
         stream->~ServerStream();
         TR_Memory::jitPersistentFree(stream);
         entry._stream = NULL;
         }
      }
   catch (const JITServer::StreamClientSessionTerminate &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Stream client session terminated by JITClient on compThreadID=%d: %s", getCompThreadId(), e.what());

      abortCompilation = true;
      deleteClientSessionData(e.getClientId(), compInfo, compThread);

      if (!enableJITaaSPerCompConn)
         {
         stream->~ServerStream();
         TR_Memory::jitPersistentFree(stream);
         entry._stream = NULL;
         }
      }
   catch (const JITServer::StreamInterrupted &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Stream interrupted by JITClient while compThreadID=%d was reading the compilation request: %s",
            getCompThreadId(), e.what());

      abortCompilation = true;
      // If the client aborted this compilation it could have happened only while
      // asking for CHTable updates and at that point the seqNo was not updated.
      // We must update it now to allow for blocking threads to pass through.
      clientSession->getSequencingMonitor()->enter();
      TR_ASSERT(seqNo == clientSession->getExpectedSeqNo(), "Unexpected seqNo");
      updateSeqNo(clientSession);
      // Release the sequencing monitor so that other threads can process their lists of unloaded classes
      clientSession->getSequencingMonitor()->exit();
      }
   catch (const JITServer::StreamOOO &e)
      {
      // Error message was printed when the exception was thrown
      // TODO: the client should handle this error code
      stream->writeError(compilationStreamLostMessage); // the client should recognize this code and retry
      abortCompilation = true;
      }
   catch (const std::bad_alloc &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Server out of memory in processEntry: %s", e.what());
      stream->writeError(compilationLowPhysicalMemory);
      abortCompilation = true;
      }

   // Acquire VM access
   //
   acquireVMAccessNoSuspend(compThread);

   if (abortCompilation)
      {
      if (clientOptions)
         TR_Memory::jitPersistentFree(clientOptions);
      if (optPlan)
         TR_OptimizationPlan::freeOptimizationPlan(optPlan);
      if (_recompilationMethodInfo)
         TR_Memory::jitPersistentFree(_recompilationMethodInfo);

      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         {
         if (getClientData())
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d did an early abort for clientUID=%llu seqNo=%u",
               getCompThreadId(), getClientData()->getClientUID(), getSeqNo());
         else
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "compThreadID=%d did an early abort",
               getCompThreadId());
         }

      compInfo->acquireCompMonitor(compThread);
      releaseVMAccess(compThread);
      compInfo->decreaseQueueWeightBy(entry._weight);

      // Put the request back into the pool
      setMethodBeingCompiled(NULL); // Must have the compQmonitor

      compInfo->requeueOutOfProcessEntry(&entry);

      // Reset the pointer to the cached client session data
      if (getClientData())
         {
         getClientData()->decInUse();  // We have the compilation monitor so it's safe to access the inUse counter
         if (getClientData()->getInUse() == 0)
            {
            bool result = compInfo->getClientSessionHT()->deleteClientSession(clientId, false);
            if (result)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,"client (%llu) deleted", (unsigned long long)clientId);
               }
            }
         setClientData(NULL);
         }
      if (enableJITaaSPerCompConn)
         {
         // Delete server stream
         stream->~ServerStream();
         TR_Memory::jitPersistentFree(stream);
         entry._stream = NULL;
         }
      return;
      }

   // Do the hack for newInstance thunks
   // Also make the method appear as interpreted, otherwise we might want to access recompilation info
   // JITaaS TODO: is this ever executed for JITaaS?
   //if (entry.getMethodDetails().isNewInstanceThunk())
   //   {
   //   J9::NewInstanceThunkDetails &newInstanceDetails = static_cast<J9::NewInstanceThunkDetails &>(entry.getMethodDetails());
   //   J9Class  *classForNewInstance = newInstanceDetails.classNeedingThunk();
   //   TR::CompilationInfo::setJ9MethodExtra(ramMethod, (uintptrj_t)classForNewInstance | J9_STARTPC_NOT_TRANSLATED);
   //   }
#ifdef STATS
   statQueueSize.update(compInfo->getMethodQueueSize());
#endif
   // The following call will return with compilation monitor in hand
   //
   void *startPC = compile(compThread, &entry, scratchSegmentProvider);
   if (entry._compErrCode == compilationStreamFailure)
      {
      if (!enableJITaaSPerCompConn)
         {
         TR_ASSERT(entry._stream, "stream should still exist after compilation even if it encounters a streamFailure.");
         // Clean up server stream because the stream is already dead
         stream->~ServerStream();
         TR_Memory::jitPersistentFree(stream);
         entry._stream = NULL;
         }
      }

   // Release the queue slot monitor because we don't need it anymore
   // This will allow us to acquire the sequencing monitor later
   entry.releaseSlotMonitor(compThread);

   // Decrement number of active threads before _inUse, but we
   // need to acquire the sequencing monitor when accessing numActiveThreads
   getClientData()->getSequencingMonitor()->enter();
   getClientData()->decNumActiveThreads(); 
   getClientData()->getSequencingMonitor()->exit();
   getClientData()->decInUse();  // We have the compMonitor so it's safe to access the inUse counter
   if (getClientData()->getInUse() == 0)
      {
      bool deleted = compInfo->getClientSessionHT()->deleteClientSession(clientId, false);
      if (deleted && TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,"client (%llu) deleted", (unsigned long long)clientId);
      }

   setClientData(NULL); // Reset the pointer to the cached client session data

   entry._newStartPC = startPC;
   // Update statistics regarding the compilation status (including compilationOK)
   compInfo->updateCompilationErrorStats((TR_CompilationErrorCode)entry._compErrCode);

   TR_OptimizationPlan::freeOptimizationPlan(entry._optimizationPlan); // we no longer need the optimization plan
   // decrease the queue weight
   compInfo->decreaseQueueWeightBy(entry._weight);
   // Put the request back into the pool
   setMethodBeingCompiled(NULL);
   compInfo->requeueOutOfProcessEntry(&entry);
   compInfo->printQueue();


   // At this point we should always have VMAccess
   // We should always have the compilation monitor
   // we should never have classUnloadMonitor

   // Release VM access
   //
   compInfo->debugPrint(compThread, "\tcompilation thread releasing VM access\n");
   releaseVMAccess(compThread);
   compInfo->debugPrint(compThread, "-VMacc\n");

   // We can suspend this thread if too many are active
   if (
      !(isDiagnosticThread()) // must not be reserved for log
      && compInfo->getNumCompThreadsActive() > 1 // we should have at least one active besides this one
      && compilationThreadIsActive() // We haven't already been signaled to suspend or terminate
      && (compInfo->getRampDownMCT() || compInfo->getSuspendThreadDueToLowPhysicalMemory())
      )
      {
      // Suspend this thread
      setCompilationThreadState(COMPTHREAD_SIGNAL_SUSPEND);
      compInfo->decNumCompThreadsActive();
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Suspend compThread %d Qweight=%d active=%d %s %s",
            (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
            getCompThreadId(),
            compInfo->getQueueWeight(),
            compInfo->getNumCompThreadsActive(),
            compInfo->getRampDownMCT() ? "RampDownMCT" : "",
            compInfo->getSuspendThreadDueToLowPhysicalMemory() ? "LowPhysicalMem" : "");
         }
      // If the other remaining active thread(s) are sleeping (maybe because
      // we wanted to avoid two concurrent hot requests) we need to wake them
      // now as a preventive measure. Worst case scenario they will go back to sleep
      if (compInfo->getNumCompThreadsJobless() > 0)
         {
         compInfo->getCompilationMonitor()->notifyAll();
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u compThread %d notifying other sleeping comp threads. Jobless=%d",
               (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
               getCompThreadId(),
               compInfo->getNumCompThreadsJobless());
            }
         }
      }
   else // We will not suspend this thread
      {
      // If the low memory flag was set but there was no additional comp thread
      // to suspend, we must clear the flag now
      if (compInfo->getSuspendThreadDueToLowPhysicalMemory() &&
         compInfo->getNumCompThreadsActive() < 2)
         compInfo->setSuspendThreadDueToLowPhysicalMemory(false);
      }

   
   if (enableJITaaSPerCompConn)
      {
      // Delete server stream
      stream->~ServerStream();
      TR_Memory::jitPersistentFree(stream);
      entry._stream = NULL;
      }
   }

bool
TR::CompilationInfoPerThreadRemote::cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry)
   {
   IPTableHeapEntry *entryMap = NULL;
   if(!getCachedValueFromPerCompilationMap(_methodIPDataPerComp, (J9Method *) method, entryMap))
      {
      // Either first time cacheIProfilerInfo called during current compilation,
      // so _methodIPDataPerComp is NULL, or caching a new method, entryMap not found
      if (entry)
         {
         cacheToPerCompilationMap(entryMap, byteCodeIndex, entry);
         }
      else
         {
         // if entry is NULL, create entryMap, but do not cache anything
         initializePerCompilationCache(entryMap);
         }
      cacheToPerCompilationMap(_methodIPDataPerComp, (J9Method *) method, entryMap); 
      }
   else if (entry)
      {
      // Adding an entry for already seen method (i.e. entryMap exists)
      cacheToPerCompilationMap(entryMap, byteCodeIndex, entry);
      }
   return true;
   }

TR_IPBytecodeHashTableEntry*
TR::CompilationInfoPerThreadRemote::getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent)
   {
   *methodInfoPresent = false;

   IPTableHeapEntry *entryMap = NULL;
   TR_IPBytecodeHashTableEntry *ipEntry = NULL;
   
   getCachedValueFromPerCompilationMap(_methodIPDataPerComp, (J9Method *) method, entryMap);
   // methodInfoPresent=true means that we cached info for this method (i.e. entryMap is not NULL),
   // but entry for this bytecode might not exist, which means it's NULL
   if (entryMap)
      {
      *methodInfoPresent = true;
      getCachedValueFromPerCompilationMap(entryMap, byteCodeIndex, ipEntry);
      }
   return ipEntry;
   }

void
TR::CompilationInfoPerThreadRemote::cacheResolvedMethod(TR_ResolvedMethodKey key, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo)
   {
   static bool useCaching = !feGetEnv("TR_DisableResolvedMethodsCaching");
   if (!useCaching)
      return;

   cacheToPerCompilationMap(_resolvedMethodInfoMap, key, {method, vTableSlot, methodInfo});
   }

// retrieve a resolved method from the cache
// returns true if the method is cached, sets resolvedMethod and unresolvedInCP to the cached values.
// returns false otherwise.
bool 
TR::CompilationInfoPerThreadRemote::getCachedResolvedMethod(TR_ResolvedMethodKey key, TR_ResolvedJ9JITaaSServerMethod *owningMethod, TR_ResolvedMethod **resolvedMethod, bool *unresolvedInCP)
   {
   TR_ResolvedMethodCacheEntry methodCacheEntry;
   if (getCachedValueFromPerCompilationMap(_resolvedMethodInfoMap, key, methodCacheEntry))
      {
      auto comp = getCompilation();
      TR_OpaqueMethodBlock *method = methodCacheEntry.method;
      if (!method)
         {
         *resolvedMethod = NULL;
         return true;
         }
      auto methodInfo = methodCacheEntry.methodInfo;
      uint32_t vTableSlot = methodCacheEntry.vTableSlot;


      // Re-add validation record
      if (comp->compileRelocatableCode() && comp->getOption(TR_UseSymbolValidationManager) && !comp->getSymbolValidationManager()->inHeuristicRegion())
         {
         if(!owningMethod->addValidationRecordForCachedResolvedMethod(key, method))
            {
            // Could not add a validation record
            *resolvedMethod = NULL;
            if (unresolvedInCP)
               *unresolvedInCP = true;
            return true;
            }
         }
      // Create resolved method from cached method info
      if (key.type != TR_ResolvedMethodType::VirtualFromOffset)
         {
         *resolvedMethod = owningMethod->createResolvedMethodFromJ9Method(
                              comp,
                              key.cpIndex,
                              vTableSlot,
                              (J9Method *) method,
                              unresolvedInCP,
                              NULL,
                              methodInfo);
         }
      else
         {
         if (_vm->isAOT_DEPRECATED_DO_NOT_USE())
            *resolvedMethod = method ? new (comp->trHeapMemory()) TR_ResolvedRelocatableJ9JITaaSServerMethod(method, _vm, comp->trMemory(), methodInfo, owningMethod) : 0;
         else
            *resolvedMethod = method ? new (comp->trHeapMemory()) TR_ResolvedJ9JITaaSServerMethod(method, _vm, comp->trMemory(), methodInfo, owningMethod) : 0;
         }
         
      if (*resolvedMethod)
         {
         if (unresolvedInCP)
            *unresolvedInCP = false;
         return true;
         }
      else
         {
         TR_ASSERT(false, "Should not have cached unresolved method globally");
         }
      }
   return false;
   }

TR_ResolvedMethodKey
TR::CompilationInfoPerThreadRemote::getResolvedMethodKey(TR_ResolvedMethodType type, TR_OpaqueClassBlock *ramClass, int32_t cpIndex, TR_OpaqueClassBlock *classObject)
   {
   TR_ResolvedMethodKey key = {type, ramClass, cpIndex, classObject};
   return key;
   }

void
TR::CompilationInfoPerThreadRemote::cacheResolvedMirrorMethodsPersistIPInfo(TR_ResolvedJ9Method *resolvedMethod)
   {
   if (!_resolvedMirrorMethodsPersistIPInfo)
      {
      initializePerCompilationCache(_resolvedMirrorMethodsPersistIPInfo);
      if (!_resolvedMirrorMethodsPersistIPInfo)
         return;
      }

   _resolvedMirrorMethodsPersistIPInfo->push_back(resolvedMethod);
   }

void
TR::CompilationInfoPerThreadRemote::cacheNullClassOfStatic(TR_OpaqueClassBlock *ramClass, int32_t cpIndex)
   {
   TR_OpaqueClassBlock *nullClazz = NULL;
   cacheToPerCompilationMap(_classOfStaticMap, std::make_pair(ramClass, cpIndex), nullClazz);
   }

bool
TR::CompilationInfoPerThreadRemote::getCachedNullClassOfStatic(TR_OpaqueClassBlock *ramClass, int32_t cpIndex)
   {
   TR_OpaqueClassBlock *nullClazz;
   return getCachedValueFromPerCompilationMap(_classOfStaticMap, std::make_pair(ramClass, cpIndex), nullClazz);
   }

void
TR::CompilationInfoPerThreadRemote::clearPerCompilationCaches()
   {
   clearPerCompilationCache(_methodIPDataPerComp);
   clearPerCompilationCache(_resolvedMethodInfoMap);
   clearPerCompilationCache(_resolvedMirrorMethodsPersistIPInfo);
   clearPerCompilationCache(_classOfStaticMap);
   }

void
TR::CompilationInfoPerThreadRemote::deleteClientSessionData(uint64_t clientId, TR::CompilationInfo* compInfo, J9VMThread* compThread)
   {
   compInfo->acquireCompMonitor(compThread); //need to acquire compilation monitor for both deleting the client data and the setting the thread state to COMPTHREAD_SIGNAL_SUSPEND.
   bool result = compInfo->getClientSessionHT()->deleteClientSession(clientId, true);
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      {
      if (!result)
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,"Message to delete Client (%llu) received. Data for client not deleted", (unsigned long long)clientId);
         }
      else
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,"Message to delete Client (%llu) received. Data for client deleted", (unsigned long long)clientId);
         }
      }
   compInfo->releaseCompMonitor(compThread);
   }
