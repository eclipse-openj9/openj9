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

#include "ilgen/J9ByteCodeIterator.hpp"
#include "compile/Compilation.hpp"
#include "env/VMJ9.h"
#include "ras/Debug.hpp"
#include "env/IO.hpp"

void
TR_J9ByteCodeIterator::initialize(TR_ResolvedJ9Method * method, TR_J9VMBase * fe)
   {
   _fe = fe;
   _code = method->bytecodeStart();
   _bcIndex = -1;
   }

int32_t
TR_J9ByteCodeIterator::nextSwitchValue(int32_t & bcIndex)
   {
   int32_t value = *(int32_t *)&_code[bcIndex];
   bcIndex += 4;
   return value;
   }

void
TR_J9ByteCodeIterator::stepOverVariableSizeBC()
   {
   if (_bc == J9BCwide)
      convertOpCodeToByteCodeEnum(++_bcIndex) == J9BCiinc ? _bcIndex += 5 : _bcIndex += 3;
   else if (_bc == J9BClookupswitch)
      {
      _bcIndex = defaultTargetIndex() + 4;
      int32_t tableSize = nextSwitchValue(_bcIndex);
      _bcIndex += (8 * tableSize);
      }
   else
      {
      TR_ASSERT(_bc == J9BCtableswitch, "invalid 0 size for a byte code");
      _bcIndex = defaultTargetIndex() + 4;
      int32_t low = nextSwitchValue(_bcIndex);
      int32_t high = nextSwitchValue(_bcIndex);
      _bcIndex += (4 * (high - low + 1));
      }
   }

bool
TR_J9ByteCodeIterator::isThisChanged()
   {
   for (TR_J9ByteCode bc = first(); bc != J9BCunknown; bc = next())
      {
      switch (bc)
         {
         case J9BCastore0:
            return true;
         case J9BCastore:
            if (nextByte() == 0)
               return true;
         default:
            //nothing to do here
            break;
         }
      }
   return false;
   }

int32_t
TR_J9ByteCodeIterator::findFloatingPointInstruction()
   {
   bool isVolatile, isPrivate;
   TR::DataType type = TR::NoType;
   uint32_t offset;
   void * staticAddress;
   for (TR_J9ByteCode bc = first(); bc != J9BCunknown; bc = next())
      switch (bc)
         {
         case J9BCfconst0: case J9BCfconst1: case J9BCfconst2:
         case J9BCdconst0: case J9BCdconst1:
         case J9BCldc2dw:
         case J9BCfload: case J9BCdload:
         case J9BCfload0: case J9BCfload1: case J9BCfload2: case J9BCfload3:
         case J9BCdload0: case J9BCdload1: case J9BCdload2: case J9BCdload3:
         case J9BCfaload: case J9BCdaload:
         case J9BCfloadw: case J9BCdloadw:
         case J9BCfstore: case J9BCdstore:
         case J9BCfstorew: case J9BCdstorew:
         case J9BCfstore0: case J9BCfstore1: case J9BCfstore2: case J9BCfstore3:
         case J9BCdstore0: case J9BCdstore1: case J9BCdstore2: case J9BCdstore3:
         case J9BCfastore: case J9BCdastore:
         case J9BCfadd: case J9BCdadd:
         case J9BCfsub: case J9BCdsub:
         case J9BCfmul: case J9BCdmul:
         case J9BCfdiv: case J9BCddiv:
         case J9BCfrem: case J9BCdrem:
         case J9BCfneg: case J9BCdneg:
         case J9BCi2f: case J9BCi2d:
         case J9BCl2f: case J9BCl2d: case J9BCf2i: case J9BCf2l: case J9BCf2d:
         case J9BCd2i: case J9BCd2l: case J9BCd2f:
         case J9BCfcmpl: case J9BCfcmpg: case J9BCdcmpl: case J9BCdcmpg:
            return bcIndex();
         case J9BCldc:
            if (method()->getLDCType(nextByte()) == TR::Float)
               return bcIndex();
            break;
         case J9BCldcw:
            if (method()->getLDCType(next2Bytes()) == TR::Float)
               return bcIndex();
            break;
         case J9BCgetfield: case J9BCputfield:
            {
            method()->fieldAttributes(_compilation, next2Bytes(), &offset, &type, &isVolatile, NULL, &isPrivate, (bc == J9BCputfield), NULL, false);
            if (type == TR::Float || type == TR::Double)
               return bcIndex();
            break;
            }
         case J9BCgetstatic: case J9BCputstatic:
            method()->staticAttributes(_compilation, next2Bytes(), &staticAddress, &type, &isVolatile, NULL, &isPrivate, (bc == J9BCputstatic), NULL, false);
            if (type == TR::Float || type == TR::Double)
               return bcIndex();
            break;
         case J9BCinvokestatic:  case J9BCinvokevirtual: case J9BCinvokespecial: case J9BCinvokeinterface:
         case J9BCinvokedynamic: case J9BCinvokehandle:  case J9BCinvokehandlegeneric:
         case J9BCinvokestaticsplit: case J9BCinvokespecialsplit:
             {
            int32_t index = next2Bytes();
            if (bc == J9BCinvokestaticsplit)
               index = index | J9_STATIC_SPLIT_TABLE_INDEX_FLAG;
            if (bc == J9BCinvokespecialsplit)
               index = index | J9_SPECIAL_SPLIT_TABLE_INDEX_FLAG;

            TR_J9VMBase *fej9 = (TR_J9VMBase *)_fe;
            TR_Method *thisMethod = fej9->createMethod(_trMemory, method()->containingClass(), index);

            // check return type
            type = thisMethod->returnType();
            if (type == TR::Float || type == TR::Double)
               return bcIndex();

            // check parameter types - in case an arg is only ldc'ed
            int32_t argNum, elems = thisMethod->numberOfExplicitParameters();
            for(argNum=0;argNum<elems;argNum++)
               {
               TR::DataType type = thisMethod->parmType(argNum);
               if (type == TR::Float || type == TR::Double)
                  return bcIndex();
               }
            break;
            }
         default:
         	break;
         }
   return -1;
   }

