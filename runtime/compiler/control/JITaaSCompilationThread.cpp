/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

uint32_t serverMsgTypeCount[JITaaS::J9ServerMessageType_ARRAYSIZE] = {};

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

void handler_IProfiler_profilingSample(JITaaS::J9ClientStream *client, TR_J9VM *fe, TR::Compilation *comp)
   {
   auto recv = client->getRecvData<TR_OpaqueMethodBlock*, uint32_t, uintptrj_t>();
   auto method = std::get<0>(recv);
   auto bcIndex = std::get<1>(recv);
   auto data = std::get<2>(recv); // data==1 means 'send info for 1 bytecode'; data==0 means 'send info for entire method if possible'

   TR_JITaaSClientIProfiler *iProfiler = (TR_JITaaSClientIProfiler *)fe->getIProfiler();

   bool isCompiled = TR::CompilationInfo::isCompiled((J9Method*)method);
   bool isInProgress = comp->getMethodBeingCompiled()->getPersistentIdentifier() == method;
   bool wholeMethodInfo = (isCompiled || isInProgress) && (data == 0);

   if (!iProfiler)
      {
      // iProfiler is enabled on the server if we got here, but not enabled here on the client
      client->write(std::string(), wholeMethodInfo);
      return;
      }
   if (wholeMethodInfo)
      {
      // Serialize all the information related to this method
      iProfiler->serializeAndSendIProfileInfoForMethod(method, comp, client);
      }
   else  // Send information just for this entry
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
            client->write(entryBytes, wholeMethodInfo);
            }
         else
            {
            client->write(std::string(), wholeMethodInfo);
            }
         // unlock the entry
         if (auto callGraphEntry = entry->asIPBCDataCallGraph())
            if (canPersist != IPBC_ENTRY_PERSIST_LOCK && callGraphEntry->isLocked())
               callGraphEntry->releaseEntry();
         }
      else // no valid info for specified bytecode index
         {
         client->write(std::string(), wholeMethodInfo);
         }
      }
   }

