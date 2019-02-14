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

#ifndef J9METHODSERVER_H
#define J9METHODSERVER_H

#include "env/j9method.h"
#include "env/PersistentCollections.hpp"
#include "control/JITaaSCompilationThread.hpp"
#include "runtime/JITaaSIProfiler.hpp"

struct
TR_J9MethodFieldAttributes
   {
   // for field attributes, use fieldOffset,
   // for static attributes, use address
   union
      {
      U_32 fieldOffset;
      void *address;
      };
   TR::DataTypes type; 
   bool volatileP;
   bool isFinal;
   bool isPrivate;
   bool unresolvedInCP;
   bool result;
   };

struct
TR_ResolvedJ9JITaaSServerMethodInfoStruct
   {
   TR_ResolvedJ9Method *remoteMirror;
   J9RAMConstantPoolItem *literals;
   J9Class *ramClass;
   uint64_t methodIndex;
   uintptrj_t jniProperties;
   void *jniTargetAddress;
   bool isInterpreted;
   bool isJNINative;
   bool isMethodInValidLibrary;
   TR::RecognizedMethod mandatoryRm;
   TR::RecognizedMethod rm;
   void *startAddressForJittedMethod;
   bool virtualMethodIsOverridden;
   void *addressContainingIsOverriddenBit;
   J9ClassLoader *classLoader;
   };

// The last 3 strings are serialized versions of jittedBodyInfo, persistentMethodInfo and TR_ContiguousIPMethodHashTableInfo
using TR_ResolvedJ9JITaaSServerMethodInfo = std::tuple<TR_ResolvedJ9JITaaSServerMethodInfoStruct, std::string, std::string, std::string>;

struct
TR_RemoteROMStringKey
   {
   void *basePtr;
   uint32_t offsets;
   bool operator==(const TR_RemoteROMStringKey &other) const
      {
      return (basePtr == other.basePtr) && (offsets == other.offsets);
      }
   };

// define a hash function for TR_RemoteROMStringKey
namespace std 
   {
   template <>
   struct hash<TR_RemoteROMStringKey>
      {
      std::size_t operator()(const TR_RemoteROMStringKey &k) const
         {
         // Compute a hash for the table of ROM strings by hashing basePtr and offsets 
         // separately and then XORing them
         return (std::hash<void *>()(k.basePtr)) ^ (std::hash<uint32_t>()(k.offsets));
         }
      };
   }

// key used to identify a resolved method in resolved methods cache.
// Since one cache contains different types of resolved methods, need to uniquely identify
// each type. Apparently, the same cpIndex may refer to both virtual or special method,
// for different method calls, so using TR_ResolvedMethodType is necessary.
enum TR_ResolvedMethodType {Virtual, Interface, Static, Special};
struct
TR_ResolvedMethodKey
   {
   TR_ResolvedMethodType type;
   int32_t cpIndex;
   TR_OpaqueClassBlock *classObject; // only set for resolved interface methods

   bool operator==(const TR_ResolvedMethodKey &other) const
      {
      return type == other.type && cpIndex == other.cpIndex && classObject == other.classObject;
      }
   };


// define a hash function for TR_ResolvedMethodKey
namespace std
   {
   template <>
   struct hash<TR_ResolvedMethodKey>
      {
      std::size_t operator()(const TR_ResolvedMethodKey &k) const
         {
         // Compute a hash by hashing each part separately and then XORing them.
         return std::hash<int32_t>()(static_cast<int32_t>(k.type)) ^ std::hash<int32_t>()(k.cpIndex) ^ std::hash<TR_OpaqueClassBlock *>()(k.classObject);
         }
      };
   }

struct TR_ResolvedMethodCacheEntry
   {
   TR_ResolvedMethod *resolvedMethod;
   bool unresolvedInCP;
   };

