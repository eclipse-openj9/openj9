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

#ifndef JITSERVER_HELPERS_H
#define JITSERVER_HELPERS_H

#include "net/gen/compile.pb.h"
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
      };
   // NOTE: when adding new elements to this tuple, add them to the end,
   // to not mess with the established order.
   using ClassInfoTuple = std::tuple
      <
      std::string, J9Method *,                                       // 0:  string                  1:  _methodsOfClass
      TR_OpaqueClassBlock *, int32_t,                                // 2:  _baseComponentClass     3:  _numDimensions
      TR_OpaqueClassBlock *, std::vector<TR_OpaqueClassBlock *>,     // 4:  _parentClass            5:  _tmpInterfaces
      std::vector<uint8_t>, bool,                                    // 6:  _methodTracingInfo      7:  _classHasFinalFields
      uintptrj_t, bool,                                              // 8:  _classDepthAndFlags     9:  _classInitialized
      uint32_t, TR_OpaqueClassBlock *,                               // 10: _byteOffsetToLockword   11: _leafComponentClass
      void *, TR_OpaqueClassBlock *,                                 // 12: _classLoader            13: _hostClass
      TR_OpaqueClassBlock *, TR_OpaqueClassBlock *,                  // 14: _componentClass         15: _arrayClass
      uintptrj_t, J9ROMClass *,                                      // 16: _totalInstanceSize      17: _remoteRomClass
      uintptrj_t, uintptrj_t                                         // 18: _constantPool           19: _classFlags
      >;

   static ClassInfoTuple packRemoteROMClassInfo(J9Class *clazz, J9VMThread *vmThread, TR_Memory *trMemory);
   static void cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple);
   static void cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple, ClientSessionData::ClassInfo &classInfo);
   static J9ROMClass *getRemoteROMClassIfCached(ClientSessionData *clientSessionData, J9Class *clazz);
   static J9ROMClass *getRemoteROMClass(J9Class *, JITServer::ServerStream *stream, TR_Memory *trMemory, ClassInfoTuple *classInfoTuple);
   static J9ROMClass *romClassFromString(const std::string &romClassStr, TR_PersistentMemory *trMemory);
   static bool getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType, void *data);
   static bool getAndCacheRAMClassInfo(J9Class *clazz, ClientSessionData *clientSessionData, JITServer::ServerStream *stream, ClassInfoDataType dataType1, void *data1,
                                       ClassInfoDataType dataType2, void *data2);
   static J9ROMMethod *romMethodOfRamMethod(J9Method* method);

   static void insertIntoOOSequenceEntryList(ClientSessionData *clientData, TR_MethodToBeCompiled *entry);

   // Functions used for allowing the client to compile locally when server is unavailable.
   // Should be used only on the client side.
   static void postStreamFailure(OMRPortLibrary *portLibrary);
   static bool shouldRetryConnection(OMRPortLibrary *portLibrary);
   static void postStreamConnectionSuccess();
   static bool isServerAvailable() { return _serverAvailable; }

   static void printJITServerMsgStats(J9JITConfig *);
   static void printJITServerCHTableStats(J9JITConfig *, TR::CompilationInfo *);
   static void printJITServerCacheStats(J9JITConfig *, TR::CompilationInfo *);

   static uint32_t serverMsgTypeCount[JITServer::MessageType_ARRAYSIZE];

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
