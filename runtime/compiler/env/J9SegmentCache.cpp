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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "env/J9SegmentCache.hpp"
#include "j9.h"
#include "infra/Assert.hpp"

J9::J9SegmentCache::J9SegmentCache(size_t cachedSegmentSize, J9SegmentProvider &backingProvider) :
   _cachedSegmentSize(cachedSegmentSize),
   _backingProvider(backingProvider),
   _firstSegment(&_backingProvider.request(_cachedSegmentSize)),
   _firstSegmentInUse(false)
   {
   }

J9::J9SegmentCache::J9SegmentCache(J9SegmentCache &donor) :
   _cachedSegmentSize(donor._cachedSegmentSize),
   _backingProvider(donor._backingProvider),
   _firstSegment(donor._firstSegment),
   _firstSegmentInUse(false)
   {
   TR_ASSERT(donor._firstSegmentInUse == false, "Unsafe hand off between SegmentCaches");
   donor._firstSegment = 0;
   }

J9::J9SegmentCache::~J9SegmentCache() throw()
   {
   if (_firstSegment)
      _backingProvider.release(*_firstSegment);
   }

J9MemorySegment &
J9::J9SegmentCache::request(size_t requiredSize)
   {
   TR_ASSERT(_firstSegment, "Segment was stolen");
   if (
      _firstSegmentInUse
      || requiredSize > _cachedSegmentSize
      )
      {
      return _backingProvider.request(requiredSize);
      }
   _firstSegmentInUse = true;
   return *_firstSegment;
   }

void
J9::J9SegmentCache::release(J9MemorySegment &unusedSegment) throw()
   {
   if ( &unusedSegment == _firstSegment )
      {
      _firstSegmentInUse = false;
      unusedSegment.heapAlloc = unusedSegment.heapBase;
      }
   else
      {
      _backingProvider.release(unusedSegment);
      }
   }
