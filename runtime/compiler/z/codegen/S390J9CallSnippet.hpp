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

#ifndef TR_S390J9CALLSNIPPET_INCL
#define TR_S390J9CALLSNIPPET_INCL

#include "z/codegen/CallSnippet.hpp"
#include "z/codegen/ConstantDataSnippet.hpp"
#include "z/codegen/S390Instruction.hpp"

class TR_J2IThunk;
namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

namespace TR {

class S390J9CallSnippet : public TR::S390CallSnippet
   {
   public:

   S390J9CallSnippet(
         TR::CodeGenerator *cg,
         TR::Node *n,
         TR::LabelSymbol *lab,
         int32_t s) :
      TR::S390CallSnippet(cg, n, lab, s)
      {}

   S390J9CallSnippet(
         TR::CodeGenerator *cg,
         TR::Node *n,
         TR::LabelSymbol *lab,
         TR::SymbolReference *symRef,
         int32_t s) :
      TR::S390CallSnippet(cg, n, lab, symRef, s) {}


   static uint8_t *generateVIThunk(TR::Node *callNode, int32_t argSize, TR::CodeGenerator *cg);
   static TR_J2IThunk *generateInvokeExactJ2IThunk(TR::Node *callNode, int32_t argSize, char* signature, TR::CodeGenerator *cg);
   virtual uint8_t *emitSnippetBody();
   };


class S390UnresolvedCallSnippet : public TR::S390J9CallSnippet
   {

   public:

   S390UnresolvedCallSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s)
      : TR::S390J9CallSnippet(cg, c, lab, s)
      {
      }

   virtual Kind getKind() { return IsUnresolvedCall; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };


class S390VirtualSnippet : public TR::S390J9CallSnippet
   {
   public:

   S390VirtualSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s)
      : TR::S390J9CallSnippet(cg, c, lab, s)
      {
      }

   virtual Kind getKind() { return IsVirtual; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };


class S390VirtualUnresolvedSnippet : public TR::S390VirtualSnippet
   {
   void *thunkAddress;

   public:

   S390VirtualUnresolvedSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s)
      : TR::S390VirtualSnippet(cg, c, lab, s), thunkAddress(NULL)
      {
      }

   S390VirtualUnresolvedSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s, void *thunkPtr)
      : TR::S390VirtualSnippet(cg, c, lab, s), thunkAddress(thunkPtr)
      {
      }

   virtual Kind getKind() { return IsVirtualUnresolved; }

   virtual uint8_t *emitSnippetBody();

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   void* getJ2IThunkAddress() { return thunkAddress; }

   TR::Instruction *patchVftInstruction;
   TR::Instruction *indirectCallInstruction;    // the BASR in the virtual dispatch sequence

   TR::Instruction *setPatchVftInstruction(TR::Instruction *i) {return patchVftInstruction=i;}
   TR::Instruction *getPatchVftInstruction() {return patchVftInstruction;}

   TR::Instruction *setIndirectCallInstruction(TR::Instruction *i) {return indirectCallInstruction = i;}
   TR::Instruction *getIndirectCallInstruction() {return indirectCallInstruction;}
   };

class J9S390InterfaceCallDataSnippet : public TR::S390ConstantDataSnippet
   {
   TR::Instruction * _firstCLFI;
   uint8_t _numInterfaceCallCacheSlots;
   uint8_t* _codeRA;
   void *_thunkAddress;
   bool _useCLFIandBRCL;

   public:
   J9S390InterfaceCallDataSnippet(TR::CodeGenerator *,
                                  TR::Node *,
                                  uint8_t,
                                  void *,
                                  bool useCLFIandBRCL = false);

   virtual Kind getKind() { return IsInterfaceCallData; }
   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   int8_t getNumInterfaceCallCacheSlots() {return _numInterfaceCallCacheSlots;}

   void setUseCLFIandBRCL(bool useCLFIandBRCL) {_useCLFIandBRCL = useCLFIandBRCL;}
   bool isUseCLFIandBRCL() {return _useCLFIandBRCL;}

   void setFirstCLFI(TR::Instruction* firstCLFI) { _firstCLFI = firstCLFI; }
   TR::Instruction* getFirstCLFI() { return _firstCLFI;}

   uint8_t* getCodeRA() { return _codeRA;}
   uint8_t* setCodeRA(uint8_t *codeRA)
      {
      return _codeRA = codeRA;
      }

   virtual uint32_t getCallReturnAddressOffset();
   virtual uint32_t getSingleDynamicSlotOffset();
   virtual uint32_t getLastCachedSlotFieldOffset();
   virtual uint32_t getFirstSlotFieldOffset();
   virtual uint32_t getLastSlotFieldOffset();
   virtual uint32_t getFirstSlotOffset();
   virtual uint32_t getLastSlotOffset();
   };


class S390InterfaceCallSnippet : public TR::S390VirtualSnippet
   {
   TR::J9S390InterfaceCallDataSnippet * _dataSnippet;
   int8_t _numInterfaceCallCacheSlots;
   bool _useCLFIandBRCL;

   public:

   S390InterfaceCallSnippet(TR::CodeGenerator *cg, TR::Node *c, TR::LabelSymbol *lab, int32_t s, int8_t n, void *thunkPtr, bool useCLFIandBRCL = false);

   virtual Kind getKind() { return IsInterfaceCall; }
   int8_t getNumInterfaceCallCacheSlots() {return _numInterfaceCallCacheSlots;}
   void setUseCLFIandBRCL(bool useCLFIandBRCL) {
      _useCLFIandBRCL = useCLFIandBRCL;
      if (getDataConstantSnippet() != NULL)
         {
         getDataConstantSnippet()->setUseCLFIandBRCL(useCLFIandBRCL);
         }
      }
   bool isUseCLFIandBRCL() {return _useCLFIandBRCL;}

   TR::J9S390InterfaceCallDataSnippet *getDataConstantSnippet() { return _dataSnippet; }
   TR::J9S390InterfaceCallDataSnippet *setDataConstantSnippet(TR::J9S390InterfaceCallDataSnippet *snippet)
      {
      return _dataSnippet = snippet;
      }

   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual uint8_t *emitSnippetBody();

   };
}

#endif