bool handleServerMessage(JITaaS::J9ClientStream *client, TR_J9VM *fe)
   {
   using JITaaS::J9ServerMessageType;
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

   auto response = client->read();


   // re-acquire VM access and check for possible class unloading
   acquireVMAccessNoSuspend(vmThread);

   // If JVM has unloaded classes inform the server to abort this compilation
   if (compInfoPT->compilationShouldBeInterrupted())
      {
      if (response != J9ServerMessageType::compilationCode)
         client->writeError(); // inform the server if compilation is not yet complete

      if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
         TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH,
            "Interrupting remote compilation of %s @ %s", comp->signature(), comp->getHotnessName());

      comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted in handleServerMessage");
      }

   // update statistics for server message type
   serverMsgTypeCount[response] += 1;

   bool done = false;
   switch (response)
      {
      case J9ServerMessageType::compilationCode:
         done = true;
         break;

      case J9ServerMessageType::VM_isClassLibraryClass:
         {
         bool rv = fe->isClassLibraryClass(std::get<0>(client->getRecvData<TR_OpaqueClassBlock*>()));
         client->write(rv);
         }
         break;
      case J9ServerMessageType::VM_isClassLibraryMethod:
         {
         auto tup = client->getRecvData<TR_OpaqueMethodBlock*, bool>();
         bool rv = fe->isClassLibraryMethod(std::get<0>(tup), std::get<1>(tup));
         client->write(rv);
         }
         break;
      case J9ServerMessageType::VM_getSuperClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getSuperClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_isInstanceOf:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, TR_OpaqueClassBlock *, bool, bool, bool>();
         TR_OpaqueClassBlock *a = std::get<0>(recv);
         TR_OpaqueClassBlock *b = std::get<1>(recv);
         bool objectTypeIsFixed = std::get<2>(recv);
         bool castTypeIsFixed = std::get<3>(recv);
         bool optimizeForAOT = std::get<4>(recv);
         client->write(fe->isInstanceOf(a, b, objectTypeIsFixed, castTypeIsFixed, optimizeForAOT));
         }
         break;
      case J9ServerMessageType::VM_getSystemClassFromClassName:
         {
         auto recv = client->getRecvData<std::string, bool>();
         const std::string name = std::get<0>(recv);
         bool isVettedForAOT = std::get<1>(recv);
         client->write(fe->getSystemClassFromClassName(name.c_str(), name.length(), isVettedForAOT));
         }
         break;
      case J9ServerMessageType::VM_isMethodEnterTracingEnabled:
         {
         auto method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(fe->isMethodEnterTracingEnabled(method));
         }
         break;
      case J9ServerMessageType::VM_isMethodExitTracingEnabled:
         {
         auto method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(fe->isMethodExitTracingEnabled(method));
         }
         break;
      case J9ServerMessageType::VM_getClassClassPointer:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock*>());
         client->write(fe->getClassClassPointer(clazz));
         }
         break;
      case J9ServerMessageType::VM_getClassOfMethod:
         {
         auto method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock*>());
         client->write(fe->getClassOfMethod(method));
         }
         break;
      case J9ServerMessageType::VM_getBaseComponentClass:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock*, int32_t>();
         auto clazz = std::get<0>(recv);
         int32_t numDims = std::get<1>(recv);
         auto outClass = fe->getBaseComponentClass(clazz, numDims);
         client->write(outClass, numDims);
         };
         break;
      case J9ServerMessageType::VM_getLeafComponentClassFromArrayClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock*>());
         client->write(fe->getLeafComponentClassFromArrayClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_getClassFromSignature:
         {
         auto recv = client->getRecvData<std::string, TR_OpaqueMethodBlock *, bool>();
         std::string sig = std::get<0>(recv);
         auto method = std::get<1>(recv);
         bool isVettedForAOT = std::get<2>(recv);
         auto clazz = fe->getClassFromSignature(sig.c_str(), sig.length(), method, isVettedForAOT);
         client->write(clazz);
         }
         break;
      case J9ServerMessageType::VM_jitFieldsAreSame:
         {
         auto recv = client->getRecvData<TR_ResolvedMethod*, int32_t, TR_ResolvedMethod *, int32_t, int32_t>();
         TR_ResolvedMethod *method1 = std::get<0>(recv);
         int32_t cpIndex1 = std::get<1>(recv);
         TR_ResolvedMethod *method2 = std::get<2>(recv);
         int32_t cpIndex2 = std::get<3>(recv);
         int32_t isStatic = std::get<4>(recv);
         client->write(fe->jitFieldsAreSame(method1, cpIndex1, method2, cpIndex2, isStatic));
         };
         break;
      case J9ServerMessageType::VM_jitStaticsAreSame:
         {
         auto recv = client->getRecvData<TR_ResolvedMethod*, int32_t, TR_ResolvedMethod *, int32_t>();
         TR_ResolvedMethod *method1 = std::get<0>(recv);
         int32_t cpIndex1 = std::get<1>(recv);
         TR_ResolvedMethod *method2 = std::get<2>(recv);
         int32_t cpIndex2 = std::get<3>(recv);
         client->write(fe->jitStaticsAreSame(method1, cpIndex1, method2, cpIndex2));
         };
         break;
      case J9ServerMessageType::VM_compiledAsDLTBefore:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_ResolvedMethod *>());
         client->write(fe->compiledAsDLTBefore(clazz));
         }
         break;
      case J9ServerMessageType::VM_getComponentClassFromArrayClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getComponentClassFromArrayClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_classHasBeenReplaced:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->classHasBeenReplaced(clazz));
         }
         break;
      case J9ServerMessageType::VM_classHasBeenExtended:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->classHasBeenExtended(clazz));
         }
         break;
      case J9ServerMessageType::VM_isThunkArchetype:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(fe->isThunkArchetype(method));
         }
         break;
      case J9ServerMessageType::VM_printTruncatedSignature:
         {
         J9Method *method = (J9Method *) std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         J9UTF8 * className;
         J9UTF8 * name;
         J9UTF8 * signature;
         getClassNameSignatureFromMethod(method, className, name, signature);
         std::string classNameStr(utf8Data(className), J9UTF8_LENGTH(className));
         std::string nameStr(utf8Data(name), J9UTF8_LENGTH(name));
         std::string signatureStr(utf8Data(signature), J9UTF8_LENGTH(signature));
         client->write(classNameStr, nameStr, signatureStr);
         }
         break;
      case J9ServerMessageType::VM_getStaticHookAddress:
         {
         int32_t event = std::get<0>(client->getRecvData<int32_t>());
         client->write(fe->getStaticHookAddress(event));
         }
         break;
      case J9ServerMessageType::VM_isClassInitialized:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->isClassInitialized(clazz));
         }
         break;
      case J9ServerMessageType::VM_getOSRFrameSizeInBytes:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(fe->getOSRFrameSizeInBytes(method));
         }
         break;
      case J9ServerMessageType::VM_getByteOffsetToLockword:
         {
         TR_OpaqueClassBlock * clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getByteOffsetToLockword(clazz));
         }
         break;
      case J9ServerMessageType::VM_isString1:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->isString(clazz));
         }
         break;
      case J9ServerMessageType::VM_getMethods:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getMethods(clazz));
         }
         break;
      case J9ServerMessageType::VM_getResolvedMethodsAndMirror:
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
            TR_ResolvedJ9JITaaSServerMethod::createResolvedJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) &(methods[i]), 0, 0, fe, trMemory);
            methodsInfo.push_back(methodInfo);
            }
         client->write(methods, methodsInfo);
         }
         break;
      case J9ServerMessageType::VM_getVMInfo:
         {
         ClientSessionData::VMInfo vmInfo;
         vmInfo._systemClassLoader = fe->getSystemClassLoader();
         vmInfo._processID = fe->getProcessID();
         vmInfo._canMethodEnterEventBeHooked = fe->canMethodEnterEventBeHooked();
         vmInfo._canMethodExitEventBeHooked = fe->canMethodExitEventBeHooked();
         vmInfo._usesDiscontiguousArraylets = TR::Compiler->om.usesDiscontiguousArraylets();
         vmInfo._arrayletLeafLogSize = TR::Compiler->om.arrayletLeafLogSize();
         vmInfo._arrayletLeafSize = TR::Compiler->om.arrayletLeafSize();
         vmInfo._overflowSafeAllocSize = static_cast<uint64_t>(fe->getOverflowSafeAllocSize());

         client->write(vmInfo);
         }
         break;
      case J9ServerMessageType::VM_isPrimitiveArray:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->isPrimitiveArray(clazz));
         }
         break;
      case J9ServerMessageType::VM_getAllocationSize:
         {
         auto recv = client->getRecvData<TR::StaticSymbol *, TR_OpaqueClassBlock *>();
         TR::StaticSymbol *classSym = std::get<0>(recv);
         TR_OpaqueClassBlock *clazz = std::get<1>(recv);
         client->write(fe->getAllocationSize(classSym, clazz));
         }
         break;
      case J9ServerMessageType::VM_getObjectClass:
         {
         TR::VMAccessCriticalSection getObjectClass(fe);
         uintptrj_t objectPointer = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(fe->getObjectClass(objectPointer));
         }
         break;
      case J9ServerMessageType::VM_getStaticReferenceFieldAtAddress:
         {
         TR::VMAccessCriticalSection getStaticReferenceFieldAtAddress(fe);
         uintptrj_t fieldAddress = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(fe->getStaticReferenceFieldAtAddress(fieldAddress));
         }
         break;
      case J9ServerMessageType::VM_stackWalkerMaySkipFrames:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *>();
         TR_OpaqueMethodBlock *method = std::get<0>(recv);
         TR_OpaqueClassBlock *clazz = std::get<1>(recv);
         client->write(fe->stackWalkerMaySkipFrames(method, clazz));
         }
         break;
      case J9ServerMessageType::VM_hasFinalFieldsInClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->hasFinalFieldsInClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_getClassNameSignatureFromMethod:
         {
         auto recv = client->getRecvData<J9Method*>();
         J9Method *method = std::get<0>(recv);
         J9UTF8 *className, *name, *signature;
         getClassNameSignatureFromMethod(method, className, name, signature);
         std::string classNameStr(utf8Data(className), J9UTF8_LENGTH(className));
         std::string nameStr(utf8Data(name), J9UTF8_LENGTH(name));
         std::string signatureStr(utf8Data(signature), J9UTF8_LENGTH(signature));
         client->write(classNameStr, nameStr, signatureStr);
         }
         break;
      case J9ServerMessageType::VM_getHostClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getHostClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_getStringUTF8Length:
         {
         uintptrj_t string = std::get<0>(client->getRecvData<uintptrj_t>());
            {
            TR::VMAccessCriticalSection getStringUTF8Length(fe);
            client->write(fe->getStringUTF8Length(string));
            }
         }
         break;
      case J9ServerMessageType::VM_classInitIsFinished:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->classInitIsFinished(clazz));
         }
         break;
      case J9ServerMessageType::VM_getNewArrayTypeFromClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getNewArrayTypeFromClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_getClassFromNewArrayType:
         {
         int32_t index = std::get<0>(client->getRecvData<int32_t>());
         client->write(fe->getClassFromNewArrayType(index));
         }
         break;
      case J9ServerMessageType::VM_isCloneable:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->isCloneable(clazz));
         }
         break;
      case J9ServerMessageType::VM_canAllocateInlineClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->canAllocateInlineClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_getArrayClassFromComponentClass:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getArrayClassFromComponentClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_matchRAMclassFromROMclass:
         {
         J9ROMClass *clazz = std::get<0>(client->getRecvData<J9ROMClass *>());
         client->write(fe->matchRAMclassFromROMclass(clazz, comp));
         }
         break;
      case J9ServerMessageType::VM_getReferenceFieldAtAddress:
         {
         TR::VMAccessCriticalSection getReferenceFieldAtAddress(fe);
         uintptrj_t fieldAddress = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(fe->getReferenceFieldAtAddress(fieldAddress));
         }
         break;
      case J9ServerMessageType::VM_getVolatileReferenceFieldAt:
         {
         auto recv = client->getRecvData<uintptrj_t, uintptrj_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         client->write(fe->getVolatileReferenceFieldAt(objectPointer, fieldOffset));
         }
         break;
      case J9ServerMessageType::VM_getInt32FieldAt:
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
         client->write(fe->getInt32FieldAt(objectPointer, fieldOffset));
         }
         break;
      case J9ServerMessageType::VM_getInt64FieldAt:
         {
         auto recv = client->getRecvData<uintptrj_t, uintptrj_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         client->write(fe->getInt64FieldAt(objectPointer, fieldOffset));
         }
         break;
      case J9ServerMessageType::VM_setInt64FieldAt:
         {
         auto recv = client->getRecvData<uintptrj_t, uintptrj_t, int64_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         int64_t newValue = std::get<2>(recv);
         fe->setInt64FieldAt(objectPointer, fieldOffset, newValue);
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::VM_compareAndSwapInt64FieldAt:
         {
         auto recv = client->getRecvData<uintptrj_t, uintptrj_t, int64_t, int64_t>();
         uintptrj_t objectPointer = std::get<0>(recv);
         uintptrj_t fieldOffset = std::get<1>(recv);
         int64_t oldValue = std::get<2>(recv);
         int64_t newValue = std::get<3>(recv);
         client->write(fe->compareAndSwapInt64FieldAt(objectPointer, fieldOffset, oldValue, newValue));
         }
         break;
      case J9ServerMessageType::VM_getArrayLengthInElements:
         {
         if (compInfoPT->getLastLocalGCCounter() != compInfoPT->getCompilationInfo()->getLocalGCCounter())
            {
            // GC happened, fail compilation
            auto comp = compInfoPT->getCompilation();
            comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted due to GC");
            }

         uintptrj_t objectPointer = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(fe->getArrayLengthInElements(objectPointer));
         }
         break;
      case J9ServerMessageType::VM_getClassFromJavaLangClass:
         {
         uintptrj_t objectPointer = std::get<0>(client->getRecvData<uintptrj_t>());
         client->write(fe->getClassFromJavaLangClass(objectPointer));
         }
         break;
      case J9ServerMessageType::VM_getOffsetOfClassFromJavaLangClassField:
         {
         client->getRecvData<JITaaS::Void>();
         client->write(fe->getOffsetOfClassFromJavaLangClassField());
         }
         break;
      case J9ServerMessageType::VM_getConstantPoolFromMethod:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(fe->getConstantPoolFromMethod(method));
         }
         break;
      case J9ServerMessageType::VM_getConstantPoolFromClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getConstantPoolFromClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_getIdentityHashSaltPolicy:
         {
         client->getRecvData<JITaaS::Void>();
         client->write(fe->getIdentityHashSaltPolicy());
         }
         break;
      case J9ServerMessageType::VM_getOffsetOfJLThreadJ9Thread:
         {
         client->getRecvData<JITaaS::Void>();
         client->write(fe->getOffsetOfJLThreadJ9Thread());
         }
         break;
      case J9ServerMessageType::VM_scanReferenceSlotsInClassForOffset:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, int32_t>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         int32_t offset = std::get<1>(recv);
         client->write(fe->scanReferenceSlotsInClassForOffset(comp, clazz, offset));
         }
         break;
      case J9ServerMessageType::VM_findFirstHotFieldTenuredClassOffset:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->findFirstHotFieldTenuredClassOffset(comp, clazz));
         }
         break;
      case J9ServerMessageType::VM_getResolvedVirtualMethod:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, I_32, bool>();
         auto clazz = std::get<0>(recv);
         auto offset = std::get<1>(recv);
         auto ignoreRTResolve = std::get<2>(recv);
         client->write(fe->getResolvedVirtualMethod(clazz, offset, ignoreRTResolve));
         }
         break;
      case J9ServerMessageType::VM_setJ2IThunk:
         {
         auto recv = client->getRecvData<void*, std::string>();
         void *thunk = std::get<0>(recv);
         std::string signature = std::get<1>(recv);
         TR::compInfoPT->addThunkToBeRelocated(thunk, signature);
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::VM_setInvokeExactJ2IThunk:
         {
         auto recv = client->getRecvData<void*>();
         void *thunk = std::get<0>(recv);
         TR::compInfoPT->addInvokeExactThunkToBeRelocated((TR_J2IThunk*) thunk);
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::VM_sameClassLoaders:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, TR_OpaqueClassBlock *>();
         TR_OpaqueClassBlock *class1 = std::get<0>(recv);
         TR_OpaqueClassBlock *class2 = std::get<1>(recv);
         client->write(fe->sameClassLoaders(class1, class2));
         }
         break;
      case J9ServerMessageType::VM_isUnloadAssumptionRequired:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, TR_ResolvedMethod *>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         TR_ResolvedMethod *method = std::get<1>(recv);
         client->write(fe->isUnloadAssumptionRequired(clazz, method));
         }
         break;
      case J9ServerMessageType::VM_getInstanceFieldOffset:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, std::string, std::string, UDATA>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         std::string field = std::get<1>(recv);
         std::string sig = std::get<2>(recv);
         UDATA options = std::get<3>(recv);
         client->write(fe->getInstanceFieldOffset(clazz, const_cast<char*>(field.c_str()), field.length(),
                  const_cast<char*>(sig.c_str()), sig.length(), options));
         }
         break;
      case J9ServerMessageType::VM_getJavaLangClassHashCode:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         bool hashCodeComputed = false;
         int32_t hashCode = fe->getJavaLangClassHashCode(comp, clazz, hashCodeComputed);
         client->write(hashCode, hashCodeComputed);
         }
         break;
      case J9ServerMessageType::VM_hasFinalizer:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->hasFinalizer(clazz));
         }
         break;
      case J9ServerMessageType::VM_getClassDepthAndFlagsValue:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getClassDepthAndFlagsValue(clazz));
         }
         break;
      case J9ServerMessageType::VM_getMethodFromName:
         {
         auto recv = client->getRecvData<std::string, std::string, std::string, TR_OpaqueMethodBlock *>();
         std::string className = std::get<0>(recv);
         std::string methodName = std::get<1>(recv);
         std::string signature = std::get<2>(recv);
         TR_OpaqueMethodBlock *callingMethod = std::get<3>(recv);
         client->write(fe->getMethodFromName(const_cast<char*>(className.c_str()),
                  const_cast<char*>(methodName.c_str()), const_cast<char*>(signature.c_str()), callingMethod));
         }
         break;
      case J9ServerMessageType::VM_getMethodFromClass:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, std::string, std::string, TR_OpaqueClassBlock *>();
         TR_OpaqueClassBlock *methodClass = std::get<0>(recv);
         std::string methodName = std::get<1>(recv);
         std::string signature = std::get<2>(recv);
         TR_OpaqueClassBlock *callingClass = std::get<3>(recv);
         client->write(fe->getMethodFromClass(methodClass, const_cast<char*>(methodName.c_str()),
                  const_cast<char*>(signature.c_str()), callingClass));
         }
         break;
      case J9ServerMessageType::VM_createMethodHandleArchetypeSpecimen:
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
            client->write(archetype, std::string(thunkableSignature, length));
            }
         }
         break;
      case J9ServerMessageType::VM_isClassVisible:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, TR_OpaqueClassBlock *>();
         TR_OpaqueClassBlock *sourceClass = std::get<0>(recv);
         TR_OpaqueClassBlock *destClass = std::get<1>(recv);
         client->write(fe->isClassVisible(sourceClass, destClass));
         }
         break;
      case J9ServerMessageType::VM_markClassForTenuredAlignment:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, uint32_t>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         uint32_t alignFromStart = std::get<1>(recv);
         fe->markClassForTenuredAlignment(comp, clazz, alignFromStart);
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::VM_getReferenceSlotsInClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         int32_t *start = fe->getReferenceSlotsInClass(comp, clazz);
         if (!start)
            client->write(std::string(""));
         else
            {
            int32_t numSlots = 0;
            for (; start[numSlots]; ++numSlots);
            // Copy the null terminated array into a string
            std::string slotsStr((char *) start, (1 + numSlots) * sizeof(int32_t));
            client->write(slotsStr);
            }
         }
         break;
      case J9ServerMessageType::VM_getMethodSize:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(fe->getMethodSize(method));
         }
         break;
      case J9ServerMessageType::VM_addressOfFirstClassStatic:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->addressOfFirstClassStatic(clazz));
         }
         break;
      case J9ServerMessageType::VM_getStaticFieldAddress:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, std::string, std::string>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         std::string fieldName = std::get<1>(recv);
         std::string sigName = std::get<2>(recv);
         client->write(fe->getStaticFieldAddress(clazz, 
                  reinterpret_cast<unsigned char*>(const_cast<char*>(fieldName.c_str())), 
                  fieldName.length(), 
                  reinterpret_cast<unsigned char*>(const_cast<char*>(sigName.c_str())), 
                  sigName.length()));
         }
         break;
      case J9ServerMessageType::VM_getInterpreterVTableSlot:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock *, TR_OpaqueClassBlock *>();
         TR_OpaqueMethodBlock *method = std::get<0>(recv);
         TR_OpaqueClassBlock *clazz = std::get<1>(recv);
         client->write(fe->getInterpreterVTableSlot(method, clazz));
         }
         break;
      case J9ServerMessageType::VM_revertToInterpreted:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         fe->revertToInterpreted(method);
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::VM_getLocationOfClassLoaderObjectPointer:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->getLocationOfClassLoaderObjectPointer(clazz));
         }
         break;
      case J9ServerMessageType::VM_isOwnableSyncClass:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->isOwnableSyncClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_getClassFromMethodBlock:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(fe->getClassFromMethodBlock(method));
         }
         break;
      case J9ServerMessageType::VM_fetchMethodExtendedFlagsPointer:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(fe->fetchMethodExtendedFlagsPointer(method));
         }
         break;
      case J9ServerMessageType::VM_stringEquals:
         {
         auto recv = client->getRecvData<uintptrj_t *, uintptrj_t *>();
         uintptrj_t *stringLocation1 = std::get<0>(recv);
         uintptrj_t *stringLocation2 = std::get<1>(recv);
         int32_t result;
         bool isEqual = fe->stringEquals(comp, stringLocation1, stringLocation2, result);
         client->write(result, isEqual);
         }
         break;
      case J9ServerMessageType::VM_getStringHashCode:
         {
         uintptrj_t *stringLocation = std::get<0>(client->getRecvData<uintptrj_t *>());
         int32_t result;
         bool isGet = fe->getStringHashCode(comp, stringLocation, result);
         client->write(result, isGet);
         }
         break;
      case J9ServerMessageType::VM_getLineNumberForMethodAndByteCodeIndex:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock *, int32_t>();
         TR_OpaqueMethodBlock *method = std::get<0>(recv);
         int32_t bcIndex = std::get<1>(recv);
         client->write(fe->getLineNumberForMethodAndByteCodeIndex(method, bcIndex));
         }
         break;
      case J9ServerMessageType::VM_getObjectNewInstanceImplMethod:
         {
         client->getRecvData<JITaaS::Void>();
         client->write(fe->getObjectNewInstanceImplMethod());
         }
         break;
      case J9ServerMessageType::VM_getBytecodePC:
         {
         TR_OpaqueMethodBlock *method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(TR::Compiler->mtd.bytecodeStart(method));
         }
         break;
      case J9ServerMessageType::VM_isClassLoadedBySystemClassLoader:
         {
         TR_OpaqueClassBlock *clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(fe->isClassLoadedBySystemClassLoader(clazz));
         }
         break;
      case J9ServerMessageType::VM_getVFTEntry:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, int32_t>();
         client->write(fe->getVFTEntry(std::get<0>(recv), std::get<1>(recv)));
         }
         break;
      case J9ServerMessageType::VM_getArrayLengthOfStaticAddress:
         {
         auto recv = client->getRecvData<void*>();
         void *ptr = std::get<0>(recv);
         int32_t length;
         bool ok = fe->getArrayLengthOfStaticAddress(ptr, length);
         client->write(ok, length);
         }
         break;
      case J9ServerMessageType::VM_isClassArray:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock*>();
         auto clazz = std::get<0>(recv);
         client->write(fe->isClassArray(clazz));
         }
         break;
      case J9ServerMessageType::VM_instanceOfOrCheckCast:
         {
         auto recv = client->getRecvData<J9Class*, J9Class*>();
         auto clazz1 = std::get<0>(recv);
         auto clazz2 = std::get<1>(recv);
         client->write(fe->instanceOfOrCheckCast(clazz1, clazz2));
         }
         break;
      case J9ServerMessageType::VM_transformJlrMethodInvoke:
         {
         auto recv = client->getRecvData<J9Method*, J9Class*>();
         auto method = std::get<0>(recv);
         auto clazz = std::get<1>(recv);
         client->write(fe->transformJlrMethodInvoke(method, clazz));
         }
         break;
      case J9ServerMessageType::mirrorResolvedJ9Method:
         {
         // allocate a new TR_ResolvedRelocatableJ9Method on the heap, to be used as a mirror for performing actions which are only
         // easily done on the client side.
         auto recv = client->getRecvData<TR_OpaqueMethodBlock *, TR_ResolvedJ9Method *, uint32_t>();
         TR_OpaqueMethodBlock *method = std::get<0>(recv);
         auto *owningMethod = std::get<1>(recv);
         uint32_t vTableSlot = std::get<2>(recv);
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
         TR_ResolvedJ9JITaaSServerMethod::createResolvedJ9MethodMirror(methodInfo, method, vTableSlot, owningMethod, fe, trMemory);
         
         client->write(methodInfo);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getRemoteROMClassAndMethods:
         {
         J9Class *clazz = std::get<0>(client->getRecvData<J9Class *>());
         client->write(JITaaSHelpers::packRemoteROMClassInfo(clazz, fe, trMemory));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_isJNINative:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(method->isJNINative());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_isInterpreted:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(method->isInterpreted());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_staticAttributes:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         int32_t isStore = std::get<2>(recv);
         int32_t needAOTValidation = std::get<3>(recv);
         void *address;
         TR::DataType type;
         bool volatileP, isFinal, isPrivate, unresolvedInCP;
         bool result = method->staticAttributes(comp, cpIndex, &address, &type, &volatileP, &isFinal,
               &isPrivate, isStore, &unresolvedInCP, needAOTValidation);
         TR_J9MethodFieldAttributes attrs = {{.address = address}, type.getDataType(), volatileP, isFinal, isPrivate, unresolvedInCP, result};
         client->write(attrs);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getClassFromConstantPool:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, uint32_t>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         uint32_t cpIndex = std::get<1>(recv);
         client->write(method->getClassFromConstantPool(comp, cpIndex));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getDeclaringClassFromFieldOrStatic:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(method->getDeclaringClassFromFieldOrStatic(comp, cpIndex));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_classOfStatic:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         int32_t returnClassForAOT = std::get<2>(recv);
         client->write(method->classOfStatic(cpIndex, returnClassForAOT));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_fieldAttributes:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         I_32 cpIndex = std::get<1>(recv);
         bool isStore = std::get<2>(recv);
         bool needAOTValidation = std::get<3>(recv);
         U_32 fieldOffset;
         TR::DataType type;
         bool volatileP, isFinal, isPrivate, unresolvedInCP;
         bool result = method->fieldAttributes(comp, cpIndex, &fieldOffset, &type, &volatileP, &isFinal,
               &isPrivate, isStore, &unresolvedInCP, needAOTValidation);
         TR_J9MethodFieldAttributes attrs = {{.fieldOffset = fieldOffset}, type.getDataType(), volatileP, isFinal, isPrivate, unresolvedInCP, result};
         client->write(attrs);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getResolvedStaticMethodAndMirror:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, I_32>();
         auto *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         J9Method *ramMethod = nullptr;
            {
            TR::VMAccessCriticalSection resolveStaticMethodRef(fe);
            ramMethod = jitResolveStaticMethodRef(fe->vmThread(), method->cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
            }
        
         // create a mirror right away
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
         if (ramMethod)
            TR_ResolvedJ9JITaaSServerMethod::createResolvedJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, method, fe, trMemory);

         client->write(ramMethod, methodInfo);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getResolvedSpecialMethodAndMirror:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, I_32, bool>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         bool canBeUnresolved = std::get<2>(recv);
         J9Method *ramMethod = nullptr;
         bool unresolved = false, tookBranch = false;
         if (canBeUnresolved)
            {
            ramMethod = jitGetJ9MethodUsingIndex(fe->vmThread(), method->cp(), cpIndex);
            unresolved = true;
            }
         if (!((fe->_jitConfig->runtimeFlags & J9JIT_RUNTIME_RESOLVE) &&
                           comp->ilGenRequest().details().isMethodHandleThunk() &&
                           performTransformation(comp, "Setting as unresolved special call cpIndex=%d\n",cpIndex)))
            {
            TR::VMAccessCriticalSection resolveSpecialMethodRef(fe);
            ramMethod = jitResolveSpecialMethodRef(fe->vmThread(), method->cp(), cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME);
            tookBranch = true;
            }
         
         // create a mirror right away
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo; 
         if (ramMethod)
            TR_ResolvedJ9JITaaSServerMethod::createResolvedJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, method, fe, trMemory);

         client->write(ramMethod, unresolved, tookBranch, methodInfo);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_classCPIndexOfMethod:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, uint32_t>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(method->classCPIndexOfMethod(cpIndex));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_startAddressForJittedMethod:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(method->startAddressForJittedMethod());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getResolvedPossiblyPrivateVirtualMethodAndMirror:
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
         if ((J9JIT_INTERP_VTABLE_OFFSET + sizeof(uintptrj_t)) == vTableIndex)
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
            TR_ResolvedJ9JITaaSServerMethod::createResolvedJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, (uint32_t) vTableIndex, owningMethod, fe, trMemory);
                        
            client->write(ramMethod, vTableIndex, resolvedInCP, methodInfo);
            }
         else
            {
            client->write(ramMethod, vTableIndex, resolvedInCP, TR_ResolvedJ9JITaaSServerMethodInfo());
            }
         }
         break;
      case J9ServerMessageType::ResolvedMethod_virtualMethodIsOverridden:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(method->virtualMethodIsOverridden());
         }
         break;
      case J9ServerMessageType::get_params_to_construct_TR_j9method:
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
         client->write(classNameStr, nameStr, signatureStr);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_setRecognizedMethodInfo:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, TR::RecognizedMethod>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         TR::RecognizedMethod rm = std::get<1>(recv);
         method->setRecognizedMethodInfo(rm);
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_localName:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, U_32, U_32>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         U_32 slotNumber = std::get<1>(recv);
         U_32 bcIndex = std::get<2>(recv);
         I_32 len;
         char *nameChars = method->localName(slotNumber, bcIndex, len, trMemory);
         if (nameChars)
            client->write(std::string(nameChars, len));
         else
            client->write(std::string());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_jniNativeMethodProperties:
         {
         auto ramMethod = std::get<0>(client->getRecvData<J9Method*>());
         uintptrj_t jniProperties;
         void *jniTargetAddress = fe->getJ9JITConfig()->javaVM->internalVMFunctions->jniNativeMethodProperties(fe->vmThread(), ramMethod, &jniProperties);
         client->write(jniProperties, jniTargetAddress);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getResolvedInterfaceMethod_2:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, I_32>();
         auto mirror = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         UDATA pITableIndex;
         TR_OpaqueClassBlock *clazz = mirror->getResolvedInterfaceMethod(cpIndex, &pITableIndex);
         client->write(clazz, pITableIndex);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getResolvedInterfaceMethodAndMirror_3:
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
            TR_ResolvedJ9JITaaSServerMethod::createResolvedJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, owningMethod, fe, trMemory);

         client->write(resolved, ramMethod, methodInfo);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getResolvedInterfaceMethodOffset:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, TR_OpaqueClassBlock*, I_32>();
         auto mirror = std::get<0>(recv);
         auto clazz = std::get<1>(recv);
         auto cpIndex = std::get<2>(recv);
         U_32 offset = mirror->getResolvedInterfaceMethodOffset(clazz, cpIndex);
         client->write(offset);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getResolvedImproperInterfaceMethod:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, I_32>();
         auto mirror = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         J9Method *j9method = jitGetImproperInterfaceMethodFromCP(fe->vmThread(), mirror->cp(), cpIndex);
         client->write(j9method);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_startAddressForJNIMethod:
         {
         TR_ResolvedJ9Method *ramMethod = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(ramMethod->startAddressForJNIMethod(comp));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_startAddressForInterpreterOfJittedMethod:
         {
         TR_ResolvedJ9Method *ramMethod = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(ramMethod->startAddressForInterpreterOfJittedMethod());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getUnresolvedStaticMethodInCP:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method= std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(method->getUnresolvedStaticMethodInCP(cpIndex));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getUnresolvedSpecialMethodInCP:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method= std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(method->getUnresolvedSpecialMethodInCP(cpIndex));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getUnresolvedFieldInCP:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method= std::get<0>(recv);
         int32_t cpIndex = std::get<1>(recv);
         client->write(method->getUnresolvedFieldInCP(cpIndex));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getRemoteROMString:
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
         client->write(std::string(data, len));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_fieldOrStaticName:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         TR_ResolvedJ9Method *method = std::get<0>(recv);
         size_t cpIndex = std::get<1>(recv);
         int32_t len;

         // Call staticName here in lieu of fieldOrStaticName as the server has already checked for "<internal field>"
         char *s = method->staticName(cpIndex, len, trMemory, heapAlloc);
         client->write(std::string(s, len));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_isSubjectToPhaseChange:
         {
         TR_ResolvedJ9Method *method = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(method->isSubjectToPhaseChange(comp));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getResolvedHandleMethod:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, I_32>();
         auto mirror = std::get<0>(recv);
         I_32 cpIndex = std::get<1>(recv);
         TR::VMAccessCriticalSection getResolvedHandleMethod(fe);

#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
         bool unresolvedInCP = mirror->isUnresolvedMethodTypeTableEntry(cpIndex);
         TR_OpaqueMethodBlock *dummyInvokeExact = fe->getMethodFromName("java/lang/invoke/MethodHandle",
               "invokeExact", "([Ljava/lang/Object;)Ljava/lang/Object;", mirror->getNonPersistentIdentifier());
         J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)(mirror->cp()->romConstantPool + cpIndex);
         J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
         int32_t signatureLength;
         char   *signature = utf8Data(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig), signatureLength);
#else
         bool unresolvedInCP = mirror->isUnresolvedMethodType(cpIndex);
         TR_OpaqueMethodBlock *dummyInvokeExact = fe->getMethodFromName("java/lang/invoke/MethodHandle",
               "invokeExact", "([Ljava/lang/Object;)Ljava/lang/Object;", mirror->getNonPersistentIdentifier());
         J9ROMMethodTypeRef *romMethodTypeRef = (J9ROMMethodTypeRef *)(mirror->cp()->romConstantPool + cpIndex);
         int32_t signatureLength;
         char   *signature = utf8Data(J9ROMMETHODTYPEREF_SIGNATURE(romMethodTypeRef), signatureLength);
#endif
         client->write(dummyInvokeExact, std::string(signature, signatureLength), unresolvedInCP);
         }
         break;
