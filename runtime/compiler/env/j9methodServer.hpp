#ifndef J9METHODSERVER_H
#define J9METHODSERVER_H

#include "env/j9method.h"
#include "env/PersistentCollections.hpp"
#include "control/JITaaSCompilationThread.hpp"

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
   bool isMethodInValidLibrary;
   TR::RecognizedMethod mandatoryRm;
   TR::RecognizedMethod rm;
   void *startAddressForJittedMethod;
   bool virtualMethodIsOverridden;
   void *addressContainingIsOverriddenBit;
   J9ClassLoader *classLoader;
   }; 

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

namespace std {
  template <>
  struct hash<TR_RemoteROMStringKey>
  {
    std::size_t operator()(const TR_RemoteROMStringKey& k) const
    {
      // Compute a hash for the table of ROM strings by hashing basePtr and offsets 
      // separately and then XORing them
      return (std::hash<void *>()(k.basePtr)) ^ (std::hash<uint32_t>()(k.offsets));
    }
  };
}

// The last 2 strings are serialized versions of jittedBodyInfo and persistentMethodInfo
typedef std::tuple<TR_ResolvedJ9JITaaSServerMethodInfoStruct, std::string, std::string> TR_ResolvedJ9JITaaSServerMethodInfo;

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
   virtual bool isWarmCallGraphTooBig(uint32_t, TR::Compilation *) override;
   virtual void setWarmCallGraphTooBig(uint32_t, TR::Compilation *) override;
   virtual void setVirtualMethodIsOverridden() override;
   virtual void * addressContainingIsOverriddenBit() override { return _addressContainingIsOverriddenBit; }
   virtual bool methodIsNotzAAPEligible() override;
   virtual void setClassForNewInstance(J9Class *c) override;
   virtual TR_OpaqueClassBlock * classOfMethod() override;
   virtual TR_PersistentJittedBodyInfo *getExistingJittedBodyInfo() override;

   TR_ResolvedJ9Method *getRemoteMirror() const { return _remoteMirror; }
   bool inROMClass(void *address);
   static void createResolvedJ9MethodMirror(TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo, TR_OpaqueMethodBlock *method, uint32_t vTableSlot, TR_ResolvedMethod *owningMethod, TR_FrontEnd *fe, TR_Memory *trMemory);

private:
   JITaaS::J9ServerStream *_stream;
   J9ROMClass *_romClass; // cached copy of ROM class from client
   J9RAMConstantPoolItem *_literals; // client pointer to constant pool
   J9Class *_ramClass; // client pointer to RAM class
   TR_ResolvedJ9Method *_remoteMirror;
   void *_startAddressForJittedMethod; // JIT entry point
   void *_addressContainingIsOverriddenBit; // Only valid at the client. Cached info from the client
   J9ClassLoader *_classLoader; // class loader for the class of this method; only valid at the client
   bool _isInterpreted; // cached information coming from client
   bool _isMethodInValidLibrary;
   bool _virtualMethodIsOverridden; // cached information coming from client
   TR_PersistentJittedBodyInfo *_bodyInfo; // cached info coming from the client; uses heap memory
                                           // If method is not yet compiled this is null

   char* getROMString(int32_t& len, void *basePtr, std::initializer_list<size_t> offsets);
   char* getRemoteROMString(int32_t& len, void *basePtr, std::initializer_list<size_t> offsets);
   virtual char * fieldOrStaticName(I_32 cpIndex, int32_t & len, TR_Memory * trMemory, TR_AllocationKind kind = heapAlloc) override;
   void setAttributes(TR_OpaqueMethodBlock * aMethod, TR_FrontEnd * fe, TR_Memory * trMemory, uint32_t vTableSlot, TR::CompilationInfoPerThread *threadCompInfo, const TR_ResolvedJ9JITaaSServerMethodInfo &methodInfo);
   UnorderedMap<uint32_t, TR_J9MethodFieldAttributes> _fieldAttributesCache; 
   UnorderedMap<uint32_t, TR_J9MethodFieldAttributes> _staticAttributesCache; 
   };

#endif // J9METHODSERVER_H
