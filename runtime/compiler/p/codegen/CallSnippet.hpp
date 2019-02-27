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

#ifndef PPCCALLSNIPPET_INCL
#define PPCCALLSNIPPET_INCL

#include "codegen/Snippet.hpp"
#include "env/VMJ9.h"
#include "infra/Annotations.hpp"
#include "p/codegen/PPCInstruction.hpp"

namespace TR { class CodeGenerator; }
class TR_J2IThunk;

extern void ppcCodeSync(uint8_t *codePointer, uint32_t codeSize);

namespace TR {

class PPCCallSnippet : public TR::Snippet
   {
   uint8_t *callRA;
   int32_t  sizeOfArguments;

   bool needsGCMap(TR::CodeGenerator *cg, TR::SymbolReference *methodSymRef)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(cg->fe());
      if (OMR_UNLIKELY(cg->comp()->compileRelocatableCode() && !cg->comp()->getOption(TR_UseSymbolValidationManager)))
         return false;
      TR::MethodSymbol *methodSymbol = methodSymRef->getSymbol()->castToMethodSymbol();
      return !methodSymRef->isUnresolved() &&
             (methodSymbol->isVMInternalNative() || methodSymbol->isJITInternalNative());
      }

   protected:
   TR_RuntimeHelper getInterpretedDispatchHelper(TR::SymbolReference *methodSymRef,
                                                 TR::DataType type, bool isSynchronized,
                                                 bool& isNativeStatic, TR::CodeGenerator* cg);
   TR::SymbolReference * _realMethodSymbolReference;

   public:

   PPCCallSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s)
      : TR::Snippet(cg, c, lab, needsGCMap(cg, c->getSymbolReference())), sizeOfArguments(s), callRA(0)
      {
      _realMethodSymbolReference = NULL;
      }

   PPCCallSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, TR::SymbolReference *symRef, int32_t s)
      : TR::Snippet(cg, c, lab, needsGCMap(cg, symRef ? symRef : c->getSymbolReference())), sizeOfArguments(s), callRA(0)
      {
      _realMethodSymbolReference = symRef;
      }

   virtual Kind getKind() { return IsCall; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   int32_t getSizeOfArguments()          {return sizeOfArguments;}
   int32_t setSizeOfArguments(int32_t s) {return (sizeOfArguments = s);}

   uint8_t *getCallRA() {return callRA;}
   uint8_t *setCallRA(uint8_t *ra) {return (callRA=ra);}

   TR::SymbolReference *getRealMethodSymbolReference() {return _realMethodSymbolReference;}
   void setRealMethodSymbolReference(TR::SymbolReference *sf) {_realMethodSymbolReference = sf;}

   uint8_t *setUpArgumentsInRegister(uint8_t *buffer, TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg);

   static uint8_t *generateVIThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg);
   static TR_J2IThunk *generateInvokeExactJ2IThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg, char *signature);
   static int32_t instructionCountForArguments(TR::Node *callNode, TR::CodeGenerator *cg);
   };

class PPCUnresolvedCallSnippet : public TR::PPCCallSnippet
   {

   public:

   PPCUnresolvedCallSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s)
      : TR::PPCCallSnippet(cg, c, lab, s)
      {
      }

   virtual Kind getKind() { return IsUnresolvedCall; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

class PPCVirtualSnippet : public TR::Snippet
   {
   TR::LabelSymbol *returnLabel;
   int32_t  sizeOfArguments;

   public:

   PPCVirtualSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s, TR::LabelSymbol *retl, bool isGCSafePoint = false)
      : TR::Snippet(cg, c, lab, isGCSafePoint), sizeOfArguments(s), returnLabel(retl) {}

   virtual Kind getKind() { return IsVirtual; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   int32_t getSizeOfArguments()          {return sizeOfArguments;}
   int32_t setSizeOfArguments(int32_t s) {return (sizeOfArguments = s);}

   TR::LabelSymbol *getReturnLabel() {return returnLabel;}
   TR::LabelSymbol *setReturnLabel(TR::LabelSymbol *rl) {return (returnLabel=rl);}
   };

class PPCVirtualUnresolvedSnippet : public TR::PPCVirtualSnippet
   {
   uint8_t *thunkAddress;
   public:

   PPCVirtualUnresolvedSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s, TR::LabelSymbol *retl)
      : TR::PPCVirtualSnippet(cg, c, lab, s, retl, true), thunkAddress(NULL)
      {
      }

   PPCVirtualUnresolvedSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s, TR::LabelSymbol *retl, uint8_t *thunkPtr)
      : TR::PPCVirtualSnippet(cg, c, lab, s, retl, true), thunkAddress(thunkPtr)
      {
      }

   virtual Kind getKind() { return IsVirtualUnresolved; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

class PPCInterfaceCallSnippet : public TR::PPCVirtualSnippet
   {
   TR::Instruction *_upperInstruction, *_lowerInstruction;
   int32_t            _tocOffset;
   uint8_t *thunkAddress;

   public:

   PPCInterfaceCallSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s, TR::LabelSymbol *retl)
      : TR::PPCVirtualSnippet(cg, c, lab, s, retl, true),
        _upperInstruction(NULL), _lowerInstruction(NULL), _tocOffset(0), thunkAddress(NULL)
      {
      }

   PPCInterfaceCallSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s, TR::LabelSymbol *retl, uint8_t *thunkPtr)
      : TR::PPCVirtualSnippet(cg, c, lab, s, retl, true),
        _upperInstruction(NULL), _lowerInstruction(NULL), _tocOffset(0), thunkAddress(thunkPtr)
      {
      }

   virtual Kind getKind() { return IsInterfaceCall; }

   TR::Instruction *getUpperInstruction() {return _upperInstruction;}
   TR::Instruction *setUpperInstruction(TR::Instruction *pi)
      {
      return (_upperInstruction = pi);
      }

   TR::Instruction *getLowerInstruction() {return _lowerInstruction;}
   TR::Instruction *setLowerInstruction(TR::Instruction *pi)
      {
      return (_lowerInstruction = pi);
      }

   int32_t getTOCOffset() {return _tocOffset;}
   void setTOCOffset(int32_t v) {_tocOffset = v;}

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };

}

#endif
