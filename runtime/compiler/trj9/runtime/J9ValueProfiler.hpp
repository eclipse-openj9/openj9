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
#include "infra/CriticalSection.hpp" // for OMR::CriticalSection
#include "compile/Compilation.hpp"
#include "infra/vector.hpp"         // for TR::vector

extern TR::Monitor *vpMonitor;

#ifdef TR_HOST_64BIT
#define HIGH_ORDER_BIT (0x8000000000000000)
#else
#define HIGH_ORDER_BIT (0x80000000)
#endif

#define ARRAY_MAX_NUM_VALUES (5)
#define LINKEDLIST_MAX_NUM_VALUES (20)

class TR_ResolvedMethod;
template <class T> class List;
template <class T> class ListElement;

/**
 * Old versions of XLC cannot determine uintptr_t is equivalent
 * to uint32_t or uint64_t.
 */
#if defined(TR_HOST_POWER) || defined(TR_HOST_S390)
#if TR_HOST_64BIT
typedef uint64_t ProfileAddressType;
#else
typedef uint32_t ProfileAddressType;
#endif
#else
typedef uintptr_t ProfileAddressType;
#endif

/**
 * Profiling info consists of two class hierarchies: TR_AbstractInfo & TR_AbstractProfilerInfo
 *
 * TR_AbstractProfilerInfo is the persistent data structures use by the various profiling
 * implementations. They can manage a variety of basic types, such as uint32_t and uint64_t.
 * The class hierarchy, with T being uint32_t, uint64_t or TR_ByteInfo:
 *
 * TR_AbstractProfilerInfo
 *    TR_LinkedListProfilerInfo<T>
 *    TR_ArrayProfilerInfo<T>
 *
 * TR_AbstractInfo provides a common API for more complex data types, such as addresses and
 * BigDecimals, consistent across the various sources of profiling information.
 * The class hierarchy, with T being uint32_t, uint64_t or TR_ByteInfo:
 *
 * TR_AbstractInfo
 *    TR_GenericValueInfo<T>
 *       TR_AddressInfo
 *       TR_BigDecimalInfo
 */

/**
 * Kinds of value profiling information
 */
enum TR_ValueInfoKind
   {
   ValueInfo,
   LongValueInfo,
   AddressInfo,
   BigDecimalInfo,
   StringInfo,
   LastValueInfo,
   };

/**
 * Sources of JIT value profiling information
 * This excludes external sources, such as HW and the interpreter
 */
enum TR_ValueInfoSource
   {
   LinkedListProfiler,
   ArrayProfiler,
   HashTableProfiler,
   LastProfiler,
   };

/**
 * Profiling for a byte array.
 * Will perform a deep copy and compare.
 */
struct TR_ByteInfo
   {
   size_t length;
   char const *chars;

   TR_ByteInfo() : length(0), chars(NULL) {}
   TR_ByteInfo(char* source, size_t len) : length(len), chars(source) {}
   TR_ByteInfo(const TR_ByteInfo& orig);
   ~TR_ByteInfo();

   bool operator==(const TR_ByteInfo &orig) const
      {
      return orig.length != length ? false : memcmp(chars, orig.chars, length * sizeof(char)) == 0;
      }
   bool operator < (const TR_ByteInfo &orig) const
      {
      return length < orig.length;
      }
   };

/**
 * BigDecimal info for profiling.
 * Consists of two 32 bit integers, so it will be compressed into a single 64 bit
 * integer to reuse existing profilers.
 */
struct TR_BigDecimalInfo
   {
   union
      {
      uint64_t value;
      struct 
         {
         int32_t flag;
         int32_t scale;
         };
      };
   };

/**
 * Struct for returning value and frequency information
 */
template <typename T>
struct TR_ProfiledValue
   {
   T _value;
   uint32_t _frequency;

   bool operator > (const TR_ProfiledValue<T> &b) const
      {
      return (_frequency > b._frequency);
      }
   };

