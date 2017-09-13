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
#include "infra/CriticalSection.hpp"

#define MAX_NUM_VALUES_PROFILED 20
#define MAX_NUM_ADDRESSES_PROFILED 20
#ifdef TR_HOST_64BIT
#define HIGH_ORDER_BIT (0x8000000000000000)
#else
#define HIGH_ORDER_BIT (0x80000000)
#endif

TR_ExtraValueInfo *TR_ExtraValueInfo::create(uint32_t value, uint32_t frequency, uintptrj_t totalFrequency)
     {
     TR_ExtraValueInfo *extraValueInfo = (TR_ExtraValueInfo *)jitPersistentAlloc(sizeof(TR_ExtraValueInfo));
     if (extraValueInfo)
        {
        if (((uintptrj_t) extraValueInfo) & 0x00000001)
           TR_ASSERT(0, "Extra value info pointer must be 2 byte aligned at least for value profiler to behave correctly\n");
        extraValueInfo->_value = value;
        extraValueInfo->_frequency = frequency;
        extraValueInfo->_totalFrequency = totalFrequency;
        }

     return extraValueInfo;
     }

TR_ExtraLongValueInfo *TR_ExtraLongValueInfo::create(uint64_t value, uint32_t frequency, uintptrj_t totalFrequency)
     {
     TR_ExtraLongValueInfo *extraValueInfo = (TR_ExtraLongValueInfo *)jitPersistentAlloc(sizeof(TR_ExtraLongValueInfo));
     if (extraValueInfo)
        {
        if (((uintptrj_t) extraValueInfo) & 0x00000001)
           TR_ASSERT(0, "Extra value info pointer must be 2 byte aligned at least for value profiler to behave correctly\n");
        extraValueInfo->_value = value;
        extraValueInfo->_frequency = frequency;
        extraValueInfo->_totalFrequency = totalFrequency;
        }

     return extraValueInfo;
     }


TR_ExtraBigDecimalValueInfo *TR_ExtraBigDecimalValueInfo::create(int32_t scale, int32_t flag, uint32_t frequency, uintptrj_t totalFrequency)
     {
     TR_ExtraBigDecimalValueInfo *extraValueInfo = (TR_ExtraBigDecimalValueInfo *)jitPersistentAlloc(sizeof(TR_ExtraBigDecimalValueInfo));
     if (extraValueInfo)
        {
        if (((uintptrj_t) extraValueInfo) & 0x00000001)
           TR_ASSERT(0, "Extra value info pointer must be 2 byte aligned at least for value profiler to behave correctly\n");
        extraValueInfo->_scale = scale;
        extraValueInfo->_flag = flag;
        extraValueInfo->_frequency = frequency;
        extraValueInfo->_totalFrequency = totalFrequency;
        }

     return extraValueInfo;
     }



TR_ExtraStringValueInfo *TR_ExtraStringValueInfo::create(char *chars, int32_t length, uint32_t frequency, uintptrj_t totalFrequency)
     {
     TR_ExtraStringValueInfo *extraValueInfo = (TR_ExtraStringValueInfo *)jitPersistentAlloc(sizeof(TR_ExtraStringValueInfo));
     if (extraValueInfo)
        {
        if (((uintptrj_t) extraValueInfo) & 0x00000001)
           TR_ASSERT(0, "Extra value info pointer must be 2 byte aligned at least for value profiler to behave correctly\n");
        char *newChars = (char *)jitPersistentAlloc(2*length*sizeof(char));
        memcpy(newChars, chars, 2*length);
        extraValueInfo->_chars = newChars;
        extraValueInfo->_length = length;
        extraValueInfo->_frequency = frequency;
        extraValueInfo->_totalFrequency = totalFrequency;
        }

     return extraValueInfo;
     }


TR_ExtraAddressInfo *TR_ExtraAddressInfo::create(uintptrj_t value, uint32_t frequency, uintptrj_t totalFrequency)
     {
     TR_ExtraAddressInfo *extraAddressInfo = (TR_ExtraAddressInfo *)jitPersistentAlloc(sizeof(TR_ExtraAddressInfo));
     if (extraAddressInfo)
        {
        if (((uintptrj_t) extraAddressInfo) & 0x00000001)
           TR_ASSERT(0, "Extra value info pointer must be 2 byte aligned at least for value profiler to behave correctly\n");
        extraAddressInfo->_value = value;
        extraAddressInfo->_frequency = frequency;
        extraAddressInfo->_totalFrequency = totalFrequency;
        }

     return extraAddressInfo;
     }


uint32_t TR_ExtraAbstractInfo::getTotalFrequency(uintptrj_t **addrOfTotalFrequency)
   {
   OMR::CriticalSection gettingTotalFrequency(vpMonitor);
   uint32_t initialTotalFrequency = (_totalFrequency & ~HIGH_ORDER_BIT);
   uint32_t totalFrequency = initialTotalFrequency;

   TR_ExtraAbstractInfo *extraInfo = this; // (TR_ExtraValueInfo *) initialTotalFrequency;
   while (extraInfo->_totalFrequency & HIGH_ORDER_BIT)
     extraInfo = (TR_ExtraAbstractInfo *) (extraInfo->_totalFrequency << 1);
   totalFrequency = extraInfo->_totalFrequency;
   if (addrOfTotalFrequency)
     *addrOfTotalFrequency = &(extraInfo->_totalFrequency);
   return totalFrequency;
   }



