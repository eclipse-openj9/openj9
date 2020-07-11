/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "control/JITServerHelpers.hpp"

#include "control/CompilationRuntime.hpp"
#include "control/JITServerCompilationThread.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/Statistics.hpp"
#include "net/CommunicationStream.hpp"



uint32_t     JITServerHelpers::serverMsgTypeCount[] = {};
uint64_t     JITServerHelpers::_waitTimeMs = 1000;
bool         JITServerHelpers::_serverAvailable = true;
uint64_t     JITServerHelpers::_nextConnectionRetryTime = 0;
TR::Monitor *JITServerHelpers::_clientStreamMonitor = NULL;

static size_t
methodStringsLength(J9ROMMethod *method)
   {
   J9UTF8 *name = J9ROMMETHOD_NAME(method);
   J9UTF8 *sig = J9ROMMETHOD_SIGNATURE(method);
   return (name->length + sig->length + (2 * sizeof(U_16)));
   }

// Packs a ROMClass into a std::string to be transferred to the server.
// The name and signature of all methods are appended to the end of the cloned class body and the
// self referential pointers to them are updated to deal with possible interning. The method names
// and signature are needed on the server but may be interned globally on the client.
static std::string
packROMClass(J9ROMClass *origRomClass, TR_Memory *trMemory)
   {
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

// insertIntoOOSequenceEntryList needs to be executed with sequencingMonitor in hand.
// This method belongs to ClientSessionData, but is temporarily moved here to be able
// to push the ClientSessionData related code as a standalone piece.
void 
JITServerHelpers::insertIntoOOSequenceEntryList(ClientSessionData *clientData, TR_MethodToBeCompiled *entry)
   {
   uint32_t seqNo = ((TR::CompilationInfoPerThreadRemote*)(entry->_compInfoPT))->getSeqNo();
   TR_MethodToBeCompiled *crtEntry = clientData->getOOSequenceEntryList();
   TR_MethodToBeCompiled *prevEntry = NULL;
   while (crtEntry && (seqNo > ((TR::CompilationInfoPerThreadRemote*)(crtEntry->_compInfoPT))->getSeqNo()))
      {
      prevEntry = crtEntry;
      crtEntry = crtEntry->_next;
      }
   entry->_next = crtEntry;
   if (prevEntry)
      prevEntry->_next = entry;
   else
      clientData->setOOSequenceEntryList(entry);
   }

void
JITServerHelpers::printJITServerMsgStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
   unsigned totalMsgCount = 0;	   
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   j9tty_printf(PORTLIB, "JITServer Message Type Statistics:\n");
   j9tty_printf(PORTLIB, "Type# #called");
#ifdef MESSAGE_SIZE_STATS  
   j9tty_printf(PORTLIB, "\t\tMax\t\tMin\t\tMean\t\tStdDev\t\tSum");
#endif // defined(MESSAGE_SIZE_STATS)
   j9tty_printf(PORTLIB, "\t\tTypeName\n");
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      for (int i = 0; i < JITServer::MessageType_ARRAYSIZE; ++i)
         {
         if (JITServerHelpers::serverMsgTypeCount[i] > 0)
            {
            j9tty_printf(PORTLIB, "#%04d %7u", i, JITServerHelpers::serverMsgTypeCount[i]);
#ifdef MESSAGE_SIZE_STATS            
            j9tty_printf(PORTLIB, "\t%f\t%f\t%f\t%f\t%f", JITServer::CommunicationStream::collectMsgStat[i].maxVal(), 
                     JITServer::CommunicationStream::collectMsgStat[i].minVal(), JITServer::CommunicationStream::collectMsgStat[i].mean(),
                     JITServer::CommunicationStream::collectMsgStat[i].stddev(), JITServer::CommunicationStream::collectMsgStat[i].sum());
#endif // defined(MESSAGE_SIZE_STATS)
            j9tty_printf(PORTLIB, "\t\t%s\n", JITServer::messageNames[i]);
            totalMsgCount += JITServerHelpers::serverMsgTypeCount[i];
            }
         }
      if (JITServerHelpers::serverMsgTypeCount[0])
         j9tty_printf(PORTLIB, "Total number of messages: %d. Average number of messages per compilation:%f\n", totalMsgCount, totalMsgCount/float(JITServerHelpers::serverMsgTypeCount[0]));
      }
   else if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      // to print in server, run ./jitserver -Xdump:jit:events=user
      // then kill -3 <pidof jitserver>
