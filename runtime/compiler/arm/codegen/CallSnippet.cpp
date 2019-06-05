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

#include "codegen/CallSnippet.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Register.hpp"
#include "codegen/ARMAOTRelocation.hpp"
#include "env/CompilerEnv.hpp"
#include "env/J2IThunk.hpp"
#include "env/VMJ9.h"
#include "codegen/CodeGenerator.hpp" /* @@@@ */
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

#define TR_ARM_ARG_SLOT_SIZE 4

static uint8_t *flushArgumentsToStack(uint8_t *buffer, TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg)
   {
   uint32_t        intArgNum=0, floatArgNum=0, offset;
   TR::Machine *machine = cg->machine();
   const TR::ARMLinkageProperties &linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention())->getProperties();
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
         case TR::Address:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
         case TR::Float:
#endif
            if (!linkage.getRightToLeft())
               offset -= 4;
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(ARMOp_str, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               }
            intArgNum++;
            if (linkage.getRightToLeft())
               offset += 4;
            break;
         case TR::Int64:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
         case TR::Double:
#endif
            if (!linkage.getRightToLeft())
               offset -= 8;
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               buffer = storeArgumentItem(ARMOp_str, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum)), offset, cg);
               if (intArgNum < linkage.getNumIntArgRegs()-1)
           	  {
           	  buffer = storeArgumentItem(ARMOp_str, buffer, machine->getRealRegister(linkage.getIntegerArgumentRegister(intArgNum+1)), offset+4, cg);
           	  }
               }
            intArgNum += 2;
            if (linkage.getRightToLeft())
               offset += 8;
            break;
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
         case TR::Float:
            /* TODO
               if (!linkage.getRightToLeft())
               offset -= 4;
               if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               buffer = storeArgumentItem(ARMOp_stfs, buffer, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
               floatArgNum++;
               if (linkage.getRightToLeft())
               offset += 4;
            */
            break;
         case TR::Double:
            /* TODO
               if (!linkage.getRightToLeft())
               offset -= 8;
               if (floatArgNum < linkage.getNumFloatArgRegs())
               {
               buffer = storeArgumentItem(ARMOp_stfd, buffer, machine->getRealRegister(linkage.getFloatArgumentRegister(floatArgNum)), offset, cg);
               }
               floatArgNum++;
               if (linkage.getRightToLeft())
               offset += 8;
            */
            break;
#endif
         }
      }
   return(buffer);
   }

static int32_t instructionCountForArguments(TR::Node *callNode, TR::CodeGenerator *cg)
   {
   uint32_t        intArgNum=0, floatArgNum=0, count=0;
   const TR::ARMLinkageProperties &linkage = cg->getLinkage(callNode->getSymbol()->castToMethodSymbol()->getLinkageConvention())->getProperties();
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
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
         case TR::Float:
#endif
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               count++;
               }
            intArgNum++;
            break;
         case TR::Int64:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
         case TR::Double:
#endif
            if (intArgNum < linkage.getNumIntArgRegs())
               {
               count++;
               if (intArgNum < linkage.getNumIntArgRegs()-1)
           	  {
        	  count++;
           	  }
               }
            intArgNum += 2;
            break;
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
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
#endif
         }
      }
   return(count);
   }

TR_RuntimeHelper
TR::ARMCallSnippet::getHelper()
   {
   TR::Compilation * comp = cg()->comp();
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg()->fe());
   TR::Node *callNode = getNode();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::SymbolReference *glueRef = NULL;

   if (methodSymRef->isUnresolved() || comp->compileRelocatableCode())
      {
      if (methodSymbol->isSpecial())
         return TR_ARMinterpreterUnresolvedSpecialGlue;
      if (methodSymbol->isStatic())
         return TR_ARMinterpreterUnresolvedStaticGlue;
      return TR_ARMinterpreterUnresolvedDirectVirtualGlue;
      }

   bool synchronised = methodSymbol->isSynchronised();

   if (methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative())
      return TR_ARMnativeStaticHelper;

   TR::DataType dataType = callNode->getDataType();
   switch (dataType)
      {
      case TR::NoType:
         if (synchronised)
            return TR_ARMinterpreterSyncVoidStaticGlue;
         return TR_ARMinterpreterVoidStaticGlue;

      case TR::Int32:
      case TR::Address:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::Float:
#endif
         if (synchronised)
            return TR_ARMinterpreterSyncGPR3StaticGlue;
         return TR_ARMinterpreterGPR3StaticGlue;

      case TR::Int64:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::Double:
#endif
         if (synchronised)
            return TR_ARMinterpreterSyncGPR3GPR4StaticGlue;
         return TR_ARMinterpreterGPR3GPR4StaticGlue;
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
      case TR::Float:
         if (synchronised)
            return TR_ARMinterpreterSyncFPR0FStaticGlue;
         return TR_ARMinterpreterFPR0FStaticGlue;

      case TR::Double:
         if (synchronised)
            return TR_ARMinterpreterSyncFPR0DStaticGlue;
         return TR_ARMinterpreterFPR0DStaticGlue;
#endif
      default:
         TR_ASSERT(false, "Bad return data type for a call node.  DataType was %s\n",
                  cg()->getDebug()->getName(dataType));
         return (TR_RuntimeHelper)0;
      }
   }

