/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef RUNTIME_ASSUMPTIONS_INCL
#define RUNTIME_ASSUMPTIONS_INCL

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "env/RuntimeAssumptionTable.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "runtime/OMRRuntimeAssumptions.hpp"


class TR_FrontEnd;
class TR_OpaqueClassBlock;
class TR_PatchJNICallSite;
class TR_PatchNOPedGuardSite;
class TR_PersistentClassInfo;
class TR_PersistentClassInfoForFields;
class TR_PreXRecompile;
class TR_RedefinedClassPicSite;
class TR_UnloadedClassPicSite;

/**
 * @brief Enum to describe the result of a Class Chain Validation (CCV).
 */
enum CCVResult : uint8_t
   {
   notYetValidated,
   success,
   failure
   };

class TR_SubClass : public TR_Link0<TR_SubClass>
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo);
   TR_SubClass(TR_PersistentClassInfo * classInfo) : _classInfo(classInfo) { }
   TR_PersistentClassInfo * getClassInfo() { return _classInfo; }

private:
   TR_PersistentClassInfo * _classInfo;
   };

class TR_PersistentClassInfo : public TR_Link0<TR_PersistentClassInfo>
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo);
   TR_PersistentClassInfo(TR_OpaqueClassBlock *id) :
      _classId(id), _fieldInfo(0),
      _prexAssumptions(0), _timeStamp(0),
      _nameLength(-1), _ccvResult(notYetValidated)
    {
    uintptr_t classPointerValue = (uintptr_t) id;

    // Bit is switched on to mark it as uninitialized
    //
    _classId = (TR_OpaqueClassBlock *) ((classPointerValue | 1));
    }

   TR_OpaqueClassBlock *getClassId() { return (TR_OpaqueClassBlock *) (((uintptr_t) _classId) & ~(uintptr_t)1); }
   bool isInitialized(bool validate = true);
   virtual void setInitialized(TR_PersistentMemory *);

   // HCR
   virtual void setClassId(TR_OpaqueClassBlock *newClass)
      {
      _classId = (TR_OpaqueClassBlock *) (((uintptr_t)newClass) | (uintptr_t)isInitialized());
      setClassHasBeenRedefined(true);
      }

   TR_SubClass *getFirstSubclass() { return _subClasses.getFirst(); }
   virtual void setFirstSubClass(TR_SubClass *sc) { _subClasses.setFirst(sc); }

   void setVisited() {_visitedStatus |= 1; }
   void resetVisited() { _visitedStatus = _visitedStatus & ~(uintptr_t)1; }
   bool hasBeenVisited()
    {
    if (_visitedStatus & 1)
       return true;
    return false;
    }

   TR_PersistentClassInfoForFields *getFieldInfo()
      {
      uintptr_t fieldInfo = (uintptr_t) _fieldInfo;
      fieldInfo = fieldInfo & ~(uintptr_t)3;
      return (TR_PersistentClassInfoForFields *) fieldInfo;
      }
   virtual void setFieldInfo(TR_PersistentClassInfoForFields *i)
      {
      uintptr_t fieldInfo = (uintptr_t) i;
      fieldInfo |= (((uintptr_t) _fieldInfo) & (uintptr_t)3);
      _visitedStatus = fieldInfo;
      }

   int32_t getSubClassesCount() { return _subClasses.getSize(); }

   virtual TR_SubClass *addSubClass(TR_PersistentClassInfo *subClass);
   virtual void removeSubClasses();
   virtual void removeASubClass(TR_PersistentClassInfo *subClass);
   virtual void removeUnloadedSubClasses();
   virtual void setUnloaded(){_visitedStatus |= 0x2;}
   bool getUnloaded()
    {
    if (_visitedStatus & 0x2)
       return true;
    return false;
   }
   uint16_t getTimeStamp() { return _timeStamp; }
   int16_t getNumPrexAssumptions() { return _prexAssumptions; }
   virtual void incNumPrexAssumptions() { _prexAssumptions++; }

   virtual void setReservable(bool v = true)              { _flags.set(_isReservable, v); }
   bool isReservable()                            { return _flags.testAny(_isReservable); }

   virtual void setShouldNotBeNewlyExtended(int32_t ID);
   virtual void resetShouldNotBeNewlyExtended(int32_t ID){ _shouldNotBeNewlyExtended.reset(1 << ID); }
   virtual void clearShouldNotBeNewlyExtended()          { _shouldNotBeNewlyExtended.clear(); }
   bool shouldNotBeNewlyExtended()               { return _shouldNotBeNewlyExtended.testAny(0xff); }
   bool shouldNotBeNewlyExtended(int32_t ID)     { return _shouldNotBeNewlyExtended.testAny(1 << ID); }
   flags8_t getShouldNotBeNewlyExtendedMask() const { return _shouldNotBeNewlyExtended; }

   virtual void setHasRecognizedAnnotations(bool v = true){ _flags.set(_containsRecognizedAnnotations, v); }
   bool hasRecognizedAnnotations()                { return _flags.testAny(_containsRecognizedAnnotations); }
   virtual void setAlreadyCheckedForAnnotations(bool v = true){ _flags.set(_alreadyScannedForAnnotations, v); }
   bool alreadyCheckedForAnnotations()            { return _flags.testAny(_alreadyScannedForAnnotations); }

   virtual void setCannotTrustStaticFinal(bool v = true)  { _flags.set(_cannotTrustStaticFinal, v); }
   bool cannotTrustStaticFinal()                  { return _flags.testAny(_cannotTrustStaticFinal); }

   // HCR
   virtual void setClassHasBeenRedefined(bool v = true)  { _flags.set(_classHasBeenRedefined, v); }
   bool classHasBeenRedefined()                  { return _flags.testAny(_classHasBeenRedefined); }

   int32_t getNameLength()                       { return _nameLength; }
   virtual void setNameLength(int32_t length)            { _nameLength = length; }

   void setCCVResult(CCVResult result) { _ccvResult = result; }
   CCVResult getCCVResult() const { return _ccvResult; }

   private:

   enum // flag bits
      {
      _isReservable                         = 0x02,
      _containsRecognizedAnnotations        = 0x04,
      _alreadyScannedForAnnotations         = 0x08,
      _cannotTrustStaticFinal               = 0x10,
      // HCR
      _classHasBeenRedefined                = 0x20,
      _dummyEnum
      };

   friend class TR_ClassQueries;
   friend class J9::Options;
   friend class ::OMR::Options;
