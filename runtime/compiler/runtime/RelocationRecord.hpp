/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

//
// PLEASE DO NOT USE ANY J9-SPECIFIC DATA TYPES IN THIS FILE
//

#ifndef RELOCATION_RECORD_INCL
#define RELOCATION_RECORD_INCL

#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/jittypes.h"
#include "infra/Link.hpp"
#include "infra/Flags.hpp"

class TR_RelocationRuntime;
class TR_RelocationRecord;
class TR_RelocationTarget;
struct TR_RelocationRecordBinaryTemplate;
typedef TR_ExternalRelocationTargetKind TR_RelocationRecordType;

// These *BinaryTemplate structs describe the shape of the binary relocation records.
struct TR_RelocationRecordBinaryTemplate
   {
   uint8_t type(TR_RelocationTarget *reloTarget);

   uint16_t _size;
   uint8_t _type;
   uint8_t _flags;

#if defined(TR_HOST_64BIT)
   uint32_t _extra;
#endif
   };

// Generating 32-bit code on a 64-bit machine or vice-versa won't work because the alignment won't
// be right.  Relying on inheritance in the structures here means padding is automatically inserted
// at each inheritance boundary: this inserted padding won't match across 32-bit and 64-bit platforms.
// Making as many fields as possible UDATA should minimize the differences and gives the most freedom
// in the hierarchy of binary relocation record structures, but the header definitely has an inheritance
// boundary at offset 4B.

struct TR_RelocationRecordHelperAddressBinaryTemplate
   {
   uint16_t _size;
   uint8_t _type;
   uint8_t _flags;
   uint32_t _helperID;
   };

struct TR_RelocationRecordPicTrampolineBinaryTemplate
   {
   uint16_t _size;
   uint8_t _type;
   uint8_t _flags;
   uint32_t _numTrampolines;
   };

struct TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   UDATA _inlinedSiteIndex;
   };

struct TR_RelocationRecordValidateArbitraryClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate //public TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   UDATA _loaderClassChainOffset;
   UDATA _classChainOffsetForClassBeingValidated;
   };


struct TR_RelocationRecordWithOffsetBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   UDATA _offset;
   };

struct TR_RelocationRecordConstantPoolBinaryTemplate : public TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   UDATA _constantPool;
   };

struct TR_RelocationRecordConstantPoolWithIndexBinaryTemplate : public TR_RelocationRecordConstantPoolBinaryTemplate
   {
   UDATA _index;
   };

typedef TR_RelocationRecordConstantPoolWithIndexBinaryTemplate TR_RelocationRecordClassObjectBinaryTemplate;

struct TR_RelocationRecordJ2IVirtualThunkPointerBinaryTemplate : public TR_RelocationRecordConstantPoolBinaryTemplate
   {
   IDATA _offsetToJ2IVirtualThunkPointer;
   };

struct TR_RelocationRecordInlinedAllocationBinaryTemplate : public TR_RelocationRecordConstantPoolWithIndexBinaryTemplate
   {
   UDATA _branchOffset;
   };

struct TR_RelocationRecordVerifyClassObjectForAllocBinaryTemplate : public TR_RelocationRecordInlinedAllocationBinaryTemplate
   {
   UDATA _allocationSize;
   };

struct TR_RelocationRecordRomClassFromCPBinaryTemplate : public TR_RelocationRecordConstantPoolWithIndexBinaryTemplate
   {
   UDATA  _romClassOffsetInSharedCache;
   };

typedef TR_RelocationRecordRomClassFromCPBinaryTemplate TR_RelocationRecordInlinedMethodBinaryTemplate;

struct TR_RelocationRecordNopGuardBinaryTemplate : public TR_RelocationRecordInlinedMethodBinaryTemplate
   {
   UDATA _destinationAddress;
   };

typedef TR_RelocationRecordNopGuardBinaryTemplate TR_RelocationRecordInlinedStaticMethodWithNopGuardBinaryTemplate;
typedef TR_RelocationRecordNopGuardBinaryTemplate TR_RelocationRecordInlinedSpecialMethodWithNopGuardBinaryTemplate;
typedef TR_RelocationRecordNopGuardBinaryTemplate TR_RelocationRecordInlinedVirtualMethodWithNopGuardBinaryTemplate;
typedef TR_RelocationRecordNopGuardBinaryTemplate TR_RelocationRecordInlinedInterfaceMethodWithNopGuardBinaryTemplate;


struct TR_RelocationRecordProfiledInlinedMethodBinaryTemplate : TR_RelocationRecordInlinedMethodBinaryTemplate
   {
   UDATA _classChainIdentifyingLoaderOffsetInSharedCache;

   // Class chain of the defining class of the inlined method
   UDATA _classChainForInlinedMethod;

   // The inlined j9method's index into its defining class' array of j9methods
   UDATA _methodIndex;
   };

typedef TR_RelocationRecordProfiledInlinedMethodBinaryTemplate TR_RelocationRecordProfiledGuardBinaryTemplate;
typedef TR_RelocationRecordProfiledInlinedMethodBinaryTemplate TR_RelocationRecordProfiledClassGuardBinaryTemplate;
typedef TR_RelocationRecordProfiledInlinedMethodBinaryTemplate TR_RelocationRecordProfiledMethodGuardBinaryTemplate;


typedef TR_RelocationRecordBinaryTemplate TR_RelocationRecordRamMethodBinaryTemplate;

typedef TR_RelocationRecordBinaryTemplate TR_RelocationRecordArrayCopyHelperTemplate;

struct TR_RelocationRecordMethodTracingCheckBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   UDATA _destinationAddress;
   };

struct TR_RelocationRecordValidateClassBinaryTemplate : public TR_RelocationRecordConstantPoolWithIndexBinaryTemplate
   {
   UDATA _classChainOffsetInSharedCache;
   };

typedef TR_RelocationRecordRomClassFromCPBinaryTemplate TR_RelocationRecordValidateStaticFieldBinaryTemplate;

struct TR_RelocationRecordDataAddressBinaryTemplate : public TR_RelocationRecordConstantPoolWithIndexBinaryTemplate
   {
   UDATA _offset;
   };

struct TR_RelocationRecordPointerBinaryTemplate : TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   UDATA _classChainIdentifyingLoaderOffsetInSharedCache;
   UDATA _classChainForInlinedMethod;
   };

