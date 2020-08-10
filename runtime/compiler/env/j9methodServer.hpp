/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#include "control/J9Recompilation.hpp"
#include "env/j9method.h"
#include "env/PersistentCollections.hpp"
#include "runtime/JITServerIProfiler.hpp"
#include "runtime/JITClientSession.hpp"

struct
TR_ResolvedJ9JITServerMethodInfoStruct
   {
   TR_ResolvedJ9JITServerMethodInfoStruct() :
      remoteMirror(NULL)
      {}

   TR_ResolvedJ9Method *remoteMirror;
   J9RAMConstantPoolItem *literals;
   J9Class *ramClass;
   uint64_t methodIndex;
   uintptr_t jniProperties;
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
using TR_ResolvedJ9JITServerMethodInfo = std::tuple<TR_ResolvedJ9JITServerMethodInfoStruct, std::string, std::string, std::string>;

// key used to identify a resolved method in resolved methods cache.
// Since one cache contains different types of resolved methods, need to uniquely identify
// each type. Apparently, the same cpIndex may refer to both virtual or special method,
// for different method calls, so using TR_ResolvedMethodType is necessary.
enum TR_ResolvedMethodType {VirtualFromCP, VirtualFromOffset, Interface, Static, Special, ImproperInterface, NoType};
struct
TR_ResolvedMethodKey
   {
   TR_ResolvedMethodType type;
   TR_OpaqueClassBlock *ramClass;
   int32_t cpIndex;
   TR_OpaqueClassBlock *classObject; // only set for resolved interface methods

   bool operator==(const TR_ResolvedMethodKey &other) const
      {
      return
         type == other.type &&
         ramClass == other.ramClass &&
         cpIndex == other.cpIndex &&
         classObject == other.classObject;
      }
   };


struct TR_IsUnresolvedString
   {
   TR_IsUnresolvedString():
      _optimizeForAOTTrueResult(false),
      _optimizeForAOTFalseResult(false)
   {}
   TR_IsUnresolvedString(bool optimizeForAOTTrueResult, bool optimizeForAOTFalseResult):
      _optimizeForAOTTrueResult(optimizeForAOTTrueResult),
      _optimizeForAOTFalseResult(optimizeForAOTFalseResult)
   {}
   bool _optimizeForAOTTrueResult;
   bool _optimizeForAOTFalseResult;
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
         return
            std::hash<int32_t>()(static_cast<int32_t>(k.type)) ^
            std::hash<TR_OpaqueClassBlock *>()(k.ramClass) ^
            std::hash<int32_t>()(k.cpIndex) ^
            std::hash<TR_OpaqueClassBlock *>()(k.classObject);
         }
      };
   }

struct
TR_ResolvedMethodCacheEntry
   {
   TR_OpaqueMethodBlock *method;
   uint32_t vTableSlot;
   TR_ResolvedJ9JITServerMethodInfoStruct methodInfoStruct;
   TR_PersistentJittedBodyInfo *persistentBodyInfo;
   TR_PersistentMethodInfo *persistentMethodInfo;
   TR_ContiguousIPMethodHashTableEntry *IPMethodInfo;
   int32_t ttlForUnresolved;
   };

using TR_ResolvedMethodInfoCache = UnorderedMap<TR_ResolvedMethodKey, TR_ResolvedMethodCacheEntry>;

/**
 * @class TR_ResolvedJ9JITServerMethod
 * @brief Class used by JITServer for obtaining method/class information needed
 * during compilation from JITClient
 *
 * This class is an extension of the TR_ResolvedJ9Method class. Most of the APIs of
 * TR_ResolvedJ9JITServerMethod are remote calls to the JITClient to obtain
 * compilation information about the compiling method and related classes. Upon
 * instantiation of a TR_ResolvedJ9JITServerMethod class, a mirror TR_ResolvedJ9Method
 * will be created on the client via TR_ResolvedJ9JITServerMethod::createResolvedMethodMirror.
 * During compilation the JITServer will mostly be communicating with the client-side
 * mirror instance. Certain data are cached at the JITServer to reduce the number
 * of remote calls to JITClient.
 */

