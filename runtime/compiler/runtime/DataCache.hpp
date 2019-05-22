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

#ifndef DATACACHE_HPP
#define DATACACHE_HPP

#include <stdint.h>
#include "j9.h"
#include "infra/Assert.hpp"

namespace TR { class Monitor; }
struct J9JITDataCacheHeader;

//#undef DATA_CACHE_DEBUG
#define DATA_CACHE_VERBOSITY_LEVEL 3 // Higher numbers means more verbose output
class TR_DataCache
{
   friend class TR_DebugExt;
private:
   TR_DataCache    *_next;      // to be able to chain them
   J9MemorySegment *_segment;   // the segment where the memory for the dataCache is
   J9VMThread      *_vmThread;  // thread that is actively working on this cache; could be NULL
   uint8_t         *_allocationMark; // used if we want to give back memory up to previously set mark
   //TR::Monitor       *_mutex;     // Is this needed?
   int32_t          _status;    // mostly RAS at this point
public:
   enum {
      RESERVED=1,
      ACTIVE,
      ALMOST_FULL,
      FULL,
   };
   friend class TR_DataCacheManager;
   // The following methods need to be called on a reserved DataCache
   uint8_t *allocateDataCacheSpace(int32_t size);
   void setAllocationMark() { _allocationMark = _segment->heapAlloc; }
   void resetAllocationToMark() {
      /*memset(_allocationMark, 0, _segment->heapAlloc-_allocationMark);*/
      _segment->heapAlloc = _allocationMark;
   } // used for TOSS_CODE
   uint8_t *getAllocationMark() const { return _allocationMark; }
   uint32_t remainingSpace() { return _segment->heapTop - _segment->heapAlloc; }
   uint8_t *getCurrentHeapAlloc() { return _segment->heapAlloc; } // Must have the cache reserved
   J9MemorySegment *getSegment() { return _segment; } // try not to use this method
};

class TR_DataCacheManager
{
protected:

   template < class T >
   class InPlaceList
   {
   public:
      struct ListElement {
      public:
         ListElement *_prev;
         ListElement *_next;
         T *_contents;
         void* operator new(size_t, void *p) {return p;}
         explicit ListElement(T *contents = 0) :
            _prev(),
            _next(),
            _contents(contents)
            {
               this->_prev = this;
               this->_next = this;
            TR_ASSERT(_prev , "List element created with a null pointer to the previous element");
            TR_ASSERT(_next , "List element created with a null pointer to the next element");
            }
         ListElement(ListElement *insertPrev, ListElement *insertNext) :
            _prev(insertPrev),
            _next(insertNext),
            _contents(0)
            {
            TR_ASSERT(_prev , "List element created with a null pointer to the previous element");
            TR_ASSERT(_next , "List element created with a null pointer to the next element");
            }
         T* contents() { return _contents; }
         void remove()
            {
            TR_ASSERT(_prev , "List element left with a null pointer to the previous element");
            TR_ASSERT(_next , "List element left with a null pointer to the next element");
            _prev->_next = _next;
            _next->_prev = _prev;
            }
         void update(ListElement *prev, ListElement *next)
            {
            TR_ASSERT(_prev , "Attempting to update list element with a null pointer to the previous element");
            TR_ASSERT(_next , "Attempting to update list element with a null pointer to the next element");
            _prev = prev;
            _next = next;
            }
         void fixRefs()
            {
            _prev->_next = this;
            _next->_prev = this;
            TR_ASSERT(_prev , "List element left with a null pointer to the previous element");
            TR_ASSERT(_next , "List element left with a null pointer to the next element");
            }
#if defined(DATA_CACHE_DEBUG)
         void sanityCheck()
            {
            TR_ASSERT( _prev, "List element with NULL pointer to previous node");
            TR_ASSERT( _next, "List element with NULL pointer to following node");
            }
#endif
      };
   private:
      ListElement _sentinel;
#if defined(DATA_CACHE_DEBUG)
      void checkList()
         {
         Iterator it1 = begin();
         Iterator it2 = begin();
         while (it1 != end() && it2 != end())
            {
            ++it2;
            if (it2 == end())
               break;
            TR_ASSERT(it2 != it1, "List sanity failure");
            ++it1;
            ++it2;
            }
         }
#endif
   public:
      friend class TR_DebugExt;
      class Iterator {
      private:
         ListElement *_currentElement;
      public:
         Iterator(ListElement *startElement) :
            _currentElement(startElement)
            {
            TR_ASSERT(startElement, "Creating an interator with a null start element.");
            }
         Iterator &operator++()
            {
#if defined(DATA_CACHE_DEBUG)
            _currentElement->sanityCheck();
#endif
            TR_ASSERT(_currentElement->_next, "Attempting to move iterator forward to a null list element.");
            _currentElement = _currentElement->_next;
#if defined(DATA_CACHE_DEBUG)
            _currentElement->sanityCheck();
#endif
            return *this;
            }
         Iterator operator++(int)
            {
            Iterator ret = *this;
            ++*this;
            return ret;
            }
         Iterator &operator--()
            {
#if defined(DATA_CACHE_DEBUG)
            _currentElement->sanityCheck();
#endif
            TR_ASSERT(_currentElement->_prev, "Attempting to move iterator backward to a null list element.");
            _currentElement = _currentElement->_prev;
#if defined(DATA_CACHE_DEBUG)
            _currentElement->sanityCheck();
#endif
            return *this;
            }
         Iterator operator--(int)
            {
            Iterator ret = *this;
            --*this;
            return ret;
            }
         bool operator ==(const Iterator &compare) { return this->_currentElement == compare._currentElement; }
         bool operator !=(const Iterator &compare) { return !( *this == compare); }
         T& operator *() { return *(_currentElement->contents()); }
         T* operator ->() { return _currentElement->contents(); }
         ListElement *currentElement()
            {
            return _currentElement;
            }
      };
      void *operator new(size_t size, void *ptr) { return ptr; }
      InPlaceList() :
         _sentinel()
         {
         }
      InPlaceList(InPlaceList<T> &list):
         _sentinel(list._sentinel)
         {
         }
      bool empty()
         {
         return _sentinel._next == &_sentinel;
         }
      Iterator begin()
         {
         Iterator ret(_sentinel._next);
         return ret;
         }
      Iterator end()
         {
         Iterator ret(&_sentinel);
         return ret;
         }
      void insert(Iterator pos, T& newItem)
         {
         ListElement *newListElement = newItem.getListElement();
         newListElement->update( pos.currentElement()->_prev, pos.currentElement() );
         newListElement->fixRefs();
#if defined(DATA_CACHE_DEBUG)
         checkList();
#endif
         }
      Iterator remove(Iterator pos)
         {
         ListElement *removeMe = pos.currentElement();
         ++pos;
         removeMe->remove();
#if defined(DATA_CACHE_DEBUG)
         checkList();
#endif
         return pos;
         }
      void push_front(T& newItem)
         {
         ListElement* newListElement = newItem.getListElement();
         newListElement->update(&_sentinel, _sentinel._next);
         newListElement->fixRefs();
#if defined(DATA_CACHE_DEBUG)
         checkList();
#endif
         }
   };

