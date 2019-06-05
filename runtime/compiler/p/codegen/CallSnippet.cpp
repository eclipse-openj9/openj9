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

#include "p/codegen/CallSnippet.hpp"

#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/SnippetGCMap.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/J2IThunk.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "p/codegen/PPCAOTRelocation.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "runtime/CodeCacheManager.hpp"

uint8_t *flushArgumentsToStack(uint8_t *buffer, TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg)
   {
   int32_t        intArgNum=0, floatArgNum=0, offset;
   TR::Compilation* comp = cg->comp();
   TR::InstOpCode::Mnemonic  storeGPROp= TR::Compiler->target.is64Bit() ? TR::InstOpCode::std : TR::InstOpCode::stw;
   TR::Machine *machine = cg->machine();
   const TR::PPCLinkageProperties &linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention())->getProperties();
   int32_t argStart = callNode->getFirstArgumentIndex();

   if (linkage.getRightToLeft())
      offset = linkage.getOffsetToFirstParm();
   else
      offset = argSize+linkage.getOffsetToFirstParm();

   for (int i=argStart; i<callNode->getNumChildren();i++)
      {
      TR::Node *child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (!linkage.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::stw, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Address:
            if (!linkage.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(storeGPROp, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Int64:
            if (!linkage.getRightToLeft())
               offset -= 2*TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(storeGPROp, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               if (TR::Compiler->target.is32Bit())
                  {
                  if (intArgNum < linkage.getNumIntArgRegs()-1)
                     {
                     buffer = storeArgumentItem(TR::InstOpCode::stw, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum+1)), offset+4, cg);
                     }
                  }
               }
            intArgNum += TR::Compiler->target.is64Bit() ? 1 : 2;
            if (linkage.getRightToLeft())
               offset += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Float:
            if (!linkage.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::stfs, buffer, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
            floatArgNum++;
            if (linkage.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Double:
            if (!linkage.getRightToLeft())
               offset -= 2*TR::Compiler->om.sizeofReferenceAddress();
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               buffer = storeArgumentItem(TR::InstOpCode::stfd, buffer, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
            floatArgNum++;
            if (linkage.getRightToLeft())
               offset += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         }
      }
   return(buffer);
   }

uint8_t *TR::PPCCallSnippet::setUpArgumentsInRegister(uint8_t *buffer, TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg)
   {
   int32_t        intArgNum=0, floatArgNum=0, offset;
   TR::InstOpCode::Mnemonic  loadGPROp= TR::Compiler->target.is64Bit() ? TR::InstOpCode::ld : TR::InstOpCode::lwz;
   TR::Machine *machine = cg->machine();
   TR::Compilation* comp = cg->comp();
   const TR::PPCLinkageProperties &linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention())->getProperties();
   int32_t argStart = callNode->getFirstArgumentIndex();

   if (linkage.getRightToLeft())
      offset = linkage.getOffsetToFirstParm();
   else
      offset = argSize+linkage.getOffsetToFirstParm();

   for (int i=argStart; i<callNode->getNumChildren();i++)
      {
      TR::Node *child = callNode->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (!linkage.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = loadArgumentItem(TR::InstOpCode::lwz, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Address:
            if (!linkage.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = loadArgumentItem(loadGPROp, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Int64:
            if (!linkage.getRightToLeft())
               offset -= 2*TR::Compiler->om.sizeofReferenceAddress();
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = loadArgumentItem(loadGPROp, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               if (TR::Compiler->target.is32Bit() && (intArgNum < linkage.getNumIntArgRegs()-1))
                     {
                     buffer = loadArgumentItem(TR::InstOpCode::lwz, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum+1)), offset+4, cg);
                     }
               }
            intArgNum += TR::Compiler->target.is64Bit() ? 1 : 2;
            if (linkage.getRightToLeft())
               offset += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Float:
            if (!linkage.getRightToLeft())
               offset -= TR::Compiler->om.sizeofReferenceAddress();
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               buffer = loadArgumentItem(TR::InstOpCode::lfs, buffer, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
            floatArgNum++;
            if (linkage.getRightToLeft())
               offset += TR::Compiler->om.sizeofReferenceAddress();
            break;
         case TR::Double:
            if (!linkage.getRightToLeft())
               offset -= 2*TR::Compiler->om.sizeofReferenceAddress();
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               buffer = loadArgumentItem(TR::InstOpCode::lfd, buffer, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
            floatArgNum++;
            if (linkage.getRightToLeft())
               offset += 2*TR::Compiler->om.sizeofReferenceAddress();
            break;
         }
      }
   return(buffer);
   }

int32_t TR::PPCCallSnippet::instructionCountForArguments(TR::Node *callNode, TR::CodeGenerator *cg)
   {
   int32_t        intArgNum=0, floatArgNum=0, count=0;
   const TR::PPCLinkageProperties &linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention())->getProperties();
   int32_t argStart = callNode->getFirstArgumentIndex();

   for (int i=argStart; i<callNode->getNumChildren();i++)
          {
          TR::Node *child = callNode->getChild(i);
          switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
         case TR::Address:
            if (intArgNum < linkage.getNumIntArgRegs())
               {
                           count++;
               }
                        intArgNum++;
                        break;
         case TR::Int64:
            if (intArgNum < linkage.getNumIntArgRegs())
               {
                           count++;
               if (TR::Compiler->target.is32Bit() && (intArgNum < linkage.getNumIntArgRegs()-1))
                     {
                                  count++;
                     }
               }
            intArgNum += TR::Compiler->target.is64Bit() ? 1 : 2;
                        break;
         case TR::Float:
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
                           count++;
               }
            floatArgNum++;
                        break;
         case TR::Double:
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
                           count++;
               }
            floatArgNum++;
                        break;
                 }
          }
   return(count);
   }

