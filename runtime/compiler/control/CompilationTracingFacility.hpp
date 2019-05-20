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

#ifndef COMPILATIONTRACINGFACILITY_HPP
#define COMPILATIONTRACINGFACILITY_HPP

#pragma once

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "control/CompilationOperations.hpp"

extern "C" {
struct J9VMThread;
}

//---------------------------- TR_CompilationTracingEntry ------------------
// This is used for debugging purposes and only under a JIT option to avoid overhead
//-------------------------------------------------------------------------
struct TR_CompilationTracingEntry
   {
   TR_PERSISTENT_ALLOC(TR_Memory::OptimizationPlan) // lie about the type as this is for debug only
   uint16_t _J9VMThreadId; // last meaningful bits from the vm thread pointer
   uint8_t  _operation;
   uint8_t  _otherData;
   }; // TR_CompilationTracingEntry


// NOTE: if changes are made to this data structure, DebugExt.cpp might need
// to be changed as well
//
namespace TR
{

class CompilationTracingFacility
   {
   public:
   TR_PERSISTENT_ALLOC(TR_Memory::OptimizationPlan) // lie about the type as this is for debug only
   CompilationTracingFacility() : _circularBuffer(0),_size(0),_index(0){} // default constructor must be present
   void initialize(int32_t size)
      {
      if (!isInitialized())
         {
         if (size == (size & (-size))) // power of two test
            {
            //fprintf(stderr, "Allocating buffer\n");
            _circularBuffer = new (PERSISTENT_NEW) TR_CompilationTracingEntry[size];
            memset(_circularBuffer, 0, size*sizeof(TR_CompilationTracingEntry));
            _size = size;
            _index = 0;
            }
         }
      }
   // Must have compilationMonitor in hand when calling this method
   void addNewEntry(J9VMThread * vmThread, TR_CompilationOperations op, uint8_t otherData)
      {
      _circularBuffer[_index]._J9VMThreadId = compactJ9VMThreadPtr(vmThread);
      _circularBuffer[_index]._operation = (uint8_t)op;
      _circularBuffer[_index]._otherData = otherData;
      _index = ++_index & (_size-1); // assumes size is a power of two
      }
   uint16_t compactJ9VMThreadPtr(J9VMThread *vmThread) const { return (uint16_t)((((uintptrj_t)vmThread) >> 8) & 0xffff); }
   J9VMThread *expandJ9VMThreadId(uint16_t vmThreadId) const { return (J9VMThread*)(((uintptrj_t)vmThreadId) << 8);}
   bool    isInitialized() const { return _circularBuffer ? true : false; }
   int32_t getIndex()      const { return _index; }
   int32_t getSize()       const { return _size; }
   int32_t getNextIndex(int32_t i) const { return (i+1) & (getSize()-1); }
   TR_CompilationTracingEntry *getEntry(int32_t index) { return index < _size ? _circularBuffer+index : 0; }
   private:
   TR_CompilationTracingEntry *_circularBuffer; // will be allocated dynamically
   int32_t                     _index; // pointing to next entry to be written (oldest)
   int32_t                     _size;  // size of the circular buffer; power of two
   }; // CompilationTracingFacility

} // namespace TR
#endif // COMPILATIONTRACINGFACILITY_HPP

