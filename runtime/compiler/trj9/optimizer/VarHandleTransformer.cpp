/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "optimizer/VarHandleTransformer.hpp"
#include <stddef.h>                            // for size_t
#include <stdint.h>                            // for int32_t, uint32_t, etc
#include "codegen/CodeGenerator.hpp"           // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                // for TR_FrontEnd, feGetEnv, etc
#include "compile/Compilation.hpp"             // for Compilation, comp, etc
#include "compile/ResolvedMethod.hpp"          // for TR_ResolvedMethod
#include "compile/SymbolReferenceTable.hpp"    // for SymbolReferenceTable, etc
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"         // for TR::Options, etc
#include "env/CompilerEnv.hpp"
#include "exceptions/AOTFailure.hpp"
#include "exceptions/FSDFailure.hpp"
#include "env/IO.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::treetop, etc
#include "il/ILOps.hpp"                        // for ILOpCode, TR::ILOpCode
#include "il/Node.hpp"                         // for Node, etc
#include "il/Node_inlines.hpp"                 // for Node::getFirstChild, etc
#include "il/Symbol.hpp"                       // for Symbol, etc
#include "il/SymbolReference.hpp"              // for SymbolReference, etc
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"              // for TreeTop::getNode, etc
#include "il/symbol/MethodSymbol.hpp"          // for MethodSymbol, etc
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "optimizer/Optimization.hpp"          // for Optimization
#include "optimizer/Optimization_inlines.hpp"  // for trace()
#include "optimizer/J9TransformUtil.hpp"       // for calculateElementAddress

// All VarHandle Access methods
/*
get
set
getVolatile
setVolatile
getOpaque
setOpaque
getAcquire
setRelease
compareAndSet
compareAndExchangeVolatile
compareAndExchangeAcquire
compareAndExchangeRelease
weakCompareAndSet
weakCompareAndSetAcquire
weakCompareAndSetRelease
getAndSet
getAndAdd
addAndGet
*/
#define VarHandleParmLength 28
#define VarHandleParam "Ljava/lang/invoke/VarHandle;"
#define JSR292_MethodHandle   "java/lang/invoke/MethodHandle"
#define JSR292_asType              "asType"
#define JSR292_asTypeSig           "(Ljava/lang/invoke/MethodHandle;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/MethodHandle;"
#define JSR292_invokeExactTargetAddress    "invokeExactTargetAddress"
#define JSR292_invokeExactTargetAddressSig "()J"
#define JSR292_invokeExact    "invokeExact"
#define JSR292_invokeExactSig "([Ljava/lang/Object;)Ljava/lang/Object;"


struct X
   {
   TR::RecognizedMethod _enum;
   char _nameLen;
   const char * _name;
   int16_t _sigLen;
   const char * _sig;
   const int32_t tableIndex;
};