void TR_ExtraValueInfo::incrementOrCreateExtraValueInfo(uint32_t value, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumValuesProfiled)
   {
   OMR::CriticalSection incrementingOrCreatingExtraValueInfo(vpMonitor);
   uintptrj_t totalFrequency;
   if (*addrOfTotalFrequency)
      totalFrequency = **addrOfTotalFrequency;
   else
      totalFrequency = getTotalFrequency(addrOfTotalFrequency);

   if (totalFrequency == ~HIGH_ORDER_BIT)
      {
      TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
      return;
      }

   int32_t numDistinctValuesProfiled = 0;
   TR_ExtraValueInfo *cursorExtraInfo = this;
   while (cursorExtraInfo)
      {
      if ((cursorExtraInfo->_value == value) ||
	  (cursorExtraInfo->_frequency == 0))
         {
	 if (cursorExtraInfo->_frequency == 0)
	    cursorExtraInfo->_value = value;

	 cursorExtraInfo->_frequency++;
	 totalFrequency++;
	 **addrOfTotalFrequency = totalFrequency;
	 return;
	 }

      numDistinctValuesProfiled++;
      if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	cursorExtraInfo = (TR_ExtraValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
      else
	break;
      }

   maxNumValuesProfiled = std::min<uint32_t>(maxNumValuesProfiled, MAX_NUM_VALUES_PROFILED);
   if (numDistinctValuesProfiled <= maxNumValuesProfiled)
      {
      TR_ExtraValueInfo *newExtraInfo = TR_ExtraValueInfo::create(value, 1, ++totalFrequency);
      if (newExtraInfo)
         {
         cursorExtraInfo->_totalFrequency = (uintptrj_t) newExtraInfo;
         cursorExtraInfo->_totalFrequency = ((cursorExtraInfo->_totalFrequency>>1) | HIGH_ORDER_BIT);
         cursorExtraInfo = newExtraInfo;
         }
      else
         cursorExtraInfo->_totalFrequency = totalFrequency;
      }
   else
      {
      totalFrequency++;
      **addrOfTotalFrequency = totalFrequency;
      }

   *addrOfTotalFrequency = &(cursorExtraInfo->_totalFrequency);
   }


void TR_ExtraLongValueInfo::incrementOrCreateExtraLongValueInfo(uint64_t value, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumValuesProfiled)
   {
   OMR::CriticalSection incrementingOrCreatingExtraLongValueInfo(vpMonitor);
   uintptrj_t totalFrequency;
   if (*addrOfTotalFrequency)
      totalFrequency = **addrOfTotalFrequency;
   else
      totalFrequency = getTotalFrequency(addrOfTotalFrequency);

   if (totalFrequency == ~HIGH_ORDER_BIT)
      {
      TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
      return;
      }

   int32_t numDistinctValuesProfiled = 0;
   TR_ExtraLongValueInfo *cursorExtraInfo = this;
   while (cursorExtraInfo)
      {
      if ((cursorExtraInfo->_value == value) ||
	  (cursorExtraInfo->_frequency == 0))
         {
	 if (cursorExtraInfo->_frequency == 0)
	    cursorExtraInfo->_value = value;

	 cursorExtraInfo->_frequency++;
	 totalFrequency++;
	 **addrOfTotalFrequency = totalFrequency;
	 return;
	 }

      numDistinctValuesProfiled++;
      if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	cursorExtraInfo = (TR_ExtraLongValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
      else
	break;
      }

   maxNumValuesProfiled = std::min<uint32_t>(maxNumValuesProfiled, MAX_NUM_VALUES_PROFILED);
   if (numDistinctValuesProfiled <= maxNumValuesProfiled)
      {
      TR_ExtraLongValueInfo *newExtraInfo = TR_ExtraLongValueInfo::create(value, 1, ++totalFrequency);
       if (newExtraInfo)
         {
         cursorExtraInfo->_totalFrequency = (uintptrj_t) newExtraInfo;
         cursorExtraInfo->_totalFrequency = ((cursorExtraInfo->_totalFrequency>>1) | HIGH_ORDER_BIT);
         cursorExtraInfo = newExtraInfo;
         }
      else
         cursorExtraInfo->_totalFrequency = totalFrequency;
      }
   else
      {
      totalFrequency++;
      **addrOfTotalFrequency = totalFrequency;
      }

   *addrOfTotalFrequency = &(cursorExtraInfo->_totalFrequency);
   }


void TR_ExtraBigDecimalValueInfo::incrementOrCreateExtraBigDecimalValueInfo(int32_t scale, int32_t flag, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumValuesProfiled)
   {
   OMR::CriticalSection incrementingOrCreatingExtraBigDecimalValueInfo(vpMonitor);
   uintptrj_t totalFrequency;
   if (*addrOfTotalFrequency)
      totalFrequency = **addrOfTotalFrequency;
   else
      totalFrequency = getTotalFrequency(addrOfTotalFrequency);

   if (totalFrequency == ~HIGH_ORDER_BIT)
      {
      TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
      return;
      }

   int32_t numDistinctValuesProfiled = 0;
   TR_ExtraBigDecimalValueInfo *cursorExtraInfo = this;
   while (cursorExtraInfo)
      {
      if (((cursorExtraInfo->_scale == scale) && (cursorExtraInfo->_flag == flag)) ||
	  (cursorExtraInfo->_frequency == 0))
         {
	 if (cursorExtraInfo->_frequency == 0)
            {
	    cursorExtraInfo->_flag = flag;
            cursorExtraInfo->_scale = scale;
            }

	 cursorExtraInfo->_frequency++;
	 totalFrequency++;
	 **addrOfTotalFrequency = totalFrequency;
	 return;
	 }

      numDistinctValuesProfiled++;
      if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	cursorExtraInfo = (TR_ExtraBigDecimalValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
      else
	break;
      }

   maxNumValuesProfiled = std::min<uint32_t>(maxNumValuesProfiled, MAX_NUM_VALUES_PROFILED);
   if (numDistinctValuesProfiled <= maxNumValuesProfiled)
      {
      TR_ExtraBigDecimalValueInfo *newExtraInfo = TR_ExtraBigDecimalValueInfo::create(scale, flag, 1, ++totalFrequency);
      if (newExtraInfo)
         {
         cursorExtraInfo->_totalFrequency = (uintptrj_t) newExtraInfo;
         cursorExtraInfo->_totalFrequency = ((cursorExtraInfo->_totalFrequency>>1) | HIGH_ORDER_BIT);
         cursorExtraInfo = newExtraInfo;
         }
      else
         cursorExtraInfo->_totalFrequency = totalFrequency;
      }
   else
      {
      totalFrequency++;
      **addrOfTotalFrequency = totalFrequency;
      }

   *addrOfTotalFrequency = &(cursorExtraInfo->_totalFrequency);
   }





char *TR_StringValueInfo::createChars(int32_t length)
   {
   return (char *)jitPersistentAlloc(length*2*sizeof(char));
   }

bool TR_StringValueInfo::matchStrings(char *chars1, int32_t length1, char *chars2, int32_t length2)
   {
   bool match = true;
   if (length1 != length2)
      match = false;
   else
      {
      int32_t i = 0;
      while (i < 2*length1)
         {
         if (chars1[i] != chars2[i])
            {
            match = false;
            break;
            }

         i++;
         }
      }

   return match;
   }


void TR_ExtraStringValueInfo::incrementOrCreateExtraStringValueInfo(char *chars, int32_t length, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumValuesProfiled)
   {
   OMR::CriticalSection incrementingOrCreatingExtraStringValueInfo(vpMonitor);
   uintptrj_t totalFrequency;
   if (*addrOfTotalFrequency)
      totalFrequency = **addrOfTotalFrequency;
   else
      totalFrequency = getTotalFrequency(addrOfTotalFrequency);

   if (totalFrequency == ~HIGH_ORDER_BIT)
      {
      TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
      return;
      }

   int32_t numDistinctValuesProfiled = 0;
   TR_ExtraStringValueInfo *cursorExtraInfo = this;
   while (cursorExtraInfo)
      {
      bool match = true;
      if (cursorExtraInfo->_frequency != 0)
         {
         char *curChars = cursorExtraInfo->_chars;
         int32_t curLength = cursorExtraInfo->_length;
         match = TR_StringValueInfo::matchStrings(curChars, curLength, chars, length);
         }

      if (match)
         {
	 if (cursorExtraInfo->_frequency == 0)
            {
            char *newChars = (char *)jitPersistentAlloc(2*length*sizeof(char));
            memcpy(newChars, chars, 2*length);
	    cursorExtraInfo->_chars = newChars;
            cursorExtraInfo->_length = length;
            }

	 cursorExtraInfo->_frequency++;
	 totalFrequency++;
	 **addrOfTotalFrequency = totalFrequency;
	 return;
	 }

      numDistinctValuesProfiled++;
      if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	cursorExtraInfo = (TR_ExtraStringValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
      else
	break;
      }

   maxNumValuesProfiled = std::min<uint32_t>(maxNumValuesProfiled, MAX_NUM_VALUES_PROFILED);
   if (numDistinctValuesProfiled <= maxNumValuesProfiled)
      {
      TR_ExtraStringValueInfo *newExtraInfo = TR_ExtraStringValueInfo::create(chars, length, 1, ++totalFrequency);
      if (newExtraInfo)
         {
         cursorExtraInfo->_totalFrequency = (uintptrj_t) newExtraInfo;
         cursorExtraInfo->_totalFrequency = ((cursorExtraInfo->_totalFrequency>>1) | HIGH_ORDER_BIT);
         cursorExtraInfo = newExtraInfo;
         }
      else
         cursorExtraInfo->_totalFrequency = totalFrequency;
      }
   else
      {
      totalFrequency++;
      **addrOfTotalFrequency = totalFrequency;
      }

   *addrOfTotalFrequency = &(cursorExtraInfo->_totalFrequency);
   }



void TR_StringValueInfo::incrementOrCreateExtraStringValueInfo(char *chars, int32_t length, uintptrj_t **addrForTotalFrequency, uint32_t maxNumValuesProfiled)
   {
   OMR::CriticalSection incrementingOrCreatingExtraStringValueInfo(vpMonitor);
   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      TR_ExtraStringValueInfo *extraValueInfo = TR_ExtraStringValueInfo::create(chars, length, 0, _totalFrequency);
      if (extraValueInfo)
         {
         _totalFrequency = (uintptrj_t) extraValueInfo;
         _totalFrequency = (_totalFrequency>>1) | HIGH_ORDER_BIT;
         *addrForTotalFrequency = &(extraValueInfo->_totalFrequency);
         }
      else
         {
         _totalFrequency++;
         *addrForTotalFrequency = &_totalFrequency;
         return;
         }
      }

   TR_ExtraStringValueInfo *firstExtraValueInfo = (TR_ExtraStringValueInfo *) (_totalFrequency << 1);
   firstExtraValueInfo->incrementOrCreateExtraStringValueInfo(chars, length, addrForTotalFrequency, maxNumValuesProfiled);
   }




void TR_ExtraAddressInfo::incrementOrCreateExtraAddressInfo(uintptrj_t value, uintptrj_t **addrOfTotalFrequency, uint32_t maxNumAddressesProfiled, uint32_t frequency, bool externalProfileValue)
   {
   OMR::CriticalSection incrementingOrCreatingExtraAddressInfo(vpMonitor);
   uintptrj_t totalFrequency;
   if (*addrOfTotalFrequency)
      totalFrequency = **addrOfTotalFrequency;
   else
      totalFrequency = getTotalFrequency(addrOfTotalFrequency);

   if (totalFrequency == ~HIGH_ORDER_BIT)
      {
      TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
      return;
      }

   int32_t numDistinctAddressesProfiled = 0;
   TR_ExtraAddressInfo *cursorExtraInfo = this;
   while (cursorExtraInfo)
      {
      if ((cursorExtraInfo->_value == value) ||
	  (cursorExtraInfo->_frequency == 0))
         {
	 if (cursorExtraInfo->_frequency == 0)
	    cursorExtraInfo->_value = value;

	 if (externalProfileValue && frequency != 0)
	    {
	    cursorExtraInfo->_frequency = frequency;
	    totalFrequency += frequency;
	    }
	 else
	    {
	    cursorExtraInfo->_frequency++;
	    totalFrequency++;
	    }
	 **addrOfTotalFrequency = totalFrequency;
	 return;
	 }

      numDistinctAddressesProfiled++;
      if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	cursorExtraInfo = (TR_ExtraAddressInfo *) (cursorExtraInfo->_totalFrequency << 1);
      else
	break;
      }

   maxNumAddressesProfiled = std::min<uint32_t>(maxNumAddressesProfiled, MAX_NUM_ADDRESSES_PROFILED);
   if (numDistinctAddressesProfiled <= maxNumAddressesProfiled)
      {
      uint32_t tmpFrequency;
      if (externalProfileValue && frequency != 0)
         {
	 tmpFrequency = frequency;
	 totalFrequency += frequency;
         }
      else
         {
         tmpFrequency = 1;
	 totalFrequency += 1;
         }
      TR_ExtraAddressInfo *newExtraInfo = TR_ExtraAddressInfo::create(value, tmpFrequency, totalFrequency);
      if (newExtraInfo)
         {
         cursorExtraInfo->_totalFrequency = (uintptrj_t) newExtraInfo;
         cursorExtraInfo->_totalFrequency = ((cursorExtraInfo->_totalFrequency>>1) | HIGH_ORDER_BIT);
         cursorExtraInfo = newExtraInfo;
         }
      else
         cursorExtraInfo->_totalFrequency = totalFrequency;
      }
   else
      {
      totalFrequency++;
      **addrOfTotalFrequency = totalFrequency;
      }

   *addrOfTotalFrequency = &(cursorExtraInfo->_totalFrequency);
   }

uint32_t TR_AbstractInfo::getTotalFrequency(uintptrj_t **addrOfTotalFrequency)
   {
   OMR::CriticalSection gettingTotalFrequency(vpMonitor);

   uintptrj_t initialTotalFrequency = (_totalFrequency & ~HIGH_ORDER_BIT);
   uintptrj_t totalFrequency = initialTotalFrequency;

   if (addrOfTotalFrequency)
      *addrOfTotalFrequency = &(_totalFrequency);

   if (_totalFrequency & HIGH_ORDER_BIT)
      {
      TR_ExtraAbstractInfo *extraInfo = (TR_ExtraAbstractInfo *) (_totalFrequency << 1);
      totalFrequency = extraInfo->getTotalFrequency(addrOfTotalFrequency);
      }

   return totalFrequency;
   }



void TR_ValueInfo::incrementOrCreateExtraValueInfo(uint32_t value, uintptrj_t **addrForTotalFrequency, uint32_t maxNumValuesProfiled)
   {
   OMR::CriticalSection incrementingOrCreatingExtraValueInfo(vpMonitor);
   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      TR_ExtraValueInfo *extraValueInfo = TR_ExtraValueInfo::create(value, 0, _totalFrequency);
      if (extraValueInfo)
         {
         _totalFrequency = (uintptrj_t) extraValueInfo;
         _totalFrequency = (_totalFrequency>>1) | HIGH_ORDER_BIT;
         *addrForTotalFrequency = &(extraValueInfo->_totalFrequency);
         }
      else
         {
         _totalFrequency++;
         *addrForTotalFrequency = &_totalFrequency;
         return;
         }
      }

   TR_ExtraValueInfo *firstExtraValueInfo = (TR_ExtraValueInfo *) (_totalFrequency << 1);
   firstExtraValueInfo->incrementOrCreateExtraValueInfo(value, addrForTotalFrequency, maxNumValuesProfiled);
   }


void TR_LongValueInfo::incrementOrCreateExtraLongValueInfo(uint64_t value, uintptrj_t **addrForTotalFrequency, uint32_t maxNumValuesProfiled)
   {
   OMR::CriticalSection incrementingOrCreatingExtraLongValueInfo(vpMonitor);

   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      TR_ExtraLongValueInfo *extraValueInfo = TR_ExtraLongValueInfo::create(value, 0, _totalFrequency);
      if (extraValueInfo)
         {
         _totalFrequency = (uintptrj_t) extraValueInfo;
         _totalFrequency = (_totalFrequency>>1) | HIGH_ORDER_BIT;
         *addrForTotalFrequency = &(extraValueInfo->_totalFrequency);
         }
      else
         {
         _totalFrequency++;
         *addrForTotalFrequency = &_totalFrequency;
         return;
         }
      }

   TR_ExtraLongValueInfo *firstExtraValueInfo = (TR_ExtraLongValueInfo *) (_totalFrequency << 1);
   firstExtraValueInfo->incrementOrCreateExtraLongValueInfo(value, addrForTotalFrequency, maxNumValuesProfiled);
   }


void TR_BigDecimalValueInfo::incrementOrCreateExtraBigDecimalValueInfo(int32_t scale, int32_t flag, uintptrj_t **addrForTotalFrequency, uint32_t maxNumValuesProfiled)
   {
   OMR::CriticalSection incrementingOrCreatingExtraBigDecimalValueInfo(vpMonitor);

   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      TR_ExtraBigDecimalValueInfo *extraValueInfo = TR_ExtraBigDecimalValueInfo::create(scale, flag, 0, _totalFrequency);
      if (extraValueInfo)
         {
         _totalFrequency = (uintptrj_t) extraValueInfo;
         _totalFrequency = (_totalFrequency>>1) | HIGH_ORDER_BIT;
         *addrForTotalFrequency = &(extraValueInfo->_totalFrequency);
         }
      else
         {
         _totalFrequency++;
         *addrForTotalFrequency = &_totalFrequency;
         return;
         }
      }

   TR_ExtraBigDecimalValueInfo *firstExtraValueInfo = (TR_ExtraBigDecimalValueInfo *) (_totalFrequency << 1);
   firstExtraValueInfo->incrementOrCreateExtraBigDecimalValueInfo(scale, flag, addrForTotalFrequency, maxNumValuesProfiled);
   }