typedef TR_RelocationRecordPointerBinaryTemplate TR_RelocationRecordClassPointerBinaryTemplate;
struct TR_RelocationRecordMethodPointerBinaryTemplate : TR_RelocationRecordPointerBinaryTemplate
   {
   UDATA _vTableSlot;
   };
struct TR_RelocationRecordEmitClassBinaryTemplate : public TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   int32_t _bcIndex;
#if defined(TR_HOST_64BIT)
   uint32_t _extra;
#endif
   };

struct TR_RelocationRecordDebugCounterBinaryTemplate : public TR_RelocationRecordWithInlinedSiteIndexBinaryTemplate
   {
   UDATA _bcIndex;
   UDATA _offsetOfNameString;
   UDATA _delta;
   UDATA _fidelity;
   UDATA _staticDelta;
   };

typedef TR_RelocationRecordBinaryTemplate TR_RelocationRecordClassUnloadAssumptionBinaryTemplate;

struct TR_RelocationRecordValidateClassByNameBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   uint16_t _beholderID;
   UDATA _classChainOffsetInSCC;
   };

struct TR_RelocationRecordValidateProfiledClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   UDATA _classChainOffsetInSCC;
   UDATA _classChainOffsetForCLInScc;
   };

struct TR_RelocationRecordValidateClassFromCPBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   uint16_t _beholderID;
   uint32_t _cpIndex;
   };

struct TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   bool _isStatic;
   uint16_t _classID;
   uint16_t _beholderID;
   uint32_t _cpIndex;
   };

typedef TR_RelocationRecordValidateClassFromCPBinaryTemplate TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate;

struct TR_RelocationRecordValidateArrayFromCompBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _arrayClassID;
   uint16_t _componentClassID;
   };

struct TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _superClassID;
   uint16_t _childClassID;
   };

struct TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   bool _objectTypeIsFixed;
   bool _castTypeIsFixed;
   bool _isInstanceOf;
   uint16_t _classOneID;
   uint16_t _classTwoID;
   };

struct TR_RelocationRecordValidateSystemClassByNameBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _systemClassID;
   UDATA _classChainOffsetInSCC;
   };

struct TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   uint16_t _beholderID;
   uint32_t _cpIndex;
   };

typedef TR_RelocationRecordValidateClassFromCPBinaryTemplate TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate;

struct TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _childClassID;
   uint16_t _superClassID;
   };

struct TR_RelocationRecordValidateClassChainBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   UDATA _classChainOffsetInSCC;
   };

struct TR_RelocationRecordValidateMethodFromClassBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _beholderID;
   uint32_t _index;
   };

#define TR_VALIDATE_STATIC_OR_SPECIAL_METHOD_FROM_CP_IS_SPLIT 0x01

struct TR_RelocationRecordValidateMethodFromCPBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _beholderID;
   uint16_t _cpIndex; // constrained to 16 bits by the class file format
   };

typedef TR_RelocationRecordValidateMethodFromCPBinaryTemplate TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate;
typedef TR_RelocationRecordValidateMethodFromCPBinaryTemplate TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate;
typedef TR_RelocationRecordValidateMethodFromCPBinaryTemplate TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate;
typedef TR_RelocationRecordValidateMethodFromCPBinaryTemplate TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate;

struct TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _beholderID;

   // Value is (offset | ignoreRtResolve); offset must be even
   uint16_t _virtualCallOffsetAndIgnoreRtResolve;
   };

struct TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _beholderID;
   uint16_t _lookupID;
   uint16_t _cpIndex; // constrained to 16 bits by the class file format
   };

struct TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _lookupClassID;
   uint16_t _beholderID;
   UDATA _romMethodOffsetInSCC;
   };

struct TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _thisClassID;
   int32_t _cpIndexOrVftSlot;
   uint16_t _callerMethodID;
   uint16_t _useGetResolvedInterfaceMethod;
   };

struct TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _thisClassID;
   uint16_t _callerMethodID;
   uint16_t _cpIndex; // constrained to 16 bits by the class file format
   };

struct TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _definingClassID;
   uint16_t _thisClassID;
   uint16_t _callerMethodID;
   int32_t _vftSlot;
   };

struct TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _methodID;
   uint16_t _methodClassID;
   bool _skipFrames;
   };

struct TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _classID;
   bool _isInitialized;
   };

struct TR_RelocationRecordSymbolFromManagerBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _symbolID;
   uint16_t _symbolType;
   };

struct TR_RelocationRecordMethodCallAddressBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   UDATA _methodAddress;
   };

struct TR_RelocationRecordResolvedTrampolinesBinaryTemplate : public TR_RelocationRecordBinaryTemplate
   {
   uint16_t _symbolID;
   };

extern char* AOTcgDiagOn;