void
TR_J9ByteCodeIterator::printByteCodePrologue()
   {
   trfprintf(comp()->getOutFile(), "\n"
      "        +------------- Byte Code Index\n"
      "        |  +-------------------- OpCode\n"
      "        |  |                        +------------- First Field\n"
      "        |  |                        |     +------------- Branch Target\n"
      "        |  |                        |     |      +------- Const Pool Index\n"
      "        |  |                        |     |      |    +------------- Constant\n"
      "        |  |                        |     |      |    |\n"
      "        V  V                        V     V      V    V\n");
   }

void
TR_J9ByteCodeIterator::printByteCodeEpilogue()
   {
   trfprintf(comp()->getOutFile(), "\n\n"); comp()->getDebug()->printByteCodeAnnotations();
   }

void
TR_J9ByteCodeIterator::printFirst(int32_t i)
   {
   trfprintf(comp()->getOutFile(), "%5i", i);
   }

void
TR_J9ByteCodeIterator::printCPIndex(int32_t i)
   {
   trfprintf(comp()->getOutFile(), "%13s%5i", "", i);
   }

void
TR_J9ByteCodeIterator::printConstant(int32_t i)
   {
   trfprintf(comp()->getOutFile(), "%11s%12i  ", "", i);
   }

void
TR_J9ByteCodeIterator::printConstant(double d)
   {
   trfprintf(comp()->getOutFile(), "%11s%12e  ", "", d);
   }

void
TR_J9ByteCodeIterator::printFirstAndConstant(int32_t i, int32_t j)
   {
   trfprintf(comp()->getOutFile(), "%5i%6s%12i  ", i, "", j);
   }

void
TR_J9ByteCodeIterator::printJumpIndex(int32_t offset)
   {
   trfprintf(comp()->getOutFile(), "%5i,%5d,%11s ", offset, offset + bcIndex(), "");
   }

