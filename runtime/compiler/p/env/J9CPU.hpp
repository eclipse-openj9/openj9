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

#ifndef J9_POWER_CPU_INCL
#define J9_POWER_CPU_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_CPU_CONNECTOR
#define J9_CPU_CONNECTOR
namespace J9 { namespace Power { class CPU; } }
namespace J9 { typedef J9::Power::CPU CPUConnector; }
#else
#error J9::Power::CPU expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/env/J9CPU.hpp"
#include "env/ProcessorInfo.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

namespace J9
{

namespace Power
{

class OMR_EXTENSIBLE CPU : public J9::CPU
   {
protected:

   CPU() : J9::CPU() {}
   CPU(const OMRProcessorDesc& processorDescription) : J9::CPU(processorDescription) {}

public:

   bool isCompatible(const OMRProcessorDesc& processorDescription);
   void applyUserOptions();
   };

}

}

#endif
