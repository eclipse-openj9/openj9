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

#if defined(J9VM_OPT_SHARED_CLASSES)
#include "j9jitnls.h"
#endif

extern TR::Monitor *assumptionTableMutex;

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
            client->write(entryBytes, false, usePersistentCache);
            }
         else
            {
            client->write(std::string(), false, usePersistentCache);
            }
         // unlock the entry
         if (auto callGraphEntry = entry->asIPBCDataCallGraph())
            if (canPersist != IPBC_ENTRY_PERSIST_LOCK && callGraphEntry->isLocked())
               callGraphEntry->releaseEntry();
         }
      else // no valid info for specified bytecode index
         {
         client->write(std::string(), false, usePersistentCache);
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

      case J9ServerMessageType::getUnloadedClassRanges:
         {
         auto unloadedClasses = comp->getPersistentInfo()->getUnloadedClassAddresses();
         std::vector<TR_AddressRange> ranges(unloadedClasses->getNumberOfRanges());
            {
            OMR::CriticalSection getAddressSetRanges(assumptionTableMutex);
            unloadedClasses->getRanges(ranges);
            }
         client->write(ranges, unloadedClasses->getMaxRanges());
         break;
         }

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
      case J9ServerMessageType::VM_isMethodTracingEnabled:
         {
         auto method = std::get<0>(client->getRecvData<TR_OpaqueMethodBlock *>());
         client->write(fe->isMethodTracingEnabled(method));
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
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) &(methods[i]), 0, 0, fe, trMemory);
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
         vmInfo._isIProfilerEnabled = fe->getIProfiler();
         vmInfo._arrayletLeafLogSize = TR::Compiler->om.arrayletLeafLogSize();
         vmInfo._arrayletLeafSize = TR::Compiler->om.arrayletLeafSize();
         vmInfo._overflowSafeAllocSize = static_cast<uint64_t>(fe->getOverflowSafeAllocSize());
         vmInfo._compressedReferenceShift = TR::Compiler->om.compressedReferenceShift();

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
      case J9ServerMessageType::VM_isAnonymousClass:
         {
         auto recv = client->getRecvData<TR_OpaqueClassBlock *>();
         TR_OpaqueClassBlock *clazz = std::get<0>(recv);
         client->write(fe->isAnonymousClass(clazz));
         }
         break;
      case J9ServerMessageType::VM_dereferenceStaticAddress:
         {
         auto recv = client->getRecvData<void *, TR::DataType>();
         void *address = std::get<0>(recv);
         auto addressType = std::get<1>(recv);
         client->write(fe->dereferenceStaticFinalAddress(address, addressType));
         }
         break;
      case J9ServerMessageType::SharedCacheVM_persistThunk:
         {
         auto recv = client->getRecvData<std::string, J9SharedDataDescriptor>();
         auto signatureStr = std::get<0>(recv);
         auto dataDescriptor = std::get<1>(recv);
         const void *store= fe->_jitConfig->javaVM->sharedClassConfig->storeSharedData(fe->getCurrentVMThread(), signatureStr.data(), signatureStr.length(), &dataDescriptor);
         client->write(store);
         }
         break;
      case J9ServerMessageType::SharedCacheVM_findPersistentThunk:
         {
         auto recv = client->getRecvData<std::string>();
         auto signatureStr = std::get<0>(recv);
         J9SharedDataDescriptor firstDescriptor;
         J9VMThread *curThread = fe->getCurrentVMThread();
         firstDescriptor.address = NULL;

         fe->_jitConfig->javaVM->sharedClassConfig->findSharedData(curThread, signatureStr.data(), signatureStr.length(),
                                                         J9SHR_DATA_TYPE_AOTTHUNK, false, &firstDescriptor, NULL);
         client->write(firstDescriptor.address);
         }
         break;
      case J9ServerMessageType::mirrorResolvedJ9Method:
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

         client->write(ramMethod, methodInfo, unresolvedInCP);
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
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, method, fe, trMemory);

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
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, (uint32_t) vTableIndex, owningMethod, fe, trMemory);
                        
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
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) ramMethod, 0, owningMethod, fe, trMemory);

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
      case J9ServerMessageType::ResolvedMethod_getResolvedImproperInterfaceMethodAndMirror:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, I_32>();
         auto mirror = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         J9Method *j9method = nullptr;
            {
            TR::VMAccessCriticalSection getResolvedHandleMethod(fe);
            j9method = jitGetImproperInterfaceMethodFromCP(fe->vmThread(), mirror->cp(), cpIndex);
            }
         // create a mirror right away
         TR_ResolvedJ9JITaaSServerMethodInfo methodInfo;
         if (j9method)
            TR_ResolvedJ9JITaaSServerMethod::createResolvedMethodFromJ9MethodMirror(methodInfo, (TR_OpaqueMethodBlock *) j9method, 0, mirror, fe, trMemory);

         client->write(j9method, methodInfo);
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
      case J9ServerMessageType::ResolvedMethod_isInlineable:
         {
         TR_ResolvedJ9Method *mirror = std::get<0>(client->getRecvData<TR_ResolvedJ9Method *>());
         client->write(mirror->isInlineable(comp));
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
      case J9ServerMessageType::ResolvedMethod_isUnresolvedString:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t, bool>();
         auto mirror = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         auto optimizeForAOT = std::get<2>(recv);
         client->write(mirror->isUnresolvedString(cpIndex, optimizeForAOT));
         }
         break;
      case J9ServerMessageType::ResolvedMethod_stringConstant:
         {
         auto recv = client->getRecvData<TR_ResolvedJ9Method *, int32_t>();
         auto mirror = std::get<0>(recv);
         auto cpIndex = std::get<1>(recv);
         client->write(mirror->stringConstant(cpIndex));
         }
         break;
      case J9ServerMessageType::ResolvedRelocatableMethod_createResolvedRelocatableJ9Method:
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

         client->write(methodInfo, isRomClassForMethodInSC, sameLoaders, sameClass);
         }
         break;
      case J9ServerMessageType::ResolvedRelocatableMethod_storeValidationRecordIfNecessary:
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
         UDATA *classChain = fe->sharedCache()->rememberClass(definingClass);

         client->write(clazz, definingClass, classChain);
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

         int32_t leafClassNameLength;
         char *leafClassNameChars = fe->getClassNameChars((TR_OpaqueClassBlock*)leafClass, leafClassNameLength);
         bool isPrimitive = TR::Compiler->cls.isPrimitiveClass(comp, (TR_OpaqueClassBlock*)leafClass);
         client->write(arrayJ9Class, spreadPosition, arity, leafClass, std::string(leafClassNameChars, leafClassNameLength), isPrimitive);
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
         client->write(startPos, nextSignatureString, filtersList);
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
         auto recv = client->getRecvData<TR_OpaqueMethodBlock*>();
         auto method = std::get<0>(recv);
         TR_JITaaSClientIProfiler *iProfiler = (TR_JITaaSClientIProfiler *) fe->getIProfiler();
         client->write(iProfiler->serializeIProfilerMethodEntry(method));
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
      case J9ServerMessageType::IProfiler_setCallCount:
         {
         auto recv = client->getRecvData<TR_OpaqueMethodBlock*, int32_t, int32_t>();
         auto method = std::get<0>(recv);
         auto bcIndex = std::get<1>(recv);
         auto count = std::get<2>(recv);
         TR_IProfiler * iProfiler = fe->getIProfiler();
         iProfiler->setCallCount(method, bcIndex, count, comp);
         client->write(JITaaS::Void());
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

   // Prepare the parameters for the compilation request
   J9Class *clazz = J9_CLASS_FROM_METHOD(method);
   J9ROMClass *romClass = clazz->romClass;
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
   uint32_t romMethodOffset = uint32_t((uint8_t*) romMethod - (uint8_t*) romClass);
   std::string detailsStr = std::string((char*) &details, sizeof(TR::IlGeneratorMethodDetails));
   TR::CompilationInfo *compInfo = compInfoPT->getCompilationInfo();
   bool useAotCompilation = compInfoPT->getMethodBeingCompiled()->_useAotCompilation;

   if (compiler->getOption(TR_UseSymbolValidationManager))
       {
       // We do not want client to validate anything during compilation, because
       // validations are done on the server. Creating heuristic region makes SVM assume that everything is valdiated.
       compiler->enterHeuristicRegion();
       }

   if (compiler->isOptServer())
      compiler->setOption(TR_Server);
   auto classInfoTuple = JITaaSHelpers::packRemoteROMClassInfo(clazz, compiler->fej9vm(), compiler->trMemory());
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
      // message just in case we block in the write operation   
      releaseVMAccess(vmThread);

      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,
            "Client sending compReq seqNo=%u to server for method %s @ %s.",
            seqNo, compiler->signature(), compiler->getHotnessName());
         }
      client.buildCompileRequest(TR::comp()->getPersistentInfo()->getJITaaSId(), romMethodOffset, 
                                 method, clazz, compiler->getMethodHotness(), detailsStr, details.getType(), unloadedClasses,
                                 classInfoTuple, optionsStr, recompMethodInfoStr, seqNo, useAotCompilation);
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
         if (compiler->getOption(TR_EnableSymbolValidationManager))
            {
            // compilation is done, now we need client to validate all of the records accumulated by the server,
            // so need to exit heuristic region.
            compiler->exitHeuristicRegion();
            }

         TR_ASSERT(codeCacheStr.size(), "must have code cache");
         TR_ASSERT(dataCacheStr.size(), "must have data cache");

         // relocate the received compiled code
         metaData = remoteCompilationEnd(vmThread, compiler, compilee, method, compInfoPT, codeCacheStr, dataCacheStr);

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

         int compilationSequenceNumber = compiler->getOptions()->writeLogFileFromServer(logFileStr);
         if (TR::Options::getCmdLineOptions()->getOption(TR_EnableJITaaSDoLocalCompilesForRemoteCompiles) && compilationSequenceNumber)
            {
            // if compilationSequenceNumber is not 0, vlog from server is enabled
            // double compile is also controlled by TR_EnableJITaaSDoubleCompile option
            // try to perform an equivalent local compilation and generate vlog
            // append the same compilationSequenceNumber in the filename
            intptr_t rtn = 0;
            compiler->getOptions()->setLogFileForClientOptions(compilationSequenceNumber);
            if ((rtn = compiler->compile()) != COMPILATION_SUCCEEDED)
               {
               TR_ASSERT(false, "Compiler returned non zero return code %d\n", rtn);
               compiler->failCompilation<TR::CompilationException>("Compilation Failure");
               }
            compiler->getOptions()->closeLogFileForClientOptions();
            }

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
   void *startPC = NULL;
   TR_J9VM *fe = comp->fej9vm();
   TR_MethodToBeCompiled *entry = compInfoPT->getMethodBeingCompiled();
   J9JITConfig *jitConfig = compInfoPT->getJitConfig();
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   const J9JITDataCacheHeader *storedCompiledMethod = nullptr;
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
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,
                                           "JITaaS Relocation failure: %d",
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
      bool safeToStore;
      TR_ASSERT(entry->_useAotCompilation, "entry must be an AOT compilation");
      TR_ASSERT(entry->isRemoteCompReq(), "entry must be a remote compilation");

      if (static_cast<TR_JitPrivateConfig *>(jitConfig->privateConfig)->aotValidHeader == TR_yes)
         {
         safeToStore = true;
         }
      else if (static_cast<TR_JitPrivateConfig *>(jitConfig->privateConfig)->aotValidHeader == TR_maybe)
         {
         // If validation has been performed, then a header already existed
         // or one was already been created in this JVM
         safeToStore = entry->_compInfoPT->reloRuntime()->storeAOTHeader(jitConfig->javaVM, static_cast<TR_J9SharedCacheVM *>(fe), vmThread);
         }
      else
         {
         safeToStore = false;
         }

      const U_8 *dataStart;
      const U_8 *codeStart;
      UDATA dataSize, codeSize;
      UDATA classReloAmount = 0;
      const U_8 *returnCode = NULL;

      dataStart = (U_8 *)(&dataCacheStr[0]);
      dataSize  = dataCacheStr.size();
      codeStart = (U_8 *)(&codeCacheStr[0]);
      codeSize  = codeCacheStr.size();

      J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
      if (safeToStore)
         {
         storedCompiledMethod =
            reinterpret_cast<const J9JITDataCacheHeader*>(
               jitConfig->javaVM->sharedClassConfig->storeCompiledMethod(
                  vmThread,
                  romMethod,
                  dataStart,
                  dataSize,
                  codeStart,
                  codeSize,
                  0));
         switch(reinterpret_cast<uintptr_t>(storedCompiledMethod))
            {
            case J9SHR_RESOURCE_STORE_FULL:
               {
               if (jitConfig->javaVM->sharedClassConfig->verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE)
                  j9nls_printf( PORTLIB, J9NLS_WARNING,  J9NLS_RELOCATABLE_CODE_STORE_FULL);
               TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::SHARED_CACHE_FULL);
               TR::CompilationInfo::disableAOTCompilations();
               }
               break;
            case J9SHR_RESOURCE_STORE_ERROR:
               {
               if (jitConfig->javaVM->sharedClassConfig->verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE)
                  j9nls_printf( PORTLIB, J9NLS_WARNING,  J9NLS_RELOCATABLE_CODE_STORE_ERROR);
               TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::SHARED_CACHE_STORE_ERROR);
               TR::Options::getAOTCmdLineOptions()->setOption(TR_NoLoadAOT);
               TR::CompilationInfo::disableAOTCompilations();
               }
            }
         }
      else
         {
         if (TR::Options::getAOTCmdLineOptions()->getVerboseOption(TR_VerboseJITaaS))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, " Failed AOT cache validation");
            }

         TR::CompilationInfo::disableAOTCompilations();
         }