class TR_ResolvedJ9JITaaSServerMethod : public TR_ResolvedJ9Method
   {
public:
   TR_ResolvedJ9JITaaSServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);
   TR_ResolvedJ9JITaaSServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, const TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);

   virtual J9ROMClass *romClassPtr() override;
   virtual J9RAMConstantPoolItem *literals() override;
   virtual J9Class *constantPoolHdr() override;
   virtual bool isJNINative() override;
   virtual bool isInterpreted() override { return _isInterpreted; };
   virtual bool isMethodInValidLibrary() override { return _isMethodInValidLibrary; };
   virtual bool shouldFailSetRecognizedMethodInfoBecauseOfHCR() override;
   virtual void setRecognizedMethodInfo(TR::RecognizedMethod rm) override;
   virtual J9ClassLoader *getClassLoader() override;
   virtual bool staticAttributes( TR::Compilation *, int32_t cpIndex, void * *, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOTValidation) override;
   virtual TR_OpaqueClassBlock * getClassFromConstantPool( TR::Compilation *, uint32_t cpIndex, bool returnClassToAOT = false) override;
   virtual TR_OpaqueClassBlock * getDeclaringClassFromFieldOrStatic( TR::Compilation *comp, int32_t cpIndex) override;
   virtual TR_OpaqueClassBlock * classOfStatic(int32_t cpIndex, bool returnClassForAOT = false) override;
   virtual bool isConstantDynamic(I_32 cpIndex) override;
   virtual bool isUnresolvedString(int32_t cpIndex, bool optimizeForAOT = false) override;
   virtual TR_ResolvedMethod * getResolvedVirtualMethod( TR::Compilation *, int32_t cpIndex, bool ignoreRtResolve, bool * unresolvedInCP) override;
   virtual TR_ResolvedMethod *   getResolvedPossiblyPrivateVirtualMethod( TR::Compilation *, int32_t cpIndex, bool ignoreRtResolve, bool * unresolvedInCP) override;
   virtual TR_ResolvedMethod * getResolvedStaticMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP) override;
   virtual TR_ResolvedMethod * getResolvedSpecialMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP) override;
   virtual TR_ResolvedMethod * createResolvedMethodFromJ9Method( TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats) override;
   virtual TR_ResolvedMethod * createResolvedMethodFromJ9Method( TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats, const TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo);
   virtual uint32_t classCPIndexOfMethod(uint32_t methodCPIndex) override;
   virtual bool fieldsAreSame(int32_t cpIndex1, TR_ResolvedMethod *m2, int32_t cpIndex2, bool &sigSame) override;
   virtual bool staticsAreSame(int32_t cpIndex1, TR_ResolvedMethod *m2, int32_t cpIndex2, bool &sigSame) override;
   virtual bool fieldAttributes(TR::Compilation *, int32_t cpIndex, uint32_t * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOTValidation) override;
   virtual void * startAddressForJittedMethod() override;
   virtual char * localName(uint32_t slotNumber, uint32_t bcIndex, int32_t &len, TR_Memory *) override;
   virtual bool virtualMethodIsOverridden() override { return _virtualMethodIsOverridden; }
   virtual TR_ResolvedMethod * getResolvedInterfaceMethod(TR::Compilation *, TR_OpaqueClassBlock * classObject, int32_t cpIndex) override;
   virtual TR_OpaqueClassBlock * getResolvedInterfaceMethod(int32_t cpIndex, uintptrj_t * pITableIndex) override;
   virtual uint32_t getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock * classObject, int32_t cpIndex) override;
   virtual TR_ResolvedMethod *   getResolvedImproperInterfaceMethod(TR::Compilation * comp, I_32 cpIndex) override;
   virtual void * startAddressForJNIMethod(TR::Compilation *) override;
   virtual bool getUnresolvedStaticMethodInCP(int32_t cpIndex) override;
   virtual void *startAddressForInterpreterOfJittedMethod() override;
   virtual bool getUnresolvedSpecialMethodInCP(I_32 cpIndex) override;
   virtual TR_ResolvedMethod *getResolvedVirtualMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset , bool ignoreRtResolve) override;
   virtual bool getUnresolvedFieldInCP(I_32 cpIndex) override;
   virtual char * fieldOrStaticSignatureChars(I_32 cpIndex, int32_t & len) override;
   virtual char * getClassNameFromConstantPool(uint32_t cpIndex, uint32_t &length) override;
   virtual char * classNameOfFieldOrStatic(int32_t cpIndex, int32_t & len) override;
   virtual char * classSignatureOfFieldOrStatic(int32_t cpIndex, int32_t & len) override;
   virtual char * fieldOrStaticNameChars(int32_t cpIndex, int32_t & len) override;
   virtual bool isSubjectToPhaseChange(TR::Compilation *comp) override;
   virtual void * stringConstant(int32_t cpIndex) override;
   virtual TR_ResolvedMethod *getResolvedHandleMethod( TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP) override;
#if defined(J9VM_OPT_REMOVE_CONSTANT_POOL_SPLITTING)
   virtual bool isUnresolvedMethodTypeTableEntry(int32_t cpIndex) override;
   virtual void * methodTypeTableEntryAddress(int32_t cpIndex) override;
