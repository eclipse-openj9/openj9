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

#include "SystemSegmentProvider.hpp"
#include "env/MemorySegment.hpp"
#include <algorithm>

J9::SystemSegmentProvider::SystemSegmentProvider(size_t defaultSegmentSize, size_t systemSegmentSize, size_t allocationLimit, J9::J9SegmentProvider &segmentAllocator, TR::RawAllocator rawAllocator) :
   SegmentAllocator(defaultSegmentSize),
   _allocationLimit(allocationLimit),
   _systemBytesAllocated(0),
   _regionBytesAllocated(0),
   _systemSegmentAllocator(segmentAllocator),
   _systemSegments( SystemSegmentDequeAllocator(rawAllocator) ),
   _segments(std::less< TR::MemorySegment >(), SegmentSetAllocator(rawAllocator)),
   _freeSegments( FreeSegmentDequeAllocator(rawAllocator) ),
   _currentSystemSegment( TR::ref(_systemSegmentAllocator.request(systemSegmentSize) ) )
   {
   TR_ASSERT(defaultSegmentSize <= systemSegmentSize, "defaultSegmentSize should be smaller than or equal to systemSegmentSize");

   // We cannot simply assign systemSegmentSize to _systemSegmentSize because:
   // We want to make sure that _currentSystemSegment is always a small system segment, i.e. its size <= _systemSegmentSize. When
   // size alignment happens in _systemSegmentAllocator.request, this will be violated, make it _currentSystemSegment a large
   // system segment capable of allocating large segments and small segments. A large segment and its containing system segment
   // is allocated/released differently, we don't want to have a system segment whose memory is used by a mix of small segments
   // and large segments.
   //
   _systemSegmentSize = _currentSystemSegment.get().size;

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

bool
J9::SystemSegmentProvider::isLargeSegment(size_t segmentSize)
   {
   return segmentSize > _systemSegmentSize;
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
      // Only allocate small segments from _currentSystemSegment
      TR_ASSERT(!isLargeSegment(remaining(_currentSystemSegment)), "_currentSystemSegment must be a small segment");
      return allocateNewSegment(roundedSize, _currentSystemSegment);
      }

   size_t systemSegmentSize = std::max(roundedSize, _systemSegmentSize);
   if (_systemBytesAllocated + systemSegmentSize > _allocationLimit )
      {
      throw std::bad_alloc();
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

   TR::reference_wrapper<J9MemorySegment> newSegmentRef = TR::ref(newSegment);

   try
      {
      _systemSegments.push_back(newSegmentRef);
      }
   catch (...)
      {
      _systemSegmentAllocator.release(newSegment);
      throw;
      }
   _systemBytesAllocated += systemSegmentSize;

   // Rounded size determines when the segment is released, see J9::SystemSegmentProvider::release
   if (!isLargeSegment(roundedSize))
      {
      // We want to use the remaining space of _currentSystemSegment after updating it.
      // Carve its remaining space into small segments and add them to the free segment list so that we can use it later
      //
      while (remaining(_currentSystemSegment) >= defaultSegmentSize())
         {
         _freeSegments.push_back( TR::ref(allocateNewSegment(defaultSegmentSize(), _currentSystemSegment) ) );
         }

      _currentSystemSegment = newSegmentRef;
      }
   else
      {
      // _currentSystemSegment should not point to any segment with space larger than _systemSegmentSize because
      // such segment cannot be reused for other requests and is to be released when the release method is invoked,
      // e.g. when a TR::Region goes out of scope
      //
      }

   return allocateNewSegment(roundedSize, newSegmentRef);
   }

void
J9::SystemSegmentProvider::release(TR::MemorySegment & segment) throw()
   {
   size_t const segmentSize = segment.size();

   if (segmentSize == defaultSegmentSize())
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
   else if (isLargeSegment(segmentSize))
      {
      // System segments larger than _systemSegmentSize is used to create only one segment,
      // release the corresponding system segment when releasing `segment`
      for (auto i = _systemSegments.begin(); i != _systemSegments.end(); ++i)
         {
         if (i->get().heapBase == segment.base())
            {
            _regionBytesAllocated -= segmentSize;
            _systemBytesAllocated -= segmentSize;

            /* Removing segment from _segments */
            auto it = _segments.find(segment);
            _segments.erase(it);


            J9MemorySegment &customSystemSegment = i->get();
            _systemSegments.erase(i);
            _systemSegmentAllocator.release(customSystemSegment);
            break;
            }
         }
      }
   else
      {
      size_t const oldSegmentSize = segmentSize;
      void * const oldSegmentArea = segment.base();

      /* Removing segment from _segments */
      auto it = _segments.find(segment);
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
J9::SystemSegmentProvider::allocateNewSegment(size_t size, TR::reference_wrapper<J9MemorySegment> systemSegment)
   {
   TR_ASSERT( (size % defaultSegmentSize()) == 0, "Misaligned segment");
   void *newSegmentArea = operator new(size, systemSegment);
   if (!newSegmentArea) throw std::bad_alloc();
   try
      {
      TR::MemorySegment &newSegment = createSegmentFromArea(size, newSegmentArea);
      _regionBytesAllocated += size;
      return newSegment;
      }
   catch (...)
      {
      ::operator delete(newSegmentArea, systemSegment);
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
