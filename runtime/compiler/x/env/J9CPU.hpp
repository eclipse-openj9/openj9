/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef J9_X86_CPU_INCL
#define J9_X86_CPU_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_CPU_CONNECTOR
#define J9_CPU_CONNECTOR
namespace J9 { namespace X86 { class CPU; } }
namespace J9 { typedef J9::X86::CPU CPUConnector; }
#else
#error J9::X86::CPU expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/env/J9CPU.hpp"
#include "env/ProcessorInfo.hpp"

namespace J9
{

namespace X86
{

class OMR_EXTENSIBLE CPU : public J9::CPU
   {
protected:

   CPU() : J9::CPU() {}
   CPU(const OMRProcessorDesc& processorDescription) : J9::CPU(processorDescription) {}

public:
   /** 
    * @brief Returns the processor type and features that will be used by portable AOT compilations
    * @param[in] omrPortLib : the port library
    * @return TR::CPU
    */
   static TR::CPU detectRelocatable(OMRPortLibrary * const omrPortLib);

   TR_X86CPUIDBuffer *queryX86TargetCPUID();
   const char * getProcessorVendorId();
   uint32_t getProcessorSignature();

   bool testOSForSSESupport() { return true; } // VM guarantees SSE/SSE2 are available
   bool hasPopulationCountInstruction();

   bool isCompatible(const OMRProcessorDesc& processorDescription);
   uint32_t getX86ProcessorFeatureFlags();
   uint32_t getX86ProcessorFeatureFlags2();
   uint32_t getX86ProcessorFeatureFlags8();

   bool is_test(OMRProcessorArchitecture p);
   bool supports_feature_test(uint32_t feature);
   };

}

}

#endif