void TR_AddressInfo::incrementOrCreateExtraAddressInfo(uintptrj_t value, uintptrj_t **addrForTotalFrequency, uint32_t maxNumAddressesProfiled, uint32_t frequency, bool externalProfileValue)
   {
   OMR::CriticalSection incrementingOrCreatingExtraAddressInfo(vpMonitor);

   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      TR_ExtraAddressInfo *extraAddressInfo = TR_ExtraAddressInfo::create(value, frequency, _totalFrequency);
       if (extraAddressInfo)
         {
         _totalFrequency = (uintptrj_t) extraAddressInfo;
         _totalFrequency = (_totalFrequency>>1) | HIGH_ORDER_BIT;
         *addrForTotalFrequency = &(extraAddressInfo->_totalFrequency);
         }
      else
         {
         _totalFrequency++;
         *addrForTotalFrequency = &_totalFrequency;
         return;
         }
      }

   TR_ExtraAddressInfo *firstExtraAddressInfo = (TR_ExtraAddressInfo *) (_totalFrequency << 1);
   firstExtraAddressInfo->incrementOrCreateExtraAddressInfo(value, addrForTotalFrequency, maxNumAddressesProfiled, frequency, externalProfileValue);
   }


uint32_t TR_ValueInfo::getTopValue()
   {
   OMR::CriticalSection gettingTopValue(vpMonitor);

   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      uintptrj_t v = _value1;
      return v;
      }
   else
      {
      uint32_t topValue = _value1;
      uint32_t topFrequency = _frequency1;

      TR_ExtraValueInfo *cursorExtraInfo = (TR_ExtraValueInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > topFrequency)
	    {
	    topFrequency = cursorExtraInfo->_frequency;
	    topValue = cursorExtraInfo->_value;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	   cursorExtraInfo = (TR_ExtraValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    break;
	 }
      return topValue;
      }

   TR_ASSERT(0, "Unexpected path");
   return 0;
   }