#endif
   virtual bool isUnresolvedCallSiteTableEntry(int32_t callSiteIndex) override;
   virtual void * callSiteTableEntryAddress(int32_t callSiteIndex) override;
   virtual TR_ResolvedMethod * getResolvedDynamicMethod( TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP) override;
   virtual bool isSameMethod(TR_ResolvedMethod *) override;
   virtual bool isInlineable(TR::Compilation *) override;
   virtual void setWarmCallGraphTooBig(uint32_t, TR::Compilation *) override;
   virtual void setVirtualMethodIsOverridden() override;
   virtual void * addressContainingIsOverriddenBit() override { return _addressContainingIsOverriddenBit; }
   virtual bool methodIsNotzAAPEligible() override;
   virtual void setClassForNewInstance(J9Class *c) override;
   virtual TR_OpaqueClassBlock * classOfMethod() override;
   virtual TR_PersistentJittedBodyInfo *getExistingJittedBodyInfo() override;
   virtual void getFaninInfo(uint32_t *count, uint32_t *weight, uint32_t *otherBucketWeight = nullptr) override;
   virtual bool getCallerWeight(TR_ResolvedJ9Method *caller, uint32_t *weight, uint32_t pcIndex=~0) override;

   TR_ResolvedJ9Method *getRemoteMirror() const { return _remoteMirror; }
   bool inROMClass(void *address);
   static void createResolvedJ9MethodMirror(TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedMethod *owningMethod, TR_FrontEnd *fe, TR_Memory *trMemory);
   static bool _useCaching; // set by TR_DisableResolvedMethodsCaching. When false, caching of resolved methods is disabled 

protected:
   JITaaS::J9ServerStream *_stream;
   static void packMethodInfo(TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo, TR_ResolvedJ9Method *resolvedMethod, TR_FrontEnd *fe);

private:

   J9ROMClass *_romClass; // cached copy of ROM class from client
   J9RAMConstantPoolItem *_literals; // client pointer to constant pool
   J9Class *_ramClass; // client pointer to RAM class
   TR_ResolvedJ9Method *_remoteMirror;
   void *_startAddressForJittedMethod; // JIT entry point
   void *_addressContainingIsOverriddenBit; // Only valid at the client. Cached info from the client
   J9ClassLoader *_classLoader; // class loader for the class of this method; only valid at the client
   bool _isInterpreted; // cached information coming from client
   bool _isJNINative;
   bool _isMethodInValidLibrary;
   bool _virtualMethodIsOverridden; // cached information coming from client
   TR_PersistentJittedBodyInfo *_bodyInfo; // cached info coming from the client; uses heap memory
                                           // If method is not yet compiled this is null
   UnorderedMap<uint32_t, TR_J9MethodFieldAttributes> _fieldAttributesCache; 
   UnorderedMap<uint32_t, TR_J9MethodFieldAttributes> _staticAttributesCache; 
   UnorderedMap<TR_ResolvedMethodKey, TR_ResolvedMethodCacheEntry> _resolvedMethodsCache;
   TR_IPMethodHashTableEntry *_iProfilerMethodEntry;

   char* getROMString(int32_t& len, void *basePtr, std::initializer_list<size_t> offsets);
   char* getRemoteROMString(int32_t& len, void *basePtr, std::initializer_list<size_t> offsets);
   virtual char * fieldOrStaticName(I_32 cpIndex, int32_t & len, TR_Memory * trMemory, TR_AllocationKind kind = heapAlloc) override;
   void unpackMethodInfo(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, uint32_t vTableSlot, TR::CompilationInfoPerThread *threadCompInfo, const TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo);
   void cacheResolvedMethod(TR_ResolvedMethodKey key, TR_ResolvedMethod *resolvedMethod, bool *unresolvedInCP);
   bool getCachedResolvedMethod(TR_ResolvedMethodKey key, TR_ResolvedMethod **resolvedMethod, bool *unresolvedInCP);
   };