   // These are only created when space is allocated directly through the data cache manager.
   // (AOT) allocations requested through reserved data caches do not create one of these as part of the allocation.
   class Allocation
      {
      private:
         J9JITDataCacheHeader _header;
         InPlaceList<Allocation>::ListElement _listElement;

      public:
         friend class TR_DebugExt;
         void *operator new (size_t size, void * ptr) { return ptr; }
         explicit Allocation(uint32_t size) :
            _listElement(this)
            {
            _header.size = size;
            _header.type = J9_JIT_DCE_UNALLOCATED;
#if defined(DATA_CACHE_DEBUG) && (DATA_CACHE_VERBOSITY_LEVEL >= 3)
            fprintf(stderr, "Creating allocation at %p.\n", this);
#endif
            }
         uint32_t size() { return _header.size; }
         Allocation *split ( uint32_t size );
         InPlaceList<Allocation>::ListElement *getListElement() { return &_listElement; }
         void *getBuffer() { return static_cast<void *>(&_listElement); }
         void prepareForUse() { _header.type = J9_JIT_DCE_IN_USE; }
         void print();
      };


   class SizeBucket
   {
   private:
      InPlaceList<SizeBucket>::ListElement _listElement;
      U_32 _size;
      InPlaceList<Allocation> _allocations;
   public:
      friend class TR_DebugExt;
      void *operator new (size_t size, void *ptr) { return ptr; }
      SizeBucket():
      _listElement(this),
      _size(0),
      _allocations()
         {
         }
      explicit SizeBucket(Allocation* allocation) :
         _listElement(this),
         _size(allocation->size()),
         _allocations()
         {
         push(allocation);
         }
      SizeBucket (SizeBucket &sb):
         _listElement(this),
         _size(sb._size),
         _allocations(sb._allocations)
         {
         }
      ~SizeBucket();
      U_32 size() const { return _size; }
      void push(Allocation *alloc);
      Allocation *pop();
      bool isEmpty()
         {
         return _allocations.empty();
         }
      InPlaceList<SizeBucket>::ListElement *getListElement() { return &_listElement; }
      void print();
      UDATA calculateBucketSize();
   };

private:
   static TR_DataCacheManager *_dataCacheManager; // singleton

   TR_DataCache    *_activeDataCacheList;
   TR_DataCache    *_almostFullDataCacheList; // for future implementation
   TR_DataCache     *_cachesInPool;
   int32_t          _numAllocatedCaches;
   uint32_t         _flags;     // for configuration
   J9JITConfig     *_jitConfig;

   // Added as part of data cache reclamation
   const uint32_t _quantumSize;
   const uint32_t _minQuanta;
   const bool _newImplementation;
   const bool _worstFit;