uint8_t *TR::ARMCallSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg()->fe());
   uint8_t            *cursor = cg()->getBinaryBufferCursor();
   TR::Node            *callNode = getNode();
   TR::SymbolReference *methodSymRef = callNode->getSymbolReference();
   TR::MethodSymbol    *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   TR::SymbolReference *glueRef;
   TR::Compilation *comp = cg()->comp();
   void               *trmpln = NULL;

   getSnippetLabel()->setCodeLocation(cursor);

   // Flush in-register arguments back to the stack for interpreter
   cursor = flushArgumentsToStack(cursor, callNode, getSizeOfArguments(), cg());

   glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(getHelper(), false, false, false);

   // bl glueRef
   *(int32_t *)cursor = encodeHelperBranchAndLink(glueRef, cursor, callNode, cg());
   cursor += 4;

   // Store the code cache RA
   *(int32_t *)cursor = (intptr_t)getCallRA();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
        						 cursor,
        						 NULL,
        						 TR_AbsoluteMethodAddress, cg()), __FILE__, __LINE__, getNode());
   cursor += 4;

   // Store the method pointer: it is NULL for unresolved
   if (methodSymRef->isUnresolved() || comp->compileRelocatableCode())
      {
      *(int32_t *)cursor = 0;
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
      *(int32_t *)cursor = (uintptr_t)methodSymbol->getMethodAddress();
      if (comp->getOption(TR_EnableHCR))
         cg()->jitAddPicToPatchOnClassRedefinition((void *)methodSymbol->getMethodAddress(), (void *)cursor);
         cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)methodSymRef,
                                                                                 getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                 TR_MethodObject, cg()),
                                      __FILE__, __LINE__, callNode);
      /*
      TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
      recordInfo->data1 = (uintptr_t)methodSymRef;
      recordInfo->data2 = (uintptr_t)(getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1);
      recordInfo->data3 = (uintptr_t)fixedSequence1;

      cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                             cursor,
                             (uint8_t *)recordInfo,
                             TR_MethodObject, cg()), __FILE__, __LINE__, getNode());
       */
      }
   cursor += TR::Compiler->om.sizeofReferenceAddress();

   // Lock word initialized to 0
   *(int32_t *)cursor = 0;

   return (cursor+4);
   }

uint32_t TR::ARMCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return((instructionCountForArguments(getNode(), cg()) + 4) * 4);
   }


uint8_t *TR::ARMUnresolvedCallSnippet::emitSnippetBody()
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg()->fe());
   uint8_t *cursor = TR::ARMCallSnippet::emitSnippetBody();

   TR::SymbolReference *methodSymRef = getNode()->getSymbolReference();
   TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
   int32_t helperLookupOffset;

   TR::Compilation* comp = cg()->comp();

   switch (getNode()->getDataType())
      {
      case TR::NoType:
         helperLookupOffset = 0;
         break;
      case TR::Int32:
      case TR::Address:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::Float:
#endif
         helperLookupOffset = 4;
         break;
      case TR::Int64:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::Double:
#endif
         helperLookupOffset = 8;
         break;
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
      case TR::Float:
         helperLookupOffset = 12;
         break;
      case TR::Double:
         helperLookupOffset = 16;
         break;
#endif
      }

   *(int32_t *)cursor = (helperLookupOffset<<24) | methodSymRef->getCPIndexForVM();
   cursor += 4;
   *(int32_t *)cursor = (intptr_t)methodSymRef->getOwningMethod(comp)->constantPool();

   if (comp->compileRelocatableCode() && comp->getOption(TR_TraceRelocatableDataDetailsCG))
      {
      traceMsg(comp, "<relocatableDataTrampolinesCG>\n");
      traceMsg(comp, "%s\n", comp->signature());
      traceMsg(comp, "%-8s", "cpIndex");
      traceMsg(comp, "cp\n");
      traceMsg(comp, "%-8x", methodSymRef->getCPIndexForVM());
      traceMsg(comp, "%x\n", methodSymRef->getOwningMethod(comp)->constantPool());
      traceMsg(comp, "</relocatableDataTrampolinesCG>\n");
      }

   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
        						 cursor,
        						 *(uint8_t **)cursor,
        						 getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
        						 TR_Trampolines, cg()), __FILE__, __LINE__, getNode());

   return cursor+4;
   }