#ifdef MESSAGE_SIZE_STATS  
      for (int i = 0; i < JITServer::MessageType_ARRAYSIZE; ++i)
         {
         if (JITServer::CommunicationStream::collectMsgStat[i].samples() > 0)
            { 
            j9tty_printf(PORTLIB, "#%04d %7u", i, JITServer::CommunicationStream::collectMsgStat[i].samples());            
            j9tty_printf(PORTLIB, "\t%f\t%f\t%f\t%f\t%f", JITServer::CommunicationStream::collectMsgStat[i].maxVal(), 
                     JITServer::CommunicationStream::collectMsgStat[i].minVal(), JITServer::CommunicationStream::collectMsgStat[i].mean(),
                     JITServer::CommunicationStream::collectMsgStat[i].stddev(), JITServer::CommunicationStream::collectMsgStat[i].sum());

            j9tty_printf(PORTLIB, "\t\t%s\n", JITServer::messageNames[i]);
            totalMsgCount += JITServer::CommunicationStream::collectMsgStat[i].samples();
            }
         }
      j9tty_printf(PORTLIB, "Total number of messages: %u\n", totalMsgCount);
#endif // defined(MESSAGE_SIZE_STATS)
      }
   }

void
JITServerHelpers::printJITServerCHTableStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
#ifdef COLLECT_CHTABLE_STATS
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   j9tty_printf(PORTLIB, "JITServer CHTable Statistics:\n");
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      JITClientPersistentCHTable *table = (JITClientPersistentCHTable*)compInfo->getPersistentInfo()->getPersistentCHTable();
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
      JITServerPersistentCHTable *table = (JITServerPersistentCHTable*)compInfo->getPersistentInfo()->getPersistentCHTable();
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

void
JITServerHelpers::printJITServerCacheStats(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      auto clientSessionHT = compInfo->getClientSessionHT();
      clientSessionHT->printStats();
      }
   }

void 
JITServerHelpers::cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple)
   {
   ClientSessionData::ClassInfo classInfo;
   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find((J9Class*)clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, classInfoTuple, classInfo);
      }
   }

void
JITServerHelpers::cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple, ClientSessionData::ClassInfo &classInfoStruct)
   {
   ClassInfoTuple &classInfo = *classInfoTuple;

   classInfoStruct._romClass = romClass;
   classInfoStruct._methodsOfClass = std::get<1>(classInfo);
   J9Method *methods = classInfoStruct._methodsOfClass;
   classInfoStruct._baseComponentClass = std::get<2>(classInfo);
   classInfoStruct._numDimensions = std::get<3>(classInfo);
   classInfoStruct._parentClass = std::get<4>(classInfo);
   auto &tmpInterfaces = std::get<5>(classInfo);
   classInfoStruct._interfaces = new (PERSISTENT_NEW) PersistentVector<TR_OpaqueClassBlock *>
      (tmpInterfaces.begin(), tmpInterfaces.end(),
       PersistentVector<TR_OpaqueClassBlock *>::allocator_type(TR::Compiler->persistentAllocator()));
   auto &methodTracingInfo = std::get<6>(classInfo);
   classInfoStruct._classHasFinalFields = std::get<7>(classInfo);
   classInfoStruct._classDepthAndFlags = std::get<8>(classInfo);
   classInfoStruct._classInitialized = std::get<9>(classInfo);
   classInfoStruct._byteOffsetToLockword = std::get<10>(classInfo);
   classInfoStruct._leafComponentClass = std::get<11>(classInfo);
   classInfoStruct._classLoader = std::get<12>(classInfo);
   classInfoStruct._hostClass = std::get<13>(classInfo);
   classInfoStruct._componentClass = std::get<14>(classInfo);
   classInfoStruct._arrayClass = std::get<15>(classInfo);
   classInfoStruct._totalInstanceSize = std::get<16>(classInfo);
   classInfoStruct._remoteRomClass = std::get<17>(classInfo);
   classInfoStruct._constantPool = (J9ConstantPool *)std::get<18>(classInfo);
   classInfoStruct._classFlags = std::get<19>(classInfo);
   classInfoStruct._classChainOffsetOfIdentifyingLoaderForClazz = std::get<20>(classInfo);
   clientSessionData->getROMClassMap().insert({ clazz, classInfoStruct});

   auto &origROMMethods = std::get<21>(classInfo);

   uint32_t numMethods = romClass->romMethodCount;
   J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
   for (uint32_t i = 0; i < numMethods; i++)
      {
      clientSessionData->getJ9MethodMap().insert({&methods[i],
            {romMethod, origROMMethods[i], NULL, static_cast<bool>(methodTracingInfo[i]), (TR_OpaqueClassBlock *)clazz, false}});
      romMethod = nextROMMethod(romMethod);
      }
   }