class TR_RelocationRecordGroup
   {
   public:
      TR_RelocationRecordGroup(TR_RelocationRecordBinaryTemplate *groupData) : _group(groupData) {};

      void setSize(TR_RelocationTarget *reloTarget, uintptr_t size);
      uintptr_t size(TR_RelocationTarget *reloTarget);

      TR_RelocationRecordBinaryTemplate *firstRecord(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      TR_RelocationRecordBinaryTemplate *pastLastRecord(TR_RelocationTarget *reloTarget);
      const uintptrj_t *wellKnownClassChainOffsets(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      int32_t applyRelocations(TR_RelocationRuntime *reloRuntime,
                               TR_RelocationTarget *reloTarget,
                               uint8_t *reloOrigin);
   private:
      int32_t handleRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_RelocationRecord *reloRecord, uint8_t *reloOrigin);

      TR_RelocationRecordBinaryTemplate *_group;
   };


// These *PrivateData structures are used to store useful data while processing a relocation record
// The preparePrivateData function, defined by all TR_RelocationRecords, creates these values which
//   are then used by the applyRelocation function.
struct TR_RelocationRecordHelperAddressPrivateData
   {
   uint32_t _helperID;
   uint8_t *_helper;
   };

struct TR_RelocationRecordInlinedAllocationPrivateData
   {
   bool _inlinedCodeIsOkay;
   };

struct TR_RelocationRecordInlinedMethodPrivateData
   {
   bool                  _failValidation;
   uint8_t  *            _destination;
   TR_OpaqueMethodBlock *_ramMethod;
   TR_OpaqueClassBlock * _receiverClass;
   };

struct TR_RelocationRecordProfiledInlinedMethodPrivateData
   {
   bool                 _failValidation;
   uint8_t  *           _destination;
   TR_OpaqueClassBlock *_inlinedCodeClass;
   uintptrj_t           _guardValue;
   bool                 _needUnloadAssumption;
   };

struct TR_RelocationRecordMethodTracingCheckPrivateData
   {
   uint8_t *_destinationAddress;
   };

struct TR_RelocationRecordWithOffsetPrivateData
   {
   uint8_t *_addressToPatch;
   };

struct TR_RelocationRecordArrayCopyPrivateData
   {
   uint8_t *_addressToPatch;
   };

struct TR_RelocationRecordPointerPrivateData
   {
   bool                 _activatePointer;
   TR_OpaqueClassBlock *_clazz;
   uintptrj_t           _pointer;
   bool                 _needUnloadAssumption;
   };

struct TR_RelocationRecordMethodCallPrivateData
   {
   int32_t _hwpInstructionType;
   int32_t _bcIndex;
   TR_OpaqueMethodBlock *_method;
   };

struct TR_RelocationRecordEmitClassPrivateData
   {
   int32_t _bcIndex;
   TR_OpaqueMethodBlock *_method;
   };

struct TR_RelocationRecordDebugCounterPrivateData
   {
   int32_t _bcIndex;
   int32_t _delta;
   int8_t  _fidelity;
   int32_t _staticDelta;
   TR_OpaqueMethodBlock *_method;
   const char *_name;
   };

struct TR_RelocationSymbolFromManagerPrivateData
   {
   uint16_t _symbolType;
   void *_symbol;
   };

struct TR_RelocationRecordResolvedTrampolinesPrivateData
   {
   TR_OpaqueMethodBlock *_method;
   };

union TR_RelocationRecordPrivateData
   {
   TR_RelocationRecordHelperAddressPrivateData helperAddress;
   TR_RelocationRecordInlinedAllocationPrivateData inlinedAllocation;
   TR_RelocationRecordInlinedMethodPrivateData inlinedMethod;
   TR_RelocationRecordProfiledInlinedMethodPrivateData profiledInlinedMethod;
   TR_RelocationRecordMethodTracingCheckPrivateData methodTracingCheck;
   TR_RelocationRecordWithOffsetPrivateData offset;
   TR_RelocationRecordArrayCopyPrivateData arraycopy;
   TR_RelocationRecordPointerPrivateData pointer;
   TR_RelocationRecordMethodCallPrivateData methodCall;
   TR_RelocationRecordEmitClassPrivateData emitClass;
   TR_RelocationRecordDebugCounterPrivateData debugCounter;
   TR_RelocationSymbolFromManagerPrivateData symbolFromManager;
   TR_RelocationRecordResolvedTrampolinesPrivateData resolvedTrampolines;
   };

enum TR_RelocationRecordAction
   {
   ignore,
   apply,
   failCompilation
   };

// TR_RelocationRecord is the base class for all relocation records.  It is used for all queries on relocation
// records as well as holding all the "wrapper" parts.  These classes are an interface to the *BinaryTemplate
// classes which are simply structs that can be used to directly access the binary representation of the relocation
// records stored in the cache (*BinaryTemplate structs are defined near the end of this file after the
// RelocationRecord* classes.  The RelocationRecord* classes permit virtual function calls access to the
// *BinaryTemplate classes and must access the binary structs via the _record field in the TR_RelocationRecord
// class.  Most consumers should directly manipulate the TR_RelocationRecord* classes since they offer
// the most flexibility.

class TR_RelocationRecord
   {
   public:
      TR_RelocationRecord() {}
      TR_RelocationRecord(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record)
         : _reloRuntime(reloRuntime), _record(record)
         {}

      void * operator new (size_t s, TR_RelocationRecord *p)   { return p; }

      virtual void print(TR_RelocationRuntime *reloRuntime);
      virtual char *name() { return "TR_RelocationRecord"; }

      virtual bool isValidationRecord() { return false; }


      static TR_RelocationRecord *create(TR_RelocationRecord *storage, TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_RelocationRecordBinaryTemplate *recordPointer);

      virtual void clean(TR_RelocationTarget *reloTarget);
      virtual int32_t bytesInHeaderAndPayload();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual int32_t applyRelocationAtAllOffsets(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *relocationOrigin);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation) {return -1;}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow) {return -1;}

      TR_RelocationRecordBinaryTemplate *nextBinaryRecord(TR_RelocationTarget *reloTarget);
      TR_RelocationRecordBinaryTemplate *binaryRecord();

      void setSize(TR_RelocationTarget *reloTarget, uint16_t size);
      uint16_t size(TR_RelocationTarget *reloTarget);

      void setType(TR_RelocationTarget *reloTarget, TR_RelocationRecordType type);
      TR_RelocationRecordType type(TR_RelocationTarget *reloTarget);

      void setWideOffsets(TR_RelocationTarget *reloTarget);
      bool wideOffsets(TR_RelocationTarget *reloTarget);

      void setEipRelative(TR_RelocationTarget *reloTarget);
      bool eipRelative(TR_RelocationTarget *reloTarget);

      void setFlag(TR_RelocationTarget *reloTarget, uint8_t flag);
      uint8_t flags(TR_RelocationTarget *reloTarget);

      void setReloFlags(TR_RelocationTarget *reloTarget, uint8_t reloFlags);
      uint8_t reloFlags(TR_RelocationTarget *reloTarget);

      TR_RelocationRuntime *_reloRuntime;

      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);

      static uint32_t getSizeOfAOTRelocationHeader(TR_ExternalRelocationTargetKind k)
         {
         return _relocationRecordHeaderSizeTable[k];
         }

   protected:
      OMR::RuntimeAssumption** getMetadataAssumptionList(J9JITExceptionTable *exceptionTable)
         {
         return (OMR::RuntimeAssumption**)(&exceptionTable->runtimeAssumptionList);
         }
      uint8_t *computeHelperAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *baseLocation);

      TR_RelocationRecordPrivateData *privateData()
         {
         return &_privateData;
         }

      TR_RelocationRecordBinaryTemplate *_record;

      TR_RelocationRecordPrivateData _privateData;

      static uint32_t _relocationRecordHeaderSizeTable[TR_NumExternalRelocationKinds];
   };

// No class that derives from TR_RelocationRecord should define any state: all state variables should be declared
//  in TR_RelocationRecord or the constructor/decode() mechanisms will not work properly


// Relocation record classes for "real" relocation record types

