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

#ifndef X86CALLSNIPPET_INCL
#define X86CALLSNIPPET_INCL

#include "codegen/Snippet.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class MethodSymbol; }
namespace TR { class SymbolReference; }

namespace TR {

class X86PicDataSnippet : public TR::Snippet
   {
   TR::SymbolReference *_methodSymRef;
   TR::SymbolReference *_dispatchSymRef;
   TR::Instruction     *_slotPatchInstruction;
   TR::Instruction     *_startOfPicInstruction;
   TR::LabelSymbol      *_doneLabel;
   uint8_t            *_thunkAddress;
   int32_t             _numberOfSlots;
   bool                _isInterface;
   bool                _hasJ2IThunkInPicData;

   public:

   X86PicDataSnippet(
      int32_t              numberOfSlots,
      TR::Instruction      *startOfPicInstruction,
      TR::LabelSymbol       *snippetLabel,
      TR::LabelSymbol       *doneLabel,
      TR::SymbolReference  *methodSymRef,
      TR::Instruction      *slotPatchInstruction,
      uint8_t             *thunkAddress,
      bool                 isInterface,
      TR::CodeGenerator *cg) :
         TR::Snippet(cg, NULL, snippetLabel, true),
         _numberOfSlots(numberOfSlots),
         _startOfPicInstruction(startOfPicInstruction),
         _methodSymRef(methodSymRef),
         _doneLabel(doneLabel),
         _slotPatchInstruction(slotPatchInstruction),
         _isInterface(isInterface),
         _dispatchSymRef(NULL),
         _thunkAddress(thunkAddress),
         _hasJ2IThunkInPicData(shouldEmitJ2IThunkPointer())
      {}

   bool isInterface()         {return _isInterface;}
   int32_t getNumberOfSlots() {return _numberOfSlots;}
   bool hasJ2IThunkInPicData() {return _hasJ2IThunkInPicData;}

   TR::SymbolReference *getDispatchSymRef() {return _dispatchSymRef;}
   TR::SymbolReference *getMethodSymRef() {return _methodSymRef;}

   TR::LabelSymbol *getDoneLabel() {return _doneLabel;}

   bool forceUnresolvedDispatch()
      {
      return ((TR_J9VMBase*)(cg()->fe()))->forceUnresolvedDispatch();
      }

   bool unresolvedDispatch()
      {
      return _methodSymRef->isUnresolved() || forceUnresolvedDispatch();
      }

   uint8_t *encodeConstantPoolInfo(uint8_t *cursor);
   uint8_t *encodeJ2IThunkPointer(uint8_t *cursor);

   virtual Kind getKind() { return (_isInterface ? IsIPicData : IsVPicData); }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   private:
   bool shouldEmitJ2IThunkPointer();
   };

class X86CallSnippet : public TR::Snippet
   {
   public:

   X86CallSnippet(TR::CodeGenerator *cg, TR::Node * n, TR::LabelSymbol * lab, bool isGCSafePoint)
      : TR::Snippet(cg, n, lab, isGCSafePoint)
      {
      _realMethodSymbolReference = NULL;
      }

   X86CallSnippet(TR::CodeGenerator *cg, TR::Node * n, TR::LabelSymbol * lab, TR::SymbolReference* realSymRef, bool isGCSafePoint)
      : TR::Snippet(cg, n, lab, isGCSafePoint)
      {
      _realMethodSymbolReference = realSymRef;
      }

   virtual Kind getKind() { return IsCall; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   TR::SymbolReference *getRealMethodSymbolReference() { return _realMethodSymbolReference; }

   private:

   uint8_t * alignCursorForCodePatching(uint8_t *cursor, bool alignWithNOPs=false);

   TR::SymbolReference * _realMethodSymbolReference;
   };

}

#endif
