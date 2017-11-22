/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef VALUEPROFILER_INCL
#define VALUEPROFILER_INCL

#include <stdint.h>          // for uint32_t, int32_t, uint64_t
#include <stdio.h>           // for fwrite, NULL, FILE
#include "env/TRMemory.hpp"  // for TR_Memory, etc
#include "env/jittypes.h"    // for TR_ByteCodeInfo, etc
#include "il/DataTypes.hpp"  // for DataTypes
#include "infra/Monitor.hpp" // for TR::Monitor

extern TR::Monitor *vpMonitor;

#define MAX_UNLOCKED_PROFILING_VALUES 5

class TR_ResolvedMethod;
namespace TR { class Compilation; }
template <class T> class List;
template <class T> class ListElement;


class TR_ExtraAbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_ExtraAbstractInfo() {}

   uintptrj_t      _totalFrequency;
   uint32_t        _frequency;

   uint32_t getTotalFrequency(uintptrj_t **addrOfTotalFrequency = 0);
   };

class TR_ExtraValueInfo : public TR_ExtraAbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_ExtraValueInfo() {}

   uint32_t        _value;
   static TR_ExtraValueInfo *create(uint32_t value, uint32_t frequency = 0, uintptrj_t totalFrequency = 0);
   void incrementOrCreateExtraValueInfo(uint32_t value, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumProfiledValues);

   };

class TR_ExtraLongValueInfo : public TR_ExtraAbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_ExtraLongValueInfo() {}

   uint64_t        _value;
   static TR_ExtraLongValueInfo *create(uint64_t value, uint32_t frequency = 0, uintptrj_t totalFrequency = 0);
   void incrementOrCreateExtraLongValueInfo(uint64_t value, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumProfiledValues);

   };

class TR_ExtraBigDecimalValueInfo : public TR_ExtraAbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_ExtraBigDecimalValueInfo() {}

   int32_t        _scale;
   int32_t        _flag;

   static TR_ExtraBigDecimalValueInfo *create(int32_t scale, int32_t flag, uint32_t frequency = 0, uintptrj_t totalFrequency = 0);
   void incrementOrCreateExtraBigDecimalValueInfo(int32_t scale, int32_t flag, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumProfiledValues);
   };

class TR_ExtraStringValueInfo : public TR_ExtraAbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_ExtraStringValueInfo() {}

   char *_chars;
   int32_t _length;

   static TR_ExtraStringValueInfo *create(char *chars, int32_t length, uint32_t frequency = 0, uintptrj_t totalFrequency = 0);
   void incrementOrCreateExtraStringValueInfo(char *chars, int32_t length, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumProfiledValues);
   };

class TR_ExtraAddressInfo : public TR_ExtraAbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_ExtraAddressInfo() {}

   uintptrj_t        _value;
   static TR_ExtraAddressInfo *create(uintptrj_t value, uint32_t frequency = 0, uintptrj_t totalFrequency = 0);
   void incrementOrCreateExtraAddressInfo(uintptrj_t value, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumProfiledValues, uint32_t frequency = 0, bool externalProfileValue = false);

   };

class TR_AbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_AbstractInfo() {}
   virtual ~TR_AbstractInfo() {}

   uintptrj_t      _totalFrequency;
   uint32_t        _frequency1;
   TR_ByteCodeInfo _byteCodeInfo;
   TR_AbstractInfo   *_next;

   uint32_t getTotalFrequency(uintptrj_t **addrOfTotalFrequency = 0);
   virtual float    getTopProbability();
   virtual uint32_t getNumProfiledValues();
   virtual void print()=0;
   virtual void getSortedList(TR::Compilation *, List<TR_ExtraAbstractInfo> *sortedList, ListElement<TR_ExtraAbstractInfo> *listHead);
   void insertInSortedList(TR::Compilation *, TR_ExtraAbstractInfo *currValueInfo, ListElement<TR_ExtraAbstractInfo> *&listHead);

   virtual void * asAddressInfo() {return NULL;}
   virtual void * asWarmCompilePICAddressInfo() {return NULL;}
   virtual void * asBigDecimalValueInfo() { return NULL; }
   virtual void * asStringValueInfo() { return NULL; }
   };

class TR_ValueInfo : public TR_AbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_ValueInfo() {}

   uint32_t        _value1;
   uint32_t        _maxValue;

   void incrementOrCreateExtraValueInfo(uint32_t value, uintptrj_t **addrForTotalFrequency, uint32_t maxNumProfiledValues);
   uint32_t getTopValue();
   uint32_t getMaxValue() { return _maxValue; }
   uint32_t setMaxValue(uint32_t maxValue) { return _maxValue = maxValue; }
   void getSortedList(TR::Compilation *, List<TR_ExtraAbstractInfo> *sortedList);
   virtual void print();
   };

