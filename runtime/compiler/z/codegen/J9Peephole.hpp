/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#ifndef J9_Z_PEEPHOLE_INCL
#define J9_Z_PEEPHOLE_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_PEEPHOLE_CONNECTOR
#define J9_PEEPHOLE_CONNECTOR
   namespace J9 { namespace Z { class Peephole; } }
   namespace J9 {typedef J9::Z::Peephole PeepholeConnector; }
#else
   #error J9::Z::Peephole expected to be a primary connector, but a J9 connector is already defined
#endif

#include "codegen/OMRPeephole.hpp"

namespace J9
{

namespace Z
{

class OMR_EXTENSIBLE Peephole : public OMR::PeepholeConnector
   {
   public:

   Peephole(TR::Compilation* comp);

   virtual bool performOnInstruction(TR::Instruction* cursor);

   private:
      
   /** \brief
    *     Reloads the literal pool register in the catch block when literal pool on demand is turned off. This is
    *     required because the signal handler will take control of the execution and we are not guaranteed that
    *     the literal pool register will remaind valid once control is transferred back to the catch block.
    *
    *  \param cursor
    *     The instruction cursor currently being processed.
    */
   void reloadLiteralPoolRegisterForCatchBlock(TR::Instruction* cursor);
   };
}

}

#endif
