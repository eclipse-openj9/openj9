/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#ifndef X86CHECKFAILURESNIPPET_INCL
#define X86CHECKFAILURESNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stdint.h>
#include "codegen/Instruction.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "infra/SimpleRegex.hpp"
#include "il/SymbolReference.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Symbol; }

#define TR_BREAKONTHROW_NPE  1
#define TR_BREAKONTHROW_AIOB 2

namespace TR {

class X86CheckFailureSnippet : public TR::Snippet
   {
   TR::SymbolReference *_destination;
   TR::Instruction     *_checkInstruction;
   bool                _requiresFPstackPop;
   uint8_t             _breakOnThrowType;

   public:

   X86CheckFailureSnippet(
      TR::CodeGenerator   * cg,
      TR::SymbolReference * dest,
      TR::LabelSymbol      * lab,
      TR::Instruction     * checkInstruction,
      bool                 popFPstack = false,
      uint8_t              breakOnThrowType = 0)
      : TR::Snippet(cg, checkInstruction->getNode(), lab, dest->canCauseGC()),
        _destination(dest),
        _checkInstruction(checkInstruction),
        _requiresFPstackPop(popFPstack),
        _breakOnThrowType(breakOnThrowType)
      {
      // No registers preserved at this call
      //
      gcMap().setGCRegisterMask(0);
      checkBreakOnThrowOption();
      }

   virtual Kind getKind() { return IsCheckFailure; }

   TR::SymbolReference *getDestination()                      {return _destination;}
   TR::SymbolReference *setDestination(TR::SymbolReference *s) {return (_destination = s);}

   TR::Instruction *getCheckInstruction()                   {return _checkInstruction;}
   TR::Instruction *setCheckInstruction(TR::Instruction *ci) {return (_checkInstruction = ci);}

   bool getRequiredFPstackPop() { return _requiresFPstackPop; }

   virtual uint8_t *emitSnippetBody();
   uint8_t *emitCheckFailureSnippetBody(uint8_t *buffer);

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   void checkBreakOnThrowOption()
      {
      TR::Compilation *comp = TR::comp();
      TR::SimpleRegex *r = comp->getOptions()->getBreakOnThrow();
      if (r && (TR::SimpleRegex::matchIgnoringLocale(r, "java/lang/NullPointerException")
                   || TR::SimpleRegex::matchIgnoringLocale(r, "NPE", false)))
         {
         _breakOnThrowType|=TR_BREAKONTHROW_NPE;
         }
      if (r && (TR::SimpleRegex::matchIgnoringLocale(r, "java/lang/ArrayIndexOutOfBoundsException")
                  || TR::SimpleRegex::matchIgnoringLocale(r, "AIOB", false)))
         {
         _breakOnThrowType|=TR_BREAKONTHROW_AIOB;
         }
      }
   };


class X86BoundCheckWithSpineCheckSnippet : public TR::X86CheckFailureSnippet
   {
   TR::LabelSymbol *_restartLabel;

   public:

   X86BoundCheckWithSpineCheckSnippet(
      TR::CodeGenerator   *cg,
      TR::SymbolReference *bndchkSymRef,
      TR::LabelSymbol      *snippetLabel,
      TR::LabelSymbol      *restartLabel,
      TR::Instruction     *checkInstruction,
      bool                popFPstack = false) :
         TR::X86CheckFailureSnippet(
            cg,
            bndchkSymRef,
            snippetLabel,
            checkInstruction,
            popFPstack),
            _restartLabel(restartLabel)
            {
            }

   virtual Kind getKind() { return IsBoundCheckWithSpineCheck; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   };


// This is not the final shape of this snippet.  Its just here to
// make everything compile.
//
class X86SpineCheckSnippet : public TR::X86CheckFailureSnippet
   {
   TR::LabelSymbol *_restartLabel;

   public:

   X86SpineCheckSnippet(
      TR::CodeGenerator   *cg,
      TR::SymbolReference *bndchkSymRef,
      TR::LabelSymbol      *snippetLabel,
      TR::LabelSymbol      *restartLabel,
      TR::Instruction     *checkInstruction,
      bool                popFPstack = false) :
         TR::X86CheckFailureSnippet(
            cg,
            bndchkSymRef,
            snippetLabel,
            checkInstruction,
            popFPstack),
            _restartLabel(restartLabel)
            {
            }

   virtual Kind getKind() { return IsSpineCheck; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   };


class X86CheckFailureSnippetWithResolve : public TR::X86CheckFailureSnippet
   {
   TR::SymbolReference *_dataSymbolReference;
   flags8_t            _flags;
   TR_RuntimeHelper    _helper;
   uint8_t             _numLiveX87FPRs;

   enum
      {
      hasLiveXMMRs = 0x04
      };

   public:

   X86CheckFailureSnippetWithResolve(
      TR::CodeGenerator   *cg,
      TR::SymbolReference *dest,
      TR::SymbolReference *dataSymbolRef,
      TR_RuntimeHelper    resolverCall,
      TR::LabelSymbol      *snippetLabel,
      TR::Instruction     *checkInstruction,
      bool                popFPstack = false) :
         TR::X86CheckFailureSnippet(
            cg,
            dest,
            snippetLabel,
            checkInstruction),
            _dataSymbolReference(dataSymbolRef),
            _helper(resolverCall),
            _numLiveX87FPRs(0),
            _flags(0)
         {
         }

   virtual Kind getKind() { return IsCheckFailureWithResolve; }

   TR::Symbol *getDataSymbol() {return _dataSymbolReference->getSymbol();}

   TR::SymbolReference *getDataSymbolReference() {return _dataSymbolReference;}
   TR::SymbolReference *setDataSymbolReference(TR::SymbolReference *sr) {return (_dataSymbolReference = sr);}

   TR_RuntimeHelper getHelper() { return _helper; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   bool getHasLiveXMMRs()       {return _flags.testAny(hasLiveXMMRs);}
   void setHasLiveXMMRs()       {_flags.set(hasLiveXMMRs);}
   void resetHasLiveXMMRs()     {_flags.reset(hasLiveXMMRs);}
   void setHasLiveXMMRs(bool b) {_flags.set(hasLiveXMMRs, b);}

   uint8_t setNumLiveX87Registers(uint8_t l) {return (_numLiveX87FPRs = l);}
   uint8_t getNumLiveX87Registers()          {return _numLiveX87FPRs;}

   };

}

#endif