// Method names for unresolved VarHandle access methods
static X VarHandleMethods[] =
      {
      // We only get the original method name when the call is unresolved, but recognized method only works for resovled method
      // The list for resolved method is in j9method.cpp, any changes in this list must be reflected in the other
      // Notice this list has a tableIndex field to indicate the index of MethodHandle/MethodType of VarHandle access operation
      // in the handleTable/typeTable, check java/lang/invoke/VarHandle.java for the indice of the methods
      {  TR::java_lang_invoke_VarHandle_get                       ,    3, "get",                             (int16_t)-1, "*",  0},
      {  TR::java_lang_invoke_VarHandle_set                       ,    3, "set",                             (int16_t)-1, "*",  1},
      {  TR::java_lang_invoke_VarHandle_getVolatile               ,   11, "getVolatile",                     (int16_t)-1, "*",  2},
      {  TR::java_lang_invoke_VarHandle_setVolatile               ,   11, "setVolatile",                     (int16_t)-1, "*",  3},
      {  TR::java_lang_invoke_VarHandle_getOpaque                 ,    9, "getOpaque",                       (int16_t)-1, "*",  4},
      {  TR::java_lang_invoke_VarHandle_setOpaque                 ,    9, "setOpaque",                       (int16_t)-1, "*",  5},
      {  TR::java_lang_invoke_VarHandle_getAcquire                ,   10, "getAcquire",                      (int16_t)-1, "*",  6},
      {  TR::java_lang_invoke_VarHandle_setRelease                ,   10, "setRelease",                      (int16_t)-1, "*",  7},
      {  TR::java_lang_invoke_VarHandle_compareAndSet             ,   13, "compareAndSet",                   (int16_t)-1, "*",  8},
      {  TR::java_lang_invoke_VarHandle_compareAndExchange        ,   18, "compareAndExchange",              (int16_t)-1, "*",  9},
      {  TR::java_lang_invoke_VarHandle_compareAndExchangeAcquire ,   25, "compareAndExchangeAcquire",       (int16_t)-1, "*", 10},
      {  TR::java_lang_invoke_VarHandle_compareAndExchangeRelease ,   25, "compareAndExchangeRelease",       (int16_t)-1, "*", 11},
      {  TR::java_lang_invoke_VarHandle_weakCompareAndSet         ,   17, "weakCompareAndSet",               (int16_t)-1, "*", 12},
      {  TR::java_lang_invoke_VarHandle_weakCompareAndSetAcquire  ,   24, "weakCompareAndSetAcquire",        (int16_t)-1, "*", 13},
      {  TR::java_lang_invoke_VarHandle_weakCompareAndSetRelease  ,   24, "weakCompareAndSetRelease",        (int16_t)-1, "*", 14},
      {  TR::java_lang_invoke_VarHandle_weakCompareAndSetPlain    ,   22, "weakCompareAndSetPlain",          (int16_t)-1, "*", 15},
      {  TR::java_lang_invoke_VarHandle_getAndSet                 ,    9, "getAndSet",                       (int16_t)-1, "*", 16},
      {  TR::java_lang_invoke_VarHandle_getAndSetAcquire          ,   16, "getAndSetAcquire",                (int16_t)-1, "*", 17},
      {  TR::java_lang_invoke_VarHandle_getAndSetRelease          ,   16, "getAndSetRelease",                (int16_t)-1, "*", 18},
      {  TR::java_lang_invoke_VarHandle_getAndAdd                 ,    9, "getAndAdd",                       (int16_t)-1, "*", 19},
      {  TR::java_lang_invoke_VarHandle_getAndAddAcquire          ,   16, "getAndAddAcquire",                (int16_t)-1, "*", 20},
      {  TR::java_lang_invoke_VarHandle_getAndAddRelease          ,   16, "getAndAddRelease",                (int16_t)-1, "*", 21},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseAnd          ,   16, "getAndBitwiseAnd",                (int16_t)-1, "*", 22},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseAndAcquire   ,   23, "getAndBitwiseAndAcquire",         (int16_t)-1, "*", 23},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseAndRelease   ,   23, "getAndBitwiseAndRelease",         (int16_t)-1, "*", 24},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseOr           ,   15, "getAndBitwiseOr",                 (int16_t)-1, "*", 25},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseOrAcquire    ,   23, "getAndBitwiseAndAcquire",         (int16_t)-1, "*", 26},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseOrRelease    ,   23, "getAndBitwiseAndRelease",         (int16_t)-1, "*", 27},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseXor          ,   15, "getAndBitwiseOr",                 (int16_t)-1, "*", 28},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseXorAcquire   ,   23, "getAndBitwiseAndAcquire",         (int16_t)-1, "*", 29},
      {  TR::java_lang_invoke_VarHandle_getAndBitwiseXorRelease   ,   23, "getAndBitwiseAndRelease",         (int16_t)-1, "*", 30},
      {  TR::unknownMethod, 0, 0, 0, 0, -1}
      };

