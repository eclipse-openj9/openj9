/*******************************************************************************
 * Copyright (c) 2019, 2022 IBM Corp. and others
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

#ifndef JITSERVER_HELPERS_H
#define JITSERVER_HELPERS_H

#include "net/MessageTypes.hpp"
#include "runtime/JITClientSession.hpp"


class JITServerHelpers
   {
public:
   enum ClassInfoDataType
      {
      CLASSINFO_ROMCLASS_MODIFIERS,
      CLASSINFO_ROMCLASS_EXTRAMODIFIERS,
      CLASSINFO_BASE_COMPONENT_CLASS,
      CLASSINFO_NUMBER_DIMENSIONS,
      CLASSINFO_PARENT_CLASS,
      CLASSINFO_INTERFACE_CLASS,
      CLASSINFO_CLASS_HAS_FINAL_FIELDS,
      CLASSINFO_CLASS_DEPTH_AND_FLAGS,
      CLASSINFO_CLASS_INITIALIZED,
      CLASSINFO_BYTE_OFFSET_TO_LOCKWORD,
      CLASSINFO_LEAF_COMPONENT_CLASS,
      CLASSINFO_CLASS_LOADER,
      CLASSINFO_HOST_CLASS,
      CLASSINFO_COMPONENT_CLASS,
      CLASSINFO_ARRAY_CLASS,
      CLASSINFO_TOTAL_INSTANCE_SIZE,
      CLASSINFO_CLASS_OF_STATIC_CACHE,
      CLASSINFO_REMOTE_ROM_CLASS,
      CLASSINFO_CLASS_FLAGS,
      CLASSINFO_METHODS_OF_CLASS,
      CLASSINFO_CONSTANT_POOL,
      CLASSINFO_CLASS_CHAIN_OFFSET_IDENTIFYING_LOADER,
      CLASSINFO_ARRAY_ELEMENT_SIZE,
      };

   // NOTE: when adding new elements to this tuple, add them to the end,
   // to not mess with the established order.
   using ClassInfoTuple = std::tuple
      <
      std::string,                       // 0:  _packedROMClass
      J9Method *,                        // 1:  _methodsOfClass
      TR_OpaqueClassBlock *,             // 2:  _baseComponentClass
      int32_t,                           // 3:  _numDimensions
      TR_OpaqueClassBlock *,             // 4:  _parentClass
      std::vector<TR_OpaqueClassBlock *>,// 5:  _tmpInterfaces
      std::vector<uint8_t>,              // 6:  _methodTracingInfo
      bool,                              // 7:  _classHasFinalFields
      uintptr_t,                         // 8:  _classDepthAndFlags
      bool,                              // 9:  _classInitialized
      uint32_t,                          // 10: _byteOffsetToLockword
      TR_OpaqueClassBlock *,             // 11: _leafComponentClass
      void *,                            // 12: _classLoader
      TR_OpaqueClassBlock *,             // 13: _hostClass
      TR_OpaqueClassBlock *,             // 14: _componentClass
      TR_OpaqueClassBlock *,             // 15: _arrayClass
      uintptr_t,                         // 16: _totalInstanceSize
      J9ROMClass *,                      // 17: _remoteRomClass
      uintptr_t,                         // 18: _constantPool
      uintptr_t,                         // 19: _classFlags
      uintptr_t,                         // 20: _classChainOffsetIdentifyingLoader
      std::vector<J9ROMMethod *>,        // 21: _origROMMethods
      std::string,                       // 22: _classNameIdentifyingLoader
      int32_t                            // 23: _arrayElementSize
      >;

   // Packs a ROMClass to be transferred to the server.
   // The result is allocated from the stack region of trMemory (as well as temporary data
   // structures used for packing). This function should be used with TR::StackMemoryRegion.
   // If passed non-zero expectedSize, and it doesn't match the resulting packedSize
   // (which is returned to the caller by reference), this function returns NULL.
   static J9ROMClass *packROMClass(J9ROMClass *romClass, TR_Memory *trMemory, size_t &packedSize, size_t expectedSize = 0);

   static ClassInfoTuple packRemoteROMClassInfo(J9Class *clazz, J9VMThread *vmThread, TR_Memory *trMemory, bool serializeClass);
   static void freeRemoteROMClass(J9ROMClass *romClass, TR_PersistentMemory *persistentMemory);
   static J9ROMClass *cacheRemoteROMClassOrFreeIt(ClientSessionData *clientSessionData, J9Class *clazz,
                                                  J9ROMClass *romClass, const ClassInfoTuple &classInfoTuple);
   static ClientSessionData::ClassInfo &cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz,
                                                            J9ROMClass *romClass, const ClassInfoTuple &classInfoTuple);
   static J9ROMClass *getRemoteROMClassIfCached(ClientSessionData *clientSessionData, J9Class *clazz);
   static J9ROMClass *getRemoteROMClass(J9Class *clazz, JITServer::ServerStream *stream,
                                        TR_PersistentMemory *persistentMemory, ClassInfoTuple &classInfoTuple);
   static J9ROMClass *romClassFromString(const std::string &romClassStr, TR_PersistentMemory *persistentMemory);
   static bool getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData,
                                       JITServer::ServerStream *stream, ClassInfoDataType dataType, void *data);
   static bool getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData,
                                       JITServer::ServerStream *stream, ClassInfoDataType dataType1, void *data1,
                                       ClassInfoDataType dataType2, void *data2);
   static J9ROMMethod *romMethodOfRamMethod(J9Method* method);

   static void insertIntoOOSequenceEntryList(ClientSessionData *clientData, TR_MethodToBeCompiled *entry);

   static uintptr_t getRemoteClassDepthAndFlagsWhenROMClassNotCached(J9Class *clazz, ClientSessionData *clientSessionData,
                                                                     JITServer::ServerStream *stream);

   // Functions used for allowing the client to compile locally when server is unavailable.
   // Should be used only on the client side.
   static void postStreamFailure(OMRPortLibrary *portLibrary, TR::CompilationInfo *compInfo, bool retryConnectionImmediately);
   static bool shouldRetryConnection(OMRPortLibrary *portLibrary);
   static void postStreamConnectionSuccess();
   static bool isServerAvailable() { return _serverAvailable; }

   static void printJITServerMsgStats(J9JITConfig *, TR::CompilationInfo *);
   static void printJITServerCHTableStats(J9JITConfig *, TR::CompilationInfo *);
   static void printJITServerCacheStats(J9JITConfig *, TR::CompilationInfo *);

   static bool isAddressInROMClass(const void *address, const J9ROMClass *romClass);

   static uintptr_t walkReferenceChainWithOffsets(TR_J9VM *fe, const std::vector<uintptr_t> &listOfOffsets, uintptr_t receiver);

   // Called by the client as part of preparing the compilation request and handling the SharedCache_rememberClass message
   // when AOT cache is enabled. Returns the list of RAMClasses in the class chain for clazz and populates uncachedRAMClasses
   // and uncachedClassInfos with the classes (and their data) that the client has not yet sent to the server.
   static std::vector<J9Class *>
   getRAMClassChain(J9Class *clazz, size_t numClasses, J9VMThread *vmThread, TR_Memory *trMemory, TR::CompilationInfo *compInfo,
                    std::vector<J9Class *> &uncachedRAMClasses, std::vector<ClassInfoTuple> &uncachedClassInfos);

   static void cacheRemoteROMClassBatch(ClientSessionData *clientData, const std::vector<J9Class *> &ramClasses,
                                        const std::vector<ClassInfoTuple> &classInfoTuples);

private:
   static void getROMClassData(const ClientSessionData::ClassInfo &classInfo, ClassInfoDataType dataType, void *data);

   static TR::Monitor *getClientStreamMonitor()
      {
      if (!_clientStreamMonitor)
         _clientStreamMonitor = TR::Monitor::create("clientStreamMonitor");
      return _clientStreamMonitor;
      }

   static uint64_t _waitTimeMs;
   static uint64_t _nextConnectionRetryTime;
   static bool _serverAvailable;
   static TR::Monitor * _clientStreamMonitor;
   }; // class JITServerHelpers

#endif // defined(JITSERVER_HELPERS_H)