class TR_ResolvedJ9JITServerMethod : public TR_ResolvedJ9Method
   {
public:
   TR_ResolvedJ9JITServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);
   TR_ResolvedJ9JITServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, const TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);

   virtual J9ROMClass *romClassPtr() override;
   virtual J9RAMConstantPoolItem *literals() override;
   virtual J9Class *constantPoolHdr() override;
   virtual bool isJNINative() override;
   virtual bool isInterpreted() override { return _isInterpreted; };
   virtual bool isMethodInValidLibrary() override { return _isMethodInValidLibrary; };
   virtual bool shouldFailSetRecognizedMethodInfoBecauseOfHCR() override;
   virtual void setRecognizedMethodInfo(TR::RecognizedMethod rm) override;
   virtual J9ClassLoader *getClassLoader() override;
   virtual bool staticAttributes(TR::Compilation *, int32_t cpIndex, void * *, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOTValidation) override;
   virtual TR_OpaqueClassBlock * definingClassFromCPFieldRef(TR::Compilation *comp, int32_t cpIndex, bool isStatic) override;
   virtual TR_OpaqueClassBlock * getClassFromConstantPool(TR::Compilation *, uint32_t cpIndex, bool returnClassToAOT = false) override;
   virtual TR_OpaqueClassBlock * getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex) override;
   virtual TR_OpaqueClassBlock * classOfStatic(int32_t cpIndex, bool returnClassForAOT = false) override;

   virtual void * getConstantDynamicTypeFromCP(int32_t cpIndex) override;
   virtual bool isConstantDynamic(I_32 cpIndex) override;
   virtual bool isUnresolvedConstantDynamic(int32_t cpIndex) override;
   virtual void * dynamicConstant(int32_t cpIndex, uintptr_t *obj) override;

   virtual bool isUnresolvedString(int32_t cpIndex, bool optimizeForAOT = false) override;
   virtual TR_ResolvedMethod * getResolvedVirtualMethod(TR::Compilation *, int32_t cpIndex, bool ignoreRtResolve, bool * unresolvedInCP) override;
   virtual TR_ResolvedMethod *   getResolvedPossiblyPrivateVirtualMethod(TR::Compilation *, int32_t cpIndex, bool ignoreRtResolve, bool * unresolvedInCP) override;
   virtual TR_ResolvedMethod * getResolvedStaticMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP) override;
   virtual TR_ResolvedMethod * getResolvedSpecialMethod(TR::Compilation * comp, I_32 cpIndex, bool * unresolvedInCP) override;
   virtual TR_ResolvedMethod * createResolvedMethodFromJ9Method(TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats) override;
   virtual TR_ResolvedMethod * createResolvedMethodFromJ9Method(TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats, const TR_ResolvedJ9JITServerMethodInfo &methodInfo);
   virtual uint32_t classCPIndexOfMethod(uint32_t methodCPIndex) override;
   virtual bool fieldsAreSame(int32_t cpIndex1, TR_ResolvedMethod *m2, int32_t cpIndex2, bool &sigSame) override;
   virtual bool staticsAreSame(int32_t cpIndex1, TR_ResolvedMethod *m2, int32_t cpIndex2, bool &sigSame) override;
   virtual bool fieldAttributes(TR::Compilation *, int32_t cpIndex, uint32_t * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP, bool needsAOTValidation) override;
   virtual void * startAddressForJittedMethod() override;
   virtual char * localName(uint32_t slotNumber, uint32_t bcIndex, int32_t &len, TR_Memory *) override;
   virtual bool virtualMethodIsOverridden() override { return _virtualMethodIsOverridden; }
   virtual TR_ResolvedMethod * getResolvedInterfaceMethod(TR::Compilation *, TR_OpaqueClassBlock * classObject, int32_t cpIndex) override;
   virtual TR_OpaqueClassBlock * getResolvedInterfaceMethod(int32_t cpIndex, uintptr_t * pITableIndex) override;
   virtual uint32_t getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock * classObject, int32_t cpIndex) override;
   virtual TR_ResolvedMethod *   getResolvedImproperInterfaceMethod(TR::Compilation * comp, I_32 cpIndex) override;
   virtual void * startAddressForJNIMethod(TR::Compilation *) override;
   virtual void *startAddressForInterpreterOfJittedMethod() override;
   virtual TR_ResolvedMethod *getResolvedVirtualMethod(TR::Compilation * comp, TR_OpaqueClassBlock * classObject, I_32 virtualCallOffset , bool ignoreRtResolve) override;
   virtual char * fieldOrStaticSignatureChars(I_32 cpIndex, int32_t & len) override;
   virtual char * getClassNameFromConstantPool(uint32_t cpIndex, uint32_t &length) override;
   virtual char * classNameOfFieldOrStatic(int32_t cpIndex, int32_t & len) override;
   virtual char * classSignatureOfFieldOrStatic(int32_t cpIndex, int32_t & len) override;
   virtual char * fieldOrStaticNameChars(int32_t cpIndex, int32_t & len) override;
   virtual bool isSubjectToPhaseChange(TR::Compilation *comp) override;
   virtual void * stringConstant(int32_t cpIndex) override;
   virtual TR_ResolvedMethod *getResolvedHandleMethod(TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP) override;
   virtual bool isUnresolvedMethodTypeTableEntry(int32_t cpIndex) override;
   virtual void * methodTypeTableEntryAddress(int32_t cpIndex) override;
   virtual bool isUnresolvedCallSiteTableEntry(int32_t callSiteIndex) override;
   virtual void * callSiteTableEntryAddress(int32_t callSiteIndex) override;
   virtual bool isUnresolvedVarHandleMethodTypeTableEntry(int32_t cpIndex) override;
   virtual void * varHandleMethodTypeTableEntryAddress(int32_t cpIndex) override;
   virtual TR_ResolvedMethod * getResolvedDynamicMethod(TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP) override;
   virtual bool isSameMethod(TR_ResolvedMethod *) override;
   virtual bool isInlineable(TR::Compilation *) override;
   virtual void setWarmCallGraphTooBig(uint32_t, TR::Compilation *) override;
   virtual void setVirtualMethodIsOverridden() override;
   virtual void * addressContainingIsOverriddenBit() override { return _addressContainingIsOverriddenBit; }
   virtual bool methodIsNotzAAPEligible() override;
   virtual void setClassForNewInstance(J9Class *c) override;
   virtual TR_OpaqueClassBlock * classOfMethod() override;
   virtual TR_PersistentJittedBodyInfo *getExistingJittedBodyInfo() override;
   virtual void getFaninInfo(uint32_t *count, uint32_t *weight, uint32_t *otherBucketWeight = NULL) override;
   virtual bool getCallerWeight(TR_ResolvedJ9Method *caller, uint32_t *weight, uint32_t pcIndex=~0) override;
   virtual uint16_t archetypeArgPlaceholderSlot() override;

   TR_ResolvedJ9Method *getRemoteMirror() const { return _remoteMirror; }
   static void createResolvedMethodMirror(TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedMethod *owningMethod, TR_FrontEnd *fe, TR_Memory *trMemory);
   static void createResolvedMethodFromJ9MethodMirror(TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedMethod *owningMethod, TR_FrontEnd *fe, TR_Memory *trMemory);
   bool addValidationRecordForCachedResolvedMethod(const TR_ResolvedMethodKey &key, TR_OpaqueMethodBlock *method);
   void cacheResolvedMethodsCallees(int32_t ttlForUnresolved = 2);
   void cacheFields();
   int32_t collectImplementorsCapped(TR_OpaqueClassBlock *topClass, int32_t maxCount, int32_t cpIndexOrOffset, TR_YesNoMaybe useGetResolvedInterfaceMethod, TR_ResolvedMethod **implArray);
   static void packMethodInfo(TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_ResolvedJ9Method *resolvedMethod, TR_FrontEnd *fe);

