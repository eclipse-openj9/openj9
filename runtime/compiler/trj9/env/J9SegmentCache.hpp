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

#ifndef J9SEGMENT_CACHE_H
#define J9SEGMENT_CACHE_H

#pragma once

#include "env/J9SegmentProvider.hpp"

namespace J9 {

class J9SegmentCache : public J9SegmentProvider
   {
public:

   J9SegmentCache(size_t cachedSegmentSize, J9SegmentProvider &backingProvider);
   J9SegmentCache(J9SegmentCache &donor);

   ~J9SegmentCache() throw();

   virtual J9MemorySegment& request(size_t requiredSize);
   virtual void release(J9MemorySegment& segment) throw();
   virtual size_t getPreferredSegmentSize() { return _cachedSegmentSize; }

   J9SegmentCache &ref() { return *this; }

private:
   size_t _cachedSegmentSize;
   J9SegmentProvider &_backingProvider;
   J9MemorySegment *_firstSegment;
   bool _firstSegmentInUse;
   };

}

#endif // J9SEGMENT_CACHE_H