class TR_LongValueInfo : public TR_AbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_LongValueInfo() {}

   uint64_t        _value1;

   void incrementOrCreateExtraLongValueInfo(uint64_t value, uintptrj_t **addrForTotalFrequency, uint32_t maxNumProfiledValues);
   uint64_t getTopValue();
   void getSortedList(TR::Compilation *, List<TR_ExtraAbstractInfo> *sortedList);
   virtual void print();
   };

class TR_BigDecimalValueInfo : public TR_AbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_BigDecimalValueInfo() {}

   int32_t        _scale1;
   int32_t        _flag1;

   void incrementOrCreateExtraBigDecimalValueInfo(int32_t scale, int32_t flag, uintptrj_t **addrForTotalFrequency, uint32_t maxNumProfiledValues);
   int32_t getTopValue(int32_t &flag);

   void getSortedList(TR::Compilation *, List<TR_ExtraAbstractInfo> *sortedList);
   virtual void print();
   virtual void *asBigDecimalValueInfo() { return this; }
   };


class TR_AddressInfo : public TR_AbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_AddressInfo() {}

   uintptrj_t        _value1;

   void incrementOrCreateExtraAddressInfo(uintptrj_t value, uintptrj_t **addrForTotalFrequency, uint32_t maxNumProfiledValues, uint32_t frequency = 0, bool externalProfileValue = false);
   uintptrj_t getTopValue();
   void getSortedList(TR::Compilation *, List<TR_ExtraAbstractInfo> *sortedList);
   void getMethodsList(TR::Compilation *, TR_ResolvedMethod *callerMethod, TR_OpaqueClassBlock *calleeClass, int32_t vftSlot, List<TR_ExtraAbstractInfo> *sortedList);
   virtual void print();
   virtual void * asAddressInfo() {return this;}
   virtual void * asWarmCompilePICAddressInfo() {return NULL;}

private:
   struct ClassAndFrequency
      {
      ClassAndFrequency(
         TR_ExtraAddressInfo *cursor,
         TR_OpaqueClassBlock *methodClass,
         uint32_t frequency
         ) :
         cursor(cursor),
         methodClass(methodClass),
         frequency(frequency)
         {
         }
      TR_ExtraAddressInfo *cursor;
      TR_OpaqueClassBlock *methodClass;
      uint32_t frequency;
      };

   ClassAndFrequency getInitialClassAndFrequency();
   ClassAndFrequency getNextClassAndFrequency(const ClassAndFrequency &);
   ClassAndFrequency findEntry(uintptr_t totalFrequencyOrExtraInfo);
   };


class TR_StringValueInfo : public TR_AbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_StringValueInfo() {}
   // TODO: cleanup _chars1 in destructor if required

   char *_chars1;
   int32_t _length1;

   static bool matchStrings(char *chars1, int32_t length1, char *chars2, int32_t length2);
   static char *createChars(int32_t length);

   void incrementOrCreateExtraStringValueInfo(char *chars, int32_t length, uintptrj_t **addrForTotalFrequency, uint32_t maxNumProfiledValues);
   char *getTopValue(int32_t &length);

   void getSortedList(TR::Compilation *, List<TR_ExtraAbstractInfo> *sortedList);
   virtual void print();
   virtual void *asStringValueInfo() { return this; }
   };


class TR_WarmCompilePICAddressInfo : public TR_AbstractInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_WarmCompilePICAddressInfo()
      {
      for (int32_t i=0; i<MAX_UNLOCKED_PROFILING_VALUES; i++) _frequency[i]=0;
      }

   uintptrj_t      _address   [MAX_UNLOCKED_PROFILING_VALUES];
   int32_t         _frequency [MAX_UNLOCKED_PROFILING_VALUES];

   uintptrj_t   getTopValue();
   void         getSortedList(TR::Compilation *, List<TR_ExtraAbstractInfo> *sortedList);
   uint32_t     getNumProfiledValues();
   float        getTopProbability();
   virtual void print();
   virtual void * asAddressInfo() {return NULL;}
   virtual void * asWarmCompilePICAddressInfo() {return this;}
   };


#if defined(TR_HOST_POWER)
   void PPCinitializeValueProfiler();
#endif


#endif // VALUE_PROFILER_INCL
