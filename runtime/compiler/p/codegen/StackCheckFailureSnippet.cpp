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

#include "p/codegen/StackCheckFailureSnippet.hpp"

#include "codegen/CodeGenerator.hpp"
#include "codegen/GCStackAtlas.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/jittypes.h"
#include "env/VMJ9.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "runtime/CodeCacheManager.hpp"

uint8_t *storeArgumentItem(TR::InstOpCode::Mnemonic op, uint8_t *buffer, TR::RealRegister *reg, int32_t offset, TR::CodeGenerator *cg)
   {
   TR::RealRegister *stackPtr = cg->getLinkage()->cg()->getStackPointerRegister();
   TR::InstOpCode       opCode(op);
   opCode.copyBinaryToBuffer(buffer);
   reg->setRegisterFieldRS(toPPCCursor(buffer));
   stackPtr->setRegisterFieldRA(toPPCCursor(buffer));
   *toPPCCursor(buffer) |= offset & 0x0000ffff;
   return(buffer+4);
   }

uint8_t *loadArgumentItem(TR::InstOpCode::Mnemonic op, uint8_t *buffer, TR::RealRegister *reg, int32_t offset, TR::CodeGenerator *cg)
   {
   TR::RealRegister *stackPtr = cg->getLinkage()->cg()->getStackPointerRegister();
   TR::InstOpCode       opCode(op);
   opCode.copyBinaryToBuffer(buffer);
   reg->setRegisterFieldRT(toPPCCursor(buffer));
   stackPtr->setRegisterFieldRA(toPPCCursor(buffer));
   *toPPCCursor(buffer) |= offset & 0x0000ffff;
   return(buffer+4);
   }

