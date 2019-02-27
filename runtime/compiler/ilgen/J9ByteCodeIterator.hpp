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

#ifndef J9BYTECODEITERATOR_INCL
#define J9BYTECODEITERATOR_INCL

#include "env/jittypes.h"
#include "il/Symbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/StaticSymbol.hpp"
#include "ilgen/ByteCodeIterator.hpp"
#include "infra/Link.hpp"
#include "env/j9method.h"
#include "env/VMJ9.h"
#include "ilgen/J9ByteCode.hpp"

class TR_J9ByteCodeIterator : public TR_ByteCodeIterator<TR_J9ByteCode, TR_ResolvedJ9Method>
   {
   typedef TR_ByteCodeIterator<TR_J9ByteCode, TR_ResolvedJ9Method> Base;

public:
   TR_ALLOC(TR_Memory::ByteCodeIterator);

   enum TR_ByteCodeFlags
      {
      SizeMask        = 0x07,
      Branch          = 0x08,
      ToHandler       = 0x10,
      Absolute        = 0x20,
      TwoByteOffset   = 0x40,
      FourByteOffset  = 0x80,

      TwoByteRelativeBranch = (Branch | TwoByteOffset),
      FourByteRelativeBranch = (Branch | FourByteOffset),
      TwoByteAbsoluteBranch = (Branch | Absolute | TwoByteOffset),
      FourByteAbsoluteBranch = (Branch | Absolute | FourByteOffset),
      TwoByteRelativeToHandler = (ToHandler | TwoByteOffset),
      FourByteRelativeToHandler = (ToHandler | FourByteOffset),
      TwoByteAbsoluteToHandler = (ToHandler | Absolute | TwoByteOffset),
      FourByteAbsoluteToHandler = (ToHandler | Absolute | FourByteOffset),

      };

   TR_J9ByteCodeIterator(TR::ResolvedMethodSymbol *methodSymbol, TR::Compilation *comp) 
      : Base(methodSymbol, static_cast<TR_ResolvedJ9Method*>(methodSymbol->getResolvedMethod()), comp) 
      {}

   TR_J9ByteCodeIterator(TR::ResolvedMethodSymbol *methodSymbol, TR_ResolvedJ9Method *method, TR_J9VMBase * fe, TR::Compilation * comp) :
      Base(methodSymbol, method, comp)
      {
      initialize(method, fe);
      }

   void initialize(TR_ResolvedJ9Method *, TR_J9VMBase *);

   int32_t defaultTargetIndex() { return (int32_t)(_bcIndex + 1 + ((4 - ((intptrj_t)&_code[_bcIndex+1] & 3)) & 3)); }
   
   bool isThisChanged();

   int32_t findFloatingPointInstruction();

   int32_t nextSwitchValue(int32_t & bcIndex);

   static int32_t size(TR_J9ByteCode bc)
      {
      return _byteCodeFlags[bc] & SizeMask;
      }

   TR_J9ByteCode first()
      {
      _bcIndex = 0;
      return current();
      }

   TR_J9ByteCode next()
      {
      int32_t size = _byteCodeFlags[_bc] & SizeMask;
      if (size)
         _bcIndex += size;
      else
         stepOverVariableSizeBC();
      return current();
      }

   int32_t currentByteCodeIndex()
      {
      return _bcIndex;
      }

   TR_J9ByteCode current()
      {
      _bc = (_bcIndex >= _maxByteCodeIndex ? J9BCunknown : convertOpCodeToByteCodeEnum(_code[_bcIndex]));
      TR_ASSERT_FATAL(_bcIndex >= _maxByteCodeIndex || _bc != J9BCunknown, "Unknown bytecode to JIT %d \n", _code[_bcIndex]);
      return _bc;
      }

   static TR_J9ByteCode convertOpCodeToByteCodeEnum(uint8_t opcode)
      {
      return _opCodeToByteCodeEnum[opcode];
      }
   uint32_t estimatedCodeSize()                   { return estimatedCodeSize(current()); }
   static uint32_t estimatedCodeSize(TR_J9ByteCode bc) { return _estimatedCodeSize[bc]; }


   bool     isBranch()                            { return (_byteCodeFlags[_bc] & Branch) != 0; }
   int32_t  relativeBranch()                      { TR_ASSERT((_byteCodeFlags[_bc] & Absolute) == 0, "assertion failure"); return (_byteCodeFlags[_bc] & TwoByteOffset) ? next2BytesSigned() : next4BytesSigned(); }
   int32_t  absoluteBranch()                      { TR_ASSERT((_byteCodeFlags[_bc] & Absolute) != 0, "assertion failure"); return (_byteCodeFlags[_bc] & TwoByteOffset) ? next2Bytes() : next4Bytes(); }
   int32_t  branchDestination(int32_t base)       { TR_ASSERT((_byteCodeFlags[_bc] & Branch) != 0, "assertion failure"); return ((_byteCodeFlags[_bc] & Absolute) != 0) ? absoluteBranch() : base + relativeBranch(); }

   uint8_t  nextByte(int32_t o = 1)               { return _code[_bcIndex + o]; }
   int8_t   nextByteSigned(int32_t o = 1)         { return nextByte(o); }

#if defined(NETWORK_ORDER_BYTECODE)
   uint16_t next2Bytes(int32_t o = 1)             { return nextByte(o) << 8 | nextByte(o + 1); }
   uint32_t next4Bytes(int32_t o = 1)             { return nextByte(o) << 24 | nextByte(o + 1) << 16 |
                                                                         nextByte(o + 2) << 8 |
                                                                         nextByte(o + 3); }
   int16_t  next2BytesSigned(int32_t o = 1)       { return next2Bytes(o); }
   int32_t  next4BytesSigned(int32_t o = 1)       { return next4Bytes(o); }
#else 
   uint16_t next2Bytes(int32_t o = 1)             { return *((uint16_t*)&_code[_bcIndex + o]); }
   uint32_t next4Bytes(int32_t o = 1)             { return *((uint32_t*)&_code[_bcIndex + o]); }
   int16_t  next2BytesSigned(int32_t o = 1)       { return next2Bytes(o); }
   int32_t  next4BytesSigned(int32_t o = 1)       { return next4Bytes(o); }
#endif

protected:
   void stepOverVariableSizeBC();

   void printFirst(int32_t i);
   void printCPIndex(int32_t i);
   void printConstant(int32_t i);
   void printConstant(double d);
   void printFirstAndConstant(int32_t i, int32_t j);
   void printJumpIndex(int32_t offset);

   void printByteCodePrologue();
   void printByteCode();
   void printByteCodeEpilogue();

   TR_J9VMBase *fe()             { return _fe; }
   //TR_ResolvedJ9Method *method() { return _method; }
   //TR_ResolvedJ9Method  *   _method;
   TR_J9VMBase          *   _fe;
   const uint8_t        *   _code;
   TR_J9ByteCode            _bc; // TODO: remove this field, Replace uses with call to current()

   static const uint8_t       _byteCodeFlags[];
   static const TR_J9ByteCode _opCodeToByteCodeEnum[256];
   static const uint8_t       _estimatedCodeSize[];

   static TR::ILOpCodes    _lcmpOps[];
   static TR::ILOpCodes    _fcmplOps[];
   static TR::ILOpCodes    _fcmpgOps[];
   static TR::ILOpCodes    _dcmplOps[];
   static TR::ILOpCodes    _dcmpgOps[];
   };

#endif