TR_RuntimeHelper TR::PPCCallSnippet::getInterpretedDispatchHelper(
   TR::SymbolReference *methodSymRef,
   TR::DataType     type,
   bool               isSynchronised,
   bool&              isNativeStatic,
   TR::CodeGenerator   *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   bool isJitInduceOSRCall = false;
   if (methodSymbol->isHelper() &&
       methodSymRef->isOSRInductionHelper())
      {
      isJitInduceOSRCall = true;
      }

   bool forceUnresolvedDispatch = fej9->forceUnresolvedDispatch();
   if (comp->getOption(TR_UseSymbolValidationManager))
      forceUnresolvedDispatch = false;

   if (methodSymRef->isUnresolved() || forceUnresolvedDispatch)
      {
      TR_ASSERT(!isJitInduceOSRCall || !forceUnresolvedDispatch, "calling jitInduceOSR is not supported yet under AOT\n");

      if (methodSymbol->isSpecial())
         return TR_PPCinterpreterUnresolvedSpecialGlue;
      else if (methodSymbol->isStatic())
         return TR_PPCinterpreterUnresolvedStaticGlue;
      else
         return TR_PPCinterpreterUnresolvedDirectVirtualGlue;
      }
   else if (methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative())
      {
      isNativeStatic = true;
      return TR_PPCnativeStaticHelper;
      }
   else if (isJitInduceOSRCall)
      return (TR_RuntimeHelper) methodSymRef->getReferenceNumber();
   else
      {
      switch (type)
         {
         case TR::NoType:
            return isSynchronised?TR_PPCinterpreterSyncVoidStaticGlue: TR_PPCinterpreterVoidStaticGlue;
         case TR::Int32:
            return isSynchronised?TR_PPCinterpreterSyncGPR3StaticGlue:TR_PPCinterpreterGPR3StaticGlue;
         case TR::Address:
            if (TR::Compiler->target.is64Bit())
               return isSynchronised?TR_PPCinterpreterSyncGPR3GPR4StaticGlue:TR_PPCinterpreterGPR3GPR4StaticGlue;
            else
               return isSynchronised?TR_PPCinterpreterSyncGPR3StaticGlue:TR_PPCinterpreterGPR3StaticGlue;
         case TR::Int64:
            return isSynchronised?TR_PPCinterpreterSyncGPR3GPR4StaticGlue:TR_PPCinterpreterGPR3GPR4StaticGlue;
         case TR::Float:
            return isSynchronised?TR_PPCinterpreterSyncFPR0FStaticGlue:TR_PPCinterpreterFPR0FStaticGlue;
         case TR::Double:
            return isSynchronised?TR_PPCinterpreterSyncFPR0DStaticGlue:TR_PPCinterpreterFPR0DStaticGlue;
         default:
            TR_ASSERT(0, "Bad return data type for a call node.  DataType was %s\n",
                    comp->getDebug()->getName(type));
            return (TR_RuntimeHelper)0;
         }
      }
   }

uint8_t *TR::PPCCallSnippet::emitSnippetBody()
   {

   uint8_t       *cursor = cg()->getBinaryBufferCursor();
   TR::Node       *callNode = getNode();
   TR::SymbolReference *methodSymRef = (_realMethodSymbolReference)?_realMethodSymbolReference:callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::SymbolReference *glueRef;
   bool isNativeStatic = false;
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   getSnippetLabel()->setCodeLocation(cursor);

   // Flush in-register arguments back to the stack for interpreter
   cursor = flushArgumentsToStack(cursor, callNode, getSizeOfArguments(), cg());

   TR_RuntimeHelper runtimeHelper = getInterpretedDispatchHelper(methodSymRef, callNode->getDataType(),
                                                                 methodSymbol->isSynchronised(), isNativeStatic, cg());
   glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(runtimeHelper, false, false, false);

   intptrj_t helperAddress = (intptrj_t)glueRef->getMethodAddress();
   if (cg()->directCallRequiresTrampoline(helperAddress, (intptrj_t)cursor))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(glueRef->getReferenceNumber(), (void *)cursor);
      TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange(helperAddress, (intptrj_t)cursor),
                      "Helper address is out of range");
      }

   // 'b glueRef' for jitInduceOSRAtCurrentPC, 'bl glueRef' otherwise
   // we use "b" for induceOSR because we want the helper to think that it's been called from the mainline code and not from the snippet.
   int32_t branchInstruction = (glueRef->isOSRInductionHelper()) ? 0x48000000 : 0x48000001;
   *(int32_t *)cursor = branchInstruction | ((helperAddress - (intptrj_t)cursor) & 0x03fffffc);
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,(uint8_t *)glueRef,TR_HelperAddress, cg()),
      __FILE__, __LINE__, callNode);

   cursor += PPC_INSTRUCTION_LENGTH;

   if (isNativeStatic)
      {
      // Rather than placing the return address as data after the 'bl', place a 'b' back to main line code
      // This insures that all 'blr's return to their corresponding 'bl's
      *(int32_t *)cursor = 0x48000000 | ((intptrj_t)(getCallRA() - (intptrj_t)cursor) & 0x03fffffc);
      TR_ASSERT(gcMap().isGCSafePoint() && gcMap().getStackMap(), "Native static call snippets must have GC maps when preserving the link stack");
      gcMap().registerStackMap(cursor - PPC_INSTRUCTION_LENGTH, cg());
      cursor += PPC_INSTRUCTION_LENGTH;

      // Padding; VM helper depends this gap being present
      if (TR::Compiler->target.is64Bit())
         {
         *(int32_t *)cursor = 0xdeadc0de;
         cursor += PPC_INSTRUCTION_LENGTH;
         }
      }
   else
      {
      // Store the code cache RA
      *(intptrj_t *)cursor = (intptrj_t)getCallRA();
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,NULL,TR_AbsoluteMethodAddress, cg()),
            __FILE__, __LINE__, callNode);

      cursor += TR::Compiler->om.sizeofReferenceAddress();
      }
   //induceOSRAtCurrentPC is implemented in the VM, and it knows, by looking at the current PC, what method it needs to
   //continue execution in interpreted mode. Therefore, it doesn't need the method pointer.
   if (!glueRef->isOSRInductionHelper())
      {
      bool forceUnresolvedDispatch = fej9->forceUnresolvedDispatch();
      if (comp->getOption(TR_UseSymbolValidationManager))
         forceUnresolvedDispatch = false;

      // Store the method pointer: it is NULL for unresolved
      if (methodSymRef->isUnresolved() || forceUnresolvedDispatch)
         {
         *(intptrj_t *)cursor = 0;
         if (comp->getOption(TR_EnableHCR))
            {
            cg()->jitAddPicToPatchOnClassRedefinition((void*)-1, (void *)cursor, true);
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation((uint8_t *)cursor, NULL,(uint8_t *)needsFullSizeRuntimeAssumption,
                                                                                         TR_HCR, cg()),__FILE__, __LINE__,
                                   getNode());
            }
         }
      else
         {
         *(intptrj_t *)cursor = (intptrj_t)methodSymbol->getMethodAddress();
         if (comp->getOption(TR_EnableHCR))
            cg()->jitAddPicToPatchOnClassRedefinition((void *)methodSymbol->getMethodAddress(), (void *)cursor);

         if (comp->compileRelocatableCode())
            {
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                  (uint8_t *)methodSymbol->getMethodAddress(),
                                                                  (uint8_t *)TR::SymbolType::typeMethod,
                                                                  TR_SymbolFromManager,
                                                                  cg()),  __FILE__, __LINE__, getNode());
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                  (uint8_t *)methodSymbol->getMethodAddress(),
                                                                  TR_ResolvedTrampolines,
                                                                  cg()), __FILE__, __LINE__, getNode());
            }
         }
      }
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // Lock word initialized to 0
   *(int32_t *)cursor = 0;

   return (cursor+4);
   }