#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
      case J9ServerMessageType::ResolvedMethod_methodTypeTableEntryAddress:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, I_32>();
         auto mirror = std::get<0>(recv);
         I_32 cpIndex = std::get<1>(recv);
         client->write(mirror->methodTypeTableEntryAddress(cpIndex));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_isUnresolvedMethodTypeTableEntry:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, I_32>();
         auto mirror = std::get<0>(recv);
         I_32 cpIndex = std::get<1>(recv);
         client->write(mirror->isUnresolvedMethodTypeTableEntry(cpIndex));
         }
         break;
#endif
      case J9ServerMessageType::ResolvedMethod_isUnresolvedCallSiteTableEntry:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, int32_t>();
         auto mirror = std::get<0>(recv);
         int32_t callSiteIndex = std::get<1>(recv);
         client->write(mirror->isUnresolvedCallSiteTableEntry(callSiteIndex));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_callSiteTableEntryAddress:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, int32_t>();
         auto mirror = std::get<0>(recv);
         int32_t callSiteIndex = std::get<1>(recv);
         client->write(mirror->callSiteTableEntryAddress(callSiteIndex));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getResolvedDynamicMethod:
         {
         auto recv = client->getRecvData<int32_t, J9Class *, TR_OpaqueMethodBlock*>();
         int32_t callSiteIndex = std::get<0>(recv);
         J9Class *clazz = std::get<1>(recv);
         TR_OpaqueMethodBlock *method = std::get<2>(recv);

         TR::VMAccessCriticalSection getResolvedDynamicMethod(fe);
         J9ROMClass *romClass = clazz->romClass;
         bool unresolvedInCP = (clazz->callSites[callSiteIndex] == NULL);
         J9SRP                 *namesAndSigs = (J9SRP*)J9ROMCLASS_CALLSITEDATA(romClass);
         J9ROMNameAndSignature *nameAndSig   = NNSRP_GET(namesAndSigs[callSiteIndex], J9ROMNameAndSignature*);
         J9UTF8                *signature    = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
         TR_OpaqueMethodBlock *dummyInvokeExact = fe->getMethodFromName("java/lang/invoke/MethodHandle",
               "invokeExact", "([Ljava/lang/Object;)Ljava/lang/Object;", method);
         client->write(dummyInvokeExact, std::string(utf8Data(signature), J9UTF8_LENGTH(signature)), unresolvedInCP);
         }
         break;
      case J9ServerMessageType::ResolvedMethod_shouldFailSetRecognizedMethodInfoBecauseOfHCR:
         {
         auto mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method*>());
         client->write(mirror->shouldFailSetRecognizedMethodInfoBecauseOfHCR());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_isSameMethod:
         {
         auto recv = client->getRecvData<uintptrj_t*, uintptrj_t*>();
         client->write(*std::get<0>(recv) == *std::get<1>(recv));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_isBigDecimalMethod:
         {
         J9Method *j9method = std::get<0>(client->getRecvData<J9Method*>());
         client->write(TR_J9MethodBase::isBigDecimalMethod(j9method));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_isBigDecimalConvertersMethod:
         {
         J9Method *j9method = std::get<0>(client->getRecvData<J9Method*>());
         client->write(TR_J9MethodBase::isBigDecimalConvertersMethod(j9method));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_osrFrameSize:
         {
         J9Method *j9method = std::get<0>(client->getRecvData<J9Method*>());
         client->write(TR_J9MethodBase::osrFrameSize(j9method));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_isInlineable:
         {
         TR_ResolvedJ9Method *mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(mirror->isInlineable(comp));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_isWarmCallGraphTooBig:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, uint32_t>();
         TR_ResolvedJ9Method *mirror = std::get<0>(recv);
         uint32_t bcIndex = std::get<1>(recv);
         client->write(mirror->isWarmCallGraphTooBig(bcIndex, comp));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_setWarmCallGraphTooBig:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, uint32_t>();
         TR_ResolvedJ9Method *mirror = std::get<0>(recv);
         uint32_t bcIndex = std::get<1>(recv);
         mirror->setWarmCallGraphTooBig(bcIndex, comp);
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_setVirtualMethodIsOverridden:
         {
         TR_ResolvedJ9Method *mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         mirror->setVirtualMethodIsOverridden();
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_addressContainingIsOverriddenBit:
         {
         TR_ResolvedJ9Method *mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(mirror->addressContainingIsOverriddenBit());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_methodIsNotzAAPEligible:
         {
         TR_ResolvedJ9Method *mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(mirror->methodIsNotzAAPEligible());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_setClassForNewInstance:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method*, J9Class*>();
         TR_ResolvedJ9Method *mirror = std::get<0>(recv);
         J9Class *clazz = std::get<1>(recv);
         mirror->setClassForNewInstance(clazz);
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::ResolvedMethod_getJittedBodyInfo:
         {
         auto mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         void *startPC = mirror->startAddressForInterpreterOfJittedMethod();
         auto bodyInfo = J9::Recompilation::getJittedBodyInfoFromPC(startPC);
         if (!bodyInfo)
            client->write(std::string(), std::string());
         else
            client->write(std::string((char*) bodyInfo, sizeof(TR_PersistentJittedBodyInfo)),
                          std::string((char*) bodyInfo->getMethodInfo(), sizeof(TR_PersistentMethodInfo)));
         }
         break;
      case J9ServerMessageType::CompInfo_isCompiled:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(TR::CompilationInfo::isCompiled(method));
         }
         break;
      case J9ServerMessageType::CompInfo_getJ9MethodExtra:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write((uint64_t) TR::CompilationInfo::getJ9MethodExtra(method));
         }
         break;
      case J9ServerMessageType::CompInfo_getInvocationCount:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(TR::CompilationInfo::getInvocationCount(method));
         }
         break;
      case J9ServerMessageType::CompInfo_setInvocationCount:
         {
         auto recv = client->getRecvData<J9Method *, int32_t>();
         J9Method *method = std::get<0>(recv);
         int32_t count = std::get<1>(recv);
         client->write(TR::CompilationInfo::setInvocationCount(method, count));
         }
         break;
      case J9ServerMessageType::CompInfo_setInvocationCountAtomic:
         {
         auto recv = client->getRecvData<J9Method *, int32_t, int32_t>();
         J9Method *method = std::get<0>(recv);
         int32_t oldCount = std::get<1>(recv);
         int32_t newCount = std::get<2>(recv);
         client->write(TR::CompilationInfo::setInvocationCount(method, oldCount, newCount));
         }
         break;
      case J9ServerMessageType::CompInfo_isJNINative:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(TR::CompilationInfo::isJNINative(method));
         }
         break;
      case J9ServerMessageType::CompInfo_isJSR292:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(TR::CompilationInfo::isJSR292(method));
         }
         break;
      case J9ServerMessageType::CompInfo_getMethodBytecodeSize:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(TR::CompilationInfo::getMethodBytecodeSize(method));
         }
         break;
      case J9ServerMessageType::CompInfo_setJ9MethodExtra:
         {
         auto recv = client->getRecvData<J9Method *, uint64_t>();
         J9Method *method = std::get<0>(recv);
         uint64_t count = std::get<1>(recv);
         TR::CompilationInfo::setJ9MethodExtra(method, count);
         client->write(JITaaS::Void());
         }
         break;
      case J9ServerMessageType::CompInfo_isClassSpecial:
         {
         J9Class *clazz = std::get<0>(client->getRecvData<J9Class *>());
         bool val = clazz->classDepthAndFlags & (J9_JAVA_CLASS_REFERENCE_WEAK      |
                                                 J9_JAVA_CLASS_REFERENCE_SOFT      |
                                                 J9_JAVA_CLASS_FINALIZE            |
                                                 J9_JAVA_CLASS_OWNABLE_SYNCHRONIZER);
         client->write(val);
         }
         break;
      case J9ServerMessageType::CompInfo_getJ9MethodStartPC:
         {
         J9Method *method = std::get<0>(client->getRecvData<J9Method *>());
         client->write(TR::CompilationInfo::getJ9MethodStartPC(method));
         }
         break;

      case J9ServerMessageType::ClassEnv_classFlagsValue:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(TR::Compiler->cls.classFlagsValue(clazz));
         }
         break;
      case J9ServerMessageType::ClassEnv_classDepthOf:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(TR::Compiler->cls.classDepthOf(clazz));
         }
         break;
      case J9ServerMessageType::ClassEnv_classInstanceSize:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(TR::Compiler->cls.classInstanceSize(clazz));
         }
         break;
      case J9ServerMessageType::ClassEnv_superClassesOf:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(TR::Compiler->cls.superClassesOf(clazz));
         }
         break;
      case J9ServerMessageType::ClassEnv_iTableOf:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(TR::Compiler->cls.iTableOf(clazz));
         }
         break;
      case J9ServerMessageType::ClassEnv_iTableNext:
         {
         auto clazz = std::get<0>(client->getRecvData<J9ITable *>());
         client->write(TR::Compiler->cls.iTableNext(clazz));
         }
         break;
      case J9ServerMessageType::ClassEnv_iTableRomClass:
         {
         auto clazz = std::get<0>(client->getRecvData<J9ITable *>());
         client->write(TR::Compiler->cls.iTableRomClass(clazz));
         }
         break;
      case J9ServerMessageType::ClassEnv_getITable:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         client->write(TR::Compiler->cls.getITable(clazz));
         }
         break;
      case J9ServerMessageType::ClassEnv_indexedSuperClassOf:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *, size_t>();
         auto clazz = std::get<0>(recv);
         size_t index = std::get<1>(recv);
         client->write(TR::Compiler->cls.superClassesOf(clazz)[index]);
         }
         break;
      case J9ServerMessageType::SharedCache_getClassChainOffsetInSharedCache:
         {
         auto j9class = std::get<0>(client->getRecvData<TR_OpaqueClassBlock *>());
         uintptrj_t classChainOffsetInSharedCache = fe->sharedCache()->lookupClassChainOffsetInSharedCacheFromClass(j9class);
         client->write(classChainOffsetInSharedCache);
         }
         break;
      case J9ServerMessageType::runFEMacro_derefUintptrjPtr:
         {
         TR::VMAccessCriticalSection deref(fe);
         compInfoPT->updateLastLocalGCCounter();
         client->write(*std::get<0>(client->getRecvData<uintptrj_t*>()));
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeILGenMacrosInvokeExactAndFixup:
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
         client->write(std::string(methodDescriptor, methodDescriptorLength));
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeCollectHandleNumArgsToCollect:
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
         client->write(collectArraySize, numArguments, collectionStart);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeGuardWithTestHandleNumGuardArgs:
         {
         auto recv = client->getRecvData<uintptrj_t*>();
         TR::VMAccessCriticalSection invokeGuardWithTestHandleNumGuardArgs(fe);
         uintptrj_t methodHandle = *std::get<0>(recv);
         uintptrj_t guardArgs = fe->getReferenceField(fe->methodHandle_type(fe->getReferenceField(methodHandle,
            "guard", "Ljava/lang/invoke/MethodHandle;")),
            "arguments", "[Ljava/lang/Class;");
         int32_t numGuardArgs = (int32_t)fe->getArrayLengthInElements(guardArgs);
         client->write(numGuardArgs);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeExplicitCastHandleConvertArgs:
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
         client->write(std::string(methodDescriptor, methodDescriptorLength));
         }
         break;
      case J9ServerMessageType::runFEMacro_targetTypeL:
         {
         auto recv = client->getRecvData<uintptrj_t*, int32_t>();
         int32_t argIndex = std::get<1>(recv);
         TR::VMAccessCriticalSection targetTypeL(fe);
         uintptrj_t methodHandle = *std::get<0>(recv);
         // We've already loaded the handle once, but must reload it because we released VM access in between.
         uintptrj_t arguments = fe->getReferenceField(fe->getReferenceField(fe->getReferenceField(
                                                      methodHandle,
                                                      "next",             "Ljava/lang/invoke/MethodHandle;"),
                                                      "type",             "Ljava/lang/invoke/MethodType;"),
                                                      "arguments",        "[Ljava/lang/Class;");
         TR_OpaqueClassBlock *parmClass = (TR_OpaqueClassBlock*)(intptrj_t)fe->getInt64Field(fe->getReferenceElement(arguments, argIndex),
                                                                          "vmRef" /* should use fej9->getOffsetOfClassFromJavaLangClassField() */);
         client->write(parmClass);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeDirectHandleDirectCall:
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
            TR_OpaqueMethodBlock **vtable = (TR_OpaqueMethodBlock**)(((uintptrj_t)fe->getClassFromJavaLangClass(jlClass)) + J9JIT_INTERP_VTABLE_OFFSET);
            int32_t index = (int32_t)((vmSlot - J9JIT_INTERP_VTABLE_OFFSET) / sizeof(vtable[0]));
            j9method = vtable[index];
            }
         else
            {
            j9method = (TR_OpaqueMethodBlock*)(intptrj_t)vmSlot;
            }
         client->write(j9method, vmSlot);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeSpreadHandleArrayArg:
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
         client->write(spreadPosition, arity, leafClass);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeSpreadHandle:
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
         client->write(numArguments, numNextArguments, spreadStart);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeInsertHandle:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeInsertHandle(fe);
         int32_t insertionIndex = fe->getInt32Field(methodHandle, "insertionIndex");
         uintptrj_t arguments = fe->getReferenceField(fe->getReferenceField(methodHandle, "type",
                  "Ljava/lang/invoke/MethodType;"), "arguments", "[Ljava/lang/Class;");
         int32_t numArguments = (int32_t)fe->getArrayLengthInElements(arguments);
         uintptrj_t values = fe->getReferenceField(methodHandle, "values", "[Ljava/lang/Object;");
         int32_t numValues = (int32_t)fe->getArrayLengthInElements(values);
         client->write(insertionIndex, numArguments, numValues);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeFoldHandle:
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
            for (int i = arrayLength-1; i>=0; i--)
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
         client->write(indices, foldPosition, numArgs);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeFoldHandle2:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeFoldHandle(fe);
         int32_t foldPosition = fe->getInt32Field(methodHandle, "foldPosition");
         client->write(foldPosition);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeFinallyHandle:
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
         client->write(numArgsPassToFinallyTarget, std::string(methodDescriptor, methodDescriptorLength));
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeFilterArgumentsHandle:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeFilterArgumentsHandle(fe);

         int32_t startPos = (int32_t)fe->getInt32Field(methodHandle, "startPos");

         uintptrj_t filters = fe->getReferenceField(methodHandle, "filters", "[Ljava/lang/invoke/MethodHandle;");
         int32_t numFilters = fe->getArrayLengthInElements(filters);
         std::vector<TR::KnownObjectTable::Index> filterIndexList(numFilters);
         TR::KnownObjectTable *knot = comp->getOrCreateKnownObjectTable();
         for (int i = 0; i <numFilters; i++)
            {
            filterIndexList[i] = knot->getIndex(fe->getReferenceElement(filters, i));
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
         client->write(startPos, filterIndexList, nextSignatureString);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeFilterArgumentsHandle2:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeFilderArgumentsHandle(fe);
         uintptrj_t arguments = fe->getReferenceField(fe->methodHandle_type(methodHandle), "arguments", "[Ljava/lang/Class;");
         int32_t numArguments = (int32_t)fe->getArrayLengthInElements(arguments);
         int32_t startPos     = (int32_t)fe->getInt32Field(methodHandle, "startPos");
         uintptrj_t filters = fe->getReferenceField(methodHandle, "filters", "[Ljava/lang/invoke/MethodHandle;");
         int32_t numFilters = fe->getArrayLengthInElements(filters);
         client->write(numArguments, startPos, numFilters);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeCatchHandle:
         {
         uintptrj_t methodHandle = *std::get<0>(client->getRecvData<uintptrj_t*>());
         TR::VMAccessCriticalSection invokeCatchHandle(fe);
         uintptrj_t catchTarget    = fe->getReferenceField(methodHandle, "catchTarget", "Ljava/lang/invoke/MethodHandle;");
         uintptrj_t catchArguments = fe->getReferenceField(fe->getReferenceField(
            catchTarget,
            "type", "Ljava/lang/invoke/MethodType;"),
            "arguments", "[Ljava/lang/Class;");
         int32_t numCatchArguments = (int32_t)fe->getArrayLengthInElements(catchArguments);
         client->write(numCatchArguments);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeArgumentMoverHandlePermuteArgs:
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
         client->write(std::string(nextHandleSignature, methodDescriptorLength));
         }
         break;
      case J9ServerMessageType::runFEMacro_invokePermuteHandlePermuteArgs:
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
         client->write(permuteLength, argIndices);
         }
         break;
      case J9ServerMessageType::runFEMacro_invokeILGenMacros:
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
         client->write(parameterCount);
         }
         break;

      case J9ServerMessageType::CHTable_getAllClassInfo:
         {
         client->getRecvData<JITaaS::Void>();
         auto table = (TR_JITaaSClientPersistentCHTable*)comp->getPersistentInfo()->getPersistentCHTable();
         TR_OpaqueClassBlock *rootClass = fe->getSystemClassFromClassName("java/lang/Object", 16);
         TR_PersistentClassInfo* result = table->findClassInfoAfterLocking(rootClass, comp, false);
         std::string encoded = FlatPersistentClassInfo::serializeHierarchy(result);
         client->write(encoded);
         }
         break;
      case J9ServerMessageType::CHTable_getClassInfoUpdates:
         {
         client->getRecvData<JITaaS::Void>();
         auto table = (TR_JITaaSClientPersistentCHTable*)comp->getPersistentInfo()->getPersistentCHTable();
         auto encoded = table->serializeUpdates();
         client->write(encoded.first, encoded.second);
         }
         break;
      case J9ServerMessageType::CHTable_clearReservable:
         {
         auto clazz = std::get<0>(client->getRecvData<TR_OpaqueClassBlock*>());
         auto table = (TR_JITaaSClientPersistentCHTable*)comp->getPersistentInfo()->getPersistentCHTable();
         auto info = table->findClassInfoAfterLocking(clazz, comp);
         info->setReservable(false);
         }
         break;
      case J9ServerMessageType::IProfiler_searchForMethodSample:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock*, int32_t>();
         auto method = std::get<0>(recv);
         auto bucket = std::get<1>(recv);
         TR_IProfiler *iProfiler = fe->getIProfiler();
         if (!iProfiler)
            {
            // iProfiler is enabled on the server if we got here, but not enabled here on the client
            client->write(std::string());
            break;
            }
         auto entry = iProfiler->searchForMethodSample(method, bucket);
         if (entry)
            {
            auto serialEntry = TR_ContiguousIPMethodHashTableEntry::serialize(entry);
            std::string entryStr((char *) &serialEntry, sizeof(TR_ContiguousIPMethodHashTableEntry));
            client->write(entryStr);
            }
         else
            {
            client->write(std::string());
            }
         }
         break;
      case J9ServerMessageType::IProfiler_profilingSample:
         {
         handler_IProfiler_profilingSample(client, fe, comp);
         }
         break;
      case J9ServerMessageType::IProfiler_getMaxCallCount:
         {
         TR_IProfiler *iProfiler = fe->getIProfiler();
         client->write(iProfiler->getMaxCallCount());
         }
         break;

      case J9ServerMessageType::Recompilation_getExistingMethodInfo:
         {
         auto recomp = comp->getRecompilationInfo();
         auto methodInfo = recomp->getMethodInfo();
         client->write(*methodInfo);
         }
         break;
      case J9ServerMessageType::Recompilation_getJittedBodyInfoFromPC:
         {
         void *startPC = std::get<0>(client->getRecvData<void *>());
         auto bodyInfo = J9::Recompilation::getJittedBodyInfoFromPC(startPC);
         if (!bodyInfo)
            client->write(std::string(), std::string());
         else
            client->write(std::string((char*) bodyInfo, sizeof(TR_PersistentJittedBodyInfo)),
                          std::string((char*) bodyInfo->getMethodInfo(), sizeof(TR_PersistentMethodInfo)));
         }
         break;
      default:
         // It is vital that this remains a hard error during dev!
         TR_ASSERT(false, "JITaaS: handleServerMessage received an unknown message type");
      }
   return done;
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
   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
      {
      TR_VerboseLog::writeLineLocked(
         TR_Vlog_JITaaS,
         "Client sending compilation request to server for method %s @ %s.",
         compiler->signature(),
         compiler->getHotnessName()
         );
      }

   // Prepare the parameters for the compilation request
   J9Class *clazz = J9_CLASS_FROM_METHOD(method);
   J9ROMClass *romClass = clazz->romClass;
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
   uint32_t romMethodOffset = uint32_t((uint8_t*) romMethod - (uint8_t*) romClass);
   std::string detailsStr = std::string((char*) &details, sizeof(TR::IlGeneratorMethodDetails));
   TR::CompilationInfo *compInfo = compInfoPT->getCompilationInfo();
   std::vector<TR_OpaqueClassBlock*> unloadedClasses(compInfo->getUnloadedClassesTempList()->begin(), compInfo->getUnloadedClassesTempList()->end());
   compInfo->getUnloadedClassesTempList()->clear();
   auto classInfoTuple = JITaaSHelpers::packRemoteROMClassInfo(clazz, compiler->fej9vm(), compiler->trMemory());
   std::string optionsStr = TR::Options::packOptions(compiler->getOptions());

   JITaaS::J9ClientStream client(compInfo->getPersistentInfo());

   uint32_t statusCode = compilationFailure;
   std::string codeCacheStr;
   std::string dataCacheStr;
   CHTableCommitData chTableData;
   std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended;
   std::string logFileStr;
   try
      {
      // release VM access just before sending the compilation request
      // message just in case we block in the wite operation   
      releaseVMAccess(vmThread);

      client.buildCompileRequest(TR::comp()->getPersistentInfo()->getJITaaSId(), romMethodOffset, 
                                 method, clazz, compiler->getMethodHotness(), detailsStr, details.getType(), 
                                 unloadedClasses, classInfoTuple, optionsStr);
      // re-acquire VM access and check for possible class unloading
      acquireVMAccessNoSuspend(vmThread);
      if (compInfoPT->compilationShouldBeInterrupted())
         {
         // JITaaS FIXME: The server is not informed that the client is going to abort
         // Is it better to let the code flow into handleServerMessage() wait for a 
         // query from the server and then abort sending a "cancel" message back?
         auto comp = compInfoPT->getCompilation();
         if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
            TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH, "Interrupting remote compilation of %s @ %s", comp->signature(), comp->getHotnessName());
         comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted in handleServerMessage");
         }

      while(!handleServerMessage(&client, compiler->fej9vm()));
      auto recv = client.getRecvData<uint32_t, std::string, std::string, CHTableCommitData, std::vector<TR_OpaqueClassBlock*>, std::string>();
      statusCode = std::get<0>(recv);
      codeCacheStr = std::get<1>(recv);
      dataCacheStr = std::get<2>(recv);
      chTableData = std::get<3>(recv);
      classesThatShouldNotBeNewlyExtended = std::get<4>(recv);
      logFileStr = std::get<5>(recv);
      if (statusCode >= compilationMaxError)
         throw JITaaS::StreamTypeMismatch("Did not receive a valid TR_CompilationErrorCode as the final message on the stream.");
      }
   catch (const JITaaS::StreamFailure &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, e.what());
      
      compiler->failCompilation<JITaaS::StreamFailure>(e.what());
      }
   JITaaS::Status status = client.waitForFinish();
   TR_MethodMetaData *metaData = NULL;
   if (status.ok() && (statusCode == compilationOK || statusCode == compilationNotNeeded))
      {
      try
         {
         compInfoPT->reloRuntime()->setReloStartTime(compInfoPT->getTimeWhenCompStarted());

         TR_ASSERT(codeCacheStr.size(), "must have code cache");
         TR_ASSERT(dataCacheStr.size(), "must have data cache");

         metaData = compInfoPT->reloRuntime()->prepareRelocateJITCodeAndData(vmThread, compiler->fej9vm(),
               compiler->getCurrentCodeCache(), (uint8_t *)&codeCacheStr[0], (J9JITDataCacheHeader *)&dataCacheStr[0],
               method, false, TR::comp()->getOptions(), TR::comp(), compilee);

         TR::compInfoPT->setMetadata(metaData);

         if (!metaData)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,
                                              "JITaaS Relocation failure: %d",
                                              compInfoPT->reloRuntime()->returnCode());
               }
            // relocation failed, fail compilation
            compInfoPT->getMethodBeingCompiled()->_compErrCode = compInfoPT->reloRuntime()->returnCode();
            compiler->failCompilation<J9::AOTRelocationFailed>("Failed to relocate");
            }
         // TR_ASSERT(metaData, "relocation must succeed");

         if (!TR::comp()->getOption(TR_DisableCHOpts))
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
                  if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITaaS, TR_VerboseCompileEnd, TR_VerbosePerformance, TR_VerboseCompFailure))
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
               if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJITaaS, TR_VerboseCompileEnd, TR_VerbosePerformance, TR_VerboseCompFailure))
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "Failure while committing chtable for %s", compiler->signature());
                  }
               compiler->failCompilation<J9::CHTableCommitFailure>("CHTable commit failure");
               }
            }

         TR::compInfoPT->relocateThunks();

         TR_ASSERT(!metaData->startColdPC, "coldPC should be null");

         compiler->getOptions()->writeLogFileFromServer(logFileStr);

         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_JITaaS,
               "Client successfully loaded method %s @ %s following compilation request. [metaData=%p, startPC=%p]",
               compiler->signature(),
               compiler->getHotnessName(),
               metaData, metaData->startPC
               );
            }
         }
      catch (const std::exception &e)
         {
         // Log for JITaaS mode and re-throw
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_JITaaS,
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
      compiler->failCompilation<JITaaS::ServerCompFailure>("JITaaS compilation failed.");
      }
   return metaData;
   }