void
TR_J9ByteCodeIterator::printByteCode()
   {
   uint8_t opcode = nextByte(0);

   trfprintf(comp()->getOutFile(), "\n   %6i, %-15s      ", bcIndex(), ((TR_J9VMBase *)fe())->getByteCodeName(opcode));

   TR_J9ByteCode bc = convertOpCodeToByteCodeEnum(opcode);
   switch (bc)
      {
      case J9BCbipush:
         printConstant(nextByteSigned());
         break;

      case J9BCsipush:
         printConstant(next2BytesSigned());
         break;

      case J9BCiload: case J9BClload: case J9BCfload: case J9BCdload: case J9BCaload:
      case J9BCistore: case J9BClstore: case J9BCfstore: case J9BCdstore: case J9BCastore:
         printFirst(nextByte());
         break;

      case J9BCiinc:
         printFirstAndConstant(nextByte(), nextByteSigned(2));
         break;

      case J9BCinvokevirtual:
      case J9BCinvokespecial:
      case J9BCinvokestatic:
      case J9BCinvokeinterface:
      case J9BCinvokedynamic: // Could eventually need next3bytes
      case J9BCinvokehandle:
      case J9BCinvokehandlegeneric:
      case J9BCinvokespecialsplit:
      case J9BCinvokestaticsplit:
         printFirst(next2Bytes());
         break;

      case J9BCgetstatic: case J9BCgetfield: case J9BCputstatic: case J9BCputfield:
      case J9BCcheckcast: case J9BCinstanceof:
      case J9BCnew: case J9BCanewarray:
         printCPIndex(next2Bytes());
         break;

      case J9BCnewarray:
         printCPIndex(nextByte());
         break;

      case J9BCmultianewarray:
         printCPIndex(next2Bytes());
         printConstant(nextByte(3));
         break;

      case J9BCifeq: case J9BCifne: case J9BCiflt: case J9BCifge: case J9BCifgt: case J9BCifle: case J9BCifnull: case J9BCifnonnull:
      case J9BCificmpeq: case J9BCificmpne: case J9BCificmplt: case J9BCificmpge: case J9BCificmpgt: case J9BCificmple: case J9BCifacmpeq: case J9BCifacmpne:
      case J9BCgoto:
         printJumpIndex(next2BytesSigned());
         break;

      case J9BCgotow:
         printJumpIndex(next4BytesSigned());
         break;
      default:
      	break;
      }
   }

