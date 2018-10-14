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

#ifndef J9_DBG_EXT_SEGMENT_PROVIDER_H
#define J9_DBG_EXT_SEGMENT_PROVIDER_H

#pragma once

#include "env/SegmentProvider.hpp"
#include "env/jittypes.h"


namespace J9 {

class DebugSegmentProvider : public TR::SegmentProvider
   {
public:
   DebugSegmentProvider(
      size_t segmentSize,
      void* (*dbgMalloc)(uintptrj_t size, void *originalAddress),
      void  (*dbgFree)(void *addr)
   );

   virtual TR::MemorySegment &request(size_t requiredSize);
   virtual void release(TR::MemorySegment &segment) throw();
   virtual size_t bytesAllocated() const throw();

private:
   void* (*_dbgMalloc)(uintptrj_t size, void *originalAddress);
   void  (*_dbgFree)(void *addr);
   };

} // namespace J9

#endif // J9_DBG_EXT_SEGMENT_PROVIDER_H
