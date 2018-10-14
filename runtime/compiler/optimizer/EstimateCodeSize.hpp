/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef ESTIMATECS_INCL
#define ESTIMATECS_INCL

#include <stddef.h>               // for size_t
#include <stdint.h>               // for int32_t
#include "env/TRMemory.hpp"       // for Allocator
#include "infra/Flags.hpp"        // for flags8_t
#include "optimizer/Inliner.hpp"  // for TR_InlinerTracer (ptr only), etc

class TR_CallStack;
namespace TR { class Compilation; }
struct TR_CallSite;
struct TR_CallTarget;

#define MAX_ECS_RECURSION_DEPTH 30

class TR_EstimateCodeSize
   {
   public:

   void * operator new (size_t size, TR::Allocator allocator) { return allocator.allocate(size); }

   //    {
   //    TR_EstimateCodeSize::raiiWrapper lexicalScopeObject(....);
   //    TR_EstimateCodeSize *ecs = lexicalScopeObject->getCodeEstimator();
   //    ... do whatever you gotta do
   //    } // estimator will be automatically freed by lexicalScopeObject regardless of how scope is exitted
   class raiiWrapper
      {
      public:
      raiiWrapper(TR_InlinerBase *inliner, TR_InlinerTracer *tracer, int32_t sizeThreshold)
         {
         _estimator = TR_EstimateCodeSize::get(inliner, tracer, sizeThreshold);
         }

      ~raiiWrapper()
         {
         TR_EstimateCodeSize::release(_estimator);
         }

      TR_EstimateCodeSize *getCodeEstimator()
         {
         return _estimator;
         }

      private:
      TR_EstimateCodeSize *_estimator;
      };

   TR_EstimateCodeSize() { }

   static TR_EstimateCodeSize *get(TR_InlinerBase *inliner, TR_InlinerTracer *tracer, int32_t sizeThreshold);
   static void release(TR_EstimateCodeSize *estimator);

   bool calculateCodeSize(TR_CallTarget *, TR_CallStack *, bool recurseDown = true);

   int32_t getSize()                   { return _realSize; }
   virtual int32_t getOptimisticSize() { return 0; } // override in subclasses that support partial inlining
   int32_t getError()                  { return _error; }
   int32_t getSizeThreshold()          { return _sizeThreshold; }
   bool aggressivelyInlineThrows()     { return _aggressivelyInlineThrows; }
   bool recursedTooDeep()              { return _recursedTooDeep; }
   bool isLeaf()                       { return _isLeaf; }

   int32_t getNumOfEstimatedCalls()    { return _numOfEstimatedCalls; }

   TR::Compilation *comp()              { return _inliner->comp(); }
   TR_InlinerTracer *tracer()          { return _tracer; }

   protected:

   virtual bool estimateCodeSize(TR_CallTarget *, TR_CallStack * , bool recurseDown = true) = 0;

   bool isInlineable(TR_CallStack *, TR_CallSite *callsite);

   void markIsCold(flags8_t * flags, int32_t i);

   bool returnCleanup(int32_t);      // common tasks requiring completion before returning from estimation

   enum
      {
      bbStart        = 0x01,
      isCold         = 0x02,
      isBranch         = 0x04,
      isUnsanitizeable = 0x08,
      };

   /* Fields */

   bool _isLeaf;
   bool _foundThrow;
   bool _hasExceptionHandlers;
   bool _mayHaveVirtualCallProfileInfo;
   bool _aggressivelyInlineThrows;
   int32_t _throwCount;

   int32_t _recursionDepth;
   bool _recursedTooDeep;

   int32_t _sizeThreshold;
   int32_t _realSize;        // size once we know if we're doing a partial inline or not
   int32_t _error;

   int32_t _totalBCSize;     // Pure accumulation of the bytecode size. Used by HW-based inlining.

   TR_InlinerBase * _inliner;
   TR_InlinerTracer *_tracer;

   int32_t _numOfEstimatedCalls;
   bool _hasNonColdCalls;
   };

#endif