const TR_J9ByteCode TR_J9ByteCodeIterator::_opCodeToByteCodeEnum[] =
   {
   /* 0   */ J9BCnop,
   /* 1   */ J9BCaconstnull, J9BCiconstm1,
   /* 3   */ J9BCiconst0, J9BCiconst1, J9BCiconst2, J9BCiconst3, J9BCiconst4, J9BCiconst5,
   /* 9   */ J9BClconst0, J9BClconst1,
   /* 11  */ J9BCfconst0, J9BCfconst1, J9BCfconst2,
   /* 14  */ J9BCdconst0, J9BCdconst1,
   /* 16  */ J9BCbipush, J9BCsipush,
   /* 18  */ J9BCldc, J9BCldcw, J9BCldc2lw,
   /* 21  */ J9BCiload, J9BClload, J9BCfload, J9BCdload, J9BCaload,
   /* 26  */ J9BCiload0, J9BCiload1, J9BCiload2, J9BCiload3,
   /* 30  */ J9BClload0, J9BClload1, J9BClload2, J9BClload3,
   /* 34  */ J9BCfload0, J9BCfload1, J9BCfload2, J9BCfload3,
   /* 38  */ J9BCdload0, J9BCdload1, J9BCdload2, J9BCdload3,
   /* 42  */ J9BCaload0, J9BCaload1, J9BCaload2, J9BCaload3,
   /* 46  */ J9BCiaload, J9BClaload, J9BCfaload, J9BCdaload, J9BCaaload, J9BCbaload, J9BCcaload, J9BCsaload,
   /* 54  */ J9BCistore, J9BClstore, J9BCfstore, J9BCdstore, J9BCastore,
   /* 59  */ J9BCistore0, J9BCistore1, J9BCistore2, J9BCistore3,
   /* 63  */ J9BClstore0, J9BClstore1, J9BClstore2, J9BClstore3,
   /* 67  */ J9BCfstore0, J9BCfstore1, J9BCfstore2, J9BCfstore3,
   /* 71  */ J9BCdstore0, J9BCdstore1, J9BCdstore2, J9BCdstore3,
   /* 75  */ J9BCastore0, J9BCastore1, J9BCastore2, J9BCastore3,
   /* 79  */ J9BCiastore, J9BClastore, J9BCfastore, J9BCdastore, J9BCaastore, J9BCbastore, J9BCcastore, J9BCsastore,
   /* 87  */ J9BCpop, J9BCpop2,
   /* 89  */ J9BCdup, J9BCdupx1, J9BCdupx2, J9BCdup2, J9BCdup2x1, J9BCdup2x2,
   /* 95  */ J9BCswap,
   /* 96  */ J9BCiadd, J9BCladd, J9BCfadd, J9BCdadd,
   /* 100 */ J9BCisub, J9BClsub, J9BCfsub, J9BCdsub,
   /* 104 */ J9BCimul, J9BClmul, J9BCfmul, J9BCdmul,
   /* 108 */ J9BCidiv, J9BCldiv, J9BCfdiv, J9BCddiv,
   /* 112 */ J9BCirem, J9BClrem, J9BCfrem, J9BCdrem,
   /* 116 */ J9BCineg, J9BClneg, J9BCfneg, J9BCdneg,
   /* 120 */ J9BCishl, J9BClshl, J9BCishr, J9BClshr, J9BCiushr, J9BClushr,
   /* 126 */ J9BCiand, J9BCland,
   /* 128 */ J9BCior, J9BClor, J9BCixor, J9BClxor,
   /* 132 */ J9BCiinc,
   /* 133 */ J9BCi2l, J9BCi2f, J9BCi2d,
   /* 136 */ J9BCl2i, J9BCl2f, J9BCl2d,
   /* 139 */ J9BCf2i, J9BCf2l, J9BCf2d,
   /* 142 */ J9BCd2i, J9BCd2l, J9BCd2f,
   /* 145 */ J9BCi2b, J9BCi2c, J9BCi2s,
   /* 148 */ J9BClcmp, J9BCfcmpl, J9BCfcmpg, J9BCdcmpl, J9BCdcmpg,
   /* 153 */ J9BCifeq, J9BCifne, J9BCiflt, J9BCifge, J9BCifgt, J9BCifle,
   /* 159 */ J9BCificmpeq, J9BCificmpne, J9BCificmplt, J9BCificmpge, J9BCificmpgt, J9BCificmple, J9BCifacmpeq, J9BCifacmpne,
   /* 167 */ J9BCgoto, J9BCunknown, J9BCunknown,
   /* 170 */ J9BCtableswitch, J9BClookupswitch,
   /* 172 */ J9BCgenericReturn, J9BCgenericReturn, J9BCgenericReturn,
   /* 175 */ J9BCgenericReturn, J9BCgenericReturn, J9BCgenericReturn,
   /* 178 */ J9BCgetstatic, J9BCputstatic, J9BCgetfield, J9BCputfield,
   /* 182 */ J9BCinvokevirtual, J9BCinvokespecial, J9BCinvokestatic, J9BCinvokeinterface, J9BCinvokedynamic,
   /* 187 */ J9BCnew, J9BCnewarray, J9BCanewarray,
   /* 190 */ J9BCarraylength,
   /* 191 */ J9BCathrow,
   /* 192 */ J9BCcheckcast,
   /* 193 */ J9BCinstanceof,
   /* 194 */ J9BCmonitorenter, J9BCmonitorexit,
   /* 196 */ J9BCunknown,
   /* 197 */ J9BCmultianewarray,
   /* 198 */ J9BCifnull, J9BCifnonnull,
   /* 200 */ J9BCgotow, J9BCunknown,
   /* 202 */ J9BCbreakpoint,

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
   /* 203 */ J9BCdefaultvalue,
   /* 204 */ J9BCwithfield,
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
   /* 203 */ J9BCunknown,
   /* 204 */ J9BCunknown,
#endif
   /* 205 */ J9BCunknown, J9BCunknown, J9BCunknown, J9BCunknown,
   /* 209 */ J9BCunknown, J9BCunknown, J9BCunknown, J9BCunknown,
   /* 213 */ J9BCiincw, J9BCunknown,
   /* 215 - JBaload0getfield */ J9BCaload0,
   /* 216 - JBnewdup */ J9BCnew,
   /* 217 */ J9BCiloadw, J9BClloadw, J9BCfloadw, J9BCdloadw, J9BCaloadw,
   /* 222 */ J9BCistorew, J9BClstorew, J9BCfstorew, J9BCdstorew, J9BCastorew,
   /* 227 */ J9BCunknown,
   /* 228 */ J9BCgenericReturn, J9BCgenericReturn, J9BCunknown, J9BCinvokeinterface2,
   /* 232 */ J9BCinvokehandle, J9BCinvokehandlegeneric,
   /* 234 */ J9BCinvokestaticsplit, J9BCinvokespecialsplit,
   /* 236 */ J9BCReturnC, J9BCReturnS, J9BCReturnB, J9BCReturnZ,
   /* 240 */ J9BCunknown, J9BCunknown, J9BCunknown, J9BCunknown,
   /* 244 */ J9BCunknown, J9BCunknown, J9BCunknown, J9BCunknown, J9BCunknown,
   /* 249 */ J9BCldc2dw,
   /* 250 */ J9BCunknown, J9BCunknown, J9BCunknown, J9BCunknown, J9BCunknown, J9BCunknown
   };