protected:
   JITServer::ServerStream *_stream;
   J9Class *_ramClass; // client pointer to RAM class
   static void setAttributeResultFromResolvedMethodFieldAttributes(const TR_J9MethodFieldAttributes &attributes, U_32 * fieldOffset, void **address, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool * unresolvedInCP, bool *result, bool isStatic);
   virtual bool getCachedFieldAttributes(int32_t cpIndex, TR_J9MethodFieldAttributes &attributes, bool isStatic);
   virtual void cacheFieldAttributes(int32_t cpIndex, const TR_J9MethodFieldAttributes &attributes, bool isStatic);
   virtual TR_FieldAttributesCache &getAttributesCache(bool isStatic, bool unresolvedInCP=false);
   virtual bool validateMethodFieldAttributes(const TR_J9MethodFieldAttributes &attributes, bool isStatic, int32_t cpIndex, bool isStore, bool needAOTValidation);
   virtual bool canCacheFieldAttributes(int32_t cpIndex, const TR_J9MethodFieldAttributes &attributes, bool isStatic);

private:

   J9ROMClass *_romClass; // cached copy of ROM class from client
   J9RAMConstantPoolItem *_literals; // client pointer to constant pool
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
   TR_IPMethodHashTableEntry *_iProfilerMethodEntry;

   char* getROMString(int32_t& len, void *basePtr, std::initializer_list<size_t> offsets);
   char* getRemoteROMString(int32_t& len, void *basePtr, std::initializer_list<size_t> offsets);
   virtual char * fieldOrStaticName(I_32 cpIndex, int32_t & len, TR_Memory * trMemory, TR_AllocationKind kind = heapAlloc) override;
   void unpackMethodInfo(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, uint32_t vTableSlot, TR::CompilationInfoPerThread *threadCompInfo, const TR_ResolvedJ9JITServerMethodInfo &methodInfo);
   };