/**
 * Base class for persistent value profiling information at a BCI.
 *
 * Contains common methods for metadata, frequencies and debugging.
 * Also contains all the necessary methods to extract profiled values
 * for the basic types: uint32_t, uint64_t and TR_ByteInfo.
 */
class TR_AbstractProfilerInfo : public TR_Link0<TR_AbstractProfilerInfo>
   {
   public:
   TR_AbstractProfilerInfo(TR_ByteCodeInfo &bci) : _byteCodeInfo(bci) {}

   /**
    * Common methods based on metadata and frequencies.
    */
   TR_ByteCodeInfo& getByteCodeInfo() { return _byteCodeInfo; }
   TR_AbstractInfo* getAbstractInfo(TR::Region&);

   virtual TR_ValueInfoKind   getKind() = 0;
   virtual TR_ValueInfoSource getSource() = 0;
   virtual uint32_t getTotalFrequency() = 0;
   virtual uint32_t getNumProfiledValues() = 0;
   virtual void     dumpInfo(TR::FILE *logFile) = 0;

   /**
    * Basic methods for each type of value.
    * Shares defaults with empty profiling information.
    */
   virtual uint32_t getTopValue(uint32_t&) { return 0; }
   virtual uint32_t getMaxValue(uint32_t&) { return 0; }
   virtual void     getList(TR::vector<TR_ProfiledValue<uint32_t>, TR::Region&> &vec) { vec.clear(); }

   virtual uint32_t getTopValue(uint64_t&) { return 0; }
   virtual uint32_t getMaxValue(uint64_t&) { return 0; }
   virtual void     getList(TR::vector<TR_ProfiledValue<uint64_t>, TR::Region&> &vec) { vec.clear(); }

   virtual uint32_t getTopValue(TR_ByteInfo&) { return 0; };
   virtual uint32_t getMaxValue(TR_ByteInfo&) { return 0; };
   virtual void     getList(TR::vector<TR_ProfiledValue<TR_ByteInfo>, TR::Region&> &vec) { vec.clear(); }

   protected:
   TR_ByteCodeInfo _byteCodeInfo;
   };

/**
 * Linked list implementation of value profiling.
 * It will profile up to a set number of values, adding values as Element.
 */
template <typename T>
class TR_LinkedListProfilerInfo : public TR_AbstractProfilerInfo
   {
   public:
   TR_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_LinkedListProfilerInfo(TR_ByteCodeInfo &bci, TR_ValueInfoKind kind, T initialValue, uint32_t initialFreq, bool external = false) :
       TR_AbstractProfilerInfo(bci),
       _kind(kind),
       _first(initialValue, initialFreq, initialFreq),
       _external(external)
      {}
   ~TR_LinkedListProfilerInfo();

   /**
    * Element in linked list profiling implementation.
    * Contains value, frequency and either a next pointer or the total frequency.
    */
   class Element
      {
      public:
      TR_ALLOC(TR_Memory::PersistentProfileInfo)

      Element(T value, uint32_t frequency, uint32_t totalFrequency) :
          _value(value),
          _frequency(frequency),
          _totalFrequency((uintptr_t) totalFrequency)
         {
         TR_ASSERT(!((uintptr_t)this & 0x00000001), "Linked list pointer must be at least 2 byte aligned\n");
         }

      Element *getNext() { return (_totalFrequency & HIGH_ORDER_BIT) ? (Element *) (_totalFrequency << 1) : NULL; }
      void setNext(Element *next) { _totalFrequency = (((uintptr_t)next) >> 1) | HIGH_ORDER_BIT; }

      uintptr_t  _totalFrequency;
      uint32_t   _frequency;
      T          _value;
      };

   /**
    * Common methods
    */
   TR_ValueInfoKind   getKind()   { return _kind; }
   TR_ValueInfoSource getSource() { return _external ? LastProfiler : LinkedListProfiler; }
   uint32_t getTotalFrequency()   { return getTotalFrequency(NULL); }
   uint32_t getNumProfiledValues();
   void     dumpInfo(TR::FILE*);

   uint32_t getTopValue(T&);
   uint32_t getMaxValue(T&);
   void     getList(TR::vector<TR_ProfiledValue<T>, TR::Region&>&);

   /**
    * Runtime helpers
    */
   Element *getFirst() { return &_first; }
   uint32_t getTotalFrequency(uintptr_t **addrOfTotalFrequency);
   void     incrementOrCreate(T &value, uintptr_t **addrOfTotalFrequency, uint32_t maxNumValuesProfiled,
      uint32_t inc = 1, TR::Region *region = NULL);

   protected:
   bool             _external;
   TR_ValueInfoKind _kind;
   Element          _first;
   };