const uint8_t TR_J9ByteCodeIterator::_byteCodeFlags[] =
   {
// FLAGS                  |  SIZE,
                             0x01, // J9BCnop
                             0x01, // J9BCaconstnull
                             0x01, // J9BCiconstm1
                             0x01, // J9BCiconst0
                             0x01, // J9BCiconst1
                             0x01, // J9BCiconst2
                             0x01, // J9BCiconst3
                             0x01, // J9BCiconst4
                             0x01, // J9BCiconst5
                             0x01, // J9BClconst0
                             0x01, // J9BClconst1
                             0x01, // J9BCfconst0
                             0x01, // J9BCfconst1
                             0x01, // J9BCfconst2
                             0x01, // J9BCdconst0
                             0x01, // J9BCdconst1
                             0x02, // J9BCbipush
                             0x03, // J9BCsipush
                             0x02, // J9BCldc
                             0x03, // J9BCldcw
                             0x03, // J9BCldc2lw
                             0x03, // J9BCldc2dw
                             0x02, // J9BCiload
                             0x02, // J9BClload
                             0x02, // J9BCfload
                             0x02, // J9BCdload
                             0x02, // J9BCaload
                             0x01, // J9BCiload0
                             0x01, // J9BCiload1
                             0x01, // J9BCiload2
                             0x01, // J9BCiload3
                             0x01, // J9BClload0
                             0x01, // J9BClload1
                             0x01, // J9BClload2
                             0x01, // J9BClload3
                             0x01, // J9BCfload0
                             0x01, // J9BCfload1
                             0x01, // J9BCfload2
                             0x01, // J9BCfload3
                             0x01, // J9BCdload0
                             0x01, // J9BCdload1
                             0x01, // J9BCdload2
                             0x01, // J9BCdload3
                             0x01, // J9BCaload0
                             0x01, // J9BCaload1
                             0x01, // J9BCaload2
                             0x01, // J9BCaload3
                             0x01, // J9BCiaload
                             0x01, // J9BClaload
                             0x01, // J9BCfaload
                             0x01, // J9BCdaload
                             0x01, // J9BCaaload
                             0x01, // J9BCbaload
                             0x01, // J9BCcaload
                             0x01, // J9BCsaload
                             0x03, // J9BCiloadw
                             0x03, // J9BClloadw
                             0x03, // J9BCfloadw
                             0x03, // J9BCdloadw
                             0x03, // J9BCaloadw
                             0x02, // J9BCistore
                             0x02, // J9BClstore
                             0x02, // J9BCfstore
                             0x02, // J9BCdstore
                             0x02, // J9BCastore
                             0x03, // J9BCistorew
                             0x03, // J9BClstorew
                             0x03, // J9BCfstorew
                             0x03, // J9BCdstorew
                             0x03, // J9BCastorew
                             0x01, // J9BCistore0
                             0x01, // J9BCistore1
                             0x01, // J9BCistore2
                             0x01, // J9BCistore3
                             0x01, // J9BClstore0
                             0x01, // J9BClstore1
                             0x01, // J9BClstore2
                             0x01, // J9BClstore3
                             0x01, // J9BCfstore0
                             0x01, // J9BCfstore1
                             0x01, // J9BCfstore2
                             0x01, // J9BCfstore3
                             0x01, // J9BCdstore0
                             0x01, // J9BCdstore1
                             0x01, // J9BCdstore2
                             0x01, // J9BCdstore3
                             0x01, // J9BCastore0
                             0x01, // J9BCastore1
                             0x01, // J9BCastore2
                             0x01, // J9BCastore3
                             0x01, // J9BCiastore
                             0x01, // J9BClastore
                             0x01, // J9BCfastore
                             0x01, // J9BCdastore
                             0x01, // J9BCaastore
                             0x01, // J9BCbastore
                             0x01, // J9BCcastore
                             0x01, // J9BCsastore
                             0x01, // J9BCpop
                             0x01, // J9BCpop2
                             0x01, // J9BCdup
                             0x01, // J9BCdupx1
                             0x01, // J9BCdupx2
                             0x01, // J9BCdup2
                             0x01, // J9BCdup2x1
                             0x01, // J9BCdup2x2
                             0x01, // J9BCswap
                             0x01, // J9BCiadd
                             0x01, // J9BCladd
                             0x01, // J9BCfadd
                             0x01, // J9BCdadd
                             0x01, // J9BCisub
                             0x01, // J9BClsub
                             0x01, // J9BCfsub
                             0x01, // J9BCdsub
                             0x01, // J9BCimul
                             0x01, // J9BClmul
                             0x01, // J9BCfmul
                             0x01, // J9BCdmul
                             0x01, // J9BCidiv
                             0x01, // J9BCldiv
                             0x01, // J9BCfdiv
                             0x01, // J9BCddiv
                             0x01, // J9BCirem
                             0x01, // J9BClrem
                             0x01, // J9BCfrem
                             0x01, // J9BCdrem
                             0x01, // J9BCineg
                             0x01, // J9BClneg
                             0x01, // J9BCfneg
                             0x01, // J9BCdneg
                             0x01, // J9BCishl
                             0x01, // J9BClshl
                             0x01, // J9BCishr
                             0x01, // J9BClshr
                             0x01, // J9BCiushr
                             0x01, // J9BClushr
                             0x01, // J9BCiand
                             0x01, // J9BCland
                             0x01, // J9BCior
                             0x01, // J9BClor
                             0x01, // J9BCixor
                             0x01, // J9BClxor
                             0x03, // J9BCiinc
                             0x05, // J9BCiincw
                             0x01, // J9BCi2l
                             0x01, // J9BCi2f
                             0x01, // J9BCi2d
                             0x01, // J9BCl2i
                             0x01, // J9BCl2f
                             0x01, // J9BCl2d
                             0x01, // J9BCf2i
                             0x01, // J9BCf2l
                             0x01, // J9BCf2d
                             0x01, // J9BCd2i
                             0x01, // J9BCd2l
                             0x01, // J9BCd2f
                             0x01, // J9BCi2b
                             0x01, // J9BCi2c
                             0x01, // J9BCi2s
                             0x01, // J9BClcmp
                             0x01, // J9BCfcmpl
                             0x01, // J9BCfcmpg
                             0x01, // J9BCdcmpl
                             0x01, // J9BCdcmpg
   TwoByteRelativeBranch  |  0x03, // J9BCifeq
   TwoByteRelativeBranch  |  0x03, // J9BCifne
   TwoByteRelativeBranch  |  0x03, // J9BCiflt
   TwoByteRelativeBranch  |  0x03, // J9BCifge
   TwoByteRelativeBranch  |  0x03, // J9BCifgt
   TwoByteRelativeBranch  |  0x03, // J9BCifle
   TwoByteRelativeBranch  |  0x03, // J9BCificmpeq
   TwoByteRelativeBranch  |  0x03, // J9BCificmpne
   TwoByteRelativeBranch  |  0x03, // J9BCificmplt
   TwoByteRelativeBranch  |  0x03, // J9BCificmpge
   TwoByteRelativeBranch  |  0x03, // J9BCificmpgt
   TwoByteRelativeBranch  |  0x03, // J9BCificmple
   TwoByteRelativeBranch  |  0x03, // J9BCifacmpeq
   TwoByteRelativeBranch  |  0x03, // J9BCifacmpne
   TwoByteRelativeBranch  |  0x03, // J9BCifnull
   TwoByteRelativeBranch  |  0x03, // J9BCifnonnull
   TwoByteRelativeBranch  |  0x03, // J9BCgoto
   FourByteRelativeBranch |  0x05, // J9BCgotow
                             0x00, // J9BCtableswitch
                             0x00, // J9BClookupswitch
                             0x01, // J9BCgenericReturn
                             0x03, // J9BCgetstatic
                             0x03, // J9BCputstatic
                             0x03, // J9BCgetfield
                             0x03, // J9BCputfield
                             0x03, // J9BCinvokevirtual
                             0x03, // J9BCinvokespecial
                             0x03, // J9BCinvokestatic
                             0x03, // J9BCinvokeinterface
                             0x03, // J9BCinvokedynamic
                             0x03, // J9BCinvokehandle
                             0x03, // J9BCinvokehandlegeneric
                             0x03, // J9BCinvokespecialsplit
                             0x01, // J9BCReturnC
                             0x01, // J9BCReturnS
                             0x01, // J9BCReturnB
                             0x01, // J9BCReturnZ
                             0x03, // J9BCinvokestaticsplit
                             0x01, // J9BCinvokeinterface2
                             0x03, // J9BCnew
                             0x02, // J9BCnewarray
                             0x03, // J9BCanewarray
                             0x04, // J9BCmultianewarray
                             0x01, // J9BCarraylength
                             0x01, // J9BCathrow
                             0x03, // J9BCcheckcast
                             0x03, // J9BCinstanceof
                             0x01, // J9BCmonitorenter
                             0x01, // J9BCmonitorexit
                             0x00, // J9BCwide
                             0x01, // BCunknown
   };

