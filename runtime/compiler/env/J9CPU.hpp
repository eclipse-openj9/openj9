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

#ifndef J9_CPU_INCL
#define J9_CPU_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_CPU_CONNECTOR
#define J9_CPU_CONNECTOR
namespace J9 { class CPU; }
namespace J9 { typedef CPU CPUConnector; }
#endif

#include "env/OMRCPU.hpp"

namespace J9
{
class OMR_EXTENSIBLE CPU : public OMR::CPUConnector
   {
protected:

   CPU() : OMR::CPUConnector() {}
   CPU(const OMRProcessorDesc& processorDescription) : OMR::CPUConnector(processorDescription) {}

   /**
    * @brief Contains the list of processor features exploited by the compiler, initialized via TR::CPU::initializeFeatureMasks()
    */
   static OMRProcessorDesc _supportedFeatureMasks;

   /**
    * @brief _isSupportedFeatureMasksEnabled tells you whether _supportedFeatureMasks was used for masking out unused processor features
    */
   static bool _isSupportedFeatureMasksEnabled;

public:

   /** 
    * @brief A factory method used to construct a CPU object based on the underlying hardware
    * @param[in] omrPortLib : the port library
    * @return TR::CPU
    */
   static TR::CPU detect(OMRPortLibrary * const omrPortLib);

   /** 
    * @brief A factory method used to construct a CPU object based on user customized processorDescription
    * @param[in] OMRProcessorDesc : the processor description
    * @return TR::CPU
    */
   static TR::CPU customize(OMRProcessorDesc processorDescription);

   /**
    * @brief Intialize _supportedFeatureMasks to the list of processor features that will be exploited by the compiler and set _isSupportedFeatureMasksEnabled to true
    * @return void
    */
   static void enableFeatureMasks();

   bool supportsFeature(uint32_t feature);
   
   const char *getProcessorVendorId();
   uint32_t getProcessorSignature();
   };
}

#endif
