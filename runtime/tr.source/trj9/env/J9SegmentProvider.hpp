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

#ifndef J9_SEGMENT_PROVIDER
#define J9_SEGMENT_PROVIDER

#pragma once

#include <stddef.h>
#include <new>

extern "C" {
struct J9MemorySegment;
}

namespace J9 {

class J9SegmentProvider
   {
public:
   virtual J9MemorySegment& request(size_t requiredSize) = 0;
   virtual void release(J9MemorySegment& segment) throw() = 0;
   virtual size_t getPreferredSegmentSize() { return 0; }

protected:
   J9SegmentProvider();
   J9SegmentProvider(const J9SegmentProvider &other);

   /*
    * Require knowledge of the concrete class in order to destroy SegmentProviders
    */
   virtual ~J9SegmentProvider() throw();
   };

} // namespace J9

void *operator new(size_t, J9MemorySegment &) throw();
void *operator new[](size_t, J9MemorySegment &) throw();
void operator delete(void *, J9MemorySegment &) throw();
void operator delete[](void *, J9MemorySegment &) throw();

#endif // J9_SEGMENT_PROVIDER
