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

#ifndef JITaaS_COMPILATION_THREAD_H
#define JITaaS_COMPILATION_THREAD_H

#include <unordered_map>
#include "control/CompilationThread.hpp"
#include "rpc/J9Client.hpp"
#include "env/PersistentCollections.hpp"

class TR_PersistentClassInfo;
class TR_IPBytecodeHashTableEntry;
struct TR_RemoteROMStringKey;

using IPTable_t = PersistentUnorderedMap<uint32_t, TR_IPBytecodeHashTableEntry*>;

class ClientSessionData
   {
   public:
   struct ClassInfo
      {
      void freeClassInfo(); // this method is in place of a destructor. We can't have destructor
      // becaues it would be called after inserting ClassInfo into the ROM map, freeing romClass
      J9ROMClass *romClass; // romClass content exists in persistentMemory at the server
      J9Method *methodsOfClass;
      // Fields meaningful for arrays
      TR_OpaqueClassBlock *baseComponentClass; 
      int32_t numDimensions;
      PersistentUnorderedMap<TR_RemoteROMStringKey, std::string> *_remoteROMStringsCache; // cached strings from the client
      PersistentUnorderedMap<int32_t, std::string> *_fieldOrStaticNameCache;
      TR_OpaqueClassBlock *parentClass;
      PersistentVector<TR_OpaqueClassBlock *> *interfaces; 
      bool classHasFinalFields;
      uintptrj_t classDepthAndFlags;
      bool classInitialized;
      uint32_t byteOffsetToLockword;
      TR_OpaqueClassBlock * leafComponentClass;
      void *classLoader;
      TR_OpaqueClassBlock * hostClass;
      TR_OpaqueClassBlock * componentClass; // caching the componentType of the J9ArrayClass
      TR_OpaqueClassBlock * arrayClass;
      };

   struct J9MethodInfo
      {
      J9ROMMethod *_romMethod; // pointer to local/server cache
      // The following is a hashtable that maps a bcIndex to IProfiler data
      // The hashtable is created on demand (nullptr means it is missing)
      IPTable_t *_IPData;
      bool _isMethodEnterTracingEnabled;
      bool _isMethodExitTracingEnabled;
      };

   // This struct contains information about VM that does not change during its lifetime.
   struct VMInfo
      {
      void *_systemClassLoader;
      uintptrj_t _processID;
      bool _canMethodEnterEventBeHooked;
      bool _canMethodExitEventBeHooked;
      bool _usesDiscontiguousArraylets;
      int32_t _arrayletLeafLogSize;
      int32_t _arrayletLeafSize;
      uint64_t _overflowSafeAllocSize;
      };

   TR_PERSISTENT_ALLOC(TR_Memory::ClientSessionData)
   ClientSessionData(uint64_t clientUID);
   ~ClientSessionData();

   void setJavaLangClassPtr(TR_OpaqueClassBlock* j9clazz) { _javaLangClassPtr = j9clazz; }
   TR_OpaqueClassBlock * getJavaLangClassPtr() const { return _javaLangClassPtr; }
   PersistentUnorderedMap<TR_OpaqueClassBlock*, TR_PersistentClassInfo*> & getCHTableClassMap() { return _chTableClassMap; }
   PersistentUnorderedMap<J9Class*, ClassInfo> & getROMClassMap() { return _romClassMap; }
   PersistentUnorderedMap<J9Method*, J9MethodInfo> & getJ9MethodMap() { return _J9MethodMap; }
   PersistentUnorderedMap<std::string, TR_OpaqueClassBlock*> & getSystemClassByNameMap() { return _systemClassByNameMap; }
   void processUnloadedClasses(const std::vector<TR_OpaqueClassBlock*> &classes);
   TR::Monitor *getROMMapMonitor() { return _romMapMonitor; }
   TR::Monitor *getSystemClassMapMonitor() { return _systemClassMapMonitor; }
   TR_IPBytecodeHashTableEntry *getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent);
   bool cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry);
   VMInfo *getOrCacheVMInfo(JITaaS::J9ServerStream *stream);

   void incInUse() { _inUse++; }
   void decInUse() { _inUse--; TR_ASSERT(_inUse >= 0, "_inUse=%d must be positive\n", _inUse); }
   bool getInUse() const { return _inUse; }

   void updateTimeOfLastAccess();
   int64_t getTimeOflastAccess() const { return _timeOfLastAccess; }

   void printStats();

   private:
   uint64_t _clientUID; // for RAS
   int64_t  _timeOfLastAccess; // in ms
   TR_OpaqueClassBlock *_javaLangClassPtr; // nullptr means not set
   PersistentUnorderedMap<TR_OpaqueClassBlock*, TR_PersistentClassInfo*> _chTableClassMap; // cache of persistent CHTable
   PersistentUnorderedMap<J9Class*, ClassInfo> _romClassMap;
   // Hashtable for information related to one J9Method
   PersistentUnorderedMap<J9Method*, J9MethodInfo> _J9MethodMap;
   // The following hashtable caches <classname> --> <J9Class> mappings
   // All classes in here are loaded by the systemClassLoader so we know they cannot be unloaded
   PersistentUnorderedMap<std::string, TR_OpaqueClassBlock*> _systemClassByNameMap;
 
   TR::Monitor *_romMapMonitor;
   TR::Monitor *_systemClassMapMonitor;
   int8_t  _inUse;  // Number of concurrent compilations from the same client 
                    // Accessed with compilation monitor in hand
   VMInfo *_vmInfo; // info specific to a client VM that does not change, nullptr means not set
   }; // ClientSessionData

