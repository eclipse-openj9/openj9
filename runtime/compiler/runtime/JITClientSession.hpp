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

#ifndef JIT_CLIENT_SESSION_H
#define JIT_CLIENT_SESSION_H

#include "infra/Monitor.hpp"  // TR::Monitor
#include "env/PersistentCollections.hpp" // for PersistentUnorderedMap
#include "il/DataTypes.hpp" // for DataType
#include "env/VMJ9.h" // for TR_StaticFinalData

class J9ROMClass;
class J9Class;
class J9Method;
class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
class TR_PersistentClassInfo;
class J9ConstantPool;
class TR_IPBytecodeHashTableEntry;
class TR_MethodToBeCompiled;
class TR_AddressRange;
class TR_PersistentCHTable;
class JITServerPersistentCHTable;
namespace JITServer { class ServerStream; }


using IPTable_t = PersistentUnorderedMap<uint32_t, TR_IPBytecodeHashTableEntry*>;
using TR_JitFieldsCacheEntry = std::pair<J9Class*, UDATA>;
using TR_JitFieldsCache = PersistentUnorderedMap<int32_t, TR_JitFieldsCacheEntry>;

/**
   @class TR_J9MethodFieldAttributes
   @brief Class used for caching various field attributes of a j9method
 */

class TR_J9MethodFieldAttributes
   {
   public:
   TR_J9MethodFieldAttributes() :
      TR_J9MethodFieldAttributes(0, TR::NoType, false, false, false, false, false)
      {}

   TR_J9MethodFieldAttributes(uintptr_t fieldOffsetOrAddress,
                              TR::DataType type,
                              bool volatileP,
                              bool isFinal,
                              bool isPrivate,
                              bool unresolvedInCP,
                              bool result,
                              TR_OpaqueClassBlock *definingClass = NULL) :
      _fieldOffsetOrAddress(fieldOffsetOrAddress),
      _type(type),
      _volatileP(volatileP),
      _isFinal(isFinal),
      _isPrivate(isPrivate),
      _unresolvedInCP(unresolvedInCP),
      _result(result),
      _definingClass(definingClass)
      {}

   bool operator==(const TR_J9MethodFieldAttributes &other) const
      {
      if (!_result && !other._result) return true; // result is false, shouldn't compare other fields
      if (_fieldOffsetOrAddress != other._fieldOffsetOrAddress) return false;
      if (_type.getDataType() != other._type.getDataType()) return false;
      if (_volatileP != other._volatileP) return false;
      if (_isFinal != other._isFinal) return false;
      if (_isPrivate != other._isPrivate) return false;
      if (_unresolvedInCP != other._unresolvedInCP) return false;
      if (_result != other._result) return false;
      if (_definingClass != other._definingClass) return false;
      return true;
      }
   
   void setMethodFieldAttributesResult(uint32_t *fieldOffset, TR::DataType *type, bool *volatileP, bool *isFinal, bool *isPrivate, bool *unresolvedInCP, bool *result, TR_OpaqueClassBlock **definingClass = NULL)
      {
      setMethodFieldAttributesResult(type, volatileP, isFinal, isPrivate, unresolvedInCP, result, definingClass);
      if (fieldOffset) *fieldOffset = static_cast<uint32_t>(_fieldOffsetOrAddress);
      }

   void setMethodFieldAttributesResult(void **address, TR::DataType *type, bool *volatileP, bool *isFinal, bool *isPrivate, bool *unresolvedInCP, bool *result, TR_OpaqueClassBlock **definingClass = NULL)
      {
      setMethodFieldAttributesResult(type, volatileP, isFinal, isPrivate, unresolvedInCP, result, definingClass);
      if (address) *address = reinterpret_cast<void *>(_fieldOffsetOrAddress);
      }

   bool isUnresolvedInCP() const { return _unresolvedInCP; }

   private:
   void setMethodFieldAttributesResult(TR::DataType *type, bool *volatileP, bool *isFinal, bool *isPrivate, bool *unresolvedInCP, bool *result, TR_OpaqueClassBlock **definingClass = NULL)
      {
      if (type) *type = _type;
      if (volatileP) *volatileP = _volatileP;
      if (isFinal) *isFinal = _isFinal;
      if (isPrivate) *isPrivate = _isPrivate;
      if (unresolvedInCP) *unresolvedInCP = _unresolvedInCP;
      if (result) *result = _result;
      if (definingClass) *definingClass = _definingClass;
      }

   uintptr_t _fieldOffsetOrAddress; // Stores a uint32_t representing an offset for non-static fields, or an address for static fields.
   TR::DataType _type; 
   bool _volatileP;
   bool _isFinal;
   bool _isPrivate;
   bool _unresolvedInCP;
   bool _result;
   TR_OpaqueClassBlock *_definingClass; // only needed for AOT compilations
   }; // class TR_J9MethodFieldAttributes


