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

#ifndef J9_AMD64_CALLSNIPPET_INCL
#define J9_AMD64_CALLSNIPPET_INCL

#include "codegen/Snippet.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR
{

class X86ResolveVirtualDispatchReadOnlyDataSnippet : public TR::Snippet
   {
private:

   TR::LabelSymbol *_loadResolvedVtableOffsetLabel;

   TR::LabelSymbol *_doneLabel;

   intptr_t _resolveVirtualDataAddress;

   TR::Node *_callNode;

public:

   X86ResolveVirtualDispatchReadOnlyDataSnippet(
      TR::LabelSymbol *resolveVirtualDispatchReadOnlyDataSnippetLabel,
      TR::Node *callNode,
      intptr_t resolveVirtualDataAddress,
      TR::LabelSymbol *loadResolvedVtableOffsetLabel,
      TR::LabelSymbol *doneLabel,
      TR::CodeGenerator *cg) :
         TR::Snippet(cg, NULL, resolveVirtualDispatchReadOnlyDataSnippetLabel, true),
         _resolveVirtualDataAddress(resolveVirtualDataAddress),
         _callNode(callNode),
         _loadResolvedVtableOffsetLabel(loadResolvedVtableOffsetLabel),
         _doneLabel(doneLabel)
      {}

   virtual Kind getKind() { return IsResolveVirtualDispatchReadOnlyData; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   };



class X86CallReadOnlySnippet : public TR::Snippet
   {
public:

   X86CallReadOnlySnippet(TR::CodeGenerator *cg, TR::Node * n, TR::LabelSymbol * lab, bool isGCSafePoint)
      : TR::Snippet(cg, n, lab, isGCSafePoint)
      {
      _realMethodSymbolReference = 0;
      _unresolvedStaticSpecialDataAddress = 0;
      _interpretedStaticSpecialDataAddress = 0;
      _ramMethodDataAddress = 0;
      }

   X86CallReadOnlySnippet(TR::CodeGenerator *cg, TR::Node * n, TR::LabelSymbol * lab, TR::SymbolReference* realSymRef, bool isGCSafePoint)
      : TR::Snippet(cg, n, lab, isGCSafePoint)
      {
      _realMethodSymbolReference = realSymRef;
      _unresolvedStaticSpecialDataAddress = 0;
      _interpretedStaticSpecialDataAddress = 0;
      _ramMethodDataAddress = 0;
      }

   virtual Kind getKind() { return IsCallReadOnly; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   void setUnresolvedStaticSpecialDataAddress(intptr_t a) { _unresolvedStaticSpecialDataAddress = a; }
   void setInterpretedStaticSpecialDataAddress(intptr_t a) { _interpretedStaticSpecialDataAddress = a; }
   void setRAMMethodDataAddress(intptr_t a) { _ramMethodDataAddress = a; }

   TR::SymbolReference *getRealMethodSymbolReference() { return _realMethodSymbolReference; }

private:

   TR::SymbolReference * _realMethodSymbolReference;

   intptr_t _unresolvedStaticSpecialDataAddress;

   intptr_t _interpretedStaticSpecialDataAddress;

   intptr_t _ramMethodDataAddress;

   };


class X86InterfaceDispatchReadOnlySnippet : public TR::Snippet
   {
private:

   intptr_t _interfaceDispatchDataAddress;

   TR::LabelSymbol *_slotRestartLabel;

   TR::LabelSymbol *_doneLabel;

   TR::Node *_callNode;

public:

   X86InterfaceDispatchReadOnlySnippet(
      TR::LabelSymbol *interfaceDispatchSnippetLabel,
      TR::Node *callNode,
      intptr_t interfaceDispatchDataAddress,
      TR::LabelSymbol *slotRestartLabel,
      TR::LabelSymbol *doneLabel,
      TR::CodeGenerator *cg) :
         TR::Snippet(cg, NULL, interfaceDispatchSnippetLabel, true),
         _interfaceDispatchDataAddress(interfaceDispatchDataAddress),
         _slotRestartLabel(slotRestartLabel),
         _callNode(callNode),
         _doneLabel(doneLabel)
      {}

   virtual Kind getKind() { return IsInterfaceDispatchReadOnly; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   };
   
}

#endif