void
remoteCompilationEnd(
   TR::IlGeneratorMethodDetails &details,
   J9JITConfig *jitConfig,
   TR_FrontEnd *fe,
   TR_MethodToBeCompiled *entry,
   TR::Compilation *comp)
   {
   entry->_tryCompilingAgain = false; // TODO: Need to handle recompilations gracefully when relocation fails

   TR::CodeCache *codeCache = comp->getCurrentCodeCache();
#if 0
   OMR::CodeCacheMethodHeader *codeCacheHeader = (OMR::CodeCacheMethodHeader*) (comp->getOptimizationPlan()->_mandatoryCodeAddress);
#else
   J9JITDataCacheHeader *aotMethodHeader      = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
   TR_ASSERT(aotMethodHeader, "The header must have been set");
   TR_AOTMethodHeader   *aotMethodHeaderEntry = (TR_AOTMethodHeader *)(aotMethodHeader + 1);

   U_8 *codeStart = (U_8 *)aotMethodHeaderEntry->compileMethodCodeStartPC;
   OMR::CodeCacheMethodHeader *codeCacheHeader = (OMR::CodeCacheMethodHeader*)codeStart;
#endif

   TR_DataCache *dataCache = (TR_DataCache*)comp->getReservedDataCache();
   TR_ASSERT(dataCache, "A dataCache must be reserved for JITaaS compilations\n");
   J9JITDataCacheHeader *dataCacheHeader = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
   J9JITExceptionTable *metaData = TR::compInfoPT->getMetadata();

   size_t codeSize = codeCache->getWarmCodeAlloc() - (uint8_t*)codeCacheHeader;
   size_t dataSize = dataCache->getSegment()->heapAlloc - (uint8_t*)dataCacheHeader;

   //TR_ASSERT(((OMR::RuntimeAssumption*)metaData->runtimeAssumptionList)->getNextAssumptionForSameJittedBody() == metaData->runtimeAssumptionList, "assuming no assumptions");

   std::string codeCacheStr((char*) codeCacheHeader, codeSize);
   std::string dataCacheStr((char*) dataCacheHeader, dataSize);

   CHTableCommitData chTableData;
   if (!comp->getOption(TR_DisableCHOpts))
      {
      TR_CHTable *chTable = comp->getCHTable();
      chTableData = chTable->computeDataForCHTableCommit(comp);
      }

   auto classesThatShouldNotBeNewlyExtended = TR::compInfoPT->getClassesThatShouldNotBeNewlyExtended();

   // pack log file to send to client
   std::string logFileStr = TR::Options::packLogFile(comp->getOutFile());

   entry->_stream->finishCompilation(compilationOK, codeCacheStr, dataCacheStr, chTableData,
                                     std::vector<TR_OpaqueClassBlock*>(classesThatShouldNotBeNewlyExtended->begin(), classesThatShouldNotBeNewlyExtended->end()), logFileStr);

   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,
                                     "Server has successfully compiled %s", entry->_compInfoPT->getCompilation()->signature());
      }
   }