template <typename T>
TR_LinkedListProfilerInfo<T>::~TR_LinkedListProfilerInfo()
   {
   if (_external)
      return;

   OMR::CriticalSection lock(vpMonitor);

   auto cursor = getFirst()->getNext();
   while (cursor)
      {
      auto next = cursor->getNext();
      ~Element(cursor);
      jitPersistentFree(cursor);
      cursor = next;
      }
   }

/**
 * Get the number of profiled values with non-zero frequencies.
 */
template <typename T> uint32_t
TR_LinkedListProfilerInfo<T>::getNumProfiledValues()
   {
   OMR::CriticalSection lock(vpMonitor);

   size_t numProfiled = 0;
   for (auto iter = getFirst(); iter; iter = iter->getNext())
      {
      if (iter->_frequency > 0)
         numProfiled++;
      }

   return numProfiled;
   }

/**
 * Get the value with the highest frequency.
 *
 * \param value Reference to return value. Will not be modified if table is empty.
 * \return The frequency for the value.
 */
template <typename T> uint32_t
TR_LinkedListProfilerInfo<T>::getTopValue(T &value)
   {
   OMR::CriticalSection lock(vpMonitor);

   uint32_t topFrequency = 0;
   for (auto iter = getFirst(); iter; iter = iter->getNext())
      {
      if (iter->_frequency > topFrequency)
         {
         topFrequency = iter->_frequency;
         value = iter->_value;
         }
      }

   return topFrequency;
   }

/**
 * Get the largest value.
 *
 * \param value Reference to return value. Will not be modified if table is empty.
 * \return The frequency for the value.
 */
template <typename T> uint32_t
TR_LinkedListProfilerInfo<T>::getMaxValue(T &value)
   {
   OMR::CriticalSection lock(vpMonitor);

   uint32_t topFrequency = 0;
   for (auto iter = getFirst(); iter; iter = iter->getNext())
      {
      if (topFrequency == 0 || value < iter->_value)
         {
         topFrequency = iter->_frequency;
         value = iter->_value;
         }
      }

   return topFrequency;
   }

/**
 * Stash the values and frequencies into a vector.
 * 
 * \param vec Vector to fill. Will be cleared.
 * \return Reference to vector.
 */
template <typename T> void
TR_LinkedListProfilerInfo<T>::getList(TR::vector<TR_ProfiledValue<T>, TR::Region&> &vec)
   {
   OMR::CriticalSection lock(vpMonitor);

   vec.clear();
   vec.resize(getNumProfiledValues());
   size_t i = 0;
   for (auto iter = getFirst(); iter; iter = iter->getNext())
      { 
      if (iter->_frequency > 0)
         {
         vec[i]._value = iter->_value;
         vec[i++]._frequency = iter->_frequency;
         }
      }
   }

/**
 * Trace properties and content.
 * Custom trace for TR_ByteInfo, with hex print for other types.
 */
template <> void
TR_LinkedListProfilerInfo<TR_ByteInfo>::dumpInfo(TR::FILE *logFile);