uint64_t TR_LongValueInfo::getTopValue()
   {
   OMR::CriticalSection gettingTopValue(vpMonitor);

   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      uint64_t v = _value1;
      return v;
      }
   else
      {
      uint64_t topValue = _value1;
      uint32_t topFrequency = _frequency1;

      TR_ExtraLongValueInfo *cursorExtraInfo = (TR_ExtraLongValueInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > topFrequency)
	    {
	    topFrequency = cursorExtraInfo->_frequency;
	    topValue = cursorExtraInfo->_value;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	   cursorExtraInfo = (TR_ExtraLongValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    break;
	 }
      return topValue;
      }

   TR_ASSERT(0, "Unexpected Path");
   return 0;
   }


/**
 * Get the top flag and scale values.
 *
 * @param       flag    The top flag.
 * @returns     The top scale.
 */
int32_t TR_BigDecimalValueInfo::getTopValue(int32_t &flag)
   {
   OMR::CriticalSection gettingTopValue(vpMonitor);

   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      int32_t v = _scale1;
      flag = _flag1;
      return v;
      }
   else
      {
      int32_t topScale = _scale1;
      int32_t topFlag = _flag1;

      uint32_t topFrequency = _frequency1;

      TR_ExtraBigDecimalValueInfo *cursorExtraInfo = (TR_ExtraBigDecimalValueInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > topFrequency)
	    {
	    topFrequency = cursorExtraInfo->_frequency;
	    topScale = cursorExtraInfo->_scale;
            topFlag = cursorExtraInfo->_flag;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	   cursorExtraInfo = (TR_ExtraBigDecimalValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    break;
	 }

      flag = topFlag;
      return topScale;
      }

   TR_ASSERT(0, "Unexpected Path");
   return 0;
   }