uint32_t TR::PPCCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   TR::Compilation* comp = cg()->comp();
   return((instructionCountForArguments(getNode(), cg())*4) + 2*TR::Compiler->om.sizeofReferenceAddress() + 8);
   }

uint8_t *TR::PPCUnresolvedCallSnippet::emitSnippetBody()
   {
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   uint8_t *cursor = TR::PPCCallSnippet::emitSnippetBody();

   TR::SymbolReference *methodSymRef = (_realMethodSymbolReference)?_realMethodSymbolReference:getNode()->getSymbolReference();
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   int32_t helperLookupOffset;

   switch (getNode()->getDataType())
      {
      case TR::NoType:
         helperLookupOffset = 0;
         break;
      case TR::Int32:
         helperLookupOffset = TR::Compiler->om.sizeofReferenceAddress();
         break;
      case TR::Address:
         if (TR::Compiler->target.is64Bit())
            helperLookupOffset = 2*TR::Compiler->om.sizeofReferenceAddress();
      else
         helperLookupOffset = TR::Compiler->om.sizeofReferenceAddress();
         break;
      case TR::Int64:
         helperLookupOffset = 2*TR::Compiler->om.sizeofReferenceAddress();
         break;
      case TR::Float:
         helperLookupOffset = 3*TR::Compiler->om.sizeofReferenceAddress();
         break;
      case TR::Double:
         helperLookupOffset = 4*TR::Compiler->om.sizeofReferenceAddress();
         break;
      }

   *(uint32_t *)cursor = (helperLookupOffset<<24) | methodSymRef->getCPIndexForVM();

   cursor += 4;
   *(intptrj_t *)cursor = (intptrj_t)methodSymRef->getOwningMethod(comp)->constantPool();

   if (cg()->comp()->compileRelocatableCode() && comp->getOption(TR_TraceRelocatableDataDetailsCG))
      {
      traceMsg(comp, "<relocatableDataTrampolinesCG>\n");
      traceMsg(comp, "%s\n", comp->signature());
      traceMsg(comp, "%-8s", "cpIndex");
      traceMsg(comp, "cp\n");
      traceMsg(comp, "%-8x", methodSymRef->getCPIndexForVM());
      traceMsg(comp, "%x\n", methodSymRef->getOwningMethod(comp)->constantPool());
      traceMsg(comp, "</relocatableDataTrampolinesCG>\n");
      }

   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,
         *(uint8_t **)cursor,
         getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
         TR_Trampolines, cg()),
         __FILE__, __LINE__, getNode());

   cursor += TR::Compiler->om.sizeofReferenceAddress();

   *(int32_t *)cursor = 0;
   return cursor+4;
   }

uint32_t TR::PPCUnresolvedCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   TR::Compilation* comp = cg()->comp();
   return TR::PPCCallSnippet::getLength(estimatedSnippetStart) + 8 + TR::Compiler->om.sizeofReferenceAddress();
   }

uint8_t *TR::PPCVirtualSnippet::emitSnippetBody()
   {
   return(NULL);
   }

uint32_t TR::PPCVirtualSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return(0);
   }

uint8_t *TR::PPCVirtualUnresolvedSnippet::emitSnippetBody()
   {
   uint8_t       *cursor = cg()->getBinaryBufferCursor();
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::Node       *callNode = getNode();
   TR::SymbolReference *glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCvirtualUnresolvedHelper, false, false, false);
   void *thunk = fej9->getJ2IThunk(callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethod(), comp);
   uint8_t *j2iThunkRelocationPoint;

   // We want the data in the snippet to be naturally aligned
   if (TR::Compiler->target.is64Bit() && (((uint64_t)cursor % TR::Compiler->om.sizeofReferenceAddress()) == 4))
      {
      *(int32_t *)cursor = 0xdeadc0de;
      cursor += 4;
      }

   getSnippetLabel()->setCodeLocation(cursor);

   intptrj_t helperAddress = (intptrj_t)glueRef->getMethodAddress();
   if (cg()->directCallRequiresTrampoline(helperAddress, (intptrj_t)cursor))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(glueRef->getReferenceNumber(), (void *)cursor);
      TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange(helperAddress, (intptrj_t)cursor),
                      "Helper address is out of range");
      }

   // bl glueRef
   *(int32_t *)cursor = 0x48000001 | ((helperAddress - (intptrj_t)cursor) & 0x03fffffc);
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,(uint8_t *)glueRef,TR_HelperAddress, cg()),
      __FILE__, __LINE__, callNode);
   cursor += 4;

   /*
    * Place a 'b' back to the main line code after the 'bl'.
    * This is used to have 'blr's return to their corresponding 'bl's when handling private nestmate calls.
    * Ideally, 'blr's return to their corresponding 'bl's in other cases as well but currently that does not happen.
    */
   intptrj_t distance = (intptrj_t)getReturnLabel()->getCodeLocation() - (intptrj_t)cursor;
   *(int32_t *)cursor = 0x48000000 | (distance & 0x03fffffc);

   TR_ASSERT(gcMap().isGCSafePoint() && gcMap().getStackMap(), "Virtual call snippets must have GC maps when preserving the link stack");
   gcMap().registerStackMap(cursor - 4, cg());
   cursor += 4;

   // Store the code cache RA
   *(intptrj_t *)cursor = (intptrj_t)getReturnLabel()->getCodeLocation();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor,NULL,TR_AbsoluteMethodAddress, cg()),
         __FILE__, __LINE__, callNode);

   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // GJ - Swizzled the order of the following lines to conform to helper
   intptrj_t cpAddr = (intptrj_t)callNode->getSymbolReference()->getOwningMethod(comp)->constantPool();
   *(intptrj_t *)cursor = cpAddr;
   j2iThunkRelocationPoint = cursor;

   cursor += TR::Compiler->om.sizeofReferenceAddress();

   *(uintptrj_t *)cursor = callNode->getSymbolReference()->getCPIndexForVM();
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   /*
    * Reserved spot to hold J9Method pointer of the callee.
    * This is used for private nestmate calls.
    */
   *(intptrj_t *)cursor = (intptrj_t)0;
   cursor += sizeof(intptrj_t);

   /*
    * J2I thunk address.
    * This is used for private nestmate calls.
    */
   *(intptrj_t*)cursor = (intptrj_t)thunk;

   auto info =
      (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(
         sizeof (TR_RelocationRecordInformation),
         heapAlloc);

   // data1 = constantPool
   info->data1 = cpAddr;

   // data2 = inlined site index
   info->data2 = callNode ? callNode->getInlinedSiteIndex() : (uintptr_t)-1;

   // data3 = distance in bytes from Constant Pool Pointer to J2I Thunk
   info->data3 = (intptrj_t)cursor - (intptrj_t)j2iThunkRelocationPoint;

   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(j2iThunkRelocationPoint, (uint8_t *)info, NULL, TR_J2IVirtualThunkPointer, cg()),
                               __FILE__, __LINE__, callNode);

   cursor += sizeof(intptrj_t);

   *(int32_t *)cursor = 0;        // Lock word
   cursor += sizeof(int32_t);

   return cursor;
   }