template <typename T> void
TR_LinkedListProfilerInfo<T>::dumpInfo(TR::FILE *logFile)
   {
   OMR::CriticalSection lock(vpMonitor);

   trfprintf(logFile, "   Linked List Profiling Info %p\n", this);
   trfprintf(logFile, "   Kind: %d BCI: %d:%d\n Values:\n", _kind,
      TR_AbstractProfilerInfo::getByteCodeInfo().getCallerIndex(),
      TR_AbstractProfilerInfo::getByteCodeInfo().getByteCodeIndex());

   size_t count = 0;
   size_t padding = 2 * sizeof(T) + 2;
   for (auto iter = getFirst(); iter; iter = iter->getNext())
      trfprintf(logFile, "    %d: %d %0*x", count++, iter->_frequency, padding, iter->_value); 

   trfprintf(logFile, "   Num: %d Total Frequency: %d\n", count, getTotalFrequency());
   }

/**
 * Get the total frequency.
 *
 * \param addrOfTotalFrequency Optional pointer to the address of the total frequency.
 * \return The total frequency.
 */
template <typename T> uint32_t
TR_LinkedListProfilerInfo<T>::getTotalFrequency(uintptr_t **addrOfTotalFrequency)
   {
   OMR::CriticalSection lock(vpMonitor);
   
   uintptr_t *addr = NULL;
   for (auto cursor = getFirst(); cursor; cursor = cursor->getNext())
      addr = &cursor->_totalFrequency;

   if (addrOfTotalFrequency)
      *addrOfTotalFrequency = addr;
   return (uint32_t) *addr; 
   } 

/**
 * Add a value to the list if there is room and its not already present.
 * If it is present, increment the matched values frequency.
 *
 * \param value The value to search for. Will be copied when adding to the list.
 * \param addrOfTotalFrequency Address of the total frequency, in case its already known.
 * \param maxNumValuesProfiled The number of values the list can grow by.
 */
template <typename T> void
TR_LinkedListProfilerInfo<T>::incrementOrCreate(T &value, uintptr_t **addrOfTotalFrequency, uint32_t maxNumValuesProfiled,
    uint32_t inc, TR::Region *region)
   {
   OMR::CriticalSection incrementingOrCreatingExtraAddressInfo(vpMonitor);

   uintptr_t totalFrequency;
   if (*addrOfTotalFrequency)
      totalFrequency = **addrOfTotalFrequency;
   else
      totalFrequency = getTotalFrequency(addrOfTotalFrequency);

   int32_t numDistinctValuesProfiled = 0;
   Element *cursorExtraInfo = getFirst()->getNext();
   while (cursorExtraInfo)
      {
      if ((cursorExtraInfo->_value == value) || (cursorExtraInfo->_frequency == 0))
         {
         if (cursorExtraInfo->_frequency == 0)
            cursorExtraInfo->_value = T(value);

         cursorExtraInfo->_frequency += inc;
         totalFrequency += inc;
         **addrOfTotalFrequency = totalFrequency;
         return;
         }

      numDistinctValuesProfiled++;
      cursorExtraInfo = cursorExtraInfo->getNext();
      }

   if (!cursorExtraInfo)
      cursorExtraInfo = getFirst();

   maxNumValuesProfiled = std::min<uint32_t>(maxNumValuesProfiled, LINKEDLIST_MAX_NUM_VALUES);
   if (numDistinctValuesProfiled <= maxNumValuesProfiled)
      {
      totalFrequency += inc;
      Element *newExtraInfo = region ? new (*region) Element(value, inc, totalFrequency) :
         new (PERSISTENT_NEW) Element(value, inc, totalFrequency);

      if (newExtraInfo)
         {
         cursorExtraInfo->setNext(newExtraInfo);
         cursorExtraInfo = newExtraInfo;
         }
      else
         cursorExtraInfo->_totalFrequency = totalFrequency;
      }
   else
      {
      totalFrequency += inc;
      **addrOfTotalFrequency = totalFrequency;
      }

   *addrOfTotalFrequency = &(cursorExtraInfo->_totalFrequency);
   }

/**
 * Array value profiling implemenation
 * It will profile up to ARRAY_MAX_NUM_VALUES values and maintain a total frequency counter.
 */
