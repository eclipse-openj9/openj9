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

#ifndef RUNTIME_ASSUMPTIONS_INCL
#define RUNTIME_ASSUMPTIONS_INCL

#include <algorithm>
#include <stddef.h>
#include <stdint.h>
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
   TR_PersistentClassInfo(TR_OpaqueClassBlock *id) : _classId(id), _fieldInfo(0), _prexAssumptions(0), _timeStamp(0), _nameLength(-1)
    {
    uintptrj_t classPointerValue = (uintptrj_t) id;

    // Bit is switched on to mark it as uninitialized
    //
    _classId = (TR_OpaqueClassBlock *) ((classPointerValue | 1));
    }

   TR_OpaqueClassBlock *getClassId() { return (TR_OpaqueClassBlock *) (((uintptr_t) _classId) & ~(uintptr_t)1); }
   bool isInitialized()  { return ((((uintptr_t) _classId) & 1) == 0); }
   void setInitialized(TR_PersistentMemory *);

   // HCR
   void setClassId(TR_OpaqueClassBlock *newClass)
      {
      _classId = (TR_OpaqueClassBlock *) (((uintptrj_t)newClass) | (uintptrj_t)isInitialized());
      setClassHasBeenRedefined(true);
      }

   TR_SubClass *getFirstSubclass() { return _subClasses.getFirst(); }
   void setFirstSubClass(TR_SubClass *sc) { _subClasses.setFirst(sc); }

   void setVisited() {_visitedStatus |= 1; }
   void resetVisited() { _visitedStatus = _visitedStatus & ~(uintptrj_t)1; }
   bool hasBeenVisited()
    {
    if (_visitedStatus & 1)
       return true;
    return false;
    }

   TR_PersistentClassInfoForFields *getFieldInfo()
      {
      uintptrj_t fieldInfo = (uintptrj_t) _fieldInfo;
      fieldInfo = fieldInfo & ~(uintptrj_t)3;
      return (TR_PersistentClassInfoForFields *) fieldInfo;
      }
   void setFieldInfo(TR_PersistentClassInfoForFields *i)
      {
      uintptrj_t fieldInfo = (uintptrj_t) i;
      fieldInfo |= (((uintptrj_t) _fieldInfo) & (uintptrj_t)3);
      _visitedStatus = fieldInfo;
      }

   int32_t getSubClassesCount() { return _subClasses.getSize(); }

   TR_SubClass *addSubClass(TR_PersistentClassInfo *subClass);
   void removeSubClasses();
   void removeASubClass(TR_PersistentClassInfo *subClass);
   void removeUnloadedSubClasses();
   void setUnloaded(){_visitedStatus |= 0x2;}
   bool getUnloaded()
    {
    if (_visitedStatus & 0x2)
       return true;
    return false;
   }
   uint16_t getTimeStamp() { return _timeStamp; }
   int16_t getNumPrexAssumptions() { return _prexAssumptions; }
   void incNumPrexAssumptions() { _prexAssumptions++; }

   void setReservable(bool v = true)              { _flags.set(_isReservable, v); }
   bool isReservable()                            { return _flags.testAny(_isReservable); }

   void setShouldNotBeNewlyExtended(int32_t ID) { _shouldNotBeNewlyExtended.set(1 << ID); }
   void resetShouldNotBeNewlyExtended(int32_t ID){ _shouldNotBeNewlyExtended.reset(1 << ID); }
   void clearShouldNotBeNewlyExtended()          { _shouldNotBeNewlyExtended.clear(); }
   bool shouldNotBeNewlyExtended()               { return _shouldNotBeNewlyExtended.testAny(0xff); }
   bool shouldNotBeNewlyExtended(int32_t ID)     { return _shouldNotBeNewlyExtended.testAny(1 << ID); }
   flags8_t getShouldNotBeNewlyExtendedMask() const { return _shouldNotBeNewlyExtended; }

   void setHasRecognizedAnnotations(bool v = true){ _flags.set(_containsRecognizedAnnotations, v); }
   bool hasRecognizedAnnotations()                { return _flags.testAny(_containsRecognizedAnnotations); }
   void setAlreadyCheckedForAnnotations(bool v = true){ _flags.set(_alreadyScannedForAnnotations, v); }
   bool alreadyCheckedForAnnotations()            { return _flags.testAny(_alreadyScannedForAnnotations); }

   void setCannotTrustStaticFinal(bool v = true)  { _flags.set(_cannotTrustStaticFinal, v); }
   bool cannotTrustStaticFinal()                  { return _flags.testAny(_cannotTrustStaticFinal); }

   // HCR
   void setClassHasBeenRedefined(bool v = true)  { _flags.set(_classHasBeenRedefined, v); }
   bool classHasBeenRedefined()                  { return _flags.testAny(_classHasBeenRedefined); }

   int32_t getNameLength()                       { return _nameLength; }
   void setNameLength(int32_t length)            { _nameLength = length; }

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

   TR_OpaqueClassBlock               *_classId;
   TR_LinkHead0<TR_SubClass>          _subClasses;

   union
      {
      uintptrj_t _visitedStatus;
      TR_PersistentClassInfoForFields *_fieldInfo;
      };
   int16_t                             _prexAssumptions;
   uint16_t                            _timeStamp;
   int32_t                             _nameLength;
   flags8_t                            _flags;
   flags8_t                            _shouldNotBeNewlyExtended; // one bit for each possible compilation thread
   };

class TR_AddressRange
   {
   uintptrj_t _start;
   uintptrj_t _end; // start == end means only one address in this range

   public:
   TR_PERSISTENT_ALLOC(TR_MemoryBase::Assumption);
   TR_AddressRange()
#if defined(DEBUG)
      // Paint invalid fields
      :_start(0x321dead),
       _end(0x321dead)
#endif
      {}

   uintptrj_t getStart() { return _start; }
   uintptrj_t getEnd()   { return _end; }

   void initialize(uintptrj_t start, uintptrj_t end){ _start = start; _end = end; }
   void add(uintptrj_t start, uintptrj_t end){ _start = std::min(_start, start); _end = std::max(end, _end); }
   bool covers(uintptrj_t address){ return _start <= address && address <= _end; }

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
   void findCheapestRangesToMerge(uintptrj_t *cost, int32_t *lowerIndex);

   int32_t firstHigherAddressRangeIndex(uintptrj_t address); // the binary search

   public:


   TR_PERSISTENT_ALLOC(TR_Memory::Assumption);

   TR_AddressSet(TR_PersistentMemory *persistentMemory, int32_t maxAddressRanges):
      _addressRanges(new (persistentMemory) TR_AddressRange[maxAddressRanges]),
      _numAddressRanges(0),
      _maxAddressRanges(maxAddressRanges)
      {}

   void add        (uintptrj_t address){ add(address, address); }
   void add        (uintptrj_t start, uintptrj_t end);
   bool mayContain (uintptrj_t address)
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
   TR_UnloadedClassPicSite(TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size = sizeof(uintptrj_t))
      : OMR::ValueModifyRuntimeAssumption(pm, key), _picLocation(picLocation), _size(size)
         {}

   public:
   static TR_UnloadedClassPicSite *make(
      TR_FrontEnd *fe, TR_PersistentMemory * pm, uintptrj_t key, uint8_t *picLocation, uint32_t size,
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
   uintptrj_t getPicEntry() { return *((uintptrj_t*)_picLocation); }

   virtual void dumpInfo();

   private:
   uint8_t    *_picLocation;
   uint32_t    _size;
   };

#endif