uint32_t TR::PPCVirtualUnresolvedSnippet::getLength(int32_t estimatedSnippetStart)
   {
   /*
    * 4 = Code alignment may add 4 to the length. To be conservative it is always part of the estimate.
    * 8 = Two instructions. One bl and one b instruction.
    * 5 address fields:
    *   - Call Site RA
    *   - Constant Pool Pointer
    *   - Constant Pool Index
    *   - Private J9Method pointer
    *   - J2I thunk address
    * 4 = Lockword
    */
   TR::Compilation* comp = cg()->comp();
   return(4 + 8 + (5 * TR::Compiler->om.sizeofReferenceAddress()) + 4);
   }

uint8_t *TR::PPCInterfaceCallSnippet::emitSnippetBody()
   {
   uint8_t       *cursor = cg()->getBinaryBufferCursor();
   uint8_t       *blAddress;
   TR::Node       *callNode = getNode();
   TR::Compilation *comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   TR::SymbolReference *glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_PPCinterfaceCallHelper, false, false, false);
   void *thunk = fej9->getJ2IThunk(callNode->getSymbolReference()->getSymbol()->castToMethodSymbol()->getMethod(), comp);
   uint8_t *j2iThunkRelocationPoint;

   // We want the data in the snippet to be naturally aligned
   if (TR::Compiler->target.is64Bit() && (((uint64_t)cursor % TR::Compiler->om.sizeofReferenceAddress()) == 0))
      {
      // icallVMprJavaSendPatchupVirtual needs to determine if it was called for virtual dispatch as opposed to interface dispatch
      // To do that it checks for 'mtctr r12' at -8(LR), which points here when it's called for interface dispatch in 64 bit mode
      // We make sure it doesn't contain that particular bit pattern by accident (which can actually happen)
      *(int32_t *)cursor = 0xdeadc0de;
      cursor += 4;
      }

   getSnippetLabel()->setCodeLocation(cursor);

   intptrj_t helperAddress = (intptrj_t)glueRef->getMethodAddress();
   if (cg()->directCallRequiresTrampoline(helperAddress, (intptrj_t)cursor))
      {
      helperAddress = TR::CodeCacheManager::instance()->findHelperTrampoline(glueRef->getReferenceNumber(), (void *)cursor);
      TR_ASSERT_FATAL(TR::Compiler->target.cpu.isTargetWithinIFormBranchRange(helperAddress, (intptrj_t)cursor),
                      "Helper address is out of range");
      }

   // bl glueRef
   *(int32_t *)cursor = 0x48000001 | ((helperAddress - (intptrj_t)cursor) & 0x03fffffc);
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)glueRef, TR_HelperAddress, cg()),
                             __FILE__, __LINE__, callNode);
   blAddress = cursor;
   cursor += PPC_INSTRUCTION_LENGTH;

   // Rather than placing the return address as data after the 'bl', place a 'b' back to main line code
   // This insures that all 'blr's return to their corresponding 'bl's
   intptrj_t distance = (intptrj_t)getReturnLabel()->getCodeLocation() - (intptrj_t)cursor;
   *(int32_t *)cursor = 0x48000000 | (distance & 0x03fffffc);

   TR_ASSERT(gcMap().isGCSafePoint() && gcMap().getStackMap(), "Interface call snippets must have GC maps when preserving the link stack");
   gcMap().registerStackMap(cursor - PPC_INSTRUCTION_LENGTH, cg());
   cursor += PPC_INSTRUCTION_LENGTH;

   // Padding; jitLookupInterfaceMethod depends on this gap being present
   if (TR::Compiler->target.is64Bit())
      {
      *(int32_t *)cursor = 0xdeadc0de;
      cursor += PPC_INSTRUCTION_LENGTH;
      }

   intptrj_t cpAddr = (intptrj_t)callNode->getSymbolReference()->getOwningMethod(comp)->constantPool();
   *(intptrj_t *)cursor = cpAddr;
   j2iThunkRelocationPoint = cursor;

   cursor += TR::Compiler->om.sizeofReferenceAddress();

   *(uintptrj_t *)cursor = callNode->getSymbolReference()->getCPIndexForVM();
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // Add two slots for interface class & iTable index, keeping cp/cpindex around
   ((uintptrj_t *)cursor)[0] = 0;
   ((uintptrj_t *)cursor)[1] = 0;
   cursor += 2*TR::Compiler->om.sizeofReferenceAddress();

   if (TR::Compiler->target.is64Bit())
      {
      if (getTOCOffset() != PTOC_FULL_INDEX)
         {
         TR_PPCTableOfConstants::setTOCSlot(getTOCOffset(), (uintptrj_t)cursor);
         }
      else
         {
         int32_t  *patchAddr = (int32_t *)getLowerInstruction()->getBinaryEncoding();
         intptrj_t addrValue = (intptrj_t)cursor;
         if (!comp->compileRelocatableCode())
            {
            // If the high nibble is 0 and the next nibble's high bit is clear, change the first instruction to a nop and the third to a li
            // Next nibble's high bit needs to be clear in order to use li (because li will sign extend the immediate)
            if ((addrValue >> 48) == 0 && ((addrValue >> 32) & 0x8000) == 0)
               {
               *patchAddr |= addrValue & 0x0000ffff;
               addrValue = cg()->hiValue(addrValue);
               uint32_t ori = *(patchAddr-2);
               uint32_t li = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::li) | (ori & 0x03e00000);
               *(patchAddr-2) = li | ((addrValue>>16) & 0x0000ffff);
               *(patchAddr-3) |= addrValue & 0x0000ffff;
               *(patchAddr-4) = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::nop);
               }
            else
               {
               *patchAddr |= addrValue & 0x0000ffff;
               addrValue = cg()->hiValue(addrValue);
               *(patchAddr-2) |= (addrValue>>16) & 0x0000ffff;
               *(patchAddr-3) |= addrValue & 0x0000ffff;
               *(patchAddr-4) |= (addrValue>>32) & 0x0000ffff;
               }
            }
         else
            {
            cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(getUpperInstruction(),
               (uint8_t *)(addrValue),
               (uint8_t *)fixedSequence4,
               TR_FixedSequenceAddress2,
               cg()),
               __FILE__, __LINE__, callNode);
            }
         }
      }
   else
      {
      // Patch up the main line codes
      int32_t *patchAddress1 = (int32_t *)getUpperInstruction()->getBinaryEncoding();
      *patchAddress1 |= cg()->hiValue((int32_t)(intptrj_t)cursor) & 0x0000ffff;
      int32_t *patchAddress2 = (int32_t *)getLowerInstruction()->getBinaryEncoding();
      *patchAddress2 |= (int32_t)(intptrj_t)cursor & 0x0000ffff;
      TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data3 = orderedPairSequence1;
      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation((uint8_t *)patchAddress1,
         (uint8_t *)patchAddress2,
         (uint8_t *)recordInfo,
         TR_AbsoluteMethodAddressOrderedPair, cg()),
         __FILE__, __LINE__, callNode);
      }

   // Initialize for: two class ptrs, two target addrs
   // Initialize target addrs with the address of the bl. see 134322
   *(intptrj_t *)cursor = -1;
   *(intptrj_t *)(cursor+TR::Compiler->om.sizeofReferenceAddress()) = (intptrj_t)blAddress;
   *(intptrj_t *)(cursor+2*TR::Compiler->om.sizeofReferenceAddress()) = -1;
   *(intptrj_t *)(cursor+3*TR::Compiler->om.sizeofReferenceAddress()) = (intptrj_t)blAddress;

   // Register for relation of the 1st target address
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor+TR::Compiler->om.sizeofReferenceAddress(), NULL, TR_AbsoluteMethodAddress, cg()),
         __FILE__, __LINE__, callNode);

   // Register for relocation of the 2nd target address
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor+3*TR::Compiler->om.sizeofReferenceAddress(), NULL, TR_AbsoluteMethodAddress, cg()),
         __FILE__, __LINE__, callNode);

   cursor += 4 * TR::Compiler->om.sizeofReferenceAddress();

   /*
    * J2I thunk address.
    * This is used for private nestmate calls.
    */
   *(intptrj_t*)cursor = (intptrj_t)thunk;

   if (cg()->comp()->compileRelocatableCode())
      {
      auto info =
         (TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(
            sizeof (TR_RelocationRecordInformation),
            heapAlloc);

      // data1 = constantPool
      info->data1 = cpAddr;

      // data2 = inlined site index
      info->data2 = callNode ? callNode->getInlinedSiteIndex() : (uintptr_t)-1;

      // data3 = distance in bytes from Constant Pool Pointer to J2I Thunk
      info->data3 = (intptrj_t)cursor - (intptrj_t)j2iThunkRelocationPoint;

      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(j2iThunkRelocationPoint, (uint8_t *)info, NULL, TR_J2IVirtualThunkPointer, cg()),
                               __FILE__, __LINE__, callNode);
      }
   cursor += sizeof(intptrj_t);


   return cursor;
   }

