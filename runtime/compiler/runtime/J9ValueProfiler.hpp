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

#ifndef VALUEPROFILER_INCL
#define VALUEPROFILER_INCL

#include <stdint.h>
#include <stdio.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "infra/CriticalSection.hpp"
#include "compile/Compilation.hpp"
#include "infra/vector.hpp"
#include "omrformatconsts.h"

// Global lock used by Array & List profilers
extern TR::Monitor *vpMonitor;

#ifdef TR_HOST_64BIT
#define HIGH_ORDER_BIT (0x8000000000000000)
#else
#define HIGH_ORDER_BIT (0x80000000)
#endif

/**
 * These enums specify the maximum size for each implementation.
 * For array and linked list, these are the number of entires they can hold.
 * For the hash table, size is based on the number of bits in the index.
 */
#define ARRAY_MAX_NUM_VALUES (5)
#define LINKEDLIST_MAX_NUM_VALUES (20)
#define HASHTABLE_MAX_BITS  (8)

class TR_ResolvedMethod;
template <class T> class List;
template <class T> class ListElement;

/**
 * Clang and old versions of XLC cannot determine uintptr_t is
 * equivalent to uint32_t or uint64_t.
 */
#if defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(OSX)
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
 *    TR_AbstractHashTableProfilerInfo
 *       TR_HashTableProfilerInfo<T>
 *          TR_EmbeddedHashTable<T>
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

/*
 * Types of hash functions
 *
 * BitShiftHash: The hash function consists of a series of shifts on the original value, which are then ORed together
 * BitMaskHash:  The nominated bits in a mask are extracted and then gathered into the low bits to form an index
 * BitIndexHash: The hash function nominates 8 bit indices, these bits are then extracted to form a byte
 */