using TR_FieldAttributesCache = PersistentUnorderedMap<int32_t, TR_J9MethodFieldAttributes>;

struct ClassLoaderStringPair
   {
   J9ClassLoader *_classLoader;
   std::string    _className;

   bool operator==(const ClassLoaderStringPair &other) const
      {
      return _classLoader == other._classLoader &&  _className == other._className;
      }
   };

struct TR_RemoteROMStringKey
   {
   void *_basePtr;
   uint32_t _offsets;
   bool operator==(const TR_RemoteROMStringKey &other) const
      {
      return (_basePtr == other._basePtr) && (_offsets == other._offsets);
      }
   };


// custom specializations of std::hash injected in std namespace
namespace std
   {
   template<> struct hash<ClassLoaderStringPair>
      {
      typedef ClassLoaderStringPair argument_type;
      typedef std::size_t result_type;
      result_type operator()(argument_type const& clsPair) const noexcept
         {
         return std::hash<void*>()((void*)(clsPair._classLoader)) ^ std::hash<std::string>()(clsPair._className);
         }
      };

   template<typename T, typename Q> struct hash<std::pair<T, Q>>
      {
      std::size_t operator()(const std::pair<T, Q> &key) const noexcept
         {
         return std::hash<T>()(key.first) ^ std::hash<Q>()(key.second);
         }
      };

   template <> struct hash<TR_RemoteROMStringKey>
      {
      std::size_t operator()(const TR_RemoteROMStringKey &k) const noexcept
         {
         // Compute a hash for the table of ROM strings by hashing basePtr and offsets 
         // separately and then XORing them
         return (std::hash<void *>()(k._basePtr)) ^ (std::hash<uint32_t>()(k._offsets));
         }
      };
   }



struct ClassUnloadedData
   {
   TR_OpaqueClassBlock* _class;
   ClassLoaderStringPair _pair;
   J9ConstantPool *_cp;
   bool _cached;
   };

struct J9MethodNameAndSignature
   {
   std::string _classNameStr;
   std::string _methodNameStr;
   std::string _methodSignatureStr;
   };

/**
   @class ClientSessionData
   @brief Data structure that holds data specific to a client

   An object of type ClientSessionData is created when a JITClient connects
   for the first time to a server and it's destroyed when the client informs
   the server that it is going to shutdown, or when the JITServer purges stale
   data belonging to clients that haven't connected for a long time.
   Because a ClientSessionObject can be accessed in parallel by different threads
   (several compilation threads from the client can issue compilation requests)
   access to most fields need to be protected by monitors.
 */

