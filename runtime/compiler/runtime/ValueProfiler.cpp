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

#include <algorithm>
#include <math.h>
#include <stdint.h>
#include "j9.h"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "runtime/asmprotos.h"
#include "runtime/J9Runtime.hpp"
#include "runtime/J9ValueProfiler.hpp"
#include "AtomicSupport.hpp"

/**
 * Construct a ByteInfo. Used to profile UTF16 strings.
 * String will become persistently allocated only through the copy constructor.
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

   // Remove entries that are not compatible with the callee class
   // Also replace class with method in _value
   for (auto iterClass = vec.begin(); iterClass != vec.end(); ++iterClass)
      {
      if (!iterClass->_value)
         continue;

      TR_OpaqueClassBlock *methodClass = reinterpret_cast<TR_OpaqueClassBlock *>(iterClass->_value);
      if (comp->fej9()->isInstanceOf(methodClass, calleeClass, true, true, true))
         {
         TR_ResolvedMethod *method = callerMethod->getResolvedVirtualMethod(comp, methodClass, vftSlot);
         iterClass->_value = method;
         }
      else
         {
         iterClass->_value = 0;
         iterClass->_frequency = 0;
         }
      }

   // Add the frequencies of the same method to the first entry containing the method in the vector
   for (auto iterMethod = vec.begin(); iterMethod != vec.end(); ++iterMethod)
      {
      if (!iterMethod->_value)
         continue;

      TR_ResolvedMethod* method = reinterpret_cast<TR_ResolvedMethod *>(iterMethod->_value);
      for (auto iter = iterMethod + 1; iter != vec.end(); ++iter)
         {
         if (iter->_value && method->isSameMethod(reinterpret_cast<TR_ResolvedMethod *>(iter->_value)))
            {
            iterMethod->_frequency += iter->_frequency;
            iter->_value = 0;
            iter->_frequency = 0;
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
      if (!iter->_value)
         continue;

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

/**
 * Lock access to the map, blocking if necessary.
 */
void
TR_AbstractHashTableProfilerInfo::lock()
   {
   while (!tryLock())
      {
      VM_AtomicSupport::nop();
      }
   }

/**
 * Try to lock access to the map.
 *
 * \param disableJITAccess This will trigger the short circuit test in jitted code.
 */
bool
TR_AbstractHashTableProfilerInfo::tryLock(bool disableJITAccess)
   {
   // Set lock and, if desired, clear high bit in other index
   MetaData locked = _metaData;
   if (disableJITAccess && locked.otherIndex < 0)
      locked.otherIndex = ~locked.otherIndex;
   locked.lock = 1;

   // Current state must be unlocked
   MetaData unlocked = _metaData;
   unlocked.lock = 0;

   return unlocked.rawData == VM_AtomicSupport::lockCompareExchangeU32((uint32_t*) &_metaData, unlocked.rawData, locked.rawData);
   }

/**
 * Unlock the map.
 *
 * \param enableJITAccess This will disable the short circuit test in jitted code.
 */
void
TR_AbstractHashTableProfilerInfo::unlock(bool enableJITAccess)
   {
   MetaData locked = _metaData;
   MetaData unlocked = _metaData;

   do {
      locked.rawData = _metaData.rawData;

      // Clear lock and, if desired, set high bit in other index
      unlocked.rawData = _metaData.rawData;
      if (enableJITAccess && unlocked.otherIndex > -1)
         unlocked.otherIndex = ~unlocked.otherIndex;
      unlocked.lock = 0;
      }
   while (locked.rawData != VM_AtomicSupport::lockCompareExchangeU32((uint32_t*) &_metaData, locked.rawData, unlocked.rawData));
   }

#if defined(TR_HOST_POWER)
JIT_HELPER(_jitProfileValue);
JIT_HELPER(_jitProfileLongValue);
JIT_HELPER(_jitProfileBigDecimalValue);
JIT_HELPER(_jitProfileStringValue);
JIT_HELPER(_jitProfileAddress);
JIT_HELPER(_jitProfileWarmCompilePICAddress);
JIT_HELPER(_jProfile32BitValue);
JIT_HELPER(_jProfile64BitValue);

void PPCinitializeValueProfiler()
   {
   #define SET runtimeHelpers.setAddress
   SET(TR_jitProfileWarmCompilePICAddress, (void *) _jitProfileWarmCompilePICAddress, TR_Helper);
   SET(TR_jitProfileAddress,               (void *) _jitProfileAddress,               TR_Helper);
   SET(TR_jitProfileValue,                 (void *) _jitProfileValue,                 TR_Helper);
   SET(TR_jitProfileLongValue,             (void *) _jitProfileLongValue,             TR_Helper);
   SET(TR_jitProfileBigDecimalValue,       (void *) _jitProfileBigDecimalValue,       TR_Helper);
   SET(TR_jitProfileStringValue,           (void *) _jitProfileStringValue,           TR_Helper);
   SET(TR_jProfile32BitValue,              (void *) _jProfile32BitValue,              TR_Helper);
   SET(TR_jProfile64BitValue,              (void *) _jProfile64BitValue,              TR_Helper);
   }
#endif