uintptrj_t TR_AddressInfo::getTopValue()
   {
   OMR::CriticalSection gettingTopValue(vpMonitor);

   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      uintptrj_t v = _value1;
      return v;
      }
   else
      {
      uintptrj_t topValue = _value1;
      uint32_t topFrequency = _frequency1;

      TR_ExtraAddressInfo *cursorExtraInfo = (TR_ExtraAddressInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > topFrequency)
	    {
	    topFrequency = cursorExtraInfo->_frequency;
	    topValue = cursorExtraInfo->_value;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	   cursorExtraInfo = (TR_ExtraAddressInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    break;
	 }
      return topValue;
      }

   TR_ASSERT(0, "Unexpected Path");
   return 0;
   }



char *TR_StringValueInfo::getTopValue(int32_t &length)
   {
   OMR::CriticalSection gettingTopValue(vpMonitor);

   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      char *v = _chars1;
      length = _length1;
      return v;
      }
   else
      {
      char *topChars = _chars1;
      int32_t topLength = _length1;

      uint32_t topFrequency = _frequency1;

      TR_ExtraStringValueInfo *cursorExtraInfo = (TR_ExtraStringValueInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > topFrequency)
	    {
	    topFrequency = cursorExtraInfo->_frequency;
	    topChars = cursorExtraInfo->_chars;
            topLength = cursorExtraInfo->_length;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	   cursorExtraInfo = (TR_ExtraStringValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    break;
	 }

      length = topLength;
      return topChars;
      }

   // if a path to this exit is ever created, make sure the VP mutex is released!
   TR_ASSERT(0, "need to release VP mutex");
   return 0;
   }


uintptrj_t TR_WarmCompilePICAddressInfo::getTopValue()
   {
   OMR::CriticalSection gettingTopValue(vpMonitor);

   uintptrj_t topFrequency = _frequency[0];

   for (int32_t i=1; i<MAX_UNLOCKED_PROFILING_VALUES; i++)
      {
      if (_frequency[i] > topFrequency)
         topFrequency = _frequency[i];
      }

   return topFrequency;
   }


float TR_AbstractInfo::getTopProbability()
   {
   // Could have implemented this in a cleaner manner by calling
   // getTopValue and getTotalFrequency but that would involve two walks
   // over the linked list; so below implementation was chosen.
   //
   OMR::CriticalSection gettingTopProbability(vpMonitor);

   if (!(_totalFrequency & HIGH_ORDER_BIT))
      {
      uint32_t totalFrequency = _totalFrequency;
      uint32_t frequency1 = _frequency1;

      if (totalFrequency == 0) return 0;
      return ((float)frequency1) / totalFrequency;
      }
   else
      {
      //uint32_t topValue = _value1;
      uint32_t topFrequency = _frequency1;
      uint32_t totalFrequency = 0;

      TR_ExtraAbstractInfo *cursorExtraInfo = (TR_ExtraAbstractInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > topFrequency)
	    {
	    topFrequency = cursorExtraInfo->_frequency;
	    //topValue = cursorExtraInfo->_value;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	   cursorExtraInfo = (TR_ExtraAbstractInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    {
	    totalFrequency = cursorExtraInfo->_totalFrequency;
	    break;
	    }
	 }

      if (totalFrequency == 0)
	return 0;
      return ((float) topFrequency) / totalFrequency;
      }

   TR_ASSERT(0, "Unexpected path");
   return 0;
   }

float TR_WarmCompilePICAddressInfo::getTopProbability()
   {
   OMR::CriticalSection gettingTopProbability(vpMonitor);

   uint32_t topFrequency = _frequency[0];
   uint32_t totalFrequency = 0;

   for (int32_t i=1; i<MAX_UNLOCKED_PROFILING_VALUES; i++)
     {
     if (_frequency[i] > topFrequency)
        topFrequency = _frequency[i];
     }

   totalFrequency = _totalFrequency;
   if (totalFrequency == 0)
      return 0;
   return ((float) topFrequency) / totalFrequency;
   }