// Recognized method doesn't work for unresolved method, the following code works for both case
TR::RecognizedMethod TR_VarHandleTransformer::getVarHandleAccessMethod(TR::Node * node)
{
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::RecognizedMethod varHandleAccessMethod = TR::unknownMethod;
   OMR::MethodSymbol *symbol = node->getSymbol()->getMethodSymbol();
      TR_J9Method * method = (TR_J9Method*)(symbol->getMethod());
   if (symRef->isUnresolved())
      {
      char *className    = method->classNameChars();
      int   classNameLen = method->classNameLength();
      char *name         = method->nameChars();
      int   nameLen      = method->nameLength();
      // We only need to compare the class and the method name
      if (classNameLen == 26 && !strncmp(className, "java/lang/invoke/VarHandle", classNameLen))
         {
         for (X * m =  VarHandleMethods; m->_enum != TR::unknownMethod; ++m)
            {
            if (m->_nameLen == nameLen && !strncmp(m->_name, name, nameLen))
               {
               varHandleAccessMethod = m->_enum;
               break;
               }
            }
         }
      }
   else
      {
      if (method->isVarHandleAccessMethod(comp()))
         varHandleAccessMethod = method->getMandatoryRecognizedMethod();
      }
   return varHandleAccessMethod;
}

/**
 *
 * Transform calls to VarHandle access methods to MethodHandle.invokeExact()
 */