class TR_RelocationRecordBodyInfo : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordBodyInfo() {}
      TR_RelocationRecordBodyInfo(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name();

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordWithOffset : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordWithOffset() {}
      TR_RelocationRecordWithOffset(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setOffset(TR_RelocationTarget *reloTarget, uintptr_t offset);
      uintptr_t offset(TR_RelocationTarget *reloTarget);

      virtual int32_t bytesInHeaderAndPayload();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordGlobalValue : public TR_RelocationRecordWithOffset
   {
   public:
      TR_RelocationRecordGlobalValue() {}
      TR_RelocationRecordGlobalValue(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithOffset(reloRuntime, record) {}
      virtual char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordBodyInfoLoad : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordBodyInfoLoad() {}
      TR_RelocationRecordBodyInfoLoad(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordArrayCopyHelper : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordArrayCopyHelper() {}
      TR_RelocationRecordArrayCopyHelper(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordArrayCopyToc : public TR_RelocationRecordArrayCopyHelper
   {
   public:
      TR_RelocationRecordArrayCopyToc() {}
      TR_RelocationRecordArrayCopyToc(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordArrayCopyHelper(reloRuntime, record) {}
      virtual char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordRamSequence : public TR_RelocationRecordWithOffset
   {
   public:
      TR_RelocationRecordRamSequence() {}
      TR_RelocationRecordRamSequence(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithOffset(reloRuntime, record) {}
      virtual char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordWithInlinedSiteIndex : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordWithInlinedSiteIndex() {}
      TR_RelocationRecordWithInlinedSiteIndex(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setInlinedSiteIndex(TR_RelocationTarget *reloTarget, uintptr_t inlinedSiteIndex);
      uintptr_t inlinedSiteIndex(TR_RelocationTarget *reloTarget);

      virtual int32_t bytesInHeaderAndPayload();

      // This relocation record is not used directly, only via subclasses
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
         {
         return 1;
         }
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
         {
         return 1;
         }

      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);
   protected:
      virtual TR_OpaqueMethodBlock *getInlinedSiteCallerMethod(TR_RelocationRuntime *reloRuntime);
      virtual TR_OpaqueMethodBlock *getInlinedSiteMethod(TR_RelocationRuntime *reloRuntime);
      virtual TR_OpaqueMethodBlock *getInlinedSiteMethod(TR_RelocationRuntime *reloRuntime, uintptrj_t siteIndex);
   };



class TR_RelocationRecordConstantPool : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordConstantPool() {}
      TR_RelocationRecordConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setConstantPool(TR_RelocationTarget *reloTarget, uintptrj_t constantPool);
      uintptrj_t constantPool(TR_RelocationTarget *reloTarget);

      virtual int32_t bytesInHeaderAndPayload();

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);


   protected:
      uintptrj_t currentConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptrj_t oldValue);
      uintptrj_t findConstantPool(TR_RelocationTarget *reloTarget, uintptrj_t oldValue, TR_OpaqueMethodBlock *ramMethod);
      uintptrj_t computeNewConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptrj_t oldValue);
   };

class TR_RelocationRecordConstantPoolWithIndex : public TR_RelocationRecordConstantPool
   {
   public:
      TR_RelocationRecordConstantPoolWithIndex() {}
      TR_RelocationRecordConstantPoolWithIndex(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPool(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setCpIndex(TR_RelocationTarget *reloTarget, uintptrj_t cpIndex);
      uintptrj_t cpIndex(TR_RelocationTarget *reloTarget);

      TR_OpaqueMethodBlock *getVirtualMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex);
      TR_OpaqueMethodBlock *getStaticMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex);
      TR_OpaqueMethodBlock *getSpecialMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex);
      TR_OpaqueMethodBlock *getInterfaceMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod);
      TR_OpaqueMethodBlock *getAbstractMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod);

      virtual int32_t bytesInHeaderAndPayload();
   };

class TR_RelocationRecordHelperAddress : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordHelperAddress() {}
      TR_RelocationRecordHelperAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setHelperID(TR_RelocationTarget *reloTarget, uint32_t helperID);
      uint32_t helperID(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual int32_t bytesInHeaderAndPayload();
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

   };

class TR_RelocationRecordAbsoluteHelperAddress : public TR_RelocationRecordHelperAddress
   {
   public:
      TR_RelocationRecordAbsoluteHelperAddress() {}
      TR_RelocationRecordAbsoluteHelperAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordHelperAddress(reloRuntime, record) {}
      virtual char *name();
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordDataAddress : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordDataAddress() {}
      TR_RelocationRecordDataAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual int32_t bytesInHeaderAndPayload();

      void setOffset(TR_RelocationTarget *reloTarget, uintptr_t offset);
      uintptr_t offset(TR_RelocationTarget *reloTarget);

      uint8_t *findDataAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

   };

class TR_RelocationRecordMethodAddress : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordMethodAddress() {}
      TR_RelocationRecordMethodAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name();
      uint8_t *currentMethodAddress(TR_RelocationRuntime *reloRuntime, uint8_t *oldMethodAddress);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordClassAddress : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordClassAddress() {}
      TR_RelocationRecordClassAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual char *name();

      TR_OpaqueClassBlock *computeNewClassAddress(TR_RelocationRuntime *reloRuntime, uintptrj_t newConstantPool, uintptrj_t inlinedSiteIndex, uintptrj_t cpIndex);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordDirectJNICall : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
   TR_RelocationRecordDirectJNICall() {}
   TR_RelocationRecordDirectJNICall(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
   virtual char *name();
   virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

   private:
   virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex);
   };

class TR_RelocationRecordDirectJNISpecialMethodCall : public TR_RelocationRecordDirectJNICall
   {
   public:
   TR_RelocationRecordDirectJNISpecialMethodCall() {}
   TR_RelocationRecordDirectJNISpecialMethodCall(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordDirectJNICall(reloRuntime, record) {}

   private:
   virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex);
   };


class TR_RelocationRecordDirectJNIVirtualMethodCall : public TR_RelocationRecordDirectJNICall
   {
   public:
   TR_RelocationRecordDirectJNIVirtualMethodCall() {}
   TR_RelocationRecordDirectJNIVirtualMethodCall(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordDirectJNICall(reloRuntime, record) {}

   private:
   virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex);
   };

class TR_RelocationRecordDirectJNIStaticMethodCall : public TR_RelocationRecordDirectJNICall
   {
   public:
   TR_RelocationRecordDirectJNIStaticMethodCall() {}
   TR_RelocationRecordDirectJNIStaticMethodCall(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordDirectJNICall(reloRuntime, record) {}

   private:
   virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex);
   };

class TR_RelocationRecordRamMethodConst : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
   TR_RelocationRecordRamMethodConst() {};
   TR_RelocationRecordRamMethodConst(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
   virtual char *name()=0;
   virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

   private:
   virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex)=0;
   };


