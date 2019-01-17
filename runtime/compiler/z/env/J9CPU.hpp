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

#ifndef J9_Z_CPU_INCL
#define J9_Z_CPU_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_CPU_CONNECTOR
#define J9_CPU_CONNECTOR
namespace J9 { namespace Z { class CPU; } }
namespace J9 { typedef J9::Z::CPU CPUConnector; }
#else
#error J9::Z::CPU expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/env/J9CPU.hpp"
#include "env/ProcessorInfo.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#define PROCESSOR_FEATURES_SIZE 1
typedef struct TR_ProcessorFeatureFlags {
  uint32_t featureFlags[PROCESSOR_FEATURES_SIZE];
} TR_ProcessorFeatureFlags;

#ifdef __cplusplus
}
#endif

namespace J9
{

namespace Z
{


enum S390SupportsArchitectures
   {
   S390SupportsUnknownArch = 0,
   S390SupportsZ900        = 1,
   S390SupportsZ990        = 2,
   S390SupportsZ9          = 3,
   S390SupportsZ10         = 4,
   S390SupportsZ196        = 5,
   S390SupportsZEC12       = 6,
   S390SupportsZ13         = 7,
   S390SupportsZ14         = 8,
   S390SupportsZNext       = 9,
   S390SupportsLatestArch  = S390SupportsZNext
   };


enum // flags
   {
   //                                       = 0x00000001,     // AVAILABLE FOR USE !!
   HasResumableTrapHandler                  = 0x00000002,
   HasFixedFrameC_CallingConvention         = 0x00000004,
   SupportsScaledIndexAddressing            = 0x00000080,
   S390SupportsDFP                          = 0x00000100,
   S390SupportsFPE                          = 0x00000200,
   S390SupportsHPRDebug                     = 0x00001000,
   IsInZOSSupervisorState                   = 0x00008000,
   S390SupportsTM                           = 0x00010000,
   S390SupportsRI                           = 0x00020000,
   S390SupportsVectorFacility               = 0x00040000,
   S390SupportsVectorPackedDecimalFacility  = 0x00080000,
   S390SupportsGuardedStorageFacility       = 0x00100000,
   S390SupportsSideEffectAccessFacility     = 0x00200000,
   DummyLastFlag
   };


class CPU : public J9::CPU
   {
protected:

   CPU() :
         J9::CPU(),
         _supportsArch(S390SupportsUnknownArch),
         _flags(0)
      {}

public:

   bool getS390SupportsArch(S390SupportsArchitectures arch)
      {
      TR_ASSERT(arch >= S390SupportsUnknownArch  && arch <= S390SupportsLatestArch, "Unknown Architecture");
      return _supportsArch >= arch;
      }

   void setS390SupportsArch(S390SupportsArchitectures arch)
      {
      TR_ASSERT(arch >= S390SupportsUnknownArch && arch <= S390SupportsLatestArch, "Unknown Architecture");
      _supportsArch = _supportsArch >= arch ? _supportsArch : arch;
      }

   bool getS390SupportsZ900() {return getS390SupportsArch(S390SupportsZ900);}
   void setS390SupportsZ900() { setS390SupportsArch(S390SupportsZ900);}

   bool getS390SupportsZ990() {return getS390SupportsArch(S390SupportsZ990);}
   void setS390SupportsZ990() { setS390SupportsArch(S390SupportsZ990);}

   bool getS390SupportsZ9() {return getS390SupportsArch(S390SupportsZ9);}
   void setS390SupportsZ9() { setS390SupportsArch(S390SupportsZ9);}

   bool getS390SupportsZ10() {return getS390SupportsArch(S390SupportsZ10);}
   void setS390SupportsZ10() { setS390SupportsArch(S390SupportsZ10);}

   bool getS390SupportsZ196() {return getS390SupportsArch(S390SupportsZ196);}
   void setS390SupportsZ196() { setS390SupportsArch(S390SupportsZ196);}

   bool getS390SupportsZEC12() {return getS390SupportsArch(S390SupportsZEC12);}
   void setS390SupportsZEC12() { setS390SupportsArch(S390SupportsZEC12);}

   bool getS390SupportsZ13() {return getS390SupportsArch(S390SupportsZ13);}
   void setS390SupportsZ13() { setS390SupportsArch(S390SupportsZ13);}

   bool getS390SupportsZ14() {return getS390SupportsArch(S390SupportsZ14);}
   void setS390SupportsZ14() { setS390SupportsArch(S390SupportsZ14);}

   bool getS390SupportsZNext() {return getS390SupportsArch(S390SupportsZNext);}
   void setS390SupportsZNext() { setS390SupportsArch(S390SupportsZNext);}

   bool getS390SupportsHPRDebug() {return _flags.testAny(S390SupportsHPRDebug);}
   void setS390SupportsHPRDebug() { _flags.set(S390SupportsHPRDebug);}

   bool getS390SupportsDFP() {return _flags.testAny(S390SupportsDFP);}
   void setS390SupportsDFP() { _flags.set(S390SupportsDFP);}

   bool getS390SupportsFPE();
   void setS390SupportsFPE() { _flags.set(S390SupportsFPE);}

   bool getS390SupportsTM();
   void setS390SupportsTM() { _flags.set(S390SupportsTM);}

   bool getS390SupportsRI();
   void setS390SupportsRI() { _flags.set(S390SupportsRI);}

   bool getS390SupportsVectorFacility();
   void setS390SupportsVectorFacility() { _flags.set(S390SupportsVectorFacility); }

   bool getS390SupportsVectorPackedDecimalFacility();
   void setS390SupportsVectorPackedDecimalFacility() { _flags.set(S390SupportsVectorPackedDecimalFacility); }

   /** \brief
    *     Determines whether the guarded storage facility is available on the current processor.
    *
    *  \return
    *     <c>true</c> if the guarded storage and side effect access facilities are available on the current processor;
    *     <c>false</c> otherwise.
    */
   bool getS390SupportsGuardedStorageFacility();
   void setS390SupportsGuardedStorageFacility() { _flags.set(S390SupportsGuardedStorageFacility);
                                                  _flags.set(S390SupportsSideEffectAccessFacility);}

   static int32_t TO_PORTLIB_get390MachineId();
   static bool TO_PORTLIB_get390_supportsZNext();
   static bool TO_PORTLIB_get390_supportsZ14();
   static bool TO_PORTLIB_get390_supportsZ13();
   static bool TO_PORTLIB_get390_supportsZ6();
   static bool TO_PORTLIB_get390_supportsZGryphon();
   static bool TO_PORTLIB_get390_supportsZHelix();

   void initializeS390ProcessorFeatures();

   TR_ProcessorFeatureFlags getProcessorFeatureFlags();
   bool isCompatible(TR_Processor processorSignature, TR_ProcessorFeatureFlags processorFeatureFlags);

private:

   S390SupportsArchitectures _supportsArch;

   flags32_t _flags;


   };

}

}

#endif