J9ROMClass *
JITServerHelpers::getRemoteROMClassIfCached(ClientSessionData *clientSessionData, J9Class *clazz)
   {
   OMR::CriticalSection getRemoteROMClassIfCached(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   return (it == clientSessionData->getROMClassMap().end()) ? NULL : it->second._romClass;
   }

JITServerHelpers::ClassInfoTuple
JITServerHelpers::packRemoteROMClassInfo(J9Class *clazz, J9VMThread *vmThread, TR_Memory *trMemory, bool serializeClass)
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

   std::vector<J9ROMMethod *> origROMMethods;
   origROMMethods.reserve(numMethods);
   for(uint32_t i = 0; i < numMethods; ++i)
      {
      methodTracingInfo.push_back(static_cast<uint8_t>(fe->isMethodTracingEnabled((TR_OpaqueMethodBlock *) &methodsOfClass[i])));
      // record client-side pointers to ROM methods
      origROMMethods.push_back(fe->getROMMethodFromRAMMethod(&methodsOfClass[i]));
      }

   bool classHasFinalFields = fe->hasFinalFieldsInClass((TR_OpaqueClassBlock *)clazz);
   uintptr_t classDepthAndFlags = fe->getClassDepthAndFlagsValue((TR_OpaqueClassBlock *)clazz);
   bool classInitialized =  fe->isClassInitialized((TR_OpaqueClassBlock *)clazz);
   uint32_t byteOffsetToLockword = fe->getByteOffsetToLockword((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock * leafComponentClass = fe->getLeafComponentClassFromArrayClass((TR_OpaqueClassBlock *)clazz);
   void * classLoader = fe->getClassLoader((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock * hostClass = fe->convertClassPtrToClassOffset(clazz->hostClass);
   TR_OpaqueClassBlock * componentClass = fe->getComponentClassFromArrayClass((TR_OpaqueClassBlock *)clazz);
   TR_OpaqueClassBlock * arrayClass = fe->getArrayClassFromComponentClass((TR_OpaqueClassBlock *)clazz);
   uintptr_t totalInstanceSize = clazz->totalInstanceSize;
   uintptr_t cp = fe->getConstantPoolFromClass((TR_OpaqueClassBlock *)clazz);
   uintptr_t classFlags = fe->getClassFlagsValue((TR_OpaqueClassBlock *)clazz);
   uintptr_t classChainOffsetOfIdentifyingLoaderForClazz = fe->sharedCache() ? 
      fe->sharedCache()->getClassChainOffsetOfIdentifyingLoaderForClazzInSharedCacheNoFail((TR_OpaqueClassBlock *)clazz) : 0;

   return std::make_tuple(serializeClass ? packROMClass(clazz->romClass, trMemory) : std::string(), methodsOfClass, baseClass, numDims, parentClass,
                          TR::Compiler->cls.getITable((TR_OpaqueClassBlock *) clazz), methodTracingInfo,
                          classHasFinalFields, classDepthAndFlags, classInitialized, byteOffsetToLockword,
                          leafComponentClass, classLoader, hostClass, componentClass, arrayClass, totalInstanceSize,
                          clazz->romClass, cp, classFlags, classChainOffsetOfIdentifyingLoaderForClazz, origROMMethods);
   }

J9ROMClass *
JITServerHelpers::romClassFromString(const std::string &romClassStr, TR_PersistentMemory *trMemory)
   {
   auto romClass = (J9ROMClass *)(trMemory->allocatePersistentMemory(romClassStr.size(), TR_Memory::ROMClass));
   if (!romClass)
      throw std::bad_alloc();
   memcpy(romClass, &romClassStr[0], romClassStr.size());
   return romClass;
   }

J9ROMClass *
JITServerHelpers::getRemoteROMClass(J9Class *clazz, JITServer::ServerStream *stream, TR_Memory *trMemory, ClassInfoTuple *classInfoTuple)
   {
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   *classInfoTuple = std::get<0>(recv);
   return romClassFromString(std::get<0>(*classInfoTuple), trMemory->trPersistentMemory());
   }

J9ROMClass *
JITServerHelpers::getRemoteROMClass(J9Class *clazz, JITServer::ServerStream *stream, TR_PersistentMemory *trMemory, ClassInfoTuple *classInfoTuple)
   {
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<ClassInfoTuple>();
   *classInfoTuple = std::get<0>(recv);
   return romClassFromString(std::get<0>(*classInfoTuple), trMemory);
   }

// Return true if able to get data from cache, return false otherwise.
bool
JITServerHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType, void *data)
   {
   JITServerHelpers::ClassInfoTuple classInfoTuple;
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
         JITServerHelpers::getROMClassData(it->second, dataType, data);
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
      JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, &classInfoTuple, classInfo);
      JITServerHelpers::getROMClassData(classInfo, dataType, data);
      }
   else
      {
      JITServerHelpers::getROMClassData(it->second, dataType, data);
      }
   return false;
   }

// Return true if able to get data from cache, return false otherwise.
bool
JITServerHelpers::getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType1, void *data1, ClassInfoDataType dataType2, void *data2)
   {
   JITServerHelpers::ClassInfoTuple classInfoTuple;
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
         JITServerHelpers::getROMClassData(it->second, dataType1, data1);
         JITServerHelpers::getROMClassData(it->second, dataType2, data2);
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
      JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, &classInfoTuple, classInfo);
      JITServerHelpers::getROMClassData(classInfo, dataType1, data1);
      JITServerHelpers::getROMClassData(classInfo, dataType2, data2);
      }
   else
      {
      JITServerHelpers::getROMClassData(it->second, dataType1, data1);
      JITServerHelpers::getROMClassData(it->second, dataType2, data2);
      }
   return false;
   }