#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)

      TR_Debug *debug = TR::Options::getDebug();
      bool canRelocateMethod = false;
      if (debug)
         {
         TR_FilterBST *filter = NULL;
         J9UTF8 *className = ((TR_ResolvedJ9Method*)comp->getCurrentMethod())->_className;
         J9UTF8 *name = ((TR_ResolvedJ9Method*)comp->getCurrentMethod())->_name;
         J9UTF8 *signature = ((TR_ResolvedJ9Method*)comp->getCurrentMethod())->_signature;
         char *methodSignature;
         char arr[1024];
         int32_t len = J9UTF8_LENGTH(className) + J9UTF8_LENGTH(name) + J9UTF8_LENGTH(signature) + 3;
         if (len < 1024)
            methodSignature = arr;
         else
            methodSignature = (char *) TR_MemoryBase::jitPersistentAlloc(len);

         if (methodSignature)
            {
            sprintf(methodSignature, "%.*s.%.*s%.*s", J9UTF8_LENGTH(className), utf8Data(className), J9UTF8_LENGTH(name), utf8Data(name), J9UTF8_LENGTH(signature), utf8Data(signature));
            //printf("methodSig: %s\n", methodSignature);

            if (debug->methodSigCanBeRelocated(methodSignature, filter))
               canRelocateMethod = true;
            }
         else
            canRelocateMethod = true;

         if (methodSignature && (len >= 1024))
            TR_MemoryBase::jitPersistentFree(methodSignature);
         }
      else
         {
         // Prevent the relocation if specific option is given
         if (!comp->getOption(TR_DisableDelayRelocationForAOTCompilations))
            canRelocateMethod = false;
         else
            canRelocateMethod = true;
         }

      if (canRelocateMethod)
         {
         J9JITDataCacheHeader *cacheEntry;

         TR_ASSERT_FATAL(comp->cg(), "CodeGenerator must be allocated");

         cacheEntry = (J9JITDataCacheHeader *)dataStart;

         int32_t returnCode = 0;

         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_JITaaS,
               "Applying JITaaS remote AOT relocations to newly AOT compiled body for %s @ %s",
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
               cacheEntry,
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
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "JITaaS Client successfully relocated metadata for %s", comp->signature());
               }

            if (J9_EVENT_IS_HOOKED(jitConfig->javaVM->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
               {
               TR::CompilationInfo::addJ9HookVMDynamicCodeLoadForAOT(vmThread, method, jitConfig, relocatedMetaData);
               }
            }
         else
            {
            entry->_doNotUseAotCodeFromSharedCache = true;
            entry->_compErrCode = returnCode;

            if (entry->_compilationAttemptsLeft > 0)
               {
               entry->_tryCompilingAgain = true;
               }
            }
         }
