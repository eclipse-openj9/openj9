/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

/**
 * Support code for TR::CodeGenerator.  Code related to generating GPU
 */

#if defined (_MSC_VER) && (_MSC_VER < 1900)
#define snprintf _snprintf
#endif

#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/annotations/GPUAnnotation.hpp"
#include "optimizer/Dominators.hpp"
#include "optimizer/Structure.hpp"
#include "omrformatconsts.h"

#define OPT_DETAILS "O^O CODE GENERATION: "

static const char* getOpCodeName(TR::ILOpCodes opcode) {

   TR_ASSERT(opcode < TR::NumIlOps, "Wrong opcode");
   
   switch(opcode)
      {
      case TR::iload:
      case TR::fload:
      case TR::dload:
      case TR::aload:
      case TR::bload:
      case TR::sload:
      case TR::lload:
      case TR::iloadi:
      case TR::floadi:
      case TR::dloadi:
      case TR::aloadi:
      case TR::bloadi:
      case TR::sloadi:
      case TR::lloadi:
      case TR::iuload:
      case TR::luload:
      case TR::buload:
      case TR::iuloadi:
      case TR::luloadi:
      case TR::buloadi:
      case TR::cload:
      case TR::cloadi:
         return "load";

      case TR::istore:
      case TR::lstore:
      case TR::fstore:
      case TR::dstore:
      case TR::astore:
      case TR::bstore:
      case TR::sstore:
      case TR::lstorei:
      case TR::fstorei:
      case TR::dstorei:
      case TR::astorei:
      case TR::bstorei:
      case TR::sstorei:
      case TR::istorei:
      case TR::iustore:
      case TR::lustore:
      case TR::bustore:
      case TR::iustorei:
      case TR::lustorei:
      case TR::bustorei:
      case TR::cstore:
      case TR::cstorei:
         return "store";

      case TR::Goto:
         return "br";

      case TR::ireturn:
      case TR::lreturn:
      case TR::freturn:
      case TR::dreturn:
      case TR::areturn:
      case TR::Return:
         return "ret";

      case TR::iadd:
      case TR::ladd:
      case TR::badd:
      case TR::sadd:
      case TR::iuadd:
      case TR::luadd:
      case TR::buadd:
      case TR::cadd:
         return "add";

      case TR::fadd:
      case TR::dadd:
         return "fadd";

      case TR::isub:
      case TR::lsub:
      case TR::bsub:
      case TR::ssub:
      case TR::ineg:
      case TR::lneg:
      case TR::bneg:
      case TR::sneg:
      case TR::iusub:
      case TR::lusub:
      case TR::busub:
      case TR::iuneg:
      case TR::luneg:
      case TR::csub:
         return "sub";

      case TR::dsub:
      case TR::fsub:
      case TR::fneg:
      case TR::dneg:
         return "fsub";

      case TR::imul:
      case TR::lmul:
      case TR::bmul:
      case TR::smul:
         return "mul";

      case TR::fmul:
      case TR::dmul:
         return "fmul";

      case TR::idiv:
      case TR::ldiv:
      case TR::bdiv:
      case TR::sdiv:
         return "sdiv";

      case TR::fdiv:
      case TR::ddiv:
         return "fdiv";

      case TR::iudiv:
      case TR::ludiv:
         return "udiv";

      case TR::irem:
      case TR::lrem:
      case TR::brem:
      case TR::srem:
         return "srem";

      case TR::frem:
      case TR::drem:
         return "frem";

      case TR::iurem:
         return "urem";

      case TR::ishl:
      case TR::lshl:
      case TR::bshl:
      case TR::sshl:
         return "shl";

      case TR::ishr:
      case TR::lshr:
      case TR::bshr:
      case TR::sshr:
         return "ashr";

      case TR::iushr:
      case TR::lushr:
      case TR::bushr:
      case TR::sushr:
         return "lshr";

      case TR::iand:
      case TR::land:
      case TR::band:
      case TR::sand:
         return "and";

      case TR::ior:
      case TR::lor:
      case TR::bor:
      case TR::sor:
         return "or";

      case TR::ixor:
      case TR::lxor:
      case TR::bxor:
      case TR::sxor:
         return "xor";

      case TR::i2l:
      case TR::b2i:
      case TR::b2l:
      case TR::b2s:
      case TR::s2i:
      case TR::s2l:
         return "sext";

      case TR::i2f:
      case TR::i2d:
      case TR::l2f:
      case TR::l2d:
      case TR::b2f:
      case TR::b2d:
      case TR::s2f:
      case TR::s2d:
         return "sitofp";

      case TR::i2b:
      case TR::i2s:
      case TR::l2i:
      case TR::l2b:
      case TR::l2s:
      case TR::s2b:
         return "trunc";

      case TR::l2a:
      case TR::i2a:
      case TR::s2a:
      case TR::b2a:
      case TR::lu2a:
      case TR::iu2a:
      case TR::su2a:
      case TR::bu2a:
         return "inttoptr";

      case TR::iu2l:
      case TR::bu2i:
      case TR::bu2l:
      case TR::bu2s:
      case TR::su2i:
      case TR::su2l:
         return "zext";
         
      case TR::iu2f:
      case TR::iu2d:
      case TR::lu2f:
      case TR::lu2d:
      case TR::bu2f:
      case TR::bu2d:
      case TR::su2f:
      case TR::su2d:
         return "uitofp";

      case TR::f2i:
      case TR::f2l:
      case TR::f2b:
      case TR::f2s:
      case TR::d2i:
      case TR::d2l:
      case TR::d2b:
      case TR::d2s:
         return "fptosi";

      case TR::f2d:
         return "fpext";

      case TR::d2f:
         return "fptrunc";

      case TR::a2i:
      case TR::a2l:
      case TR::a2b:
      case TR::a2s:
         return "ptrtoint";

      case TR::icmpeq:
      case TR::lcmpeq:
      case TR::acmpeq:
      case TR::bcmpeq:
      case TR::scmpeq:
      case TR::ificmpeq:
      case TR::iflcmpeq:
      case TR::ifacmpeq:
      case TR::ifbcmpeq:
      case TR::ifscmpeq:
         return "icmp eq";

      case TR::icmpne:
      case TR::lcmpne:
      case TR::acmpne:
      case TR::bcmpne:
      case TR::scmpne:
      case TR::ificmpne:
      case TR::iflcmpne:
      case TR::ifacmpne:
      case TR::ifbcmpne:
      case TR::ifscmpne:
         return "icmp ne";

      case TR::icmplt:
      case TR::lcmplt:
      case TR::bcmplt:
      case TR::scmplt:
      case TR::ificmplt:
      case TR::iflcmplt:
      case TR::ifbcmplt:
      case TR::ifscmplt:
         return "icmp slt";

      case TR::icmpge:
      case TR::lcmpge:
      case TR::bcmpge:
      case TR::scmpge:
      case TR::ificmpge:
      case TR::iflcmpge:
      case TR::ifbcmpge:
      case TR::ifscmpge:
         return "icmp sge";

      case TR::icmpgt:
      case TR::lcmpgt:
      case TR::bcmpgt:
      case TR::scmpgt:
      case TR::ificmpgt:
      case TR::iflcmpgt:
      case TR::ifbcmpgt:
      case TR::ifscmpgt:
         return "icmp sgt";

      case TR::icmple:
      case TR::lcmple:
      case TR::bcmple:
      case TR::scmple:
      case TR::ificmple:
      case TR::iflcmple:
      case TR::ifbcmple:
      case TR::ifscmple:
         return "icmp sle";

      case TR::acmplt:
      case TR::iucmplt:
      case TR::lucmplt:
      case TR::bucmplt:
      case TR::sucmplt:
      case TR::ifacmplt:
      case TR::ifiucmplt:
      case TR::iflucmplt:
      case TR::ifbucmplt:
      case TR::ifsucmplt:
         return "icmp ult";

      case TR::acmpge:
      case TR::iucmpge:
      case TR::bucmpge:
      case TR::lucmpge:
      case TR::sucmpge:
      case TR::ifacmpge:
      case TR::ifiucmpge:
      case TR::iflucmpge:
      case TR::ifbucmpge:
      case TR::ifsucmpge:
         return "icmp uge";

      case TR::acmpgt:
      case TR::iucmpgt:
      case TR::lucmpgt:
      case TR::bucmpgt:
      case TR::sucmpgt:
      case TR::ifacmpgt:
      case TR::ifiucmpgt:
      case TR::iflucmpgt:
      case TR::ifbucmpgt:
      case TR::ifsucmpgt:
         return "icmp ugt";

      case TR::acmple:
      case TR::iucmple:
      case TR::lucmple:
      case TR::bucmple:
      case TR::sucmple:
      case TR::ifacmple:
      case TR::ifiucmple:
      case TR::iflucmple:
      case TR::ifbucmple:
      case TR::ifsucmple:
         return "icmp ule";

      case TR::fcmpeq:
      case TR::dcmpeq:
      case TR::iffcmpeq:
      case TR::ifdcmpeq:
         return "fcmp oeq";

      case TR::fcmpne:
      case TR::dcmpne:
      case TR::iffcmpne:
      case TR::ifdcmpne:
         return "fcmp one";

      case TR::fcmplt:
      case TR::dcmplt:
      case TR::iffcmplt:
      case TR::ifdcmplt:
         return "fcmp olt";

      case TR::fcmpge:
      case TR::dcmpge:
      case TR::iffcmpge:
      case TR::ifdcmpge:
         return "fcmp oge";

      case TR::fcmpgt:
      case TR::dcmpgt:
      case TR::iffcmpgt:
      case TR::ifdcmpgt:
         return "fcmp ogt";

      case TR::fcmple:
      case TR::dcmple:
      case TR::iffcmple:
      case TR::ifdcmple:
         return "fcmp ole";

      case TR::fcmpequ:
      case TR::dcmpequ:
      case TR::iffcmpequ:
      case TR::ifdcmpequ:
         return "fcmp ueq";

      case TR::fcmpneu:
      case TR::dcmpneu:
      case TR::iffcmpneu:
      case TR::ifdcmpneu:
         return "fcmp une";

      case TR::fcmpltu:
      case TR::dcmpltu:
      case TR::iffcmpltu:
      case TR::ifdcmpltu:
         return "fcmp ult";

      case TR::fcmpgeu:
      case TR::dcmpgeu:
      case TR::iffcmpgeu:
      case TR::ifdcmpgeu:
         return "fcmp uge";

      case TR::fcmpgtu:
      case TR::dcmpgtu:
      case TR::iffcmpgtu:
      case TR::ifdcmpgtu:
         return "fcmp ugt";

      case TR::fcmpleu:
      case TR::dcmpleu:
      case TR::iffcmpleu:
      case TR::ifdcmpleu:
         return "fcmp ule";

      case TR::d2c:
      case TR::f2c:
      case TR::f2bu:
      case TR::f2iu:
      case TR::f2lu:
      case TR::d2iu:
      case TR::d2lu:
      case TR::d2bu:
         return "fptoui";

      case TR::aiadd:
      case TR::aladd:
      case TR::aiuadd:
      case TR::aluadd:
         return "getelementptr";

      case TR::ibits2f:
      case TR::fbits2i:
      case TR::lbits2d:
      case TR::dbits2l:
         return "bitcast";

      case TR::lookup:
      case TR::table:
         return "switch";

      case TR::BBStart:
      case TR::BBEnd:
         return "";

      case TR::newarray:
         return "INVALID";

      default:
         return NULL;  
      }

}