uint8_t *TR::PPCStackCheckFailureSnippet::emitSnippetBody()
   {
   TR::Compilation * comp = cg()->comp();
   TR::ResolvedMethodSymbol *bodySymbol=comp->getJittedMethodSymbol();
   TR::Machine *machine = cg()->machine();
   TR::SymbolReference  *sofRef  = comp->getSymRefTab()->findOrCreateStackOverflowSymbolRef(comp->getJittedMethodSymbol());
   TR::MethodSymbol *sof = sofRef->getSymbol()->castToMethodSymbol();

   int32_t              offsetAdjust=cg()->getFrameSizeInBytes();
   ListIterator<TR::ParameterSymbol> paramIterator(&(bodySymbol->getParameterList()));
   TR::ParameterSymbol  *paramCursor = paramIterator.getFirst();
   const TR::PPCLinkageProperties &linkage = cg()->getLinkage()->getProperties();

   uint8_t             *buffer = cg()->getBinaryBufferCursor();
   uint8_t             *bufferStart = buffer;
   uint8_t             *returnLocation;
   bool                 saveLR = (cg()->getSnippetList().size()<=1 &&
                                  !bodySymbol->isEHAware()         &&
                                  !cg()->canExceptByTrap()         &&
                                  !machine->getLinkRegisterKilled());
   TR::InstOpCode         add_opCode(TR::InstOpCode::add);
   TR::InstOpCode         addi_opCode(TR::InstOpCode::addi);
   TR::InstOpCode         subf_opCode(TR::InstOpCode::subf);
   TR::RealRegister  *stackPtr = cg()->getLinkage()->cg()->getStackPointerRegister();
   TR::RealRegister  *gr12 = machine->getRealRegister(TR::RealRegister::gr12);

   getSnippetLabel()->setCodeLocation(buffer);

   if (offsetAdjust)
      {
      if (offsetAdjust > UPPER_IMMED)	// addi is not applicable
         {
         // prologue arranged for gr12 to contain offsetAdjust
         // add sp, sp, gr12
         add_opCode.copyBinaryToBuffer(buffer);
         stackPtr->setRegisterFieldRT(toPPCCursor(buffer));
         stackPtr->setRegisterFieldRA(toPPCCursor(buffer));
         gr12->setRegisterFieldRB(toPPCCursor(buffer));
         buffer += 4;
         }
      else
         {
         // addi sp, sp, framesize
         addi_opCode.copyBinaryToBuffer(buffer);
         stackPtr->setRegisterFieldRT(toPPCCursor(buffer));
         stackPtr->setRegisterFieldRA(toPPCCursor(buffer));
         *toPPCCursor(buffer) |= offsetAdjust & 0x0000ffff;
         buffer += 4;
         // pass framesize to jitStackOverflow in gr12
         // addi gr12, gr0, framesize (ie li gr12, framesize)
         addi_opCode.copyBinaryToBuffer(buffer);
         gr12->setRegisterFieldRT(toPPCCursor(buffer));
         *toPPCCursor(buffer) |= offsetAdjust & 0x0000ffff;
         buffer += 4;
         }
      }
   else
      {
      if (saveLR)
         {
         //  addi sp, sp, -sizeOfJavaPointer
         addi_opCode.copyBinaryToBuffer(buffer);
         stackPtr->setRegisterFieldRT(toPPCCursor(buffer));
         stackPtr->setRegisterFieldRA(toPPCCursor(buffer));
         *toPPCCursor(buffer) |= (-TR::Compiler->om.sizeofReferenceAddress()) & 0x0000ffff;
         buffer += 4;
         }
      // pass framesize to jitStackOverflow in gr12
      // addi gr12, gr0, framesize (ie li gr12, framesize)
      addi_opCode.copyBinaryToBuffer(buffer);
      gr12->setRegisterFieldRT(toPPCCursor(buffer));
      *toPPCCursor(buffer) |= offsetAdjust & 0x0000ffff;
      buffer += 4;
      }

   if (saveLR)
      {
      // mflr gr0
      *(int32_t *)buffer = 0x7c0802a6;
      buffer += 4;
      }

   if (saveLR)
      {
      if (TR::Compiler->target.is64Bit())
        // std [gr14, 0], gr0
        *(int32_t *)buffer = 0xf80e0000;
      else
        // stw [gr14, 0], gr0
        *(int32_t *)buffer = 0x900e0000;
      buffer += 4;
      }

   intptrj_t helperAddress = (intptrj_t)sof->getMethodAddress();
   if (cg()->directCallRequiresTrampoline(helperAddress, (intptrj_t)buffer))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(sofRef->getReferenceNumber(), (void *)buffer);
      TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange(helperAddress, (intptrj_t)buffer), "Helper address is out of range");
      }

   // bl distance
   *(int32_t *)buffer = 0x48000001 | ((helperAddress - (intptrj_t)buffer) & 0x03ffffff);
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(buffer,
                                                         (uint8_t *)sofRef,
                                                         TR_HelperAddress, cg()),
                        __FILE__, __LINE__, getNode());
   buffer += 4;
   returnLocation = buffer;

   if (saveLR)
      {
      // For FSD, we have to reload the return address
      if (comp->getOption(TR_FullSpeedDebug))
	 {
         if (TR::Compiler->target.is64Bit())
            // ld gr0, [gr14, 0]
            *(int32_t *)buffer = 0xe80e0000;
         else
            // lwz gr0, [gr14, 0]
            *(int32_t *)buffer = 0x800e0000;
         buffer += 4;
	 }

      // mtlr gr0
      *(int32_t *)buffer = 0x7c0803a6;
      buffer += 4;
      }

   if (offsetAdjust)
      {
      if (offsetAdjust > (-LOWER_IMMED))	// addi is not applicable
         {
         // prologue arranged for gr12 to contain offsetAdjust
         // and gr12 is preserved across the call to jitStackOverflow
         // subf sp, gr12, sp
         subf_opCode.copyBinaryToBuffer(buffer);
         stackPtr->setRegisterFieldRT(toPPCCursor(buffer));
         gr12->setRegisterFieldRA(toPPCCursor(buffer));
         stackPtr->setRegisterFieldRB(toPPCCursor(buffer));
         }
      else
         {
         // addi sp, sp, -framesize
         addi_opCode.copyBinaryToBuffer(buffer);
         stackPtr->setRegisterFieldRT(toPPCCursor(buffer));
         stackPtr->setRegisterFieldRA(toPPCCursor(buffer));
         *toPPCCursor(buffer) |= -offsetAdjust & 0x0000ffff;
         }
      buffer += 4;
      }
   else if (saveLR)
      {
      // addi sp, sp, sizeOfJavaPointer
      addi_opCode.copyBinaryToBuffer(buffer);
      stackPtr->setRegisterFieldRT(toPPCCursor(buffer));
      stackPtr->setRegisterFieldRA(toPPCCursor(buffer));
      *toPPCCursor(buffer) |= TR::Compiler->om.sizeofReferenceAddress() & 0x0000ffff;
      buffer += 4;
      }

   // b restartLabel  -- assuming it is less than 64MB away.
   *(int32_t *)buffer = 0x48000000 | (((intptrj_t)getReStartLabel()->getCodeLocation() - (intptrj_t)buffer) & 0x03fffffc);

   TR::GCStackAtlas *atlas = cg()->getStackAtlas();
   if (atlas)
      {
      // only the arg references are live at this point
      uint32_t  numberOfParmSlots = atlas->getNumberOfParmSlotsMapped();
      TR_GCStackMap *map = new (cg()->trHeapMemory(), numberOfParmSlots) TR_GCStackMap(numberOfParmSlots);

      map->copy(atlas->getParameterMap());
      while (paramCursor != NULL)
	 {
         int32_t  intRegArgIndex = paramCursor->getLinkageRegisterIndex();
         if (intRegArgIndex >= 0                   &&
             paramCursor->isReferencedParameter()  &&
             paramCursor->isCollectedReference())
	    {
            // In full speed debug all the parameters are passed in the stack for this case
            // but will also reside in the registers
            if (!comp->getOption(TR_FullSpeedDebug))
               {
               map->resetBit(paramCursor->getGCMapIndex());
               }
            map->setRegisterBits(cg()->registerBitMask(linkage.getIntegerArgumentRegister(intRegArgIndex)));
	    }
         paramCursor = paramIterator.getNext();
	 }

      // set the GC map
      gcMap().setStackMap(map);
      atlas->setParameterMap(map);
      }
   gcMap().registerStackMap(returnLocation, cg());
   return buffer + 4;
   }


