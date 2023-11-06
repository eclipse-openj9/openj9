/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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
#include "runtime/RelocationRuntime.hpp"

class TR_RelocationRecord;
class TR_RelocationTarget;
struct TR_RelocationRecordBinaryTemplate;
typedef TR_ExternalRelocationTargetKind TR_RelocationRecordType;
namespace TR { class AheadOfTimeCompile; }
class AOTCacheClassChainRecord;

class TR_RelocationRecordGroup
   {
   public:
      TR_RelocationRecordGroup(TR_RelocationRecordBinaryTemplate *groupData) : _group(groupData) {}

      void setSize(TR_RelocationTarget *reloTarget, uintptr_t size);
      uintptr_t size(TR_RelocationTarget *reloTarget);

      TR_RelocationRecordBinaryTemplate *firstRecord(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      TR_RelocationRecordBinaryTemplate *pastLastRecord(TR_RelocationTarget *reloTarget);
      const uintptr_t *wellKnownClassChainOffsets(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      TR_RelocationErrorCode applyRelocations(TR_RelocationRuntime *reloRuntime,
                                              TR_RelocationTarget *reloTarget,
                                              uint8_t *reloOrigin);
   private:
      TR_RelocationErrorCode handleRelocation(TR_RelocationRuntime *reloRuntime,
                                              TR_RelocationTarget *reloTarget,
                                              TR_RelocationRecord *reloRecord,
                                              uint8_t *reloOrigin);

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
   uintptr_t           _guardValue;
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
   uintptr_t           _pointer;
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

struct TR_RelocationRecordBlockFrequencyPrivateData
   {
   uint8_t *_addressToPatch;
   };

struct TR_RelocationRecordRecompQueuedFlagPrivateData
   {
   uint8_t *_addressToPatch;
   };

struct TR_RelocationRecordBreakpointGuardPrivateData
   {
   bool _compensateGuard;
   TR_OpaqueMethodBlock *_method;
   uint8_t *_destinationAddress;
   };

struct TR_RelocationRecordMethodEnterExitHookAddressPrivateData
   {
   bool _isEnterHookAddr;
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
   TR_RelocationRecordBlockFrequencyPrivateData blockFrequency;
   TR_RelocationRecordRecompQueuedFlagPrivateData recompQueuedFlag;
   TR_RelocationRecordBreakpointGuardPrivateData breakpointGuard;
   TR_RelocationRecordMethodEnterExitHookAddressPrivateData hookAddress;
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
      void operator delete (void *, TR_RelocationRecord *p) {}

      virtual void print(TR_RelocationRuntime *reloRuntime);
      virtual const char *name() { return "TR_RelocationRecord"; }

      virtual bool isValidationRecord() { return false; }


      static TR_RelocationRecord *create(TR_RelocationRecord *storage, TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_RelocationRecordBinaryTemplate *record);
      static TR_RelocationRecord *create(TR_RelocationRecord *storage, TR_RelocationRuntime *reloRuntime, uint8_t reloType, TR_RelocationRecordBinaryTemplate *record);

      virtual void clean(TR_RelocationTarget *reloTarget);
      int32_t bytesInHeader(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocationAtAllOffsets(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *relocationOrigin);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation) {return TR_RelocationErrorCode::invalidRelocation;}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow) {return TR_RelocationErrorCode::invalidRelocation;}

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

   private:
      void updateFlags(TR_RelocationTarget *reloTarget, uint16_t flagsToSet);
   };

// No class that derives from TR_RelocationRecord should define any state: all state variables should be declared
//  in TR_RelocationRecord or the constructor/decode() mechanisms will not work properly


// Relocation record classes for "real" relocation record types

class TR_RelocationRecordBodyInfo : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordBodyInfo() {}
      TR_RelocationRecordBodyInfo(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name();

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordWithOffset : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordWithOffset() {}
      TR_RelocationRecordWithOffset(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setOffset(TR_RelocationTarget *reloTarget, uintptr_t offset);
      uintptr_t offset(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordBlockFrequency : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordBlockFrequency() {}
      TR_RelocationRecordBlockFrequency(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}

      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setFrequencyOffset(TR_RelocationTarget *reloTarget, uintptr_t offset);
      uintptr_t frequencyOffset(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordRecompQueuedFlag : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordRecompQueuedFlag() {}
      TR_RelocationRecordRecompQueuedFlag(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}

      virtual const char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordGlobalValue : public TR_RelocationRecordWithOffset
   {
   public:
      TR_RelocationRecordGlobalValue() {}
      TR_RelocationRecordGlobalValue(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithOffset(reloRuntime, record) {}
      virtual const char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordBodyInfoLoad : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordBodyInfoLoad() {}
      TR_RelocationRecordBodyInfoLoad(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordArrayCopyHelper : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordArrayCopyHelper() {}
      TR_RelocationRecordArrayCopyHelper(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordArrayCopyToc : public TR_RelocationRecordArrayCopyHelper
   {
   public:
      TR_RelocationRecordArrayCopyToc() {}
      TR_RelocationRecordArrayCopyToc(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordArrayCopyHelper(reloRuntime, record) {}
      virtual const char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordRamSequence : public TR_RelocationRecordWithOffset
   {
   public:
      TR_RelocationRecordRamSequence() {}
      TR_RelocationRecordRamSequence(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithOffset(reloRuntime, record) {}
      virtual const char *name();

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

      // This relocation record is not used directly, only via subclasses
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation)
         {
         TR_ASSERT_FATAL(false, "Should not be called directly\n");
         return TR_RelocationErrorCode::invalidRelocation;
         }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow)
         {
         TR_ASSERT_FATAL(false, "Should not be called directly\n");
         return TR_RelocationErrorCode::invalidRelocation;
         }

      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);
   protected:
      virtual TR_OpaqueMethodBlock *getInlinedSiteCallerMethod(TR_RelocationRuntime *reloRuntime);
      virtual TR_OpaqueMethodBlock *getInlinedSiteMethod(TR_RelocationRuntime *reloRuntime);
      virtual TR_OpaqueMethodBlock *getInlinedSiteMethod(TR_RelocationRuntime *reloRuntime, uintptr_t siteIndex);
   };



class TR_RelocationRecordConstantPool : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordConstantPool() {}
      TR_RelocationRecordConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}
      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setConstantPool(TR_RelocationTarget *reloTarget, uintptr_t constantPool);
      uintptr_t constantPool(TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);


   protected:
      uintptr_t currentConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptr_t oldValue);
      uintptr_t findConstantPool(TR_RelocationTarget *reloTarget, uintptr_t oldValue, TR_OpaqueMethodBlock *ramMethod);
      uintptr_t computeNewConstantPool(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptr_t oldValue);
   };

class TR_RelocationRecordConstantPoolWithIndex : public TR_RelocationRecordConstantPool
   {
   public:
      TR_RelocationRecordConstantPoolWithIndex() {}
      TR_RelocationRecordConstantPoolWithIndex(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPool(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setCpIndex(TR_RelocationTarget *reloTarget, uintptr_t cpIndex);
      uintptr_t cpIndex(TR_RelocationTarget *reloTarget);

      TR_OpaqueMethodBlock *getVirtualMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex);
      TR_OpaqueMethodBlock *getStaticMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex);
      TR_OpaqueMethodBlock *getSpecialMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex);
      TR_OpaqueMethodBlock *getInterfaceMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod);
      TR_OpaqueMethodBlock *getAbstractMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod);
   };

class TR_RelocationRecordHelperAddress : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordHelperAddress() {}
      TR_RelocationRecordHelperAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setHelperID(TR_RelocationTarget *reloTarget, uint32_t helperID);
      uint32_t helperID(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

   };

class TR_RelocationRecordAbsoluteHelperAddress : public TR_RelocationRecordHelperAddress
   {
   public:
      TR_RelocationRecordAbsoluteHelperAddress() {}
      TR_RelocationRecordAbsoluteHelperAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordHelperAddress(reloRuntime, record) {}
      virtual const char *name();
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordDataAddress : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordDataAddress() {}
      TR_RelocationRecordDataAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setOffset(TR_RelocationTarget *reloTarget, uintptr_t offset);
      uintptr_t offset(TR_RelocationTarget *reloTarget);

      uint8_t *findDataAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

   };

class TR_RelocationRecordMethodAddress : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordMethodAddress() {}
      TR_RelocationRecordMethodAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name();
      uint8_t *currentMethodAddress(TR_RelocationRuntime *reloRuntime, uint8_t *oldMethodAddress);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordClassAddress : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordClassAddress() {}
      TR_RelocationRecordClassAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual const char *name();

      TR_OpaqueClassBlock *computeNewClassAddress(TR_RelocationRuntime *reloRuntime, uintptr_t newConstantPool, uintptr_t inlinedSiteIndex, uintptr_t cpIndex);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordDirectJNICall : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
   TR_RelocationRecordDirectJNICall() {}
   TR_RelocationRecordDirectJNICall(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
   virtual const char *name();

   void setOffsetToReloLocation(TR_RelocationTarget *reloTarget, uint8_t offsetToReloLocation);
   uint8_t offsetToReloLocation(TR_RelocationTarget *reloTarget);

   virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

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
   virtual const char *name()=0;
   virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

   private:
   virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex)=0;
   };


class TR_RelocationRecordStaticRamMethodConst : public TR_RelocationRecordRamMethodConst
   {
   public:
   TR_RelocationRecordStaticRamMethodConst() {};
   TR_RelocationRecordStaticRamMethodConst(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordRamMethodConst(reloRuntime, record) {}
   virtual const char *name()  { return "TR_RelocationRecordStaticRamMethodConst"; };

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
   virtual const char *name()  { return "TR_RelocationRecordVirtualRamMethodConst"; };

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
   virtual const char *name()  { return "TR_RelocationRecordSpecialRamMethodConst"; };

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
      virtual const char *name();

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordThunks : public TR_RelocationRecordConstantPool
   {
   public:
      TR_RelocationRecordThunks() {}
      TR_RelocationRecordThunks(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPool(reloRuntime, record) {}
      virtual const char *name();
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

   protected:
      TR_RelocationErrorCode relocateAndRegisterThunk(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uintptr_t cp, uintptr_t cpIndex, uint8_t *reloLocation);

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

      virtual const char *name();

      void setOffsetToJ2IVirtualThunkPointer(TR_RelocationTarget *reloTarget, uintptr_t j2iVirtualThunkPointer);
      uintptr_t getOffsetToJ2IVirtualThunkPointer(TR_RelocationTarget *reloTarget);

   protected:
      virtual void relocateJ2IVirtualThunkPointer(TR_RelocationTarget *reloTarget, uint8_t *reloLocation, void *thunk);
      uintptr_t offsetToJ2IVirtualThunkPointer(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordTrampolines : public TR_RelocationRecordConstantPool
   {
   public:
      TR_RelocationRecordTrampolines() {}
      TR_RelocationRecordTrampolines(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPool(reloRuntime, record) {}
      virtual const char *name();
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordPicTrampolines : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordPicTrampolines() {}
      TR_RelocationRecordPicTrampolines(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setNumTrampolines(TR_RelocationTarget *reloTarget, uint32_t numTrampolines);
      uint32_t numTrampolines(TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordInlinedAllocation : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordInlinedAllocation() {}
      TR_RelocationRecordInlinedAllocation(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setBranchOffset(TR_RelocationTarget *reloTarget, uintptr_t branchOffset);
      uintptr_t branchOffset(TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   protected:

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual bool verifyClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *clazz);
   };

class TR_RelocationRecordVerifyRefArrayForAlloc : public TR_RelocationRecordInlinedAllocation
   {
   public:
      TR_RelocationRecordVerifyRefArrayForAlloc() {}
      TR_RelocationRecordVerifyRefArrayForAlloc(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedAllocation(reloRuntime, record) {}
      virtual const char *name();
   protected:
      virtual bool verifyClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *clazz);
   };

class TR_RelocationRecordVerifyClassObjectForAlloc : public TR_RelocationRecordInlinedAllocation
   {
   public:
      TR_RelocationRecordVerifyClassObjectForAlloc() {}
      TR_RelocationRecordVerifyClassObjectForAlloc(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedAllocation(reloRuntime, record) {}
      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

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

      void setRomClassOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptr_t romClassOffsetInSharedCache,
                                          TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      void setRomClassOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptr_t romClassOffsetInSharedCache,
                                          TR::AheadOfTimeCompile *aotCompile, TR_OpaqueClassBlock *ramClass);
      uintptr_t romClassOffsetInSharedCache(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

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

      void setDestinationAddress(TR_RelocationTarget *reloTarget, uintptr_t destinationAddress);
      uintptr_t destinationAddress(TR_RelocationTarget *reloTarget);

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
      virtual const char *name();

   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
   };

class TR_RelocationRecordInlinedStaticMethod: public TR_RelocationRecordInlinedMethod
   {
   public:
      TR_RelocationRecordInlinedStaticMethod() {}
      TR_RelocationRecordInlinedStaticMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedMethod(reloRuntime, record) {}
      virtual const char *name();
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
   };


class TR_RelocationRecordInlinedSpecialMethodWithNopGuard : public TR_RelocationRecordNopGuard
   {
   public:
      TR_RelocationRecordInlinedSpecialMethodWithNopGuard() {}
      TR_RelocationRecordInlinedSpecialMethodWithNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordNopGuard(reloRuntime, record) {}
      virtual const char *name();

   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
   };

class TR_RelocationRecordInlinedSpecialMethod: public TR_RelocationRecordInlinedMethod
   {
   public:
      TR_RelocationRecordInlinedSpecialMethod() {}
      TR_RelocationRecordInlinedSpecialMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedMethod(reloRuntime, record) {}
      virtual const char *name();
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
   };

class TR_RelocationRecordInlinedVirtualMethodWithNopGuard : public TR_RelocationRecordNopGuard
   {
   public:
      TR_RelocationRecordInlinedVirtualMethodWithNopGuard() {}
      TR_RelocationRecordInlinedVirtualMethodWithNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordNopGuard(reloRuntime, record) {}
      virtual const char *name();
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
      virtual void createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation);
   };

class TR_RelocationRecordInlinedVirtualMethod: public TR_RelocationRecordInlinedMethod
   {
   public:
      TR_RelocationRecordInlinedVirtualMethod() {}
      TR_RelocationRecordInlinedVirtualMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedMethod(reloRuntime, record) {}
      virtual const char *name();
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
   };

class TR_RelocationRecordInlinedInterfaceMethodWithNopGuard : public TR_RelocationRecordNopGuard
   {
   public:
      TR_RelocationRecordInlinedInterfaceMethodWithNopGuard() {}
      TR_RelocationRecordInlinedInterfaceMethodWithNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordNopGuard(reloRuntime, record) {}
      virtual const char *name();
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
      virtual const char *name();
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
   };

class TR_RelocationRecordInlinedAbstractMethodWithNopGuard : public TR_RelocationRecordNopGuard
   {
   public:
      TR_RelocationRecordInlinedAbstractMethodWithNopGuard() {}
      TR_RelocationRecordInlinedAbstractMethodWithNopGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordNopGuard(reloRuntime, record) {}
      virtual const char *name();
   protected:
      virtual bool needsReceiverClassFromID() { return true; }
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod);
      virtual void updateFailedStats(TR_AOTStats *aotStats);
      virtual void updateSucceededStats(TR_AOTStats *aotStats);
      virtual void createAssumptions(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation);
   };

class TR_RelocationRecordInlinedAbstractMethod: public TR_RelocationRecordInlinedMethod
   {
   public:
      TR_RelocationRecordInlinedAbstractMethod() {}
      TR_RelocationRecordInlinedAbstractMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedMethod(reloRuntime, record) {}
      virtual const char *name();
   private:
      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpIndex, TR_OpaqueMethodBlock *callerMethod);
   };

class TR_RelocationRecordProfiledInlinedMethod : public TR_RelocationRecordInlinedMethod
   {
   public:
      TR_RelocationRecordProfiledInlinedMethod() {}
      TR_RelocationRecordProfiledInlinedMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordInlinedMethod(reloRuntime, record) {}
      virtual const char *name();

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setClassChainIdentifyingLoaderOffsetInSharedCache(
         TR_RelocationTarget *reloTarget, uintptr_t classChainOffsetInSharedCache,
         TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord
      );
      uintptr_t classChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget);

      void setClassChainForInlinedMethod(TR_RelocationTarget *reloTarget, uintptr_t classChainForInlinedMethod,
                                         TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainForInlinedMethod(TR_RelocationTarget *reloTarget);

      void setMethodIndex(TR_RelocationTarget *reloTarget, uintptr_t methodIndex);
      uintptr_t methodIndex(TR_RelocationTarget *reloTarget);

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
      virtual const char *name();

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
      virtual const char *name();

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

      void setDestinationAddress(TR_RelocationTarget *reloTarget, uintptr_t destinationAddress);
      uintptr_t destinationAddress(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordMethodEnterCheck : public TR_RelocationRecordMethodTracingCheck
   {
   public:
      TR_RelocationRecordMethodEnterCheck() {}
      TR_RelocationRecordMethodEnterCheck(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordMethodTracingCheck(reloRuntime, record) {}
      virtual const char *name();
      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);
   };

class TR_RelocationRecordMethodExitCheck : public TR_RelocationRecordMethodTracingCheck
   {
   public:
      TR_RelocationRecordMethodExitCheck() {}
      TR_RelocationRecordMethodExitCheck(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordMethodTracingCheck(reloRuntime, record) {}
      virtual const char *name();
      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);
   };

class TR_RelocationRecordRamMethod : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordRamMethod() {}
      TR_RelocationRecordRamMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name();

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordValidateClass : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordValidateClass() {}
      TR_RelocationRecordValidateClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual bool isStaticFieldValidation() { return false ; }

      void setClassChainOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptr_t classChainOffsetInSharedCache,
                                            TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainOffsetInSharedCache(TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

   protected:
      virtual TR_OpaqueClassBlock *getClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp);
      virtual bool validateClass(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *clazz, void *classChainOrRomClass);
      virtual TR_RelocationErrorCode failureCode();
   };

class TR_RelocationRecordValidateInstanceField : public TR_RelocationRecordValidateClass
   {
   public:
      TR_RelocationRecordValidateInstanceField() {}
      TR_RelocationRecordValidateInstanceField(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateClass(reloRuntime, record) {}
      virtual const char *name();

   protected:
      virtual TR_OpaqueClassBlock *getClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp);
      virtual TR_RelocationErrorCode failureCode();
   };

class TR_RelocationRecordValidateStaticField : public TR_RelocationRecordValidateInstanceField
   {
   public:
      TR_RelocationRecordValidateStaticField() {}
      TR_RelocationRecordValidateStaticField(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateInstanceField(reloRuntime, record) {}
      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual bool isStaticFieldValidation() { return true; }

      void setRomClassOffsetInSharedCache(TR_RelocationTarget *reloTarget, uintptr_t romClassOffsetInSharedCache,
                                          TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t romClassOffsetInSharedCache(TR_RelocationTarget *reloTarget);

   protected:
      virtual TR_OpaqueClassBlock *getClass(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, void *void_cp);
      virtual bool validateClass(TR_RelocationRuntime *reloRuntime, TR_OpaqueClassBlock *clazz, void *classChainOrRomClass);
   };

class TR_RelocationRecordValidateArbitraryClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateArbitraryClass() {}
      TR_RelocationRecordValidateArbitraryClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setClassChainIdentifyingLoaderOffset(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset,
                                                TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainIdentifyingLoaderOffset(TR_RelocationTarget *reloTarget);

      void setClassChainOffsetForClassBeingValidated(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset,
                                                     TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainOffsetForClassBeingValidated(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };


/* SYMBOL VALIDATION MANAGER */
class TR_RelocationRecordValidateClassByName : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassByName() {}
      TR_RelocationRecordValidateClassByName(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateClassByName"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setClassID(TR_RelocationTarget *reloTarget, uint16_t classID);
      uint16_t classID(TR_RelocationTarget *reloTarget);

      void setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID);
      uint16_t beholderID(TR_RelocationTarget *reloTarget);

      void setClassChainOffset(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset,
                               TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainOffset(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateProfiledClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateProfiledClass() {}
      TR_RelocationRecordValidateProfiledClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateProfiledClass"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setClassID(TR_RelocationTarget *reloTarget, uint16_t classID);
      uint16_t classID(TR_RelocationTarget *reloTarget);

      void setClassChainOffset(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset,
                               TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainOffset(TR_RelocationTarget *reloTarget);

      void setClassChainOffsetForClassLoader(TR_RelocationTarget *reloTarget, uintptr_t classChainOffsetForCL,
                                             TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainOffsetForClassLoader(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateClassFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassFromCP() {}
      TR_RelocationRecordValidateClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateClassFromCP"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setClassID(TR_RelocationTarget *reloTarget, uint16_t classID);
      uint16_t classID(TR_RelocationTarget *reloTarget);

      void setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID);
      uint16_t beholderID(TR_RelocationTarget *reloTarget);

      void setCpIndex(TR_RelocationTarget *reloTarget, uint32_t cpIndex);
      uint32_t cpIndex(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateDefiningClassFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateDefiningClassFromCP() {}
      TR_RelocationRecordValidateDefiningClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateDefiningClassFromCP"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setIsStatic(TR_RelocationTarget *reloTarget, bool isStatic);
      bool isStatic(TR_RelocationTarget *reloTarget);

      void setClassID(TR_RelocationTarget *reloTarget, uint16_t classID);
      uint16_t classID(TR_RelocationTarget *reloTarget);

      void setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID);
      uint16_t beholderID(TR_RelocationTarget *reloTarget);

      void setCpIndex(TR_RelocationTarget *reloTarget, uint32_t cpIndex);
      uint32_t cpIndex(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateStaticClassFromCP : public TR_RelocationRecordValidateClassFromCP
   {
   public:
      TR_RelocationRecordValidateStaticClassFromCP() {}
      TR_RelocationRecordValidateStaticClassFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateClassFromCP(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordValidateStaticClassFromCP"; }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateArrayClassFromComponentClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateArrayClassFromComponentClass() {}
      TR_RelocationRecordValidateArrayClassFromComponentClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateArrayClassFromComponentClass"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setArrayClassID(TR_RelocationTarget *reloTarget, uint16_t arrayClassID);
      uint16_t arrayClassID(TR_RelocationTarget *reloTarget);

      void setComponentClassID(TR_RelocationTarget *reloTarget, uint16_t componentClassID);
      uint16_t componentClassID(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateSuperClassFromClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateSuperClassFromClass() {}
      TR_RelocationRecordValidateSuperClassFromClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateSuperClassFromClass"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setSuperClassID(TR_RelocationTarget *reloTarget, uint16_t superClassID);
      uint16_t superClassID(TR_RelocationTarget *reloTarget);

      void setChildClassID(TR_RelocationTarget *reloTarget, uint16_t childClassID);
      uint16_t childClassID(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateClassInstanceOfClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassInstanceOfClass() {}
      TR_RelocationRecordValidateClassInstanceOfClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateClassInstanceOfClass"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setObjectTypeIsFixed(TR_RelocationTarget *reloTarget, bool objectTypeIsFixed);
      bool objectTypeIsFixed(TR_RelocationTarget *reloTarget);

      void setCastTypeIsFixed(TR_RelocationTarget *reloTarget, bool castTypeIsFixed);
      bool castTypeIsFixed(TR_RelocationTarget *reloTarget);

      void setIsInstanceOf(TR_RelocationTarget *reloTarget, bool isInstanceOf);
      bool isInstanceOf(TR_RelocationTarget *reloTarget);

      void setClassOneID(TR_RelocationTarget *reloTarget, uint16_t classOneID);
      uint16_t classOneID(TR_RelocationTarget *reloTarget);

      void setClassTwoID(TR_RelocationTarget *reloTarget, uint16_t classTwoID);
      uint16_t classTwoID(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateSystemClassByName : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateSystemClassByName() {}
      TR_RelocationRecordValidateSystemClassByName(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateSystemClassByName"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setSystemClassID(TR_RelocationTarget *reloTarget, uint16_t systemClassID);
      uint16_t systemClassID(TR_RelocationTarget *reloTarget);

      void setClassChainOffset(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset,
                               TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainOffset(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateClassFromITableIndexCP : public TR_RelocationRecordValidateClassFromCP
   {
   public:
      TR_RelocationRecordValidateClassFromITableIndexCP() {}
      TR_RelocationRecordValidateClassFromITableIndexCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateClassFromCP(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordValidateClassFromITableIndexCP"; }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic : public TR_RelocationRecordValidateClassFromCP
   {
   public:
      TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic() {}
      TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateClassFromCP(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordValidateDeclaringClassFromFieldOrStatic"; }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateConcreteSubClassFromClass : public TR_RelocationRecordValidateSuperClassFromClass
   {
   public:
      TR_RelocationRecordValidateConcreteSubClassFromClass() {}
      TR_RelocationRecordValidateConcreteSubClassFromClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record)
         : TR_RelocationRecordValidateSuperClassFromClass(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordValidateConcreteSubClassFromClass"; }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateClassChain : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassChain() {}
      TR_RelocationRecordValidateClassChain(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateClassChain"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setClassID(TR_RelocationTarget *reloTarget, uint16_t classID);
      uint16_t classID(TR_RelocationTarget *reloTarget);

      void setClassChainOffset(TR_RelocationTarget *reloTarget, uintptr_t classChainOffset,
                               TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainOffset(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateMethodFromClass : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromClass() {}
      TR_RelocationRecordValidateMethodFromClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateMethodFromClass"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);

      void setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID);
      uint16_t beholderID(TR_RelocationTarget *reloTarget);

      void setIndex(TR_RelocationTarget *reloTarget, uint32_t index);
      uint32_t index(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateMethodFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromCP() {}
      TR_RelocationRecordValidateMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);

      void setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID);
      uint16_t definingClassID(TR_RelocationTarget *reloTarget);

      void setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID);
      uint16_t beholderID(TR_RelocationTarget *reloTarget);

      void setCpIndex(TR_RelocationTarget *reloTarget, uint16_t cpIndex);
      uint16_t cpIndex(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateStaticMethodFromCP : public TR_RelocationRecordValidateMethodFromCP
   {
   public:
      TR_RelocationRecordValidateStaticMethodFromCP() {}
      TR_RelocationRecordValidateStaticMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateMethodFromCP(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordValidateStaticMethodFromCP"; }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateSpecialMethodFromCP : public TR_RelocationRecordValidateMethodFromCP
   {
   public:
      TR_RelocationRecordValidateSpecialMethodFromCP() {}
      TR_RelocationRecordValidateSpecialMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateMethodFromCP(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordValidateSpecialMethodFromCP"; }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateVirtualMethodFromCP : public TR_RelocationRecordValidateMethodFromCP
   {
   public:
      TR_RelocationRecordValidateVirtualMethodFromCP() {}
      TR_RelocationRecordValidateVirtualMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateMethodFromCP(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordValidateVirtualMethodFromCP"; }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateVirtualMethodFromOffset : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateVirtualMethodFromOffset() {}
      TR_RelocationRecordValidateVirtualMethodFromOffset(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateVirtualMethodFromOffset"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);

      void setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID);
      uint16_t definingClassID(TR_RelocationTarget *reloTarget);

      void setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID);
      uint16_t beholderID(TR_RelocationTarget *reloTarget);

      void setVirtualCallOffsetAndIgnoreRtResolve(TR_RelocationTarget *reloTarget, uint16_t virtualCallOffsetAndIgnoreRtResolve);
      uint16_t virtualCallOffsetAndIgnoreRtResolve(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateInterfaceMethodFromCP : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateInterfaceMethodFromCP() {}
      TR_RelocationRecordValidateInterfaceMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateInterfaceMethodFromCP"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);

      void setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID);
      uint16_t definingClassID(TR_RelocationTarget *reloTarget);

      void setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID);
      uint16_t beholderID(TR_RelocationTarget *reloTarget);

      void setLookupID(TR_RelocationTarget *reloTarget, uint16_t lookupID);
      uint16_t lookupID(TR_RelocationTarget *reloTarget);

      void setCpIndex(TR_RelocationTarget *reloTarget, uint16_t cpIndex);
      uint16_t cpIndex(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateImproperInterfaceMethodFromCP : public TR_RelocationRecordValidateMethodFromCP
   {
   public:
      TR_RelocationRecordValidateImproperInterfaceMethodFromCP() {}
      TR_RelocationRecordValidateImproperInterfaceMethodFromCP(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordValidateMethodFromCP(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordValidateImproperInterfaceMethodFromCP"; }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordValidateMethodFromClassAndSig : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromClassAndSig() {}
      TR_RelocationRecordValidateMethodFromClassAndSig(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateMethodFromClassAndSig"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);

      void setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID);
      uint16_t definingClassID(TR_RelocationTarget *reloTarget);

      void setBeholderID(TR_RelocationTarget *reloTarget, uint16_t beholderID);
      uint16_t beholderID(TR_RelocationTarget *reloTarget);

      void setLookupClassID(TR_RelocationTarget *reloTarget, uint16_t lookupClassID);
      uint16_t lookupClassID(TR_RelocationTarget *reloTarget);

      void setRomMethodOffsetInSCC(TR_RelocationTarget *reloTarget, uintptr_t romMethodOffsetInSCC,
                                   TR::AheadOfTimeCompile *aotCompile, J9Method *method, TR_OpaqueClassBlock *definingClass);
      uintptr_t romMethodOffsetInSCC(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateStackWalkerMaySkipFrames : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateStackWalkerMaySkipFrames() {}
      TR_RelocationRecordValidateStackWalkerMaySkipFrames(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateStackWalkerMaySkipFrames"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);

      void setMethodClassID(TR_RelocationTarget *reloTarget, uint16_t methodClassID);
      uint16_t methodClassID(TR_RelocationTarget *reloTarget);

      void setSkipFrames(TR_RelocationTarget *reloTarget, bool skipFrames);
      bool skipFrames(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateClassInfoIsInitialized : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateClassInfoIsInitialized() {}
      TR_RelocationRecordValidateClassInfoIsInitialized(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateClassInfoIsInitialized"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setClassID(TR_RelocationTarget *reloTarget, uint16_t classID);
      uint16_t classID(TR_RelocationTarget *reloTarget);

      void setIsInitialized(TR_RelocationTarget *reloTarget, bool isInitialized);
      bool isInitialized(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateMethodFromSingleImpl : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromSingleImpl() {}
      TR_RelocationRecordValidateMethodFromSingleImpl(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateMethodFromSingleImpl"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);

      void setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID);
      uint16_t definingClassID(TR_RelocationTarget *reloTarget);

      void setThisClassID(TR_RelocationTarget *reloTarget, uint16_t thisClassID);
      uint16_t thisClassID(TR_RelocationTarget *reloTarget);

      void setCpIndexOrVftSlot(TR_RelocationTarget *reloTarget, int32_t cpIndexOrVftSlot);
      int32_t cpIndexOrVftSlot(TR_RelocationTarget *reloTarget);

      void setCallerMethodID  (TR_RelocationTarget *reloTarget, uint16_t callerMethodID);
      uint16_t callerMethodID(TR_RelocationTarget *reloTarget);

      void setUseGetResolvedInterfaceMethod(TR_RelocationTarget *reloTarget, TR_YesNoMaybe useGetResolvedInterfaceMethod);
      TR_YesNoMaybe useGetResolvedInterfaceMethod(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateMethodFromSingleInterfaceImpl : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromSingleInterfaceImpl() {}
      TR_RelocationRecordValidateMethodFromSingleInterfaceImpl(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateMethodFromSingleInterfaceImpl"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);

      void setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID);
      uint16_t definingClassID(TR_RelocationTarget *reloTarget);

      void setThisClassID(TR_RelocationTarget *reloTarget, uint16_t thisClassID);
      uint16_t thisClassID(TR_RelocationTarget *reloTarget);

      void setCpIndex(TR_RelocationTarget *reloTarget, uint16_t cpIndex);
      uint16_t cpIndex(TR_RelocationTarget *reloTarget);

      void setCallerMethodID  (TR_RelocationTarget *reloTarget, uint16_t callerMethodID);
      uint16_t callerMethodID(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateMethodFromSingleAbstractImpl : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateMethodFromSingleAbstractImpl() {}
      TR_RelocationRecordValidateMethodFromSingleAbstractImpl(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateMethodFromSingleAbstractImpl"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);

      void setDefiningClassID(TR_RelocationTarget *reloTarget, uint16_t definingClassID);
      uint16_t definingClassID(TR_RelocationTarget *reloTarget);

      void setThisClassID(TR_RelocationTarget *reloTarget, uint16_t thisClassID);
      uint16_t thisClassID(TR_RelocationTarget *reloTarget);

      void setVftSlot(TR_RelocationTarget *reloTarget, int32_t vftSlot);
      int32_t vftSlot(TR_RelocationTarget *reloTarget);

      void setCallerMethodID  (TR_RelocationTarget *reloTarget, uint16_t callerMethodID);
      uint16_t callerMethodID(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordSymbolFromManager : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordSymbolFromManager() {}
      TR_RelocationRecordSymbolFromManager(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordSymbolFromManager"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual void storePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual void activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      bool needsUnloadAssumptions(TR::SymbolType symbolType);
      bool needsRedefinitionAssumption(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation, TR_OpaqueClassBlock *clazz, TR::SymbolType symbolType);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setSymbolID(TR_RelocationTarget *reloTarget, uint16_t symbolID);
      uint16_t symbolID(TR_RelocationTarget *reloTarget);

      void setSymbolType(TR_RelocationTarget *reloTarget, TR::SymbolType symbolType);
      TR::SymbolType symbolType(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordDiscontiguousSymbolFromManager : public TR_RelocationRecordSymbolFromManager
   {
   public:
      TR_RelocationRecordDiscontiguousSymbolFromManager() {}
      TR_RelocationRecordDiscontiguousSymbolFromManager(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordSymbolFromManager(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordDiscontiguousSymbolFromManager"; }
      virtual void storePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordResolvedTrampolines : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordResolvedTrampolines() {}
      TR_RelocationRecordResolvedTrampolines(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual const char *name() { return "TR_RelocationRecordResolvedTrampolines"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setSymbolID(TR_RelocationTarget *reloTarget, uint16_t symbolID);
      uint16_t symbolID(TR_RelocationTarget *reloTarget);
   };
/* SYMBOL VALIDATION MANAGER */

class TR_RelocationRecordHCR : public TR_RelocationRecordWithOffset
   {
   public:
      TR_RelocationRecordHCR() {}
      TR_RelocationRecordHCR(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithOffset(reloRuntime, record) {}
      virtual const char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) { }

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);
   };

class TR_RelocationRecordPointer : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordPointer() {}
      TR_RelocationRecordPointer(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setClassChainIdentifyingLoaderOffsetInSharedCache(
         TR_RelocationTarget *reloTarget, uintptr_t classChainOffsetInSharedCache,
         TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord
      );
      uintptr_t classChainIdentifyingLoaderOffsetInSharedCache(TR_RelocationTarget *reloTarget);

      void setClassChainForInlinedMethod(TR_RelocationTarget *reloTarget, uintptr_t classChainOffsetInSharedCache,
                                         TR::AheadOfTimeCompile *aotCompile, const AOTCacheClassChainRecord *classChainRecord);
      uintptr_t classChainForInlinedMethod(TR_RelocationTarget *reloTarget);

      virtual TR_RelocationRecordAction action(TR_RelocationRuntime *reloRuntime);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   protected:
      virtual uintptr_t computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer) { return 0;}
      virtual void activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      void registerHCRAssumption(TR_RelocationRuntime *reloRuntime, uint8_t *reloLocation);
   };

class TR_RelocationRecordClassPointer : public TR_RelocationRecordPointer
   {
   public:
      TR_RelocationRecordClassPointer() {}
      TR_RelocationRecordClassPointer(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordPointer(reloRuntime, record) {}
      virtual const char *name();

   protected:
      virtual uintptr_t computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer);
      virtual void activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

// Relocation record data to identify the class is as for TR_ClassPointer, but
// patching in the value is done as though for TR_ClassAddress.
class TR_RelocationRecordArbitraryClassAddress : public TR_RelocationRecordClassPointer
   {
   public:
      TR_RelocationRecordArbitraryClassAddress() {}
      TR_RelocationRecordArbitraryClassAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordClassPointer(reloRuntime, record) {}
      virtual const char *name();

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

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
      virtual const char *name();
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setVTableSlot(TR_RelocationTarget *reloTarget, uintptr_t vTableSlot);
      uintptr_t vTableSlot(TR_RelocationTarget *reloTarget);

   protected:
      virtual uintptr_t computePointer(TR_RelocationTarget *reloTarget, TR_OpaqueClassBlock *classPointer);
      virtual void activatePointer(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordInlinedMethodPointer : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordInlinedMethodPointer() {}
      TR_RelocationRecordInlinedMethodPointer(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}
      virtual const char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordEmitClass : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordEmitClass() {}
      TR_RelocationRecordEmitClass(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}

      void setBCIndex(TR_RelocationTarget *reloTarget, int32_t bcIndex);
      int32_t bcIndex(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordDebugCounter : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordDebugCounter() {}
      TR_RelocationRecordDebugCounter(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}
      virtual const char *name();

      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);

      void setBCIndex(TR_RelocationTarget *reloTarget, int32_t bcIndex);
      int32_t bcIndex(TR_RelocationTarget *reloTarget);

      void setDelta(TR_RelocationTarget *reloTarget, int32_t delta);
      int32_t delta(TR_RelocationTarget *reloTarget);

      void setFidelity(TR_RelocationTarget *reloTarget, int8_t fidelity);
      int8_t fidelity(TR_RelocationTarget *reloTarget);

      void setStaticDelta(TR_RelocationTarget *reloTarget, int32_t staticDelta);
      int32_t staticDelta(TR_RelocationTarget *reloTarget);

      void setOffsetOfNameString(TR_RelocationTarget *reloTarget, uintptr_t offsetOfNameString);
      uintptr_t offsetOfNameString(TR_RelocationTarget *reloTarget);

   private:
      TR::DebugCounterBase *findOrCreateCounter(TR_RelocationRuntime *reloRuntime);
   };

class TR_RelocationRecordClassUnloadAssumption : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordClassUnloadAssumption() {}
      TR_RelocationRecordClassUnloadAssumption(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}

      virtual const char *name();

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
   };

class TR_RelocationRecordMethodCallAddress : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordMethodCallAddress() {}
      TR_RelocationRecordMethodCallAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}

      virtual const char *name() { return "TR_MethodCallAddress"; }
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      uint8_t* address(TR_RelocationTarget *reloTarget);
      void setAddress(TR_RelocationTarget *reloTarget, uint8_t* callTargetAddress);

   private:
      uint8_t *computeTargetMethodAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *baseLocation);
   };

class TR_RelocationRecordBreakpointGuard : public TR_RelocationRecordWithInlinedSiteIndex
   {
   public:
      TR_RelocationRecordBreakpointGuard() {}
      TR_RelocationRecordBreakpointGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record)
         : TR_RelocationRecordWithInlinedSiteIndex(reloRuntime, record) {}

      virtual const char *name() { return "TR_Breakpoint"; }
      virtual void print(TR_RelocationRuntime *reloRuntime);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      void setDestinationAddress(TR_RelocationTarget *reloTarget, uintptr_t destinationAddress);
      uintptr_t destinationAddress(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordValidateJ2IThunkFromMethod : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateJ2IThunkFromMethod() {}
      TR_RelocationRecordValidateJ2IThunkFromMethod(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateJ2IThunkFromMethod"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setThunkID(TR_RelocationTarget *reloTarget, uint16_t methodClassID);
      uint16_t thunkID(TR_RelocationTarget *reloTarget);

      void setMethodID(TR_RelocationTarget *reloTarget, uint16_t methodID);
      uint16_t methodID(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordStaticDefaultValueInstance : public TR_RelocationRecordClassAddress
   {
   public:
      TR_RelocationRecordStaticDefaultValueInstance() {}
      TR_RelocationRecordStaticDefaultValueInstance(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordClassAddress(reloRuntime, record) {}

      virtual const char *name();
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordValidateIsClassVisible : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordValidateIsClassVisible() {}
      TR_RelocationRecordValidateIsClassVisible(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}
      virtual bool isValidationRecord() { return true; }
      virtual const char *name() { return "TR_RelocationRecordValidateIsClassVisible"; }
      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) {}
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setSourceClassID(TR_RelocationTarget *reloTarget, uint16_t sourceClassID);
      uint16_t sourceClassID(TR_RelocationTarget *reloTarget);

      void setDestClassID(TR_RelocationTarget *reloTarget, uint16_t destClassID);
      uint16_t destClassID(TR_RelocationTarget *reloTarget);

      void setIsVisible(TR_RelocationTarget *reloTarget, bool isVisible);
      bool isVisible(TR_RelocationTarget *reloTarget);
   };

class TR_RelocationRecordCatchBlockCounter : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordCatchBlockCounter() {}
      TR_RelocationRecordCatchBlockCounter(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}

      virtual const char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordStartPC : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordStartPC() {}
      TR_RelocationRecordStartPC(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}

      virtual const char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

class TR_RelocationRecordMethodEnterExitHookAddress : public TR_RelocationRecord
   {
   public:
      TR_RelocationRecordMethodEnterExitHookAddress() {}
      TR_RelocationRecordMethodEnterExitHookAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecord(reloRuntime, record) {}

      virtual const char *name() { return "TR_RelocationRecordMethodEnterExitHookAddress"; }
      virtual void print(TR_RelocationRuntime *reloRuntime);

      void setIsEnterHookAddr(TR_RelocationTarget *reloTarget, bool isEnterHookAddr);
      bool isEnterHookAddr(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);
      virtual TR_RelocationErrorCode applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocationHigh, uint8_t *reloLocationLow);
   };

#endif   // RELOCATION_RECORD_INCL