enum TR_HashFunctionType
   {
   BitShiftHash = 0x00,
   BitMaskHash  = 0x01,
   BitIndexHash = 0x02,
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
union TR_BigDecimalInfo
   {
   uint64_t value;
   struct
      {
      int32_t flag;
      int32_t scale;
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

   virtual ~TR_AbstractProfilerInfo() {}
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
 * Hash table abstract profiler
 *
 * Provides common methods for generating the instrumentation
 */
class TR_AbstractHashTableProfilerInfo : public TR_AbstractProfilerInfo
   {
   public:
   TR_AbstractHashTableProfilerInfo(TR_ByteCodeInfo &bci, size_t bits, TR_HashFunctionType hash, TR_ValueInfoKind kind) :
       TR_AbstractProfilerInfo(bci)
      {
      _metaData.lock = 0;
      _metaData.full = 0;
      _metaData.bits = bits;
      _metaData.hash = hash;
      _metaData.kind = kind;
      }

   /**
    * Table properties
    */
   size_t getBits() { return _metaData.bits; }
   size_t getSize() { return 1 << _metaData.bits; }
   bool   isFull()  { return _metaData.full == 1; }
   size_t getCapacity() { return 1 + _metaData.bits; }
   size_t getOtherIndex() { return _metaData.otherIndex < 0 ? ~_metaData.otherIndex : _metaData.otherIndex; }
   TR_HashFunctionType getHashType() { return static_cast<TR_HashFunctionType>(_metaData.hash); }
   TR_ValueInfoKind   getKind()   { return static_cast<TR_ValueInfoKind>(_metaData.kind); }
   TR_ValueInfoSource getSource() { return HashTableProfiler; }

   // JProfile thread helper
   virtual bool resetLowFreqKeys() = 0;

   /**
    * Table layout
    */
   uintptr_t getBaseAddress() { return (uintptr_t) this; }
   virtual size_t getLockOffset() = 0;
   virtual size_t getHashOffset() = 0;
   virtual size_t getKeysOffset()  = 0;
   virtual size_t getFreqOffset() = 0;
   virtual TR::DataType getDataType() = 0;

   protected:
   /**
    * There are three contexts from which these maps will be accessed:
    * 1) The jitted code, which will increment freqs and call the runtime helper.
    * 2) The runtime helper, which will add keys to the map or reset it.
    * 3) A compilation, which will read information, but should not modify it.
    *
    * To reduce overhead, there are two different forms of synchronization used in the map updates.
    *
    * First, jitted code will perform no synchronization for its freq increments, as this would incur
    * a high overhead. However, it may decide to call the runtime helper to add a value to the
    * table, with a quick test before hand to avoid the call overhead if possible. This test will
    * check if another thread is updating the table or if the table is full, avoiding the call if so.
    * This is implemented as a load of otherIndex and a comparison with 0. If less than 0,
    * the table can be modified. Otherwise, the loaded value is used to increment freqs[otherIndex].
    *
    * Second, synchronization is needed between the runtime helper and accesses in a recompilation.
    * The otherIndex cannot be used, as it cannot distinguish between a full or locked table.
    * Instead an additional lock word is used.
    *
    * These synchronization functions will manage the lock, with a flag to manipulate otherIndex.
    * lock and otherIndex should not be modified otherwise.
    */
   void lock();
   bool tryLock(bool disableJITAccess = false);
   void unlock(bool enableJITAccess = false);

   /**
    * Metadata for the map
    * Should be 32 bits, as it is used for atomic compare and swaps on the otherIndex byte.
    */
   union MetaData
      {
      struct
         {
         int16_t otherIndex;
         uint8_t lock : 3;
         uint8_t full : 1;
         uint8_t bits : 4;
         uint8_t hash : 4;
         uint8_t kind : 4;
         };
      uint32_t rawData;
      };

   MetaData _metaData;
#if defined(TR_TARGET_X86)
   static_assert(sizeof(MetaData) == sizeof(uint32_t),  "Synchronization on lock requires _metaData to be 32 bit");
#endif
   };

/**
 * Hash table implementation of value profiling
 *
 * Instrumentation will generate a hash and check for a match in jitted code.
 * Collisions are managed by the runtime helper.
 */
template <typename T>
class TR_HashTableProfilerInfo : public TR_AbstractHashTableProfilerInfo
   {
   public:
   TR_HashTableProfilerInfo(TR_ByteCodeInfo &bci, size_t bits, TR_HashFunctionType hash, TR_ValueInfoKind kind) :
       TR_AbstractHashTableProfilerInfo(bci, bits, hash, kind)
      {}

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
   size_t getLockOffset() { return offsetof(TR_HashTableProfilerInfo<T>, _metaData.otherIndex); }
   size_t getHashOffset() { return offsetof(TR_HashTableProfilerInfo<T>, _hashConfig); }
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

   TR::DataType getDataType() { return sizeof(T) <= 4 ? TR::Int32 : TR::Int64; }

   /**
    * Common methods
    */
   uint32_t getTotalFrequency();
   uint32_t getNumProfiledValues();
   void     dumpInfo(TR::FILE *logFile);
   uint32_t getTopValue(T&);
   uint32_t getMaxValue(T&);
   void     getList(TR::vector<TR_ProfiledValue<T>, TR::Region&>&);

   /**
    * Runtime helper, add a value to the table.
    *
    * This is a virtual method, which will impact performance at runtime, but
    * its the only one made by the helper.
    */
   virtual void addKey(T value) = 0;

   /**
    * Runtime helper, update frequency of the value
    * 
    * This helper function will be called at runtime by jit compiled code to
    * increment the frequency of the value in the table.
    */
   virtual void updateFrequency(T value) = 0;

   protected:
   virtual uint32_t* getFrequencies() = 0;
   virtual T* getKeys() = 0;

   /**
    * Dynamically updated hash configuration, modified by addKey()
    */
   union HashFunction
      {
      T mask;
      uint8_t shifts[HASHTABLE_MAX_BITS];
      uint8_t indices[HASHTABLE_MAX_BITS];
      };

   size_t  initialHashConfig(HashFunction &hash, T value);
   void    updateHashConfig(HashFunction &hash, T mask);
   size_t  applyHash(HashFunction &hash, T value);

   HashFunction _hashConfig;
#if defined(TR_TARGET_X86)
   static_assert(sizeof(T) <= 8, "Not expected to support values larger than 8 bytes");
#endif
   };

/**
 * Hash map with embedded arrays for keys and frequencies, based
 * on the desired table size.
 */
template <typename T, size_t bits>
class TR_EmbeddedHashTable : public TR_HashTableProfilerInfo<T>
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentProfileInfo)

   TR_EmbeddedHashTable<T, bits>(TR_ByteCodeInfo &bci, TR_HashFunctionType hash, TR_ValueInfoKind kind) :
       TR_HashTableProfilerInfo<T>(bci, bits, hash, kind)
      {
      reset();
      // Set the first other index to be the last slot, based on the real size
      this->_metaData.otherIndex = ~(((int16_t)_realSize) - 1);
      }

   bool resetLowFreqKeys();

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
   size_t getKeysOffset() { return offsetof(this_t, _keys); }
   size_t getFreqOffset() { return offsetof(this_t, _freqs); }
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__) || defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

   protected:
   typedef TR_EmbeddedHashTable<T, bits> this_t;
   typedef typename TR_HashTableProfilerInfo<T>::HashFunction HashFunction;

   /**
    * Runtime helpers
    */
   void addKey(T value);
   void updateFrequency(T value);
   T    recursivelySplit(T mask, T choices);
   void rearrange(HashFunction &hash);
   void reset();

   T        *getKeys() { return _keys; }
   uint32_t *getFrequencies() { return _freqs; }

   /**
    * Embedded key and frequency arrays.
    * Real size includes the 'other' counter, which will reuse a slot or extend the table.
    */
   const static size_t _realSize = bits < 2 ? (bits + 2) : (1 << bits);
   T        _keys[_realSize];
   uint32_t _freqs[_realSize];

   /**
    * To avoid accidental matches in the middle of rearrangement, which could
    * increment the number of values beyond the capacity, slots have default values
    * that should be unmatchable. For slot 0, this is -1. For all other slots, this is 0.
    * Due to how the hash function works, 0 should only be placed in the first slot and
    * -1 should be placed in any but the first, preventing all matches.
    *
    * These are helper methods to assist in maintaining this constraint.
    */
   T getDefault(size_t i) { return i == 0 ? -1 :  0; }

#if defined(TR_TARGET_X86)
   static_assert(bits <= HASHTABLE_MAX_BITS,  "Hash table exceeded max bits");
#endif
   };