#endif /* J9VM_INTERP_AOT_RUNTIME_SUPPORT */
      }
#endif // defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))

   return relocatedMetaData;
   }

void
outOfProcessCompilationEnd(
   TR::IlGeneratorMethodDetails &details,
   J9JITConfig *jitConfig,
   TR_FrontEnd *fe,
   TR_MethodToBeCompiled *entry,
   TR::Compilation *comp)
   {
   entry->_tryCompilingAgain = false; // TODO: Need to handle recompilations gracefully when relocation fails

   TR::CodeCache *codeCache = comp->cg()->getCodeCache();
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
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "compThreadID=%d has successfully compiled %s",
         entry->_compInfoPT->getCompThreadId(), entry->_compInfoPT->getCompilation()->signature());
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

ClientSessionData::ClientSessionData(uint64_t clientUID, uint32_t seqNo) : 
   _clientUID(clientUID), _expectedSeqNo(seqNo), _maxReceivedSeqNo(seqNo), _OOSequenceEntryList(NULL),
   _chTableClassMap(decltype(_chTableClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _romClassMap(decltype(_romClassMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _J9MethodMap(decltype(_J9MethodMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _systemClassByNameMap(decltype(_systemClassByNameMap)::allocator_type(TR::Compiler->persistentAllocator())),
   _unloadedClassAddresses(NULL),
   _requestUnloadedClasses(true),
   _staticFinalDataMap(decltype(_staticFinalDataMap)::allocator_type(TR::Compiler->persistentAllocator()))
   {
   updateTimeOfLastAccess();
   _javaLangClassPtr = nullptr;
   _inUse = 1;
   _numActiveThreads = 0;
   _romMapMonitor = TR::Monitor::create("JIT-JITaaSROMMapMonitor");
   _systemClassMapMonitor = TR::Monitor::create("JIT-JITaaSSystemClassMapMonitor");
   _sequencingMonitor = TR::Monitor::create("JIT-JITaaSSequencingMonitor");
   _vmInfo = nullptr;
   _staticMapMonitor = TR::Monitor::create("JIT-JITaaSStaticMapMonitor");
   _markedForDeletion = false;
   }

ClientSessionData::~ClientSessionData()
   {
   clearCaches();
   _romMapMonitor->destroy();
   _systemClassMapMonitor->destroy();
   _sequencingMonitor->destroy();
   if (_unloadedClassAddresses)
      {
      _unloadedClassAddresses->destroy();
      jitPersistentFree(_unloadedClassAddresses);
      }
   _staticMapMonitor->destroy();
   if (_vmInfo)
      jitPersistentFree(_vmInfo);
   }

void
ClientSessionData::processUnloadedClasses(JITaaS::J9ServerStream *stream, const std::vector<TR_OpaqueClassBlock*> &classes)
   {
   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Server will process a list of %u unloaded classes for clientUID %llu", (unsigned)classes.size(), (unsigned long long)_clientUID);

   bool updateUnloadedClasses = true;
   if (_requestUnloadedClasses)
      {
      stream->write(JITaaS::J9ServerMessageType::getUnloadedClassRanges, JITaaS::Void());

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

   OMR::CriticalSection processUnloadedClasses(getROMMapMonitor());

   for (auto clazz : classes)
      {
      if (updateUnloadedClasses)
         _unloadedClassAddresses->add((uintptrj_t)clazz);

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


void
ClientSessionData::clearCaches()
   {
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
         it.second._IPData = nullptr;
         }
      }
   _J9MethodMap.clear();
   // Free memory for j9class info
   for (auto it : _romClassMap)
      {
      it.second.freeClassInfo();
      }
   _romClassMap.clear();
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
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Server allocated data for a new clientUID %llu", (unsigned long long)clientUID);
         }
      else
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "ERROR: Server could not allocate client session data");
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
   ClientSessionData *clientData = nullptr;
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
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Server will purge session data for clientUID %llu", (unsigned long long)iter->first);
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
   classInfoStruct._remoteROMStringsCache = nullptr;
   classInfoStruct._fieldOrStaticNameCache = nullptr;
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
   classInfoStruct._classOfStaticCache = nullptr;
   classInfoStruct._constantClassPoolCache = nullptr;

   clientSessionData->getROMClassMap().insert({ clazz, classInfoStruct});

   uint32_t numMethods = romClass->romMethodCount;
   J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   for (uint32_t i = 0; i < numMethods; i++)
      {
      clientSessionData->getJ9MethodMap().insert({ &methods[i], 
            {romMethod, nullptr, static_cast<bool>(methodTracingInfo[i])} });
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
   return std::make_tuple(packROMClass(clazz->romClass, trMemory), methodsOfClass, baseClass, numDims, parentClass, TR::Compiler->cls.getITable((TR_OpaqueClassBlock *) clazz), methodTracingInfo, classHasFinalFields, classDepthAndFlags, classInitialized, byteOffsetToLockword, leafComponentClass, classLoader, hostClass, componentClass, arrayClass, totalInstanceSize);
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

bool
JITaaSHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITaaS::J9ServerStream *stream, ClassInfoDataType dataType, void *data)
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
   stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
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
   return true;
   }

bool
JITaaSHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITaaS::J9ServerStream *stream, ClassInfoDataType dataType1, void *data1, ClassInfoDataType dataType2, void *data2)
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
   stream->write(JITaaS::J9ServerMessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
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
   return true;
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
      default :
         {
         TR_ASSERT(0, "Class Info not supported %u \n", dataType);
         }
         break;
      }
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


// waitForMyTurn needs to be exeuted with sequencingMonitor in hand
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
      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "compThreadID=%d (entry=%p) doing a timed wait for %d ms",
         getCompThreadId(), &entry, (int32_t)waitTimeMillis);

      intptr_t monitorStatus = entry.getMonitor()->wait_timed(waitTimeMillis, 0); // 0 or J9THREAD_TIMED_OUT
      if (monitorStatus == 0) // thread was notified
         {
         entry.getMonitor()->exit();
         clientSession->getSequencingMonitor()->enter();

         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Parked compThreadID=%d (entry=%p) seqNo=%u was notified.",
               getCompThreadId(), &entry, seqNo);
         // will verify condition again to see if expectedSeqNo has advanced enough
         }
      else
         {
         entry.getMonitor()->exit();
         TR_ASSERT(monitorStatus == J9THREAD_TIMED_OUT, "Unexpected monitor state");
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompFailure, TR_VerboseJITaaS, TR_VerbosePerformance))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "compThreadID=%d timed-out while waiting for seqNo=%d (entry=%p)", 
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
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,
                  "compThreadID=%d has cleared the session caches for clientUID=%llu expectedSeqNo=%u detachedEntry=%p",
                  getCompThreadId(), clientSession->getClientUID(), clientSession->getExpectedSeqNo(), detachedEntry);
            }
         else
            {
            // Wait until all active threads have been drained
            // and the head of the list has cleared the caches
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,
                  "compThreadID=%d which previously timed-out will go to sleep again. Possible reasons numActiveThreads=%d waitToBeNotified=%d",
                  getCompThreadId(), clientSession->getNumActiveThreads(), getWaitToBeNotified());
            }
         }
      } while (seqNo > clientSession->getExpectedSeqNo());
   }