template <typename T>
class TR_ArrayProfilerInfo : public TR_AbstractProfilerInfo
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_ArrayProfilerInfo(TR_ByteCodeInfo &bci, TR_ValueInfoKind kind) :
       TR_AbstractProfilerInfo(bci),
       _kind(kind)
      {
      memset(_frequencies, 0, sizeof(uint32_t) * ARRAY_MAX_NUM_VALUES);
      }

   /**
    * Common methods
    */
   TR_ValueInfoKind getKind()     { return _kind; }
   TR_ValueInfoSource getSource() { return ArrayProfiler; }
   uint32_t getTotalFrequency()   { return _totalFrequency; }
   uint32_t getNumProfiledValues();
   void     dumpInfo(TR::FILE*);

   uint32_t getTopValue(T&);
   uint32_t getMaxValue(T&);
   void     getList(TR::vector<TR_ProfiledValue<T>, TR::Region&>&);

   /**
    * Runtime helpers
    */
   size_t   getSize() { return ARRAY_MAX_NUM_VALUES; }
   void     incrementOrCreate(const T &value);

   protected:
   TR_ValueInfoKind _kind;
   uint32_t _totalFrequency;
   uint32_t _frequencies[ARRAY_MAX_NUM_VALUES];
   T        _values[ARRAY_MAX_NUM_VALUES];
   };

/**
 * Get the number of profiled values with non-zero frequencies.
 */
template <typename T> uint32_t
TR_ArrayProfilerInfo<T>::getNumProfiledValues()
   {
   OMR::CriticalSection lock(vpMonitor);

   size_t numProfiled = 0;
   for (size_t i = 0; i < getSize(); ++i)
      {
      if (_frequencies[i] > 0)
         numProfiled++;
      }

   return numProfiled;
   }

/**
 * Get the value with the highest frequency.
 *
 * \param value Reference to return value. Will not be modified if table is empty.
 * \return The frequency for the value.
 */
template <typename T> uint32_t
TR_ArrayProfilerInfo<T>::getTopValue(T &value)
   {
   OMR::CriticalSection lock(vpMonitor);

   uint32_t topFrequency = 0;
   for (size_t i = 0; i < getSize(); ++i)
      {
      if (_frequencies[i] > topFrequency)
         {
         topFrequency = _frequencies[i];
         value = _values[i];
         }
      }

   return topFrequency;
   }

/**
 * Get the largest value.
 *
 * \param value Reference to return value. Will not be modified if table is empty.
 * \return The frequency for the value.
 */
template <typename T> uint32_t
TR_ArrayProfilerInfo<T>::getMaxValue(T &value)
   {
   OMR::CriticalSection lock(vpMonitor);

   uint32_t topFrequency = 0;
   for (size_t i = 0; i < getSize(); ++i)
      {
      if (topFrequency == 0 || value < _values[i])
         {
         topFrequency = _frequencies[i];
         value = _values[i];
         }
      }

   return topFrequency;
   }

/**
 * Stash the values and frequencies into a vector.
 * 
 * \param vec Vector to fill. Will be cleared.
 * \return Reference to vector.
 */
template <typename T> void
TR_ArrayProfilerInfo<T>::getList(TR::vector<TR_ProfiledValue<T>, TR::Region&> &vec)
   {
   OMR::CriticalSection lock(vpMonitor);

   vec.clear();
   vec.resize(getNumProfiledValues());
   size_t j = 0;
   for (size_t i = 0; i < getSize(); ++i)
      { 
      if (_frequencies[i] > 0)
         {
         vec[j]._value = _values[i];
         vec[j++]._frequency = _frequencies[i];
         }
      }
   }

/**
 * Trace properties and content.
 */
template <typename T> void
TR_ArrayProfilerInfo<T>::dumpInfo(TR::FILE *logFile)
   {
   OMR::CriticalSection lock(vpMonitor);

   trfprintf(logFile, "   Array Profiling Info %p\n", this);
   trfprintf(logFile, "   Kind: %d BCI: %d:%d\n Values:\n", _kind,
      TR_AbstractProfilerInfo::getByteCodeInfo().getCallerIndex(),
      TR_AbstractProfilerInfo::getByteCodeInfo().getByteCodeIndex());

   size_t count = 0;
   size_t padding = 2 * sizeof(T) + 2;
   for (size_t i = 0; i < getSize(); ++i)
      trfprintf(logFile, "    %d: %d %0*x", count++, _frequencies[i], padding, _values[i]);

   trfprintf(logFile, "   Num: %d Total Frequency: %d\n", count, getTotalFrequency());
   }