void
JITServerHelpers::getROMClassData(const ClientSessionData::ClassInfo &classInfo, ClassInfoDataType dataType, void *data)
   {
   switch (dataType)
      {
      case CLASSINFO_ROMCLASS_MODIFIERS :
         {
         *(uint32_t *)data = classInfo._romClass->modifiers;
         }
         break;
      case CLASSINFO_ROMCLASS_EXTRAMODIFIERS :
         {
         *(uint32_t *)data = classInfo._romClass->extraModifiers;
         }
         break;
      case CLASSINFO_BASE_COMPONENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._baseComponentClass;
         }
         break;
      case CLASSINFO_NUMBER_DIMENSIONS :
         {
         *(int32_t *)data = classInfo._numDimensions;
         }
         break;
      case CLASSINFO_PARENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._parentClass;
         }
         break;
      case CLASSINFO_CLASS_HAS_FINAL_FIELDS :
         {
         *(bool *)data = classInfo._classHasFinalFields;
         }
         break;
      case CLASSINFO_CLASS_DEPTH_AND_FLAGS :
         {
         *(uintptr_t *)data = classInfo._classDepthAndFlags;
         }
         break;
      case CLASSINFO_CLASS_INITIALIZED :
         {
         *(bool *)data = classInfo._classInitialized;
         }
         break;
      case CLASSINFO_BYTE_OFFSET_TO_LOCKWORD :
         {
         *(uint32_t *)data = classInfo._byteOffsetToLockword;
         }
         break;
      case CLASSINFO_LEAF_COMPONENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._leafComponentClass;
         }
         break;
      case CLASSINFO_CLASS_LOADER :
         {
         *(void **)data = classInfo._classLoader;
         }
         break;
      case CLASSINFO_HOST_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._hostClass;
         }
         break;
      case CLASSINFO_COMPONENT_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._componentClass;
         }
         break;
      case CLASSINFO_ARRAY_CLASS :
         {
         *(TR_OpaqueClassBlock **)data = classInfo._arrayClass;
         }
         break;
      case CLASSINFO_TOTAL_INSTANCE_SIZE :
         {
         *(uintptr_t *)data = classInfo._totalInstanceSize;
         }
         break;
      case CLASSINFO_REMOTE_ROM_CLASS :
         {
         *(J9ROMClass **)data = classInfo._remoteRomClass;
         }
         break;
      case CLASSINFO_CLASS_FLAGS :
         {
         *(uintptr_t *)data = classInfo._classFlags;
         }
         break;
      case CLASSINFO_METHODS_OF_CLASS :
         {
         *(J9Method **)data = classInfo._methodsOfClass;
         }
         break;
      case CLASSINFO_CONSTANT_POOL :
         {
         *(J9ConstantPool **)data = classInfo._constantPool;
         }
         break;
      case CLASSINFO_CLASS_CHAIN_OFFSET:
         {
         *(uintptr_t *)data = classInfo._classChainOffsetOfIdentifyingLoaderForClazz;
         }
         break;
      default :
         {
         TR_ASSERT(0, "Class Info not supported %u \n", dataType);
         }
         break;
      }
   }