char *nvvmTypeNames[TR::NumTypes] =
   {
   "void",    // "TR::NoType"
   "i8",      // "TR::Int8"
   "i16",     // "TR::Int16"
   "i32",     // "TR::Int32"
   "i64",     // "TR::Int64"
   "float",   // "TR::Float"
   "double",  // "TR::Double"
   "i8*"      // "TR::Address"
   };

static const char* getTypeName(TR::DataType type) {
    if (type >= TR::NoType && type <= TR::Address)
       {
       return nvvmTypeNames[type];
       }
    else
       {
       TR_ASSERT(false, "Unsupported type");
       return "???";
       }
}

char *nvvmVarTypeNames[TR::NumTypes] =
   {
   "void",    // "TR::NoType"
   "i8",      // "TR::Int8"
   "i16",     // "TR::Int16"
   "i32",     // "TR::Int32"
   "i64",     // "TR::Int64"
   "f32",     // "TR::Float"
   "f64",     // "TR::Double"
   "p64"      // "TR::Address"
   };

static const char* getVarTypeName(TR::DataType type) {
    if (type >= TR::NoType && type <= TR::Address)
       {
       return nvvmVarTypeNames[type];
       }
    else
       {
       TR_ASSERT(false, "Unsupported type");
       return "???";
       }
}

#define MAX_NAME 256


static void getParmName(int32_t slot, char * s, bool addr = true)
   {
   int32_t len = 0;

   len = snprintf(s, MAX_NAME, "%%p%" OMR_PRId32 "%s", slot,  addr ? ".addr" : "");

   TR_ASSERT(len < MAX_NAME, "Auto's or parm's name is too long\n");
   }


static void getAutoOrParmName(TR::Symbol *sym, char * s, bool addr = true)
   {
   int32_t len = 0;
   TR_ASSERT(sym->isAutoOrParm(), "expecting auto or parm");

   if (sym->isParm())
      len = snprintf(s, MAX_NAME, "%%p%" OMR_PRId32 "%s", sym->castToParmSymbol()->getSlot(),  addr ? ".addr" : "");
   else
      len = snprintf(s, MAX_NAME, "%%a%" OMR_PRId32 "%s", sym->castToAutoSymbol()->getLiveLocalIndex(), addr ? ".addr" : "");

   TR_ASSERT(len < MAX_NAME, "Auto's or parm's name is too long\n");
   }


#define INIT_BUFFER_SIZE 65535

class NVVMIRBuffer
   {
   public:
   NVVMIRBuffer(TR_Memory* mem)
      {
      m = mem;
      size = INIT_BUFFER_SIZE;
      buffer = (char*)m->allocateHeapMemory(size);
      s = buffer;
      }
   void print(char *format, ...)
      {
      va_list args;
      va_start (args, format);
      int32_t left = size - (s - buffer);

      va_list args_copy;
      va_copy(args_copy, args);
      int32_t len = vsnprintf(s, left, format, args_copy);
      va_copy_end(args_copy);

      if ((len + 1) > left)
         {
         expand(len + 1 - left);
         left = size - (s - buffer);
         len = vsnprintf(s, left, format, args);
         }

      s += len;
      va_end(args);
      }

   char * getString() { return buffer; }

   private:

   void expand(int32_t min)
      {
      size += (min >= size) ? size*2 : size;

      char * newBuffer = (char*)m->allocateHeapMemory(size);
      memcpy(newBuffer, buffer, s - buffer);
      s = newBuffer + (s - buffer);
      buffer = newBuffer;
      }

   char *buffer;
   char *s;
   int32_t size;
   TR_Memory* m;
};


static void getNodeName(TR::Node* node, char * s, TR::Compilation *comp)
   {
   int32_t len = 0;
   if (node->getOpCode().isLoadConst())
      {
      bool isUnsigned = node->getOpCode().isUnsigned();
      switch (node->getDataType())
         {
         case TR::Int8:
            if(isUnsigned)
               len = snprintf(s, MAX_NAME, "%" OMR_PRIu8, node->getUnsignedByte());
            else
               len = snprintf(s, MAX_NAME, "%" OMR_PRId8, node->getByte());
            break;
         case TR::Int16:
            len = snprintf(s, MAX_NAME, "%" OMR_PRIu16, node->getConst<uint16_t>());
            break;
         case TR::Int32:
            if(isUnsigned)
               len = snprintf(s, MAX_NAME, "%" OMR_PRIu32, node->getUnsignedInt());
            else
               len = snprintf(s, MAX_NAME, "%" OMR_PRId32, node->getInt());
            break;
         case TR::Int64:
            if(isUnsigned)
               len = snprintf(s, MAX_NAME, "%" OMR_PRIu64, node->getUnsignedLongInt());
            else
               len = snprintf(s, MAX_NAME, "%" OMR_PRId64, node->getLongInt());
            break;
         case TR::Float:
            union
               {
               double                  doubleValue;
               int64_t                 doubleBits;
               };
            doubleValue = node->getFloat();
            len = snprintf(s, MAX_NAME, "0x%016" OMR_PRIx64, doubleBits);
            break;
         case TR::Double:
            len = snprintf(s, MAX_NAME, "0x%016" OMR_PRIx64, node->getDoubleBits());
            break;
         case TR::Address:
            if (node->getAddress() == 0)
               len = snprintf(s, MAX_NAME, "null");
            else
               TR_ASSERT(0, "Non-null Address constants should not occur.\n");
            break;
         default:
            TR_ASSERT(0, "Unknown/unimplemented data type\n");
         }
      }
   else
      {
      len = snprintf(s, MAX_NAME, "%%%" OMR_PRIu32, node->getLocalIndex());
      }

   TR_ASSERT(len < MAX_NAME, "Node's name is too long\n");
   }

char* getNVVMMathFunctionName(TR::Node *node)
   {
   switch (((TR::MethodSymbol*)node->getSymbolReference()->getSymbol())->getRecognizedMethod())
      {
      case TR::java_lang_Math_sqrt:
         return "sqrt";
      case TR::java_lang_Math_sin:
      case TR::java_lang_StrictMath_sin:
         return "sin";
      case TR::java_lang_Math_cos:
      case TR::java_lang_StrictMath_cos:
         return "cos";
      case TR::java_lang_Math_log:
      case TR::java_lang_StrictMath_log:
         return "log";
      case TR::java_lang_Math_exp:
      case TR::java_lang_StrictMath_exp:
         return "exp";
      case TR::java_lang_Math_abs_F:
         return "fabsf";
      case TR::java_lang_Math_abs_D:
         return "fabs";
      default:
         return "ERROR";
      }
   return "ERROR";
   }

bool J9::CodeGenerator::handleRecognizedMethod(TR::Node *node, NVVMIRBuffer &ir, TR::Compilation *comp)
   {
   char name0[MAX_NAME];
   switch (((TR::MethodSymbol*)node->getSymbolReference()->getSymbol())->getRecognizedMethod())
      {
      case TR::com_ibm_gpu_Kernel_blockIdxX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockIdxY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.y()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockIdxZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.z()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockDimX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.x()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockDimY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.y()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_blockDimZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.z()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_threadIdxX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_threadIdxY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.y()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_threadIdxZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.z()\n", node->getLocalIndex());
          break;
      case TR::com_ibm_gpu_Kernel_syncThreads:
          ir.print("  call void @llvm.nvvm.barrier0()\n");
          node->setLocalIndex(_gpuNodeCount--);
          break;
      case TR::java_lang_Math_sqrt:
      case TR::java_lang_Math_sin:
      case TR::java_lang_Math_cos:
      case TR::java_lang_Math_log:
      case TR::java_lang_Math_exp:
      case TR::java_lang_Math_abs_D:
          if (!comp->getOptions()->getEnableGPU(TR_EnableGPUEnableMath)) return false;
          getNodeName(node->getChild(0), name0, comp);
          ir.print("  %%%d = call double @__nv_%s(double %s)\n", node->getLocalIndex(), getNVVMMathFunctionName(node), name0);
          break;
      case TR::java_lang_StrictMath_sin:
      case TR::java_lang_StrictMath_cos:
      case TR::java_lang_StrictMath_log:
      case TR::java_lang_StrictMath_exp:
          if (!comp->getOptions()->getEnableGPU(TR_EnableGPUEnableMath)) return false;
          getNodeName(node->getChild(1), name0, comp);
          ir.print("  %%%d = call double @__nv_%s(double %s)\n", node->getLocalIndex(), getNVVMMathFunctionName(node), name0);
          break;
      case TR::java_lang_Math_abs_F:
          if (!comp->getOptions()->getEnableGPU(TR_EnableGPUEnableMath)) return false;
          getNodeName(node->getChild(0), name0, comp);
          ir.print("  %%%d = call float @__nv_%s(float %s)\n", node->getLocalIndex(), getNVVMMathFunctionName(node), name0);
          break;
      default:
          return false;
      }
   return true;
   }


bool J9::CodeGenerator::handleRecognizedField(TR::Node *node, NVVMIRBuffer &ir)
   {
   switch (node->getSymbolReference()->getSymbol()->getRecognizedField())
      {
      case TR::Symbol::Com_ibm_gpu_Kernel_blockIdxX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockIdxY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.y()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockIdxZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.z()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockDimX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.x()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockDimY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.y()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_blockDimZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.ntid.z()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_threadIdxX:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_threadIdxY:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.y()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_threadIdxZ:
          ir.print("  %%%d = call i32 @llvm.nvvm.read.ptx.sreg.tid.z()\n", node->getLocalIndex());
          break;
      case TR::Symbol::Com_ibm_gpu_Kernel_syncThreads:
          ir.print("  call void @llvm.nvvm.barrier0()\n");
          node->setLocalIndex(_gpuNodeCount--);
          break;
      default:
          return false;
      }
      return true;
   }