/**
 * Get the total frequency.
 */
template <typename T> uint32_t
TR_HashTableProfilerInfo<T>::getTotalFrequency()
   {
   uint32_t *freqs = getFrequencies();

   lock();

   uint32_t totalFrequency = freqs[getOtherIndex()];
   for (size_t i = 0; i < getSize(); ++i)
      {
      if (freqs[i] > 0 && i != getOtherIndex())
         totalFrequency += freqs[i];
      }

   unlock();

   return totalFrequency;
   }

/**
 * Get the number of profiled values with non-zero frequencies.
 */
template <typename T> uint32_t
TR_HashTableProfilerInfo<T>::getNumProfiledValues()
   {
   uint32_t *freqs = getFrequencies();
   uint32_t numProfiled = 0;

   lock();

   for (size_t i = 0; i < getSize(); ++i)
      {
      if (freqs[i] > 0 && i != getOtherIndex())
         numProfiled++;
      }

   unlock();

   return numProfiled;
   }

/**
 * Get the value with the highest frequency.
 *
 * \param value Reference to return value. Will not be modified if table is empty.
 * \return The frequency for the value.
 */
template <typename T> uint32_t
TR_HashTableProfilerInfo<T>::getTopValue(T &value)
   {
   uint32_t topFrequency = 0;
   uint32_t *freqs = getFrequencies();
   T *values = getKeys();

   lock();

   for (size_t i = 0; i < getSize(); ++i)
      {
      if (freqs[i] > topFrequency && i != getOtherIndex())
         {
         topFrequency = freqs[i];
         value = values[i];
         }
      }

   unlock();

   return topFrequency;
   }

/**
 * Get the largest value.
 *
 * \param value Reference to return value. Will not be modified if table is empty.
 * \return The frequency for the value.
 */