uint32_t TR::PPCInterfaceCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   /*
    * 4 = Code alignment may add 4 to the length. To be conservative it is always part of the estimate.
    * 8 = Two instructions. One bl and one b instruction.
    * 0 or 4 = Padding. Only needed under 64 bit.
    * 9 address fields:
    *   - CP Pointer
    *   - CP Index
    *   - Interface Class Pointer
    *   - ITable Index (may also contain a tagged J9Method* when handling nestmates)
    *   - First Class Pointer
    *   - First Class Target
    *   - Second Class Pointer
    *   - Second Class Target
    *   - J2I thunk address
    */
   TR::Compilation* comp = cg()->comp();
   return(4 + 8 + (TR::Compiler->target.is64Bit() ? 4 : 0) + (9 * TR::Compiler->om.sizeofReferenceAddress()));
   }

uint8_t *TR::PPCCallSnippet::generateVIThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg)
   {
   TR::Compilation * comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());
   int32_t  codeSize = 4*(instructionCountForArguments(callNode, cg) + (TR::Compiler->target.is64Bit()?7:4)) + 8; // Additional 4 bytes to hold size of thunk
   uint8_t *thunk, *buffer, *returnValue;
   intptrj_t  dispatcher;
   int32_t sizeThunk;

   switch (callNode->getDataType())
          {
          case TR::NoType:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtual0, false, false, false)->getMethodAddress();
                 break;
          case TR::Int32:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtual1, false, false, false)->getMethodAddress();
                 break;
          case TR::Address:
                 if (TR::Compiler->target.is64Bit())
                    dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtualJ, false, false, false)->getMethodAddress();
                 else
                    dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtual1, false, false, false)->getMethodAddress();
                 break;
          case TR::Int64:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtualJ, false, false, false)->getMethodAddress();
                 break;
          case TR::Float:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtualF, false, false, false)->getMethodAddress();
                 break;
          case TR::Double:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_PPCicallVMprJavaSendVirtualD, false, false, false)->getMethodAddress();
                 break;
          default:
                 TR_ASSERT(0, "Bad return data type for a call node.  DataType was %s\n",
                                  comp->getDebug()->getName(callNode->getDataType()));
          }

   if ( TR::Compiler->target.is32Bit() && (((dispatcher&0x80008000) == 0x80008000) || cg->comp()->compileRelocatableCode()) )
      codeSize += 4;

   if (cg->comp()->compileRelocatableCode())
      thunk = (uint8_t *)comp->trMemory()->allocateMemory(codeSize, heapAlloc);
   else
      thunk = (uint8_t *)cg->allocateCodeMemory(codeSize, true, false);
   buffer = returnValue = thunk + 8;

   buffer = flushArgumentsToStack(buffer, callNode, argSize, cg);
   *((int32_t *)thunk + 1)= buffer - returnValue; // patch offset for AOT relocation

   // NOTE: modification of the layout of the following will require a corresponding change in AOT relocation code (codert/ppc/AOTRelocations.cpp)
   if (TR::Compiler->target.is64Bit())
      {
      // todo64: fix me, I'm just a temporary kludge
      // lis gr4, upper 16-bits
      *(int32_t *)buffer = 0x3c800000 | ((dispatcher>>48) & 0x0000ffff);
      buffer += 4;

      // ori gr4, gr4, next 16-bits
      *(int32_t *)buffer = 0x60840000 | ((dispatcher>>32) & 0x0000ffff);
      buffer += 4;

      // rldicr gr4, gr4, 32, 31
      *(int32_t *)buffer = 0x788403e6;
      buffer += 4;

      // oris gr4, gr4, next 16-bits
      *(int32_t *)buffer = 0x64840000 | ((dispatcher>>16) & 0x0000ffff);
      buffer += 4;

      // ori gr4, gr4, last 16-bits
      *(int32_t *)buffer = 0x60840000 | (dispatcher & 0x0000ffff);
      buffer += 4;
      }
   else
      {
      // For POWER4 which has a problem with the CTR/LR cache when the upper
      // bits are not 0 extended.. Use li/oris when the 16th bit is off
      if( !(dispatcher & 0x00008000) )
         {
         // li r4, lower
         *(int32_t *)buffer = 0x38800000 | (dispatcher & 0x0000ffff);
         buffer += 4;
         // oris r4, r4, upper
         *(int32_t *)buffer = 0x64840000 | ((dispatcher>>16) & 0x0000ffff);
         buffer += 4;
         }
      else
         {
         // lis gr4, upper
         *(int32_t *)buffer = 0x3c800000 |
                                 (((dispatcher>>16) + (dispatcher&(1<<15)?1:0)) & 0x0000ffff);
         buffer += 4;

         // addi gr4, gr4, lower
         *(int32_t *)buffer = 0x38840000 | (dispatcher & 0x0000ffff);
         buffer += 4;
         // Now, if highest bit is on we need to clear the sign extend bits on 64bit CPUs
         // ** POWER4 pref fix **
         if( dispatcher & 0x80000000 )
            {
            // rlwinm r4,r4,sh=0,mb=0,me=31
            *(int32_t *)buffer = 0x5484003e;
            buffer += 4;
            }
         }
      }

   // mtctr gr4
   *(int32_t *)buffer = 0x7c8903a6;
   buffer += 4;

   // bcctr
   *(int32_t *)buffer = 0x4e800420;
   buffer += 4;

   sizeThunk = buffer - returnValue;
   if (TR::Compiler->target.is32Bit() && comp->compileRelocatableCode() && !((dispatcher&0x80008000) == 0x80008000)) // Make size of thunk larger for AOT even if extra instruction is not generated at compile time.  The extra instruction could be needed for the runtime address.
      sizeThunk += 4;

   // patch size of thunk
   *(int32_t *)thunk = sizeThunk;

   ppcCodeSync(thunk, codeSize);

   return(returnValue);
   }