void J9::CodeGenerator::printArrayCopyNVVMIR(TR::Node *node, NVVMIRBuffer &ir, TR::Compilation *comp)
   {
   //Some forms of array copy have five children. First two nodes are used for write barriers which we don't need
   // Three child version
   //      child 0 ------  Source byte address;
   //      child 1 ------  Destination byte address;
   //      child 2 ------  Copy length in byte;
   // Five child version:
   //      child 0 ------  Source array object; (skipped)
   //      child 1 ------  Destination array object; (skipped)
   //      child 2 ------  Source byte address;
   //      child 3 ------  Destination byte address;
   //      child 4 ------  Copy length in byte;
   //childrenNodeOffset is set such that we access Source byte address, Destination byte address and Copy length.
   int childrenNodeOffset = node->getNumChildren() == 5 ? 2 : 0;

   char name0[MAX_NAME], name1[MAX_NAME], name2[MAX_NAME];
   getNodeName(node->getChild(0+childrenNodeOffset), name0, comp);
   getNodeName(node->getChild(1+childrenNodeOffset), name1, comp);
   getNodeName(node->getChild(2+childrenNodeOffset), name2, comp);

   int arrayCopyID = node->getLocalIndex();
   bool isWordCopy = node->chkWordElementArrayCopy();
   bool isHalfwordCopy = node->chkHalfWordElementArrayCopy();
   bool unknownCopy = !(isWordCopy || isHalfwordCopy);
   bool isBackwardsCopy = !node->isForwardArrayCopy();
   bool is64bitCopyLength = (node->getChild(2+childrenNodeOffset)->getDataType() == TR::Int64);

   /* Example NVVM IR:

   ; Inputs to the array copy that come from the children:
     %8 = getelementptr inbounds i8* %7, i64 76  ; Source addr
     %10 = getelementptr inbounds i8* %9, i64 76 ; Destination addr
     %14 = mul i64 %13, 2                        ; Copy Length in bytes

   ; Generated ArrayCopy NVVM IR
   ; This is a reverse halfword array copy
     br label %ArrayCopy15
   ArrayCopy15:
     %15 = ptrtoint i8* %8 to i64  ; Generated for reverse array copy.
     %16 = ptrtoint i8* %10 to i64 ; Changes source and destination
     %17 = add i64 %15, %14        ; to point to the end of the array
     %18 = add i64 %16, %14        ; These lines are not generated
     %19 = sub i64 %17, 2          ; for a forward array copy
     %20 = sub i64 %18, 2          ;
     %21 = inttoptr i64 %19 to i8* ;
     %22 = inttoptr i64 %20 to i8* ;
     br label %ArrayCopyHeader15
   ArrayCopyHeader15:
     %23 = phi i64 [ %14, %ArrayCopy15 ], [ %36, %ArrayCopyBody15 ] ; Phi nodes save a different value to the temp
     %24 = phi i8* [ %21, %ArrayCopy15 ], [ %34, %ArrayCopyBody15 ] ; based on the name of the previous block before
     %25 = phi i8* [ %22, %ArrayCopy15 ], [ %35, %ArrayCopyBody15 ] ; jumping to ArrayCopyHeader15
     %26 = bitcast i8* %24 to i16*
     %27 = bitcast i8* %25 to i16*
     %28 = icmp sle i64 %23, 0
     br i1 %28, label %AfterArrayCopy15, label %ArrayCopyBody15     ; branch to exit if no more work to do
   ArrayCopyBody15:
     %29 = load i16* %26              ; load data from input array
     store i16 %29, i16* %27          ; store data to output array
     %30 = ptrtoint i16* %26 to i64
     %31 = ptrtoint i16* %27 to i64
     %32 = sub i64 %30, 2             ; sub is used for reverse copy, add used for forward copy
     %33 = sub i64 %31, 2             ; sub is used for reverse copy, add used for forward copy
     %34 = inttoptr i64 %32 to i8*
     %35 = inttoptr i64 %33 to i8*
     %36 = sub i64 %23, 2             ; decrement copy length
     br label %ArrayCopyHeader15
   AfterArrayCopy15:
   */


   //create a new block so the phi nodes know the name of the preceding block
   ir.print("  br label %%ArrayCopy%d\n", arrayCopyID);
   ir.print("ArrayCopy%d:\n", arrayCopyID);

   //for a backwards copy, the source and destination pointers need to be adjusted to
   //point to the last element.
   if (isBackwardsCopy)
      {
      if (!is64bitCopyLength)
         {
         ir.print("  %%%d = sext %s %s to i64\n",
                      node->getLocalIndex(),
                      getTypeName(node->getChild(2+childrenNodeOffset)->getDataType()),
                      name2);
         node->setLocalIndex(_gpuNodeCount++);
         }

      ir.print("  %%%d = ptrtoint %s %s to i64\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                   name0);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = ptrtoint %s %s to i64\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                   name1);
      node->setLocalIndex(_gpuNodeCount++);

      if (is64bitCopyLength)
         {
         ir.print("  %%%d = add i64 %%%d, %s\n",
                      node->getLocalIndex(),
                      node->getLocalIndex()-2,
                      name2);
         node->setLocalIndex(_gpuNodeCount++);

         ir.print("  %%%d = add i64 %%%d, %s\n",
                      node->getLocalIndex(),
                      node->getLocalIndex()-2,
                      name2);
         node->setLocalIndex(_gpuNodeCount++);
         }
      else
         {
         ir.print("  %%%d = add i64 %%%d, %%%d\n",
                      node->getLocalIndex(),
                      node->getLocalIndex()-2,
                      node->getLocalIndex()-3);
         node->setLocalIndex(_gpuNodeCount++);

         ir.print("  %%%d = add i64 %%%d, %%%d\n",
                      node->getLocalIndex(),
                      node->getLocalIndex()-2,
                      node->getLocalIndex()-4);
         node->setLocalIndex(_gpuNodeCount++);
         }

      ir.print("  %%%d = sub i64 %%%d, %d\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-2,
                   isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = sub i64 %%%d, %d\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-2,
                   isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = inttoptr i64 %%%d to %s\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-2,
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()));
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = inttoptr i64 %%%d to %s\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-2,
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()));
      node->setLocalIndex(_gpuNodeCount++);
      }

   ir.print("  br label %%ArrayCopyHeader%d\n", arrayCopyID);
   ir.print("ArrayCopyHeader%d:\n", arrayCopyID);

   //copy length in bytes
   ir.print("  %%%d = phi %s [ %s, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                node->getLocalIndex(),
                getTypeName(node->getChild(2+childrenNodeOffset)->getDataType()),
                name2,
                arrayCopyID,
                unknownCopy ? node->getLocalIndex()+11 : node->getLocalIndex()+13,
                arrayCopyID);
   node->setLocalIndex(_gpuNodeCount++);

   if (!isBackwardsCopy)
      {
      //source address
      ir.print("  %%%d = phi %s [ %s, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                   name0,
                   arrayCopyID,
                   unknownCopy ? node->getLocalIndex()+8 : node->getLocalIndex()+10,
                   arrayCopyID);
      node->setLocalIndex(_gpuNodeCount++);

      //destination address
      ir.print("  %%%d = phi %s [ %s, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                   name1,
                   arrayCopyID,
                   unknownCopy ? node->getLocalIndex()+8 : node->getLocalIndex()+10,
                   arrayCopyID);
      node->setLocalIndex(_gpuNodeCount++);
      }
   else
      {
      //source address
      ir.print("  %%%d = phi %s [ %%%d, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                   node->getLocalIndex()-3,
                   arrayCopyID,
                   unknownCopy ? node->getLocalIndex()+8 : node->getLocalIndex()+10,
                   arrayCopyID);
      node->setLocalIndex(_gpuNodeCount++);

      //destination address
      ir.print("  %%%d = phi %s [ %%%d, %%ArrayCopy%d ], [ %%%d, %%ArrayCopyBody%d ]\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                   node->getLocalIndex()-3,
                   arrayCopyID,
                   unknownCopy ? node->getLocalIndex()+8 : node->getLocalIndex()+10,
                   arrayCopyID);
      node->setLocalIndex(_gpuNodeCount++);
      }

   //change pointer types from i8* if copying halfword or word data
   if (isWordCopy || isHalfwordCopy)
      {
      ir.print("  %%%d = bitcast %s %%%d to %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                   node->getLocalIndex()-2,
                   isWordCopy ? "i32*" : "i16*");
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = bitcast %s %%%d to %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                   node->getLocalIndex()-2,
                   isWordCopy ? "i32*" : "i16*");
      node->setLocalIndex(_gpuNodeCount++);
      }

   //check if byte length is less than or equal to zero and skip the copy if true
   ir.print("  %%%d = icmp sle %s %%%d, 0\n",
                node->getLocalIndex(),
                getTypeName(node->getChild(2+childrenNodeOffset)->getDataType()),
                unknownCopy ? node->getLocalIndex()-3 : node->getLocalIndex()-5);
   node->setLocalIndex(_gpuNodeCount++);

   ir.print("  br i1 %%%d, label %%AfterArrayCopy%d, label %%ArrayCopyBody%d\n",
                node->getLocalIndex()-1,
                arrayCopyID,
                arrayCopyID);

   ir.print("ArrayCopyBody%d:\n", arrayCopyID);

   //load data to copy
   ir.print("  %%%d = load %s %%%d\n",
                node->getLocalIndex(),
                isWordCopy ? "i32*" : isHalfwordCopy ? "i16*" : getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                node->getLocalIndex()-3);
   node->setLocalIndex(_gpuNodeCount++);

   //store loaded data
   ir.print("  store %s %%%d, %s %%%d\n",
                isWordCopy ? "i32" : isHalfwordCopy ? "i16" : "i8",
                node->getLocalIndex()-1,
                isWordCopy ? "i32*" : isHalfwordCopy ? "i16*" : getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                node->getLocalIndex()-3);

   ir.print("  %%%d = ptrtoint %s %%%d to i64\n",
                node->getLocalIndex(),
                isWordCopy ? "i32*" : isHalfwordCopy ? "i16*" : getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()),
                node->getLocalIndex()-4);
   node->setLocalIndex(_gpuNodeCount++);

   ir.print("  %%%d = ptrtoint %s %%%d to i64\n",
                node->getLocalIndex(),
                isWordCopy ? "i32*" : isHalfwordCopy ? "i16*" : getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()),
                node->getLocalIndex()-4);
   node->setLocalIndex(_gpuNodeCount++);

   //move source pointer
   ir.print("  %%%d = %s i64 %%%d, %d\n",
                node->getLocalIndex(),
                isBackwardsCopy ? "sub" : "add",
                node->getLocalIndex()-2,
                isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);
   node->setLocalIndex(_gpuNodeCount++);

   //move destination pointer
   ir.print("  %%%d = %s i64 %%%d, %d\n",
                node->getLocalIndex(),
                isBackwardsCopy ? "sub" : "add",
                node->getLocalIndex()-2,
                isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);
   node->setLocalIndex(_gpuNodeCount++);

   ir.print("  %%%d = inttoptr i64 %%%d to %s\n",
                node->getLocalIndex(),
                node->getLocalIndex()-2,
                getTypeName(node->getChild(0+childrenNodeOffset)->getDataType()));
   node->setLocalIndex(_gpuNodeCount++);

   ir.print("  %%%d = inttoptr i64 %%%d to %s\n",
                node->getLocalIndex(),
                node->getLocalIndex()-2,
                getTypeName(node->getChild(1+childrenNodeOffset)->getDataType()));
   node->setLocalIndex(_gpuNodeCount++);

   //decrement copy length
   ir.print("  %%%d = sub %s %%%d, %d\n",
                node->getLocalIndex(),
                getTypeName(node->getChild(2+childrenNodeOffset)->getDataType()),
                unknownCopy ? node->getLocalIndex()-11 : node->getLocalIndex()-13,
                isWordCopy ? 4 : isHalfwordCopy ? 2 : 1);

   ir.print("  br label %%ArrayCopyHeader%d\n", arrayCopyID);
   ir.print("AfterArrayCopy%d:\n", arrayCopyID);
   }

bool isThisPointer(TR::SymbolReference * symRef)
   {
   return symRef->getSymbol()->isParm() &&
          ((TR::ParameterSymbol *)symRef->getSymbol())->getSlot() == 0;
   }

char * getTypeNameFromSignature(char* sig, int32_t sigLength)
   {
   TR_ASSERT(sigLength == 2 && sig[0] == '[', "only handling static shared arrays");
   switch (sig[1])
      {
      case 'Z': return "i8";
      case 'B': return "i8";
      case 'C': return "i16";
      case 'S': return "i16";
      case 'I': return "i32";
      case 'J': return "i64";
      case 'F': return "float";
      case 'D': return "double";
      }
   TR_ASSERT(false, "unsupported shared array type\n");
   return NULL;
   }

static bool isSharedMemory(TR::Node *node, TR_SharedMemoryAnnotations *sharedMemory, TR::Compilation *comp)
   {
   if (!comp->isGPUCompilation()) return false;

   TR::SymbolReference *symRef = node->getSymbolReference();
   if (!symRef->getSymbol()->isAutoOrParm() && symRef->getCPIndex() != -1)
      {
      TR_SharedMemoryField field = sharedMemory->find(comp, symRef);
      if (field.getSize() > 0) return true;
      }
   return false;
   }