uint32_t TR_AbstractInfo::getNumProfiledValues()
   {
   OMR::CriticalSection gettingNumberOfProfiledValues(vpMonitor);

   uint32_t numValues = 0;
   if (_frequency1 > 0)
      numValues++;

   if (_totalFrequency & HIGH_ORDER_BIT)
      {
	TR_ExtraAbstractInfo *cursorExtraInfo = (TR_ExtraAbstractInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > 0)
	    numValues++;

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	    cursorExtraInfo = (TR_ExtraAbstractInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    break;
	 }
      }

   return numValues;
   }

uint32_t TR_WarmCompilePICAddressInfo::getNumProfiledValues()
   {
   OMR::CriticalSection gettingNumberOfProfiledValues(vpMonitor);

   uint32_t numValues = 0;
   for (int32_t i=0; i<MAX_UNLOCKED_PROFILING_VALUES; i++)
     {
     if (_frequency[i] > 0)
        numValues++;
     }

   return numValues;
   }


void TR_ValueInfo::getSortedList(TR::Compilation * comp, List<TR_ExtraAbstractInfo> *sortedValues)
   {
   OMR::CriticalSection gettingSortedList(vpMonitor);

   ListElement<TR_ExtraValueInfo> *listHead = NULL;
   TR_ExtraValueInfo *currValueInfo = NULL;

   if (_frequency1 > 0)
      {
      currValueInfo = (TR_ExtraValueInfo *) comp->trMemory()->allocateStackMemory(sizeof(TR_ExtraValueInfo));
      currValueInfo->_frequency = _frequency1;
      currValueInfo->_value = _value1;
      sortedValues->add(currValueInfo);
      listHead = (ListElement<TR_ExtraValueInfo> *) sortedValues->getListHead();
      }

   TR_AbstractInfo::getSortedList(comp, sortedValues, (ListElement<TR_ExtraAbstractInfo> *)listHead);
   }



void TR_LongValueInfo::getSortedList(TR::Compilation * comp, List<TR_ExtraAbstractInfo> *sortedValues)
   {
   OMR::CriticalSection gettingSortedList(vpMonitor);

   ListElement<TR_ExtraLongValueInfo> *listHead = NULL;
   TR_ExtraLongValueInfo *currValueInfo = NULL;

   if (_frequency1 > 0)
      {
      currValueInfo = (TR_ExtraLongValueInfo *) comp->trMemory()->allocateStackMemory(sizeof(TR_ExtraLongValueInfo));
      currValueInfo->_frequency = _frequency1;
      currValueInfo->_value = _value1;
      sortedValues->add(currValueInfo);
      listHead = (ListElement<TR_ExtraLongValueInfo> *) sortedValues->getListHead();
      }

   TR_AbstractInfo::getSortedList(comp, sortedValues, (ListElement<TR_ExtraAbstractInfo> *)listHead);
   }


void TR_BigDecimalValueInfo::getSortedList(TR::Compilation * comp, List<TR_ExtraAbstractInfo> *sortedValues)
   {
   OMR::CriticalSection gettingSortedList(vpMonitor);

   ListElement<TR_ExtraBigDecimalValueInfo> *listHead = NULL;
   TR_ExtraBigDecimalValueInfo *currValueInfo = NULL;

   if (_frequency1 > 0)
      {
      currValueInfo = (TR_ExtraBigDecimalValueInfo *) comp->trMemory()->allocateStackMemory(sizeof(TR_ExtraBigDecimalValueInfo));
      currValueInfo->_frequency = _frequency1;
      currValueInfo->_scale = _scale1;
      currValueInfo->_flag = _flag1;
      sortedValues->add(currValueInfo);
      listHead = (ListElement<TR_ExtraBigDecimalValueInfo> *) sortedValues->getListHead();
      }

   TR_AbstractInfo::getSortedList(comp, sortedValues, (ListElement<TR_ExtraAbstractInfo> *)listHead);
   }



void TR_StringValueInfo::getSortedList(TR::Compilation * comp, List<TR_ExtraAbstractInfo> *sortedValues)
   {
   OMR::CriticalSection gettingSortedList(vpMonitor);

   ListElement<TR_ExtraStringValueInfo> *listHead = NULL;
   TR_ExtraStringValueInfo *currValueInfo = NULL;

   if (_frequency1 > 0)
      {
      currValueInfo = (TR_ExtraStringValueInfo *) comp->trMemory()->allocateStackMemory(sizeof(TR_ExtraStringValueInfo));
      currValueInfo->_frequency = _frequency1;
      currValueInfo->_chars = _chars1;
      currValueInfo->_length = _length1;
      sortedValues->add(currValueInfo);
      listHead = (ListElement<TR_ExtraStringValueInfo> *) sortedValues->getListHead();
      }

   TR_AbstractInfo::getSortedList(comp, sortedValues, (ListElement<TR_ExtraAbstractInfo> *)listHead);
   }


void TR_AddressInfo::getSortedList(TR::Compilation * comp, List<TR_ExtraAbstractInfo> *sortedAddresses)
   {
   OMR::CriticalSection gettingSortedList(vpMonitor);

   ListElement<TR_ExtraAddressInfo> *listHead = NULL;
   TR_ExtraAddressInfo *currAddressInfo = NULL;

   if (_frequency1 > 0)
      {
      currAddressInfo = (TR_ExtraAddressInfo *) comp->trMemory()->allocateStackMemory(sizeof(TR_ExtraAddressInfo));
      currAddressInfo->_frequency = _frequency1;
      currAddressInfo->_value = _value1;
      sortedAddresses->add(currAddressInfo);
      listHead = (ListElement<TR_ExtraAddressInfo> *)sortedAddresses->getListHead();
      }

   TR_AbstractInfo::getSortedList(comp, sortedAddresses, (ListElement<TR_ExtraAbstractInfo> *)listHead);
   }