void
TR::CompilationInfoPerThreadRemote::processEntry(TR_MethodToBeCompiled &entry, J9::J9SegmentProvider &scratchSegmentProvider)
   {
   bool abortCompilation = false;
   uint64_t clientId = 0;
   TR::CompilationInfo *compInfo = getCompilationInfo();
   J9VMThread *compThread = getCompilationThread();
   JITaaS::J9ServerStream *stream = entry._stream;
   setMethodBeingCompiled(&entry); // must have compilation monitor
   entry._compInfoPT = this; // create the reverse link
   _vm = TR_J9VMBase::get(_jitConfig, compThread, TR_J9VMBase::J9_SERVER_VM);
   // update the last time the compilation thread had to do something.
   compInfo->setLastReqStartTime(compInfo->getPersistentInfo()->getElapsedTime());
  
   _recompilationMethodInfo = NULL;
   // Release compMonitor before doing the blocking read
   compInfo->releaseCompMonitor(compThread);

   char * clientOptions = NULL;
   TR_OptimizationPlan *optPlan = NULL;
   try
      {
      auto req = stream->read<uint64_t, uint32_t, J9Method *, J9Class*, TR_Hotness, std::string,
         J9::IlGeneratorMethodDetailsType, std::vector<TR_OpaqueClassBlock*>,
         JITaaSHelpers::ClassInfoTuple, std::string, std::string, uint32_t, bool>();

      clientId                  = std::get<0>(req);
      uint32_t romMethodOffset  = std::get<1>(req);
      J9Method *ramMethod       = std::get<2>(req);
      J9Class *clazz            = std::get<3>(req);
      TR_Hotness optLevel       = std::get<4>(req);
      std::string detailsStr    = std::get<5>(req);
      auto detailsType          = std::get<6>(req);
      auto &unloadedClasses     = std::get<7>(req);
      auto &classInfoTuple      = std::get<8>(req);
      std::string clientOptStr  = std::get<9>(req);
      std::string recompInfoStr = std::get<10>(req);
      uint32_t seqNo            = std::get<11>(req); // sequence number at the client
      entry._useAotCompilation  = std::get<12>(req);

      if (entry._useAotCompilation)
         {
         _vm = TR_J9VMBase::get(_jitConfig, compThread, TR_J9VMBase::J9_SHARED_CACHE_SERVER_VM);
         }

      if (!ramMethod)
         {
         compInfo->acquireCompMonitor(compThread); //need to acquire compilation monitor for both deleting the client data and the setting the thread state to COMPTHREAD_SIGNAL_SUSPEND.
         bool result = compInfo->getClientSessionHT()->deleteClientSession(clientId, true);
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            {
            if (!result)
               {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,"Message to delete Client (%llu) received.Data for client not deleted", (unsigned long long)clientId);
               }
            else
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,"Message to delete Client (%llu) received.Data for client deleted", (unsigned long long)clientId);
               }
            }
         compInfo->releaseCompMonitor(compThread);
         // This is a null message for termination so abort compilation.
         abortCompilation = true;
         }
      else
         {
         //if (seqNo == 100)
         //   throw JITaaS::StreamFailure(); // stress testing

         stream->setClientId(clientId);
         setSeqNo(seqNo); // memorize the sequence number of this request

         bool sessionDataWasEmpty = false;
         ClientSessionData *clientSession = NULL;
         {
         // Get a pointer to this client's session data
         // Obtain monitor RAII style because creating a new hastable entry may throw bad_alloc
         OMR::CriticalSection compilationMonitorLock(compInfo->getCompilationMonitor());
         compInfo->getClientSessionHT()->purgeOldDataIfNeeded(); // Try to purge old data
         if (!(clientSession = compInfo->getClientSessionHT()->findOrCreateClientSession(clientId, seqNo, &sessionDataWasEmpty)))
            throw std::bad_alloc();

         setClientData(clientSession); // Cache the session data into CompilationInfoPerThreadRemote object
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "compThreadID=%d %s clientSessionData=%p for clientUID=%llu seqNo=%u",
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
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Out-of-sequence msg detected for clientUID=%llu seqNo=%u > expectedSeqNo=%u. Parking compThreadID=%d (entry=%p)",
               (unsigned long long)clientId, seqNo, clientSession->getExpectedSeqNo(), getCompThreadId(), &entry);

            waitForMyTurn(clientSession, entry);
            }
         else if (seqNo < clientSession->getExpectedSeqNo())
            {
            // Note that it is possible for seqNo to be smaller than expectedSeqNo.
            // This could happen for instance for the very first message that arrives late.
            // In that case, the second message would have created the client session and 
            // written its own seqNo into expectedSeqNo. We should avoid processing stale updates
            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompFailure, TR_VerboseJITaaS, TR_VerbosePerformance))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Discarding older msg for clientUID=%llu seqNo=%u < expectedSeqNo=%u compThreadID=%d",
               (unsigned long long)clientId, seqNo, clientSession->getExpectedSeqNo(), getCompThreadId());
            clientSession->getSequencingMonitor()->exit();
            throw JITaaS::StreamOOO();
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
            stream->write(JITaaS::J9ServerMessageType::CHTable_getClassInfoUpdates, JITaaS::Void());
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
         uint32_t newSeqNo = std::max(seqNo + 1, clientSession->getExpectedSeqNo());
         clientSession->setExpectedSeqNo(newSeqNo);

         // Notify a possible waiting thread that arrived out of sequence
         // and take that entry out of the OOSequenceEntryList
         TR_MethodToBeCompiled *nextEntry = clientSession->getOOSequenceEntryList();
         if (nextEntry)
            {
            uint32_t nextWaitingSeqNo = ((CompilationInfoPerThreadRemote*)(nextEntry->_compInfoPT))->getSeqNo();
            if (nextWaitingSeqNo == seqNo + 1)
               {
               clientSession->notifyAndDetachFirstWaitingThread();

               if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "compThreadID=%d notifying out-of-sequence thread %d for clientUID=%llu seqNo=%u (entry=%p)",
                     getCompThreadId(), nextEntry->_compInfoPT->getCompThreadId(), (unsigned long long)clientId, nextWaitingSeqNo, nextEntry);
               }
            }
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
         if (!(optPlan = TR_OptimizationPlan::alloc(optLevel)))
            throw std::bad_alloc();
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
         }
      }
   catch (const JITaaS::StreamFailure &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Stream failed while compThreadID=%d was reading the compilation request: %s", 
            getCompThreadId(), e.what());
      stream->cancel(); // This does nothing for raw sockets
      abortCompilation = true;
      }
   catch (const JITaaS::StreamCancel &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Stream cancelled by client while compThreadID=%d was reading the compilation request: %s",
            getCompThreadId(), e.what());
      abortCompilation = true;
      }
   catch (const JITaaS::StreamOOO &e)
      {
      // Error message was printed when the exception was thrown
      // TODO: the client should handle this error code
      stream->finishCompilation(compilationStreamLostMessage); // the client should recognize this code and retry
      abortCompilation = true;
      }
   catch (const std::bad_alloc &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Server out of memory in processEntry: %s", e.what());
      stream->finishCompilation(compilationLowPhysicalMemory);
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

      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         {
         if (getClientData())
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "compThreadID=%d did an early abort for clientUID=%llu seqNo=%u",
               getCompThreadId(), getClientData()->getClientUID(), getSeqNo());
         else
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "compThreadID=%d did an early abort",
               getCompThreadId());
         }

      compInfo->acquireCompMonitor(compThread);
      releaseVMAccess(compThread);
      compInfo->decreaseQueueWeightBy(entry._weight);

      // Put the request back into the pool
      setMethodBeingCompiled(NULL); // Must have the compQmonitor
      compInfo->recycleCompilationEntry(&entry);

      // Reset the pointer to the cached client session data
      if (getClientData())
         {
         getClientData()->decInUse();  // We have the compilation monitor so it's safe to access the inUse counter
         if (getClientData()->getInUse() == 0)
            {
            bool result = compInfo->getClientSessionHT()->deleteClientSession(clientId, false);
            if (result)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,"client (%llu) deleted", (unsigned long long)clientId);
               }
            }
         setClientData(nullptr);
         }