/**
 * Increment or add a value to the array.
 * This will walk the contents and search for either a match or an empty slot.
 * It assumes the filled slots are continuous from the start.
 * If the table is full, only the total frequency is incremented.
 *
 * \param value The value to add.
 */
template <typename T> void
TR_ArrayProfilerInfo<T>::incrementOrCreate(const T &value)
   {
   OMR::CriticalSection increment(vpMonitor);

   // Check if highest bit set, halfway to overflow
   if (_totalFrequency > (((size_t)-1) >> 1))
      {
      TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
      return;
      }

   for (size_t i = 0; i < getSize(); ++i)
      {
      if (value == _values[i])
         {
         _frequencies[i]++;
         break;
         }
      else if (_frequencies[i] == 0)
         {
         _values[i] = value;
         _frequencies[i]++;
         break;
         }
      }
   
   _totalFrequency++;
   }

/**
 * Abstract value information at a BCI
 */
class TR_AbstractInfo : public TR_Link0<TR_AbstractInfo>
   {
   public:
   static TR_AbstractInfo* create(TR_AbstractProfilerInfo *info, TR::Region *region);

   virtual TR_ByteCodeInfo& getByteCodeInfo() = 0;
   virtual uint32_t getTotalFrequency() = 0;
   virtual float    getTopProbability() = 0;
   virtual uint32_t getNumProfiledValues() = 0;
   };

/**
 * Abstract value information at a BCI for a type T.
 *
 * This contains common functionality, such as extracting the top
 * value and probability.
 */
template <typename T>
class TR_GenericValueInfo : public TR_AbstractInfo
   {
   public:
   TR_GenericValueInfo(TR_AbstractProfilerInfo *profiler) : _profiler(profiler)
      {}

   /**
    * Types for list methods
    */
   typedef TR_ProfiledValue<T> ProfiledValue;
   typedef TR::vector<TR_ProfiledValue<T>, TR::Region&> Vector;

   /**
    * Common methods
    */
   TR_ByteCodeInfo& getByteCodeInfo()      { return _profiler->getByteCodeInfo(); }
   uint32_t         getTotalFrequency()    { return _profiler->getTotalFrequency(); }
   uint32_t         getNumProfiledValues() { return _profiler->getNumProfiledValues(); }
   uint32_t         getTopValue(T &value)  { return _profiler->getTopValue(value); }
   uint32_t         getMaxValue(T &value)  { return _profiler->getMaxValue(value); }

   float getTopProbability();
   void  getSortedList(Vector &vec);

   /**
    * Old API methods
    */
   T     getTopValue();
   T     getMaxValue();
   void  getSortedList(TR::Compilation *, List<ProfiledValue> *sortedList);

   TR_AbstractProfilerInfo *getProfiler() { return _profiler; }

   protected:
   struct DescendingSort
      {
      bool operator()(const ProfiledValue &a, const ProfiledValue &b) const
         {
         return a > b;
         }
      };
   TR_AbstractProfilerInfo *_profiler;
   };

/**
 * Specialized abstract information for an address.
 * Adds functionality to generate a method list.
 */
class TR_AddressInfo : public TR_GenericValueInfo<ProfileAddressType>
   {
   public:
   TR_AddressInfo(TR_AbstractProfilerInfo *profiler) :
       TR_GenericValueInfo<ProfileAddressType>(profiler)
      {}

   typedef TR_ProfiledValue<TR_ResolvedMethod*> ProfiledMethod;
   typedef TR::vector<ProfiledMethod, TR::Region&> MethodVector;
   void getMethodsList(TR::Compilation *, TR_ResolvedMethod *callerMethod, TR_OpaqueClassBlock *calleeClass, int32_t vftSlot, MethodVector &sortedList);

   void getMethodsList(TR::Compilation *, TR_ResolvedMethod *callerMethod, TR_OpaqueClassBlock *calleeClass, int32_t vftSlot, List<ProfiledMethod> *sortedList);
   };