template <typename T> uint32_t
TR_HashTableProfilerInfo<T>::getMaxValue(T &value)
   {
   uint32_t topFrequency = 0;
   uint32_t *freqs = getFrequencies();
   T *values = getKeys();

   lock();

   for (size_t i = 0; i < getSize(); ++i)
      {
      if (freqs[i] > 0 && i != getOtherIndex())
         {
         if (topFrequency == 0 || values[i] > value)
            {
            topFrequency = freqs[i];
            value = values[i];
            }
         }
      }

   unlock();

   return topFrequency;
   }

/**
 * Stash the values and frequencies into a vector.
 *
 * \param vec Vector to fill. Will be cleared.
 * \return Reference to vector.
 */
template <typename T> void
TR_HashTableProfilerInfo<T>::getList(TR::vector<TR_ProfiledValue<T>, TR::Region&> &vec)
   {
   uint32_t numProfiled = 0;
   uint32_t *freqs = getFrequencies();
   T *values = getKeys();

   lock();

   for (size_t i = 0; i < getSize(); ++i)
      {
      if (freqs[i] > 0 && i != getOtherIndex())
         numProfiled++;
      }

   vec.clear();
   vec.resize(numProfiled);
   size_t j = 0;
   for (size_t i = 0; i < getSize(); ++i)
      {
      if (freqs[i] > 0 && i != getOtherIndex())
         {
         vec[j]._value = values[i];
         vec[j++]._frequency = freqs[i];
         }
      }

   unlock();
   }

/**
 * Trace properties and content.
 */
template <typename T> void
TR_HashTableProfilerInfo<T>::dumpInfo(TR::FILE *logFile)
   {
   uint32_t *freqs = getFrequencies();
   T *values = getKeys();
   uint32_t totalFreq = getTotalFrequency();

   lock();

   trfprintf(logFile, "\n   Hash Map Profiling Info %p\n", this);
   trfprintf(logFile, "   Bits: %d OtherIndex: %d\n", getBits(), getOtherIndex());
   trfprintf(logFile, "   Kind: %d BCI: %d:%d\n   Values:\n", getKind(),
      TR_AbstractProfilerInfo::getByteCodeInfo().getCallerIndex(),
      TR_AbstractProfilerInfo::getByteCodeInfo().getByteCodeIndex());

   size_t count = 0, seen = 0;
   size_t padding = 2 * sizeof(T);
   for (size_t i = 0; i < getSize(); ++i)
      {
      if (i == getOtherIndex())
         {
         trfprintf(logFile, "    %d: %d OTHER\n", count++, freqs[i]);
         }
      else if (freqs[i] == 0)
         {
         trfprintf(logFile, "    %d: -\n", count++);
         }
      else
         {
         trfprintf(logFile, "    %d: %d 0x%0*llX\n", count++, freqs[i], padding, values[i]);
         seen++;
         }
      }

   trfprintf(logFile, "   Num: %d Total Frequency: %d\n", seen, totalFreq);

   trfprintf(logFile, "   HashFunction: ");
   if (_metaData.hash == BitShiftHash || _metaData.hash == BitIndexHash)
      {
      trfprintf(logFile, "%s\n", _metaData.hash == BitShiftHash ? "Shift" : "Index");
      for (uint8_t i = 0; i < getBits(); ++i)
         trfprintf(logFile, "    %01d : %03d - 0x%0*llX\n", i, _hashConfig.shifts[i], padding, 1 << (_hashConfig.shifts[i] + (_metaData.hash == BitShiftHash ? i : 0)));
      }
   else
      trfprintf(logFile, "Mask\n    0x%0*llX\n", padding, _hashConfig.mask);

   trfprintf(logFile, "\n");
   unlock();
   }

/**
 * Determine whether the table should be reset or not, due to a selection
 * of bad values. Will reset the table if necessary.
 *
 * \return True if the table was reset.
 */