class TR_RelocationRecordStaticRamMethodConst : public TR_RelocationRecordRamMethodConst
   {
   public:
   TR_RelocationRecordStaticRamMethodConst() {};
   TR_RelocationRecordStaticRamMethodConst(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordRamMethodConst(reloRuntime, record) {}
   virtual char *name()  { return "TR_RelocationRecordStaticRamMethodConst"; };

   private:
   virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex)
      {
      return getStaticMethodFromCP(reloRuntime, void_cp, cpindex);
      };
   };

class TR_RelocationRecordVirtualRamMethodConst : public TR_RelocationRecordRamMethodConst
   {
   public:
   TR_RelocationRecordVirtualRamMethodConst() {};
   TR_RelocationRecordVirtualRamMethodConst(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordRamMethodConst(reloRuntime, record) {}
   virtual char *name()  { return "TR_RelocationRecordVirtualRamMethodConst"; };

   private:
   virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex)
      {
      return getVirtualMethodFromCP(reloRuntime, void_cp, cpindex);
      };
   };

class TR_RelocationRecordSpecialRamMethodConst : public TR_RelocationRecordRamMethodConst
   {
   public:
   TR_RelocationRecordSpecialRamMethodConst() {};
   TR_RelocationRecordSpecialRamMethodConst(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordRamMethodConst(reloRuntime, record) {}
   virtual char *name()  { return "TR_RelocationRecordSpecialRamMethodConst"; };

   private:
   virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex)
      {
      return getSpecialMethodFromCP(reloRuntime, void_cp, cpindex);
      };
   };

class TR_RelocationRecordMethodObject : public TR_RelocationRecordConstantPool
   {
   public:
      TR_RelocationRecordMethodObject() {}
      TR_RelocationRecordMethodObject(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPool(reloRuntime, record) {}
      virtual char *name();

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordThunks : public TR_RelocationRecordConstantPool
   {
   public:
      TR_RelocationRecordThunks() {}
      TR_RelocationRecordThunks(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPool(reloRuntime, record) {}
      virtual char *name();
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

   protected:
      int32_t relocateAndRegisterThunk(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptrj_t cp, uintptr_t cpIndex, uint8_t *reloLocation);

      /** Relocate the J2I virtual thunk pointer, if applicable */
      virtual void relocateJ2IVirtualThunkPointer(TR_RelocationTarget *reloTarget, uint8_t *reloLocation, void *thunk)
         {
         // nothing to relocate
         }
   };

class TR_RelocationRecordJ2IVirtualThunkPointer : public TR_RelocationRecordThunks
   {
   public:
      TR_RelocationRecordJ2IVirtualThunkPointer() {}

      TR_RelocationRecordJ2IVirtualThunkPointer(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record)
         : TR_RelocationRecordThunks(reloRuntime, record) {}

      virtual char *name();
      virtual int32_t bytesInHeaderAndPayload();

   protected:
      virtual void relocateJ2IVirtualThunkPointer(TR_RelocationTarget *reloTarget, uint8_t *reloLocation, void *thunk);
      uintptrj_t offsetToJ2IVirtualThunkPointer(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordTrampolines : public TR_RelocationRecordConstantPool
   {
   public:
      TR_RelocationRecordTrampolines() {}
      TR_RelocationRecordTrampolines(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPool(reloRuntime, record) {}
      virtual char *name();
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordPicTrampolines : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordPicTrampolines() {}
      TR_RelocationRecordPicTrampolines(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual int32_t bytesInHeaderAndPayload();

      void setNumTrampolines(TR_RelocationTarget *reloTarget, int numTrampolines);
      uint8_t numTrampolines(TR_RelocationTarget *reloTarget);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordInlinedAllocation : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordInlinedAllocation() {}
      TR_RelocationRecordInlinedAllocation(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual int32_t bytesInHeaderAndPayload();

      void setBranchOffset(TR_RelocationTarget *reloTarget, uintptr_t branchOffset);
      uintptr_t branchOffset(TR_RelocationTarget *reloTarget);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   protected:

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual bool verifyClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *clazz);
   };

class TR_RelocationRecordVerifyRefArrayForAlloc : public TR_RelocationRecordInlinedAllocation
   {
   public:
      TR_RelocationRecordVerifyRefArrayForAlloc() {}
      TR_RelocationRecordVerifyRefArrayForAlloc(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedAllocation(reloRuntime, record) {}
      virtual char *name();
   protected:
      virtual bool verifyClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *clazz);
   };

class TR_RelocationRecordVerifyClassObjectForAlloc : public TR_RelocationRecordInlinedAllocation
   {
   public:
      TR_RelocationRecordVerifyClassObjectForAlloc() {}
      TR_RelocationRecordVerifyClassObjectForAlloc(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedAllocation(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual int32_t bytesInHeaderAndPayload();

      void setAllocationSize(TR_RelocationTarget *reloTarget, uintptr_t allocationSize);
      uintptr_t allocationSize(TR_RelocationTarget *reloTarget);

   protected:
      virtual bool verifyClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *clazz);
   };


class TR_RelocationRecordInlinedMethod : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordInlinedMethod() {}
      TR_RelocationRecordInlinedMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setRomClassOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t romClassOffsetInSharedCache);
      uintptrj_t romClassOffsetInSharedCache(TR_RelocationTarget *reloTarget);

      virtual int32_t bytesInHeaderAndPayload();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime)
         {
         // inlined method validations cannot be skipped as they control whether inlined method's relocations are needed
         return TR_RelocationRecordAction::apply;
         }

   protected:
      virtual void fixInlinedSiteInfo(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueMethodBlock *inlinedMethod);
      virtual bool needsReceiverClassFromID() { return false; }
      bool inlinedSiteCanBeActivated(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, J9Method *currentMethod);

   private:
      virtual bool inlinedSiteValid(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueMethodBlock **theMethod);

      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod) { return NULL; }

      virtual void invalidateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation) {}
      virtual void activateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation) {}

      virtual void updateFailedStats(TR_AOTStats *aotStats) { }
      virtual void updateSucceededStats(TR_AOTStats *aotStats) { }

      virtual void createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation) { }
   };

class TR_RelocationRecordNopGuard : public TR_RelocationRecordInlinedMethod
   {
   public:
      TR_RelocationRecordNopGuard() {}
      TR_RelocationRecordNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedMethod(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setDestinationAddress(TR_RelocationTarget *reloTarget, uintptrj_t destinationAddress);
      uintptrj_t destinationAddress(TR_RelocationTarget *reloTarget);

      virtual int32_t bytesInHeaderAndPayload();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
   private:
      virtual void invalidateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual void activateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual void createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation) { }
   };

class TR_RelocationRecordInlinedStaticMethodWithNopGuard : public TR_RelocationRecordNopGuard
   {
   public:
      TR_RelocationRecordInlinedStaticMethodWithNopGuard() {}
      TR_RelocationRecordInlinedStaticMethodWithNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordNopGuard(reloRuntime, record) {}
      virtual char *name();

   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
   };


class TR_RelocationRecordInlinedSpecialMethodWithNopGuard : public TR_RelocationRecordNopGuard
   {
   public:
      TR_RelocationRecordInlinedSpecialMethodWithNopGuard() {}
      TR_RelocationRecordInlinedSpecialMethodWithNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordNopGuard(reloRuntime, record) {}
      virtual char *name();

   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
   };

class TR_RelocationRecordInlinedVirtualMethodWithNopGuard : public TR_RelocationRecordNopGuard
   {
   public:
      TR_RelocationRecordInlinedVirtualMethodWithNopGuard() {}
      TR_RelocationRecordInlinedVirtualMethodWithNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordNopGuard(reloRuntime, record) {}
      virtual char *name();
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
      virtual void createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation);
   };