void printJITaaSMsgStats(J9JITConfig *jitConfig)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   j9tty_printf(PORTLIB, "JITaaS Server Message Type Statistics:\n");
   j9tty_printf(PORTLIB, "Type# #called TypeName\n");
   const ::google::protobuf::EnumDescriptor *descriptor = JITaaS::J9ServerMessageType_descriptor();
   for (int i = 0; i < JITaaS::J9ServerMessageType_ARRAYSIZE; ++i)
      {
      if (serverMsgTypeCount[i] > 0)
         j9tty_printf(PORTLIB, "#%04d %7u %s\n", i, serverMsgTypeCount[i], descriptor->FindValueByNumber(i)->name().c_str());
      }
   }

void printJITaaSCHTableStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
#ifdef COLLECT_CHTABLE_STATS
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   j9tty_printf(PORTLIB, "JITaaS CHTable Statistics:\n");
   if (compInfo->getPersistentInfo()->getJITaaSMode() == CLIENT_MODE)
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
   else if (compInfo->getPersistentInfo()->getJITaaSMode() == SERVER_MODE)
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
   if (compInfo->getPersistentInfo()->getJITaaSMode() == SERVER_MODE)
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

ClientSessionData::ClientSessionData(uint64_t clientUID) : 
   _clientUID(clientUID),
   _chTableClassMap(decltype(_chTableClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _romClassMap(decltype(_romClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _J9MethodMap(decltype(_J9MethodMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _systemClassByNameMap(decltype(_systemClassByNameMap)::allocator_type(TR::Compiler->persistentAllocator()))
   {
   updateTimeOfLastAccess();
   _javaLangClassPtr = nullptr;
   _inUse = 1;
   _romMapMonitor = TR::Monitor::create("JIT-JITaaSROMMapMonitor");
   _systemClassMapMonitor = TR::Monitor::create("JIT-JITaaSystemClassMapMonitor");
   _vmInfo = nullptr;
   }

ClientSessionData::~ClientSessionData()
   {
   _romMapMonitor->destroy();
   _systemClassMapMonitor->destroy();
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
         it.second._IPData = nullptr;
         }
      }
   for (auto it : _romClassMap)
      {
      it.second.freeClassInfo();
      _romClassMap.erase(it.first);
      }
   jitPersistentFree(_vmInfo);
   }

void
ClientSessionData::processUnloadedClasses(const std::vector<TR_OpaqueClassBlock*> &classes)
   {
   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Server will process a list of %u unloaded classes for clientUID %llu", (unsigned)classes.size(), (unsigned long long)_clientUID);
   OMR::CriticalSection processUnloadedClasses(getROMMapMonitor());
   for (TR_OpaqueClassBlock *clazz : classes)
      {
      auto it = _romClassMap.find((J9Class*) clazz);
      if (it == _romClassMap.end())
         continue; // unloaded class was never cached
      J9ROMClass *romClass = it->second.romClass;
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
               iter->second._IPData = nullptr;
               }
            _J9MethodMap.erase(j9method);
            }
         else
            {
            // This must never happen
            }
         
         }
      it->second.freeClassInfo();
      _romClassMap.erase(it);
      }
   }

TR_IPBytecodeHashTableEntry*
ClientSessionData::getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent)
   {
   *methodInfoPresent = false;
   TR_IPBytecodeHashTableEntry *ipEntry = nullptr;
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
   }