#if defined(J9VM_OPT_JITSERVER)
   friend class FlatPersistentClassInfo;
#endif

   TR_OpaqueClassBlock               *_classId;
   TR_LinkHead0<TR_SubClass>          _subClasses;

   union
      {
      uintptr_t _visitedStatus;
      TR_PersistentClassInfoForFields *_fieldInfo;
      };
   int16_t                             _prexAssumptions;
   uint16_t                            _timeStamp;
   int32_t                             _nameLength;
   flags8_t                            _flags;
   flags8_t                            _shouldNotBeNewlyExtended; // one bit for each possible compilation thread
   CCVResult                           _ccvResult;
   };

class TR_AddressRange
   {
   uintptr_t _start;
   uintptr_t _end; // start == end means only one address in this range

   public:
   TR_PERSISTENT_ALLOC(TR_MemoryBase::Assumption);
   TR_AddressRange()
#if defined(DEBUG)
      // Paint invalid fields
      :_start(0x321dead),
       _end(0x321dead)
#endif
      {}

   uintptr_t getStart() { return _start; }
   uintptr_t getEnd()   { return _end; }

   void initialize(uintptr_t start, uintptr_t end){ _start = start; _end = end; }
   void add(uintptr_t start, uintptr_t end){ _start = std::min(_start, start); _end = std::max(end, _end); }
   bool covers(uintptr_t address){ return _start <= address && address <= _end; }

   };