class TR_RelocationRecordInlinedVirtualMethod: public TR_RelocationRecordInlinedMethod
   {
   typedef TR_RelocationRecordInlinedMethod Base;
   public:
      TR_RelocationRecordInlinedVirtualMethod() {}
      TR_RelocationRecordInlinedVirtualMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedMethod(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex);
   };

class TR_RelocationRecordInlinedInterfaceMethodWithNopGuard : public TR_RelocationRecordNopGuard
   {
   public:
      TR_RelocationRecordInlinedInterfaceMethodWithNopGuard() {}
      TR_RelocationRecordInlinedInterfaceMethodWithNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordNopGuard(reloRuntime, record) {}
      virtual char *name();
   protected:
      virtual bool needsReceiverClassFromID() { return true; }
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
      virtual void createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation);
   };

class TR_RelocationRecordInlinedInterfaceMethod: public TR_RelocationRecordInlinedMethod
   {
   public:
      TR_RelocationRecordInlinedInterfaceMethod() {}
      TR_RelocationRecordInlinedInterfaceMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedMethod(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
   };

class TR_RelocationRecordInlinedAbstractMethodWithNopGuard : public TR_RelocationRecordNopGuard
   {
   public:
      TR_RelocationRecordInlinedAbstractMethodWithNopGuard() {}
      TR_RelocationRecordInlinedAbstractMethodWithNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordNopGuard(reloRuntime, record) {}
      virtual char *name();
   protected:
      virtual bool needsReceiverClassFromID() { return true; }
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
      virtual void createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation);
   };

class TR_RelocationRecordProfiledInlinedMethod : public TR_RelocationRecordInlinedMethod
   {
   public:
      TR_RelocationRecordProfiledInlinedMethod() {}
      TR_RelocationRecordProfiledInlinedMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedMethod(reloRuntime, record) {}
      virtual char *name();

      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual int32_t bytesInHeaderAndPayload();

      void setClassChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t classChainOffsetInSharedCache);
      uintptrj_t classChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget);

      void setClassChainForInlinedMethod(TR_RelocationTarget *reloTarget, uintptrj_t classChainForInlinedMethod);
      uintptrj_t classChainForInlinedMethod(TR_RelocationTarget *reloTarget);

      void setMethodIndex(TR_RelocationTarget *reloTarget, uintptrj_t methodIndex);
      uintptrj_t methodIndex(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      TR_OpaqueMethodBlock *getInlinedMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *inlinedCodeClass);

   private:

      virtual void setupInlinedMethodData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual bool checkInlinedClassValidity(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *inlinedCodeClass);
      virtual void activateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation) {}
      virtual void invalidateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation) {}
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
   };

class TR_RelocationRecordProfiledGuard : public TR_RelocationRecordProfiledInlinedMethod
   {
   public:
   TR_RelocationRecordProfiledGuard() {}
   TR_RelocationRecordProfiledGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordProfiledInlinedMethod(reloRuntime, record) {}
   private:
      virtual void activateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual void invalidateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordProfiledClassGuard : public TR_RelocationRecordProfiledGuard
   {
   public:
      TR_RelocationRecordProfiledClassGuard() {}
      TR_RelocationRecordProfiledClassGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordProfiledGuard(reloRuntime, record) {}
      virtual char *name();

   private:
      virtual bool checkInlinedClassValidity(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *inlinedCodeClass);
      virtual void setupInlinedMethodData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
   };

class TR_RelocationRecordProfiledMethodGuard : public TR_RelocationRecordProfiledGuard
   {
   public:
      TR_RelocationRecordProfiledMethodGuard() {}
      TR_RelocationRecordProfiledMethodGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordProfiledGuard(reloRuntime, record) {}
      virtual char *name();

   private:
      virtual bool checkInlinedClassValidity(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *inlinedCodeClass);
      virtual void setupInlinedMethodData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
   };


class TR_RelocationRecordMethodTracingCheck : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordMethodTracingCheck() {}
      TR_RelocationRecordMethodTracingCheck(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual int32_t bytesInHeaderAndPayload();

      void setDestinationAddress(TR_RelocationTarget *reloTarget, uintptrj_t destinationAddress);
      uintptrj_t destinationAddress(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordMethodEnterCheck : public TR_RelocationRecordMethodTracingCheck
   {
   public:
      TR_RelocationRecordMethodEnterCheck() {}
      TR_RelocationRecordMethodEnterCheck(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordMethodTracingCheck(reloRuntime, record) {}
      virtual char *name();
      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);
   };

class TR_RelocationRecordMethodExitCheck : public TR_RelocationRecordMethodTracingCheck
   {
   public:
      TR_RelocationRecordMethodExitCheck() {}
      TR_RelocationRecordMethodExitCheck(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordMethodTracingCheck(reloRuntime, record) {}
      virtual char *name();
      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);
   };

class TR_RelocationRecordRamMethod : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordRamMethod() {}
      TR_RelocationRecordRamMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name();

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordValidateClass : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordValidateClass() {}
      TR_RelocationRecordValidateClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual int32_t bytesInHeaderAndPayload();

      void setClassChainOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t classChainOffsetInSharedCache);
      uintptrj_t classChainOffsetInSharedCache(TR_RelocationTarget *reloTarget);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

   protected:
      virtual TR_OpaqueClassBlock *getClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp);
      virtual bool validateClass(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *clazz, void *classChainOrRomClass);
      virtual int32_t failureCode();
   };