ClientSessionData::VMInfo *
ClientSessionData::getOrCacheVMInfo(JITaaS::J9ServerStream *stream)
   {
   if (!_vmInfo)
      {
      stream->write(JITaaS::J9ServerMessageType::VM_getVMInfo, JITaaS::Void());
      _vmInfo = new (PERSISTENT_NEW) VMInfo(std::get<0>(stream->read<VMInfo>()));
      }
   return _vmInfo;
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
// Side effects: _inUse is incremented on the ClientSessionData and the
// timeOflastAccess is updated with curent time.
ClientSessionData * 
ClientSessionHT::findOrCreateClientSession(uint64_t clientUID)
   {
   ClientSessionData *clientData = findClientSession(clientUID);
   if (!clientData)
      {
      // alocate a new ClientSessionData object and create a clientUID mapping
      clientData = new (PERSISTENT_NEW) ClientSessionData(clientUID);
      if (clientData)
         {
         _clientSessionMap[clientUID] = clientData;
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Server allocated data for a new clientUID %llu", (unsigned long long)clientUID);
         }
      else
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "ERROR: Server could not allocate client session data");
         }
      }
   return clientData;
   }

// Search the clientSessionHashtable for the given clientUID and return the
// data corresponding to the client.
// Must have compilation monitor in hand when calling this function.
// Side effects: _inUse is incremented on the ClientSessionData and the
// timeOflastAccess is updated with curent time.
ClientSessionData *
ClientSessionHT::findClientSession(uint64_t clientUID)
   {
   ClientSessionData *clientData = nullptr;
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
      iter->second->~ClientSessionData();
      TR_PersistentMemory::jitPersistentFree(iter->second); // delete the client data
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
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Server will purge session data for clientUID %llu", (unsigned long long)iter->first);
            iter->second->~ClientSessionData();
            TR_PersistentMemory::jitPersistentFree(iter->second); // delete the client data
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
   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   ClassInfoTuple &classInfo = *classInfoTuple;
   J9Method *methods = std::get<1>(classInfo);
   TR_OpaqueClassBlock *baseComponentClass = std::get<2>(classInfo);
   int32_t numDims = std::get<3>(classInfo);
   TR_OpaqueClassBlock *parentClass = std::get<4>(classInfo);
   auto &tmpInterfaces = std::get<5>(classInfo);
   auto interfaces = new (PERSISTENT_NEW) PersistentVector<TR_OpaqueClassBlock *>
      (tmpInterfaces.begin(), tmpInterfaces.end(),
       PersistentVector<TR_OpaqueClassBlock *>::allocator_type(TR::Compiler->persistentAllocator()));
   auto &methodTracingInfo = std::get<6>(classInfo);

   clientSessionData->getROMClassMap().insert({ clazz, { romClass, methods,
      baseComponentClass, numDims,
      nullptr, nullptr, parentClass, interfaces } });
   uint32_t numMethods = romClass->romMethodCount;
   J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   for (uint32_t i = 0; i < numMethods; i++)
      {
      clientSessionData->getJ9MethodMap().insert({ &methods[i], 
            {romMethod, nullptr, std::get<0>(methodTracingInfo[i]), std::get<1>(methodTracingInfo[i])} });
      romMethod = nextROMMethod(romMethod);
      }
   }