// Hashtable that maps clientUID to a pointer that points to ClientSessionData
// This indirection is needed so that we can cache the value of the pointer so
// that we can access client session data without going through the hashtable.
// Accesss to this hashtable must be protected by the compilation monitor.
// Compilation threads may purge old entries periodically at the beginning of a 
// compilation. Entried with inUse > 0 must not be purged.
class ClientSessionHT
   {
   public:
   ClientSessionHT();
   ~ClientSessionHT();
   static ClientSessionHT* allocate(); // allocates a new instance of this class
   ClientSessionData * findOrCreateClientSession(uint64_t clientUID);
   ClientSessionData * findClientSession(uint64_t clientUID);
   void purgeOldDataIfNeeded();
   void printStats();

   private:
   PersistentUnorderedMap<uint64_t, ClientSessionData*> _clientSessionMap;

   uint64_t _timeOfLastPurge;
   const int64_t TIME_BETWEEN_PURGES; // ms; this defines how often we are willing to scan for old entries to be purged
   const int64_t OLD_AGE;// ms; this defines what an old entry means
                         // This value must be larger than the expected life of a JVM
   }; // ClientSessionHT

size_t methodStringLength(J9ROMMethod *);
std::string packROMClass(J9ROMClass *, TR_Memory *);
bool handleServerMessage(JITaaS::J9ClientStream *, TR_J9VM *);
TR_MethodMetaData *remoteCompile(J9VMThread *, TR::Compilation *, TR_ResolvedMethod *,
      J9Method *, TR::IlGeneratorMethodDetails &, TR::CompilationInfoPerThreadBase *);
void remoteCompilationEnd(TR::IlGeneratorMethodDetails &details, J9JITConfig *jitConfig,
      TR_FrontEnd *fe, TR_MethodToBeCompiled *entry, TR::Compilation *comp);
void printJITaaSMsgStats(J9JITConfig *);
void printJITaaSCHTableStats(J9JITConfig *, TR::CompilationInfo *);
void printJITaaSCacheStats(J9JITConfig *, TR::CompilationInfo *);



class JITaaSHelpers
   {
   public:
   // NOTE: when adding new elements to this tuple, add them to the end,
   // to not mess with the established order.
   using ClassInfoTuple = std::tuple<std::string, J9Method *, TR_OpaqueClassBlock *, int32_t, TR_OpaqueClassBlock *, std::vector<TR_OpaqueClassBlock *>, std::vector<std::tuple<bool, bool>>, bool, uintptrj_t , bool, uint32_t, TR_OpaqueClassBlock *, void *, TR_OpaqueClassBlock *, TR_OpaqueClassBlock *, TR_OpaqueClassBlock *>;
   static ClassInfoTuple packRemoteROMClassInfo(J9Class *clazz, TR_J9VM *fe, TR_Memory *trMemory);
   static void cacheRemoteROMClass(ClientSessionData *clientSessionData, J9Class *clazz, J9ROMClass *romClass, ClassInfoTuple *classInfoTuple);
   static J9ROMClass *getRemoteROMClassIfCached(ClientSessionData *clientSessionData, J9Class *clazz);
   static J9ROMClass * getRemoteROMClass(J9Class *, JITaaS::J9ServerStream *stream, TR_Memory *trMemory, ClassInfoTuple *classInfoTuple);
   static J9ROMClass * romClassFromString(const std::string &romClassStr, TR_PersistentMemory *trMemory);
   };

#endif