class TR_RelocationRecordValidateInstanceField : public TR_RelocationRecordValidateClass
   {
   public:
      TR_RelocationRecordValidateInstanceField() {}
      TR_RelocationRecordValidateInstanceField(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateClass(reloRuntime, record) {}
      virtual char *name();

   protected:
      virtual TR_OpaqueClassBlock *getClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp);
      virtual int32_t failureCode();
   };

class TR_RelocationRecordValidateStaticField : public TR_RelocationRecordValidateInstanceField
   {
   public:
      TR_RelocationRecordValidateStaticField() {}
      TR_RelocationRecordValidateStaticField(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateInstanceField(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual int32_t bytesInHeaderAndPayload();

      void setRomClassOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t romClassOffsetInSharedCache);
      uintptrj_t romClassOffsetInSharedCache(TR_RelocationTarget *reloTarget);

   protected:
      virtual TR_OpaqueClassBlock *getClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp);
      virtual bool validateClass(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *clazz, void *classChainOrRomClass);
   };

class TR_RelocationRecordValidateArbitraryClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateArbitraryClass() {}
      TR_RelocationRecordValidateArbitraryClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setClassChainIdentifyingLoaderOffset(TR_RelocationTarget *reloTarget, uintptrj_t classChainOffset);
      uintptrj_t classChainIdentifyingLoaderOffset(TR_RelocationTarget *reloTarget);

      void setClassChainOffsetForClassBeingValidated(TR_RelocationTarget *reloTarget, uintptrj_t classChainOffset);
      uintptrj_t classChainOffsetForClassBeingValidated(TR_RelocationTarget *reloTarget);

      virtual int32_t bytesInHeaderAndPayload();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };


/* SYMBOL VALIDATION MANAGER */
class TR_RelocationRecordValidateClassByName : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassByName() {}
      TR_RelocationRecordValidateClassByName(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateClassByName"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateClassByNameBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateProfiledClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateProfiledClass() {}
      TR_RelocationRecordValidateProfiledClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateProfiledClass"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateProfiledClassBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateClassFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassFromCP() {}
      TR_RelocationRecordValidateClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateClassFromCP"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateClassFromCPBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateDefiningClassFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateDefiningClassFromCP() {}
      TR_RelocationRecordValidateDefiningClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateDefiningClassFromCP"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateDefiningClassFromCPBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateStaticClassFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateStaticClassFromCP() {}
      TR_RelocationRecordValidateStaticClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateStaticClassFromCP"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateStaticClassFromCPBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateArrayClassFromComponentClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateArrayClassFromComponentClass() {}
      TR_RelocationRecordValidateArrayClassFromComponentClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateArrayClassFromComponentClass"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateArrayFromCompBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateSuperClassFromClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateSuperClassFromClass() {}
      TR_RelocationRecordValidateSuperClassFromClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateSuperClassFromClass"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateSuperClassFromClassBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateClassInstanceOfClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassInstanceOfClass() {}
      TR_RelocationRecordValidateClassInstanceOfClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateClassInstanceOfClass"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateClassInstanceOfClassBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateSystemClassByName : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateSystemClassByName() {}
      TR_RelocationRecordValidateSystemClassByName(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateSystemClassByName"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateSystemClassByNameBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateClassFromITableIndexCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassFromITableIndexCP() {}
      TR_RelocationRecordValidateClassFromITableIndexCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateClassFromITableIndexCP"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateClassFromITableIndexCPBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic() {}
      TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateDeclaringClassFromFieldOrStaticBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateConcreteSubClassFromClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateConcreteSubClassFromClass() {}
      TR_RelocationRecordValidateConcreteSubClassFromClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateConcreteSubClassFromClass"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateConcreteSubFromClassBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateClassChain : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassChain() {}
      TR_RelocationRecordValidateClassChain(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateClassChain"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateClassChainBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateMethodFromClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromClass() {}
      TR_RelocationRecordValidateMethodFromClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateMethodFromClass"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateMethodFromClassBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateStaticMethodFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateStaticMethodFromCP() {}
      TR_RelocationRecordValidateStaticMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateStaticMethodFromCP"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateStaticMethodFromCPBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateSpecialMethodFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateSpecialMethodFromCP() {}
      TR_RelocationRecordValidateSpecialMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateSpecialMethodFromCP"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateSpecialMethodFromCPBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateVirtualMethodFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateVirtualMethodFromCP() {}
      TR_RelocationRecordValidateVirtualMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateVirtualMethodFromCP"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateVirtualMethodFromCPBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateVirtualMethodFromOffset : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateVirtualMethodFromOffset() {}
      TR_RelocationRecordValidateVirtualMethodFromOffset(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateVirtualMethodFromOffset"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateVirtualMethodFromOffsetBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateInterfaceMethodFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateInterfaceMethodFromCP() {}
      TR_RelocationRecordValidateInterfaceMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateInterfaceMethodFromCP"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateInterfaceMethodFromCPBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateImproperInterfaceMethodFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateImproperInterfaceMethodFromCP() {}
      TR_RelocationRecordValidateImproperInterfaceMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateImproperInterfaceMethodFromCP"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateImproperInterfaceMethodFromCPBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateMethodFromClassAndSig : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromClassAndSig() {}
      TR_RelocationRecordValidateMethodFromClassAndSig(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateMethodFromClassAndSig"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateMethodFromClassAndSigBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateStackWalkerMaySkipFrames : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateStackWalkerMaySkipFrames() {}
      TR_RelocationRecordValidateStackWalkerMaySkipFrames(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateStackWalkerMaySkipFrames"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateStackWalkerMaySkipFramesBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateClassInfoIsInitialized : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassInfoIsInitialized() {}
      TR_RelocationRecordValidateClassInfoIsInitialized(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateClassInfoIsInitialized"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateClassInfoIsInitializedBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateMethodFromSingleImpl : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromSingleImpl() {}
      TR_RelocationRecordValidateMethodFromSingleImpl(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateMethodFromSingleImpl"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateMethodFromSingleImplBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateMethodFromSingleInterfaceImpl : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromSingleInterfaceImpl() {}
      TR_RelocationRecordValidateMethodFromSingleInterfaceImpl(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateMethodFromSingleInterfaceImpl"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateMethodFromSingleInterfaceImplBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateMethodFromSingleAbstractImpl : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromSingleAbstractImpl() {}
      TR_RelocationRecordValidateMethodFromSingleAbstractImpl(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual char *name() { return "TR_RelocationRecordValidateMethodFromSingleAbstractImpl"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordValidateMethodFromSingleAbstractImplBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordSymbolFromManager : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordSymbolFromManager() {}
      TR_RelocationRecordSymbolFromManager(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name() { return "TR_RelocationRecordSymbolFromManager"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordSymbolFromManagerBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual void storePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual void activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      bool needsUnloadAssumptions(TR::SymbolType symbolType);
      bool needsRedefinitionAssumption(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation, TR_OpaqueClassBlock *clazz, TR::SymbolType symbolType);
   };

class TR_RelocationRecordDiscontiguousSymbolFromManager : public TR_RelocationRecordSymbolFromManager
   {
   public:
      TR_RelocationRecordDiscontiguousSymbolFromManager() {}
      TR_RelocationRecordDiscontiguousSymbolFromManager(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordSymbolFromManager(reloRuntime, record) {}
      virtual char *name() { return "TR_RelocationRecordDiscontiguousSymbolFromManager"; }
      virtual void storePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordResolvedTrampolines : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordResolvedTrampolines() {}
      TR_RelocationRecordResolvedTrampolines(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual char *name() { return "TR_RelocationRecordResolvedTrampolines"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordResolvedTrampolinesBinaryTemplate); }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };
/* SYMBOL VALIDATION MANAGER */

class TR_RelocationRecordHCR : public TR_RelocationRecordWithOffset
   {
   public:
      TR_RelocationRecordHCR() {}
      TR_RelocationRecordHCR(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithOffset(reloRuntime, record) {}
      virtual char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) { }

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);
   };

class TR_RelocationRecordPointer : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordPointer() {}
      TR_RelocationRecordPointer(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}

      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual int32_t bytesInHeaderAndPayload();

      void setClassChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptrj_t classChainOffsetInSharedCache);
      uintptrj_t classChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget);

      void setClassChainForInlinedMethod(TR_RelocationTarget *reloTarget, uintptrj_t classChainOffsetInSharedCache);
      uintptrj_t classChainForInlinedMethod(TR_RelocationTarget *reloTarget);

      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   protected:
      virtual uintptrj_t computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer) { return 0;}
      virtual void activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      void registerHCRAssumption(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation);
   };

class TR_RelocationRecordClassPointer : public TR_RelocationRecordPointer
   {
   public:
      TR_RelocationRecordClassPointer() {}
      TR_RelocationRecordClassPointer(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordPointer(reloRuntime, record) {}
      virtual char *name();

   protected:
      virtual uintptrj_t computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer);
      virtual void activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

// Relocation record data to identify the class is as for TR_ClassPointer, but
// patching in the value is done as though for TR_ClassAddress.
class TR_RelocationRecordArbitraryClassAddress : public TR_RelocationRecordClassPointer
   {
   public:
      TR_RelocationRecordArbitraryClassAddress() {}
      TR_RelocationRecordArbitraryClassAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordClassPointer(reloRuntime, record) {}
      virtual char *name();

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

   protected:
      virtual void activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

   private:
      void assertBootstrapLoader(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *clazz);
   };

class TR_RelocationRecordMethodPointer : public TR_RelocationRecordPointer
   {
   public:
      TR_RelocationRecordMethodPointer() {}
      TR_RelocationRecordMethodPointer(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordPointer(reloRuntime, record) {}
      virtual char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setVTableSlot(TR_RelocationTarget *reloTarget, uintptrj_t vTableSlot);
      uintptrj_t vTableSlot(TR_RelocationTarget *reloTarget);

      virtual int32_t bytesInHeaderAndPayload();

   protected:
      virtual uintptrj_t computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer);
      virtual void activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordEmitClass : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordEmitClass() {}
      TR_RelocationRecordEmitClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}

      virtual int32_t bytesInHeaderAndPayload();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordDebugCounter : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordDebugCounter() {}
      TR_RelocationRecordDebugCounter(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}
      virtual char *name();

      virtual int32_t bytesInHeaderAndPayload();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

   private:
      TR::DebugCounterBase *findOrCreateCounter(TR_RelocationRuntime *reloRuntime);
   };

class TR_RelocationRecordClassUnloadAssumption : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordClassUnloadAssumption() {}
      TR_RelocationRecordClassUnloadAssumption(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}

      virtual char *name();

      virtual int32_t bytesInHeaderAndPayload();

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordMethodCallAddress : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordMethodCallAddress() {}
      TR_RelocationRecordMethodCallAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}

      virtual char *name() { return "TR_MethodCallAddress"; }
      virtual int32_t bytesInHeaderAndPayload() { return sizeof(TR_RelocationRecordMethodCallAddressBinaryTemplate); }
      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      uint8_t* address(TR_RelocationTarget *reloTarget);
      void setAddress(TR_RelocationTarget *reloTarget, uint8_t* callTargetAddress);

   private:
      uint8_t *computeTargetMethodAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *baseLocation);
   };

#endif   // RELOCATION_RECORD_INCL