class ClientSessionData
   {
   public:
   /**
      @class ClassInfo
      @brief Struct that holds cached data about a class loaded on the JITClient.

      It contains the ROM class, which is copied in full to the JITServer, as well 
      as other items which are just pointers to data on the JITClient. ClassInfo will
      persist on the server until corresponding Java class gets unloaded or replaced by
      HCR mechanism (which JITServer also treats as a class unload event). At that point,
      JITServer will be notified and the cache will be purged.
   */

   struct ClassInfo
      {
      ClassInfo();
      void freeClassInfo(); // this method is in place of a destructor. We can't have destructor
      // because it would be called after inserting ClassInfo into the ROM map, freeing romClass

      J9ROMClass *_romClass; // romClass content exists in persistentMemory at the server
      J9ROMClass *_remoteRomClass; // pointer to the corresponding ROM class on the client
      J9Method *_methodsOfClass;
      // Fields meaningful for arrays
      TR_OpaqueClassBlock *_baseComponentClass;
      int32_t _numDimensions;
      TR_OpaqueClassBlock *_parentClass;
      PersistentVector<TR_OpaqueClassBlock *> *_interfaces;
      bool _classHasFinalFields;
      uintptr_t _classDepthAndFlags;
      bool _classInitialized;
      uint32_t _byteOffsetToLockword;
      TR_OpaqueClassBlock * _leafComponentClass;
      void *_classLoader;
      TR_OpaqueClassBlock * _hostClass;
      TR_OpaqueClassBlock * _componentClass; // caching the componentType of the J9ArrayClass
      TR_OpaqueClassBlock * _arrayClass;
      uintptr_t _totalInstanceSize;
      J9ConstantPool *_constantPool;
      uintptr_t _classFlags;
      uintptr_t _classChainOffsetOfIdentifyingLoaderForClazz;
      PersistentUnorderedMap<TR_RemoteROMStringKey, std::string> _remoteROMStringsCache; // cached strings from the client
      PersistentUnorderedMap<int32_t, std::string> _fieldOrStaticNameCache;
      PersistentUnorderedMap<int32_t, TR_OpaqueClassBlock *> _classOfStaticCache;
      PersistentUnorderedMap<int32_t, TR_OpaqueClassBlock *> _constantClassPoolCache;
      TR_FieldAttributesCache _fieldAttributesCache;
      TR_FieldAttributesCache _staticAttributesCache;
      TR_FieldAttributesCache _fieldAttributesCacheAOT;
      TR_FieldAttributesCache _staticAttributesCacheAOT;
      TR_JitFieldsCache _jitFieldsCache;
      PersistentUnorderedMap<int32_t, TR_OpaqueClassBlock *> _fieldOrStaticDeclaringClassCache;
      // The following cache is very similar to _fieldOrStaticDeclaringClassCache but it uses
      // a different API to populate it. In the future we may want to unify these two caches
      PersistentUnorderedMap<int32_t, TR_OpaqueClassBlock *> _fieldOrStaticDefiningClassCache;
      PersistentUnorderedMap<int32_t, J9MethodNameAndSignature> _J9MethodNameCache; // key is a cpIndex
      PersistentUnorderedSet<J9ClassLoader *> _referencingClassLoaders;

      char* getROMString(int32_t& len, void *basePtr, std::initializer_list<size_t> offsets);
      char* getRemoteROMString(int32_t& len, void *basePtr, std::initializer_list<size_t> offsets);
      }; // struct ClassInfo


   /**
      @class J9MethodInfo
      @brief Struct that holds relevant data for a j9method
   */
   struct J9MethodInfo
      {
      J9ROMMethod *_romMethod; // pointer to local/server cache
      J9ROMMethod *_origROMMethod; // pointer to the client-side method
      // The following is a hashtable that maps a bcIndex to IProfiler data
      // The hashtable is created on demand (NULL means it is missing)
      IPTable_t *_IPData;
      bool _isMethodTracingEnabled;
      TR_OpaqueClassBlock * _owningClass;
      bool _isCompiledWhenProfiling; // To record if the method is compiled when doing Profiling
      }; // struct J9MethodInfo

   /**
      @class VMInfo
      @brief Struct which contains information about VM that does not change during its lifetime
   */
   struct VMInfo
      {
      void *_systemClassLoader;
      uintptr_t _processID;
      bool _canMethodEnterEventBeHooked;
      bool _canMethodExitEventBeHooked;
      bool _usesDiscontiguousArraylets;
      bool _isIProfilerEnabled;
      int32_t _arrayletLeafLogSize;
      int32_t _arrayletLeafSize;
      uint64_t _overflowSafeAllocSize;
      int32_t _compressedReferenceShift;
      J9SharedClassCacheDescriptor *_j9SharedClassCacheDescriptorList;
      bool _stringCompressionEnabled;
      bool _hasSharedClassCache;
      bool _elgibleForPersistIprofileInfo;
      bool _reportByteCodeInfoAtCatchBlock;
      TR_OpaqueClassBlock *_arrayTypeClasses[8];
      TR_OpaqueClassBlock *_byteArrayClass;
      MM_GCReadBarrierType _readBarrierType;
      MM_GCWriteBarrierType _writeBarrierType;
      bool _compressObjectReferences;
      OMRProcessorDesc _processorDescription;
      J9Method *_invokeWithArgumentsHelperMethod;
      void *_noTypeInvokeExactThunkHelper;
      void *_int64InvokeExactThunkHelper;
      void *_int32InvokeExactThunkHelper;
      void *_addressInvokeExactThunkHelper;
      void *_floatInvokeExactThunkHelper;
      void *_doubleInvokeExactThunkHelper;
      size_t _interpreterVTableOffset;
      int64_t _maxHeapSizeInBytes;
      J9Method *_jlrMethodInvoke;
      uint32_t _enableGlobalLockReservation;
#if defined(J9VM_OPT_SIDECAR)
      TR_OpaqueClassBlock *_srMethodAccessorClass;
      TR_OpaqueClassBlock *_srConstructorAccessorClass;
#endif // J9VM_OPT_SIDECAR
      U_32 _extendedRuntimeFlags2;
      }; // struct VMInfo

   /**
    * @class CacheDescriptor
    * @brief Struct which contains data found in a cache descriptor
    */
   struct CacheDescriptor
      {
      uintptr_t cacheStartAddress;
      uintptr_t cacheSizeBytes;
      uintptr_t romClassStartAddress;
      uintptr_t metadataStartAddress;
      };

   TR_PERSISTENT_ALLOC(TR_Memory::ClientSessionData)
   ClientSessionData(uint64_t clientUID, uint32_t seqNo);
   ~ClientSessionData();
   static void destroy(ClientSessionData *clientSession);

   void setJavaLangClassPtr(TR_OpaqueClassBlock* j9clazz) { _javaLangClassPtr = j9clazz; }
   TR_OpaqueClassBlock * getJavaLangClassPtr() const { return _javaLangClassPtr; }
   TR_PersistentCHTable *getCHTable();
   PersistentUnorderedMap<J9Class*, ClassInfo> & getROMClassMap() { return _romClassMap; }
   PersistentUnorderedMap<J9Method*, J9MethodInfo> & getJ9MethodMap() { return _J9MethodMap; }
   PersistentUnorderedMap<ClassLoaderStringPair, TR_OpaqueClassBlock*> & getClassBySignatureMap() { return _classBySignatureMap; }
   PersistentUnorderedMap<J9Class *, UDATA *> & getClassChainDataCache() { return _classChainDataMap; }
   PersistentUnorderedMap<J9ConstantPool *, TR_OpaqueClassBlock*> & getConstantPoolToClassMap() { return _constantPoolToClassMap; }
   void initializeUnloadedClassAddrRanges(const std::vector<TR_AddressRange> &unloadedClassRanges, int32_t maxRanges);
   void processUnloadedClasses(const std::vector<TR_OpaqueClassBlock*> &classes, bool updateUnloadedClasses);
   void processIllegalFinalFieldModificationList(const std::vector<TR_OpaqueClassBlock*> &classes);
   TR::Monitor *getROMMapMonitor() { return _romMapMonitor; }
   TR::Monitor *getClassMapMonitor() { return _classMapMonitor; }
   TR::Monitor *getClassChainDataMapMonitor() { return _classChainDataMapMonitor; }
   TR_IPBytecodeHashTableEntry *getCachedIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, bool *methodInfoPresent);
   bool cacheIProfilerInfo(TR_OpaqueMethodBlock *method, uint32_t byteCodeIndex, TR_IPBytecodeHashTableEntry *entry, bool isCompiled);
   VMInfo *getOrCacheVMInfo(JITServer::ServerStream *stream);
   void clearCaches(); // destroys _chTableClassMap, _romClassMap, _J9MethodMap and _unloadedClassAddresses
   bool cachesAreCleared() const { return _requestUnloadedClasses; }
   void setCachesAreCleared(bool b) { _requestUnloadedClasses = b; }
   TR_AddressSet& getUnloadedClassAddresses()
      {
      TR_ASSERT(_unloadedClassAddresses, "Unloaded classes address set should exist by now");
      return *_unloadedClassAddresses;
      }

   void incInUse() { _inUse++; }
   void decInUse() { _inUse--; TR_ASSERT(_inUse >= 0, "_inUse=%d must be positive\n", _inUse); }
   bool getInUse() const { return _inUse; }

   uint64_t getClientUID() const { return _clientUID; }
   void updateTimeOfLastAccess();
   int64_t getTimeOflastAccess() const { return _timeOfLastAccess; }

   TR::Monitor *getSequencingMonitor() { return _sequencingMonitor; }
   TR::Monitor *getConstantPoolMonitor() { return _constantPoolMapMonitor; }
   TR_MethodToBeCompiled *getOOSequenceEntryList() const { return _OOSequenceEntryList; }
   void setOOSequenceEntryList(TR_MethodToBeCompiled *m) { _OOSequenceEntryList = m; }
   TR_MethodToBeCompiled *notifyAndDetachFirstWaitingThread();
   uint32_t getExpectedSeqNo() const { return _expectedSeqNo; }
   void setExpectedSeqNo(uint32_t seqNo) { _expectedSeqNo = seqNo; }
   uint32_t getMaxReceivedSeqNo() const { return _maxReceivedSeqNo; }
   // updateMaxReceivedSeqNo needs to be executed with sequencingMonitor in hand
   void updateMaxReceivedSeqNo(uint32_t seqNo)
      {
      if (seqNo > _maxReceivedSeqNo)
         _maxReceivedSeqNo = seqNo;
      }
   int8_t getNumActiveThreads() const { return _numActiveThreads; }
   void incNumActiveThreads() { ++_numActiveThreads; }
   void decNumActiveThreads() { --_numActiveThreads; }
   void printStats();

   void markForDeletion() { _markedForDeletion = true; }
   bool isMarkedForDeletion() const { return _markedForDeletion; }

   TR::Monitor *getStaticMapMonitor() { return _staticMapMonitor; }
   PersistentUnorderedMap<void *, TR_StaticFinalData> &getStaticFinalDataMap() { return _staticFinalDataMap; }

   bool getRtResolve() { return _rtResolve; }
   void setRtResolve(bool rtResolve) { _rtResolve = rtResolve; }

   TR::Monitor *getThunkSetMonitor() { return _thunkSetMonitor; }
   PersistentUnorderedMap<std::pair<std::string, bool>, void *> &getRegisteredJ2IThunkMap() { return _registeredJ2IThunksMap; }
   PersistentUnorderedSet<std::pair<std::string, bool>> &getRegisteredInvokeExactJ2IThunkSet() { return _registeredInvokeExactJ2IThunksSet; }

   template <typename map, typename key>
   void purgeCache(std::vector<ClassUnloadedData> *unloadedClasses, map& m, key ClassUnloadedData::*k);

   J9SharedClassCacheDescriptor * reconstructJ9SharedClassCacheDescriptorList(const std::vector<CacheDescriptor> &listOfCacheDescriptors);
   void destroyJ9SharedClassCacheDescriptorList();

   volatile bool isClassUnloadingAttempted() const { return _bClassUnloadingAttempt; }
   volatile bool isReadingClassUnload() { return !omrthread_rwmutex_is_writelocked(_classUnloadRWMutex); }

   void readAcquireClassUnloadRWMutex();
   void readReleaseClassUnloadRWMutex();
   void writeAcquireClassUnloadRWMutex();
   void writeReleaseClassUnloadRWMutex();

   private:
   const uint64_t _clientUID;
   int64_t  _timeOfLastAccess; // in ms
   TR_OpaqueClassBlock *_javaLangClassPtr; // NULL means not set
   // Server side CHTable
   JITServerPersistentCHTable *_chTable;
   // Server side cache of j9classes and their properties; romClass is copied so it can be accessed by the server
   PersistentUnorderedMap<J9Class*, ClassInfo> _romClassMap;
   // Hashtable for information related to one J9Method
   PersistentUnorderedMap<J9Method*, J9MethodInfo> _J9MethodMap;
   // The following hashtable caches <classname> --> <J9Class> mappings
   // All classes in here are loaded by the systemClassLoader so we know they cannot be unloaded
   PersistentUnorderedMap<ClassLoaderStringPair, TR_OpaqueClassBlock*> _classBySignatureMap;

   PersistentUnorderedMap<J9Class *, UDATA *> _classChainDataMap;
   //Constant pool to class map
   PersistentUnorderedMap<J9ConstantPool *, TR_OpaqueClassBlock *> _constantPoolToClassMap;
   TR::Monitor *_romMapMonitor;
   TR::Monitor *_classMapMonitor;
   TR::Monitor *_classChainDataMapMonitor;
   // The following monitor is used to protect access to _expectedSeqNo and
   // the list of out-of-sequence compilation requests (_OOSequenceEntryList)
   TR::Monitor *_sequencingMonitor;
   TR::Monitor *_constantPoolMapMonitor;
   // Compilation requests that arrived out-of-sequence wait in
   // _OOSequenceEntryList for their turn to be processed
   TR_MethodToBeCompiled *_OOSequenceEntryList;
   uint32_t _expectedSeqNo; // used for ordering compilation requests from the same client
   uint32_t _maxReceivedSeqNo; // the largest seqNo received from this client
   int8_t  _inUse;  // Number of concurrent compilations from the same client
                    // Accessed with compilation monitor in hand
   int8_t _numActiveThreads; // Number of threads working on compilations for this client
                             // This is smaller or equal to _inUse because some threads
                             // could be just starting or waiting in _OOSequenceEntryList
   VMInfo *_vmInfo; // info specific to a client VM that does not change, NULL means not set
   bool _markedForDeletion; //Client Session is marked for deletion. When the inUse count will become zero this will be deleted.
   TR_AddressSet *_unloadedClassAddresses; // Per-client versions of the unloaded class and method addresses kept in J9PersistentInfo
   bool           _requestUnloadedClasses; // If true we need to request the current state of unloaded classes from the client
   TR::Monitor *_staticMapMonitor;
   PersistentUnorderedMap<void *, TR_StaticFinalData> _staticFinalDataMap; // stores values at static final addresses in JVM
   bool _rtResolve; // treat all data references as unresolved
   TR::Monitor *_thunkSetMonitor;
   PersistentUnorderedMap<std::pair<std::string, bool>, void *> _registeredJ2IThunksMap; // stores a map of J2I thunks created for this client
   PersistentUnorderedSet<std::pair<std::string, bool>> _registeredInvokeExactJ2IThunksSet; // stores a set of invoke exact J2I thunks created for this client

   omrthread_rwmutex_t _classUnloadRWMutex;
   volatile bool _bClassUnloadingAttempt;
   }; // class ClientSessionData


