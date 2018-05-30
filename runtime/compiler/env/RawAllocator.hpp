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

#ifndef RAWALLOCATOR_HPP
#define RAWALLOCATOR_HPP

#pragma once

#ifndef TR_RAW_ALLOCATOR
#define TR_RAW_ALLOCATOR

namespace J9 {
class RawAllocator;
}

namespace TR {
using J9::RawAllocator;
}

#endif // #ifndef TR_RAW_ALLOCATOR

#include <new>
#include "env/TypedAllocator.hpp"
#include "j9.h"
#undef min
#undef max

extern "C" {
struct J9JavaVM;
}

namespace J9 {

class SegmentAllocator;

class RawAllocator
   {
public:
   explicit RawAllocator(J9JavaVM *javaVM) :
      _javaVM(javaVM)
      {
      }

   RawAllocator(const RawAllocator &other) :
      _javaVM(other._javaVM)
      {
      }

   void * allocate(size_t size, const std::nothrow_t& tag, void * hint = 0) throw()
      {
      PORT_ACCESS_FROM_JAVAVM(_javaVM);
      return j9mem_allocate_memory(size, J9MEM_CATEGORY_JIT);
      }

   void * allocate(size_t size, void * hint = 0)
      {
      void * allocated = allocate(size, std::nothrow, hint);
      if (!allocated) throw std::bad_alloc();
      return allocated;
      }

   void deallocate(void * p, size_t size = 0) throw()
      {
      PORT_ACCESS_FROM_JAVAVM(_javaVM);
      j9mem_free_memory(p);
      }

   friend bool operator ==(const RawAllocator &left, const RawAllocator &right)
      {
      return true;
      }

   friend bool operator !=(const RawAllocator &left, const RawAllocator &right)
      {
      return !operator ==(left, right);
      }

   friend class SegmentAllocator;

   template < typename T >
   operator TR::typed_allocator< T, TR::RawAllocator >()
      {
      return TR::typed_allocator< T, TR::RawAllocator >(*this);
      }

private:

   J9JavaVM * _javaVM;

   };

}

inline void * operator new(size_t size, J9::RawAllocator allocator)
   {
   return allocator.allocate(size);
   }

inline void operator delete(void *ptr, J9::RawAllocator allocator) throw()
   {
   allocator.deallocate(ptr);
   }

inline void * operator new[](size_t size, J9::RawAllocator allocator)
   {
   return allocator.allocate(size);
   }

inline void operator delete[](void *ptr, J9::RawAllocator allocator) throw()
   {
   allocator.deallocate(ptr);
   }

inline void * operator new(size_t size, J9::RawAllocator allocator, const std::nothrow_t& tag) throw()
   {
   return allocator.allocate(size, tag);
   }

inline void * operator new[](size_t size, J9::RawAllocator allocator, const std::nothrow_t& tag) throw()
   {
   return allocator.allocate(size, tag);
   }

#endif // RAWALLOCATOR_HPP