#ifdef JITAAS_USE_RAW_SOCKETS
      // Delete server stream
      stream->~J9ServerStream();
      TR_Memory::jitPersistentFree(stream);
#endif
      return;
      }

   _thunksToBeRelocated.clear();
   _invokeExactThunksToBeRelocated.clear();


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

   // Decrement number of active threads before _inUse
   getClientData()->decNumActiveThreads(); // We hold compMonitor so there is no accounting problem
   getClientData()->decInUse();  // We have the compMonitor so it's safe to access the inUse counter
   if (getClientData()->getInUse() == 0)
      {
      bool result = compInfo->getClientSessionHT()->deleteClientSession(clientId, false);
      if (result)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS,"client (%llu) deleted", (unsigned long long)clientId);
         }
      }

   setClientData(nullptr); // Reset the pointer to the cached client session data

   _customClassByNameMap.clear(); // reset before next compilation starts
   entry._newStartPC = startPC;
   // Update statistics regarding the compilation status (including compilationOK)
   compInfo->updateCompilationErrorStats((TR_CompilationErrorCode)entry._compErrCode);

   TR_OptimizationPlan::freeOptimizationPlan(entry._optimizationPlan); // we no longer need the optimization plan
   // decrease the queue weight
   compInfo->decreaseQueueWeightBy(entry._weight);
   // Put the request back into the pool
   setMethodBeingCompiled(NULL);
   compInfo->recycleCompilationEntry(&entry);

   compInfo->printQueue();

   // Release the queue slot monitor
   //
   entry.releaseSlotMonitor(compThread);

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