const uint8_t TR_J9ByteCodeIterator::_estimatedCodeSize[] =
   {
   0, // J9BCnop
   1, // J9BCaconstnull
   1, // J9BCiconstm1
   1, // J9BCiconst0
   1, // J9BCiconst1
   1, // J9BCiconst2
   1, // J9BCiconst3
   1, // J9BCiconst4
   1, // J9BCiconst5
   2, // J9BClconst0
   2, // J9BClconst1
   1, // J9BCfconst0
   1, // J9BCfconst1
   2, // J9BCfconst2
   1, // J9BCdconst0
   1, // J9BCdconst1
   1, // J9BCbipush
   1, // J9BCsipush
   1, // J9BCldc
   1, // J9BCldcw
   2, // J9BCldc2lw
   2, // J9BCldc2dw
   1, // J9BCiload
   2, // J9BClload
   1, // J9BCfload
   1, // J9BCdload
   1, // J9BCaload
   1, // J9BCiload0
   1, // J9BCiload1
   1, // J9BCiload2
   1, // J9BCiload3
   2, // J9BClload0
   2, // J9BClload1
   2, // J9BClload2
   2, // J9BClload3
   1, // J9BCfload0
   1, // J9BCfload1
   1, // J9BCfload2
   1, // J9BCfload3
   1, // J9BCdload0
   1, // J9BCdload1
   1, // J9BCdload2
   1, // J9BCdload3
   1, // J9BCaload0
   1, // J9BCaload1
   1, // J9BCaload2
   1, // J9BCaload3
   5, // J9BCiaload
   6, // J9BClaload
   5, // J9BCfaload
   5, // J9BCdaload
   5, // J9BCaaload
   5, // J9BCbaload
   5, // J9BCcaload
   5, // J9BCsaload
   1, // J9BCiloadw
   2, // J9BClloadw
   1, // J9BCfloadw
   1, // J9BCdloadw
   1, // J9BCaloadw
   1, // J9BCistore
   2, // J9BClstore
   1, // J9BCfstore
   1, // J9BCdstore
   1, // J9BCastore
   1, // J9BCistorew
   2, // J9BClstorew
   1, // J9BCfstorew
   1, // J9BCdstorew
   1, // J9BCastorew
   1, // J9BCistore0
   1, // J9BCistore1
   1, // J9BCistore2
   1, // J9BCistore3
   2, // J9BClstore0
   2, // J9BClstore1
   2, // J9BClstore2
   2, // J9BClstore3
   1, // J9BCfstore0
   1, // J9BCfstore1
   1, // J9BCfstore2
   1, // J9BCfstore3
   1, // J9BCdstore0
   1, // J9BCdstore1
   1, // J9BCdstore2
   1, // J9BCdstore3
   1, // J9BCastore0
   1, // J9BCastore1
   1, // J9BCastore2
   1, // J9BCastore3
   5, // J9BCiastore
   6, // J9BClastore
   5, // J9BCfastore
   5, // J9BCdastore
   5, // J9BCaastore
   5, // J9BCbastore
   5, // J9BCcastore
   5, // J9BCsastore
   0, // J9BCpop
   0, // J9BCpop2
   0, // J9BCdup
   0, // J9BCdupx1
   0, // J9BCdupx2
   0, // J9BCdup2
   0, // J9BCdup2x1
   0, // J9BCdup2x2
   0, // J9BCswap
   1, // J9BCiadd
   2, // J9BCladd
   1, // J9BCfadd
   1, // J9BCdadd
   1, // J9BCisub
   2, // J9BClsub
   1, // J9BCfsub
   1, // J9BCdsub
   1, // J9BCimul
   2, // J9BClmul
   1, // J9BCfmul
   1, // J9BCdmul
   1, // J9BCidiv
   2, // J9BCldiv
   3, // J9BCfdiv
   3, // J9BCddiv
   1, // J9BCirem
   2, // J9BClrem
   3, // J9BCfrem
   3, // J9BCdrem
   1, // J9BCineg
   2, // J9BClneg
   1, // J9BCfneg
   1, // J9BCdneg
   1, // J9BCishl
   2, // J9BClshl
   1, // J9BCishr
   2, // J9BClshr
   1, // J9BCiushr
   2, // J9BClushr
   1, // J9BCiand
   2, // J9BCland
   1, // J9BCior
   2, // J9BClor
   1, // J9BCixor
   2, // J9BClxor
   3, // J9BCiinc
   3, // J9BCiincw
   2, // J9BCi2l
   2, // J9BCi2f
   2, // J9BCi2d
   1, // J9BCl2i
   2, // J9BCl2f
   2, // J9BCl2d
   4, // J9BCf2i
   4, // J9BCf2l
   1, // J9BCf2d
   4, // J9BCd2i
   4, // J9BCd2l
   2, // J9BCd2f
   1, // J9BCi2b
   1, // J9BCi2c
   1, // J9BCi2s
   8, // J9BClcmp
   5, // J9BCfcmpl
   5, // J9BCfcmpg
   5, // J9BCdcmpl
   5, // J9BCdcmpg
   1, // J9BCifeq
   1, // J9BCifne
   1, // J9BCiflt
   1, // J9BCifge
   1, // J9BCifgt
   1, // J9BCifle
   2, // J9BCificmpeq
   2, // J9BCificmpne
   2, // J9BCificmplt
   2, // J9BCificmpge
   2, // J9BCificmpgt
   2, // J9BCificmple
   2, // J9BCifacmpeq
   2, // J9BCifacmpne
   2, // J9BCifnull
   2, // J9BCifnonnull
   1, // J9BCgoto
   1, // J9BCgotow
  10, // J9BCtableswitch
  10, // J9BClookupswitch
  25, // J9BCgenericReturn
   1, // J9BCgetstatic
   1, // J9BCputstatic
   1, // J9BCgetfield
   1, // J9BCputfield
   6, // J9BCinvokevirtual
   6, // J9BCinvokespecial
   4, // J9BCinvokestatic
  11, // J9BCinvokeinterface
  20, // J9BCinvokedynamic
  20, // J9BCinvokehandle
  30, // J9BCinvokehandlegeneric
   6, // J9BCinvokespecialsplit
  25, // J9BCReturnC
  25, // J9BCReturnS
  25, // J9BCReturnB
  25, // J9BCReturnZ
   4, // J9BCinvokestaticsplit
   0, // J9BCinvokeinterface2
   1, // J9BCnew
   1, // J9BCnewarray
   1, // J9BCanewarray
   1, // J9BCmultianewarray
   2, // J9BCarraylength
   1, // J9BCathrow
   5, // J9BCcheckcast
   5, // J9BCinstanceof
  25, // J9BCmonitorenter
  25, // J9BCmonitorexit
   0, // J9BCwide
   0, // J9BCunknown
   };