TR::CodeGenerator::GPUResult
J9::CodeGenerator::printNVVMIR(
      NVVMIRBuffer &ir,
      TR::Node * node,
      TR_RegionStructure *loop,
      TR_BitVector *targetBlocks,
      vcount_t visitCount,
      TR_SharedMemoryAnnotations *sharedMemory,
      int32_t &nextParmNum,
      TR::Node * &errorNode)
   {
   GPUResult result;

   static bool enableExceptionChecks = (feGetEnv("TR_disableGPUExceptionCheck") == NULL);
   TR::ILOpCode opcode = node->getOpCode();

   char name0[MAX_NAME], name1[MAX_NAME];
   bool isGenerated = false;
   bool printChildrenWithRefCount1 = true;

   if (node->isProfilingCode())
      {
      // Nothing to generate for profiling code, but we still need to visit the children
      // We can skip over the children with a reference count of one since they aren't used anywhere else.
      isGenerated = true;
      printChildrenWithRefCount1 = false;
      }

   if (node->getOpCodeValue() == TR::compressedRefs)
      {
      if (loop->isExprInvariant(node))
         return GPUSuccess; // ignore for now
      node = node->getFirstChild();
      }

   if (node->getOpCodeValue() == TR::treetop)
       node = node->getFirstChild();

   if (self()->comp()->isGPUCompilation() &&
       opcode.isLoadVarDirect() &&
       isThisPointer(node->getSymbolReference()))
       return GPUSuccess;   // will handle in the parent

   if (opcode.isLoadConst())
       if((node->getDataType() == TR::Address) && (node->getAddress() != 0))
          {
          traceMsg(self()->comp(), "Load Const with a non-zero address in node %p\n", node);
          return GPUInvalidProgram;
          }
       else
          return GPUSuccess;   // will handle in the parent

   if (node->getOpCodeValue() == TR::asynccheck)
       return GPUSuccess;

   if (!enableExceptionChecks &&
       (opcode.isNullCheck() || opcode.isBndCheck() || node->getOpCodeValue() == TR::DIVCHK))
       return GPUSuccess;


   if (node->getVisitCount() == visitCount)
      return GPUSuccess;

   node->setVisitCount(visitCount);

   if (opcode.isNullCheck())
      {

      TR::Node *refNode = node->getNullCheckReference();

      if (isSharedMemory(refNode, sharedMemory, self()->comp()))
         {
         // Shared Memory is always allocated
         ir.print("; DELETE NULLCHK [%p] since this reference [%p] is allocated in shared memory\n",
                  node, refNode);
         return GPUSuccess;
         }
      if (_gpuPostDominators && _gpuPostDominators->dominates(_gpuCurrentBlock, _gpuStartBlock))
         {
         TR::SymbolReference *symRef = refNode->getSymbolReference();
         if (symRef->getSymbol()->isParm())
            {
            int32_t argpos = symRef->getCPIndex();
            ir.print("; DELETE NULLCHK [%p] (ref[%p] is parm %d) since BB[%d] postdominates BB[%d]\n",
                     node, refNode, argpos, _gpuCurrentBlock->getNumber(), _gpuStartBlock->getNumber());
            _gpuNeedNullCheckArguments_vector |= (1L << (uint64_t)argpos);
            return GPUSuccess;
            }
         }

      result = self()->printNVVMIR(ir, refNode, loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess) return result;

      getNodeName(refNode, name0, self()->comp());
      const char *type0 = getTypeName(refNode->getDataType());

      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = icmp eq %s %s, null\n",
                   node->getLocalIndex(), type0, name0, name1);
      ir.print("  br i1 %%%d, label %%NullException, label %%nullchk_fallthru_%d, !prof !0\n",
                   node->getLocalIndex(), node->getLocalIndex());
      ir.print("nullchk_fallthru_%d:\n", node->getLocalIndex());

      _gpuHasNullCheck = true;
      isGenerated = true;
      }
   else if (opcode.isBndCheck())
      {
      bool isSMReference = false;
      int32_t smsize = -1;
      if (node->getChild(0)->getOpCodeValue() == TR::arraylength)
         {
         TR::Node *refNode = node->getChild(0)->getChild(0);
         if (isSharedMemory(refNode, sharedMemory, self()->comp()))
            {
            TR_SharedMemoryField field = sharedMemory->find(self()->comp(), refNode->getSymbolReference());
            smsize = field.getSize();
            TR_ASSERT(smsize > 0, "should be annotated as shared array with positive size");

            isSMReference = true;
            ir.print("; USE CONSTANT LENGTH %d for NULLCHK [%p] since this reference [%p] is allocated in shared memory\n",
                     smsize, node, refNode);
            }
         }

      if (!isSMReference)
         {
         result = self()->printNVVMIR(ir, node->getChild(0), loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
         if (result != GPUSuccess) return result;
         }
      result = self()->printNVVMIR(ir, node->getChild(1), loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess) return result;

      if (!isSMReference)
         {
         getNodeName(node->getChild(0), name0, self()->comp());
         }
      else
         {
         int32_t len = snprintf(name0, MAX_NAME, "%" OMR_PRId32, smsize);
         TR_ASSERT(len < MAX_NAME, "Node's name is too long\n");
         }

      getNodeName(node->getChild(1), name1, self()->comp());
      const char *type0 = getTypeName(node->getChild(0)->getDataType());

      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = icmp ule %s %s, %s\n",
                   node->getLocalIndex(), type0, name0, name1);
      ir.print("  br i1 %%%d, label %%BndException, label %%bndchk_fallthru_%d, !prof !0\n",
                   node->getLocalIndex(), node->getLocalIndex());
      ir.print("bndchk_fallthru_%d:\n", node->getLocalIndex());

      _gpuHasBndCheck = true;
      isGenerated = true;
      }
   else if (node->getOpCodeValue() == TR::DIVCHK)
      {
      TR::Node *idivNode = node->getChild(0);
      result = self()->printNVVMIR(ir, idivNode->getChild(0), loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess) return result;

      result = self()->printNVVMIR(ir, idivNode->getChild(1), loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess) return result;

      getNodeName(idivNode->getChild(1), name0, self()->comp());
      const char *type0 = getTypeName(idivNode->getChild(1)->getDataType());

      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = icmp eq %s %s, 0\n",
                   node->getLocalIndex(), type0, name0);
      ir.print("  br i1 %%%d, label %%DivException, label %%divchk_fallthru_%d, !prof !0\n",
                   node->getLocalIndex(), node->getLocalIndex());
      ir.print("divchk_fallthru_%d:\n", node->getLocalIndex());

      _gpuHasDivCheck = true;
      isGenerated = true;
      }

   // This symbol reference should become a parameter
   // children should be skipped (they are loop invariant)
   if (node->getOpCode().isLoadVar() &&
       _gpuSymbolMap[node->getSymbolReference()->getReferenceNumber()]._parmSlot != -1)
      {
      getParmName(_gpuSymbolMap[node->getSymbolReference()->getReferenceNumber()]._parmSlot, name0);

      node->setLocalIndex(_gpuNodeCount++);
      traceMsg(self()->comp(), "node %p assigned index %d\n", node, node->getLocalIndex());

      ir.print("  %%%d = %s %s* %s, align %d\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   name0,
                   node->getSize());
      return GPUSuccess;
      }

   //Don't run printNVVMIR on a children node if:
   //(they are the child of a profiling call) AND ((have a reference count less then two) OR (is a loadConst node))
   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      if ((child->getReferenceCount() >= 2 && !child->getOpCode().isLoadConst()) || printChildrenWithRefCount1)
         {
         result = self()->printNVVMIR(ir, child, loop, targetBlocks, visitCount, sharedMemory, nextParmNum, errorNode);
         if (result != GPUSuccess)
            return result;
         }
      }

   if (isGenerated)
      {
      return GPUSuccess;
      }

   node->setLocalIndex(_gpuNodeCount++);
   traceMsg(self()->comp(), "node %p assigned index %d\n", node, node->getLocalIndex());


   if (node->getOpCodeValue() == TR::PassThrough)
      {
      node->setLocalIndex(_gpuNodeCount--);
      return GPUSuccess;
      }
   else if (node->getOpCodeValue() == TR::BBStart)
      {
      node->setLocalIndex(_gpuNodeCount--);
      _gpuCurrentBlock = node->getBlock();
      if (targetBlocks->get(_gpuCurrentBlock->getNumber()))
          ir.print("block_%d:\n", _gpuCurrentBlock->getNumber());
      }
   // if block has a label previous block has to end with a branch
   else if (node->getOpCodeValue() == TR::BBEnd &&
       !_gpuCurrentBlock->endsInBranch() &&
       !_gpuCurrentBlock->getLastRealTreeTop()->getNode()->getOpCode().isReturn() &&
       !_gpuCurrentBlock->getLastRealTreeTop()->getNode()->getOpCode().isGoto() &&
       !_gpuCurrentBlock->getLastRealTreeTop()->getNode()->getOpCode().isSwitch() &&
       _gpuCurrentBlock->getNextBlock() &&
       targetBlocks->get(_gpuCurrentBlock->getNextBlock()->getNumber()))
      {
      node->setLocalIndex(_gpuNodeCount--);
      ir.print("  br label %%block_%d\n",
                   _gpuCurrentBlock->getNextBlock()->getNumber());
      }
   else if (node->getOpCodeValue() == TR::BBEnd)
      {
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCode().isReturn())
      {
      node->setLocalIndex(_gpuNodeCount--);

      if (node->getNumChildren() == 0)
         {
         ir.print("  %s void\n",
                      getOpCodeName(node->getOpCodeValue()));
         }
      else
         {
         TR_ASSERT(node->getNumChildren() == 1, "Unsupported return\n");
         getNodeName(node->getChild(0), name0, self()->comp());
         ir.print("  %s %s %s\n",
                      getOpCodeName(node->getOpCodeValue()),
                      getTypeName(node->getDataType()),
                      name0);
         }
      }
   else if (node->getOpCode().isStoreIndirect()) // TODO: handle statics
      {
      TR::Node *firstChild = node->getChild(0);
      TR::Node *secondChild = node->getChild(1);
      getNodeName(firstChild, name0, self()->comp());
      getNodeName(secondChild, name1, self()->comp());

         {
         _gpuNodeCount--;
         ir.print("  %%%d = bitcast %s %s to %s %s*\n",
                      _gpuNodeCount,
                      getTypeName(firstChild->getDataType()), name0,
                      getTypeName(secondChild->getDataType()),
                      firstChild->chkSharedMemory() ? "addrspace(3)" : "");

         ir.print("  %s %s %s, %s %s * %%%d, align %d\n",
                      getOpCodeName(node->getOpCodeValue()),
                      getTypeName(secondChild->getDataType()),
                      name1,
                      getTypeName(secondChild->getDataType()),
                      firstChild->chkSharedMemory() ? "addrspace(3)" : "",
                      _gpuNodeCount,
                      secondChild->getSize());
         _gpuNodeCount++;
         }
      }
   else if (node->getOpCode().isLoadIndirect()) // TODO: handle statics
      {
      TR::Node *firstChild = node->getChild(0);

      getNodeName(firstChild, name0, self()->comp());

      if (node->getSymbolReference()->getCPIndex() != -1) // field of some object
         {
         // TODO: check that field is an array!
         TR_ASSERT(firstChild->getOpCode().isLoadDirect() &&
                   isThisPointer(firstChild->getSymbolReference()),
                   "can only access a field of this object\n");

         // TODO: handle duplicate names from different classes
         TR_SharedMemoryField field = sharedMemory->find(self()->comp(), node->getSymbolReference());
         TR_ASSERT(field.getSize() >= 0, "field was not found in this object\n");

         if (field.getSize() > 0)
            {
            ir.print("  %%%d = bitcast [%d x %s] addrspace(3)* @%.*s to i8*\n",
                      node->getLocalIndex(),
                      field.getSize(),
                      getTypeNameFromSignature(field.getFieldSig(), field.getFieldSigLength()),
                      field.getFieldNameLength(), field.getFieldName());

            node->setSharedMemory(true);
            }
         else
            {
            int32_t parmNum = field.getParmNum();
            if (parmNum == -1)
               {
               sharedMemory->setParmNum(self()->comp(), node->getSymbolReference(), nextParmNum);
               parmNum = nextParmNum++;
               }
            ir.print("  %%%d = %s %s* %%p%d.addr, align %d\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   parmNum,
                   node->getSize());
            }
         }
      else
         {
         // assume SM35 or more
         static bool disableReadOnlyCacheArray = (feGetEnv("TR_disableGPUReadOnlyCacheArray") != NULL);
         bool isReadOnlyArray = false;
         if (node->getSymbolReference()->getSymbol()->isArrayShadowSymbol() &&
             // I disabled to generate ld.global.nc for read-only address array
             // I do not know an intrinsic function name for ld.global.nc of address
             node->getDataType() != TR::Address &&
             !disableReadOnlyCacheArray &&
             _gpuCanUseReadOnlyCache)
            {
            TR::Node *addrNode = node->getFirstChild();
            if (addrNode->getOpCodeValue() == TR::aiadd || addrNode->getOpCodeValue() == TR::aladd)
               {
               addrNode = addrNode->getFirstChild();
               }
            if ((addrNode->getOpCodeValue() == TR::aload) || (addrNode->getOpCodeValue() == TR::aloadi))
               {
               TR::SymbolReference *symRef = addrNode->getSymbolReference();
               int32_t symRefIndex = symRef->getReferenceNumber();
               CS2::ArrayOf<gpuMapElement, TR::Allocator> &gpuSymbolMap = self()->comp()->cg()->_gpuSymbolMap;

               int32_t nc = symRefIndex;
               TR::SymbolReference *hostSymRef = gpuSymbolMap[nc]._hostSymRef;
               int32_t parmSlot = gpuSymbolMap[nc]._parmSlot;

               if (!hostSymRef || parmSlot == -1)
                  {
                  TR::Node *tempNode = gpuSymbolMap[nc]._node;
                  if (tempNode && (tempNode->getOpCodeValue() == TR::astore) && (tempNode->getFirstChild()->getOpCodeValue() == TR::aloadi))
                     {
                     TR::Node *parmNode = tempNode->getFirstChild();
                     nc = parmNode->getSymbolReference()->getReferenceNumber();

                     hostSymRef = gpuSymbolMap[nc]._hostSymRef;
                     parmSlot = gpuSymbolMap[nc]._parmSlot;
                     }
                  }
               else if (hostSymRef->getReferenceNumber() != symRefIndex)
                  {
                  hostSymRef = NULL;
                  }

               if (hostSymRef && (parmSlot != -1) &&
                   (gpuSymbolMap[nc]._accessKind & TR::CodeGenerator::ReadWriteAccesses) == TR::CodeGenerator::ReadAccess)
                  {
                  isReadOnlyArray = true;
                  }
               }
            }

         ir.print("  %%%d = bitcast %s %s to %s %s*\n",
                      _gpuNodeCount-1,
                      getTypeName(firstChild->getDataType()),
                      name0,
                      getTypeName(node->getDataType()),
                      firstChild->chkSharedMemory() ? "addrspace(3)" : isReadOnlyArray ? "addrspace(1)" : "");

         node->setLocalIndex(_gpuNodeCount++);
         traceMsg(self()->comp(), "node %p assigned index %d\n", node, node->getLocalIndex());

         //NVVM 1.3 onward uses a two parameter version of ldg
         if (isReadOnlyArray)
            {
            if (_gpuUseOldLdgCalls)
               {
               ir.print("  %%%d = tail call %s @llvm.nvvm.ldg.global.%s.%s.p1%s(%s addrspace(1)* %%%d), !align !1%d\n",
                        node->getLocalIndex(),
                        getTypeName(node->getDataType()),
                        (node->getDataType() >= TR::Float) ? "f" : "i",
                        getVarTypeName(node->getDataType()),
                        getVarTypeName(node->getDataType()),
                        getTypeName(node->getDataType()),
                        _gpuNodeCount-2,
                        node->getSize());
               }
            else
               {
               ir.print("  %%%d = tail call %s @llvm.nvvm.ldg.global.%s.%s.p1%s(%s addrspace(1)* %%%d, i32 %d)\n",
                        node->getLocalIndex(),
                        getTypeName(node->getDataType()),
                        (node->getDataType() >= TR::Float) ? "f" : "i",
                        getVarTypeName(node->getDataType()),
                        getVarTypeName(node->getDataType()),
                        getTypeName(node->getDataType()),
                        _gpuNodeCount-2,
                        node->getSize());
               }
            }
         else
            // e.g. %32 = load i32 addrspace(4) * %31, align 4
            ir.print("  %%%d = %s %s %s * %%%d, align %d\n",
                     node->getLocalIndex(),
                     getOpCodeName(node->getOpCodeValue()),
                     getTypeName(node->getDataType()),
                     firstChild->chkSharedMemory() ? "addrspace(3)" : "",
                     _gpuNodeCount-2,
                     node->getSize());
         }
      }
   else if (node->getOpCode().isCall() &&
            ((TR::MethodSymbol*)node->getSymbolReference()->getSymbol())->getRecognizedMethod() != TR::unknownMethod &&
            self()->handleRecognizedMethod(node, ir, self()->comp()))
      {
      }
   else if (node->getOpCodeValue() == TR::arraycopy)
      {
      self()->printArrayCopyNVVMIR(node, ir, self()->comp());
      }
   else if (node->getOpCode().isCall())
      {
      traceMsg(self()->comp(), "unrecognized call %p\n", node);
      return GPUInvalidProgram;
      }
   else if (node->getOpCode().isStoreDirect() &&
            node->getSymbolReference()->getSymbol()->getRecognizedField() != TR::Symbol::UnknownField)
      {
      switch (node->getSymbolReference()->getSymbol()->getRecognizedField())
         {
         case TR::Symbol::Com_ibm_gpu_Kernel_syncThreads:
             ir.print("  call void @llvm.nvvm.barrier0()\n");
             break;
         default:
             break;
         }
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCode().isLoadVarDirect() &&
            node->getSymbolReference()->getSymbol()->getRecognizedField() != TR::Symbol::UnknownField &&
            self()->handleRecognizedField(node, ir))
      {
      }
   else if (node->getOpCode().isLoadVarDirect())
      {
      if (!node->getSymbol()->isAutoOrParm())
         {
         traceMsg(self()->comp(), "unexpected symbol in node %p\n");
         return GPUInvalidProgram;
         }

      getAutoOrParmName(node->getSymbol(), name0);

      ir.print("  %%%d = %s %s* %s, align %d\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   name0,
                   node->getSize());
      }
   else if (node->getOpCode().isStoreDirect())
      {
      if (!node->getSymbol()->isAutoOrParm())
         {
         traceMsg(self()->comp(), "unexpected symbol in node %p\n");
         return GPUInvalidProgram;
         }

      getNodeName(node->getChild(0), name0, self()->comp());
      getAutoOrParmName(node->getSymbol(), name1);

      ir.print("  %s %s %s, %s* %s, align %d\n",
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   getTypeName(node->getDataType()),
                   name1,
                   node->getChild(0)->getSize());

      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCode().isArrayRef())
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      ir.print("  %%%d = %s inbounds %s %s, %s %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   getTypeName(node->getChild(1)->getDataType()), name1);

      if (node->getChild(0)->chkSharedMemory())
          node->setSharedMemory(true);
      }
   else if (node->getOpCodeValue() == TR::arraylength)
      {
      // assume SM35 or more
      static bool disableReadOnlyCacheObjHdr = (feGetEnv("TR_disableGPUReadOnlyCacheObjHdr") != NULL);
      getNodeName(node->getChild(0), name0, self()->comp());

      ir.print("  %%%d = getelementptr inbounds i8* %s, i32 %d\n",
                   node->getLocalIndex(),
                   name0,
                   self()->objectLengthOffset());

      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = bitcast i8* %%%d to i32 %s*\n",
               node->getLocalIndex(),
               node->getLocalIndex() - 1,
               (_gpuCanUseReadOnlyCache && !disableReadOnlyCacheObjHdr) ? "addrspace(1)" : "");

      node->setLocalIndex(_gpuNodeCount++);

      //NVVM 1.3 onward uses a two parameter version of ldg
      if (_gpuCanUseReadOnlyCache && !disableReadOnlyCacheObjHdr)
         {
         if (_gpuUseOldLdgCalls)
            {
            ir.print("  %%%d = tail call i32 @llvm.nvvm.ldg.global.i.i32.p1i32(i32 addrspace(1)* %%%d), !align !14\n",
                     node->getLocalIndex(),
                     node->getLocalIndex() - 1);
            }
         else
            {
            ir.print("  %%%d = tail call i32 @llvm.nvvm.ldg.global.i.i32.p1i32(i32 addrspace(1)* %%%d, i32 4)\n",
                     node->getLocalIndex(),
                     node->getLocalIndex() - 1);
            }
         }
      else
         ir.print("  %%%d = load i32* %%%d, align 4\n",
                  node->getLocalIndex(),
                  node->getLocalIndex() - 1);
      }
   // Binary Operations
   else if ((node->getOpCodeValue() == TR::lshl ||
             node->getOpCodeValue() == TR::lshr) &&
            node->getChild(1)->getDataType() == TR::Int32)
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      ir.print("  %%%d = sext i32 %s to i64\n",
               node->getLocalIndex(),
               name1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = %s %s %s, %%%d\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   name0, _gpuNodeCount-2);

      }
   else if (node->getOpCodeValue() == TR::imulh || node->getOpCodeValue() == TR::iumulh ||
            node->getOpCodeValue() == TR::lmulh || node->getOpCodeValue() == TR::lumulh)
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      bool isLongMul = node->getOpCodeValue() == TR::lmulh || node->getOpCodeValue() == TR::lumulh;
      bool isSignedMul = node->getOpCodeValue() == TR::imulh || node->getOpCodeValue() == TR::lmulh;

      bool extendChild0 = isLongMul || (node->getChild(0)->getDataType() != TR::Int64);
      bool extendChild1 = isLongMul || (node->getChild(1)->getDataType() != TR::Int64);

      if (extendChild0)
         {
         ir.print("  %%%d = %s %s %s to %s\n",
                      node->getLocalIndex(),
                      isSignedMul ? "sext" : "zext",
                      getTypeName(node->getChild(0)->getDataType()),
                      name0,
                      isLongMul ? "i128" : "i64");
         node->setLocalIndex(_gpuNodeCount++);
         }
      else
         {
         ir.print("  %%%d = lshr i64 %s, 0\n",
                      node->getLocalIndex(),
                      name0);
         node->setLocalIndex(_gpuNodeCount++);
         }

      if(extendChild1)
         {
         ir.print("  %%%d = %s %s %s to %s\n",
                      node->getLocalIndex(),
                      isSignedMul ? "sext" : "zext",
                      getTypeName(node->getChild(1)->getDataType()),
                      name1,
                      isLongMul ? "i128" : "i64");
         node->setLocalIndex(_gpuNodeCount++);
         }
      else
         {
         ir.print("  %%%d = lshr i64 %s, 0\n",
                      node->getLocalIndex(),
                      name1);
         node->setLocalIndex(_gpuNodeCount++);
         }

      ir.print("  %%%d = mul %s %%%d, %%%d\n",
                   node->getLocalIndex(),
                   isLongMul ? "i128" : "i64",
                   node->getLocalIndex()-2,
                   node->getLocalIndex()-1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = lshr %s %%%d, %s\n",
                   node->getLocalIndex(),
                   isLongMul ? "i128" : "i64",
                   node->getLocalIndex()-1,
                   isLongMul ? "64" : "32");
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = trunc %s %%%d to %s\n",
                   node->getLocalIndex(),
                   isLongMul ? "i128" : "i64",
                   node->getLocalIndex()-1,
                   isLongMul ? "i64" : "i32");
      }
   else if (node->getOpCodeValue() == TR::bneg || node->getOpCodeValue() == TR::sneg ||
            node->getOpCodeValue() == TR::ineg || node->getOpCodeValue() == TR::lneg ||
            node->getOpCodeValue() == TR::fneg || node->getOpCodeValue() == TR::dneg)
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      bool isFloatDouble = node->getOpCodeValue() == TR::fneg || node->getOpCodeValue() == TR::dneg;

      ir.print("  %%%d = %s %s %s, %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   isFloatDouble ? "0.0" : "0",
                   name0);
      }
   else if (node->getOpCodeValue() == TR::iabs || node->getOpCodeValue() == TR::labs)
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      bool isInt = node->getOpCodeValue() == TR::iabs;

      ir.print("  %%%d = ashr %s %s, %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   name0,
                   isInt ? "31" : "63");
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = xor %s %s, %%%d\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   name0,
                   node->getLocalIndex()-1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = sub %s %%%d, %%%d\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   node->getLocalIndex()-1,
                   node->getLocalIndex()-2);
      }
   else if (node->getOpCodeValue() == TR::irol || node->getOpCodeValue() == TR::lrol)
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      bool isInt = node->getOpCodeValue() == TR::irol;

      ir.print("  %%%d = shl %s %s, %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   name0, name1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = sub %s %s, %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1)->getDataType()),
                   isInt ? "32" : "64",
                   name1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = and %s %%%d, %s\n",
                   node->getLocalIndex(),
                   getTypeName(node->getChild(1)->getDataType()),
                   node->getLocalIndex()-1,
                   isInt ? "31" : "63");
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = lshr %s %s, %%%d\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   name0,
                   node->getLocalIndex()-1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = or %s %%%d, %%%d\n",
                   node->getLocalIndex(),
                   getTypeName(node->getDataType()),
                   node->getLocalIndex()-4,
                   node->getLocalIndex()-1);
      }
   else if (node->getOpCodeValue() == TR::ibits2f || node->getOpCodeValue() == TR::fbits2i ||
            node->getOpCodeValue() == TR::lbits2d || node->getOpCodeValue() == TR::dbits2l)
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      ir.print("  %%%d = %s %s %s to %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   getTypeName(node->getDataType()));
      }
   else if (node->getOpCode().isArithmetic() &&
            node->getNumChildren() == 2 &&
            getOpCodeName(node->getOpCodeValue()))   // supported binary opcode
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());

      ir.print("  %%%d = %s %s %s, %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getDataType()),
                   name0, name1);
      }
   else if (node->getOpCode().isConversion() &&
            getOpCodeName(node->getOpCodeValue()))
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      ir.print("  %%%d = %s %s %s to %s\n",
                   node->getLocalIndex(),
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   getTypeName(node->getDataType()));
      }
   else if (node->getOpCode().isIf())
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());
      const char *type0 = getTypeName(node->getChild(0)->getDataType());

      const char *opcode = getOpCodeName(node->getOpCodeValue());

      ir.print("  %%%d = %s %s %s, %s\n",
                   node->getLocalIndex(), opcode, type0, name0, name1);
      ir.print("  br i1 %%%d, label %%block_%d, label %%block_%d\n",
                   node->getLocalIndex(),
                   node->getBranchDestination()->getNode()->getBlock()->getNumber(),
                   _gpuCurrentBlock->getNextBlock()->getNumber());
      }
   else if (node->getOpCodeValue() == TR::Goto)
      {
      ir.print("  %s label %%block_%d\n",
                   getOpCodeName(node->getOpCodeValue()),
                   node->getBranchDestination()->getNode()->getBlock()->getNumber());
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCodeValue() == TR::lookup)
      {
      getNodeName(node->getChild(0), name0, self()->comp());

      ir.print("  %s %s %s, label %%block_%d [ ",
                   getOpCodeName(node->getOpCodeValue()),
                   getTypeName(node->getChild(0)->getDataType()),
                   name0,
                   node->getChild(1)->getBranchDestination()->getNode()->getBlock()->getNumber()
                   );
      for(int i=2; i < node->getNumChildren(); ++i)
         {
         ir.print("%s %d, label %%block_%d ",
                      getTypeName(node->getChild(0)->getDataType()),
                      node->getChild(i)->getCaseConstant(),
                      node->getChild(i)->getBranchDestination()->getNode()->getBlock()->getNumber());
         }
      ir.print("]\n");
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (node->getOpCodeValue() == TR::table)
         {
         getNodeName(node->getChild(0), name0, self()->comp());

         ir.print("  %s %s %s, label %%block_%d [ ",
                      getOpCodeName(node->getOpCodeValue()),
                      getTypeName(node->getChild(0)->getDataType()),
                      name0,
                      node->getChild(1)->getBranchDestination()->getNode()->getBlock()->getNumber()
                      );
         for(int i=2; i < node->getNumChildren(); ++i)
            {
            ir.print("%s %d, label %%block_%d ",
                         getTypeName(node->getChild(0)->getDataType()),
                         i-2,
                         node->getChild(i)->getBranchDestination()->getNode()->getBlock()->getNumber());
            }
         ir.print("]\n");
         node->setLocalIndex(_gpuNodeCount--);
         }
   else if (node->getOpCode().isBooleanCompare()) //Needs to be after "isIf()" check
      {
      getNodeName(node->getChild(0), name0, self()->comp());
      getNodeName(node->getChild(1), name1, self()->comp());
      const char *type0 = getTypeName(node->getChild(0)->getDataType());

      const char *opcode = getOpCodeName(node->getOpCodeValue());

      ir.print("  %%%d = %s %s %s, %s\n",
                   node->getLocalIndex(), opcode, type0, name0, name1);
      node->setLocalIndex(_gpuNodeCount++);

      ir.print("  %%%d = zext i1 %%%d to i32\n",
                   node->getLocalIndex(),
                   node->getLocalIndex()-1);
      }
   else if (node->getOpCodeValue() == TR::treetop || node->getOpCodeValue() == TR::Case)
      {
      node->setLocalIndex(_gpuNodeCount--);
      }
   else if (getOpCodeName(node->getOpCodeValue()) &&
            strcmp(getOpCodeName(node->getOpCodeValue()), "INVALID") == 0)
      {
      node->setLocalIndex(_gpuNodeCount--);
      traceMsg(self()->comp(), "INVALID operation required by node %p\n", node);
      return GPUInvalidProgram;
      }
   else
      {
      node->setLocalIndex(_gpuNodeCount--);
      traceMsg(self()->comp(), "node %p assigned index %d\n", node, node->getLocalIndex());
      traceMsg(self()->comp(), "unsupported opcode (%s) on line %d %p\n", node->getOpCode().getName(), self()->comp()->getLineNumber(node), node);
      return GPUInvalidProgram;
      }

   return GPUSuccess;
   }