   TR_DataCache *allocateNewDataCache(uint32_t minimumSize);
   uint8_t *allocateDataCacheSpace(uint32_t size); // Made private for data cache reclamation.
   void freeDataCacheList(TR_DataCache *& head);

   // Added as part of data cache reclamation
   void addToPool(Allocation *);
   Allocation *getFromPool(uint32_t size);
   Allocation *convertDataCacheToAllocation(TR_DataCache *dataCache);
   void *allocateMemoryFromVM(size_t size);
   void freeMemoryToVM(void *ptr);
   uint32_t alignAllocation(uint32_t size)
      {
      if (size < (_quantumSize * _minQuanta) )
         size = (_quantumSize * _minQuanta);
      else
         size = roundToMultiple<uint32_t>(size, _quantumSize);
      return size;
      }

protected:

   TR_DataCacheManager(J9JITConfig *jitConfig, TR::Monitor *monitor, uint32_t quantumSize, uint32_t minQuanta, bool newImplementation = true, bool worstFit = false);
   void convertDataCachesToAllocations();

   virtual ~TR_DataCacheManager();
   virtual void growHook( UDATA allocationSize );
   virtual void allocationHook( UDATA allocationSize, UDATA requestedSize );
   virtual void freeHook( UDATA allocationSize );
   virtual void insertHook( UDATA allocationSize );
   virtual void removeHook( UDATA allocationSize );

   InPlaceList<SizeBucket> _sizeList;
   TR::Monitor       *_mutex;     // to add/remove from activeDataCacheList
   UDATA            _totalSegmentMemoryAllocated;

   template <typename T>
   static T roundToMultiple(T value, T multiple)
      {
      value = ( ( (value + (multiple - 1) ) / multiple ) * multiple );
      return value;
      }

public:
   friend class TR_DebugExt;
   void *operator new (size_t size, void * ptr) { return ptr; }
   TR_DataCache *reserveAvailableDataCache(J9VMThread *vmThread, uint32_t sizeHint);
   void makeDataCacheAvailable(TR_DataCache *dataCache); // put back the cache into the _activeDataCacheList
   uint8_t *allocateDataCacheRecord(uint32_t size, uint32_t allocType, uint32_t *allocSizePtr);
   void retireDataCache(TR_DataCache *dataCache);
   void fillDataCacheHeader(J9JITDataCacheHeader *hdr, uint32_t allocationType, uint32_t size);
   double computeDataCacheEfficiency();
   uint32_t getTotalSegmentMemoryAllocated() const { return _totalSegmentMemoryAllocated; }
   void freeDataCacheRecord(void *record);
   void startupOver()
      {
      convertDataCachesToAllocations();
      }

   virtual void printStatistics();


   // static methods
   static TR_DataCacheManager* initialize(J9JITConfig * jitConfig);
   static TR_DataCacheManager* getManager() { return _dataCacheManager; }
   static void destroyManager();
   static void copyDataCacheAllocation (J9JITDataCacheHeader *dest, J9JITDataCacheHeader *src);
#if defined(TR_HOST_64BIT)
#define ALIGNMENT_DCM  7
#else
#define ALIGNMENT_DCM  3
#endif
   static uint32_t alignToMachineWord(uint32_t size) { return (size + (uint32_t)ALIGNMENT_DCM) & (~((uint32_t)ALIGNMENT_DCM)); }
   template <typename T>
   static TR_DataCacheManager* constructManager (
               J9JITConfig *jitConfig,
               TR::Monitor *monitor,
               uint32_t quantumSize,
               uint32_t minQuanta,
               bool newImplementation
      );
};

#include "infra/Statistics.hpp"
class TR_InstrumentedDataCacheManager : public virtual TR_DataCacheManager {
public:
   TR_InstrumentedDataCacheManager(J9JITConfig *jitConfig, TR::Monitor *monitor, uint32_t quantumSize, uint32_t minQuanta, bool newImplementation = true, bool worstFit = false);
   virtual void printStatistics();

protected:
   virtual void growHook( UDATA allocationSize );
   virtual void allocationHook( UDATA allocationSize, UDATA requestedSize );
   virtual void freeHook( UDATA allocationSize );
   virtual void insertHook( UDATA allocationSize );
   virtual void removeHook( UDATA allocationSize );
   virtual ~TR_InstrumentedDataCacheManager();

private:
   void printPoolContents();
   UDATA calculatePoolSize();

   UDATA _jitSpace;
   UDATA _freeSpace;
   UDATA _usedSpace;
   UDATA _totalWaste;
   UDATA _numberOfAllocations;
   UDATA _numberOfCurrentAllocations;
   UDATA _bytesAllocated;
   double _maxConcurrentWasteEstimate;
   double _squares;
   UDATA _loss;
   UDATA _bytesInPool;
   TR_StatsHisto<32> _allocationStatistics;
   TR_StatsHisto< (sizeof(uintptr_t) - 1) > _wasteStatistics;
};

#endif // DATACACHE_HPP