//                                              ==           !=            <             >=            >             <=
TR::ILOpCodes TR_J9ByteCodeIterator::_lcmpOps[]  = { TR::iflcmpeq, TR::iflcmpne,  TR::iflcmplt,  TR::iflcmpge,  TR::iflcmpgt,  TR::iflcmple  };
TR::ILOpCodes TR_J9ByteCodeIterator::_fcmplOps[] = { TR::iffcmpeq, TR::iffcmpneu, TR::iffcmpltu, TR::iffcmpge,  TR::iffcmpgt,  TR::iffcmpleu };
TR::ILOpCodes TR_J9ByteCodeIterator::_fcmpgOps[] = { TR::iffcmpeq, TR::iffcmpneu, TR::iffcmplt,  TR::iffcmpgeu, TR::iffcmpgtu, TR::iffcmple  };
TR::ILOpCodes TR_J9ByteCodeIterator::_dcmplOps[] = { TR::ifdcmpeq, TR::ifdcmpneu, TR::ifdcmpltu, TR::ifdcmpge,  TR::ifdcmpgt,  TR::ifdcmpleu };
TR::ILOpCodes TR_J9ByteCodeIterator::_dcmpgOps[] = { TR::ifdcmpeq, TR::ifdcmpneu, TR::ifdcmplt,  TR::ifdcmpgeu, TR::ifdcmpgtu, TR::ifdcmple  };