#ifdef JITAAS_USE_RAW_SOCKETS
   // Delete server stream
   stream->~J9ServerStream();
   TR_Memory::jitPersistentFree(stream);
#endif
   }

void
TR::CompilationInfoPerThreadRemote::clearIProfilerMap(TR_Memory *trMemory)
   {
   if (_methodIPDataPerComp)
      {
      // the map and all of its values should be freed automatically at the end of compilation,
      // since they were allocated on the compilation heap memory
      _methodIPDataPerComp = NULL;
      }
   }

bool
TR::CompilationInfoPerThreadRemote::cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry)
   {
   TR_Memory *trMemory = getCompilation()->trMemory();
   if (!_methodIPDataPerComp)
      {
      // first time cacheIProfilerInfo called during current compilation
      _methodIPDataPerComp = new (trMemory->trHeapMemory()) IPTableHeap_t(IPTableHeap_t::allocator_type(trMemory->heapMemoryRegion()));

      if (!_methodIPDataPerComp)
         // could not allocate from heap memory
         return false;
      }

   IPTableHeapEntry *entryMap = NULL;
   auto it = _methodIPDataPerComp->find((J9Method *) method);
   if (it == _methodIPDataPerComp->end())
      {
      // caching new method, initialize an entry map
      entryMap = new (trMemory->trHeapMemory()) IPTableHeapEntry(IPTableHeapEntry::allocator_type(trMemory->heapMemoryRegion()));
      if (!entryMap)
         // could not allocate from heap memory
         return false;
      _methodIPDataPerComp->insert(std::make_pair((J9Method *) method, entryMap));
      }
   else
      {
      entryMap = it->second;
      }

   if (entry)
      // entry could be NULL
      // if all entries in a method are NULL, entryMap will be initialized but empty
      entryMap->insert(std::make_pair(byteCodeIndex, entry));

   return true;
   }

TR_IPBytecodeHashTableEntry*
TR::CompilationInfoPerThreadRemote::getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent)
   {
   *methodInfoPresent = false;
   if (!_methodIPDataPerComp)
      // nothing has been cached during current compilation, so table is uninitialized
      return NULL;

   TR_IPBytecodeHashTableEntry *ipEntry = NULL;
   auto it = _methodIPDataPerComp->find((J9Method *) method);
   if (it != _methodIPDataPerComp->end())
      {
      // methodInfoPresent=true means that we cached info for this method,
      // but entry for this bytecode might not exist, which means it's NULL
      *methodInfoPresent = true;
      IPTableHeapEntry *entryMap = it->second;
      auto entryIt = entryMap->find(byteCodeIndex);
      if (entryIt != entryMap->end())
         ipEntry = entryIt->second;
      }
   return ipEntry;
   }