TR_J2IThunk *TR::PPCCallSnippet::generateInvokeExactJ2IThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg, char *signature)
   {
   int32_t  codeSize = 4*(instructionCountForArguments(callNode, cg) + (TR::Compiler->target.is64Bit()?7:4)) + 8; // Additional 4 bytes to hold size of thunk
   intptrj_t  dispatcher;
   int32_t sizeThunk;
   TR::Compilation *comp = cg->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp->fe());

   TR_J2IThunkTable *thunkTable = comp->getPersistentInfo()->getInvokeExactJ2IThunkTable();
   TR_J2IThunk      *thunk      = TR_J2IThunk::allocate(codeSize, signature, cg, thunkTable);
   uint8_t          *buffer     = thunk->entryPoint();

   switch (callNode->getDataType())
          {
          case TR::NoType:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact0, false, false, false)->getMethodAddress();
                 break;
          case TR::Int32:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact1, false, false, false)->getMethodAddress();
                 break;
          case TR::Address:
                 if (TR::Compiler->target.is64Bit())
                    dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactJ, false, false, false)->getMethodAddress();
                 else
                    dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExact1, false, false, false)->getMethodAddress();
                 break;
          case TR::Int64:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactJ, false, false, false)->getMethodAddress();
                 break;
          case TR::Float:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactF, false, false, false)->getMethodAddress();
                 break;
          case TR::Double:
                 dispatcher = (intptrj_t)cg->symRefTab()->findOrCreateRuntimeHelper(TR_icallVMprJavaSendInvokeExactD, false, false, false)->getMethodAddress();
                 break;
          default:
                 TR_ASSERT(0, "Bad return data type '%s' for call node [" POINTER_PRINTF_FORMAT "]\n",
                                  comp->getDebug()->getName(callNode->getDataType()),
                                  callNode);
          }

   if ( TR::Compiler->target.is32Bit() && (((dispatcher&0x80008000) == 0x80008000) || comp->compileRelocatableCode()) )
      codeSize += 4;

   buffer = flushArgumentsToStack(buffer, callNode, argSize, cg);

   // NOTE: modification of the layout of the following will require a corresponding change in AOT relocation code (codert/ppc/AOTRelocations.cpp)
   if (TR::Compiler->target.is64Bit())
      {
      // todo64: fix me, I'm just a temporary kludge
      // lis gr4, upper 16-bits
      *(int32_t *)buffer = 0x3c800000 | ((dispatcher>>48) & 0x0000ffff);
      buffer += 4;

      // ori gr4, gr4, next 16-bits
      *(int32_t *)buffer = 0x60840000 | ((dispatcher>>32) & 0x0000ffff);
      buffer += 4;

      // rldicr gr4, gr4, 32, 31
      *(int32_t *)buffer = 0x788403e6;
      buffer += 4;

      // oris gr4, gr4, next 16-bits
      *(int32_t *)buffer = 0x64840000 | ((dispatcher>>16) & 0x0000ffff);
      buffer += 4;

      // ori gr4, gr4, last 16-bits
      *(int32_t *)buffer = 0x60840000 | (dispatcher & 0x0000ffff);
      buffer += 4;
      }
   else
      {
      // For POWER4 which has a problem with the CTR/LR cache when the upper
      // bits are not 0 extended.. Use li/oris when the 16th bit is off
      if( !(dispatcher & 0x00008000) )
         {
         // li r4, lower
         *(int32_t *)buffer = 0x38800000 | (dispatcher & 0x0000ffff);
         buffer += 4;
         // oris r4, r4, upper
         *(int32_t *)buffer = 0x64840000 | ((dispatcher>>16) & 0x0000ffff);
         buffer += 4;
         }
      else
         {
         // lis gr4, upper
         *(int32_t *)buffer = 0x3c800000 |
                                 (((dispatcher>>16) + (dispatcher&(1<<15)?1:0)) & 0x0000ffff);
         buffer += 4;

         // addi gr4, gr4, lower
         *(int32_t *)buffer = 0x38840000 | (dispatcher & 0x0000ffff);
         buffer += 4;
         // Now, if highest bit is on we need to clear the sign extend bits on 64bit CPUs
         // ** POWER4 pref fix **
         if( dispatcher & 0x80000000 )
            {
            // rlwinm r4,r4,sh=0,mb=0,me=31
            *(int32_t *)buffer = 0x5484003e;
            buffer += 4;
            }
         }
      }

   // mtctr gr4
   *(int32_t *)buffer = 0x7c8903a6;
   buffer += 4;

   // bcctr
   *(int32_t *)buffer = 0x4e800420;
   buffer += 4;

   ppcCodeSync(thunk->entryPoint(), codeSize);

   return(thunk);
   }