void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCStackCheckFailureSnippet * snippet)
   {
   uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Stack Check Failure Snippet");

   TR::ResolvedMethodSymbol *bodySymbol=_comp->getJittedMethodSymbol();
   const TR::PPCLinkageProperties &linkage = _cg->getLinkage()->getProperties();
   TR::Machine *machine  = _cg->machine();
   TR::RealRegister *stackPtr = _cg->getStackPointerRegister();

   bool saveLR = (_cg->getSnippetList().size() <= 1 &&
                  !bodySymbol->isEHAware()                     &&
                  !_cg->canExceptByTrap()              &&
                  !machine->getLinkRegisterKilled());

   int32_t offsetAdjust = _cg->getFrameSizeInBytes();

   if (offsetAdjust)
      {
      if (offsetAdjust > UPPER_IMMED)   // addi is not applicable
         {
         // prologue arranged for gr12 to contain offsetAdjust
         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "add \t");
         print(pOutFile, stackPtr, TR_WordReg);
         trfprintf(pOutFile, ", ");
         print(pOutFile, stackPtr, TR_WordReg);
         trfprintf(pOutFile, ", gr12");
         cursor += 4;
         }
      else
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "addi2 \t");
         print(pOutFile, stackPtr, TR_WordReg);
         trfprintf(pOutFile, ", ");
         print(pOutFile, stackPtr, TR_WordReg);
         trfprintf(pOutFile, ", 0x%x", offsetAdjust);
         cursor += 4;
         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "li \tgr12");
         trfprintf(pOutFile, ", 0x%x", offsetAdjust);
         cursor += 4;
         }
      }
   else
      {
      if (saveLR)
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "addi2 \t");
         print(pOutFile, stackPtr, TR_WordReg);
         trfprintf(pOutFile, ", ");
         print(pOutFile, stackPtr, TR_WordReg);
         trfprintf(pOutFile, ", 0x%x", -TR::Compiler->om.sizeofReferenceAddress());
         cursor += 4;
         }
      printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "li \tgr12");
      trfprintf(pOutFile, ", 0x%x", offsetAdjust);
      cursor += 4;
      }

   if (saveLR)
      {
      printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "mflr \tgr0");
      cursor += 4;
      }

   if (saveLR)
      {
      printPrefix(pOutFile, NULL, cursor, 4);
      if (TR::Compiler->target.is64Bit())
         trfprintf(pOutFile, "std \t[gr14, 0], gr0");
      else
         trfprintf(pOutFile, "stw \t[gr14, 0], gr0");
      cursor += 4;
      }

   char    *info = "";
   int32_t  distance;
   if (isBranchToTrampoline(_comp->getSymRefTab()->element(TR_stackOverflow), cursor, distance))
      info = " Through trampoline";

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;     // sign extend
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s", (intptrj_t)cursor + distance, info);
   cursor += 4;

   if (saveLR)
      {
      printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "mtlr \tgr0");
      cursor += 4;
      }

   if (offsetAdjust)
      {
      if (offsetAdjust > (-LOWER_IMMED))        // addi is not applicable
         {
         // prologue arranged for gr12 to contain offsetAdjust
         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "subf \t");
         print(pOutFile, stackPtr, TR_WordReg);
         trfprintf(pOutFile, ", gr12, ");
         print(pOutFile, stackPtr, TR_WordReg);
         }
      else
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, "addi2 \t");
         print(pOutFile, stackPtr, TR_WordReg);
         trfprintf(pOutFile, ", ");
         print(pOutFile, stackPtr, TR_WordReg);
         trfprintf(pOutFile, ", 0x%x", -offsetAdjust);
         }
      cursor += 4;
      }
   else if (saveLR)
      {
      printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, "addi2 \t");
      print(pOutFile, stackPtr, TR_WordReg);
      trfprintf(pOutFile, ", ");
      print(pOutFile, stackPtr, TR_WordReg);
      trfprintf(pOutFile, ", 0x%x", TR::Compiler->om.sizeofReferenceAddress());
      cursor += 4;
      }

   // b restartLabel  -- assuming it is less than 64MB away.
   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;     // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; Back to ", (intptrj_t)cursor + distance);
   print(pOutFile, snippet->getReStartLabel());
   }

uint32_t TR::PPCStackCheckFailureSnippet::getLength(int32_t estimatedSnippetStart)
   {
   int32_t                length = 0;
   TR::ResolvedMethodSymbol *bodySymbol=cg()->comp()->getJittedMethodSymbol();

   if (cg()->getSnippetList().size()<=1 &&
       !bodySymbol->isEHAware()         &&
       !cg()->canExceptByTrap() &&
       !cg()->machine()->getLinkRegisterKilled())
      {
      length += 12;
      if (cg()->getFrameSizeInBytes() == 0)
        length += 8;
      if (cg()->comp()->getOption(TR_FullSpeedDebug))
         length += 4;
      }

   if (cg()->getFrameSizeInBytes())
      length += 8;
   if (cg()->getFrameSizeInBytes() <= UPPER_IMMED)
      length += 4;  // to load r12 with framesize

   return(length+8);
   }