template <typename T, size_t bits> bool
TR_EmbeddedHashTable<T, bits>::resetLowFreqKeys()
   {
   uint32_t matched = 0;
   uint32_t other = _freqs[this->getOtherIndex()];
   for (size_t i = 0; i < _realSize; ++i)
      {
      if (i != this->getOtherIndex())
         matched += _freqs[i];
      }

   // Reset if matched accounts for less than 1/3 of total
   if (2 * matched < other)
      {
      this->lock();
      reset();
      this->unlock(true);
      return true;
      }

   return false;
   }

/**
 * Reset the table, clearing its keys and frequencies.
 * Hash config and full flag will be cleared.
 * Other metadata will not be modified.
 */
template <typename T, size_t bits> void
TR_EmbeddedHashTable<T, bits>::reset()
   {
   // Clear keys first, set first entry to -1
   for (size_t i = 0; i < _realSize; ++i)
      _keys[i] = getDefault(i);
   // Clear counts
   memset(&_freqs, 0, sizeof(uint32_t) * _realSize);
   // Clear metadata, skip constants and locks
   memset(&(this->_hashConfig), 0, sizeof(HashFunction));
   this->_metaData.full = 0;
   }

/**
 * A cheap helper to update the frequency of the value in the table or other
 * without acquiring lock to add value to the table.
 */
template <typename T, size_t bits> void
TR_EmbeddedHashTable<T, bits>::updateFrequency(T value)
   {
   size_t index = this->applyHash(this->_hashConfig, value);
   if (_keys[index] == value)
      {
      _freqs[index]++;
      }
   else if (this->_metaData.otherIndex >=0)
      {
      // If other thread has locked the table or table is already at capacity,
      // just increase the counter of other frequency.
      _freqs[this->getOtherIndex()]++;
      }
   else
      {
      // There is a space in the table. Try to lock the table and add the value.
      this->addKey(value);
      }
   }

/**
 * Add a new value to the hash map.
 * Might need to move around existing values.
 */
template <typename T, size_t bits> void
TR_EmbeddedHashTable<T, bits>::addKey(T value)
   {
   static bool dumpInfo = feGetEnv("TR_JProfilingValueDumpInfo") != NULL;
   if (dumpInfo)
      {
      OMR::CriticalSection lock(vpMonitor);
      printf("Pre %" OMR_PRIX64, static_cast<uint64_t>(value));
      this->dumpInfo(TR::IO::Stdout);
      fflush(stdout);
      }

   // Lock the table, disabling jitted code access
   if (!this->tryLock(true))
      {
      _freqs[this->getOtherIndex()]++;
      return;
      }

   TR_ASSERT_FATAL(this->_metaData.lock == 1, "HashTable not successfully locked");
   TR_ASSERT_FATAL(this->_metaData.otherIndex > -1, "JIT access not disabled");

   // Detect whether the value is already present, count populated slots and find the first empty one
   size_t otherIndex = this->getOtherIndex();
   size_t lastIndex = (1 << bits) - 1;
   int32_t available = -1;
   size_t populated = 0;
   size_t i = 0;
   for (; i <= lastIndex; ++i)
      {
      if (i == otherIndex)
         continue;
      if (_keys[i] == getDefault(i))
         {
         available = available > 0  ? available : i;
         continue;
         }

      if (value == _keys[i])
         break;
      ++populated;
      }

   // If found, increment the value
   if (i <= lastIndex)
      _freqs[i]++;
   else if (available > -1 && populated < this->getCapacity())
      {
      // Temporary hash config
      HashFunction hashConfig;

      // Add the value, with a short cut if its the first
      if (populated == 0)
         {
         size_t newIndex = this->initialHashConfig(hashConfig, value);
         if (otherIndex == newIndex)
            {
            // Have to move otherIndex, pick either the last or second last
            size_t newOther = lastIndex == newIndex ? lastIndex - 1 : lastIndex;
            _freqs[newOther] = _freqs[otherIndex];
            this->_metaData.otherIndex = newOther;
            }

         // Store the first value
         _keys[newIndex] = value;
         _freqs[newIndex] = 1;
         this->_hashConfig = hashConfig;
         }
      else
         {
         // Test if slot is available with current hash method
         size_t defaultIndex = this->applyHash(this->_hashConfig, value);
         if (defaultIndex != otherIndex && _keys[defaultIndex] == getDefault(defaultIndex))
            {
            _keys[defaultIndex] = value;
            _freqs[defaultIndex] = 1;
            }
         else
            {
            // Add the new value to an available slot
            _keys[available] = value;
            _freqs[available] = 1;

            // Calculate a new hash config and rearrange
            T mask = recursivelySplit(0, 0);
            this->updateHashConfig(hashConfig, mask);
            rearrange(hashConfig);
            this->_hashConfig = hashConfig;
            }
         }

      populated++;
      }

   // Unlock the table, leaving JIT access disabled if at capacity
   this->_metaData.full = populated >= this->getCapacity();
   this->unlock(populated < this->getCapacity());

   if (dumpInfo)
      {
      OMR::CriticalSection lock(vpMonitor);
      printf("Post %" OMR_PRIX64, static_cast<uint64_t>(value));
      this->dumpInfo(TR::IO::Stdout);
      fflush(stdout);
      }
   }