class TR_ResolvedRelocatableJ9JITaaSServerMethod : public TR_ResolvedJ9JITaaSServerMethod
   {
   public:
   TR_ResolvedRelocatableJ9JITaaSServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);
   TR_ResolvedRelocatableJ9JITaaSServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, const TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo, bool rememberedClass, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);

   virtual bool                  isInterpreted() override;
   virtual bool                  isInterpretedForHeuristics() override;

   virtual void *                startAddressForJittedMethod() override;
   virtual void *                startAddressForJNIMethod( TR::Compilation *) override;
   virtual void *                startAddressForJITInternalNativeMethod() override;
   virtual void *                startAddressForInterpreterOfJittedMethod() override;

   virtual TR_OpaqueClassBlock * getClassFromConstantPool( TR::Compilation *, uint32_t cpIndex, bool returnClassToAOT = false) override;
   virtual bool                  validateClassFromConstantPool( TR::Compilation *comp, J9Class *clazz, uint32_t cpIndex, TR_ExternalRelocationTargetKind reloKind = TR_ValidateClass) override;
   virtual bool                  validateArbitraryClass( TR::Compilation *comp, J9Class *clazz) override;

   virtual void *                stringConstant(int32_t cpIndex) override { TR_ASSERT(0, "called");  return NULL; }
   virtual bool                  isUnresolvedString(int32_t cpIndex, bool optimizeForAOT = false) override { TR_ASSERT(0, "called");  return false; }
   virtual void *                methodTypeConstant(int32_t cpIndex) override { TR_ASSERT(0, "called");  return NULL; }
   virtual bool                  isUnresolvedMethodType(int32_t cpIndex) override { TR_ASSERT(0, "called");  return false; }
   virtual void *                methodHandleConstant(int32_t cpIndex) override { TR_ASSERT(0, "called");  return NULL; }
   virtual bool                  isUnresolvedMethodHandle(int32_t cpIndex) override { TR_ASSERT(0, "called");  return false; }

   virtual bool                  fieldAttributes ( TR::Compilation *, int32_t cpIndex, uint32_t * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOTValidation) override { TR_ASSERT(0, "called");  return false; }

   virtual bool                  staticAttributes( TR::Compilation *, int32_t cpIndex, void * *, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOTValidation) override { TR_ASSERT(0, "called");  return false; }

   virtual int32_t               virtualCallSelector(uint32_t cpIndex) override { TR_ASSERT(0, "called");  return 0; }
   virtual char *                fieldSignatureChars(int32_t cpIndex, int32_t & len) override { TR_ASSERT(0, "called");  return NULL; }
   virtual char *                staticSignatureChars(int32_t cpIndex, int32_t & len) override { TR_ASSERT(0, "called");  return NULL; }

   virtual TR_OpaqueClassBlock * classOfStatic(int32_t cpIndex, bool returnClassForAOT = false) override { TR_ASSERT(0, "called");  return NULL; }
   virtual TR_ResolvedMethod *   getResolvedPossiblyPrivateVirtualMethod( TR::Compilation *, int32_t cpIndex, bool ignoreRtResolve, bool * unresolvedInCP) override { TR_ASSERT(0, "called");  return NULL; }

   virtual bool                  getUnresolvedFieldInCP(int32_t cpIndex) override { TR_ASSERT(0, "called");  return false; }
   virtual bool                  getUnresolvedStaticMethodInCP(int32_t cpIndex) override { TR_ASSERT(0, "called");  return false; }
   virtual bool                  getUnresolvedSpecialMethodInCP(int32_t cpIndex) override { TR_ASSERT(0, "called");  return false; }
   virtual bool                  getUnresolvedVirtualMethodInCP(int32_t cpIndex) override { TR_ASSERT(0, "called");  return false; }

   virtual TR_ResolvedMethod *   getResolvedImproperInterfaceMethod(TR::Compilation * comp, I_32 cpIndex) override { TR_ASSERT(0, "called");  return NULL; }

   virtual TR_OpaqueMethodBlock *getNonPersistentIdentifier() override;
   virtual uint8_t *             allocateException(uint32_t, TR::Compilation*) override;

   virtual TR_OpaqueClassBlock  *getDeclaringClassFromFieldOrStatic( TR::Compilation *comp, int32_t cpIndex) override { TR_ASSERT(0, "called");  return NULL; }
   bool                  storeValidationRecordIfNecessary(TR::Compilation * comp, J9ConstantPool *constantPool, int32_t cpIndex, TR_ExternalRelocationTargetKind reloKind, J9Method *ramMethod, J9Class *definingClass=0);
   static bool createResolvedRelocatableJ9MethodMirror(TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedMethod *owningMethod, TR_FrontEnd *fe, TR_Memory *trMemory);

protected:
   virtual TR_ResolvedMethod *   createResolvedMethodFromJ9Method(TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats);

   virtual void                  handleUnresolvedStaticMethodInCP(int32_t cpIndex, bool * unresolvedInCP) override { TR_ASSERT(0, "called");  return; }
   virtual void                  handleUnresolvedSpecialMethodInCP(int32_t cpIndex, bool * unresolvedInCP) override { TR_ASSERT(0, "called");  return; }
   virtual void                  handleUnresolvedVirtualMethodInCP(int32_t cpIndex, bool * unresolvedInCP) override { TR_ASSERT(0, "called");  return; }
   virtual char *                fieldOrStaticNameChars(int32_t cpIndex, int32_t & len) override { TR_ASSERT(0, "called");  return NULL; }
   };
#endif // J9METHODSERVER_H