uint32_t TR::ARMUnresolvedCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return TR::ARMCallSnippet::getLength(estimatedSnippetStart) + 8;
   }

uint8_t *TR::ARMVirtualUnresolvedSnippet::emitSnippetBody()
   {
   uint8_t            *cursor = cg()->getBinaryBufferCursor();
   TR::SymbolReference *methodSymRef = getNode()->getSymbolReference();
   TR::SymbolReference *glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARMvirtualUnresolvedHelper, false, false, false);

   TR::Compilation* comp = cg()->comp();

   getSnippetLabel()->setCodeLocation(cursor);

   // bl glueRef
   *(int32_t *)cursor = encodeHelperBranchAndLink(glueRef, cursor, getNode(), cg());
   cursor += 4;

   // Store the code cache RA
   *(int32_t *)cursor = (intptr_t)getReturnLabel()->getCodeLocation();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
        						 cursor,
        						 NULL,
        						 TR_AbsoluteMethodAddress, cg()), __FILE__, __LINE__, getNode());
   cursor += 4;

   // CP
   *(int32_t *)cursor = (intptr_t)methodSymRef->getOwningMethod(comp)->constantPool();

   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                             cursor,
                             *(uint8_t **)cursor,
                             getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                             TR_Thunks, cg()), __FILE__, __LINE__, getNode());

   cursor += 4;

   // CP index
   *(int32_t *)cursor = methodSymRef->getCPIndexForVM();

   return (cursor + 4);
   }

uint32_t TR::ARMVirtualUnresolvedSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 16;
   }

uint8_t *TR::ARMInterfaceCallSnippet::emitSnippetBody()
   {
   uint8_t            *cursor = cg()->getBinaryBufferCursor();
   TR::SymbolReference *methodSymRef = getNode()->getSymbolReference();
   TR::SymbolReference *glueRef = cg()->symRefTab()->findOrCreateRuntimeHelper(TR_ARMinterfaceCallHelper, false, false, false);

   getSnippetLabel()->setCodeLocation(cursor);

   // bl glueRef
   *(int32_t *)cursor = encodeHelperBranchAndLink(glueRef, cursor, getNode(), cg());
   cursor += 4;

   // Store the code cache RA
   *(int32_t *)cursor = (intptr_t)getReturnLabel()->getCodeLocation();
   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
        						 cursor,
        						 NULL,
        						 TR_AbsoluteMethodAddress, cg()), __FILE__, __LINE__, getNode());
   cursor += 4;

   *(int32_t *)cursor = (intptr_t)methodSymRef->getOwningMethod(cg()->comp())->constantPool();

   cg()->addExternalRelocation(new (cg()->trHeapMemory()) TR::ExternalRelocation(
                             cursor,
                             *(uint8_t **)cursor,
                             getNode() ? (uint8_t *)getNode()->getInlinedSiteIndex() : (uint8_t *)-1,
                             TR_Thunks, cg()), __FILE__, __LINE__, getNode());

   cursor += 4;

   *(int32_t *)cursor = methodSymRef->getCPIndexForVM();
   cursor += 4;

   // Add 2 more slots for resolved values
   *(int32_t *)cursor = 0;
   cursor += 4;
   *(int32_t *)cursor = 0;
   cursor += 4;

#if 0
   // AOT TODO
   // Patch up the main line codes
   int32_t *patchAddress1 = NULL; // TODO (int32_t *)getUpperInstruction()->getBinaryEncoding();
   *patchAddress1 |= (((int32_t)cursor>>16) + (((int32_t)cursor & (1<<15))?1:0)) & 0x0000ffff;

   int32_t *patchAddress2 = NULL; // TODO (int32_t *)getLowerInstruction()->getBinaryEncoding();
   *patchAddress2 |= (int32_t)cursor & 0x0000ffff;
   cg()->addRelocation(new (cg()->trHeapMemory()) TR::ExternalOrderedPair32BitRelocation(
								    (uint8_t *)patchAddress1,
								    (uint8_t *)patchAddress2,
								    NULL,
								    TR_AbsoluteMethodAddress));