/**
 * Calculate an initial hash function config. Returns the index for the first value.
 * It is biased towards placing the first value in the first slot.
 *
 * \param hash Hash function to use.
 * \param bits Number of bits to support.
 * \param value Initial value to add to the table.
 */
template <typename T> size_t
TR_HashTableProfilerInfo<T>::initialHashConfig(HashFunction &hash, T value)
   {
   // This is simple for a mask implementation: nominate no bits
   if (getHashType() == BitMaskHash)
      {
      hash.mask = 0;
      return 0;
      }

   // A bit index hash should try to nominate one bit that is zero for all
   // A bit shift hash should try to nominate one bit that is zero for all and to the left of all
   if (((getHashType() == BitIndexHash) && (~value != 0)) ||
       ((getHashType() == BitShiftHash) && ((~value >> 8) != 0)))
      {
      size_t firstZero = 0;
      if (getHashType() == BitShiftHash)
         {
         firstZero = 8;
         value >>= 8;
         }

      while (value & 1)
         {
         value >>= 1;
         firstZero++;
         }

      for (size_t i = 0; i < getBits(); ++i)
         {
         if (getHashType() == BitIndexHash)
            hash.indices[i] = firstZero;
         else
            hash.shifts[i] = firstZero - i;
         }
      return 0;
      }

   // Otherwise, place in final slot
   size_t firstOne = getHashType() == BitIndexHash ? 0 : 8;
   for (size_t i = 0; i < getBits(); ++i)
      {
      if (getHashType() == BitIndexHash)
         hash.indices[i] = firstOne;
      else
         hash.shifts[i] = firstOne - i;
      }

   return (1 << getBits()) - 1;
   }

/**
 * Inspect the hash table keys and calculate a new hash function config without collisions.
 *
 * \param hash Hash function to use.
 * \param bits Number of bits that to support.
 * \param iter Hash map iterator, to inspect the keys.
 */
template <typename T> void
TR_HashTableProfilerInfo<T>::updateHashConfig(HashFunction &hash, T mask)
   {
   // Done if this is a mask extract
   if (getHashType() == BitMaskHash)
      {
      hash.mask = mask;
      return;
      }

   // Translate this mask into a form the other configs can use
   size_t bit = 0, index = 0;
   for (; mask && bit < getBits(); ++bit)
      {
      // Shift mask until a set bit is found
      while (!(mask & 1))
         {
         ++index;
         mask >>= 1;
         }

      if (getHashType() == BitIndexHash)
         hash.indices[bit] = index;
      else
         hash.shifts[bit] = index - bit;

      ++index;
      mask >>= 1;
      }

   // Default the higher bits, still won't collide
   for (; bit < getBits(); ++bit)
      {
      if (getHashType() == BitIndexHash)
         hash.indices[bit] = 0;
      else
         hash.shifts[bit] = 0;
      }
   }

