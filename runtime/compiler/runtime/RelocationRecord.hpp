/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

extern char* AOTcgDiagOn;

class TR_RelocationRecordGroup
   {
   public:
      TR_RelocationRecordGroup(TR_RelocationRecordBinaryTemplate *groupData) : _group(groupData) {};

      void setSize(TR_RelocationTarget *reloTarget, uintptr_t size);
      uintptr_t size(TR_RelocationTarget *reloTarget);

      TR_RelocationRecordBinaryTemplate *firstRecord(TR_RelocationTarget *reloTarget);
      TR_RelocationRecordBinaryTemplate *pastLastRecord(TR_RelocationTarget *reloTarget);

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

      TR_RelocationRuntime *_reloRuntime;

      virtual bool ignore(TR_RelocationRuntime *reloRuntime);

   protected:
      OMR::RuntimeAssumption** getMetadataAssumptionList(J9JITExceptionTable *exceptionTable)
         {
         return (OMR::RuntimeAssumption**)(&exceptionTable->runtimeAssumptionList);
         }
      uint8_t *computeHelperAddress(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *baseLocation);

      void setFlag(TR_RelocationTarget *reloTarget, uint8_t flag);
      uint8_t flags(TR_RelocationTarget *reloTarget);

      void setReloFlags(TR_RelocationTarget *reloTarget, uint8_t reloFlags);
      uint8_t reloFlags(TR_RelocationTarget *reloTarget);

      TR_RelocationRecordPrivateData *privateData()
         {
         return &_privateData;
         }

      TR_RelocationRecordBinaryTemplate *_record;

      TR_RelocationRecordPrivateData _privateData;
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

      virtual bool ignore(TR_RelocationRuntime *reloRuntime);
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

class TR_RelocationRecordClassObject : public TR_RelocationRecordConstantPoolWithIndex
   {
   public:
      TR_RelocationRecordClassObject() {}
      TR_RelocationRecordClassObject(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordConstantPoolWithIndex(reloRuntime, record) {}
      virtual char *name();

      TR_OpaqueClassBlock *computeNewClassObject(TR_RelocationRuntime *reloRuntime, uintptrj_t newConstantPool, uintptrj_t inlinedSiteIndex, uintptrj_t cpIndex);

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

      virtual bool ignore(TR_RelocationRuntime *reloRuntime)
         {
         // inlined method validations cannot be skipped as they control whether inlined method's relocations are needed
         return false;
         }

   protected:
      virtual void fixInlinedSiteInfo(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueMethodBlock *inlinedMethod);

   private:
      virtual bool validateClassesSame(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, TR_OpaqueMethodBlock **theMethod);

      virtual TR_OpaqueMethodBlock *getMethodFromCP(TR_RelocationRuntime *reloRuntime, void *void_cp, int32_t cpindex, TR_OpaqueMethodBlock *callerMethod) { return NULL; }

      virtual void invalidateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation) {};
      virtual void activateGuard(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation) {};

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

      void setVTableSlot(TR_RelocationTarget *reloTarget, uintptrj_t vTableSlot);
      uintptrj_t vTableSlot(TR_RelocationTarget *reloTarget);

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget);

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
      virtual bool ignore(TR_RelocationRuntime *reloRuntime);
   };

class TR_RelocationRecordMethodExitCheck : public TR_RelocationRecordMethodTracingCheck
   {
   public:
      TR_RelocationRecordMethodExitCheck() {}
      TR_RelocationRecordMethodExitCheck(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordMethodTracingCheck(reloRuntime, record) {}
      virtual char *name();
      virtual bool ignore(TR_RelocationRuntime *reloRuntime);
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


class TR_RelocationRecordHCR : public TR_RelocationRecordWithOffset
   {
   public:
      TR_RelocationRecordHCR() {}
      TR_RelocationRecordHCR(TR_RelocationRuntime *reloRuntime, TR_RelocationRecordBinaryTemplate *record) : TR_RelocationRecordWithOffset(reloRuntime, record) {}
      virtual char *name();

      virtual void preparePrivateData(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget) { }

      virtual int32_t applyRelocation(TR_RelocationRuntime *reloRuntime, TR_RelocationTarget *reloTarget, uint8_t *reloLocation);

      virtual bool ignore(TR_RelocationRuntime *reloRuntime);
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

      virtual bool ignore(TR_RelocationRuntime *reloRuntime);

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

#endif   // RELOCATION_RECORD_INCL