void traceNVVMIR(TR::Compilation *comp, char *buffer)
   {
   traceMsg(comp, "NVVM IR:\n");
   char msg[256];
   char *cs = buffer;
   int line = 1;
   while (*cs != '\0')
      {
      char *ce = cs;
      while (*ce != '\n' && *ce != '\0')
         {
         ce++;
         }
      ce++;
      int len = (ce - cs) < 255 ? (ce - cs) : 255;
      memcpy(msg, cs, len);
      msg[len] = '\0';
      traceMsg(comp, "%6d: %s", line++, msg);
      if (*(ce - 1) == '\0')
         {
         ce--;
         }
      cs = ce;
      }
   traceMsg(comp, "\n");
   }


void
J9::CodeGenerator::findExtraParms(
      TR::Node *node,
      int32_t &numExtraParms,
      TR_SharedMemoryAnnotations *sharedMemory,
      vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (node->getOpCode().isLoadIndirect() &&
       _gpuSymbolMap[node->getSymbolReference()->getReferenceNumber()]._parmSlot == -1)
      {
      TR::Node *firstChild = node->getChild(0);
      if (node->getSymbolReference()->getCPIndex() != -1) // field of some object
         {
         // TODO: check that field is an array!
         TR_ASSERT(firstChild->getOpCode().isLoadDirect() &&
                   isThisPointer(firstChild->getSymbolReference()),
                   "can only access a field of this object\n");

         // TODO: handle duplicate names from different classes
         TR_SharedMemoryField field = sharedMemory->find(TR::comp(), node->getSymbolReference());

         if (field.getSize() == 0)
            numExtraParms++;
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      self()->findExtraParms(child, numExtraParms, sharedMemory, visitCount);
      }
   }


void
J9::CodeGenerator::dumpInvariant(
      CS2::ArrayOf<gpuParameter, TR::Allocator>::Cursor pit,
      NVVMIRBuffer &ir,
      bool isbufferalign)
   {
   return;

   for (pit.SetToFirst(); pit.Valid(); pit.SetToNext())
      {
      TR::Symbol *sym = pit->_hostSymRef->getSymbol();
      char parmName[MAX_NAME+2];
      getParmName(pit->_parmSlot, parmName, false);
      if (sym->getDataType() == TR::Address)
         {
         if (isbufferalign)
            strcat(parmName, ".t");
         ir.print("  call void @llvm.invariant.end({}* %%inv_%s_header, i64 %d, i8* %s)\n",
                                         &parmName[1], self()->objectHeaderInvariant(), parmName);
         }
      }
   }

#ifdef ENABLE_GPU
bool calculateComputeCapability(int tracing, short* computeMajor, short* computeMinor, int deviceId);
bool getNvvmVersion(int tracing, int* majorVersion, int* minorVersion);
#endif

TR::CodeGenerator::GPUResult
J9::CodeGenerator::dumpNVVMIR(
      TR::TreeTop *firstTreeTop,
      TR::TreeTop *lastTreeTop,
      TR_RegionStructure *loop,
      SharedSparseBitVector *blocksInLoop,
      ListBase<TR::AutomaticSymbol> *autos,
      ListBase<TR::ParameterSymbol> *parms,
      bool staticMethod,
      char * &nvvmIR,
      TR::Node * &errorNode,
      int gpuPtxCount,
      bool* hasExceptionChecks)
   {
   static bool isbufferalign = feGetEnv("TR_disableGPUBufferAlign") ? false : true;
   NVVMIRBuffer ir(self()->comp()->trMemory());
   GPUResult result;
   short computeMajor, computeMinor, computeCapability;
   int nvvmMajorVersion = 0;
   int nvvmMinorVersion = 0;

   _gpuHasNullCheck = false;
   _gpuHasBndCheck = false;
   _gpuHasDivCheck = false;
   _gpuNodeCount = 0;
   _gpuReturnType = TR::NoType;
   _gpuPostDominators = NULL;
   _gpuStartBlock = NULL;
   _gpuNeedNullCheckArguments_vector = 0;
   _gpuCanUseReadOnlyCache = false;
   _gpuUseOldLdgCalls = false;

#ifdef ENABLE_GPU
   if (!calculateComputeCapability(/*tracing*/0, &computeMajor, &computeMinor, /*deviceId*/0))
      {
      traceMsg(self()->comp(), "calculateComputeCapability was unsuccessful.\n");
      return GPUHelperError;
      }
   computeCapability = 100*computeMajor + computeMinor; //combines Major and Minor versions into a single number.

   if (computeCapability >= 305) //If compute capability is 3.5 or higher
      _gpuCanUseReadOnlyCache = true; //then the GPU is capable of using read only cache

   if (!getNvvmVersion(/*tracing*/0, &nvvmMajorVersion, &nvvmMinorVersion))
      {
      traceMsg(self()->comp(), "getNvvmVersion was unsuccessful.\n");
      return GPUHelperError;
      }

   /*
    * NVVM 1.3 updates LLVM support to LLVM 3.8. From LLVM 3.6 onward, ldg was changed to make alignment an explicit
    * parameter instead of as metadata. As a result, NVVM 1.2 and before uses a one parameter version of ldg while
    * NVVM 1.3 and onward uses a two parameter version.
    */
   if (nvvmMajorVersion == 1 && nvvmMinorVersion <= 2)
      {
      _gpuUseOldLdgCalls = true;
      }
#endif

   TR::CFG *cfg = self()->comp()->getFlowGraph();
   TR_BitVector targetBlocks(cfg->getNumberOfNodes(), self()->comp()->trMemory(), stackAlloc, growable);

   static bool enableExceptionCheckElimination = (feGetEnv("TR_enableGPUExceptionCheckElimination") != NULL);
   if (enableExceptionCheckElimination)
      {
      _gpuPostDominators = new (self()->comp()->trStackMemory()) TR_Dominators(self()->comp(), true);
      }
   _gpuStartBlock = toBlock(cfg->getStart());


   TR_SharedMemoryAnnotations sharedMemory(self()->comp());
   int32_t numExtraParms = 0;

   // First pass through the trees
   vcount_t visitCount = self()->comp()->incVisitCount();

   int32_t currentBlock = 0;
   int32_t firstBlock = 0;
   for (TR::TreeTop * tree = firstTreeTop; tree != lastTreeTop; tree = tree->getNextTreeTop())
      {
      if (tree->getNode()->getOpCodeValue() == TR::BBStart)
          currentBlock = tree->getNode()->getBlock()->getNumber();

      if (blocksInLoop && !blocksInLoop->ValueAt(currentBlock))
          continue;

      if (firstBlock == 0)
          firstBlock = currentBlock;

      TR::Node *node = tree->getNode();
      if (node->getOpCode().isBranch())
         {
         TR_ASSERT(node->getBranchDestination()->getNode()->getOpCodeValue() == TR::BBStart, "Attempted to get Block number of a non-BBStart node.");
         targetBlocks.set(node->getBranchDestination()->getNode()->getBlock()->getNumber());

         if (tree->getNextTreeTop() &&
             tree->getNextTreeTop()->getNextTreeTop())
            {
            node = tree->getNextTreeTop()->getNextTreeTop()->getNode();
            TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Attempted to get Block number of a non-BBStart node.");
            targetBlocks.set(node->getBlock()->getNumber());
            }
         }
      else if (node->getOpCode().isSwitch())
         {
         for (int childIndex = 0; childIndex < node->getNumChildren(); ++childIndex)
            {
            if (node->getChild(childIndex)->getOpCode().isBranch())
               {
               TR_ASSERT(node->getChild(childIndex)->getBranchDestination()->getNode()->getOpCodeValue() == TR::BBStart, "Attempted to get Block number of a non-BBStart node.");
               targetBlocks.set(node->getChild(childIndex)->getBranchDestination()->getNode()->getBlock()->getNumber());
               }
            }
         }
      else if (node->getOpCode().isReturn())
         _gpuReturnType = node->getDataType().getDataType();

      //findExtraParms(node, numExtraParms, &sharedMemory, visitCount);
      }

   traceMsg(self()->comp(), "extra parameters = %d\n", numExtraParms);
   ir.print("target triple = \"nvptx64-unknown-cuda\"\n");
   ir.print("target datalayout = \"e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v16:16:16-v32:32:32-v64:64:64-v128:128:128-n16:32:64\"\n\n");  // TODO: 32-bit


   // TODO: alignment, arraylength !!!
   for(auto lit = sharedMemory.getSharedMemoryFields().begin(); lit != sharedMemory.getSharedMemoryFields().end(); ++lit)
       {
       if ((*lit).getSize() > 0)
          ir.print("@%.*s = internal addrspace(3) global [%d x %s] zeroinitializer, align 8\n",
        		  (*lit).getFieldNameLength(), (*lit).getFieldName(), (*lit).getSize(),
                     getTypeNameFromSignature((*lit).getFieldSig(), (*lit).getFieldSigLength()));
       }

   static bool disableReadOnlyCacheArray = (feGetEnv("TR_disableGPUReadOnlyCacheArray") != NULL);
   static bool disableReadOnlyCacheObjHdr = (feGetEnv("TR_disableGPUReadOnlyCacheObjHdr") != NULL);

   //ir.print("@_ExceptionKind = addrspace(1) global [1 x i32 0, align 4\n");
   ir.print("@_ExceptionKind = addrspace(1) global [1 x i32] zeroinitializer, align 4\n");

   //NVVM 1.3 onward uses a two parameter version of ldg
   if (_gpuCanUseReadOnlyCache && (!disableReadOnlyCacheArray || !disableReadOnlyCacheObjHdr))
      {
      if (_gpuUseOldLdgCalls)
         {
         ir.print("declare i8  @llvm.nvvm.ldg.global.i.i8.p1i8(i8 addrspace(1)* %%ptr)\n");
         ir.print("declare i16 @llvm.nvvm.ldg.global.i.i16.p1i16(i16 addrspace(1)* %%ptr)\n");
         ir.print("declare i32 @llvm.nvvm.ldg.global.i.i32.p1i32(i32 addrspace(1)* %%ptr)\n");
         ir.print("declare i64 @llvm.nvvm.ldg.global.i.i64.p1i64(i64 addrspace(1)* %%ptr)\n");
         ir.print("declare float  @llvm.nvvm.ldg.global.f.f32.p1f32(float addrspace(1)* %%ptr)\n");
         ir.print("declare double @llvm.nvvm.ldg.global.f.f64.p1f64(double addrspace(1)* %%ptr)\n");
         ir.print("declare i8*    @llvm.nvvm.ldg.global.p.p64.p1p64(i8* addrspace(1)* %%ptr)\n");
         }
      else
         {
         ir.print("declare i8  @llvm.nvvm.ldg.global.i.i8.p1i8(i8 addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare i16 @llvm.nvvm.ldg.global.i.i16.p1i16(i16 addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare i32 @llvm.nvvm.ldg.global.i.i32.p1i32(i32 addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare i64 @llvm.nvvm.ldg.global.i.i64.p1i64(i64 addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare float  @llvm.nvvm.ldg.global.f.f32.p1f32(float addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare double @llvm.nvvm.ldg.global.f.f64.p1f64(double addrspace(1)* %%ptr, i32 %%align)\n");
         ir.print("declare i8*    @llvm.nvvm.ldg.global.p.p64.p1p64(i8* addrspace(1)* %%ptr, i32 %%align)\n");
         }
      }

   ir.print("declare {}* @llvm.invariant.start(i64 %%size, i8* nocapture %%ptr)\n");
   ir.print("declare void @llvm.invariant.end({}* %%start, i64 %%size, i8* nocapture %%ptr)\n");

   ir.print("\ndefine %s @test%d(", getTypeName(_gpuReturnType), gpuPtxCount);

   CS2::ArrayOf<gpuParameter, TR::Allocator> gpuParameterMap(TR::comp()->allocator());
   CS2::ArrayOf<TR::CodeGenerator::gpuMapElement, TR::Allocator>::Cursor ait(_gpuSymbolMap);

   for (ait.SetToFirst(); ait.Valid(); ait.SetToNext())
      {
      if (!ait->_hostSymRef) continue;
      traceMsg(TR::comp(), "hostSymRef #%d parmSlot %d\n", (int)ait, ait->_parmSlot);

      if (ait->_parmSlot != -1)
         {
         gpuParameter parm (ait->_hostSymRef, ait->_parmSlot);
         gpuParameterMap[ait->_parmSlot] = parm;
         }
      }


   TR::ResolvedMethodSymbol *method = self()->comp()->getJittedMethodSymbol();
   ListIterator<TR::ParameterSymbol> pi(parms);
   TR::ParameterSymbol *parm;

   bool first = true;
   int32_t nextParmNum = staticMethod ? 0 : 1;

   parm = pi.getFirst();
   if (!staticMethod) parm = pi.getNext();

   char name[MAX_NAME];

   for (; parm; parm = pi.getNext())
      {
      getAutoOrParmName(parm, name, false);

      if (!first) ir.print(", ");
      ir.print("%s %s", getTypeName(parm->getDataType()), name);
      first = false;
      nextParmNum++;
      }


   CS2::ArrayOf<gpuParameter, TR::Allocator>::Cursor pit(gpuParameterMap);
   for (pit.SetToFirst(); pit.Valid(); pit.SetToNext())
      {
      getParmName(pit->_parmSlot, name, false);

      if (!first) ir.print(", ");
      ir.print("%s %s", getTypeName(pit->_hostSymRef->getSymbol()->getDataType()), name);
      first = false;
      nextParmNum++;
      }

   int numParms = nextParmNum  - (staticMethod ? 0 : 1);

   ir.print("%s%s%s %%ExceptionKind",
            numParms > 0 ? ", " : "",
            self()->comp()->isGPUCompilation() ? "" : "i32 %startInclusive, i32 %endExclusive, ",
            getTypeName(TR::Address));

   ir.print(") {\n");
   ir.print("entry:\n");

   pi.reset();
   parm = pi.getFirst();
   if (!staticMethod) parm = pi.getNext();

   first = true;
   for (; parm; parm = pi.getNext())
      {
      char name[MAX_NAME];
      getAutoOrParmName(parm, name);
      ir.print("  %s = alloca %s, align %d\n",
                          name,
                          getTypeName(parm->getDataType()),
                          parm->getSize());

      char origName[MAX_NAME];
      getAutoOrParmName(parm, origName, false);
      ir.print("  store %s %s, %s* %s, align %d\n",
                          getTypeName(parm->getDataType()),
                          origName,
                          getTypeName(parm->getDataType()),
                          name,
                          parm->getSize());
      }


   for (pit.SetToFirst(); pit.Valid(); pit.SetToNext())
      {
      TR::Symbol *sym = pit->_hostSymRef->getSymbol();
      char addrName[MAX_NAME+2];
      getParmName(pit->_parmSlot, addrName);
      ir.print("  %s = alloca %s, align %d\n",
                          addrName,
                          getTypeName(sym->getDataType()),
                          sym->getSize());

      char parmName[MAX_NAME];
      getParmName(pit->_parmSlot, parmName, false);
      if (sym->getDataType() == TR::Address)
         {
         if (isbufferalign)
            {
            char name[MAX_NAME];
            strcpy(name, parmName);
            strcat(parmName, ".t");
            ir.print("  %s = getelementptr inbounds i8* %s, i32 %d\n",
                     parmName,
                     name,
                     GPUAlignment - TR::Compiler->om.contiguousArrayHeaderSizeInBytes());
            }
         ir.print("  %%inv_%s_header = call {}* @llvm.invariant.start(i64 %d, i8* %s)\n",
                                        &parmName[1], self()->objectHeaderInvariant(), parmName);
         }
      ir.print("  store %s %s, %s* %s, align %d\n",
                          getTypeName(sym->getDataType()),
                          parmName,
                          getTypeName(sym->getDataType()),
                          addrName,
                          sym->getSize());

      }


   ListIterator<TR::AutomaticSymbol> ai(autos);
   uint16_t liveLocalIndex = 0;
   for (TR::AutomaticSymbol *a = ai.getFirst(); a != NULL; a = ai.getNext())
      {
      ir.print("  %%a%d.addr = alloca %s, align %d\n",
                  liveLocalIndex,
                  getTypeName(a->getDataType()),
                  a->getSize());
      a->setLiveLocalIndex(liveLocalIndex++, 0);
      }


   if (!self()->comp()->isGPUCompilation())
      {
      ir.print("  %%0 = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()\n");
      ir.print("  %%1 = call i32 @llvm.nvvm.read.ptx.sreg.ntid.x()\n");
      ir.print("  %%2 = mul i32 %%0, %%1\n");
      ir.print("  %%3 = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()\n");
      ir.print("  %%4 = add i32 %%2, %%3\n");
      ir.print("  %%5 = add i32 %%startInclusive, %%4\n");
      ir.print("  store i32 %%5, i32* %%%s.addr, align 4\n", "a0");
      ir.print("  %%6 = icmp slt i32 %%5, %%endExclusive\n");
      ir.print("  br i1 %%6, label %%block_%d, label %%block_0\n", firstBlock);
      ir.print("block_0:\n");
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");

      _gpuNodeCount = 7;
      }

   // print all trees
   visitCount = self()->comp()->incVisitCount();

   for (TR::TreeTop * tree = firstTreeTop; tree != lastTreeTop; tree = tree->getNextTreeTop())
      {
      TR::Node *node = tree->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
          currentBlock = node->getBlock()->getNumber();

      if (blocksInLoop && !blocksInLoop->ValueAt(currentBlock))
          continue;

      // don't print the backedge
      if (node->getOpCode().isBranch() &&
          node->getBranchDestination()->getNode()->getBlock()->getNumber() == firstBlock)
          {
          self()->dumpInvariant(pit, ir, isbufferalign);
          ir.print("  ret void\n");
          continue;
          }

      result = self()->printNVVMIR(ir, tree->getNode(), loop, &targetBlocks, visitCount, &sharedMemory, nextParmNum, errorNode);
      if (result != GPUSuccess)
         {
         return result;
         }
      }

   if (_gpuReturnType == TR::NoType)
      {
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");
      }

   _gpuNodeCount++;

   if (_gpuNeedNullCheckArguments_vector != 0)
      {
      ir.print("; needNullCheckArguments_vector=");
      int32_t len = sizeof(uint64_t) * CHAR_BIT;
      for (int32_t i = len - 1; i >= 0; i--)
         {
         ir.print("%u", (_gpuNeedNullCheckArguments_vector >> (uint64_t)i) & 1);
         }
      ir.print("\n");
      }

   if (_gpuHasNullCheck)
      {
      ir.print("NullException:\n");
      //ir.print("  store i32 1, i32 addrspace(1)* @_ExceptionKind, align 4\n");
      ir.print("  %%%d = bitcast i8* %%ExceptionKind to i32*\n", _gpuNodeCount);
      ir.print("  store i32 %d, i32 * %%%d, align 4\n", GPUNullCheck, _gpuNodeCount++);
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");
      }
   if (_gpuHasBndCheck)
      {
      ir.print("BndException:\n");
      //ir.print("  store i32 2, i32 addrspace(1)* @_ExceptionKind, align 4\n");
      ir.print("  %%%d = bitcast i8* %%ExceptionKind to i32*\n", _gpuNodeCount);
      ir.print("  store i32 %d, i32 * %%%d, align 4\n", GPUBndCheck, _gpuNodeCount++);
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");
      }
   if (_gpuHasDivCheck)
      {
      ir.print("DivException:\n");
      //ir.print("  store i32 3, i32 addrspace(1)* @_ExceptionKind, align 4\n");
      ir.print("  %%%d = bitcast i8* %%ExceptionKind to i32*\n", _gpuNodeCount);
      ir.print("  store i32 %d, i32 * %%%d, align 4\n", GPUDivException, _gpuNodeCount++);
      self()->dumpInvariant(pit, ir, isbufferalign);
      ir.print("  ret void\n");
      }

   ir.print("}\n");

   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ctaid.x() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ctaid.y() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ctaid.z() nounwind readnone\n");

   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ntid.x() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ntid.y() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.ntid.z() nounwind readnone\n");

   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.tid.x() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.tid.y() nounwind readnone\n");
   ir.print("declare i32 @llvm.nvvm.read.ptx.sreg.tid.z() nounwind readnone\n");

   if (self()->comp()->getOptions()->getEnableGPU(TR_EnableGPUEnableMath))
      {
      ir.print("declare double @__nv_sin(double)\n");
      ir.print("declare double @__nv_cos(double)\n");
      ir.print("declare double @__nv_sqrt(double)\n");
      ir.print("declare double @__nv_log(double)\n");
      ir.print("declare double @__nv_exp(double)\n");
      ir.print("declare double @__nv_fabs(double)\n");
      ir.print("declare float @__nv_fabsf(float)\n");
      }

   ir.print("declare void @llvm.nvvm.barrier0() nounwind readnone\n");

   ir.print("!10 = metadata !{i32    0}\n");
   ir.print("!11 = metadata !{i32    1}\n");
   ir.print("!12 = metadata !{i32    2}\n");
   ir.print("!14 = metadata !{i32    4}\n");
   ir.print("!18 = metadata !{i32    8}\n");

   ir.print("!nvvmir.version = !{!0}\n");
   ir.print("!0 = metadata !{i32 1, i32 0}\n");

   ir.print("!nvvm.annotations = !{!1}\n");
   ir.print("!1 = metadata !{%s (", getTypeName(_gpuReturnType));
   pi.reset();
   parm = pi.getFirst();
   if (!staticMethod) parm = pi.getNext();

   first = true;
   for (; parm; parm = pi.getNext())
      {
      if (!first) ir.print(", ");
      ir.print("%s", getTypeName(parm->getDataType()));
      first = false;
      }

   for (pit.SetToFirst(); pit.Valid(); pit.SetToNext())
      {
      TR::Symbol *sym = pit->_hostSymRef->getSymbol();
      if (!first) ir.print(", ");
      ir.print("%s", getTypeName(sym->getDataType()));
      first = false;
      }

   ir.print("%s%s%s",
            numParms > 0 ? ", " : "",
            self()->comp()->isGPUCompilation() ? "" : "i32, i32, ",  // startInclusive, endExclusive
            getTypeName(TR::Address));   // for ExceptionCheck

   ir.print(")* @test%d, metadata !\"kernel\", i32 1}\n", gpuPtxCount);

   nvvmIR = ir.getString();

   traceNVVMIR(self()->comp(), nvvmIR);

   //if any of these are set, it means this kernel may trigger a Java exception
   *hasExceptionChecks = (_gpuHasNullCheck || _gpuHasBndCheck || _gpuHasDivCheck);

   return GPUSuccess;
   }


void
J9::CodeGenerator::generateGPU()
   {
   if (self()->comp()->isGPUCompilation())
      {
      char *programSource;
      TR::Node *errorNode;
      GPUResult result;
      TR::ResolvedMethodSymbol *method = self()->comp()->getJittedMethodSymbol();

      {
      TR::StackMemoryRegion stackMemoryRegion(*self()->trMemory());

      result = self()->dumpNVVMIR(self()->comp()->getStartTree(), self()->comp()->findLastTree(),
                          NULL,
                          NULL,
                          &method->getAutomaticList(),
                          &method->getParameterList(),
                          false, // TODO: check if method is static
                          programSource, errorNode, 0, 0); //gpuPtxCount is not applicable here so it is always set to 0.

      } // scope of the stack memory region

      self()->comp()->getOptimizationPlan()->setGPUResult(result);

      if (result == GPUSuccess)
         {
         self()->comp()->getOptimizationPlan()->setGPUIR(programSource);
         }

      if (!self()->comp()->isGPUCompileCPUCode())
         return;

      TR::CFG *cfg = self()->comp()->getFlowGraph();
      TR::Block *startBlock = self()->comp()->getStartBlock();
      startBlock->split(self()->comp()->getStartTree()->getNextTreeTop(), cfg, false, false);

      ListAppender<TR::ParameterSymbol> la(&method->getParameterList());
      la.empty(); // empty current parameter list

      TR::ParameterSymbol *parmSymbol;
      int slot = 0;

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(0);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(1);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(2);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(3);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setLinkageRegisterIndex(4);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(5);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(6);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setLinkageRegisterIndex(7);
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);


      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Int32);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      parmSymbol = method->comp()->getSymRefTab()->createParameterSymbol(method, slot, TR::Address);
      parmSymbol->setOrdinal(slot++);
      parmSymbol->setReferencedParameter();
      parmSymbol->setTypeSignature("", 0);
      la.add(parmSymbol);

      TR::Node *callNode, *parm;
      TR::SymbolReference *parmSymRef;
      callNode = TR::Node::create(self()->comp()->getStartTree()->getNode(), TR::icall, 13);

      parm = TR::Node::create(callNode, TR::aload, 0);  // vmThread
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 0, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(0, parm);

      parm = TR::Node::create(callNode, TR::aload, 0);   // method
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 1, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(1, parm);

      parm = TR::Node::create(callNode, TR::aload, 0);   // programSource
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 2, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(2, parm);

      parm = TR::Node::create(callNode, TR::aload, 0);   // invokeObject
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 3, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(3, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // deviceId
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 4, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(4, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // gridDimX
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 5, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(5, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // gridDimY
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 6, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(6, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // gridDimZ
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 7, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(7, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // blockDimX
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 8, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(8, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // blockDimY
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 9, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(9, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // blockDimZ
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 10, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(10, parm);

      parm = TR::Node::create(callNode, TR::iload, 0); // argCount
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 11, TR::Int32);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(11, parm);

      parm = TR::Node::create(callNode, TR::aload, 0); // args
      parmSymRef = self()->comp()->getSymRefTab()->findOrCreateAutoSymbol(method, 12, TR::Address);
      parm->setSymbolReference(parmSymRef);
      callNode->setAndIncChild(12, parm);

      TR::SymbolReference *helper = self()->comp()->getSymRefTab()->findOrCreateRuntimeHelper(TR_callGPU);
      helper->getSymbol()->castToMethodSymbol()->setLinkage(TR_System);
      callNode->setSymbolReference(helper);
      TR::Node *treetop = TR::Node::create(callNode, TR::treetop, 1);
      treetop->setAndIncChild(0, callNode);
      TR::TreeTop *callTreeTop = TR::TreeTop::create(self()->comp(), treetop);
      self()->comp()->getStartTree()->insertAfter(callTreeTop);

      TR::Node *returnNode = TR::Node::create(callNode, TR::ireturn, 1);   // TODO: handle mismatching returns
      returnNode->setAndIncChild(0, callNode);
      TR::TreeTop *returnTreeTop = TR::TreeTop::create(self()->comp(), returnNode);
      callTreeTop->insertAfter(returnTreeTop);

      }
   }

uintptr_t
J9::CodeGenerator::objectLengthOffset()
   {
   return self()->fe()->getOffsetOfContiguousArraySizeField();
   }

uintptr_t
J9::CodeGenerator::objectHeaderInvariant()
   {
   return self()->objectLengthOffset() + 4 /*length*/ ;
   }
