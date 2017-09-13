/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "SystemSegmentProvider.hpp"
#include "env/MemorySegment.hpp"
#include <algorithm>

J9::SystemSegmentProvider::SystemSegmentProvider(size_t defaultSegmentSize, size_t systemSegmentSize, size_t allocationLimit, J9::J9SegmentProvider &segmentAllocator, TR::RawAllocator rawAllocator) :
   SegmentAllocator(defaultSegmentSize),
   _systemSegmentSize(systemSegmentSize),
   _allocationLimit(allocationLimit),
   _systemBytesAllocated(0),
   _regionBytesAllocated(0),
   _systemSegmentAllocator(segmentAllocator),
   _systemSegments( SystemSegmentDequeAllocator(rawAllocator) ),
   _segments(std::less< TR::MemorySegment >(), SegmentSetAllocator(rawAllocator)),
   _freeSegments( FreeSegmentDequeAllocator(rawAllocator) ),
   _currentSystemSegment( TR::ref(_systemSegmentAllocator.request(_systemSegmentSize) ) )
   {
   try
      {
      _systemSegments.push_back(TR::ref(_currentSystemSegment));
      }
   catch (...)
      {
      _systemSegmentAllocator.release(_currentSystemSegment);
      throw;
      }
   _systemBytesAllocated += _systemSegmentSize;
   }

J9::SystemSegmentProvider::~SystemSegmentProvider() throw()
   {
   while (!_systemSegments.empty())
      {
      J9MemorySegment &topSegment = _systemSegments.back().get();
      _systemSegments.pop_back();
      _systemSegmentAllocator.release(topSegment);
      }
   }

TR::MemorySegment &
J9::SystemSegmentProvider::request(size_t requiredSize)
   {
   size_t const roundedSize = round(requiredSize);
   if (
      !_freeSegments.empty()
      && !(defaultSegmentSize() < roundedSize)
      )
      {
      TR::MemorySegment &recycledSegment = _freeSegments.back().get();
      _freeSegments.pop_back();
      recycledSegment.reset();
      return recycledSegment;
      }

   if (remaining(_currentSystemSegment) >= roundedSize)
      {
      return allocateNewSegment(roundedSize);
      }

   size_t systemSegmentSize = std::max(roundedSize, _systemSegmentSize);
   if (_systemBytesAllocated + systemSegmentSize > _allocationLimit )
      {
      throw std::bad_alloc();
      }

   while (remaining(_currentSystemSegment) >= defaultSegmentSize())
      {
      _freeSegments.push_back( TR::ref( allocateNewSegment(defaultSegmentSize()) ) );
      }

   J9MemorySegment &newSegment = _systemSegmentAllocator.request(systemSegmentSize);
   TR_ASSERT(
      newSegment.heapAlloc == newSegment.heapBase,
      "Segment @ %p { heapBase: %p, heapAlloc: %p, heapTop: %p } is stale",
      &newSegment,
      newSegment.heapBase,
      newSegment.heapAlloc,
      newSegment.heapTop
      );
   try
      {
      _systemSegments.push_back(TR::ref(newSegment));
      }
   catch (...)
      {
      _systemSegmentAllocator.release(newSegment);
      throw;
      }
   _systemBytesAllocated += systemSegmentSize;
   _currentSystemSegment = TR::ref(newSegment);
   return allocateNewSegment(roundedSize);
   }

void
J9::SystemSegmentProvider::release(TR::MemorySegment & segment) throw()
   {
   if (segment.size() == defaultSegmentSize())
      {
      try
         {
         _freeSegments.push_back(TR::ref(segment));
         }
      catch (...)
         {
         /* not much we can do here except leak */
         }
      }
   else if (segment.size() > _systemSegmentSize)
      {
      for (auto i = _systemSegments.begin(); i != _systemSegments.end(); ++i)
         {
         if (i->get().heapBase == segment.base())
            {
            _regionBytesAllocated -= segment.size();
            _systemBytesAllocated -= segment.size();
            J9MemorySegment &customSystemSegment = i->get();
            _systemSegments.erase(i);
            _systemSegmentAllocator.release(customSystemSegment);
            break;
            }
         }
      }
   else
      {
      auto it = _segments.find(segment);
      size_t const oldSegmentSize = segment.size();
      void * const oldSegmentArea = segment.base();
      _segments.erase(it);

      TR_ASSERT(oldSegmentSize % defaultSegmentSize() == 0, "misaligned segment");
      size_t const chunks = oldSegmentSize / defaultSegmentSize();
      for (size_t i = 0; i < chunks; ++i)
         {
         try
            {
            createSegmentFromArea(defaultSegmentSize(), static_cast<uint8_t *>(oldSegmentArea) + (defaultSegmentSize() * i));
            }
         catch (...)
            {
            /* not much we can do here except leak */
            }
         }
      }
   }

size_t
J9::SystemSegmentProvider::systemBytesAllocated() const throw()
   {
   return _systemBytesAllocated;
   }

size_t
J9::SystemSegmentProvider::regionBytesAllocated() const throw()
   {
   return _regionBytesAllocated;
   }

size_t
J9::SystemSegmentProvider::bytesAllocated() const throw()
   {
   return _regionBytesAllocated;
   }

TR::MemorySegment &
J9::SystemSegmentProvider::allocateNewSegment(size_t size)
   {
   TR_ASSERT( (size % defaultSegmentSize()) == 0, "Misaligned segment");
   void *newSegmentArea = operator new(size, _currentSystemSegment);
   if (!newSegmentArea) throw std::bad_alloc();
   try
      {
      TR::MemorySegment &newSegment = createSegmentFromArea(size, newSegmentArea);
      _regionBytesAllocated += size;
      return newSegment;
      }
   catch (...)
      {
      ::operator delete(newSegmentArea, _currentSystemSegment);
      throw;
      }
   }

TR::MemorySegment &
J9::SystemSegmentProvider::createSegmentFromArea(size_t size, void *newSegmentArea)
   {
   auto result = _segments.insert( TR::MemorySegment(newSegmentArea, size) );
   TR_ASSERT(result.first != _segments.end(), "Bad iterator");
   TR_ASSERT(result.second, "Insertion failed");
   return const_cast<TR::MemorySegment &>(*(result.first));
   }

size_t
J9::SystemSegmentProvider::allocationLimit() const throw()
   {
   return _allocationLimit;
   }

void
J9::SystemSegmentProvider::setAllocationLimit(size_t allocationLimit)
   {
   _allocationLimit = allocationLimit;
   }

size_t
J9::SystemSegmentProvider::round(size_t requestedSize)
   {
   return ( ( ( requestedSize + (defaultSegmentSize() - 1) ) / defaultSegmentSize() ) * defaultSegmentSize() );
   }

ptrdiff_t
J9::SystemSegmentProvider::remaining(const J9MemorySegment &memorySegment)
   {
   return memorySegment.heapTop - memorySegment.heapAlloc;
   }