int32_t TR_VarHandleTransformer::perform()
{
   TR::ResolvedMethodSymbol *methodSymbol = comp()->getMethodSymbol();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)(comp()->fe());
   for (TR::TreeTop * tt = methodSymbol->getFirstTreeTop(); tt != NULL; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode()->getChild(0); // Get the first child of the tree
      if (node && node->getOpCode().isCall()  && !node->getSymbol()->castToMethodSymbol()->isHelper())
         {
         TR::TreeTop *callTree = tt;
         TR::SymbolReference *symRef = node->getSymbolReference();
         OMR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
         TR_Method * method = symbol->getMethod();
         TR::RecognizedMethod varHandleAccessMethod = getVarHandleAccessMethod(node);

         if (varHandleAccessMethod != TR::unknownMethod)
            {
            if (comp()->compileRelocatableCode())
               {
               if (comp()->getOption(TR_TraceILGen))
                  traceMsg(comp(), "A call to VarHandle access method %s is not supported in AOT. Aborting compile.\n", comp()->getDebug()->getName(symRef));
               comp()->failCompilation<J9::AOTHasInvokeVarHandle>("A call to a VarHandle access method is not supported in AOT. Aborting compile.");
               }

            if (comp()->getOption(TR_FullSpeedDebug) && !comp()->isPeekingMethod())
               {
               if (comp()->getOption(TR_TraceILGen))
                  traceMsg(comp(), "A call to VarHandle access method %s is not supported in FSD. Failing ilgen.\n", comp()->getDebug()->getName(symRef));
               comp()->failCompilation<J9::FSDHasInvokeHandle>("A call to a VarHandle access method is not supported in FSD. Failing ilgen.");
               }

            // Anchoring all the children for varhandle
            anchorAllChildren(node, tt);
            performTransformation(comp(), "%sVarHandle access methods found, working on node %p\n", optDetailString(), node);
            TR::Node *varHandle = node->getChild(1); // The first child is vft
            TR::TreeTop *newTreeTop = tt;

            // Preserve the checks on the call tree
            if (callTree->getNode()->getOpCode().isCheck())
               {
               // Remove resolve check and preserve other checks, null check should be preserved with a separate tree
               bool hasNULLCHK = false;
               TR::ILOpCodes opCode = callTree->getNode()->getOpCode().getOpCodeValue();
               TR::ILOpCodes newOpCode = opCode;
               switch (opCode)
                  {
                  case TR::NULLCHK:
                  case TR::ResolveAndNULLCHK:
                       newOpCode = TR::treetop;
                       hasNULLCHK = true;
                       break;
                  case TR::checkcastAndNULLCHK:
                       newOpCode = TR::checkcast;
                       hasNULLCHK = true;
                       break;
                  case TR::ResolveCHK:
                       newOpCode = TR::treetop;
                       break;
                  }
               if (hasNULLCHK)
                  {
                  TR::Node *passthrough = TR::Node::create(node, TR::PassThrough, 1);
                  passthrough->setAndIncChild(0, varHandle);
                  TR::Node * checkNode = TR::Node::createWithSymRef(TR::NULLCHK, 1, 1, passthrough, callTree->getNode()->getSymbolReference());
                  checkNode->copyByteCodeInfo(node);
                  callTree->insertBefore(TR::TreeTop::create(comp(), checkNode));
                  }
               if (newOpCode != opCode)
                  TR::Node::recreate(callTree->getNode(), newOpCode);
               }

             // Get the index into the array
             // Question: should we just use varHandleAccessMethod - TR::java_lang_invoke_VarHandle_get as the index?
             TR::Node * index = TR::Node::iconst(VarHandleMethods[varHandleAccessMethod - TR::java_lang_invoke_VarHandle_get].tableIndex);

             // Load the handleTable containing the method handle to be invoked
             uint32_t handleTableOffset =  fej9->getVarHandleHandleTableOffset(comp());

             TR::SymbolReference *handleTableSymRef = comp()->getSymRefTab()->findOrFabricateShadowSymbol(methodSymbol, TR::Symbol::Java_lang_invoke_VarHandle_handleTable, TR::Address, handleTableOffset, false, false, true, "java/lang/invoke/VarHandle.handleTable [Ljava/lang/invoke/MethodHandle;");
             TR::Node *handleTable = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectLoad(TR::Address), 1, 1, varHandle, handleTableSymRef);
             handleTable->copyByteCodeInfo(node);

             if (comp()->useCompressedPointers())
                callTree->insertBefore(TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(handleTable)));
             else
                callTree->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1, handleTable)));

             TR::Node *methodHandleAddr = J9::TransformUtil::calculateElementAddress(comp(), handleTable, index, TR::Address);
             TR::Node *methodHandle = TR::Node::createWithSymRef(comp()->il.opCodeForIndirectArrayLoad(TR::Address), 1, 1, methodHandleAddr, comp()->getSymRefTab()->findOrCreateArrayShadowSymbolRef(TR::Address, handleTable));
             methodHandle->copyByteCodeInfo(node);

             // Spine check is needed for arraylets, no need for bound check because it's internal code
             if (TR::Compiler->om.canGenerateArraylets())
                {
                TR::SymbolReference * spineCHKSymRef = comp()->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(comp()->getMethodSymbol());
                TR::Node * spineCHK = TR::Node::create(node, TR::SpineCHK, 3);
                spineCHK->setAndIncChild(1, handleTable);
                spineCHK->setAndIncChild(2, index);
                spineCHK->setSymbolReference(spineCHKSymRef);
                spineCHK->setAndIncChild(0, methodHandle);
                spineCHK->setSpineCheckWithArrayElementChild(true);
                callTree->insertBefore(TR::TreeTop::create(comp(), spineCHK));
                }

             if (comp()->useCompressedPointers())
                callTree->insertBefore(TR::TreeTop::create(comp(), TR::Node::createCompressedRefsAnchor(methodHandle)));
             else
                callTree->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1, methodHandle)));

            // TODO:
            //fast path: get the method type from MT[] in the varhandle object
            // and compare it against the cached method type, and check if
            // operation is supported for the field, if so --> fast path

            // Slow path (default path): call MethodHandle.asType to get the converted method type
            int32_t cpIndex = symRef->getCPIndex();
            // Danger: using the compilation method as the owning method only because VarHandleTransformer is an ilgen opt
            // TODO: check if optimizer::getMethodSymbol() works to avoid confusion
            TR::SymbolReference *callSiteMethodTypeSymRef = comp()->getSymRefTab()->findOrCreateVarHandleMethodTypeTableEntrySymbol(methodSymbol, cpIndex);
            TR::Node *callSiteMethodType = TR::Node::createWithSymRef(TR::aload, 0, callSiteMethodTypeSymRef);
            callSiteMethodType->copyByteCodeInfo(node);
            if (callSiteMethodTypeSymRef->isUnresolved())
               {
               TR::Node *resolveChkOnMethodType = TR::Node::createWithSymRef(TR::ResolveCHK, 1, 1, callSiteMethodType, comp()->getSymRefTab()->findOrCreateResolveCheckSymbolRef(methodSymbol));
               callTree->insertBefore(TR::TreeTop::create(comp(), resolveChkOnMethodType));
               }
            // Convert MH
            TR::SymbolReference *typeConversionSymRef = comp()->getSymRefTab()->methodSymRefFromName(methodSymbol, JSR292_MethodHandle, JSR292_asType, JSR292_asTypeSig, OMR::MethodSymbol::Static);
            TR::Node * convertedMethodHandle = TR::Node::createWithSymRef(TR::acall, 2, 2, methodHandle, callSiteMethodType, typeConversionSymRef);
            convertedMethodHandle->copyByteCodeInfo(node);
            callTree->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1, convertedMethodHandle)));

            // Get the method address
            TR::SymbolReference *invokeExactTargetAddrSymRef = comp()->getSymRefTab()->methodSymRefFromName(methodSymbol, JSR292_MethodHandle, JSR292_invokeExactTargetAddress, JSR292_invokeExactTargetAddressSig, OMR::MethodSymbol::Special);
            TR::Node *invokeExactTargetAddr = TR::Node::createWithSymRef(TR::lcall, 1, 1, convertedMethodHandle, invokeExactTargetAddrSymRef);
            invokeExactTargetAddr->copyByteCodeInfo(node);
            callTree->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1, invokeExactTargetAddr)));

            // Construct the signature for the method handle from the signature of the VarHandle method
            // so that the last argument is the VarHandle object
            // i.e. for the call set(B)V, the signature for method handle is (BLjava/lang/invoke/VarHandle;)V
            char *sig = method->signatureChars();
            int32_t sigLen = method->signatureLength();
            char *sigChar = sig + 1; // skip the openning parenthesis
            int32_t paramSigLen = 2; // paramSig is at least 2 parenthesis
            while(*sigChar != ')') { sigChar++; paramSigLen++;}
            char *mhSig = new (comp()->trStackMemory()) char[sigLen + VarHandleParmLength];
            strncpy(mhSig, sig, paramSigLen-1);
            strncpy(mhSig+paramSigLen-1, VarHandleParam, VarHandleParmLength);
            strncpy(mhSig+paramSigLen-1+VarHandleParmLength, sig+paramSigLen-1, sigLen-paramSigLen+1);

            // Create a symref with a resolved method to represent MH with the correct signature
            TR::SymbolReference *invokeExactOriginal = comp()->getSymRefTab()->methodSymRefFromName(methodSymbol, JSR292_MethodHandle, JSR292_invokeExact, JSR292_invokeExactSig, OMR::MethodSymbol::ComputedVirtual);
            TR::SymbolReference *invokeExactSymRef = comp()->getSymRefTab()->methodSymRefWithSignature(invokeExactOriginal, mhSig, sigLen+VarHandleParmLength);
            TR::ILOpCodes callOpCode = method->indirectCallOpCode();

            // Save children except the VarHandle object for the original call before removing it
            TR::list<TR::Node *>* nodeList = new (comp()->trStackMemory()) TR::list<TR::Node*>(getTypedAllocator<TR::Node*>(comp()->allocator()));
            for(int i = 2; i < node->getNumChildren(); i++)
               nodeList->push_back(node->getChild(i));

            int32_t numChildren = method->numberOfExplicitParameters() + 3;  // The extra 3 arguments are: the target method address, the MH receiver, the VarHandle object

            prepareToReplaceNode(node); // This will remove the usedef info, valueNumber info and all children of the node
            TR::Node *invokeCall = TR::Node::recreateWithoutProperties(node, method->indirectCallOpCode(), numChildren, invokeExactSymRef);

            invokeCall->setAndIncChild(0, invokeExactTargetAddr);
            invokeCall->setAndIncChild(1, convertedMethodHandle);
            invokeCall->setAndIncChild(numChildren-1, varHandle);

            // Set other children
            int32_t child_i = 2;
            for(auto cursor = nodeList->begin(); cursor != nodeList->end(); ++cursor)
               {
               TR::Node *nodeCursor = *cursor;
               invokeCall->setAndIncChild(child_i, nodeCursor);
               child_i++;
               }
            }
         }
      }
return 0;
}

const char *
TR_VarHandleTransformer::optDetailString() const throw()
   {
   return "O^O VARHANDLE TRANSFORMER: ";
   }