void TR_AddressInfo::getMethodsList(TR::Compilation *comp, TR_ResolvedMethod *callerMethod, TR_OpaqueClassBlock *calleeClass, int32_t vftSlot, List<TR_ExtraAbstractInfo> *methods)
   {
   if (!calleeClass)
      return;


   ListElement<TR_ExtraAddressInfo> *listHead = NULL;
   TR_ExtraAddressInfo *currAddressInfo = NULL;
   uint32_t frequency1 = 0;
   TR_OpaqueClassBlock *profiledClass = 0;

   {
   OMR::CriticalSection getClassAndFrequency(vpMonitor);
   frequency1 = _frequency1;
   profiledClass = (TR_OpaqueClassBlock*)_value1;
   }

   if (_frequency1 == 0) return;

   if (comp->fej9()->isInstanceOf(profiledClass, calleeClass, true, true, true))
      {
      currAddressInfo = (TR_ExtraAddressInfo *) comp->trMemory()->allocateStackMemory(sizeof(TR_ExtraAddressInfo));
      currAddressInfo->_frequency = frequency1;
      TR_ResolvedMethod *method = callerMethod->getResolvedVirtualMethod(comp, profiledClass, vftSlot);
      currAddressInfo->_value = (uintptrj_t)method;
      methods->add(currAddressInfo);
      listHead = (ListElement<TR_ExtraAddressInfo> *)methods->getListHead();
      }

   for (
      ClassAndFrequency currentInfo = getInitialClassAndFrequency();
      currentInfo.cursor != 0;
      currentInfo = getNextClassAndFrequency(currentInfo)
      )
      {
      if (comp->fej9()->isInstanceOf(currentInfo.methodClass, calleeClass, true, true, true))
         {
         TR_ResolvedMethod *method = callerMethod->getResolvedVirtualMethod(comp, currentInfo.methodClass, vftSlot);
         ListElement<TR_ExtraAddressInfo> *cursor = listHead;
         bool found = false;
         while (cursor)
            {
            if (((TR_ResolvedMethod *)(cursor->getData()->_value))->isSameMethod(method))
               {
               cursor->getData()->_frequency += currentInfo.frequency;
               found = true;
               break;
               }

            cursor = cursor->getNextElement();
            }

         if (!found)
            {
            TR_ExtraAddressInfo *addressInfoCopy = (TR_ExtraAddressInfo *) comp->trMemory()->allocateStackMemory(sizeof(TR_ExtraAddressInfo));
            addressInfoCopy->_frequency = currentInfo.frequency;
            addressInfoCopy->_value = (uintptrj_t)method;
            methods->add(addressInfoCopy);
            }
         }
      }
   }

TR_AddressInfo::ClassAndFrequency
TR_AddressInfo::getInitialClassAndFrequency()
   {
   OMR::CriticalSection gettingInitialClassAndFrequency(vpMonitor);
   return findEntry(_totalFrequency);
   }


TR_AddressInfo::ClassAndFrequency
TR_AddressInfo::getNextClassAndFrequency(const ClassAndFrequency &currentInfo)
   {
   OMR::CriticalSection gettingNextClassAndFrequency(vpMonitor);
   return findEntry(currentInfo.cursor->_totalFrequency);
   }

TR_AddressInfo::ClassAndFrequency
TR_AddressInfo::findEntry(uintptr_t totalFrequencyOrExtraInfo)
   {
   TR_ExtraAddressInfo *cursorExtraInfo = 0;
   uint32_t frequency = 0;
   do
      {
      cursorExtraInfo = (totalFrequencyOrExtraInfo & HIGH_ORDER_BIT) ?
         reinterpret_cast<TR_ExtraAddressInfo *>(totalFrequencyOrExtraInfo << 1) :
         0;
      if (cursorExtraInfo == 0) break;
      frequency = cursorExtraInfo->_frequency;
      totalFrequencyOrExtraInfo = cursorExtraInfo->_totalFrequency;
      } while (frequency == 0);
   TR_OpaqueClassBlock *methodClass = (cursorExtraInfo != 0) && (frequency != 0) ?
      reinterpret_cast<TR_OpaqueClassBlock *>(cursorExtraInfo->_value) :
      0;
   return ClassAndFrequency(cursorExtraInfo, methodClass, frequency);
   }

void TR_WarmCompilePICAddressInfo::getSortedList(TR::Compilation * comp, List<TR_ExtraAbstractInfo> *sortedAddresses)
   {
   OMR::CriticalSection gettingSortedList(vpMonitor);
   ListElement<TR_ExtraAddressInfo> *listHead = NULL;
   TR_ExtraAddressInfo *currAddressInfo = NULL;

   for (int32_t i=0; i<MAX_UNLOCKED_PROFILING_VALUES; i++)
     {
     if (_frequency[i] > 0)
        {
        currAddressInfo = (TR_ExtraAddressInfo *) comp->trMemory()->allocateStackMemory(sizeof(TR_ExtraAddressInfo));
        currAddressInfo->_frequency = _frequency[i];
        currAddressInfo->_value = _address[i];
        if (!listHead)
           {
           sortedAddresses->add(currAddressInfo);
           listHead = (ListElement<TR_ExtraAddressInfo> *)sortedAddresses->getListHead();
           }
        else
           insertInSortedList(comp, currAddressInfo, (ListElement<TR_ExtraAbstractInfo> *&)listHead);
        }
     }

   //sortedAddresses->setListHead((ListElement<TR_ExtraAbstractInfo> *)listHead);
   }