/**
 * Recursively split keys in a hash map into sets of size 1 or smaller, based on the identification
 * of differing bits. Once split, the selected bits form a mask which can be used in BitMaskHash.
 *
 * \param iter Hash map iterator to use. Will ensure all of its keys can be distinguished.
 * \param mask The mask of selected bits so far.
 * \param choices The values of for the selected bits, in mask, for the current set.
 */
template <typename T, size_t bits> T
TR_EmbeddedHashTable<T, bits>::recursivelySplit(T mask, T choices)
   {
   T key0, key1;

   // Count the elements currently selected
   size_t matched = 0;
   for (size_t i = 0; i < this->getSize(); ++i)
      {
      if (_keys[i] == getDefault(i) || i == this->getOtherIndex())
         continue;
      if ((_keys[i] & mask) == choices)
         {
         if (matched == 0)
            key0 = _keys[i];
         else if (matched == 1)
            key1 = _keys[i];
         matched++;
         }
      }

   if (matched < 2)
      return mask;

   // Try to preserve the existing order by selecting a bit that is zero in key0, one in key1
   T diff = ~key0 & key1;

   // Couldn't find a suitable choice, grab any difference
   if (!diff)
      diff = key0 ^ key1;
   TR_ASSERT_FATAL(diff != 0, "Duplicate keys in set");

   // Grab the lowest bit and add it to the mask
   diff &= ~(diff - 1);
   mask |= diff;
   if (matched == 2)
      return mask;
   else
      return recursivelySplit(mask, choices) | recursivelySplit(mask, choices | diff);
   }

/**
 * Calculate the hash for a value.
 *
 * \param hash Type of hash function to use.
 * \param bits Number of bits to support.
 * \param value Value to process.
 */
template <typename T> size_t
TR_HashTableProfilerInfo<T>::applyHash(HashFunction &hash, T value)
   {
   size_t result = 0;
   if (getHashType() == BitMaskHash)
      {
      size_t i = 1;
      T work = hash.mask;
      while (work)
         {
         T bit = work & ~(work - 1);
         if (value & bit)
            result |= i;
         work &= ~bit;
         i = i << 1;
         }
      }
   else if (getHashType() == BitIndexHash)
      {
      for (size_t bit = 0; bit < getBits(); ++bit)
         result |= ((value >> (hash.indices[bit])) & 1) << bit;
      }
   else
      {
      for (size_t bit = 0; bit < getBits(); ++bit)
         result |= ((value >> (hash.shifts[bit] + bit)) & 1) << bit;
      }

   return result;
   }

/**
 * Rearrange a hash maps contents based on a HashFunction.
 *
 * The map's keys are copied onto the stack, then the current hash function and keys
 * are manipulated to avoid any increments other than to freqs[otherIndex].
 * Keys are rearranged on the stack and then written back to the map.
 *
 * \param hash Hash function to use for new indices.
 */