J9ROMClass *
JITaaSHelpers::getRemoteROMClassIfCached(ClientSessionData *clientSessionData, J9Class *clazz)
   {
   OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   return (it == clientSessionData->getROMClassMap().end()) ? nullptr : it->second.romClass;
   }

JITaaSHelpers::ClassInfoTuple
JITaaSHelpers::packRemoteROMClassInfo(J9Class *clazz, TR_J9VM *fe, TR_Memory *trMemory)
   {
   J9Method *methodsOfClass = (J9Method*) fe->getMethods((TR_OpaqueClassBlock*) clazz);
   int32_t numDims = 0;
   TR_OpaqueClassBlock *baseClass = fe->getBaseComponentClass((TR_OpaqueClassBlock *) clazz, numDims);
   TR_OpaqueClassBlock *parentClass = fe->getSuperClass((TR_OpaqueClassBlock *) clazz);

   uint32_t numMethods = clazz->romClass->romMethodCount;
   std::vector<std::tuple<bool, bool>> methodTracingInfo;
   methodTracingInfo.reserve(numMethods);
   for(uint32_t i = 0; i < numMethods; ++i)
      {
      methodTracingInfo.push_back(std::make_tuple(
         fe->isMethodEnterTracingEnabled((TR_OpaqueMethodBlock *) &methodsOfClass[i]),
         fe->isMethodExitTracingEnabled((TR_OpaqueMethodBlock *) &methodsOfClass[i])
         ));
      }

   return std::make_tuple(packROMClass(clazz->romClass, trMemory), methodsOfClass, baseClass, numDims, parentClass, TR::Compiler->cls.getITable((TR_OpaqueClassBlock *) clazz), methodTracingInfo);
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
JITaaSHelpers::getRemoteROMClass(J9Class *clazz, JITaaS::J9ServerStream *stream, TR_Memory *trMemory, ClassInfoTuple *classInfoTuple)
   {
   stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   *classInfoTuple = std::get<0>(recv);
   return romClassFromString(std::get<0>(*classInfoTuple), trMemory->trPersistentMemory());
   }