/**
   @class ClientSessionHT
   @brief Hashtable that maps clientUID to a pointer that points to ClientSessionData

   This indirection is needed so that we can cache the value of the pointer so
   that we can access client session data without going through the hashtable.
   Accesss to this hashtable must be protected by the compilation monitor.
   Compilation threads may purge old entries periodically at the beginning of a
   compilation. The StatistcisThread can also perform purging duties.
   Entried with inUse > 0 must not be purged.
 */

class ClientSessionHT
   {
   public:
   ClientSessionHT();
   ~ClientSessionHT();
   static ClientSessionHT* allocate(); // allocates a new instance of this class
   ClientSessionData * findOrCreateClientSession(uint64_t clientUID, uint32_t seqNo, bool *newSessionWasCreated);
   bool deleteClientSession(uint64_t clientUID, bool forDeletion);
   ClientSessionData * findClientSession(uint64_t clientUID);
   void purgeOldDataIfNeeded();
   void printStats();
   uint32_t size() const { return _clientSessionMap.size(); }

   private:
   PersistentUnorderedMap<uint64_t, ClientSessionData*> _clientSessionMap;

   uint64_t _timeOfLastPurge;
   const int64_t TIME_BETWEEN_PURGES; // ms; this defines how often we are willing to scan for old entries to be purged
   const int64_t OLD_AGE;// ms; this defines what an old entry means
                         // This value must be larger than the expected life of a JVM
   }; // class ClientSessionHT

#endif /* defined(JIT_CLIENT_SESSION_H) */