/**
 * Specialized abstract information for BigDecimal.
 * Adds functionality to extract flag and scale.
 */
class TR_BigDecimalValueInfo : public TR_GenericValueInfo<uint64_t>
   {
   public:
   TR_BigDecimalValueInfo(TR_AbstractProfilerInfo *profiler) :
       TR_GenericValueInfo<uint64_t>(profiler)
      {}

   int32_t getTopValue(int32_t&);
   };

/**
 * Typedefs for instansiations
 */
typedef TR_GenericValueInfo<uint32_t>      TR_ValueInfo;
typedef TR_GenericValueInfo<uint64_t>      TR_LongValueInfo;
typedef TR_GenericValueInfo<TR_ByteInfo>   TR_StringValueInfo;

typedef TR_ValueInfo::ProfiledValue           TR_ExtraValueInfo;
typedef TR_LongValueInfo::ProfiledValue       TR_ExtraLongValueInfo;
typedef TR_AddressInfo::ProfiledValue         TR_ExtraAddressInfo;
typedef TR_StringValueInfo::ProfiledValue     TR_ExtraStringValueInfo;
typedef TR_BigDecimalValueInfo::ProfiledValue TR_ExtraBigDecimalInfo;

/**
 * Get the top probability. Returns 0 if empty.
 */
template <typename T> float
TR_GenericValueInfo<T>::getTopProbability()
   {
   T tmp;
   uint32_t total = getTotalFrequency();
   if (total == 0)
      return 0;
   return ((float)getTopValue(tmp))/total;
   }

/**
 * Get a copy of the value with the highest frequency.
 * Replicates behaviour of prior API, defaults to 0 if the table is empty.
 */
template <typename T> T
TR_GenericValueInfo<T>::getTopValue()
   {
   T temp;
   return getTopValue(temp) > 0 ? temp : 0;
   }

/**
 * Get a copy of the value with the highest frequency.
 * Replicates behaviour of prior API, defaults to 0 if the table is empty.
 */
template <typename T> T
TR_GenericValueInfo<T>::getMaxValue()
   {
   T temp;
   return getMaxValue(temp) > 0 ? temp : 0;
   }

/**
 * Stash the values and frequencies into a vector and sort it.
 * Sort is ordered by frequency descending.
 * 
 * \param region Region to allocate vector in.
 * \return Reference to sorted vector.
 */
template <typename T> void
TR_GenericValueInfo<T>::getSortedList(Vector &vec)
   {
   _profiler->getList(vec);
   std::sort(vec.begin(), vec.end(), DescendingSort()); 
   }

/**
 * Generate a sorted list in the current stack region.
 *
 * \param comp Current compilation.
 * \param sortedList List will be modified to hold sorted result.
 */
template <typename T> void
TR_GenericValueInfo<T>::getSortedList(TR::Compilation *comp, List<ProfiledValue> *sortedList)
   {
   ListElement<ProfiledValue> *listHead = NULL, *tail = NULL;

   TR::Region &region = comp->trMemory()->currentStackRegion();
   auto vec = new (region) Vector(region);
   getSortedList(*vec);

   for (auto iter = vec->begin(); iter != vec->end(); ++iter)
      {
	ListElement<ProfiledValue> *newProfiledValue = new (comp->trStackMemory()) ListElement<ProfiledValue>(&(*iter));
      if (tail)
         tail->setNextElement(newProfiledValue);
      else
         listHead = newProfiledValue;
      tail = newProfiledValue;
      }

   sortedList->setListHead(listHead);
   }

#if defined(TR_HOST_POWER)
   void PPCinitializeValueProfiler();
#endif

#endif // VALUEPROFILER_INCL