uint8_t *
TR_Debug::printPPCArgumentsFlush(TR::FILE *pOutFile, TR::Node *node, uint8_t *cursor, int32_t argSize)
   {
   char *storeGPROpName;
   int32_t offset = 0,
           intArgNum = 0,
           floatArgNum = 0;

   if (TR::Compiler->target.is64Bit())
     {
     storeGPROpName="std";
     }
   else
     {
     storeGPROpName="stw";
     }

   TR::MethodSymbol *methodSymbol = node->getSymbol()->castToMethodSymbol();
   const TR::PPCLinkageProperties &linkage = _cg->getLinkage(methodSymbol->getLinkageConvention())->getProperties();

   TR::Machine *machine = _cg->machine();
   TR::RealRegister *stackPtr = _cg->getStackPointerRegister();

   if (linkage.getRightToLeft())
     offset = linkage.getOffsetToFirstParm();
   else
     offset = argSize + linkage.getOffsetToFirstParm();

   for (int i = node->getFirstArgumentIndex(); i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      switch (child->getDataType())
         {
         case TR::Int8:
         case TR::Int16:
         case TR::Int32:
            if (!linkage.getRightToLeft())
               offset -= sizeof(intptrj_t);
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               printPrefix(pOutFile, NULL, cursor, 4);
               trfprintf(pOutFile, "stw [");
               print(pOutFile, stackPtr, TR_WordReg);
               trfprintf(pOutFile, ", %d], ", offset);
               print(pOutFile, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), TR_WordReg);
               cursor += 4;
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += sizeof(intptrj_t);
            break;
         case TR::Address:
            if (!linkage.getRightToLeft())
               offset -= sizeof(intptrj_t);
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               printPrefix(pOutFile, NULL, cursor, 4);
               trfprintf(pOutFile, "%s [", storeGPROpName);
               print(pOutFile, stackPtr, TR_WordReg);
               trfprintf(pOutFile, ", %d], ", offset);
               print(pOutFile, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), TR_WordReg);
               cursor += 4;
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += sizeof(intptrj_t);
            break;
         case TR::Int64:
            if (!linkage.getRightToLeft())
               offset -= 2*sizeof(intptrj_t);
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               printPrefix(pOutFile, NULL, cursor, 4);
               trfprintf(pOutFile, "%s [", storeGPROpName);
               print(pOutFile, stackPtr, TR_WordReg);
               trfprintf(pOutFile, ", %d], ", offset);
               print(pOutFile, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), TR_WordReg);
               cursor += 4;
               if (TR::Compiler->target.is32Bit() && (intArgNum < linkage.getNumIntArgRegs() - 1))
                  {
                  printPrefix(pOutFile, NULL, cursor, 4);
                  trfprintf(pOutFile, "stw [");
                  print(pOutFile, stackPtr, TR_WordReg);
                  trfprintf(pOutFile, ", %d], ", offset + 4);
                  print(pOutFile, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum + 1)), TR_WordReg);
                  cursor += 4;
                  }
               }
            if (TR::Compiler->target.is64Bit())
               intArgNum += 1;
            else
               intArgNum += 2;
            if (linkage.getRightToLeft())
               offset += 2*sizeof(intptrj_t);
            break;
         case TR::Float:
            if (!linkage.getRightToLeft())
               offset -= sizeof(intptrj_t);
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               printPrefix(pOutFile, NULL, cursor, 4);
               trfprintf(pOutFile, "stfs [");
               print(pOutFile, stackPtr, TR_WordReg);
               trfprintf(pOutFile, ", %d], ", offset);
               print(pOutFile, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), TR_WordReg);
               cursor += 4;
               }
            floatArgNum++;
            if (linkage.getRightToLeft())
               offset += sizeof(intptrj_t);
            break;
         case TR::Double:
            if (!linkage.getRightToLeft())
               offset -= 2*sizeof(intptrj_t);
            if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               printPrefix(pOutFile, NULL, cursor, 4);
               trfprintf(pOutFile, "stfd [");
               print(pOutFile, stackPtr, TR_WordReg);
               trfprintf(pOutFile, ", %d], ", offset);
               print(pOutFile, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), TR_WordReg);
               cursor += 4;
               }
            floatArgNum++;
            if (linkage.getRightToLeft())
               offset += 2*sizeof(intptrj_t);
            break;
         }
      }
   return(cursor);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCCallSnippet * snippet)
   {
   TR::Compilation     *comp = TR::comp();
   TR_J9VMBase         *fej9 = (TR_J9VMBase *)(comp->fe());
   uint8_t             *cursor = snippet->getSnippetLabel()->getCodeLocation();
   TR::Node            *callNode = snippet->getNode();
   TR::SymbolReference *methodSymRef = snippet->getRealMethodSymbolReference() ? snippet->getRealMethodSymbolReference() :
                                                                                 callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::SymbolReference *glueRef = NULL;
   const char          *labelString = NULL;
   bool                 isNativeStatic = false;

   bool forceUnresolvedDispatch = fej9->forceUnresolvedDispatch();
   if (comp->getOption(TR_UseSymbolValidationManager))
      forceUnresolvedDispatch = false;

   if (methodSymbol->isHelper() &&
       methodSymRef->isOSRInductionHelper())
      {
      labelString = "Induce OSR Call Snippet";
      glueRef = methodSymRef;
      }
   else if (methodSymRef->isUnresolved() || forceUnresolvedDispatch)
      {
      labelString = "Unresolved Direct Call Snippet";
      if (methodSymbol->isSpecial())
         {
         glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedSpecialGlue);
         }
      else if (methodSymbol->isStatic())
         {
         glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedStaticGlue);
         }
      else
         {
         glueRef = _cg->getSymRef(TR_PPCinterpreterUnresolvedDirectVirtualGlue);
         }
      }
   else
      {
      if (methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative())
         {
         labelString = "Native Static Direct Call Snippet";
         glueRef = _cg->getSymRef(TR_PPCnativeStaticHelper);
         isNativeStatic = true;
         }
      else
         {
         labelString = methodSymbol->isJNI() ? "Interpreted JNI Direct Call Snippet" : "Interpreted Direct Call Snippet";
         bool synchronised = methodSymbol->isSynchronised();
         switch (callNode->getDataType())
            {
            case TR::NoType:
               if (synchronised)
                  glueRef = _cg->getSymRef(TR_PPCinterpreterSyncVoidStaticGlue);
               else
                  glueRef = _cg->getSymRef(TR_PPCinterpreterVoidStaticGlue);
               break;

            case TR::Int32:
               if (synchronised)
                  glueRef = _cg->getSymRef(TR_PPCinterpreterSyncGPR3StaticGlue);
               else
                  glueRef = _cg->getSymRef(TR_PPCinterpreterGPR3StaticGlue);
               break;

            case TR::Address:
               if (TR::Compiler->target.is64Bit())
                  {
                  if (synchronised)
                     glueRef = _cg->getSymRef(TR_PPCinterpreterSyncGPR3GPR4StaticGlue);
                  else
                     glueRef = _cg->getSymRef(TR_PPCinterpreterGPR3GPR4StaticGlue);
                  }
               else
                  {
                  if (synchronised)
                     glueRef = _cg->getSymRef(TR_PPCinterpreterSyncGPR3StaticGlue);
                  else
                     glueRef = _cg->getSymRef(TR_PPCinterpreterGPR3StaticGlue);
                  }
               break;

            case TR::Int64:
               if (synchronised)
                  glueRef = _cg->getSymRef(TR_PPCinterpreterSyncGPR3GPR4StaticGlue);
               else
                  glueRef = _cg->getSymRef(TR_PPCinterpreterGPR3GPR4StaticGlue);
               break;

            case TR::Float:
               if (synchronised)
                  glueRef = _cg->getSymRef(TR_PPCinterpreterSyncFPR0FStaticGlue);
               else
                  glueRef = _cg->getSymRef(TR_PPCinterpreterFPR0FStaticGlue);
               break;

            case TR::Double:
               if (synchronised)
                  glueRef = _cg->getSymRef(TR_PPCinterpreterSyncFPR0DStaticGlue);
               else
                  glueRef = _cg->getSymRef(TR_PPCinterpreterFPR0DStaticGlue);
               break;

            default:
               TR_ASSERT(0, "Bad return data type for a call node.  DataType was %s\n",
               getName(callNode->getDataType()));
            }
         }
      }

   TR_ASSERT(glueRef && labelString, "Expecting to have symref and label string at this point");

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, labelString);

   cursor = printPPCArgumentsFlush(pOutFile, callNode, cursor, snippet->getSizeOfArguments());

   char    *info = "";
   int32_t  distance;
   if (isBranchToTrampoline(glueRef, cursor, distance))
      info = " Through trampoline";

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;     // sign extend
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s", (intptrj_t)cursor + distance, info);
   cursor += 4;

   if (isNativeStatic)
      {
      printPrefix(pOutFile, NULL, cursor, 4);
      distance = *((int32_t *) cursor) & 0x03fffffc;
      distance = (distance << 6) >> 6;     // sign extend
      trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t;%s", (intptrj_t)cursor + distance, " back to program code");
      cursor += 4;

      if (TR::Compiler->target.is64Bit())
         {
         printPrefix(pOutFile, NULL, cursor, 4);
         trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Padding", *(int32_t *)cursor);
         cursor += 4;
         }
      }
   else
      {
      printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
      trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Call Site RA", snippet->getCallRA());
      cursor += sizeof(intptrj_t);
      }

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Method Pointer", *(uintptrj_t *)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; Lock Word For Compilation", *(int32_t *)cursor);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCUnresolvedCallSnippet * snippet)
   {
   uint8_t *cursor = snippet->getSnippetLabel()->getCodeLocation() + snippet->getLength(0) - (8+sizeof(intptrj_t));

   TR::SymbolReference *methodSymRef = snippet->getNode()->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();

   int32_t helperLookupOffset;
   switch (snippet->getNode()->getDataType())
      {
      case TR::NoType:
         helperLookupOffset = 0;
         break;
      case TR::Int32:
      case TR::Address:
         helperLookupOffset = 4;
         break;
      case TR::Int64:
         helperLookupOffset = 8;
         break;
      case TR::Float:
         helperLookupOffset = 12;
         break;
      case TR::Double:
         helperLookupOffset = 16;
         break;
      }

   print(pOutFile, (TR::PPCCallSnippet *) snippet);

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile,
           ".long \t0x%08x\t\t; Offset | Flag | CP Index",
           (helperLookupOffset << 24) | methodSymRef->getCPIndexForVM());
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Pointer To Constant Pool", *(intptrj_t *)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; Lock Word For Resolution", *(int32_t *)cursor);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCVirtualSnippet * snippet)
   {
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCVirtualUnresolvedSnippet * snippet)
   {
   uint8_t            *cursor   = snippet->getSnippetLabel()->getCodeLocation();
   TR::Node            *callNode = snippet->getNode();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Virtual Unresolved Call Snippet");

   char    *info = "";
   int32_t  distance;
   if (isBranchToTrampoline(_cg->getSymRef(TR_PPCvirtualUnresolvedHelper), cursor, distance))
      info = " Through trampoline";

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s", (intptrj_t)cursor + distance, info);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; Back to program code", (intptrj_t)cursor + distance);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Call Site RA", (intptrj_t)snippet->getReturnLabel()->getCodeLocation());
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Constant Pool Pointer", (intptrj_t)getOwningMethod(callNode->getSymbolReference())->constantPool());
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Constant Pool Index", callNode->getSymbolReference()->getCPIndexForVM());
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Private J9Method pointer", *(intptrj_t *)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; J2I thunk address for private", *(intptrj_t *)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, 4);
   trfprintf(pOutFile, ".long \t0x%08x\t\t; Lock Word For Resolution", *(int32_t *)cursor);
   }

