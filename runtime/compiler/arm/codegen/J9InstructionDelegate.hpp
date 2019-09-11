/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef J9_ARM_INSTRUCTIONDELEGATE_INCL
#define J9_ARM_INSTRUCTIONDELEGATE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_INSTRUCTIONDELEGATE_CONNECTOR
#define J9_INSTRUCTIONDELEGATE_CONNECTOR
namespace J9 { namespace ARM { class InstructionDelegate; } }
namespace J9 { typedef J9::ARM::InstructionDelegate InstructionDelegateConnector; }
#else
#error J9::ARM::InstructionDelegate expected to be a primary connector, but a J9 connector is already defined
#endif

#include "compiler/codegen/J9InstructionDelegate.hpp"
#include "infra/Annotations.hpp"

namespace J9
{

namespace ARM
{

class OMR_EXTENSIBLE InstructionDelegate : public J9::InstructionDelegate
   {
protected:

   InstructionDelegate() {}

   };

}

}

#endif