#endif

   return cursor;
   }

uint32_t TR::ARMInterfaceCallSnippet::getLength(int32_t estimatedSnippetStart)
   {
   return 24;
   }

uint8_t *TR::ARMCallSnippet::generateVIThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *) (cg->fe());
   int32_t  codeSize = 4*(instructionCountForArguments(callNode, cg) + 2) + 8; // Additional 4 bytes to hold size of thunk
   uint8_t *thunk, *buffer, *returnValue;
   int32_t  dispatcher;

   if (cg->comp()->compileRelocatableCode())
      thunk = (uint8_t *)TR::comp()->trMemory()->allocateMemory(codeSize, heapAlloc);
   else
      thunk = (uint8_t *)cg->allocateCodeMemory(codeSize, true, false);
   buffer = returnValue = thunk + 8;
   TR_RuntimeHelper helper;
   TR::DataType dataType = callNode->getDataType();

   switch (dataType)
      {
      case TR::NoType:
         helper = TR_ARMicallVMprJavaSendVirtual0;
         break;
      case TR::Int32:
      case TR::Address:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::Float:
#endif
         helper = TR_ARMicallVMprJavaSendVirtual1;
         break;
      case TR::Int64:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::Double:
#endif
         helper = TR_ARMicallVMprJavaSendVirtualJ;
         break;
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
      case TR::Float:
         helper = TR_ARMicallVMprJavaSendVirtualF;
         break;
      case TR::Double:
         helper = TR_ARMicallVMprJavaSendVirtualD;
         break;
#endif
      default:
         TR_ASSERT(false, "Bad return data type for a call node.  DataType was %s\n",
                  cg->getDebug()->getName(dataType));
      }

   dispatcher = (intptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false)->getMethodAddress();

   buffer = flushArgumentsToStack(buffer, callNode, argSize, cg);

   *(int32_t *)buffer = 0xE51FF004;           // ldr r15, [r15, #-4]
   buffer += 4;

   *((int32_t *)thunk + 1) = buffer - returnValue;  // patch offset for AOT relocation

   *(int32_t *)buffer = dispatcher;           // address of transition target
   buffer += 4;

   *(int32_t *)thunk = buffer - returnValue;        // patch size of thunk

#ifdef TR_HOST_ARM
   armCodeSync(thunk, codeSize);
#endif

   return(returnValue);
   }

TR_J2IThunk *TR::ARMCallSnippet::generateInvokeExactJ2IThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg, char *signature)
   {
   int32_t  codeSize = 4*(instructionCountForArguments(callNode, cg) + 2) + 8; // Additional 4 bytes to hold size of thunk
   int32_t  dispatcher;
   TR_J2IThunkTable *thunkTable = TR::comp()->getPersistentInfo()->getInvokeExactJ2IThunkTable();
   TR_J2IThunk      *thunk      = TR_J2IThunk::allocate(codeSize, signature, cg, thunkTable);
   uint8_t          *buffer     = thunk->entryPoint();

   TR_RuntimeHelper helper;
   TR::DataType dataType = callNode->getDataType();

   switch (dataType)
      {
      case TR::NoType:
         helper = TR_icallVMprJavaSendInvokeExact0;
         break;
      case TR::Int32:
      case TR::Address:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::Float:
#endif
         helper = TR_icallVMprJavaSendInvokeExact1;
         break;
      case TR::Int64:
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      case TR::Double:
#endif
         helper = TR_icallVMprJavaSendInvokeExactJ;
         break;
#if !defined(__VFP_FP__) || defined(__SOFTFP__)
      case TR::Float:
         helper = TR_icallVMprJavaSendInvokeExactF;
         break;
      case TR::Double:
         helper = TR_icallVMprJavaSendInvokeExactD;
         break;
#endif
      default:
         TR_ASSERT(false, "Bad return data type for a call node.  DataType was %s\n",
                  cg->getDebug()->getName(dataType));
      }

   dispatcher = (intptr_t)cg->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false)->getMethodAddress();

   buffer = flushArgumentsToStack(buffer, callNode, argSize, cg);

   *(int32_t *)buffer = 0xE51FF004;           // ldr r15, [r15, #-4]
   buffer += 4;

   *(int32_t *)buffer = dispatcher;           // address of transition target
   buffer += 4;

#ifdef TR_HOST_ARM
   armCodeSync(thunk->entryPoint(), codeSize);
#endif

   return(thunk);
   }

