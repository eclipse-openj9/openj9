/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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

#ifndef MJIT_BYTECODEITERATOR_INCL
#define MJIT_BYTECODEITERATOR_INCL
#include "ilgen/J9ByteCodeIterator.hpp"

namespace MJIT
{

class ByteCodeIterator : public TR_J9ByteCodeIterator
   {

   public:
      ByteCodeIterator(TR::ResolvedMethodSymbol *methodSymbol, TR_ResolvedJ9Method *method, TR_J9VMBase *fe, TR::Compilation *comp)
         : TR_J9ByteCodeIterator(methodSymbol, method, fe, comp)
         {};

      void printByteCodes()
         {
         this->printByteCodePrologue();
         for (TR_J9ByteCode bc = this->first(); bc != J9BCunknown; bc = this->next())
            {
            this->printByteCode();
            }
         this->printByteCodeEpilogue();
         }

      const char *currentMnemonic()
         {
         uint8_t opcode = nextByte(0);
         return  ((TR_J9VMBase *)fe())->getByteCodeName(opcode);
         }

      uint8_t currentOpcode()
         {
         return nextByte(0);
         }
   };

} // namespace MJIT
#endif /* MJIT_BYTECODEITERATOR_INCL */