/**
 * @class TR_ResolvedRelocatableJ9JITServerMethod
 * @brief Class used by JITServer for obtaining method/class information needed
 * during compilation from JITClient plus additional handling for AOT compilations
 *
 * This class is an extension of the above TR_ResolvedJ9JITServerMethod class that
 * has additional handling for AOT compilations. The relationship between
 * TR_ResolvedJ9JITServerMethod and TR_ResolvedRelocatableJ9JITServerMethod is similar
 * to the relationship between TR_ResolvedJ9Method and TR_ResolvedRelocatableJ9Method.
 */

class TR_ResolvedRelocatableJ9JITServerMethod : public TR_ResolvedJ9JITServerMethod
   {
   public:
   TR_ResolvedRelocatableJ9JITServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);
   TR_ResolvedRelocatableJ9JITServerMethod(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd *, TR_Memory *, const TR_ResolvedJ9JITServerMethodInfo &methodInfo, TR_ResolvedMethod * owningMethod = 0, uint32_t vTableSlot = 0);

   virtual void *                constantPool() override;
   virtual bool                  isInterpreted() override;
   virtual bool                  isInterpretedForHeuristics() override;

   virtual void *                startAddressForJittedMethod() override;
   virtual void *                startAddressForJNIMethod(TR::Compilation *) override;
   virtual void *                startAddressForJITInternalNativeMethod() override;
   virtual void *                startAddressForInterpreterOfJittedMethod() override;

   virtual TR_OpaqueClassBlock * definingClassFromCPFieldRef(TR::Compilation *comp, int32_t cpIndex, bool isStatic) override;
   virtual TR_OpaqueClassBlock * getClassFromConstantPool(TR::Compilation *, uint32_t cpIndex, bool returnClassToAOT = false) override;
   virtual bool                  validateClassFromConstantPool(TR::Compilation *comp, J9Class *clazz, uint32_t cpIndex, TR_ExternalRelocationTargetKind reloKind = TR_ValidateClass) override;
   virtual bool                  validateArbitraryClass(TR::Compilation *comp, J9Class *clazz) override;
   virtual bool                  isUnresolvedString(int32_t cpIndex, bool optimizeForAOT = false) override;
   virtual void *                methodTypeConstant(int32_t cpIndex) override;
   virtual bool                  isUnresolvedMethodType(int32_t cpIndex) override;
   virtual void *                methodHandleConstant(int32_t cpIndex) override;
   virtual bool                  isUnresolvedMethodHandle(int32_t cpIndex) override;
   virtual TR_OpaqueClassBlock * classOfStatic(int32_t cpIndex, bool returnClassForAOT = false) override;
   virtual TR_ResolvedMethod *   getResolvedPossiblyPrivateVirtualMethod(TR::Compilation *, int32_t cpIndex, bool ignoreRtResolve, bool * unresolvedInCP) override;
   virtual bool                  getUnresolvedFieldInCP(I_32 cpIndex) override;
   virtual bool                  getUnresolvedStaticMethodInCP(int32_t cpIndex) override;
   virtual bool                  getUnresolvedSpecialMethodInCP(I_32 cpIndex) override;
   virtual bool                  getUnresolvedVirtualMethodInCP(int32_t cpIndex) override;
   /* No need to override the stringConstant method as the parent method will be sufficient */
   virtual bool                  fieldAttributes(TR::Compilation * comp, int32_t cpIndex, uint32_t * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation) override;
   virtual bool                  staticAttributes(TR::Compilation * comp, int32_t cpIndex, void * * address,TR::DataType * type, bool * volatileP, bool * isFinal, bool * isPrivate, bool isStore, bool * unresolvedInCP, bool needAOTValidation);
   virtual TR_ResolvedMethod *   getResolvedImproperInterfaceMethod(TR::Compilation * comp, I_32 cpIndex) override;

   virtual TR_OpaqueMethodBlock *getNonPersistentIdentifier() override;
   virtual uint8_t *             allocateException(uint32_t, TR::Compilation*) override;
   virtual TR_OpaqueClassBlock  *getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex) override;
   bool                  storeValidationRecordIfNecessary(TR::Compilation * comp, J9ConstantPool *constantPool, int32_t cpIndex, TR_ExternalRelocationTargetKind reloKind, J9Method *ramMethod, J9Class *definingClass=0);

