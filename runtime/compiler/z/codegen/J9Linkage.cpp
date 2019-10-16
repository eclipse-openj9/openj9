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

 #include "codegen/Linkage.hpp"
 #include "codegen/Linkage_inlines.hpp"
 #include "codegen/S390GenerateInstructions.hpp"

TR::Instruction *
J9::Z::Linkage::loadUpArguments(TR::Instruction * cursor)
   {
   TR::RealRegister * stackPtr = getStackPointerRealRegister();
   TR::ResolvedMethodSymbol * bodySymbol = comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol> paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol * paramCursor = paramIterator.getFirst();
   TR::Node * firstNode = comp()->getStartTree()->getNode();
   int32_t numIntArgs = 0, numFloatArgs = 0;
   uint32_t binLocalRegs = 0x1<<14;   // Binary pattern representing reg14 as free for local alloc

   bool isRecompilable = false;
   if (comp()->getRecompilationInfo() && comp()->getRecompilationInfo()->couldBeCompiledAgain())
      {
      isRecompilable = true;
      }

   while ((paramCursor != NULL) && ((numIntArgs < getNumIntegerArgumentRegisters()) || (numFloatArgs < getNumFloatArgumentRegisters())))
      {
      TR::RealRegister * argRegister;
      int32_t offset = paramCursor->getParameterOffset();

      // If FSD, the JIT will conservatively store all parameters to stack later in saveArguments.
      // Hence, we need to load the value into a parameter register for I2J transitions.
      bool hasToLoadFromStack = paramCursor->isReferencedParameter() || paramCursor->isParmHasToBeOnStack() || comp()->getOption(TR_FullSpeedDebug);

      switch (paramCursor->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (hasToLoadFromStack && numIntArgs < getNumIntegerArgumentRegisters())
               {
               argRegister = getRealRegister(getIntegerArgumentRegister(numIntArgs));
               TR::MemoryReference* memRef = generateS390MemoryReference(stackPtr, offset, cg());
               cursor = generateRXInstruction(cg(), TR::InstOpCode::L, firstNode, argRegister,
                           memRef, cursor);
               cursor->setBinLocalFreeRegs(binLocalRegs);
               }
            numIntArgs++;
            break;
         case TR::Address:
            if ((hasToLoadFromStack || isRecompilable) &&
                 numIntArgs < getNumIntegerArgumentRegisters())
               {
               argRegister = getRealRegister(getIntegerArgumentRegister(numIntArgs));
               TR::MemoryReference* memRef = generateS390MemoryReference(stackPtr, offset, cg());
               cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), firstNode, argRegister,
                           memRef, cursor);
               cursor->setBinLocalFreeRegs(binLocalRegs);
               }
            numIntArgs++;
            break;
         case TR::Int64:
            if (hasToLoadFromStack && numIntArgs < getNumIntegerArgumentRegisters())
               {
               argRegister = getRealRegister(getIntegerArgumentRegister(numIntArgs));
               TR::MemoryReference* memRef = generateS390MemoryReference(stackPtr, offset, cg());
               cursor = generateRXInstruction(cg(), TR::InstOpCode::getLoadOpCode(), firstNode, argRegister,
                           memRef, cursor);
               cursor->setBinLocalFreeRegs(binLocalRegs);
               if (TR::Compiler->target.is32Bit() && numIntArgs < getNumIntegerArgumentRegisters() - 1)
                  {
                  argRegister = getRealRegister(getIntegerArgumentRegister(numIntArgs + 1));
                  cursor = generateRXInstruction(cg(), TR::InstOpCode::L, firstNode, argRegister,
                              generateS390MemoryReference(stackPtr, offset + 4, cg()), cursor);
                  cursor->setBinLocalFreeRegs(binLocalRegs);
                  }
               }
            numIntArgs += (TR::Compiler->target.is64Bit()) ? 1 : 2;
            break;
         case TR::Float:
         case TR::DecimalFloat:
            if (hasToLoadFromStack && numFloatArgs < getNumFloatArgumentRegisters())
               {
               argRegister = getRealRegister(getFloatArgumentRegister(numFloatArgs));
               TR::MemoryReference* memRef = generateS390MemoryReference(stackPtr, offset, cg());
               cursor = generateRXInstruction(cg(), TR::InstOpCode::LE, firstNode, argRegister,
                           memRef, cursor);
               cursor->setBinLocalFreeRegs(binLocalRegs);
               }
            numFloatArgs++;
            break;
         case TR::Double:
         case TR::DecimalDouble:
            if (hasToLoadFromStack && numFloatArgs < getNumFloatArgumentRegisters())
               {
               argRegister = getRealRegister(getFloatArgumentRegister(numFloatArgs));
               TR::MemoryReference* memRef = generateS390MemoryReference(stackPtr, offset, cg());
               cursor = generateRXInstruction(cg(), TR::InstOpCode::LD, firstNode, argRegister,
                           memRef, cursor);
               cursor->setBinLocalFreeRegs(binLocalRegs);
               }
            numFloatArgs++;
            break;
         }
      paramCursor = paramIterator.getNext();
      }
   return cursor;
   }