J9ROMMethod *
JITServerHelpers::romMethodOfRamMethod(J9Method* method)
   {
   // JITServer
   auto clientData = TR::compInfoPT->getClientData();
   J9ROMMethod *romMethod = NULL;

   // Check if the method is already cached.
      {
      OMR::CriticalSection romCache(clientData->getROMMapMonitor());
      auto &map = clientData->getJ9MethodMap();
      auto it = map.find((J9Method*) method);
      if (it != map.end())
         romMethod = it->second._romMethod;
      }

   // If not, cache the associated ROM class and get the ROM method from it.
   if (!romMethod)
      {
      auto stream = TR::CompilationInfo::getStream();
      stream->write(JITServer::MessageType::VM_getClassOfMethod, (TR_OpaqueMethodBlock*) method);
      J9Class *clazz = (J9Class*) std::get<0>(stream->read<TR_OpaqueClassBlock *>());
      TR::compInfoPT->getAndCacheRemoteROMClass(clazz);
         {
         OMR::CriticalSection romCache(clientData->getROMMapMonitor());
         auto &map = clientData->getJ9MethodMap();
         auto it = map.find((J9Method *) method);
         if (it != map.end())
            romMethod = it->second._romMethod;
         }
      }
   TR_ASSERT(romMethod, "Should have acquired romMethod");
   return romMethod;
   }

void
JITServerHelpers::postStreamFailure(OMRPortLibrary *portLibrary)
   {
   OMR::CriticalSection postStreamFailure(getClientStreamMonitor());

   OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
   uint64_t current_time = omrtime_current_time_millis();
   if (current_time >= _nextConnectionRetryTime)
      {
      _waitTimeMs *= 2; // Exponential backoff
      }
   _nextConnectionRetryTime = current_time + _waitTimeMs;
   _serverAvailable = false;
   }

void
JITServerHelpers::postStreamConnectionSuccess()
   {
   _serverAvailable = true;
   _waitTimeMs = 1000;
   }

bool
JITServerHelpers::shouldRetryConnection(OMRPortLibrary *portLibrary)
   {
   OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
   return omrtime_current_time_millis() > _nextConnectionRetryTime;
   }

bool
JITServerHelpers::isAddressInROMClass(const void *address, const J9ROMClass *romClass)
   {
   return ((address >= romClass) && (address < (((uint8_t*) romClass) + romClass->romSize)));
   }


uintptr_t
JITServerHelpers::walkReferenceChainWithOffsets(TR_J9VM * fe, const std::vector<uintptr_t>& listOfOffsets, uintptr_t receiver)
   {
   uintptr_t result = receiver;
   for (size_t i = 0; i < listOfOffsets.size(); i++)
      {
      result = fe->getReferenceFieldAt(result, listOfOffsets[i]);
      }
   return result;
   }

uintptr_t
JITServerHelpers::getRemoteClassDepthAndFlagsWhenROMClassNotCached(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream)
{
   ClientSessionData::ClassInfo classInfo;
   stream->write(JITServer::MessageType::ResolvedMethod_getRemoteROMClassAndMethods, clazz);
   const auto &recv = stream->read<JITServerHelpers::ClassInfoTuple>();
   JITServerHelpers::ClassInfoTuple classInfoTuple = std::get<0>(recv);

   OMR::CriticalSection cacheRemoteROMClass(clientSessionData->getROMMapMonitor());
   auto it = clientSessionData->getROMClassMap().find(clazz);
   if (it == clientSessionData->getROMClassMap().end())
      {
      auto romClass = JITServerHelpers::romClassFromString(std::get<0>(classInfoTuple), TR::comp()->trMemory()->trPersistentMemory());
      JITServerHelpers::cacheRemoteROMClass(clientSessionData, clazz, romClass, &classInfoTuple, classInfo);
      return classInfo._classDepthAndFlags;
      }
   else
      {
      return it->second._classDepthAndFlags;
      }
}