class TR_AddressSet
   {
   TR_AddressRange *_addressRanges;
   int32_t          _numAddressRanges;
   int32_t          _maxAddressRanges;

   static void trace(char *format, ...);
   static bool enableTraceDetails();
   static void traceDetails(char *format, ...);

   void moveAddressRanges(int32_t desiredHole, int32_t currentHole);
   void moveAddressRangesBy(int32_t low, int32_t high, int32_t distance);
   void findCheapestRangesToMerge(uintptr_t *cost, int32_t *lowerIndex);

   int32_t firstHigherAddressRangeIndex(uintptr_t address); // the binary search

   public:


   TR_PERSISTENT_ALLOC(TR_Memory::Assumption);

   TR_AddressSet(TR_PersistentMemory *persistentMemory, int32_t maxAddressRanges):
      _addressRanges(new (persistentMemory) TR_AddressRange[maxAddressRanges]),
      _numAddressRanges(0),
      _maxAddressRanges(maxAddressRanges)
      {}

#if defined(J9VM_OPT_JITSERVER)
   void destroy();
   void getRanges(std::vector<TR_AddressRange> &ranges); // copies the address ranges stored in the current object into a vector
   void setRanges(const std::vector<TR_AddressRange> &ranges); // loads the address ranges from the vector given as parameter
   int32_t getNumberOfRanges() const { return _numAddressRanges; }
   int32_t getMaxRanges() const { return _maxAddressRanges; }
#endif

   void add        (uintptr_t address){ add(address, address); }
   void add        (uintptr_t start, uintptr_t end);
   bool mayContain (uintptr_t address)
      {
      traceDetails("%p.mayContain(%p)\n", this, address);
      int32_t index = firstHigherAddressRangeIndex(address);
      if (index < _numAddressRanges && _addressRanges[index].covers(address))
         return true;
      else
         return false;
      }

   };

class TR_UnloadedClassPicSite : public OMR::ValueModifyRuntimeAssumption
   {
   protected:
   TR_UnloadedClassPicSite(TR_PersistentMemory * pm, uintptr_t key, uint8_t *picLocation, uint32_t size = sizeof(uintptr_t))
      : OMR::ValueModifyRuntimeAssumption(pm, key), _picLocation(picLocation), _size(size)
         {}

   public:
   static TR_UnloadedClassPicSite *make(
      TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptr_t key, uint8_t *picLocation, uint32_t size,
      TR_RuntimeAssumptionKind kind, OMR::RuntimeAssumption **sentinel);
   virtual TR_RuntimeAssumptionKind getAssumptionKind() { return RuntimeAssumptionOnClassUnload; }
   virtual void compensate(TR_FrontEnd *vm, bool isSMP, void *data);
   virtual bool equals(OMR::RuntimeAssumption &other)
      {
      TR_UnloadedClassPicSite *o = other.asUCPSite();
      return (o != 0) && o->_picLocation == _picLocation;
      }
   virtual uint8_t *getFirstAssumingPC() { return getPicLocation(); }
   virtual uint8_t *getLastAssumingPC() { return getPicLocation(); }
   virtual TR_UnloadedClassPicSite *asUCPSite() { return this; }
   uint8_t * getPicLocation()    { return _picLocation; }
   void      setPicLocation   (uint8_t *p) { _picLocation = p; }
   uintptr_t getPicEntry() { return *((uintptr_t*)_picLocation); }

   virtual void dumpInfo();

   private:
   uint8_t    *_picLocation;
   uint32_t    _size;
   };

#ifdef J9VM_OPT_JITSERVER
// The following needs to have enough fields to cover any possible
// runtime assumption that we may want to send from the server to the client
struct SerializedRuntimeAssumption
   {
   SerializedRuntimeAssumption(TR_RuntimeAssumptionKind kind,
                               uintptr_t key,
                               intptr_t offset,
                               uint32_t size = 0,
                               bool bOffsetFromMetaDataBase = false)
      : _kind(kind), _key(key), _offset(offset), _size(size), _bOffsetFromMetaDataBase(bOffsetFromMetaDataBase) {}
   TR_RuntimeAssumptionKind getKind() const { return _kind; }
   uintptr_t getKey() const { return _key; }
   intptr_t getOffset() const { return _offset; }
   uint32_t getSize() const { return _size; }
   bool isOffsetFromMetaDataBase() const { return _bOffsetFromMetaDataBase; }

   TR_RuntimeAssumptionKind _kind;
   uint32_t  _size;
   uintptr_t _key;
   intptr_t  _offset; // By default, it is the offset from the start of the binary buffer.
   bool      _bOffsetFromMetaDataBase; // If true, the _offset is from the start of TR_MethodMetaData.
   };
#endif // J9VM_OPT_JITSERVER

#endif