void TR_AbstractInfo::getSortedList(TR::Compilation * comp, List<TR_ExtraAbstractInfo> *sortedValues,
                                      ListElement<TR_ExtraAbstractInfo> *listHead )
   {
   OMR::CriticalSection gettingSortedList(vpMonitor);

   if (_totalFrequency & HIGH_ORDER_BIT)
      {
        TR_ExtraAbstractInfo *cursorExtraInfo = (TR_ExtraAbstractInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
         {
         if (cursorExtraInfo->_frequency > 0)
            insertInSortedList(comp, cursorExtraInfo, listHead);

         if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
           cursorExtraInfo = (TR_ExtraValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
         else
            break;
         }
      }
   sortedValues->setListHead(listHead);
   }


void TR_AbstractInfo::insertInSortedList(TR::Compilation * comp, TR_ExtraAbstractInfo *currInfo, ListElement<TR_ExtraAbstractInfo> *&listHead)
   {
   OMR::CriticalSection insertingInSortedList(vpMonitor);

   ListElement<TR_ExtraAbstractInfo> *prevCursor = NULL;
   ListElement<TR_ExtraAbstractInfo> *cursor = listHead;
   while (cursor)
      {
      if (cursor->getData()->_frequency < currInfo->_frequency)
	 {
	 ListElement<TR_ExtraAbstractInfo> *currListElement = new (comp->trStackMemory()) ListElement<TR_ExtraAbstractInfo>(currInfo);
	 if (prevCursor)
	    prevCursor->setNextElement(currListElement);
	 else
	    listHead = currListElement;
	 currListElement->setNextElement(cursor);
	 return;
	 }
      prevCursor = cursor;
      cursor = cursor->getNextElement();
      }

   ListElement<TR_ExtraAbstractInfo> *currListElement = new (comp->trStackMemory()) ListElement<TR_ExtraAbstractInfo>(currInfo);
   if (prevCursor)
      prevCursor->setNextElement(currListElement);
   else
      listHead = currListElement;
   currListElement->setNextElement(cursor);
   }



void TR_LongValueInfo::print()
   {
   OMR::CriticalSection printing(vpMonitor);
   uint32_t numValues = 0;

   if (_frequency1 > 0)
      {
      printf("Frequency = %d Value = %x\n", _frequency1, _value1);
      numValues++;
      }

   if (_totalFrequency & HIGH_ORDER_BIT)
      {
      TR_ExtraLongValueInfo *cursorExtraInfo = (TR_ExtraLongValueInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > 0)
	    {
	    printf("Frequency = %d Value = %x\n", cursorExtraInfo->_frequency, cursorExtraInfo->_value);
	    numValues++;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	    cursorExtraInfo = (TR_ExtraLongValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    {
	    printf("Total frequency = %d\n", cursorExtraInfo->_totalFrequency);
	    break;
	    }
	 }

      }

   printf("Number of values = %d\n", numValues);
   }



void TR_BigDecimalValueInfo::print()
   {
   OMR::CriticalSection printing(vpMonitor);

   uint32_t numValues = 0;

   if (_frequency1 > 0)
      {
      printf("Frequency = %d Scale = %x\n", _frequency1, _scale1);
      numValues++;
      }

   if (_totalFrequency & HIGH_ORDER_BIT)
      {
      TR_ExtraBigDecimalValueInfo *cursorExtraInfo = (TR_ExtraBigDecimalValueInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > 0)
	    {
	    printf("Frequency = %d Scale = %x Flag = %x\n", cursorExtraInfo->_frequency, cursorExtraInfo->_scale, cursorExtraInfo->_flag);
	    numValues++;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	    cursorExtraInfo = (TR_ExtraBigDecimalValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    {
	    printf("Total frequency = %d\n", cursorExtraInfo->_totalFrequency);
	    break;
	    }
	 }

      }
   printf("Number of values = %d\n", numValues);
   }



void TR_StringValueInfo::print()
   {
   OMR::CriticalSection printing(vpMonitor);

   uint32_t numValues = 0;

   if (_frequency1 > 0)
      {
      printf("Frequency = %d length = %d\n", _frequency1, _length1);

      int32_t i = 0;
      while (i < 2*_length1)
         {
         if ((i % 2) == 0)
            printf("%c", _chars1[i]);
         i++;
         }

            printf("\n");
            fflush(stdout);
      numValues++;
      }

   if (_totalFrequency & HIGH_ORDER_BIT)
      {
      TR_ExtraStringValueInfo *cursorExtraInfo = (TR_ExtraStringValueInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > 0)
	    {
	    printf("Frequency = %d length = %d\n", cursorExtraInfo->_frequency, cursorExtraInfo->_length);

            int32_t i = 0;
            while (i < 2*cursorExtraInfo->_length)
               {
               if ((i % 2) == 0)
                  printf("%c", cursorExtraInfo->_chars[i]);
               i++;
               }

            printf("\n");
            fflush(stdout);


	    numValues++;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	    cursorExtraInfo = (TR_ExtraStringValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    {
	    printf("Total frequency = %d\n", cursorExtraInfo->_totalFrequency);
	    break;
	    }
	 }

      }

   printf("Number of values = %d\n", numValues);
   }



void TR_ValueInfo::print()
   {
   OMR::CriticalSection printing(vpMonitor);

   uint32_t numValues = 0;

   if (_frequency1 > 0)
      {
      printf("Frequency = %d Value = %x\n", _frequency1, _value1);
      numValues++;
      }

   if (_totalFrequency & HIGH_ORDER_BIT)
      {
      TR_ExtraValueInfo *cursorExtraInfo = (TR_ExtraValueInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
	 {
	 if (cursorExtraInfo->_frequency > 0)
	    {
	    printf("Frequency = %d Value = %x\n", cursorExtraInfo->_frequency, cursorExtraInfo->_value);
	    numValues++;
	    }

	 if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
	    cursorExtraInfo = (TR_ExtraValueInfo *) (cursorExtraInfo->_totalFrequency << 1);
	 else
	    {
	    printf("Total frequency = %d\n", cursorExtraInfo->_totalFrequency);
	    break;
	    }
	 }

      }
   printf("Number of values = %d\n", numValues);
   }

void TR_WarmCompilePICAddressInfo::print()
   {
   OMR::CriticalSection printing(vpMonitor);
   uint32_t numValues = 0;

   for (int32_t i=0; i<MAX_UNLOCKED_PROFILING_VALUES; i++)
     {
     if (_frequency[i] > 0)
        {
        printf("Frequency = %d Value = %x\n", _frequency[i], _address[i]);
        numValues++;
        }
   }

   printf("Total frequency = %d\n", _totalFrequency);
   printf("Number of values = %d\n", numValues);
   }


void TR_AddressInfo::print()
   {
   OMR::CriticalSection printing(vpMonitor);
   uint32_t numValues = 0;

   if (_frequency1 > 0)
      {
      printf("Frequency = %d Value = %x\n", _frequency1, _value1);
      numValues++;
      }

   if (_totalFrequency & HIGH_ORDER_BIT)
      {
      TR_ExtraAddressInfo *cursorExtraInfo = (TR_ExtraAddressInfo *) (_totalFrequency << 1);
      while (cursorExtraInfo)
         {
         if (cursorExtraInfo->_frequency > 0)
            {
            printf("Frequency = %d Value = %x\n", cursorExtraInfo->_frequency, cursorExtraInfo->_value);
            numValues++;
            }

         if (cursorExtraInfo->_totalFrequency & HIGH_ORDER_BIT)
            cursorExtraInfo = (TR_ExtraAddressInfo *) (cursorExtraInfo->_totalFrequency << 1);
         else
            {
            printf("Total frequency = %d\n", cursorExtraInfo->_totalFrequency);
            break;
            }
         }

      }
   printf("Number of values = %d\n", numValues);
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