protected:
   virtual TR_ResolvedMethod *   createResolvedMethodFromJ9Method(TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats) override;
   virtual TR_ResolvedMethod * createResolvedMethodFromJ9Method(TR::Compilation *comp, int32_t cpIndex, uint32_t vTableSlot, J9Method *j9Method, bool * unresolvedInCP, TR_AOTInliningStats *aotStats, const TR_ResolvedJ9JITServerMethodInfo &methodInfo) override;
   virtual void                  handleUnresolvedStaticMethodInCP(int32_t cpIndex, bool * unresolvedInCP) override;
   virtual void                  handleUnresolvedSpecialMethodInCP(int32_t cpIndex, bool * unresolvedInCP) override;
   virtual void                  handleUnresolvedVirtualMethodInCP(int32_t cpIndex, bool * unresolvedInCP) override;
   virtual TR_FieldAttributesCache &getAttributesCache(bool isStatic, bool unresolvedInCP=false) override;
   virtual bool validateMethodFieldAttributes(const TR_J9MethodFieldAttributes &attributes, bool isStatic, int32_t cpIndex, bool isStore, bool needAOTValidation) override;
   UDATA getFieldType(J9ROMConstantPoolItem * CP, int32_t cpIndex);
   };

class TR_J9ServerMethod : public TR_J9Method
   {
public:
   TR_J9ServerMethod(TR_FrontEnd *trvm, TR_Memory *, J9Class * aClazz, uintptr_t cpIndex);
   };
#endif // J9METHODSERVER_H