void
TR_Debug::print(TR::FILE *pOutFile, TR::PPCInterfaceCallSnippet * snippet)
   {
   uint8_t            *cursor   = snippet->getSnippetLabel()->getCodeLocation();
   TR::Node            *callNode = snippet->getNode();

   printSnippetLabel(pOutFile, snippet->getSnippetLabel(), cursor, "Interface Call Snippet");

   char    *info = "";
   int32_t  distance;
   if (isBranchToTrampoline(_cg->getSymRef(TR_PPCinterfaceCallHelper), cursor, distance))
      info = " Through trampoline";

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "bl \t" POINTER_PRINTF_FORMAT "\t\t;%s", (intptrj_t)cursor + distance, info);
   cursor += 4;

   printPrefix(pOutFile, NULL, cursor, 4);
   distance = *((int32_t *) cursor) & 0x03fffffc;
   distance = (distance << 6) >> 6;   // sign extend
   trfprintf(pOutFile, "b \t" POINTER_PRINTF_FORMAT "\t\t; Back to program code", (intptrj_t)cursor + distance);
   cursor += 4;

   if (TR::Compiler->target.is64Bit())
      {
      printPrefix(pOutFile, NULL, cursor, 4);
      trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Padding", *(int32_t *)cursor);
      cursor += 4;
      }

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Constant Pool Pointer", *(uintptrj_t*)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Constant Pool Index", *(uintptrj_t*)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Interface Class Pointer", *(uintptrj_t*)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; ITable Index", *(uintptrj_t*)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; First Class Pointer", *(int32_t *)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; First Class Target", *(int32_t *)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Second Class Pointer", *(int32_t *)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; Second Class Target", *(int32_t *)cursor);
   cursor += sizeof(intptrj_t);

   printPrefix(pOutFile, NULL, cursor, sizeof(intptrj_t));
   trfprintf(pOutFile, ".long \t" POINTER_PRINTF_FORMAT "\t\t; J2I thunk address for private", *(intptrj_t *)cursor);
   }