template <typename T, size_t bits> void
TR_EmbeddedHashTable<T, bits>::rearrange(HashFunction &hash)
   {
   static bool dumpInfo = feGetEnv("TR_JProfilingValueDumpInfo") != NULL;
   const size_t length = 1 << bits;
   T cacheKeys[length];
   size_t plannedMoves[length];

   // Build up moves, plannedMoves[source] = dest, write _keys[source] -> _keys[dest]
   bool mustRearrange = false;
   for (size_t i = 0; i < length; ++i)
      plannedMoves[i] = i;

   // Find any swaps that are needed
   for (size_t i = 0; i < length; ++i)
      {
      // Ignore empty slots, other index
      if (_keys[i] != getDefault(i) && i != this->getOtherIndex())
         {
         size_t newIndex = this->applyHash(hash, _keys[i]);
         if (plannedMoves[i] != newIndex)
            {
            for (size_t j = 0; j < length; ++j)
               {
               if (plannedMoves[j] == newIndex)
                  {
                  plannedMoves[j] = plannedMoves[i];
                  break;
                  }
               }
            plannedMoves[i] = newIndex;
            mustRearrange = true;
            }
         }
      }

   // Nothing to do
   if (!mustRearrange)
      return;

   if (dumpInfo)
      {
      for (size_t i = 0; i < length; ++i)
         {
         printf("%" OMR_PRIu64 " -> %" OMR_PRIu64 "\n", static_cast<uint64_t>(i), static_cast<uint64_t>(plannedMoves[i]));
         }
      }

   // Back up the keys and lock the table
   memcpy(cacheKeys, _keys, sizeof(T) * (1 << bits));
   for (size_t i = 0; i < length; ++i)
      {
      _keys[i] = getDefault(i);
      }

   // Walk the table, swapping to match new mask behaviour
   bool changed;
   do {
      changed = false;
      for (size_t i = 0; i < length; ++i)
         {
         // Skip non-moves, perform other last
         size_t dest = plannedMoves[i];
         if (dest == i || i == this->getOtherIndex() || dest == this->getOtherIndex())
            {
            continue;
            }

         // Swap the two keys
         T tmpKey = cacheKeys[i] == getDefault(i) ? getDefault(dest) : cacheKeys[i];
         cacheKeys[i] = cacheKeys[dest] == getDefault(dest) ? getDefault(i) : cacheKeys[dest];
         cacheKeys[dest] = tmpKey;

         // Swap the two frequencies
         uint32_t tmpCount = _freqs[i];
         _freqs[i] = _freqs[dest];
         _freqs[dest] = tmpCount;

         // See if this is a self contained swap or part of a chain
         if (plannedMoves[dest] == i)
            {
            plannedMoves[i] = i;
            }
         else
            {
            plannedMoves[i] = plannedMoves[dest];
            }
         changed = true;
         plannedMoves[dest] = dest;
         }
      }
   while (changed);

   // Perform the other swap, must be a simple swap by now
   size_t i = this->getOtherIndex();
   if (plannedMoves[i] != i)
      {
      size_t dest = plannedMoves[i];
      TR_ASSERT_FATAL(plannedMoves[dest] == i, "Moves should have simplified to a single swap, %d %d %d %d", i, dest, plannedMoves[i], plannedMoves[dest]);

      // Swap the two keys
      T tmpKey = cacheKeys[i] == getDefault(i) ? getDefault(dest) : cacheKeys[i];
      cacheKeys[i] = cacheKeys[dest] == getDefault(dest) ? getDefault(i) : cacheKeys[dest];
      cacheKeys[dest] = tmpKey;

      // freqs[i] can be accessed by other threads, so redirect it first
      uint32_t realFreq = _freqs[i];
      _freqs[dest] = _freqs[i];
      this->_metaData.otherIndex = dest; // Assume jitted code currently locked out

      // Save the real freq
      _freqs[i] = realFreq;

      // Clean up last swap
      plannedMoves[i] = i;
      plannedMoves[dest] = dest;
      }

   for (size_t i = 0; i < length; ++i)
      {
      TR_ASSERT_FATAL(plannedMoves[i] == i, "Moves did not clean up, %d <-> %d", i, plannedMoves[i]);
      if (cacheKeys[i] != getDefault(i) && i != this->getOtherIndex())
         TR_ASSERT_FATAL(this->applyHash(hash, cacheKeys[i]) == i, "Placed in wrong slot %p %d", cacheKeys[i], i);
      }

   // Frequencies are now all valid and only freqs[dest] is being incremented
   // Write keys back, safe to do in a non-atomic way
   memcpy(_keys, cacheKeys, sizeof(T) * length);
   }

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
      cursor->~Element();
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
   Element *lastCursorExtraInfo = getFirst();
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
      lastCursorExtraInfo = cursorExtraInfo;
      cursorExtraInfo = cursorExtraInfo->getNext();
      }

   TR_ASSERT_FATAL(cursorExtraInfo == NULL, "cursorExtraInfo = 0x%p should be NULL at this point!\n", cursorExtraInfo);
   TR_ASSERT_FATAL(lastCursorExtraInfo, "lastCursorExtraInfo should not be NULL at this point!\n");

   cursorExtraInfo = lastCursorExtraInfo;

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
 * Array value profiling implementation
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
      _totalFrequency = 0;
      memset(_frequencies, 0, sizeof(uint32_t) * getSize());
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
 * Typedefs for instantiations
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
