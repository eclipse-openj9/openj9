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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <algorithm>
#include <math.h>
#include <stdint.h>
#include "j9.h"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "runtime/Runtime.hpp"
#include "runtime/asmprotos.h"
#include "runtime/J9ValueProfiler.hpp"

/**
 * Construct a ByteInfo. Used to profile UTF16 strings.
 * String will become persistently allocated only through the copy constuctor.
 * Assignment and custom constructors will just set fields.
 */
TR_ByteInfo::TR_ByteInfo(const TR_ByteInfo &orig) : length(orig.length), chars(NULL)
   {
   if (orig.chars && orig.length > 0)
      {
      chars = (char const *) TR_Memory::jitPersistentAlloc(orig.length * sizeof(char));
      memcpy((char*) chars, orig.chars, orig.length);
      }
   }

/**
 * Deallocate the persistent char list.
 */
TR_ByteInfo::~TR_ByteInfo()
   {
   if (chars)
      TR_Memory::jitPersistentFree((void*)chars);
   }

/**
 * Wrap a TR_AbstractProfilerInfo in its matching TR_AbstractInfo,
 * based on its kind.
 *
 * \param region Region to allocate the result in.
 * \return Allocated TR_AbstractInfo to extract profiling information.
 */
TR_AbstractInfo *
TR_AbstractProfilerInfo::getAbstractInfo(TR::Region &region)
   {
   TR_AbstractInfo *valueInfo = NULL;
   switch (getKind())
      {
      case ValueInfo:
         valueInfo = new (region) TR_ValueInfo(this);
         break;
      case LongValueInfo:
         valueInfo = new (region) TR_LongValueInfo(this);
         break;
      case AddressInfo:
         valueInfo = new (region) TR_AddressInfo(this);
         break;
      case BigDecimalInfo:
         valueInfo = new (region) TR_BigDecimalValueInfo(this);
         break;
      case StringInfo:
         valueInfo = new (region) TR_StringValueInfo(this);
         break;
      default:
         break;
      }

   return valueInfo;
   }

/**
 * Return scale and set flag.
 */
int32_t
TR_BigDecimalValueInfo::getTopValue(int32_t &flag)
   {
   TR_BigDecimalInfo bdi;
   if (!TR_GenericValueInfo<uint64_t>::getTopValue(bdi.value))
      return 0;
   flag = bdi.flag;
   return bdi.scale;
   }

/**
 * Convert each class to the method at the specified vft slot.
 * It will not sort the results.
 */
void
TR_AddressInfo::getMethodsList(TR::Compilation *comp, TR_ResolvedMethod *callerMethod, TR_OpaqueClassBlock *calleeClass, int32_t vftSlot, MethodVector &vec)
   {
   if (!calleeClass)
      return;

   // Grab a copy of the profiling information
   _profiler->getList((Vector&) vec);

   auto iterMethodEnd = vec.begin();

   size_t methods = 0;
   for (auto iterClass = vec.begin(); iterClass != vec.end(); ++iterClass)
      {
      if (!iterClass->_value)
         continue;

      TR_OpaqueClassBlock *methodClass = reinterpret_cast<TR_OpaqueClassBlock *>(iterClass->_value);
      if (comp->fej9()->isInstanceOf(methodClass, calleeClass, true, true, true))
         {
         TR_ResolvedMethod *method = callerMethod->getResolvedVirtualMethod(comp, methodClass, vftSlot);

         auto iterMethod = vec.begin();
         for (; iterMethod != iterMethodEnd; ++iterMethod)
            {
            if (iterMethod->_value == method)
               { 
               iterMethod->_frequency += iterClass->_frequency;
               break;
               }
            }

         if (iterMethod == iterMethodEnd)
            {
            iterMethod->_value = method;
            iterMethodEnd = iterMethod;
            }
         }
      }
   }

void
TR_AddressInfo::getMethodsList(TR::Compilation *comp, TR_ResolvedMethod *callerMethod, TR_OpaqueClassBlock *calleeClass, int32_t vftSlot, List<ProfiledMethod> *sortedList)
   {
   ListElement<ProfiledMethod> *listHead = NULL, *tail = NULL;

   TR::Region &region = comp->trMemory()->currentStackRegion();
   auto vec = new (region) MethodVector(region);
   getMethodsList(comp, callerMethod, calleeClass, vftSlot, *vec);

   for (auto iter = vec->begin(); iter != vec->end(); ++iter)
      {
	ListElement<ProfiledMethod> *newProfiledValue = new (comp->trStackMemory()) ListElement<ProfiledMethod>(&(*iter));
      if (tail)
         tail->setNextElement(newProfiledValue);
      else
         listHead = newProfiledValue;
      tail = newProfiledValue;
      }

   sortedList->setListHead(listHead);
   }

template <> void
TR_LinkedListProfilerInfo<TR_ByteInfo>::dumpInfo(TR::FILE *logFile)
   {
   OMR::CriticalSection lock(vpMonitor);

   trfprintf(logFile, "   Linked List Profiling Info %p\n", this);
   trfprintf(logFile, "   Kind: %d BCI: %d:%d\n Values:\n", _kind,
      TR_AbstractProfilerInfo::getByteCodeInfo().getCallerIndex(),
      TR_AbstractProfilerInfo::getByteCodeInfo().getByteCodeIndex());

   size_t count = 0;
   for (auto iter = getFirst(); iter; iter = iter->getNext())
      trfprintf(logFile, "    %d: %d %s", count++, iter->_frequency, iter->_value.chars);

   trfprintf(logFile, "   Num: %d Total Frequency: %d\n", count, getTotalFrequency());
   }

#if defined(TR_HOST_POWER)
JIT_HELPER(_jitProfileValue);
JIT_HELPER(_jitProfileLongValue);
JIT_HELPER(_jitProfileBigDecimalValue);
JIT_HELPER(_jitProfileStringValue);
JIT_HELPER(_jitProfileAddress);
JIT_HELPER(_jitProfileWarmCompilePICAddress);

void PPCinitializeValueProfiler()
   {
   #define SET runtimeHelpers.setAddress
   SET(TR_jitProfileWarmCompilePICAddress, (void *) _jitProfileWarmCompilePICAddress, TR_Helper);
   SET(TR_jitProfileAddress,               (void *) _jitProfileAddress,               TR_Helper);
   SET(TR_jitProfileValue,                 (void *) _jitProfileValue,                 TR_Helper);
   SET(TR_jitProfileLongValue,             (void *) _jitProfileLongValue,             TR_Helper);
   SET(TR_jitProfileBigDecimalValue,       (void *) _jitProfileBigDecimalValue,       TR_Helper);
   SET(TR_jitProfileStringValue,           (void *) _jitProfileStringValue,           TR_Helper);
   }
#endif
